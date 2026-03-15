# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4096/5519 lines (74.22%)

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
|        - |    67 | `/*` |
|        - |    68 | ` * Output control buffer entry.` |
|        - |    69 | ` * Refer to the implementation of [ob_start()] for more information.` |
|        - |    70 | ` */` |
|        - |    71 | `typedef struct VmObEntry VmObEntry;` |
|        - |    72 | `struct VmObEntry` |
|        - |    73 |  |
|        - |    74 | `	ph7_value sCallback; /* User defined callback */` |
|        - |    75 | `	SyBlob sOB;          /* Output buffer consumer */` |
|        - |    76 | `};` |
|        - |    77 | `/*` |
|        - |    78 | ` * Each installed shutdown callback (registered using [register_shutdown_function()] )` |
|        - |    79 | ` * is stored in an instance of the following structure.` |
|        - |    80 | ` * Refer to the implementation of [register_shutdown_function(()] for more information.` |
|        - |    81 | ` */` |
|        - |    82 | `typedef struct VmShutdownCB VmShutdownCB;` |
|        - |    83 | `struct VmShutdownCB` |
|        - |    84 |  |
|        - |    85 | `	ph7_value sCallback; /* Shutdown callback */` |
|        - |    86 | `	ph7_value aArg[10];   /* Callback arguments (10 maximum arguments) */` |
|        - |    87 | `	int nArg;             /* Total number of given arguments */` |
|        - |    88 | `};` |
|        - |    89 | `/* Uncaught exception code value */` |
|        - |    90 | `#define PH7_EXCEPTION -255` |
|        - |    91 |  |
|        - |    92 | `/*` |
|        - |    93 | ` * Return TRUE if either operand is a NaN real value.` |
|        - |    94 | ` */` |
|   748036 |    95 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    96 |  |
|   748038 |    97 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       23 |    98 | `		return TRUE;` |
|        - |    99 | `	}` |
|   748016 |   100 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |   101 | `		return TRUE;` |
|        - |   102 | `	}` |
|   748008 |   103 | `	return FALSE;` |
|   374042 |   104 |  |
|        - |   105 | `/* SyhttpUri, SyhttpHeader and HTTP method/protocol defines moved to ph7int.h */` |
|        - |   106 | `/*` |
|        - |   107 | ` * Register a constant and it's associated expansion callback so that` |
|        - |   108 | ` * it can be expanded from the target PHP program.` |
|        - |   109 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|        - |   110 | ` * simple and work as follows:` |
|        - |   111 | ` * Each registered constant have a C procedure associated with it.` |
|        - |   112 | ` * This procedure known as the constant expansion callback is responsible` |
|        - |   113 | ` * of expanding the invoked constant to the desired value,for example:` |
|        - |   114 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|        - |   115 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|        - |   116 | ` * (Windows,Linux,...) and so on.` |
|        - |   117 | ` * Please refer to the official documentation for additional information.` |
|        - |   118 | ` */` |
|   337762 |   119 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |   120 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   121 | `	const SyString *pName,  /* Constant name */` |
|        - |   122 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   123 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |   124 | `	)` |
|        2 |   125 |  |
|        - |   126 | `	ph7_constant *pCons;` |
|        - |   127 | `	SyHashEntry *pEntry;` |
|        - |   128 | `	char *zDupName;` |
|        - |   129 | `	sxi32 rc;` |
|   337764 |   130 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   337764 |   131 | `	if( pEntry ){` |
|        - |   132 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   133 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   134 | `		pCons->xExpand = xExpand;` |
|        6 |   135 | `		pCons->pUserData = pUserData;` |
|        6 |   136 | `		return SXRET_OK;` |
|        - |   137 | `	}` |
|        - |   138 | `	/* Allocate a new constant instance */` |
|   337760 |   139 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   337760 |   140 | `	if( pCons == 0 ){` |
|      ! 0 |   141 | `		return 0;` |
|        - |   142 | `	}` |
|        - |   143 | `	/* Duplicate constant name */` |
|   337760 |   144 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   337760 |   145 | `	if( zDupName == 0 ){` |
|      ! 0 |   146 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   147 | `		return 0;` |
|        - |   148 | `	}` |
|        - |   149 | `	/* Install the constant */` |
|   337760 |   150 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   337760 |   151 | `	pCons->xExpand = xExpand;` |
|   337760 |   152 | `	pCons->pUserData = pUserData;` |
|   337760 |   153 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   337760 |   154 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   155 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   156 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   157 | `		return rc;` |
|        - |   158 | `	}` |
|        - |   159 | `	/* All done,constant can be invoked from PHP code */` |
|   337760 |   160 | `	return SXRET_OK;` |
|   168883 |   161 |  |
|        - |   162 | `/*` |
|        - |   163 | ` * Allocate a new foreign function instance.` |
|        - |   164 | ` * This function return SXRET_OK on success. Any other` |
|        - |   165 | ` * return value indicates failure.` |
|        - |   166 | ` * Please refer to the official documentation for an introduction to` |
|        - |   167 | ` * the foreign function mechanism.` |
|        - |   168 | ` */` |
|   727320 |   169 | `static sxi32 PH7_NewForeignFunction(` |
|        - |   170 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   171 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   172 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   173 | `	void *pUserData,          /* Foreign function private data */` |
|        - |   174 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |   175 | `	)` |
|        2 |   176 |  |
|        - |   177 | `	ph7_user_func *pFunc;` |
|        - |   178 | `	char *zDup;` |
|        - |   179 | `	/* Allocate a new user function */` |
|   727322 |   180 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   727322 |   181 | `	if( pFunc == 0 ){` |
|      ! 0 |   182 | `		return SXERR_MEM;` |
|        - |   183 | `	}` |
|        - |   184 | `	/* Duplicate function name */` |
|   727322 |   185 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   727322 |   186 | `	if( zDup == 0 ){` |
|      ! 0 |   187 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   188 | `		return SXERR_MEM;` |
|        - |   189 | `	}` |
|        - |   190 | `	/* Zero the structure */` |
|   727322 |   191 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   192 | `	/* Initialize structure fields */` |
|   727322 |   193 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   727322 |   194 | `	pFunc->pVm   = pVm;` |
|   727322 |   195 | `	pFunc->xFunc = xFunc;` |
|   727322 |   196 | `	pFunc->pUserData = pUserData;` |
|   727322 |   197 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   198 | `	/* Write a pointer to the new function */` |
|   727322 |   199 | `	*ppOut = pFunc;` |
|   727322 |   200 | `	return SXRET_OK;` |
|   363662 |   201 |  |
|        - |   202 | `/*` |
|        - |   203 | ` * Install a foreign function and it's associated callback so that` |
|        - |   204 | ` * it can be invoked from the target PHP code.` |
|        - |   205 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   206 | ` * return value indicates failure.` |
|        - |   207 | ` * Please refer to the official documentation for an introduction to` |
|        - |   208 | ` * the foreign function mechanism.` |
|        - |   209 | ` */` |
|   728992 |   210 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |   211 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   212 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   213 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   214 | `	void *pUserData           /* Foreign function private data */` |
|        - |   215 | `	)` |
|        2 |   216 |  |
|        - |   217 | `	ph7_user_func *pFunc;` |
|        - |   218 | `	SyHashEntry *pEntry;` |
|        - |   219 | `	sxi32 rc;` |
|        - |   220 | `	/* Overwrite any previously registered function with the same name */` |
|   728994 |   221 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   728994 |   222 | `	if( pEntry ){` |
|     1674 |   223 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     1674 |   224 | `		pFunc->pUserData = pUserData;` |
|     1674 |   225 | `		pFunc->xFunc = xFunc;` |
|     1674 |   226 | `		SySetReset(&pFunc->aAux);` |
|     1674 |   227 | `		return SXRET_OK;` |
|        - |   228 | `	}` |
|        - |   229 | `	/* Create a new user function */` |
|   727322 |   230 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   727322 |   231 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   232 | `		return rc;` |
|        - |   233 | `	}` |
|        - |   234 | `	/* Install the function in the corresponding hashtable */` |
|   727322 |   235 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   727322 |   236 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   237 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   238 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   239 | `		return rc;` |
|        - |   240 | `	}` |
|        - |   241 | `	/* User function successfully installed */` |
|   727322 |   242 | `	return SXRET_OK;` |
|   364498 |   243 |  |
|        - |   244 | `/*` |
|        - |   245 | ` * Initialize a VM function.` |
|        - |   246 | ` */` |
|    80890 |   247 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   248 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   249 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   250 | `	const char *zName,  /* Function name */` |
|        - |   251 | `	sxu32 nByte,        /* zName length */` |
|        - |   252 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   253 | `	void *pUserData     /* Function private data */` |
|        - |   254 | `	)` |
|        2 |   255 |  |
|        - |   256 | `	/* Zero the structure */` |
|    80892 |   257 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   258 | `	/* Initialize structure fields */` |
|        - |   259 | `	/* Arguments container */` |
|    80892 |   260 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   261 | `	/* Static variable container */` |
|    80892 |   262 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   263 | `	/* Bytecode container */` |
|    80892 |   264 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   265 | `    /* Preallocate some instruction slots */` |
|    80892 |   266 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   267 | `	/* Closure environment */` |
|    80892 |   268 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|    80892 |   269 | `	pFunc->iFlags = iFlags;` |
|    80892 |   270 | `	pFunc->pUserData = pUserData;` |
|    80892 |   271 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|    80892 |   272 | `	return SXRET_OK;` |
|        2 |   273 |  |
|        - |   274 | `/*` |
|        - |   275 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   276 | ` */` |
|   258348 |   277 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   278 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   279 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   280 | `	SyString *pName     /* Function name */` |
|        - |   281 | `	)` |
|        2 |   282 |  |
|        - |   283 | `	SyHashEntry *pEntry;` |
|        - |   284 | `	sxi32 rc;` |
|   258350 |   285 | `	if( pName == 0 ){` |
|        - |   286 | `		/* Use the built-in name */` |
|    25286 |   287 | `		pName = &pFunc->sName;` |
|    12642 |   288 | `	}` |
|        - |   289 | `	/* Check for duplicates (functions with the same name) first */` |
|   258350 |   290 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   258350 |   291 | `	if( pEntry ){` |
|   192946 |   292 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   192946 |   293 | `		if( pLink != pFunc ){` |
|        - |   294 | `			/* Link */` |
|      179 |   295 | `			pFunc->pNextName = pLink;` |
|      179 |   296 | `			pEntry->pUserData = pFunc;` |
|       89 |   297 | `		}` |
|   192946 |   298 | `		return SXRET_OK;` |
|        - |   299 | `	}` |
|        - |   300 | `	/* First time seen */` |
|    65406 |   301 | `	pFunc->pNextName = 0;` |
|    65406 |   302 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    65406 |   303 | `	return rc;` |
|   129176 |   304 |  |
|        - |   305 | `/*` |
|        - |   306 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   307 | ` */` |
|    21268 |   308 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   309 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   310 | `	ph7_class *pClass /* Target Class */` |
|        - |   311 | `	)` |
|        2 |   312 |  |
|    21270 |   313 | `	SyString *pName = &pClass->sName;` |
|        - |   314 | `	SyHashEntry *pEntry;` |
|        - |   315 | `	sxi32 rc;` |
|        - |   316 | `	/* Check for duplicates */` |
|    21270 |   317 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    21270 |   318 | `	if( pEntry ){` |
|       31 |   319 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   320 | `		/* Link entry with the same name */` |
|       31 |   321 | `		pClass->pNextName = pLink;` |
|       31 |   322 | `		pEntry->pUserData = pClass;` |
|       31 |   323 | `		return SXRET_OK;` |
|        - |   324 | `	}` |
|    21240 |   325 | `	pClass->pNextName = 0;` |
|        - |   326 | `	/* Perform a simple hashtable insertion */` |
|    21240 |   327 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    21240 |   328 | `	return rc;` |
|    10636 |   329 |  |
|        - |   330 | `/*` |
|        - |   331 | ` * Instruction builder interface.` |
|        - |   332 | ` */` |
|  2017712 |   333 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   334 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   335 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   336 | `	sxi32 iP1,    /* First operand */` |
|        - |   337 | `	sxu32 iP2,    /* Second operand */` |
|        - |   338 | `	void *p3,     /* Third operand */` |
|        - |   339 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   340 | `	)` |
|        2 |   341 |  |
|        - |   342 | `	VmInstr sInstr;` |
|        - |   343 | `	sxi32 rc;` |
|        - |   344 | `	/* Fill the VM instruction */` |
|  2017714 |   345 | `	sInstr.iOp = (sxu8)iOp;` |
|  2017714 |   346 | `	sInstr.iP1 = iP1;` |
|  2017714 |   347 | `	sInstr.iP2 = iP2;` |
|  2017714 |   348 | `	sInstr.p3  = p3;` |
|  2017714 |   349 | `	if( pIndex ){` |
|        - |   350 | `		/* Instruction index in the bytecode array */` |
|   122832 |   351 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    61415 |   352 | `	}` |
|        - |   353 | `	/* Finally,record the instruction */` |
|  2017714 |   354 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  2017714 |   355 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   356 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   357 | `		/* Fall throw */` |
|      ! 0 |   358 | `	}` |
|  2017714 |   359 | `	return rc;` |
|        2 |   360 |  |
|        - |   361 | `/*` |
|        - |   362 | ` * Swap the current bytecode container with the given one.` |
|        - |   363 | ` */` |
|   196656 |   364 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   365 |  |
|   196658 |   366 | `	if( pContainer == 0 ){` |
|        - |   367 | `		/* Point to the default container */` |
|      ! 0 |   368 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   369 | `	}else{` |
|        - |   370 | `		/* Change container */` |
|   196658 |   371 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   372 | `	}` |
|   196658 |   373 | `	return SXRET_OK;` |
|        2 |   374 |  |
|        - |   375 | `/*` |
|        - |   376 | ` * Return the current bytecode container.` |
|        - |   377 | ` */` |
|    98328 |   378 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   379 |  |
|    98330 |   380 | `	return pVm->pByteContainer;` |
|        2 |   381 |  |
|        - |   382 | `/*` |
|        - |   383 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   384 | ` */` |
|   120844 |   385 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   386 |  |
|        - |   387 | `	VmInstr *pInstr;` |
|   120846 |   388 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   120846 |   389 | `	return pInstr;` |
|        2 |   390 |  |
|        - |   391 | `/*` |
|        - |   392 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   393 | ` */` |
|   586254 |   394 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   395 |  |
|   586256 |   396 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   397 |  |
|        - |   398 | `/*` |
|        - |   399 | ` * Pop the last VM instruction.` |
|        - |   400 | ` */` |
|   117524 |   401 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   402 |  |
|   117526 |   403 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   404 |  |
|        - |   405 | `/*` |
|        - |   406 | ` * Peek the last VM instruction.` |
|        - |   407 | ` */` |
|   313146 |   408 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   409 |  |
|   313148 |   410 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   411 |  |
|     7866 |   412 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   413 |  |
|        - |   414 | `	VmInstr *aInstr;` |
|        - |   415 | `	sxu32 n;` |
|     7868 |   416 | `	n = SySetUsed(pVm->pByteContainer);` |
|     7868 |   417 | `	if( n < 2 ){` |
|      ! 0 |   418 | `		return 0;` |
|        - |   419 | `	}` |
|     7868 |   420 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|     7868 |   421 | `	return &aInstr[n - 2];` |
|     3935 |   422 |  |
|        - |   423 | `/*` |
|        - |   424 | ` * Allocate a new virtual machine frame.` |
|        - |   425 | ` */` |
|    12606 |   426 | `static VmFrame * VmNewFrame(` |
|        - |   427 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   428 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   429 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   430 | `	)` |
|        2 |   431 |  |
|        - |   432 | `	VmFrame *pFrame;` |
|        - |   433 | `	/* Allocate a new vm frame */` |
|    12608 |   434 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    12608 |   435 | `	if( pFrame == 0 ){` |
|      ! 0 |   436 | `		return 0;` |
|        - |   437 | `	}` |
|        - |   438 | `	/* Zero the structure */` |
|    12608 |   439 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   440 | `	/* Initialize frame fields */` |
|    12608 |   441 | `	pFrame->pUserData = pUserData;` |
|    12608 |   442 | `	pFrame->pThis = pThis;` |
|    12608 |   443 | `	pFrame->pVm = pVm;` |
|    12608 |   444 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    12608 |   445 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    12608 |   446 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    12608 |   447 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    12608 |   448 | `	return pFrame;` |
|     6305 |   449 |  |
|        - |   450 | `/*` |
|        - |   451 | ` * Enter a VM frame.` |
|        - |   452 | ` */` |
|    12606 |   453 | `static sxi32 VmEnterFrame(` |
|        - |   454 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   455 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   456 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   457 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   458 | `	)` |
|        2 |   459 |  |
|        - |   460 | `	VmFrame *pFrame;` |
|        - |   461 | `	/* Allocate a new frame */` |
|    12608 |   462 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    12608 |   463 | `	if( pFrame == 0 ){` |
|      ! 0 |   464 | `		return SXERR_MEM;` |
|        - |   465 | `	}` |
|        - |   466 | `	/* Link to the list of active VM frame */` |
|    12608 |   467 | `	pFrame->pParent = pVm->pFrame;` |
|    12608 |   468 | `	pVm->pFrame = pFrame;` |
|    12608 |   469 | `	if( ppFrame ){` |
|        - |   470 | `		/* Write a pointer to the new VM frame */` |
|    10698 |   471 | `		*ppFrame = pFrame;` |
|     5348 |   472 | `	}` |
|    12608 |   473 | `	return SXRET_OK;` |
|     6305 |   474 |  |
|        - |   475 | `/*` |
|        - |   476 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   477 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   478 | ` * information.` |
|        - |   479 | ` */` |
|       48 |   480 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        1 |   481 |  |
|        - |   482 | `	VmFrame *pTarget,*pFrame;` |
|       49 |   483 | `	SyHashEntry *pEntry = 0;` |
|        - |   484 | `	sxi32 rc;` |
|        - |   485 | `	/* Point to the upper frame */` |
|       49 |   486 | `	pFrame = pVm->pFrame;` |
|       49 |   487 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |   488 | `		/* Safely ignore the exception frame */` |
|      ! 0 |   489 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   490 | `	}` |
|       49 |   491 | `	pTarget = pFrame;` |
|       49 |   492 | `	pFrame = pTarget->pParent;` |
|       63 |   493 | `	while( pFrame ){` |
|       63 |   494 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   495 | `			/* Query the current frame */` |
|       49 |   496 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       49 |   497 | `			if( pEntry ){` |
|        - |   498 | `				/* Variable found */` |
|       49 |   499 | `				break;` |
|        - |   500 | `			}` |
|      ! 0 |   501 | `		}` |
|        - |   502 | `		/* Point to the upper frame */` |
|       15 |   503 | `		pFrame = pFrame->pParent;` |
|        1 |   504 | `	}` |
|       49 |   505 | `	if( pEntry == 0 ){` |
|        - |   506 | `		/* Inexistant variable */` |
|      ! 0 |   507 | `		return SXERR_NOTFOUND;` |
|        - |   508 | `	}` |
|        - |   509 | `	/* Link to the current frame */` |
|       49 |   510 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       49 |   511 | `	if( rc == SXRET_OK ){` |
|        - |   512 | `		sxu32 nIdx;` |
|       49 |   513 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       49 |   514 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       24 |   515 | `	}` |
|       49 |   516 | `	return rc;` |
|       25 |   517 |  |
|        - |   518 | `/*` |
|        - |   519 | ` * Leave the top-most active frame.` |
|        - |   520 | ` */` |
|    10694 |   521 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   522 |  |
|    10696 |   523 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    10696 |   524 | `	if( pCurFrame ){` |
|        - |   525 | `		/* Unlink from the list of active VM frame */` |
|    10696 |   526 | `		pVm->pFrame = pCurFrame->pParent;` |
|    10696 |   527 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   528 | `			VmSlot  *aSlot;` |
|        - |   529 | `			sxu32 n;` |
|        - |   530 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    10678 |   531 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    77702 |   532 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   533 | `				/* Unset the local variable */` |
|    67026 |   534 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    33514 |   535 | `			}` |
|        - |   536 | `			/* Remove local reference */` |
|    10678 |   537 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    77754 |   538 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    67078 |   539 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    33540 |   540 | `			}` |
|     5338 |   541 | `		}` |
|        - |   542 | `		/* Release internal containers */` |
|    10696 |   543 | `		SyHashRelease(&pCurFrame->hVar);` |
|    10696 |   544 | `		SySetRelease(&pCurFrame->sArg);` |
|    10696 |   545 | `		SySetRelease(&pCurFrame->sLocal);` |
|    10696 |   546 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   547 | `		/* Release the whole structure */` |
|    10696 |   548 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     5347 |   549 | `	}` |
|    10696 |   550 |  |
|        - |   551 | `/*` |
|        - |   552 | ` * Compare two functions signature and return the comparison result.` |
|        - |   553 | ` */` |
|      818 |   554 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   555 |  |
|      819 |   556 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      819 |   557 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      819 |   558 | `	const char *zSin = pSecond->zString;` |
|      819 |   559 | `	const char *zFin = pFirst->zString;` |
|      819 |   560 | `	const char *zPtr = zFin;` |
|      409 |   561 | `	for(;;){` |
|      819 |   562 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      410 |   563 | `			break;` |
|        - |   564 | `		}` |
|      ! 0 |   565 | `		if( zFin[0] != zSin[0] ){` |
|        - |   566 | `			/* mismatch */` |
|      ! 0 |   567 | `			break;` |
|        - |   568 | `		}` |
|      ! 0 |   569 | `		zFin++;` |
|      ! 0 |   570 | `		zSin++;` |
|      ! 0 |   571 | `	}` |
|      819 |   572 | `	return (int)(zFin-zPtr);` |
|        1 |   573 |  |
|        - |   574 | `/*` |
|        - |   575 | ` * Select the appropriate VM function for the current call context.` |
|        - |   576 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   577 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   578 | ` * Refer to the official documentation for more information.` |
|        - |   579 | ` */` |
|      122 |   580 | `static ph7_vm_func * VmOverload(` |
|        - |   581 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   582 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   583 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   584 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   585 | `	)` |
|        1 |   586 |  |
|        - |   587 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   588 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   589 | `	ph7_vm_func *pLink;` |
|        - |   590 | `	SyString sArgSig;` |
|        - |   591 | `	SyBlob sSig;` |
|        - |   592 |  |
|      123 |   593 | `	pLink = pList;` |
|      123 |   594 | `	i = 0;` |
|        - |   595 | `	/* Put functions expecting the same number of passed arguments */` |
|     1031 |   596 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|      969 |   597 | `		if( pLink == 0 ){` |
|       61 |   598 | `			break;` |
|        - |   599 | `		}` |
|      909 |   600 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   601 | `			/* Candidate for overloading */` |
|      863 |   602 | `			apSet[i++] = pLink;` |
|      431 |   603 | `		}` |
|        - |   604 | `		/* Point to the next entry */` |
|      909 |   605 | `		pLink = pLink->pNextName;` |
|        1 |   606 | `	}` |
|      123 |   607 | `	if( i < 1 ){` |
|        - |   608 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   609 | `		return pList;` |
|        - |   610 | `	}` |
|      123 |   611 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   612 | `		/* Return the only candidate */` |
|       21 |   613 | `		return apSet[0];` |
|        - |   614 | `	}` |
|        - |   615 | `	/* Calculate function signature */` |
|      103 |   616 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      355 |   617 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      253 |   618 | `		int c = 'n'; /* null */` |
|      253 |   619 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   620 | `			/* Hashmap */` |
|       45 |   621 | `			c = 'h';` |
|      231 |   622 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   623 | `			/* bool */` |
|      ! 0 |   624 | `			c = 'b';` |
|      209 |   625 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   626 | `			/* int */` |
|        5 |   627 | `			c = 'i';` |
|      207 |   628 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   629 | `			/* String */` |
|      105 |   630 | `			c = 's';` |
|      153 |   631 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   632 | `			/* Float */` |
|      ! 0 |   633 | `			c = 'f';` |
|      101 |   634 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   635 | `			/* Class instance */` |
|      ! 0 |   636 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|      ! 0 |   637 | `			SyString *pName = &pClass->sName;` |
|      ! 0 |   638 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|      ! 0 |   639 | `			c = -1;` |
|      ! 0 |   640 | `		}` |
|      253 |   641 | `		if( c > 0 ){` |
|      253 |   642 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      126 |   643 | `		}` |
|      127 |   644 | `	}` |
|      103 |   645 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      103 |   646 | `	iTarget = 0;` |
|      103 |   647 | `	iMax = -1;` |
|        - |   648 | `	/* Select the appropriate function */` |
|      921 |   649 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   650 | `		/* Compare the two signatures */` |
|      819 |   651 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      819 |   652 | `		if( iCur > iMax ){` |
|      103 |   653 | `			iMax = iCur;` |
|      103 |   654 | `			iTarget = j;` |
|       51 |   655 | `		}` |
|      410 |   656 | `	}` |
|      103 |   657 | `	SyBlobRelease(&sSig);` |
|        - |   658 | `	/* Appropriate function for the current call context */` |
|      103 |   659 | `	return apSet[iTarget];` |
|       62 |   660 |  |
|        - |   661 | `/* Forward declaration */` |
|        - |   662 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   663 | `/*` |
|        - |   664 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   665 | ` * it can be instanciated from the executed PHP script.` |
|        - |   666 | ` */` |
|    71332 |   667 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   668 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   669 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   670 | `	)` |
|        2 |   671 |  |
|        - |   672 | `	ph7_class_method *pMeth;` |
|        - |   673 | `	ph7_class_attr *pAttr;` |
|        - |   674 | `	SyHashEntry *pEntry;` |
|        - |   675 | `	sxi32 rc;` |
|        - |   676 | `	/* Reset the loop cursor */` |
|    71334 |   677 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   678 | `	/* Process only static and constant attribute */` |
|   251756 |   679 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   680 | `		/* Extract the current attribute */` |
|   144758 |   681 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   144758 |   682 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   683 | `			ph7_value *pMemObj;` |
|        - |   684 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1290 |   685 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1290 |   686 | `			if( pMemObj == 0 ){` |
|      ! 0 |   687 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   688 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   689 | `					&pClass->sName,&pAttr->sName` |
|        - |   690 | `					);` |
|      ! 0 |   691 | `				return SXERR_MEM;` |
|        - |   692 | `			}` |
|     1290 |   693 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   694 | `				/* Initialize attribute default value (any complex expression) */` |
|     1290 |   695 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      644 |   696 | `			}` |
|        - |   697 | `			/* Record attribute index */` |
|     1290 |   698 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   699 | `			/* Install static attribute in the reference table */` |
|     1290 |   700 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      644 |   701 | `		}` |
|        2 |   702 | `	}` |
|        - |   703 | `	/* Install class methods */` |
|    71334 |   704 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |   705 | `		/* Do not mount interface methods since they are signatures only.` |
|        - |   706 | `		 */` |
|    42444 |   707 | `		return SXRET_OK;` |
|        - |   708 | `	}` |
|        - |   709 | `	/* Create constructor alias if not yet done */` |
|    28892 |   710 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   711 | `		/* User constructor with the same base class name */` |
|      206 |   712 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      206 |   713 | `		if( pEntry ){` |
|      ! 0 |   714 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   715 | `			/* Create the alias */` |
|      ! 0 |   716 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   717 | `		}` |
|      102 |   718 | `	}` |
|        - |   719 | `	/* Install the methods now */` |
|    28892 |   720 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   276407 |   721 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   233072 |   722 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   233072 |   723 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   233066 |   724 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   233066 |   725 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   726 | `				return rc;` |
|        - |   727 | `			}` |
|   116532 |   728 | `		}` |
|        2 |   729 | `	}` |
|        - |   730 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    28892 |   731 | `	pClass->bMounted = TRUE;` |
|    28892 |   732 | `	return SXRET_OK;` |
|    35668 |   733 |  |
|        - |   734 | `/*` |
|        - |   735 | ` * Allocate a private frame for attributes of the given` |
|        - |   736 | ` * class instance (Object in the PHP jargon).` |
|        - |   737 | ` */` |
|      916 |   738 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   739 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   740 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   741 | `	)` |
|        2 |   742 |  |
|      918 |   743 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   744 | `	ph7_class_attr *pAttr;` |
|        - |   745 | `	SyHashEntry *pEntry;` |
|        - |   746 | `	sxi32 rc;` |
|        - |   747 | `	/* Install class attribute in the private frame associated with this instance */` |
|      918 |   748 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     3704 |   749 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   750 | `		VmClassAttr *pVmAttr;` |
|        - |   751 | `		/* Extract the current attribute */` |
|     2788 |   752 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     2788 |   753 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     2788 |   754 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   755 | `			return SXERR_MEM;` |
|        - |   756 | `		}` |
|     2788 |   757 | `		pVmAttr->pAttr = pAttr;` |
|     2788 |   758 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   759 | `			ph7_value *pMemObj;` |
|        - |   760 | `			/* Reserve a memory object for this attribute */` |
|     2782 |   761 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     2782 |   762 | `			if( pMemObj == 0 ){` |
|      ! 0 |   763 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   764 | `				return SXERR_MEM;` |
|        - |   765 | `			}` |
|     2782 |   766 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     2782 |   767 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   768 | `				/* Initialize attribute default value (any complex expression) */` |
|      904 |   769 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      451 |   770 | `			}` |
|     2782 |   771 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     2782 |   772 | `			if( rc != SXRET_OK ){` |
|        - |   773 | `				VmSlot sSlot;` |
|        - |   774 | `				/* Restore memory object */` |
|      ! 0 |   775 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   776 | `				sSlot.pUserData = 0;` |
|      ! 0 |   777 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   778 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   779 | `				return SXERR_MEM;` |
|        - |   780 | `			}` |
|        - |   781 | `			/* Install attribute in the reference table */` |
|     2782 |   782 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1392 |   783 | `		}else{` |
|        - |   784 | `			/* Install static/constant attribute */` |
|        8 |   785 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   786 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   787 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   788 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   789 | `				return SXERR_MEM;` |
|        - |   790 | `			}` |
|        - |   791 | `		}` |
|        2 |   792 | `	}` |
|      918 |   793 | `	return SXRET_OK;` |
|      460 |   794 |  |
|        - |   795 | `/* Forward declaration */` |
|        - |   796 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   797 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   798 | `/*` |
|        - |   799 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   800 | ` */` |
|        - |   801 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   802 | `/*` |
|        - |   803 | ` * Reserve a constant memory object.` |
|        - |   804 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   805 | ` */` |
|   233306 |   806 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   807 |  |
|        - |   808 | `	ph7_value *pObj;` |
|        - |   809 | `	sxi32 rc;` |
|   233308 |   810 | `	if( pIndex ){` |
|        - |   811 | `		/* Object index in the object table */` |
|   227578 |   812 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   113788 |   813 | `	}` |
|        - |   814 | `	/* Reserve a slot for the new object */` |
|   233308 |   815 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   233308 |   816 | `	if( rc != SXRET_OK ){` |
|        - |   817 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   818 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   819 | `		 */` |
|      ! 0 |   820 | `		return 0;` |
|        - |   821 | `	}` |
|   233308 |   822 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   233308 |   823 | `	return pObj;` |
|   116655 |   824 |  |
|        - |   825 | `/*` |
|        - |   826 | ` * Reserve a memory object.` |
|        - |   827 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   828 | ` */` |
|  2129624 |   829 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   830 |  |
|        - |   831 | `	ph7_value *pObj;` |
|        - |   832 | `	sxi32 rc;` |
|  2129626 |   833 | `	if( pIndex ){` |
|        - |   834 | `		/* Object index in the object table */` |
|  2129626 |   835 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1064812 |   836 | `	}` |
|        - |   837 | `	/* Reserve a slot for the new object */` |
|  2129626 |   838 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2129626 |   839 | `	if( rc != SXRET_OK ){` |
|        - |   840 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   841 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   842 | `		 */` |
|      ! 0 |   843 | `		return 0;` |
|        - |   844 | `	}` |
|  2129626 |   845 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2129626 |   846 | `	return pObj;` |
|  1064814 |   847 |  |
|        - |   848 | `/* Forward declaration */` |
|        - |   849 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   850 | `/*` |
|        - |   851 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   852 | ` * directly as foreign functions.` |
|        - |   853 | ` */` |
|        - |   854 | `#define PH7_BUILTIN_LIB \` |
|        - |   855 | `	"class Exception { "\` |
|        - |   856 | `    "protected $message = 'Unknown exception';"\` |
|        - |   857 | `    "protected $code = 0;"\` |
|        - |   858 | `    "protected $file;"\` |
|        - |   859 | `    "protected $line;"\` |
|        - |   860 | `    "protected $trace;"\` |
|        - |   861 | `    "protected $previous;"\` |
|        - |   862 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   863 | `	"   if( isset($message) ){"\` |
|        - |   864 | `	"	  $this->message = $message;"\` |
|        - |   865 | `	"   }"\` |
|        - |   866 | `	"   $this->code = $code;"\` |
|        - |   867 | `	"   $this->file = __FILE__;"\` |
|        - |   868 | `	"   $this->line = __LINE__;"\` |
|        - |   869 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   870 | `	"   if( isset($previous) ){"\` |
|        - |   871 | `	"     $this->previous = $previous;"\` |
|        - |   872 | `	"   }"\` |
|        - |   873 | `	"}"\` |
|        - |   874 | `	"public function getMessage(){"\` |
|        - |   875 | `	"   return $this->message;"\` |
|        - |   876 | `	"}"\` |
|        - |   877 | `	" public function getCode(){"\` |
|        - |   878 | `	"  return $this->code;"\` |
|        - |   879 | `	"}"\` |
|        - |   880 | `	"public function getFile(){"\` |
|        - |   881 | `	"  return $this->file;"\` |
|        - |   882 | `	"}"\` |
|        - |   883 | `	"public function getLine(){"\` |
|        - |   884 | `	"  return $this->line;"\` |
|        - |   885 | `	"}"\` |
|        - |   886 | `	"public function getTrace(){"\` |
|        - |   887 | `	"   return $this->trace;"\` |
|        - |   888 | `	"}"\` |
|        - |   889 | `	"public function getTraceAsString(){"\` |
|        - |   890 | `	"  return debug_string_backtrace();"\` |
|        - |   891 | `	"}"\` |
|        - |   892 | `	"public function getPrevious(){"\` |
|        - |   893 | `	"    return $this->previous;"\` |
|        - |   894 | `	"}"\` |
|        - |   895 | `	"public function __toString(){"\` |
|        - |   896 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   897 | `    "}"\` |
|        - |   898 | `	"}"\` |
|        - |   899 | `	"class Error extends Exception { }"\` |
|        - |   900 | `	"class TypeError extends Error { }"\` |
|        - |   901 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   902 | `	"class ValueError extends Error { }"\` |
|        - |   903 | `	"class ErrorException extends Exception { "\` |
|        - |   904 | `	"protected $severity;"\` |
|        - |   905 | `	"public function __construct(string $message = null,"\` |
|        - |   906 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   907 | `	"   if( isset($message) ){"\` |
|        - |   908 | `	"	  $this->message = $message;"\` |
|        - |   909 | `	"   }"\` |
|        - |   910 | `	"   $this->severity = $severity;"\` |
|        - |   911 | `	"   $this->code = $code;"\` |
|        - |   912 | `	"   $this->file = $filename;"\` |
|        - |   913 | `	"   $this->line = $lineno;"\` |
|        - |   914 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   915 | `	"   if( isset($previous) ){"\` |
|        - |   916 | `	"     $this->previous = $previous;"\` |
|        - |   917 | `	"   }"\` |
|        - |   918 | `	"}"\` |
|        - |   919 | `	"public function getSeverity(){"\` |
|        - |   920 | `	"   return $this->severity;"\` |
|        - |   921 | `    "}"\` |
|        - |   922 | `	"}"\` |
|        - |   923 | `	"interface Iterator {"\` |
|        - |   924 | `	"public function current();"\` |
|        - |   925 | `	"public function key();"\` |
|        - |   926 | `	"public function next();"\` |
|        - |   927 | `	"public function rewind();"\` |
|        - |   928 | `	"public function valid();"\` |
|        - |   929 | `	"}"\` |
|        - |   930 | `	"interface IteratorAggregate {"\` |
|        - |   931 | `	"public function getIterator();"\` |
|        - |   932 | `	"}"\` |
|        - |   933 | `	"interface Serializable {"\` |
|        - |   934 | `	"public function serialize();"\` |
|        - |   935 | `	"public function unserialize(string $serialized);"\` |
|        - |   936 | `	"}"\` |
|        - |   937 | `	"/* Directory releated IO */"\` |
|        - |   938 | `	"class Directory {"\` |
|        - |   939 | `	"public $handle = null;"\` |
|        - |   940 | `	"public $path  = null;"\` |
|        - |   941 | `	"public function __construct(string $path)"\` |
|        - |   942 | `	"{"\` |
|        - |   943 | `	"   $this->handle = opendir($path);"\` |
|        - |   944 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   945 | `	"      $this->path = $path;"\` |
|        - |   946 | `	"   }"\` |
|        - |   947 | `	"}"\` |
|        - |   948 | `	"public function __destruct()"\` |
|        - |   949 | `	"{"\` |
|        - |   950 | `	"  if( $this->handle != null ){"\` |
|        - |   951 | `	"       closedir($this->handle);"\` |
|        - |   952 | `	"  }"\` |
|        - |   953 | `	"}"\` |
|        - |   954 | `	"public function read()"\` |
|        - |   955 | `	"{"\` |
|        - |   956 | `	"    return readdir($this->handle);"\` |
|        - |   957 | `	"}"\` |
|        - |   958 | `	"public function rewind()"\` |
|        - |   959 | `	"{"\` |
|        - |   960 | `	"    rewinddir($this->handle);"\` |
|        - |   961 | `	"}"\` |
|        - |   962 | `	"public function close()"\` |
|        - |   963 | `	"{"\` |
|        - |   964 | `	"    closedir($this->handle);"\` |
|        - |   965 | `	"    $this->handle = null;"\` |
|        - |   966 | `	"}"\` |
|        - |   967 | `	"}"\` |
|        - |   968 | `	"class stdClass{"\` |
|        - |   969 | `	"  public $value;"\` |
|        - |   970 | `	" /* Magic methods */"\` |
|        - |   971 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |   972 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |   973 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |   974 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |   975 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |   976 | `	"}"\` |
|        - |   977 | `	"function dir(string $path){"\` |
|        - |   978 | `	"   return new Directory($path);"\` |
|        - |   979 | `	"}"\` |
|        - |   980 | `	"function Dir(string $path){"\` |
|        - |   981 | `	"   return new Directory($path);"\` |
|        - |   982 | `	"}"\` |
|        - |   983 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |   984 | `    "{"\` |
|        - |   985 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |   986 | `	"  $aDir = array();"\` |
|        - |   987 | `	"  $pHandle = opendir($directory);"\` |
|        - |   988 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |   989 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |   990 | `	"      $aDir[] = $pEntry;"\` |
|        - |   991 | `	"   }"\` |
|        - |   992 | `	"  closedir($pHandle);"\` |
|        - |   993 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |   994 | `	"      rsort($aDir);"\` |
|        - |   995 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |   996 | `	"      sort($aDir);"\` |
|        - |   997 | `	"  }"\` |
|        - |   998 | `	"  return $aDir;"\` |
|        - |   999 | `	"}"\` |
|        - |  1000 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1001 | `	"/* Open the target directory */"\` |
|        - |  1002 | `	"$zDir = dirname($pattern);"\` |
|        - |  1003 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1004 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1005 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1006 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1007 | `	"	return FALSE;"\` |
|        - |  1008 | `	"}"\` |
|        - |  1009 | `	"$pattern = basename($pattern);"\` |
|        - |  1010 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1011 | `	"/* Loop throw available entries */"\` |
|        - |  1012 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1013 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1014 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1015 | `	"	if( $rc ){"\` |
|        - |  1016 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1017 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1018 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1019 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1020 | `	"		  }"\` |
|        - |  1021 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1022 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1023 | `	"		 continue;"\` |
|        - |  1024 | `	"	   }"\` |
|        - |  1025 | `	"	   /* Add the entry */"\` |
|        - |  1026 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1027 | `	"	}"\` |
|        - |  1028 | `	" }"\` |
|        - |  1029 | `	"/* Close the handle */"\` |
|        - |  1030 | `	"closedir($pHandle);"\` |
|        - |  1031 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1032 | `	"  /* Sort the array */"\` |
|        - |  1033 | `	"  sort($pArray);"\` |
|        - |  1034 | `	"}"\` |
|        - |  1035 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1036 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1037 | `	"  $pArray[] = $pattern;"\` |
|        - |  1038 | `	"}"\` |
|        - |  1039 | `	"/* Return the created array */"\` |
|        - |  1040 | `	"return $pArray;"\` |
|        - |  1041 | `   "}"\` |
|        - |  1042 | `   "/* Creates a temporary file */"\` |
|        - |  1043 | `   "function tmpfile(){"\` |
|        - |  1044 | `   "  /* Extract the temp directory */"\` |
|        - |  1045 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1046 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1047 | `   "    /* Use the current dir */"\` |
|        - |  1048 | `   "    $zTempDir = '.';"\` |
|        - |  1049 | `   "  }"\` |
|        - |  1050 | `   "  /* Create the file */"\` |
|        - |  1051 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1052 | `   "  return $pHandle;"\` |
|        - |  1053 | `   "}"\` |
|        - |  1054 | `   "/* Creates a temporary filename */"\` |
|        - |  1055 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1056 | `   "{"\` |
|        - |  1057 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1058 | `   "}"\` |
|        - |  1059 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1060 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1061 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1062 | `   "/* Copy arguments */"\` |
|        - |  1063 | `   "$nArgs = func_num_args();"\` |
|        - |  1064 | `   "$pNew = array();"\` |
|        - |  1065 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1066 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1067 | `    "}"\` |
|        - |  1068 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1069 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1070 | `	"/* Erase */"\` |
|        - |  1071 | `	"array_erase($pArray);"\` |
|        - |  1072 | `	"/* Unshift */"\` |
|        - |  1073 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1074 | `	"return sizeof($pArray);"\` |
|        - |  1075 | `    "}"\` |
|        - |  1076 | `	"function array_merge_recursive($array1, $array2){"\` |
|        - |  1077 | `	"if( func_num_args() < 1 ){ return NULL; }"\` |
|        - |  1078 | `    "$arrays = func_get_args();"\` |
|        - |  1079 | `    "$narrays = count($arrays);"\` |
|        - |  1080 | `    "$ret = $arrays[0];"\` |
|        - |  1081 | `    "for ($i = 1; $i < $narrays; $i++) {"\` |
|        - |  1082 | `	 " if( array_same($ret,$arrays[$i]) ){ /* Same instance */continue;}"\` |
|        - |  1083 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1084 | `     "  if (((string) $key) === ((string) intval($key))) {"\` |
|        - |  1085 | `     "   $ret[] = $value;"\` |
|        - |  1086 | `     "  }else{"\` |
|        - |  1087 | `     "  if (is_array($value) && isset($ret[$key]) ) {"\` |
|        - |  1088 | `     "   $ret[$key] = array_merge_recursive($ret[$key], $value);"\` |
|        - |  1089 | `     " }else {"\` |
|        - |  1090 | `     "   $ret[$key] = $value;"\` |
|        - |  1091 | `     "  }"\` |
|        - |  1092 | `     " }"\` |
|        - |  1093 | `     " }"\` |
|        - |  1094 | `	 "}"\` |
|        - |  1095 | `	 " return $ret;"\` |
|        - |  1096 | `    "}"\` |
|        - |  1097 | `	"function max(){"\` |
|        - |  1098 | `    "  $pArgs = func_get_args();"\` |
|        - |  1099 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1100 | `	"  return null;"\` |
|        - |  1101 | `    " }"\` |
|        - |  1102 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1103 | `    " $pArg = $pArgs[0];"\` |
|        - |  1104 | `	" if( !is_array($pArg) ){"\` |
|        - |  1105 | `	"   return $pArg; "\` |
|        - |  1106 | `	" }"\` |
|        - |  1107 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1108 | `	"   return null;"\` |
|        - |  1109 | `	" }"\` |
|        - |  1110 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1111 | `	" reset($pArg);"\` |
|        - |  1112 | `	" $max = current($pArg);"\` |
|        - |  1113 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1114 | `	"   if( $val > $max ){"\` |
|        - |  1115 | `	"     $max = $val;"\` |
|        - |  1116 | `    " }"\` |
|        - |  1117 | `	" }"\` |
|        - |  1118 | `	" return $max;"\` |
|        - |  1119 | `    " }"\` |
|        - |  1120 | `    " $max = $pArgs[0];"\` |
|        - |  1121 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1122 | `    " $val = $pArgs[$i];"\` |
|        - |  1123 | `	"if( $val > $max ){"\` |
|        - |  1124 | `	" $max = $val;"\` |
|        - |  1125 | `	"}"\` |
|        - |  1126 | `    " }"\` |
|        - |  1127 | `	" return $max;"\` |
|        - |  1128 | `    "}"\` |
|        - |  1129 | `	"function min(){"\` |
|        - |  1130 | `    "  $pArgs = func_get_args();"\` |
|        - |  1131 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1132 | `	"  return null;"\` |
|        - |  1133 | `    " }"\` |
|        - |  1134 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1135 | `    " $pArg = $pArgs[0];"\` |
|        - |  1136 | `	" if( !is_array($pArg) ){"\` |
|        - |  1137 | `	"   return $pArg; "\` |
|        - |  1138 | `	" }"\` |
|        - |  1139 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1140 | `	"   return null;"\` |
|        - |  1141 | `	" }"\` |
|        - |  1142 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1143 | `	" reset($pArg);"\` |
|        - |  1144 | `	" $min = current($pArg);"\` |
|        - |  1145 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1146 | `	"   if( $val < $min ){"\` |
|        - |  1147 | `	"     $min = $val;"\` |
|        - |  1148 | `    " }"\` |
|        - |  1149 | `	" }"\` |
|        - |  1150 | `	" return $min;"\` |
|        - |  1151 | `    " }"\` |
|        - |  1152 | `    " $min = $pArgs[0];"\` |
|        - |  1153 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1154 | `    " $val = $pArgs[$i];"\` |
|        - |  1155 | `	"if( $val < $min ){"\` |
|        - |  1156 | `	" $min = $val;"\` |
|        - |  1157 | `	" }"\` |
|        - |  1158 | `    " }"\` |
|        - |  1159 | `	" return $min;"\` |
|        - |  1160 | `	"}"\` |
|        - |  1161 | `	"function fileowner(string $file){"\` |
|        - |  1162 | `    " $a = stat($file);"\` |
|        - |  1163 | `	" if( !is_array($a) ){"\` |
|        - |  1164 | `	"	return false;"\` |
|        - |  1165 | `	" }"\` |
|        - |  1166 | `	" return $a['uid'];"\` |
|        - |  1167 | `    "}"\` |
|        - |  1168 | `    "function filegroup(string $file){"\` |
|        - |  1169 | `	" $a = stat($file);"\` |
|        - |  1170 | `	" if( !is_array($a) ){"\` |
|        - |  1171 | `	"	return false;"\` |
|        - |  1172 | `	" }"\` |
|        - |  1173 | `	" return $a['gid'];"\` |
|        - |  1174 | `    "}"\` |
|        - |  1175 | `	 "function fileinode(string $file){"\` |
|        - |  1176 | `	" $a = stat($file);"\` |
|        - |  1177 | `	" if( !is_array($a) ){"\` |
|        - |  1178 | `	"	return false;"\` |
|        - |  1179 | `	" }"\` |
|        - |  1180 | `	" return $a['ino'];"\` |
|        - |  1181 | `    "}"` |
|        - |  1182 |  |
|        - |  1183 | `/*` |
|        - |  1184 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1185 | ` * start compiling the target PHP program.` |
|        - |  1186 | ` */` |
|     1910 |  1187 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1188 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1189 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1190 | `	 )` |
|        2 |  1191 |  |
|        - |  1192 | `	SyString sBuiltin;` |
|        - |  1193 | `	ph7_value *pObj;` |
|        - |  1194 | `	sxi32 rc;` |
|        - |  1195 | `	/* Zero the structure */` |
|     1912 |  1196 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1197 | `	/* Initialize VM fields */` |
|     1912 |  1198 | `	pVm->pEngine = &(*pEngine);` |
|     1912 |  1199 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1200 | `	/* Instructions containers */` |
|     1912 |  1201 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     1912 |  1202 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     1912 |  1203 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1204 | `	/* Object containers */` |
|     1912 |  1205 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1912 |  1206 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1207 | `	/* Virtual machine internal containers */` |
|     1912 |  1208 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     1912 |  1209 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     1912 |  1210 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     1912 |  1211 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1912 |  1212 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     1912 |  1213 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     1912 |  1214 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     1912 |  1215 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     1912 |  1216 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     1912 |  1217 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     1912 |  1218 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     1912 |  1219 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     1912 |  1220 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     1912 |  1221 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     1912 |  1222 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|        - |  1223 | `	/* Configuration containers */` |
|     1912 |  1224 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     1912 |  1225 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     1912 |  1226 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     1912 |  1227 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     1912 |  1228 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1229 | `	/* Error callbacks containers */` |
|     1912 |  1230 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     1912 |  1231 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     1912 |  1232 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     1912 |  1233 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     1912 |  1234 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1235 | `	/* Set a default recursion limit */` |
|        - |  1236 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     1912 |  1237 | `	pVm->nMaxDepth = 32;` |
|        - |  1238 | `#else` |
|        - |  1239 | `	pVm->nMaxDepth = 16;` |
|        - |  1240 | `#endif` |
|        - |  1241 | `	/* Default assertion flags */` |
|     1912 |  1242 | `	pVm->iAssertFlags = PH7_ASSERT_WARNING; /* Issue a warning for each failed assertion */` |
|        - |  1243 | `	/* JSON return status */` |
|     1912 |  1244 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1245 | `	/* PRNG context */` |
|     1912 |  1246 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1247 | `	/* Install the null constant */` |
|     1912 |  1248 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1912 |  1249 | `	if( pObj == 0 ){` |
|      ! 0 |  1250 | `		rc = SXERR_MEM;` |
|      ! 0 |  1251 | `		goto Err;` |
|        - |  1252 | `	}` |
|     1912 |  1253 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1254 | `	/* Install the boolean TRUE constant */` |
|     1912 |  1255 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1912 |  1256 | `	if( pObj == 0 ){` |
|      ! 0 |  1257 | `		rc = SXERR_MEM;` |
|      ! 0 |  1258 | `		goto Err;` |
|        - |  1259 | `	}` |
|     1912 |  1260 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1261 | `	/* Install the boolean FALSE constant */` |
|     1912 |  1262 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1912 |  1263 | `	if( pObj == 0 ){` |
|      ! 0 |  1264 | `		rc = SXERR_MEM;` |
|      ! 0 |  1265 | `		goto Err;` |
|        - |  1266 | `	}` |
|     1912 |  1267 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1268 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1269 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1270 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     1912 |  1271 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     1912 |  1272 | `	if( pObj == 0 ){` |
|      ! 0 |  1273 | `		rc = SXERR_MEM;` |
|      ! 0 |  1274 | `		goto Err;` |
|        - |  1275 | `	}` |
|     1912 |  1276 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1277 | `	/* Create the global frame */` |
|     1912 |  1278 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     1912 |  1279 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1280 | `		goto Err;` |
|        - |  1281 | `	}` |
|        - |  1282 | `	/* Initialize the code generator */` |
|     1912 |  1283 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1912 |  1284 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1285 | `		goto Err;` |
|        - |  1286 | `	}` |
|        - |  1287 | `	/* VM correctly initialized,set the magic number */` |
|     1912 |  1288 | `	pVm->nMagic = PH7_VM_INIT;` |
|     1912 |  1289 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1290 | `	/* Compile the built-in library */` |
|     1912 |  1291 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1292 | `	/* Reset the code generator */` |
|     1912 |  1293 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1912 |  1294 | `	return SXRET_OK;` |
|      ! 0 |  1295 | `Err:` |
|      ! 0 |  1296 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1297 | `	return rc;` |
|      957 |  1298 |  |
|        - |  1299 | `/*` |
|        - |  1300 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1301 | ` * routine which store the output in an internal blob.` |
|        - |  1302 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1303 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1304 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1305 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1306 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1307 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1308 | ` * to finish executing and extracting the output.` |
|        - |  1309 | ` */` |
|      ! 0 |  1310 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1311 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1312 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1313 | `	void *pUserData     /* User private data */` |
|        - |  1314 | `	)` |
|      ! 0 |  1315 |  |
|        - |  1316 | `	 sxi32 rc;` |
|        - |  1317 | `	 /* Store the output in an internal BLOB */` |
|      ! 0 |  1318 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|      ! 0 |  1319 | `	 return rc;` |
|      ! 0 |  1320 |  |
|        - |  1321 | `#define VM_STACK_GUARD 16` |
|        - |  1322 | `/*` |
|        - |  1323 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1324 | ` * our compiled PHP program.` |
|        - |  1325 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1326 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1327 | ` */` |
|    26958 |  1328 | `static ph7_value * VmNewOperandStack(` |
|        - |  1329 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1330 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1331 | `	)` |
|        2 |  1332 |  |
|        - |  1333 | `	ph7_value *pStack;` |
|        - |  1334 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1335 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1336 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1337 | `  ** on the maximum stack depth required.` |
|        - |  1338 | `  **` |
|        - |  1339 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1340 | `  */` |
|    26960 |  1341 | `	nInstr += VM_STACK_GUARD;` |
|    26960 |  1342 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    26960 |  1343 | `	if( pStack == 0 ){` |
|      ! 0 |  1344 | `		return 0;` |
|        - |  1345 | `	}` |
|        - |  1346 | `	/* Initialize the operand stack */` |
|  1720202 |  1347 | `	while( nInstr > 0 ){` |
|  1693244 |  1348 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1693244 |  1349 | `		--nInstr;` |
|        2 |  1350 | `	}` |
|        - |  1351 | `	/* Ready for bytecode execution */` |
|    26960 |  1352 | `	return pStack;` |
|    13481 |  1353 |  |
|        - |  1354 | `/* Forward declaration */` |
|        - |  1355 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1356 | `/*` |
|        - |  1357 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1358 | ` * This routine gets called by the PH7 engine after` |
|        - |  1359 | ` * successful compilation of the target PHP program.` |
|        - |  1360 | ` */` |
|     1672 |  1361 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1362 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1363 | `	)` |
|        2 |  1364 |  |
|        - |  1365 | `	SyHashEntry *pEntry;` |
|        - |  1366 | `	sxi32 rc;` |
|     1674 |  1367 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1368 | `		/* Initialize your VM first */` |
|      ! 0 |  1369 | `		return SXERR_CORRUPT;` |
|        - |  1370 | `	}` |
|        - |  1371 | `	/* Mark the VM ready for byte-code execution */` |
|     1674 |  1372 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1373 | `	/* Release the code generator now we have compiled our program */` |
|     1674 |  1374 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1375 | `	/* Emit the DONE instruction */` |
|     1674 |  1376 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     1674 |  1377 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1378 | `		return SXERR_MEM;` |
|        - |  1379 | `	}` |
|        - |  1380 | `	/* Script return value */` |
|     1674 |  1381 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1382 | `	/* Allocate a new operand stack */` |
|     1674 |  1383 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     1674 |  1384 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1385 | `		return SXERR_MEM;` |
|        - |  1386 | `	}` |
|        - |  1387 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1388 | `	 * private data. */` |
|     1674 |  1389 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     1674 |  1390 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1391 | `	/* Allocate the reference table */` |
|     1674 |  1392 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     1674 |  1393 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     1674 |  1394 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1395 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1396 | `		return SXERR_MEM;` |
|        - |  1397 | `	}` |
|        - |  1398 | `	/* Zero the reference table */` |
|     1674 |  1399 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1400 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     1674 |  1401 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     1674 |  1402 | `	if( rc != SXRET_OK ){` |
|        - |  1403 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1404 | `		return rc;` |
|        - |  1405 | `	}` |
|        - |  1406 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     1674 |  1407 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     1674 |  1408 | `	if( rc != SXRET_OK ){` |
|        - |  1409 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1410 | `		return rc;` |
|        - |  1411 | `	}` |
|        - |  1412 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     1674 |  1413 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1414 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     1674 |  1415 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1416 | `	/* Initialize and install static and constants class attributes */` |
|     1674 |  1417 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    20096 |  1418 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    18424 |  1419 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    18424 |  1420 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1421 | `			return rc;` |
|        - |  1422 | `		}` |
|        2 |  1423 | `	}` |
|        - |  1424 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     1674 |  1425 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1426 | `	/* VM is ready for bytecode execution */` |
|     1674 |  1427 | `	return SXRET_OK;` |
|      838 |  1428 |  |
|        - |  1429 | `/*` |
|        - |  1430 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1431 | ` */` |
|      ! 0 |  1432 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1433 |  |
|      ! 0 |  1434 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1435 | `		return SXERR_CORRUPT;` |
|        - |  1436 | `	}` |
|        - |  1437 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1438 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1439 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1440 | `	/* Set the ready flag */` |
|      ! 0 |  1441 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1442 | `	return SXRET_OK;` |
|      ! 0 |  1443 |  |
|        - |  1444 | `/*` |
|        - |  1445 | ` * Release a Virtual Machine.` |
|        - |  1446 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1447 | ` */` |
|     1664 |  1448 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1449 |  |
|        - |  1450 | `	/* Set the stale magic number */` |
|     1666 |  1451 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1452 | `	/* Release the private memory subsystem */` |
|     1666 |  1453 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     1666 |  1454 | `	return SXRET_OK;` |
|        2 |  1455 |  |
|        - |  1456 | `/*` |
|        - |  1457 | ` * Initialize a foreign function call context.` |
|        - |  1458 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1459 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1460 | ` * functions.` |
|        - |  1461 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1462 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1463 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1464 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1465 | ` */` |
|   527968 |  1466 | `static sxi32 VmInitCallContext(` |
|        - |  1467 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1468 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1469 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1470 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1471 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1472 | `	)` |
|        2 |  1473 |  |
|   527970 |  1474 | `	pOut->pFunc = pFunc;` |
|   527970 |  1475 | `	pOut->pVm   = pVm;` |
|   527970 |  1476 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   527970 |  1477 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1478 | `	/* Assume a null return value */` |
|   527970 |  1479 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   527970 |  1480 | `	pOut->pRet = pRet;` |
|   527970 |  1481 | `	pOut->iFlags = iFlags;` |
|   527970 |  1482 | `	return SXRET_OK;` |
|        2 |  1483 |  |
|        - |  1484 | `/*` |
|        - |  1485 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1486 | ` * left behind.` |
|        - |  1487 | ` */` |
|   527968 |  1488 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1489 |  |
|        - |  1490 | `	sxu32 n;` |
|   527970 |  1491 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6164 |  1492 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    17420 |  1493 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    11258 |  1494 | `			if( apObj[n] == 0 ){` |
|        - |  1495 | `				/* Already released */` |
|      250 |  1496 | `				continue;` |
|        - |  1497 | `			}` |
|    11010 |  1498 | `			PH7_MemObjRelease(apObj[n]);` |
|    11010 |  1499 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     5506 |  1500 | `		}` |
|     6164 |  1501 | `		SySetRelease(&pCtx->sVar);` |
|     3081 |  1502 | `	}` |
|   527970 |  1503 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1504 | `		ph7_aux_data *aAux;` |
|        - |  1505 | `		void *pChunk;` |
|        - |  1506 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1507 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1508 | `		 */` |
|        9 |  1509 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1510 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1511 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1512 | `			/* Release the chunk */` |
|       25 |  1513 | `			if( pChunk ){` |
|       25 |  1514 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1515 | `			}` |
|       13 |  1516 | `		}` |
|        9 |  1517 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1518 | `	}` |
|   527970 |  1519 |  |
|        - |  1520 | `/*` |
|        - |  1521 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1522 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1523 | ` */` |
|      248 |  1524 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1525 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1526 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1527 | `	)` |
|        2 |  1528 |  |
|      250 |  1529 | `	if( pValue == 0 ){` |
|        - |  1530 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1531 | `		return;` |
|        - |  1532 | `	}` |
|      250 |  1533 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      250 |  1534 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1535 | `		sxu32 n;` |
|      936 |  1536 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      936 |  1537 | `			if( apObj[n] == pValue ){` |
|      250 |  1538 | `				PH7_MemObjRelease(pValue);` |
|      250 |  1539 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1540 | `				/* Mark as released */` |
|      250 |  1541 | `				apObj[n] = 0;` |
|      250 |  1542 | `				break;` |
|        - |  1543 | `			}` |
|      345 |  1544 | `		}` |
|      124 |  1545 | `	}` |
|      126 |  1546 |  |
|        - |  1547 | `/*` |
|        - |  1548 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1549 | ` */` |
|  3141328 |  1550 | `static void VmPopOperand(` |
|        - |  1551 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1552 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1553 | `	)` |
|        2 |  1554 |  |
|  3141330 |  1555 | `	ph7_value *pTos = *ppTos;` |
|  6654662 |  1556 | `	while( nPop > 0 ){` |
|  3513334 |  1557 | `		PH7_MemObjRelease(pTos);` |
|  3513334 |  1558 | `		pTos--;` |
|  3513334 |  1559 | `		nPop--;` |
|        2 |  1560 | `	}` |
|        - |  1561 | `	/* Top of the stack */` |
|  3141330 |  1562 | `	*ppTos = pTos;` |
|  3141330 |  1563 |  |
|        - |  1564 | `/*` |
|        - |  1565 | ` * Reserve a memory object.` |
|        - |  1566 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1567 | ` */` |
|  2940358 |  1568 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1569 |  |
|  2940360 |  1570 | `	ph7_value *pObj = 0;` |
|        - |  1571 | `	VmSlot *pSlot;` |
|        - |  1572 | `	sxu32 nIdx;` |
|        - |  1573 | `	/* Check for a free slot */` |
|  2940360 |  1574 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  2940360 |  1575 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  2940360 |  1576 | `	if( pSlot ){` |
|   810736 |  1577 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   810736 |  1578 | `		nIdx = pSlot->nIdx;` |
|   405367 |  1579 | `	}` |
|  2940360 |  1580 | `	if( pObj == 0 ){` |
|        - |  1581 | `		/* Reserve a new memory object */` |
|  2129626 |  1582 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2129626 |  1583 | `		if( pObj == 0 ){` |
|      ! 0 |  1584 | `			return 0;` |
|        - |  1585 | `		}` |
|  1064812 |  1586 | `	}` |
|        - |  1587 | `	/* Set a null default value */` |
|  2940360 |  1588 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  2940360 |  1589 | `	pObj->nIdx = nIdx;` |
|  2940360 |  1590 | `	return pObj;` |
|  1470181 |  1591 |  |
|        - |  1592 | `/*` |
|        - |  1593 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1594 | ` */` |
|    22116 |  1595 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1596 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1597 | `	const char *zKey,  /* Entry key */` |
|        - |  1598 | `	sxu32 nByte,       /* Key length */` |
|        - |  1599 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1600 | `	)` |
|        2 |  1601 |  |
|        - |  1602 | `	ph7_value sKey;` |
|        - |  1603 | `	sxi32 rc;` |
|    22118 |  1604 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    22118 |  1605 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1606 | `	/* Perform the insertion */` |
|    22118 |  1607 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    22118 |  1608 | `	PH7_MemObjRelease(&sKey);` |
|    22118 |  1609 | `	return rc;` |
|        2 |  1610 |  |
|        - |  1611 | `/*` |
|        - |  1612 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1613 | ` * Return a pointer to the variable value on success.` |
|        - |  1614 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1615 | ` */` |
|  2957242 |  1616 | `static ph7_value * VmExtractMemObj(` |
|        - |  1617 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1618 | `	const SyString *pName, /* Variable name */` |
|        - |  1619 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1620 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1621 | `	)` |
|        2 |  1622 |  |
|  2957244 |  1623 | `	int bNullify = FALSE;` |
|        - |  1624 | `	SyHashEntry *pEntry;` |
|        - |  1625 | `	VmFrame *pFrame;` |
|        - |  1626 | `	ph7_value *pObj;` |
|        - |  1627 | `	sxu32 nIdx;` |
|        - |  1628 | `	sxi32 rc;` |
|        - |  1629 | `	/* Point to the top active frame */` |
|  2957244 |  1630 | `	pFrame = pVm->pFrame;` |
|  3006596 |  1631 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1632 | `		/* Safely ignore the exception frame */` |
|    49353 |  1633 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        1 |  1634 | `	}` |
|        - |  1635 | `	/* Perform the lookup */` |
|  2957244 |  1636 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1637 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1638 | `		pName = &sAnnon;` |
|        - |  1639 | `		/* Always nullify the object */` |
|      ! 0 |  1640 | `		bNullify = TRUE;` |
|      ! 0 |  1641 | `		bDup = FALSE;` |
|      ! 0 |  1642 | `	}` |
|        - |  1643 | `	/* Check the superglobals table first */` |
|  2957244 |  1644 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  2957244 |  1645 | `	if( pEntry == 0 ){` |
|        - |  1646 | `		/* Query the top active frame */` |
|  2957208 |  1647 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  2957208 |  1648 | `		if( pEntry == 0 ){` |
|    72972 |  1649 | `			char *zName = (char *)pName->zString;` |
|        - |  1650 | `			VmSlot sLocal;` |
|    72972 |  1651 | `			if( !bCreate ){` |
|        - |  1652 | `				/* Do not create the variable,return NULL instead */` |
|      576 |  1653 | `				return 0;` |
|        - |  1654 | `			}` |
|        - |  1655 | `			/* No such variable,automatically create a new one and install` |
|        - |  1656 | `			 * it in the current frame.` |
|        - |  1657 | `			 */` |
|    72398 |  1658 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    72398 |  1659 | `			if( pObj == 0 ){` |
|      ! 0 |  1660 | `				return 0;` |
|        - |  1661 | `			}` |
|    72398 |  1662 | `			nIdx = pObj->nIdx;` |
|    72398 |  1663 | `			if( bDup ){` |
|        - |  1664 | `				/* Duplicate name */` |
|      132 |  1665 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      132 |  1666 | `				if( zName == 0 ){` |
|      ! 0 |  1667 | `					return 0;` |
|        - |  1668 | `				}` |
|       65 |  1669 | `			}` |
|        - |  1670 | `			/* Link to the top active VM frame */` |
|    72398 |  1671 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    72398 |  1672 | `			if( rc != SXRET_OK ){` |
|        - |  1673 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1674 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1675 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1676 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1677 | `				return 0;` |
|        - |  1678 | `			}` |
|    72398 |  1679 | `			if( pFrame->pParent != 0 ){` |
|        - |  1680 | `				/* Local variable */` |
|    67026 |  1681 | `				sLocal.nIdx = nIdx;` |
|    67026 |  1682 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    33514 |  1683 | `			}else{` |
|        - |  1684 | `				/* Register in the $GLOBALS array */` |
|     5374 |  1685 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1686 | `			}` |
|        - |  1687 | `			/* Install in the reference table */` |
|    72398 |  1688 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1689 | `			/* Save object index */` |
|    72398 |  1690 | `			pObj->nIdx = nIdx;` |
|    36200 |  1691 | `		}else{` |
|        - |  1692 | `			/* Extract variable contents */` |
|  2884238 |  1693 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  2884238 |  1694 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2884238 |  1695 | `			if( bNullify && pObj ){` |
|      ! 0 |  1696 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1697 | `			}` |
|        - |  1698 | `		}` |
|  1478428 |  1699 | `	}else{` |
|        - |  1700 | `		/* Superglobal */` |
|       38 |  1701 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1702 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1703 | `	}` |
|  2956670 |  1704 | `	return pObj;` |
|  1478733 |  1705 |  |
|        - |  1706 | `/*` |
|        - |  1707 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1708 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1709 | ` */` |
|     1698 |  1710 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1711 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1712 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1713 | `	sxu32 nByte        /* zName length */` |
|        - |  1714 | `	)` |
|        2 |  1715 |  |
|        - |  1716 | `	SyHashEntry *pEntry;` |
|        - |  1717 | `	ph7_value *pValue;` |
|        - |  1718 | `	sxu32 nIdx;` |
|        - |  1719 | `	/* Query the superglobal table */` |
|     1700 |  1720 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     1700 |  1721 | `	if( pEntry == 0 ){` |
|        - |  1722 | `		/* No such entry */` |
|      ! 0 |  1723 | `		return 0;` |
|        - |  1724 | `	}` |
|        - |  1725 | `	/* Extract the superglobal index in the global object pool */` |
|     1700 |  1726 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1727 | `	/* Extract the variable value  */` |
|     1700 |  1728 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     1700 |  1729 | `	return pValue;` |
|      851 |  1730 |  |
|        - |  1731 | `/*` |
|        - |  1732 | ` * Perform a raw hashmap insertion.` |
|        - |  1733 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1734 | ` */` |
|     1696 |  1735 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1736 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1737 | `	const char *zKey,   /* Entry key */` |
|        - |  1738 | `	int nKeylen,        /* zKey length*/` |
|        - |  1739 | `	const char *zData,  /* Entry data */` |
|        - |  1740 | `	int nLen            /* zData length */` |
|        - |  1741 | `	)` |
|        2 |  1742 |  |
|        - |  1743 | `	ph7_value sKey,sValue;` |
|        - |  1744 | `	sxi32 rc;` |
|     1698 |  1745 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     1698 |  1746 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     1698 |  1747 | `	if( zKey ){` |
|     1676 |  1748 | `		if( nKeylen < 0 ){` |
|     1676 |  1749 | `			nKeylen = (int)SyStrlen(zKey);` |
|      837 |  1750 | `		}` |
|     1676 |  1751 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|      837 |  1752 | `	}` |
|     1698 |  1753 | `	if( zData ){` |
|     1698 |  1754 | `		if( nLen < 0 ){` |
|        - |  1755 | `			/* Compute length automatically */` |
|      ! 0 |  1756 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1757 | `		}` |
|     1698 |  1758 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|      848 |  1759 | `	}` |
|        - |  1760 | `	/* Perform the insertion */` |
|     1698 |  1761 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     1698 |  1762 | `	PH7_MemObjRelease(&sKey);` |
|     1698 |  1763 | `	PH7_MemObjRelease(&sValue);` |
|     1698 |  1764 | `	return rc;` |
|        2 |  1765 |  |
|        - |  1766 | `/*` |
|        - |  1767 | ` * Configure a working virtual machine instance.` |
|        - |  1768 | ` *` |
|        - |  1769 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1770 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1771 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1772 | ` * The second argument to this function is an integer configuration option` |
|        - |  1773 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1774 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1775 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1776 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1777 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1778 | ` */` |
|    26776 |  1779 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1780 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1781 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1782 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1783 | `	)` |
|        2 |  1784 |  |
|    26778 |  1785 | `	sxi32 rc = SXRET_OK;` |
|    26778 |  1786 | `	switch(nOp){` |
|      836 |  1787 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     1674 |  1788 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     1674 |  1789 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1790 | `		/* VM output consumer callback */` |
|        - |  1791 | `#ifdef UNTRUST` |
|        - |  1792 | `		if( xConsumer == 0 ){` |
|        - |  1793 | `			rc = SXERR_CORRUPT;` |
|        - |  1794 | `			break;` |
|        - |  1795 | `		}` |
|        - |  1796 | `#endif` |
|        - |  1797 | `		/* Install the output consumer */` |
|     1674 |  1798 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     1674 |  1799 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     1674 |  1800 | `		break;` |
|        - |  1801 | `							   }` |
|      836 |  1802 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1803 | `		/* Import path */` |
|        - |  1804 | `		  const char *zPath;` |
|        - |  1805 | `		  SyString sPath;` |
|     1674 |  1806 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1807 | `#if defined(UNTRUST)` |
|        - |  1808 | `		  if( zPath == 0 ){` |
|        - |  1809 | `			  rc = SXERR_EMPTY;` |
|        - |  1810 | `			  break;` |
|        - |  1811 | `		  }` |
|        - |  1812 | `#endif` |
|     1674 |  1813 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1814 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1815 | `#ifdef __WINNT__` |
|        2 |  1816 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1817 | `#endif` |
|     3346 |  1818 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1819 | `		  /* Remove leading and trailing white spaces */` |
|     1674 |  1820 | `		  SyStringFullTrim(&sPath);` |
|     1674 |  1821 | `		  if( sPath.nByte > 0 ){` |
|        - |  1822 | `			  /* Store the path in the corresponding conatiner */` |
|     1674 |  1823 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      836 |  1824 | `		  }` |
|     1674 |  1825 | `		  break;` |
|        - |  1826 | `									 }` |
|      836 |  1827 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1828 | `		/* Run-Time Error report */` |
|     1674 |  1829 | `		pVm->bErrReport = 1;` |
|     1674 |  1830 | `		break;` |
|      ! 0 |  1831 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1832 | `		/* Recursion depth */` |
|      ! 0 |  1833 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1834 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1835 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1836 | `		}` |
|      ! 0 |  1837 | `		break;` |
|        - |  1838 | `									   }` |
|      ! 0 |  1839 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1840 | `		/* VM output length in bytes */` |
|      ! 0 |  1841 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1842 | `#ifdef UNTRUST` |
|        - |  1843 | `		if( pOut == 0 ){` |
|        - |  1844 | `			rc = SXERR_CORRUPT;` |
|        - |  1845 | `			break;` |
|        - |  1846 | `		}` |
|        - |  1847 | `#endif` |
|      ! 0 |  1848 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1849 | `		break;` |
|        - |  1850 | `							   }` |
|        - |  1851 |  |
|     8360 |  1852 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1853 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1854 | `		/* Create a new superglobal/global variable */` |
|    16722 |  1855 | `		const char *zName = va_arg(ap,const char *);` |
|    16722 |  1856 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1857 | `		SyHashEntry *pEntry;` |
|        - |  1858 | `		ph7_value *pObj;` |
|        - |  1859 | `		sxu32 nByte;` |
|        - |  1860 | `		sxu32 nIdx;` |
|        - |  1861 | `#ifdef UNTRUST` |
|        - |  1862 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1863 | `			rc = SXERR_CORRUPT;` |
|        - |  1864 | `			break;` |
|        - |  1865 | `		}` |
|        - |  1866 | `#endif` |
|    16722 |  1867 | `		nByte = SyStrlen(zName);` |
|    16722 |  1868 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1869 | `			/* Check if the superglobal is already installed */` |
|    16722 |  1870 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     8362 |  1871 | `		}else{` |
|        - |  1872 | `			/* Query the top active VM frame */` |
|      ! 0 |  1873 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1874 | `		}` |
|    16722 |  1875 | `		if( pEntry ){` |
|        - |  1876 | `			/* Variable already installed */` |
|      ! 0 |  1877 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1878 | `			/* Extract contents */` |
|      ! 0 |  1879 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  1880 | `			if( pObj ){` |
|        - |  1881 | `				/* Overwrite old contents */` |
|      ! 0 |  1882 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  1883 | `			}` |
|      ! 0 |  1884 | `		}else{` |
|        - |  1885 | `			/* Install a new variable */` |
|    16722 |  1886 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    16722 |  1887 | `			if( pObj == 0 ){` |
|      ! 0 |  1888 | `				rc = SXERR_MEM;` |
|      ! 0 |  1889 | `				break;` |
|        - |  1890 | `			}` |
|    16722 |  1891 | `			nIdx = pObj->nIdx;` |
|        - |  1892 | `			/* Copy value */` |
|    16722 |  1893 | `			PH7_MemObjStore(pValue,pObj);` |
|    16722 |  1894 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1895 | `				/* Install the superglobal */` |
|    16722 |  1896 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     8362 |  1897 | `			}else{` |
|        - |  1898 | `				/* Install in the current frame */` |
|      ! 0 |  1899 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1900 | `			}` |
|    16722 |  1901 | `			if( rc == SXRET_OK ){` |
|        - |  1902 | `				SyHashEntry *pRef;` |
|    16722 |  1903 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    16722 |  1904 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     8362 |  1905 | `				}else{` |
|      ! 0 |  1906 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1907 | `				}` |
|        - |  1908 | `				/* Install in the reference table */` |
|    16722 |  1909 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    16722 |  1910 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1911 | `					/* Register in the $GLOBALS array */` |
|    16722 |  1912 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     8360 |  1913 | `				}` |
|     8360 |  1914 | `			}` |
|        - |  1915 | `		}` |
|    16722 |  1916 | `		break;` |
|        - |  1917 | `									}` |
|      837 |  1918 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1919 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1920 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1921 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1922 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1923 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1924 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     1676 |  1925 | `		const char *zKey   = va_arg(ap,const char *);` |
|     1676 |  1926 | `		const char *zValue = va_arg(ap,const char *);` |
|     1676 |  1927 | `		int nLen = va_arg(ap,int);` |
|        - |  1928 | `		ph7_hashmap *pMap;` |
|        - |  1929 | `		ph7_value *pValue;` |
|     1676 |  1930 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1931 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1932 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     1675 |  1933 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  1934 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  1935 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     1674 |  1936 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  1937 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  1938 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     1674 |  1939 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  1940 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  1941 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     1674 |  1942 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  1943 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  1944 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     1674 |  1945 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  1946 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  1947 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  1948 | `		}else{` |
|        - |  1949 | `			/* Extract the $_SERVER superglobal */` |
|     1674 |  1950 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  1951 | `		}` |
|     1676 |  1952 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1953 | `			/* No such entry */` |
|      ! 0 |  1954 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1955 | `			break;` |
|        - |  1956 | `		}` |
|        - |  1957 | `		/* Point to the hashmap */` |
|     1676 |  1958 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1959 | `		/* Perform the insertion */` |
|     1676 |  1960 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     1676 |  1961 | `		break;` |
|        - |  1962 | `								   }` |
|       11 |  1963 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  1964 | `		/* Script arguments */` |
|       24 |  1965 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  1966 | `		ph7_hashmap *pMap;` |
|        - |  1967 | `		ph7_value *pValue;` |
|        - |  1968 | `		sxu32 n;` |
|       24 |  1969 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  1970 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  1971 | `			break;` |
|        - |  1972 | `		}` |
|        - |  1973 | `		/* Extract the $argv array */` |
|       24 |  1974 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  1975 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1976 | `			/* No such entry */` |
|      ! 0 |  1977 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1978 | `			break;` |
|        - |  1979 | `		}` |
|        - |  1980 | `		/* Point to the hashmap */` |
|       24 |  1981 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1982 | `		/* Perform the insertion */` |
|       24 |  1983 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  1984 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  1985 | `		if( rc == SXRET_OK ){` |
|       24 |  1986 | `			if( pMap->nEntry > 1 ){` |
|        - |  1987 | `				/* Append space separator first */` |
|       18 |  1988 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  1989 | `			}` |
|       24 |  1990 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  1991 | `		}` |
|       24 |  1992 | `		break;` |
|        - |  1993 | `								  }` |
|      ! 0 |  1994 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  1995 | `		/* error_log() consumer */` |
|      ! 0 |  1996 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  1997 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  1998 | `		break;` |
|        - |  1999 | `										}` |
|      ! 0 |  2000 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2001 | `		/* Script return value */` |
|      ! 0 |  2002 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2003 | `#ifdef UNTRUST` |
|        - |  2004 | `		if( ppValue == 0 ){` |
|        - |  2005 | `			rc = SXERR_CORRUPT;` |
|        - |  2006 | `			break;` |
|        - |  2007 | `		}` |
|        - |  2008 | `#endif` |
|      ! 0 |  2009 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2010 | `		break;` |
|        - |  2011 | `								   }` |
|     1672 |  2012 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2013 | `		/* Register an IO stream device */` |
|     3346 |  2014 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2015 | `		/* Make sure we are dealing with a valid IO stream */` |
|     5016 |  2016 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     3346 |  2017 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2018 | `				/* Invalid stream */` |
|      ! 0 |  2019 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2020 | `				break;` |
|        - |  2021 | `		}` |
|     3346 |  2022 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2023 | `			/* Make the 'file://' stream the defaut stream device */` |
|     1674 |  2024 | `			pVm->pDefStream = pStream;` |
|      836 |  2025 | `		}` |
|        - |  2026 | `		/* Insert in the appropriate container */` |
|     3346 |  2027 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     3346 |  2028 | `		break;` |
|        - |  2029 | `								  }` |
|      ! 0 |  2030 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2031 | `		/* Point to the VM internal output consumer buffer */` |
|      ! 0 |  2032 | `		const void **ppOut = va_arg(ap,const void **);` |
|      ! 0 |  2033 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2034 | `#ifdef UNTRUST` |
|        - |  2035 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2036 | `			rc = SXERR_CORRUPT;` |
|        - |  2037 | `			break;` |
|        - |  2038 | `		}` |
|        - |  2039 | `#endif` |
|      ! 0 |  2040 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|      ! 0 |  2041 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|      ! 0 |  2042 | `		break;` |
|        - |  2043 | `									   }` |
|      ! 0 |  2044 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2045 | `		/* Raw HTTP request*/` |
|      ! 0 |  2046 | `		const char *zRequest = va_arg(ap,const char *);` |
|      ! 0 |  2047 | `		int nByte = va_arg(ap,int);` |
|      ! 0 |  2048 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2049 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2050 | `			break;` |
|        - |  2051 | `		}` |
|      ! 0 |  2052 | `		if( nByte < 0 ){` |
|        - |  2053 | `			/* Compute length automatically */` |
|      ! 0 |  2054 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2055 | `		}` |
|        - |  2056 | `		/* Process the request */` |
|      ! 0 |  2057 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|      ! 0 |  2058 | `		break;` |
|        - |  2059 | `									}` |
|      ! 0 |  2060 | `	default:` |
|        - |  2061 | `		/* Unknown configuration option */` |
|      ! 0 |  2062 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2063 | `		break;` |
|        - |  2064 | `	}` |
|    26778 |  2065 | `	return rc;` |
|        2 |  2066 |  |
|        - |  2067 | `/* Forward declaration */` |
|        - |  2068 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2069 | `/*` |
|        - |  2070 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2071 | ` * format.` |
|        - |  2072 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2073 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2074 | ` * (STDOUT).` |
|        - |  2075 | ` */` |
|        2 |  2076 | `static sxi32 VmByteCodeDump(` |
|        - |  2077 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2078 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2079 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2080 | `	)` |
|        1 |  2081 |  |
|        - |  2082 | `	static const char zDump[] = {` |
|        - |  2083 | `		"====================================================\n"` |
|        - |  2084 | `		"PH7 VM Dump\n"` |
|        - |  2085 | `		"====================================================\n"` |
|        - |  2086 | `	};` |
|        - |  2087 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2088 | `	sxi32 rc = SXRET_OK;` |
|        - |  2089 | `	sxu32 n;` |
|        - |  2090 | `	/* Point to the PH7 instructions */` |
|        3 |  2091 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2092 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2093 | `	n = 0;` |
|        3 |  2094 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2095 | `	/* Dump instructions */` |
|        6 |  2096 | `	for(;;){` |
|       13 |  2097 | `		if( pInstr >= pEnd ){` |
|        - |  2098 | `			/* No more instructions */` |
|        3 |  2099 | `			break;` |
|        - |  2100 | `		}` |
|        - |  2101 | `		/* Format and call the consumer callback */` |
|       16 |  2102 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       10 |  2103 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       10 |  2104 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       11 |  2105 | `		if( rc != SXRET_OK ){` |
|        - |  2106 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2107 | `			return rc;` |
|        - |  2108 | `		}` |
|       11 |  2109 | `		++n;` |
|       11 |  2110 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2111 | `	}` |
|        3 |  2112 | `	return rc;` |
|        2 |  2113 |  |
|        - |  2114 | `/* Forward declaration */` |
|        - |  2115 | `static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData);` |
|        - |  2116 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2117 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2118 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2119 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2120 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2121 | `/*` |
|        - |  2122 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2123 | ` * consumer callback.` |
|        - |  2124 | ` */` |
|      436 |  2125 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2126 |  |
|      437 |  2127 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      437 |  2128 | `	sxi32 rc = SXRET_OK;` |
|        - |  2129 | `	/* Append a new line */` |
|        - |  2130 | `#ifdef __WINNT__` |
|        1 |  2131 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2132 | `#else` |
|      436 |  2133 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2134 | `#endif` |
|        - |  2135 | `	/* Invoke the output consumer callback */` |
|      437 |  2136 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      437 |  2137 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  2138 | `		/* Increment output length */` |
|      437 |  2139 | `		pVm->nOutputLen += SyBlobLength(pMsg);` |
|      218 |  2140 | `	}` |
|      437 |  2141 | `	return rc;` |
|        1 |  2142 |  |
|        - |  2143 | `/*` |
|        - |  2144 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2145 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2146 | ` * information.` |
|        - |  2147 | ` */` |
|      138 |  2148 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2149 |  |
|      140 |  2150 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2151 | `		ph7_value apArg[4];` |
|        - |  2152 | `		ph7_value *apArgPtr[4];` |
|        - |  2153 | `		ph7_value sResult;` |
|        - |  2154 | `		SyString sErr;` |
|        - |  2155 | `		/* Prepare arguments */` |
|       61 |  2156 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2157 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2158 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2159 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2160 | `		if( pFile ){` |
|       61 |  2161 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2162 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2163 | `		}else{` |
|      ! 0 |  2164 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2165 | `		}` |
|       61 |  2166 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2167 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2168 | `		/* Set up pointer array */` |
|       61 |  2169 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2170 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2171 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2172 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2173 | `		/* Call the handler */` |
|       61 |  2174 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2175 | `		/* Check return value */` |
|       61 |  2176 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2177 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2178 | `		}` |
|        - |  2179 | `		/* Release */` |
|       61 |  2180 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2181 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2182 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2183 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2184 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2185 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2186 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2187 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2188 | `	}` |
|        - |  2189 | `	/* No handler, always call error handler */` |
|       79 |  2190 | `	return TRUE;` |
|       71 |  2191 |  |
|      102 |  2192 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2193 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2194 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2195 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2196 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2197 | `	)` |
|        2 |  2198 |  |
|      104 |  2199 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2200 | `	SyString *pFile;` |
|        - |  2201 | `	char *zErr;` |
|      104 |  2202 | `	sxi32 rc = SXRET_OK;` |
|      104 |  2203 | `	if( !pVm->bErrReport ){` |
|        - |  2204 | `		/* Don't bother reporting errors */` |
|        3 |  2205 | `		return SXRET_OK;` |
|        - |  2206 | `	}` |
|        - |  2207 | `	/* Reset the working buffer */` |
|      102 |  2208 | `	SyBlobReset(pWorker);` |
|        - |  2209 | `	/* Peek the processed file if available */` |
|      102 |  2210 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      102 |  2211 | `	if( pFile ){` |
|        - |  2212 | `		/* Append file name */` |
|      102 |  2213 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|      102 |  2214 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       50 |  2215 | `	}` |
|        - |  2216 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2217 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2218 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2219 | `	 * E_DEPRECATED). */` |
|      102 |  2220 | `	zErr = "Error:  ";` |
|      102 |  2221 | `	switch(iErr){` |
|       21 |  2222 | `	case PH7_CTX_WARNING:` |
|       44 |  2223 | `		zErr = "Warning:  ";` |
|       44 |  2224 | `		break;` |
|        6 |  2225 | `	case PH7_CTX_NOTICE:` |
|       14 |  2226 | `		zErr = "Notice:  ";` |
|       12 |  2227 | `		break;` |
|       23 |  2228 | `	default:` |
|        - |  2229 | `		/* keep iErr unchanged */` |
|       46 |  2230 | `		break;` |
|        - |  2231 | `	}` |
|      102 |  2232 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|      102 |  2233 | `	if( pFuncName ){` |
|        - |  2234 | `		/* Append function name first */` |
|       29 |  2235 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       29 |  2236 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       14 |  2237 | `	}` |
|      102 |  2238 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2239 | `	/* Check for user error handler.  compute length of C string */` |
|      102 |  2240 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       53 |  2241 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       26 |  2242 | `	}` |
|      102 |  2243 | `	return rc;` |
|       53 |  2244 |  |
|        - |  2245 | `/*` |
|        - |  2246 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2247 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2248 | ` * information.` |
|        - |  2249 | ` */` |
|       38 |  2250 | `static sxi32 VmThrowErrorAp(` |
|        - |  2251 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2252 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2253 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2254 | `	const char *zFormat, /* Format message */` |
|        - |  2255 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2256 | `	)` |
|        2 |  2257 |  |
|       40 |  2258 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2259 | `	SyBlob sMsg;` |
|        - |  2260 | `	SyString *pFile;` |
|        - |  2261 | `	char *zErr;` |
|       40 |  2262 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2263 | `	if( !pVm->bErrReport ){` |
|        - |  2264 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2265 | `		return SXRET_OK;` |
|        - |  2266 | `	}` |
|        - |  2267 | `	/* Reset the working buffer */` |
|       40 |  2268 | `	SyBlobReset(pWorker);` |
|        - |  2269 | `	/* Peek the processed file if available */` |
|       40 |  2270 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2271 | `	if( pFile ){` |
|        - |  2272 | `		/* Append file name */` |
|       40 |  2273 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2274 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2275 | `	}` |
|        - |  2276 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2277 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2278 | `	 * the correct errno value. */` |
|       40 |  2279 | `	zErr = "Error:  ";` |
|       40 |  2280 | `	switch(iErr){` |
|        4 |  2281 | `	case PH7_CTX_WARNING:` |
|        9 |  2282 | `		zErr = "Warning:  ";` |
|        9 |  2283 | `		break;` |
|        3 |  2284 | `	case PH7_CTX_NOTICE:` |
|        7 |  2285 | `		zErr = "Notice:  ";` |
|        6 |  2286 | `		break;` |
|       12 |  2287 | `	default:` |
|        - |  2288 | `		/* do not change iErr */` |
|       24 |  2289 | `		break;` |
|        - |  2290 | `	}` |
|       40 |  2291 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2292 | `	if( pFuncName ){` |
|        - |  2293 | `		/* Append function name first */` |
|       26 |  2294 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2295 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2296 | `	}` |
|        - |  2297 | `	/* Format the raw message */` |
|       40 |  2298 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2299 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2300 | `	/* Check if a user error handler is installed */` |
|       40 |  2301 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2302 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2303 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2304 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2305 | `	}` |
|       40 |  2306 | `	SyBlobRelease(&sMsg);` |
|       40 |  2307 | `	return rc;` |
|       21 |  2308 |  |
|        - |  2309 | `/*` |
|        - |  2310 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2311 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2312 | ` * information.` |
|        - |  2313 | ` * ------------------------------------` |
|        - |  2314 | ` * Simple boring wrapper function.` |
|        - |  2315 | ` * ------------------------------------` |
|        - |  2316 | ` */` |
|       14 |  2317 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2318 |  |
|        - |  2319 | `	va_list ap;` |
|        - |  2320 | `	sxi32 rc;` |
|       15 |  2321 | `	va_start(ap,zFormat);` |
|       15 |  2322 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2323 | `	va_end(ap);` |
|       15 |  2324 | `	return rc;` |
|        1 |  2325 |  |
|        - |  2326 | `/*` |
|        - |  2327 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2328 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2329 | ` * information.` |
|        - |  2330 | ` * ------------------------------------` |
|        - |  2331 | ` * Simple boring wrapper function.` |
|        - |  2332 | ` * ------------------------------------` |
|        - |  2333 | ` */` |
|       24 |  2334 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2335 |  |
|        - |  2336 | `	sxi32 rc;` |
|       26 |  2337 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2338 | `	return rc;` |
|        2 |  2339 |  |
|        - |  2340 | `/*` |
|        - |  2341 | ` * Resolve function context from the current frame.` |
|        - |  2342 | ` */` |
|      712 |  2343 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2344 |  |
|        - |  2345 | `	VmFrame *pFrame;` |
|        - |  2346 | `	ph7_vm_func *pFunc;` |
|      713 |  2347 | `	*pzFuncName = 0;` |
|      713 |  2348 | `	*pnFuncLen = 0;` |
|      713 |  2349 | `	pFrame = pVm->pFrame;` |
|      713 |  2350 | `	if( pFrame == 0 ){` |
|      ! 0 |  2351 | `		return;` |
|        - |  2352 | `	}` |
|      713 |  2353 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  2354 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  2355 | `	}` |
|      713 |  2356 | `	if( pFrame->pParent == 0 ){` |
|      709 |  2357 | `		return;` |
|        - |  2358 | `	}` |
|        5 |  2359 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  2360 | `	if( pFunc == 0 ){` |
|      ! 0 |  2361 | `		return;` |
|        - |  2362 | `	}` |
|        5 |  2363 | `	*pzFuncName = pFunc->sName.zString;` |
|        5 |  2364 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      357 |  2365 |  |
|        - |  2366 | `/*` |
|        - |  2367 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2368 | ` */` |
|      358 |  2369 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2370 |  |
|        - |  2371 | `	SyBlob sOut;` |
|        - |  2372 | `	SyString *pFile;` |
|      359 |  2373 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2374 | `		return PH7_OK;` |
|        - |  2375 | `	}` |
|      359 |  2376 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2377 | `		zClass = "Exception";` |
|      ! 0 |  2378 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2379 | `	}` |
|      359 |  2380 | `	if( zMsg == 0 ){` |
|      ! 0 |  2381 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2382 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2383 | `	}` |
|      359 |  2384 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      355 |  2385 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      177 |  2386 | `	}` |
|      359 |  2387 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      359 |  2388 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      359 |  2389 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      359 |  2390 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      359 |  2391 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      359 |  2392 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      359 |  2393 | `	if( pFile ){` |
|      359 |  2394 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      359 |  2395 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      359 |  2396 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      179 |  2397 | `	}` |
|      359 |  2398 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      359 |  2399 | `	if( pFile ){` |
|      359 |  2400 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      359 |  2401 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      359 |  2402 | `		if( zFuncName && nFuncLen > 0 ){` |
|        5 |  2403 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        3 |  2404 | `		}else{` |
|      355 |  2405 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2406 | `		}` |
|      179 |  2407 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2408 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2409 | `	}else{` |
|      ! 0 |  2410 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2411 | `	}` |
|      359 |  2412 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      359 |  2413 | `	if( pFile ){` |
|      359 |  2414 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      359 |  2415 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      359 |  2416 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      359 |  2417 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      179 |  2418 | `	}` |
|      359 |  2419 | `	VmCallErrorHandler(pVm,&sOut);` |
|      359 |  2420 | `	SyBlobRelease(&sOut);` |
|      359 |  2421 | `	return PH7_ABORT;` |
|      180 |  2422 |  |
|        - |  2423 | `/*` |
|        - |  2424 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2425 | ` */` |
|      354 |  2426 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2427 |  |
|        - |  2428 | `	ph7_vm *pVm;` |
|        - |  2429 | `	ph7_class *pClass;` |
|        - |  2430 | `	ph7_class_instance *pThis;` |
|        - |  2431 | `	ph7_class_method *pCons;` |
|        - |  2432 | `	ph7_value sArg;` |
|        - |  2433 | `	ph7_value *apArg[1];` |
|        - |  2434 | `	SyBlob sMsg;` |
|        - |  2435 | `	SyString sMsgStr;` |
|        - |  2436 | `	VmFrame *pFrame;` |
|        - |  2437 | `	va_list ap;` |
|        - |  2438 | `	sxi32 rc;` |
|        - |  2439 |  |
|      356 |  2440 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2441 | `		return PH7_ABORT;` |
|        - |  2442 | `	}` |
|      356 |  2443 | `	pVm = pCtx->pVm;` |
|      356 |  2444 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2445 | `		zClass = "Error";` |
|      ! 0 |  2446 | `	}` |
|      356 |  2447 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      356 |  2448 | `	if( pClass == 0 ){` |
|      ! 0 |  2449 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2450 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2451 | `			zClass` |
|        - |  2452 | `			);` |
|        - |  2453 | `	}` |
|      356 |  2454 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      356 |  2455 | `	if( pThis == 0 ){` |
|      ! 0 |  2456 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2457 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2458 | `			);` |
|        - |  2459 | `	}` |
|        - |  2460 |  |
|      356 |  2461 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      356 |  2462 | `	va_start(ap,zFormat);` |
|      356 |  2463 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      356 |  2464 | `	va_end(ap);` |
|        - |  2465 |  |
|      356 |  2466 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      356 |  2467 | `	if( pCons ){` |
|      356 |  2468 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      356 |  2469 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      356 |  2470 | `		apArg[0] = &sArg;` |
|      356 |  2471 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      356 |  2472 | `		PH7_MemObjRelease(&sArg);` |
|      177 |  2473 | `	}` |
|      356 |  2474 | `	SyBlobRelease(&sMsg);` |
|        - |  2475 |  |
|      356 |  2476 | `	pFrame = pVm->pFrame;` |
|      356 |  2477 | `	if( pFrame ){` |
|      358 |  2478 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        3 |  2479 | `			pFrame = pFrame->pParent;` |
|        1 |  2480 | `		}` |
|      356 |  2481 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      177 |  2482 | `	}` |
|      356 |  2483 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      356 |  2484 | `	PH7_ClassInstanceUnref(pThis);` |
|      356 |  2485 | `	if( rc == SXERR_ABORT ){` |
|      353 |  2486 | `		return PH7_ABORT;` |
|        - |  2487 | `	}` |
|        3 |  2488 | `	return PH7_EXCEPTION;` |
|      179 |  2489 |  |
|        - |  2490 | `/*` |
|        - |  2491 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2492 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2493 | ` */` |
|      ! 0 |  2494 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2495 |  |
|        - |  2496 | `	ph7_vm *pVm;` |
|        - |  2497 | `	SyBlob sMsg;` |
|      ! 0 |  2498 | `	const char *zFuncName = 0;` |
|      ! 0 |  2499 | `	int nFuncLen = 0;` |
|        - |  2500 | `	va_list ap;` |
|        - |  2501 | `	sxi32 rc;` |
|        - |  2502 |  |
|      ! 0 |  2503 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2504 | `		return PH7_OK;` |
|        - |  2505 | `	}` |
|      ! 0 |  2506 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2507 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2508 | `		zClass = "Error";` |
|      ! 0 |  2509 | `	}` |
|        - |  2510 |  |
|      ! 0 |  2511 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2512 |  |
|      ! 0 |  2513 | `	va_start(ap,zFormat);` |
|      ! 0 |  2514 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2515 | `	va_end(ap);` |
|        - |  2516 |  |
|      ! 0 |  2517 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2518 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2519 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2520 | `	}` |
|      ! 0 |  2521 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2522 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2523 | `	}` |
|      ! 0 |  2524 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2525 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2526 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2527 | `	return rc;` |
|      ! 0 |  2528 |  |
|        - |  2529 | `/*` |
|        - |  2530 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2531 | ` *` |
|        - |  2532 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2533 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2534 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2535 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2536 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2537 | ` * then the program execution is halted.` |
|        - |  2538 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2539 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2540 | ` * or to reset the VM to it's initial state.` |
|        - |  2541 | ` */` |
|    26958 |  2542 | `static sxi32 VmByteCodeExec(` |
|        - |  2543 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2544 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2545 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2546 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2547 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2548 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2549 | `	int is_callback      /* TRUE if we are executing a callback */` |
|        - |  2550 | `	)` |
|        2 |  2551 |  |
|        - |  2552 | `	VmInstr *pInstr;` |
|        - |  2553 | `	ph7_value *pTos;` |
|        - |  2554 | `	SySet aArg;` |
|        - |  2555 | `	sxi32 pc;` |
|        - |  2556 | `	sxi32 rc;` |
|        - |  2557 | `	/* Argument container */` |
|    26960 |  2558 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    26960 |  2559 | `	if( nTos < 0 ){` |
|    25610 |  2560 | `		pTos = &pStack[-1];` |
|    12806 |  2561 | `	}else{` |
|     1352 |  2562 | `		pTos = &pStack[nTos];` |
|        - |  2563 | `	}` |
|    26960 |  2564 | `	pc = 0;` |
|        - |  2565 | `	/* Execute as much as we can */` |
|  4710173 |  2566 | `	for(;;){` |
|        - |  2567 | `		/* Fetch the instruction to execute */` |
|  9419644 |  2568 | `		pInstr = &aInstr[pc];` |
|  9419644 |  2569 | `		rc = SXRET_OK;` |
|        - |  2570 | `/*` |
|        - |  2571 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2572 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2573 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2574 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2575 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2576 | ` */` |
|  9419644 |  2577 | `		switch(pInstr->iOp){` |
|        - |  2578 | `/*` |
|        - |  2579 | ` * DONE: P1 * *` |
|        - |  2580 | ` *` |
|        - |  2581 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2582 | ` * and return immediately.` |
|        - |  2583 | ` */` |
|    13291 |  2584 | `case PH7_OP_DONE:` |
|    26584 |  2585 | `	if( pInstr->iP1 ){` |
|        - |  2586 | `#ifdef UNTRUST` |
|        - |  2587 | `		if( pTos < pStack ){` |
|        - |  2588 | `			goto Abort;` |
|        - |  2589 | `		}` |
|        - |  2590 | `#endif` |
|    15128 |  2591 | `		if( pLastRef ){` |
|     9900 |  2592 | `			*pLastRef = pTos->nIdx;` |
|     4949 |  2593 | `		}` |
|    15128 |  2594 | `		if( pResult ){` |
|        - |  2595 | `			/* Execution result */` |
|    14452 |  2596 | `			PH7_MemObjStore(pTos,pResult);` |
|     7225 |  2597 | `		}` |
|    15128 |  2598 | `		VmPopOperand(&pTos,1);` |
|    19021 |  2599 | `	}else if( pLastRef ){` |
|        - |  2600 | `		/* Nothing referenced */` |
|      762 |  2601 | `		*pLastRef = SXU32_HIGH;` |
|      380 |  2602 | `	}` |
|    26584 |  2603 | `	goto Done;` |
|        - |  2604 | `/*` |
|        - |  2605 | ` * HALT: P1 * *` |
|        - |  2606 | ` *` |
|        - |  2607 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2608 | ` * and abort immediately.` |
|        - |  2609 | ` */` |
|        4 |  2610 | `case PH7_OP_HALT:` |
|        9 |  2611 | `	if( pInstr->iP1 ){` |
|        - |  2612 | `#ifdef UNTRUST` |
|        - |  2613 | `		if( pTos < pStack ){` |
|        - |  2614 | `			goto Abort;` |
|        - |  2615 | `		}` |
|        - |  2616 | `#endif` |
|        9 |  2617 | `		if( pLastRef ){` |
|      ! 0 |  2618 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2619 | `		}` |
|        9 |  2620 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2621 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2622 | `				/* Output the exit message */` |
|        7 |  2623 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2624 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2625 | `				if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  2626 | `					/* Increment output length */` |
|        5 |  2627 | `					pVm->nOutputLen += SyBlobLength(&pTos->sBlob);` |
|        2 |  2628 | `				}` |
|        3 |  2629 | `			}` |
|        7 |  2630 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2631 | `			/* Record exit status */` |
|        5 |  2632 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2633 | `		}` |
|        9 |  2634 | `		VmPopOperand(&pTos,1);` |
|        4 |  2635 | `	}else if( pLastRef ){` |
|        - |  2636 | `		/* Nothing referenced */` |
|      ! 0 |  2637 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2638 | `	}` |
|        - |  2639 | `	/* Check if we're in an included file context */` |
|        9 |  2640 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2641 | `		/* Terminate the entire process */` |
|        9 |  2642 | `		exit(pVm->iExitStatus);` |
|        - |  2643 | `	}` |
|      ! 0 |  2644 | `	goto Abort;` |
|        - |  2645 | `/*` |
|        - |  2646 | ` * JMP: * P2 *` |
|        - |  2647 | ` *` |
|        - |  2648 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2649 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2650 | ` */` |
|   206992 |  2651 | `case PH7_OP_JMP:` |
|   414030 |  2652 | `	pc = pInstr->iP2 - 1;` |
|   414030 |  2653 | `	break;` |
|        - |  2654 | `/*` |
|        - |  2655 | ` * JZ: P1 P2 *` |
|        - |  2656 | ` *` |
|        - |  2657 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2658 | ` * entry in the stack if P1 is zero.` |
|        - |  2659 | ` */` |
|   478032 |  2660 | `case PH7_OP_JZ:` |
|        - |  2661 | `#ifdef UNTRUST` |
|        - |  2662 | `	if( pTos < pStack ){` |
|        - |  2663 | `		goto Abort;` |
|        - |  2664 | `	}` |
|        - |  2665 | `#endif` |
|        - |  2666 | `	/* Get a boolean value */` |
|   956154 |  2667 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       77 |  2668 | `		PH7_MemObjToBool(pTos);` |
|       38 |  2669 | `	}` |
|   956154 |  2670 | `	if( !pTos->x.iVal ){` |
|        - |  2671 | `		/* Take the jump */` |
|   458792 |  2672 | `		pc = pInstr->iP2 - 1;` |
|   229395 |  2673 | `	}` |
|   956154 |  2674 | `	if( !pInstr->iP1 ){` |
|   752614 |  2675 | `		VmPopOperand(&pTos,1);` |
|   376328 |  2676 | `	}` |
|   956154 |  2677 | `	break;` |
|        - |  2678 | `/*` |
|        - |  2679 | ` * JNZ: P1 P2 *` |
|        - |  2680 | ` *` |
|        - |  2681 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2682 | ` * entry in the stack if P1 is zero.` |
|        - |  2683 | ` */` |
|    51022 |  2684 | `case PH7_OP_JNZ:` |
|        - |  2685 | `#ifdef UNTRUST` |
|        - |  2686 | `	if( pTos < pStack ){` |
|        - |  2687 | `		goto Abort;` |
|        - |  2688 | `	}` |
|        - |  2689 | `#endif` |
|        - |  2690 | `	/* Get a boolean value */` |
|   102046 |  2691 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2692 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2693 | `	}` |
|   102046 |  2694 | `	if( pTos->x.iVal ){` |
|        - |  2695 | `		/* Take the jump */` |
|     3938 |  2696 | `		pc = pInstr->iP2 - 1;` |
|     1968 |  2697 | `	}` |
|   102046 |  2698 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2699 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2700 | `	}` |
|   102046 |  2701 | `	break;` |
|        - |  2702 | `/*` |
|        - |  2703 | ` * NOOP: * * *` |
|        - |  2704 | ` *` |
|        - |  2705 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2706 | ` * destination.` |
|        - |  2707 | ` */` |
|      ! 0 |  2708 | `case PH7_OP_NOOP:` |
|      ! 0 |  2709 | `	break;` |
|        - |  2710 | `/*` |
|        - |  2711 | ` * POP: P1 * *` |
|        - |  2712 | ` *` |
|        - |  2713 | ` * Pop P1 elements from the operand stack.` |
|        - |  2714 | ` */` |
|   368584 |  2715 | `case PH7_OP_POP: {` |
|   737214 |  2716 | `	sxi32 n = pInstr->iP1;` |
|   737214 |  2717 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2718 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2719 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2720 | `	}` |
|   737214 |  2721 | `	VmPopOperand(&pTos,n);` |
|   737214 |  2722 | `	break;` |
|        - |  2723 | `				 }` |
|        - |  2724 | `/*` |
|        - |  2725 | ` * CVT_INT: * * *` |
|        - |  2726 | ` *` |
|        - |  2727 | ` * Force the top of the stack to be an integer.` |
|        - |  2728 | ` */` |
|       35 |  2729 | `case PH7_OP_CVT_INT:` |
|        - |  2730 | `#ifdef UNTRUST` |
|        - |  2731 | `	if( pTos < pStack ){` |
|        - |  2732 | `		goto Abort;` |
|        - |  2733 | `	}` |
|        - |  2734 | `#endif` |
|       72 |  2735 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2736 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2737 | `	}` |
|        - |  2738 | `	/* Invalidate any prior representation */` |
|       72 |  2739 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2740 | `	break;` |
|        - |  2741 | `/*` |
|        - |  2742 | ` * CVT_REAL: * * *` |
|        - |  2743 | ` *` |
|        - |  2744 | ` * Force the top of the stack to be a real.` |
|        - |  2745 | ` */` |
|        4 |  2746 | `case PH7_OP_CVT_REAL:` |
|        - |  2747 | `#ifdef UNTRUST` |
|        - |  2748 | `	if( pTos < pStack ){` |
|        - |  2749 | `		goto Abort;` |
|        - |  2750 | `	}` |
|        - |  2751 | `#endif` |
|        9 |  2752 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2753 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2754 | `	}` |
|        - |  2755 | `	/* Invalidate any prior representation */` |
|        9 |  2756 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2757 | `	break;` |
|        - |  2758 | `/*` |
|        - |  2759 | ` * CVT_STR: * * *` |
|        - |  2760 | ` *` |
|        - |  2761 | ` * Force the top of the stack to be a string.` |
|        - |  2762 | ` */` |
|      136 |  2763 | `case PH7_OP_CVT_STR:` |
|        - |  2764 | `#ifdef UNTRUST` |
|        - |  2765 | `	if( pTos < pStack ){` |
|        - |  2766 | `		goto Abort;` |
|        - |  2767 | `	}` |
|        - |  2768 | `#endif` |
|      274 |  2769 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      274 |  2770 | `		PH7_MemObjToString(pTos);` |
|      136 |  2771 | `	}` |
|      274 |  2772 | `	break;` |
|        - |  2773 | `/*` |
|        - |  2774 | ` * CVT_BOOL: * * *` |
|        - |  2775 | ` *` |
|        - |  2776 | ` * Force the top of the stack to be a boolean.` |
|        - |  2777 | ` */` |
|        5 |  2778 | `case PH7_OP_CVT_BOOL:` |
|        - |  2779 | `#ifdef UNTRUST` |
|        - |  2780 | `	if( pTos < pStack ){` |
|        - |  2781 | `		goto Abort;` |
|        - |  2782 | `	}` |
|        - |  2783 | `#endif` |
|       11 |  2784 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2785 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2786 | `	}` |
|       11 |  2787 | `	break;` |
|        - |  2788 | `/*` |
|        - |  2789 | ` * CVT_NULL: * * *` |
|        - |  2790 | ` *` |
|        - |  2791 | ` * Nullify the top of the stack.` |
|        - |  2792 | ` */` |
|        3 |  2793 | `case PH7_OP_CVT_NULL:` |
|        - |  2794 | `#ifdef UNTRUST` |
|        - |  2795 | `	if( pTos < pStack ){` |
|        - |  2796 | `		goto Abort;` |
|        - |  2797 | `	}` |
|        - |  2798 | `#endif` |
|        7 |  2799 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2800 | `	break;` |
|        - |  2801 | `/*` |
|        - |  2802 | ` * CVT_NUMC: * * *` |
|        - |  2803 | ` *` |
|        - |  2804 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2805 | ` */` |
|      ! 0 |  2806 | `case PH7_OP_CVT_NUMC:` |
|        - |  2807 | `#ifdef UNTRUST` |
|        - |  2808 | `	if( pTos < pStack ){` |
|        - |  2809 | `		goto Abort;` |
|        - |  2810 | `	}` |
|        - |  2811 | `#endif` |
|        - |  2812 | `	/* Force a numeric cast */` |
|      ! 0 |  2813 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2814 | `	break;` |
|        - |  2815 | `/*` |
|        - |  2816 | ` * CVT_ARRAY: * * *` |
|        - |  2817 | ` *` |
|        - |  2818 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2819 | ` */` |
|       10 |  2820 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2821 | `#ifdef UNTRUST` |
|        - |  2822 | `	if( pTos < pStack ){` |
|        - |  2823 | `		goto Abort;` |
|        - |  2824 | `	}` |
|        - |  2825 | `#endif` |
|        - |  2826 | `	/* Force a hashmap cast */` |
|       21 |  2827 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  2828 | `	if( rc != SXRET_OK ){` |
|        - |  2829 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  2830 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  2831 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  2832 | `	}` |
|       21 |  2833 | `	break;` |
|        - |  2834 | `/*` |
|        - |  2835 | ` * CVT_OBJ: * * *` |
|        - |  2836 | ` *` |
|        - |  2837 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  2838 | ` */` |
|        8 |  2839 | `case PH7_OP_CVT_OBJ:` |
|        - |  2840 | `#ifdef UNTRUST` |
|        - |  2841 | `	if( pTos < pStack ){` |
|        - |  2842 | `		goto Abort;` |
|        - |  2843 | `	}` |
|        - |  2844 | `#endif` |
|       17 |  2845 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  2846 | `		/* Force a 'stdClass()' cast */` |
|       17 |  2847 | `		PH7_MemObjToObject(pTos);` |
|        8 |  2848 | `	}` |
|       17 |  2849 | `	break;` |
|        - |  2850 | `/*` |
|        - |  2851 | ` * ERR_CTRL * * *` |
|        - |  2852 | ` *` |
|        - |  2853 | ` * Error control operator.` |
|        - |  2854 | ` */` |
|    11428 |  2855 | `case PH7_OP_ERR_CTRL:` |
|        - |  2856 | `	/*` |
|        - |  2857 | `	 * TICKET 1433-038:` |
|        - |  2858 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2859 | `	 * use the public API,to control error output.` |
|        - |  2860 | `	 */` |
|    22856 |  2861 | `	break;` |
|        - |  2862 | `/*` |
|        - |  2863 | ` * IS_A * * *` |
|        - |  2864 | ` *` |
|        - |  2865 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  2866 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  2867 | ` * holding a class name or an object).` |
|        - |  2868 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  2869 | ` */` |
|       11 |  2870 | `case PH7_OP_IS_A:{` |
|       23 |  2871 | `	ph7_value *pNos = &pTos[-1];` |
|       23 |  2872 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  2873 | `#ifdef UNTRUST` |
|        - |  2874 | `	if( pNos < pStack ){` |
|        - |  2875 | `		goto Abort;` |
|        - |  2876 | `	}` |
|        - |  2877 | `#endif` |
|       23 |  2878 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       21 |  2879 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       21 |  2880 | `		ph7_class *pClass = 0;` |
|        - |  2881 | `		/* Extract the target class */` |
|       21 |  2882 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  2883 | `			/* Instance already loaded */` |
|      ! 0 |  2884 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       21 |  2885 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2886 | `			/* Perform the query */` |
|       31 |  2887 | `			pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|       20 |  2888 | `				SyBlobLength(&pTos->sBlob),FALSE,0);` |
|       10 |  2889 | `		}` |
|       21 |  2890 | `		if( pClass ){` |
|        - |  2891 | `			/* Perform the query */` |
|       21 |  2892 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       10 |  2893 | `		}` |
|       10 |  2894 | `	}` |
|        - |  2895 | `	/* Push result */` |
|       23 |  2896 | `	VmPopOperand(&pTos,1);` |
|       23 |  2897 | `	PH7_MemObjRelease(pTos);` |
|       23 |  2898 | `	pTos->x.iVal = iRes;` |
|       23 |  2899 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       23 |  2900 | `	break;` |
|        - |  2901 | `				 }` |
|        - |  2902 |  |
|        - |  2903 | `/*` |
|        - |  2904 | ` * LOADC P1 P2 *` |
|        - |  2905 | ` *` |
|        - |  2906 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  2907 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  2908 | ` */` |
|   756005 |  2909 | `case PH7_OP_LOADC: {` |
|        - |  2910 | `	ph7_value *pObj;` |
|        - |  2911 | `	/* Reserve a room */` |
|  1512056 |  2912 | `	pTos++;` |
|  1512056 |  2913 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1512056 |  2914 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2915 | `			SyHashEntry *pEntry;` |
|        - |  2916 | `			/* Candidate for expansion via user defined callbacks */` |
|    17292 |  2917 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    17292 |  2918 | `			if( pEntry ){` |
|    14210 |  2919 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2920 | `				/* Set a NULL default value */` |
|    14210 |  2921 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    14210 |  2922 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2923 | `				/* Invoke the callback and deal with the expanded value */` |
|    14210 |  2924 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  2925 | `				/* Mark as constant */` |
|    14210 |  2926 | `				pTos->nIdx = SXU32_HIGH;` |
|    14210 |  2927 | `				break;` |
|        - |  2928 | `			}` |
|     1541 |  2929 | `		}` |
|  1497848 |  2930 | `		PH7_MemObjLoad(pObj,pTos);` |
|   748947 |  2931 | `	}else{` |
|        - |  2932 | `		/* Set a NULL value */` |
|      ! 0 |  2933 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2934 | `	}` |
|        - |  2935 | `	/* Mark as constant */` |
|  1497848 |  2936 | `	pTos->nIdx = SXU32_HIGH;` |
|  1497848 |  2937 | `	break;` |
|        - |  2938 | `				  }` |
|        - |  2939 | `/*` |
|        - |  2940 | ` * LOAD: P1 * P3` |
|        - |  2941 | ` *` |
|        - |  2942 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  2943 | ` * from the P3 operand.` |
|        - |  2944 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  2945 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  2946 | ` */` |
|  1301715 |  2947 | `case PH7_OP_LOAD:{` |
|        - |  2948 | `	ph7_value *pObj;` |
|        - |  2949 | `	SyString sName;` |
|  2603652 |  2950 | `	if( pInstr->p3 == 0 ){` |
|        - |  2951 | `		/* Take the variable name from the top of the stack */` |
|        - |  2952 | `#ifdef UNTRUST` |
|        - |  2953 | `		if( pTos < pStack ){` |
|        - |  2954 | `			goto Abort;` |
|        - |  2955 | `		}` |
|        - |  2956 | `#endif` |
|        - |  2957 | `		/* Force a string cast */` |
|       19 |  2958 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2959 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  2960 | `		}` |
|       19 |  2961 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  2962 | `	}else{` |
|  2603634 |  2963 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  2964 | `		/* Reserve a room for the target object */` |
|  2603634 |  2965 | `		pTos++;` |
|        - |  2966 | `	}` |
|        - |  2967 | `	/* Extract the requested memory object */` |
|  2603652 |  2968 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2603652 |  2969 | `	if( pObj == 0 ){` |
|      568 |  2970 | `		if( pInstr->iP1 ){` |
|        - |  2971 | `			/* Variable not found,load NULL */` |
|      568 |  2972 | `			if( !pInstr->p3 ){` |
|      ! 0 |  2973 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  2974 | `			}else{` |
|      568 |  2975 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2976 | `			}` |
|      568 |  2977 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1302000 |  2978 | `			break;` |
|      ! 0 |  2979 | `		}else{` |
|        - |  2980 | `			/* Fatal error */` |
|      ! 0 |  2981 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  2982 | `			goto Abort;` |
|        - |  2983 | `		}` |
|        - |  2984 | `	}` |
|        - |  2985 | `	/* Load variable contents */` |
|  2603086 |  2986 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2603086 |  2987 | `	pTos->nIdx = pObj->nIdx;` |
|  2603086 |  2988 | `	break;` |
|        - |  2989 | `				   }` |
|        - |  2990 | `/*` |
|        - |  2991 | ` * LOAD_MAP P1 * *` |
|        - |  2992 | ` *` |
|        - |  2993 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  2994 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  2995 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  2996 | ` */` |
|    16446 |  2997 | `case PH7_OP_LOAD_MAP: {` |
|        - |  2998 | `	ph7_hashmap *pMap;` |
|        - |  2999 | `	/* Allocate a new hashmap instance */` |
|    32894 |  3000 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    32894 |  3001 | `	if( pMap == 0 ){` |
|      ! 0 |  3002 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3003 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3004 | `		goto Abort;` |
|        - |  3005 | `	}` |
|    32894 |  3006 | `	if( pInstr->iP1 > 0 ){` |
|     1934 |  3007 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3008 | `		/* Perform the insertion */` |
|     5898 |  3009 | `		while( pEntry < pTos ){` |
|     3966 |  3010 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3011 | `				/* Insertion by reference */` |
|      142 |  3012 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3013 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3014 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3015 | `					);` |
|       48 |  3016 | `			}else{` |
|        - |  3017 | `				/* Standard insertion */` |
|     5807 |  3018 | `				PH7_HashmapInsert(pMap,` |
|     3870 |  3019 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     1935 |  3020 | `					&pEntry[1]` |
|        - |  3021 | `				);` |
|        - |  3022 | `			}` |
|        - |  3023 | `			/* Next pair on the stack */` |
|     3966 |  3024 | `			pEntry += 2;` |
|        2 |  3025 | `		}` |
|        - |  3026 | `		/* Pop P1 elements */` |
|     1934 |  3027 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|      966 |  3028 | `	}` |
|        - |  3029 | `	/* Push the hashmap */` |
|    32894 |  3030 | `	pTos++;` |
|    32894 |  3031 | `	pTos->nIdx = SXU32_HIGH;` |
|    32894 |  3032 | `	pTos->x.pOther = pMap;` |
|    32894 |  3033 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    32894 |  3034 | `	break;` |
|        - |  3035 | `					  }` |
|        - |  3036 | `/*` |
|        - |  3037 | ` * LOAD_LIST: P1 * *` |
|        - |  3038 | ` *` |
|        - |  3039 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3040 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3041 | ` * Caveats:` |
|        - |  3042 | ` *  This implementation support only a single nesting level.` |
|        - |  3043 | ` */` |
|       17 |  3044 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3045 | `	ph7_value *pEntry;` |
|       35 |  3046 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3047 | `		/* Empty list,break immediately */` |
|      ! 0 |  3048 | `		break;` |
|        - |  3049 | `	}` |
|       35 |  3050 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3051 | `#ifdef UNTRUST` |
|        - |  3052 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3053 | `		goto Abort;` |
|        - |  3054 | `	}` |
|        - |  3055 | `#endif` |
|       35 |  3056 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       31 |  3057 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3058 | `		ph7_hashmap_node *pNode;` |
|        - |  3059 | `		ph7_value sKey,*pObj;` |
|        - |  3060 | `		/* Start Copying */` |
|       31 |  3061 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|       99 |  3062 | `		while( pEntry <= pTos ){` |
|       69 |  3063 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       65 |  3064 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       65 |  3065 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       65 |  3066 | `					if( rc == SXRET_OK ){` |
|        - |  3067 | `						/* Store node value */` |
|       65 |  3068 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       33 |  3069 | `					}else{` |
|        - |  3070 | `						/* Nullify the variable */` |
|      ! 0 |  3071 | `						PH7_MemObjRelease(pObj);` |
|        - |  3072 | `					}` |
|       32 |  3073 | `				}` |
|       32 |  3074 | `			}` |
|       69 |  3075 | `			sKey.x.iVal++; /* Next numeric index */` |
|       69 |  3076 | `			pEntry++;` |
|        1 |  3077 | `		}` |
|       15 |  3078 | `	}` |
|       35 |  3079 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       35 |  3080 | `	break;` |
|        - |  3081 | `					   }` |
|        - |  3082 | `/*` |
|        - |  3083 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3084 | ` *` |
|        - |  3085 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3086 | ` * from the stack.` |
|        - |  3087 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3088 | ` * instead.` |
|        - |  3089 | ` */` |
|   213799 |  3090 | `case PH7_OP_LOAD_IDX: {` |
|   427644 |  3091 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   427644 |  3092 | `	ph7_hashmap *pMap = 0;` |
|        - |  3093 | `	ph7_value *pIdx;` |
|   427644 |  3094 | `	pIdx = 0;` |
|   427644 |  3095 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3096 | `		if( !pInstr->iP2){` |
|        - |  3097 | `			/* No available index,load NULL */` |
|      ! 0 |  3098 | `			if( pTos >= pStack ){` |
|      ! 0 |  3099 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3100 | `			}else{` |
|        - |  3101 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3102 | `				pTos++;` |
|      ! 0 |  3103 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3104 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3105 | `			}` |
|        - |  3106 | `			/* Emit a notice */` |
|      ! 0 |  3107 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3108 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3109 | `			break;` |
|        - |  3110 | `		}` |
|      ! 0 |  3111 | `	}else{` |
|   427644 |  3112 | `		pIdx = pTos;` |
|   427644 |  3113 | `		pTos--;` |
|        - |  3114 | `	}` |
|   427644 |  3115 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3116 | `		/* String access */` |
|   346124 |  3117 | `		if( pIdx ){` |
|        - |  3118 | `			sxu32 nOfft;` |
|   346124 |  3119 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3120 | `				/* Force an int cast */` |
|      ! 0 |  3121 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3122 | `			}` |
|   346124 |  3123 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   346124 |  3124 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3125 | `				/* Invalid offset,load null */` |
|      ! 0 |  3126 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3127 | `			}else{` |
|   346124 |  3128 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   346124 |  3129 | `				int c = zData[nOfft];` |
|   346124 |  3130 | `				PH7_MemObjRelease(pTos);` |
|   346124 |  3131 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   346124 |  3132 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3133 | `			}` |
|   173085 |  3134 | `		}else{` |
|        - |  3135 | `			/* No available index,load NULL */` |
|      ! 0 |  3136 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3137 | `		}` |
|   346124 |  3138 | `		break;` |
|        - |  3139 | `	}` |
|    81522 |  3140 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3141 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3142 | `			ph7_value *pObj;` |
|      ! 0 |  3143 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3144 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3145 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3146 | `			}` |
|      ! 0 |  3147 | `		}` |
|      ! 0 |  3148 | `	}` |
|    81522 |  3149 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    81522 |  3150 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3151 | `		/* Point to the hashmap */` |
|    81522 |  3152 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    81522 |  3153 | `		if( pIdx ){` |
|        - |  3154 | `			/* Load the desired entry */` |
|    81522 |  3155 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    40760 |  3156 | `		}` |
|    81522 |  3157 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3158 | `			/* Create a new empty entry */` |
|      ! 0 |  3159 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3160 | `			if( rc == SXRET_OK ){` |
|        - |  3161 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3162 | `				pNode = pMap->pLast;` |
|      ! 0 |  3163 | `			}` |
|      ! 0 |  3164 | `		}` |
|    40760 |  3165 | `	}` |
|    81522 |  3166 | `	if( pIdx ){` |
|    81522 |  3167 | `		PH7_MemObjRelease(pIdx);` |
|    40760 |  3168 | `	}` |
|    81522 |  3169 | `	if( rc == SXRET_OK ){` |
|        - |  3170 | `		/* Load entry contents */` |
|    37674 |  3171 | `		if( pMap->iRef < 2 ){` |
|        - |  3172 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3173 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3174 | `			 */` |
|        7 |  3175 | `			pTos->nIdx = SXU32_HIGH;` |
|        7 |  3176 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|        4 |  3177 | `		}else{` |
|    37668 |  3178 | `			pTos->nIdx = pNode->nValIdx;` |
|    37668 |  3179 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    37668 |  3180 | `			PH7_HashmapUnref(pMap);` |
|        - |  3181 | `		}` |
|    18838 |  3182 | `	}else{` |
|        - |  3183 | `		/* No such entry,load NULL */` |
|    43850 |  3184 | `		PH7_MemObjRelease(pTos);` |
|    43850 |  3185 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3186 | `	}` |
|    81522 |  3187 | `	break;` |
|        - |  3188 | `					  }` |
|        - |  3189 | `/*` |
|        - |  3190 | ` * LOAD_CLOSURE * * P3` |
|        - |  3191 | ` *` |
|        - |  3192 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3193 | ` * name in the stack.` |
|        - |  3194 | ` */` |
|        2 |  3195 | `case PH7_OP_LOAD_CLOSURE:{` |
|        5 |  3196 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        5 |  3197 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3198 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3199 | `		ph7_vm_func *pClosure;` |
|        - |  3200 | `		char *zName;` |
|        - |  3201 | `		sxu32 mLen;` |
|        - |  3202 | `		sxu32 n;` |
|        - |  3203 | `		/* Create a new VM function */` |
|        5 |  3204 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3205 | `		/* Generate an unique closure name */` |
|        5 |  3206 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        5 |  3207 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3208 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3209 | `			goto Abort;` |
|        - |  3210 | `		}` |
|        5 |  3211 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        5 |  3212 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3213 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3214 | `		}` |
|        - |  3215 | `		/* Zero the stucture */` |
|        5 |  3216 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3217 | `		/* Perform a structure assignment on read-only items */` |
|        5 |  3218 | `		pClosure->aArgs = pFunc->aArgs;` |
|        5 |  3219 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        5 |  3220 | `		pClosure->aStatic = pFunc->aStatic;` |
|        5 |  3221 | `		pClosure->iFlags = pFunc->iFlags;` |
|        5 |  3222 | `		pClosure->pUserData = pFunc->pUserData;` |
|        5 |  3223 | `		pClosure->sSignature = pFunc->sSignature;` |
|        5 |  3224 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3225 | `		/* Register the closure */` |
|        5 |  3226 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3227 | `		/* Set up closure environment */` |
|        5 |  3228 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        5 |  3229 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       13 |  3230 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3231 | `			ph7_value *pValue;` |
|        9 |  3232 | `			pEnv = &aEnv[n];` |
|        9 |  3233 | `			sEnv.sName  = pEnv->sName;` |
|        9 |  3234 | `			sEnv.iFlags = pEnv->iFlags;` |
|        9 |  3235 | `			sEnv.nIdx = SXU32_HIGH;` |
|        9 |  3236 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|        9 |  3237 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3238 | `				/* Pass by reference */` |
|      ! 0 |  3239 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3240 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3241 | `					);` |
|      ! 0 |  3242 | `			}` |
|        - |  3243 | `			/* Standard pass by value */` |
|        9 |  3244 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|        9 |  3245 | `			if( pValue ){` |
|        - |  3246 | `				/* Copy imported value */` |
|        5 |  3247 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        2 |  3248 | `			}` |
|        - |  3249 | `			/* Insert the imported variable */` |
|        9 |  3250 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        5 |  3251 | `		}` |
|        - |  3252 | `		/* Finally,load the closure name on the stack */` |
|        5 |  3253 | `		pTos++;` |
|        5 |  3254 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        2 |  3255 | `	}` |
|        5 |  3256 | `	break;` |
|        - |  3257 | `						 }` |
|        - |  3258 | `/*` |
|        - |  3259 | ` * STORE * P2 P3` |
|        - |  3260 | ` *` |
|        - |  3261 | ` * Perform a store (Assignment) operation.` |
|        - |  3262 | ` */` |
|   100230 |  3263 | `case PH7_OP_STORE: {` |
|        - |  3264 | `	ph7_value *pObj;` |
|        - |  3265 | `	SyString sName;` |
|        - |  3266 | `#ifdef UNTRUST` |
|        - |  3267 | `	if( pTos < pStack ){` |
|        - |  3268 | `		goto Abort;` |
|        - |  3269 | `	}` |
|        - |  3270 | `#endif` |
|   200462 |  3271 | `	if( pInstr->iP2 ){` |
|        - |  3272 | `		sxu32 nIdx;` |
|        - |  3273 | `		/* Member store operation */` |
|     2258 |  3274 | `		nIdx = pTos->nIdx;` |
|     2258 |  3275 | `		VmPopOperand(&pTos,1);` |
|     2258 |  3276 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3277 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3278 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3279 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3280 | `		}else{` |
|        - |  3281 | `			/* Point to the desired memory object */` |
|     2254 |  3282 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2254 |  3283 | `			if( pObj ){` |
|        - |  3284 | `				/* Perform the store operation */` |
|     2254 |  3285 | `				PH7_MemObjStore(pTos,pObj);` |
|     1126 |  3286 | `			}` |
|        - |  3287 | `		}` |
|   101360 |  3288 | `		break;` |
|   198206 |  3289 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3290 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3291 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3292 | `			/* Force a string cast */` |
|      ! 0 |  3293 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3294 | `		}` |
|        7 |  3295 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3296 | `		pTos--;` |
|        - |  3297 | `#ifdef UNTRUST` |
|        - |  3298 | `		if( pTos < pStack  ){` |
|        - |  3299 | `			goto Abort;` |
|        - |  3300 | `		}` |
|        - |  3301 | `#endif` |
|        4 |  3302 | `	}else{` |
|   198200 |  3303 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3304 | `	}` |
|        - |  3305 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   198206 |  3306 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   198206 |  3307 | `	if( pObj == 0 ){` |
|      ! 0 |  3308 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3309 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3310 | `		goto Abort;` |
|        - |  3311 | `	}` |
|   198206 |  3312 | `	if( !pInstr->p3 ){` |
|        7 |  3313 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3314 | `	}` |
|        - |  3315 | `	/* Perform the store operation */` |
|   198206 |  3316 | `	PH7_MemObjStore(pTos,pObj);` |
|   198206 |  3317 | `	break;` |
|        - |  3318 | `				   }` |
|        - |  3319 | `/*` |
|        - |  3320 | ` * STORE_IDX:   P1 * P3` |
|        - |  3321 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3322 | ` *` |
|        - |  3323 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3324 | ` */` |
|    75936 |  3325 | `case PH7_OP_STORE_IDX:` |
|        - |  3326 | `case PH7_OP_STORE_IDX_REF: {` |
|   151874 |  3327 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3328 | `	ph7_value *pKey;` |
|        - |  3329 | `	sxu32 nIdx;` |
|   151874 |  3330 | `	if( pInstr->iP1 ){` |
|        - |  3331 | `		/* Key is next on stack */` |
|    54948 |  3332 | `		pKey = pTos;` |
|    54948 |  3333 | `		pTos--;` |
|    27475 |  3334 | `	}else{` |
|    96928 |  3335 | `		pKey = 0;` |
|        - |  3336 | `	}` |
|   151874 |  3337 | `	nIdx = pTos->nIdx;` |
|   151874 |  3338 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3339 | `		/* Hashmap already loaded */` |
|   151822 |  3340 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   151822 |  3341 | `		if( pMap->iRef < 2 ){` |
|        - |  3342 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3343 | `			pMap->iRef = 2;` |
|      ! 0 |  3344 | `		}` |
|    75912 |  3345 | `	}else{` |
|        - |  3346 | `		ph7_value *pObj;` |
|       53 |  3347 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3348 | `		if( pObj == 0 ){` |
|      ! 0 |  3349 | `			if( pKey ){` |
|      ! 0 |  3350 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3351 | `			}` |
|      ! 0 |  3352 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3353 | `			break;` |
|        - |  3354 | `		}` |
|        - |  3355 | `		/* Phase#1: Load the array */` |
|       53 |  3356 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3357 | `			VmPopOperand(&pTos,1);` |
|       53 |  3358 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3359 | `				/* Force a string cast */` |
|      ! 0 |  3360 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3361 | `			}` |
|       53 |  3362 | `			if( pKey == 0 ){` |
|        - |  3363 | `				/* Append string */` |
|        3 |  3364 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3365 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3366 | `				}` |
|        2 |  3367 | `			}else{` |
|        - |  3368 | `				sxu32 nOfft;` |
|       51 |  3369 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3370 | `					/* Force an int cast */` |
|       51 |  3371 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3372 | `				}` |
|       51 |  3373 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3374 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3375 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3376 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3377 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3378 | `				}else{` |
|      ! 0 |  3379 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3380 | `						/* Perform an append operation */` |
|      ! 0 |  3381 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3382 | `					}` |
|        - |  3383 | `				}` |
|        - |  3384 | `			}` |
|       53 |  3385 | `			if( pKey ){` |
|       51 |  3386 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3387 | `			}` |
|       53 |  3388 | `			break;` |
|      ! 0 |  3389 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3390 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3391 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3392 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3393 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3394 | `				goto Abort;` |
|        - |  3395 | `			}` |
|      ! 0 |  3396 | `		}` |
|      ! 0 |  3397 | `		pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - |  3398 | `	}` |
|   151822 |  3399 | `	VmPopOperand(&pTos,1);` |
|        - |  3400 | `	/* Phase#2: Perform the insertion */` |
|   151822 |  3401 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3402 | `		/* Insertion by reference */` |
|       15 |  3403 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3404 | `	}else{` |
|   151808 |  3405 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3406 | `	}` |
|   151822 |  3407 | `	if( pKey ){` |
|    54898 |  3408 | `		PH7_MemObjRelease(pKey);` |
|    27448 |  3409 | `	}` |
|   151822 |  3410 | `	break;` |
|        - |  3411 | `					   }` |
|        - |  3412 | `/*` |
|        - |  3413 | ` * INCR: P1 * *` |
|        - |  3414 | ` *` |
|        - |  3415 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3416 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3417 | ` * the stack and increment after that.` |
|        - |  3418 | ` */` |
|   155751 |  3419 | `case PH7_OP_INCR:` |
|        - |  3420 | `#ifdef UNTRUST` |
|        - |  3421 | `	if( pTos < pStack ){` |
|        - |  3422 | `		goto Abort;` |
|        - |  3423 | `	}` |
|        - |  3424 | `#endif` |
|   311548 |  3425 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   311548 |  3426 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3427 | `			ph7_value *pObj;` |
|   311548 |  3428 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3429 | `				/* Force a numeric cast */` |
|   311548 |  3430 | `				PH7_MemObjToNumeric(pObj);` |
|   311548 |  3431 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3432 | `					pObj->rVal++;` |
|        - |  3433 | `					/* Try to get an integer representation */` |
|      ! 0 |  3434 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3435 | `				}else{` |
|   311548 |  3436 | `					pObj->x.iVal++;` |
|   311548 |  3437 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3438 | `				}` |
|   311548 |  3439 | `				if( pInstr->iP1 ){` |
|        - |  3440 | `					/* Pre-icrement */` |
|       71 |  3441 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3442 | `				}` |
|   155795 |  3443 | `			}` |
|   155797 |  3444 | `		}else{` |
|      ! 0 |  3445 | `			if( pInstr->iP1 ){` |
|        - |  3446 | `				/* Force a numeric cast */` |
|      ! 0 |  3447 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3448 | `				/* Pre-increment */` |
|      ! 0 |  3449 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3450 | `					pTos->rVal++;` |
|        - |  3451 | `					/* Try to get an integer representation */` |
|      ! 0 |  3452 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3453 | `				}else{` |
|      ! 0 |  3454 | `					pTos->x.iVal++;` |
|      ! 0 |  3455 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3456 | `				}` |
|      ! 0 |  3457 | `			}` |
|        - |  3458 | `		}` |
|   155795 |  3459 | `	}` |
|   311548 |  3460 | `	break;` |
|        - |  3461 | `/*` |
|        - |  3462 | ` * DECR: P1 * *` |
|        - |  3463 | ` *` |
|        - |  3464 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3465 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3466 | ` * and decrement after that.` |
|        - |  3467 | ` */` |
|        2 |  3468 | `case PH7_OP_DECR:` |
|        - |  3469 | `#ifdef UNTRUST` |
|        - |  3470 | `	if( pTos < pStack ){` |
|        - |  3471 | `		goto Abort;` |
|        - |  3472 | `	}` |
|        - |  3473 | `#endif` |
|        5 |  3474 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3475 | `		/* Force a numeric cast */` |
|        5 |  3476 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3477 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3478 | `			ph7_value *pObj;` |
|        5 |  3479 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3480 | `				/* Force a numeric cast */` |
|        5 |  3481 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3482 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3483 | `					pObj->rVal--;` |
|        - |  3484 | `					/* Try to get an integer representation */` |
|      ! 0 |  3485 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3486 | `				}else{` |
|        5 |  3487 | `					pObj->x.iVal--;` |
|        5 |  3488 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3489 | `				}` |
|        5 |  3490 | `				if( pInstr->iP1 ){` |
|        - |  3491 | `					/* Pre-icrement */` |
|      ! 0 |  3492 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3493 | `				}` |
|        2 |  3494 | `			}` |
|        3 |  3495 | `		}else{` |
|      ! 0 |  3496 | `			if( pInstr->iP1 ){` |
|        - |  3497 | `				/* Pre-increment */` |
|      ! 0 |  3498 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3499 | `					pTos->rVal--;` |
|        - |  3500 | `					/* Try to get an integer representation */` |
|      ! 0 |  3501 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3502 | `				}else{` |
|      ! 0 |  3503 | `					pTos->x.iVal--;` |
|      ! 0 |  3504 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3505 | `				}` |
|      ! 0 |  3506 | `			}` |
|        - |  3507 | `		}` |
|        2 |  3508 | `	}` |
|        5 |  3509 | `	break;` |
|        - |  3510 | `/*` |
|        - |  3511 | ` * UMINUS: * * *` |
|        - |  3512 | ` *` |
|        - |  3513 | ` * Perform a unary minus operation.` |
|        - |  3514 | ` */` |
|    21293 |  3515 | `case PH7_OP_UMINUS:` |
|        - |  3516 | `#ifdef UNTRUST` |
|        - |  3517 | `	if( pTos < pStack ){` |
|        - |  3518 | `		goto Abort;` |
|        - |  3519 | `	}` |
|        - |  3520 | `#endif` |
|        - |  3521 | `	/* Force a numeric (integer,real or both) cast */` |
|    42588 |  3522 | `	PH7_MemObjToNumeric(pTos);` |
|    42588 |  3523 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       25 |  3524 | `		pTos->rVal = -pTos->rVal;` |
|       12 |  3525 | `	}` |
|    42588 |  3526 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    42564 |  3527 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    21281 |  3528 | `	}` |
|    42588 |  3529 | `	break;` |
|        - |  3530 | `/*` |
|        - |  3531 | ` * UPLUS: * * *` |
|        - |  3532 | ` *` |
|        - |  3533 | ` * Perform a unary plus operation.` |
|        - |  3534 | ` */` |
|       16 |  3535 | `case PH7_OP_UPLUS:` |
|        - |  3536 | `#ifdef UNTRUST` |
|        - |  3537 | `	if( pTos < pStack ){` |
|        - |  3538 | `		goto Abort;` |
|        - |  3539 | `	}` |
|        - |  3540 | `#endif` |
|        - |  3541 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3542 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3543 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3544 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3545 | `	}` |
|       33 |  3546 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3547 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3548 | `	}` |
|       33 |  3549 | `	break;` |
|        - |  3550 | `/*` |
|        - |  3551 | ` * OP_LNOT: * * *` |
|        - |  3552 | ` *` |
|        - |  3553 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3554 | ` * with its complement.` |
|        - |  3555 | ` */` |
|    46513 |  3556 | `case PH7_OP_LNOT:` |
|        - |  3557 | `#ifdef UNTRUST` |
|        - |  3558 | `	if( pTos < pStack ){` |
|        - |  3559 | `		goto Abort;` |
|        - |  3560 | `	}` |
|        - |  3561 | `#endif` |
|        - |  3562 | `	/* Force a boolean cast */` |
|    93072 |  3563 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3564 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3565 | `	}` |
|    93072 |  3566 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    93072 |  3567 | `	break;` |
|        - |  3568 | `/*` |
|        - |  3569 | ` * OP_BITNOT: * * *` |
|        - |  3570 | ` *` |
|        - |  3571 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3572 | ` * with its ones-complement.` |
|        - |  3573 | ` */` |
|        3 |  3574 | `case PH7_OP_BITNOT:` |
|        - |  3575 | `#ifdef UNTRUST` |
|        - |  3576 | `	if( pTos < pStack ){` |
|        - |  3577 | `		goto Abort;` |
|        - |  3578 | `	}` |
|        - |  3579 | `#endif` |
|        - |  3580 | `	/* Force an integer cast */` |
|        7 |  3581 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3582 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3583 | `	}` |
|        7 |  3584 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|        7 |  3585 | `	break;` |
|        - |  3586 | `/* OP_MUL * * *` |
|        - |  3587 | ` * OP_MUL_STORE * * *` |
|        - |  3588 | ` *` |
|        - |  3589 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3590 | ` * and push the result back onto the stack.` |
|        - |  3591 | ` */` |
|     1234 |  3592 | `case PH7_OP_MUL:` |
|        - |  3593 | `case PH7_OP_MUL_STORE: {` |
|     2470 |  3594 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3595 | `	/* Force the operand to be numeric */` |
|        - |  3596 | `#ifdef UNTRUST` |
|        - |  3597 | `	if( pNos < pStack ){` |
|        - |  3598 | `		goto Abort;` |
|        - |  3599 | `	}` |
|        - |  3600 | `#endif` |
|     2470 |  3601 | `	PH7_MemObjToNumeric(pTos);` |
|     2470 |  3602 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3603 | `	/* Perform the requested operation */` |
|     2470 |  3604 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3605 | `		/* Floating point arithemic */` |
|        - |  3606 | `		ph7_real a,b,r;` |
|       17 |  3607 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3608 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3609 | `		}` |
|       17 |  3610 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3611 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3612 | `		}` |
|       17 |  3613 | `		a = pNos->rVal;` |
|       17 |  3614 | `		b = pTos->rVal;` |
|       17 |  3615 | `		r = a * b;` |
|        - |  3616 | `		/* Push the result */` |
|       17 |  3617 | `		pNos->rVal = r;` |
|       17 |  3618 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3619 | `		/* Try to get an integer representation */` |
|       17 |  3620 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3621 | `	}else{` |
|        - |  3622 | `		/* Integer arithmetic */` |
|        - |  3623 | `		sxi64 a,b,r;` |
|     2454 |  3624 | `		a = pNos->x.iVal;` |
|     2454 |  3625 | `		b = pTos->x.iVal;` |
|     2454 |  3626 | `		r = a * b;` |
|        - |  3627 | `		/* Push the result */` |
|     2454 |  3628 | `		pNos->x.iVal = r;` |
|     2454 |  3629 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3630 | `	}` |
|     2470 |  3631 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3632 | `		ph7_value *pObj;` |
|       19 |  3633 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3634 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  3635 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  3636 | `			PH7_MemObjStore(pNos,pObj);` |
|        9 |  3637 | `		}` |
|        9 |  3638 | `	}` |
|     2470 |  3639 | `	VmPopOperand(&pTos,1);` |
|     2470 |  3640 | `	break;` |
|        - |  3641 | `				 }` |
|        - |  3642 | `/* OP_ADD * * *` |
|        - |  3643 | ` *` |
|        - |  3644 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3645 | ` * and push the result back onto the stack.` |
|        - |  3646 | ` */` |
|      425 |  3647 | `case PH7_OP_ADD:{` |
|      852 |  3648 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3649 | `#ifdef UNTRUST` |
|        - |  3650 | `	if( pNos < pStack ){` |
|        - |  3651 | `		goto Abort;` |
|        - |  3652 | `	}` |
|        - |  3653 | `#endif` |
|        - |  3654 | `	/* Perform the addition */` |
|      852 |  3655 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      852 |  3656 | `	VmPopOperand(&pTos,1);` |
|      852 |  3657 | `	break;` |
|        - |  3658 | `				}` |
|        - |  3659 | `/*` |
|        - |  3660 | ` * OP_ADD_STORE * * *` |
|        - |  3661 | ` *` |
|        - |  3662 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3663 | ` * and push the result back onto the stack.` |
|        - |  3664 | ` */` |
|      481 |  3665 | `case PH7_OP_ADD_STORE:{` |
|      963 |  3666 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3667 | `	ph7_value *pObj;` |
|        - |  3668 | `	sxu32 nIdx;` |
|        - |  3669 | `#ifdef UNTRUST` |
|        - |  3670 | `	if( pNos < pStack ){` |
|        - |  3671 | `		goto Abort;` |
|        - |  3672 | `	}` |
|        - |  3673 | `#endif` |
|        - |  3674 | `	/* Perform the addition */` |
|      963 |  3675 | `	nIdx = pTos->nIdx;` |
|      963 |  3676 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3677 | `	/* Peform the store operation */` |
|      963 |  3678 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3679 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      963 |  3680 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      963 |  3681 | `		PH7_MemObjStore(pTos,pObj);` |
|      481 |  3682 | `	}` |
|        - |  3683 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      963 |  3684 | `	PH7_MemObjStore(pTos,pNos);` |
|      963 |  3685 | `	VmPopOperand(&pTos,1);` |
|      963 |  3686 | `	break;` |
|        - |  3687 | `				}` |
|        - |  3688 | `/* OP_SUB * * *` |
|        - |  3689 | ` *` |
|        - |  3690 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3691 | ` * first (what was next on the stack) from the second (the` |
|        - |  3692 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3693 | ` */` |
|      294 |  3694 | `case PH7_OP_SUB: {` |
|      589 |  3695 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3696 | `#ifdef UNTRUST` |
|        - |  3697 | `	if( pNos < pStack ){` |
|        - |  3698 | `		goto Abort;` |
|        - |  3699 | `	}` |
|        - |  3700 | `#endif` |
|      589 |  3701 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3702 | `		/* Floating point arithemic */` |
|        - |  3703 | `		ph7_real a,b,r;` |
|       95 |  3704 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3705 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3706 | `		}` |
|       95 |  3707 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3708 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3709 | `		}` |
|       95 |  3710 | `		a = pNos->rVal;` |
|       95 |  3711 | `		b = pTos->rVal;` |
|       95 |  3712 | `		r = a - b;` |
|        - |  3713 | `		/* Push the result */` |
|       95 |  3714 | `		pNos->rVal = r;` |
|       95 |  3715 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3716 | `		/* Try to get an integer representation */` |
|       95 |  3717 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  3718 | `	}else{` |
|        - |  3719 | `		/* Integer arithmetic */` |
|        - |  3720 | `		sxi64 a,b,r;` |
|      495 |  3721 | `		a = pNos->x.iVal;` |
|      495 |  3722 | `		b = pTos->x.iVal;` |
|      495 |  3723 | `		r = a - b;` |
|        - |  3724 | `		/* Push the result */` |
|      495 |  3725 | `		pNos->x.iVal = r;` |
|      495 |  3726 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3727 | `	}` |
|      589 |  3728 | `	VmPopOperand(&pTos,1);` |
|      589 |  3729 | `	break;` |
|        - |  3730 | `				 }` |
|        - |  3731 | `/* OP_SUB_STORE * * *` |
|        - |  3732 | ` *` |
|        - |  3733 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3734 | ` * first (what was next on the stack) from the second (the` |
|        - |  3735 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3736 | ` */` |
|        1 |  3737 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3738 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3739 | `	ph7_value *pObj;` |
|        - |  3740 | `#ifdef UNTRUST` |
|        - |  3741 | `	if( pNos < pStack ){` |
|        - |  3742 | `		goto Abort;` |
|        - |  3743 | `	}` |
|        - |  3744 | `#endif` |
|        3 |  3745 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3746 | `		/* Floating point arithemic */` |
|        - |  3747 | `		ph7_real a,b,r;` |
|      ! 0 |  3748 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3749 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3750 | `		}` |
|      ! 0 |  3751 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3752 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3753 | `		}` |
|      ! 0 |  3754 | `		a = pTos->rVal;` |
|      ! 0 |  3755 | `		b = pNos->rVal;` |
|      ! 0 |  3756 | `		r = a - b;` |
|        - |  3757 | `		/* Push the result */` |
|      ! 0 |  3758 | `		pNos->rVal = r;` |
|      ! 0 |  3759 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3760 | `		/* Try to get an integer representation */` |
|      ! 0 |  3761 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3762 | `	}else{` |
|        - |  3763 | `		/* Integer arithmetic */` |
|        - |  3764 | `		sxi64 a,b,r;` |
|        3 |  3765 | `		a = pTos->x.iVal;` |
|        3 |  3766 | `		b = pNos->x.iVal;` |
|        3 |  3767 | `		r = a - b;` |
|        - |  3768 | `		/* Push the result */` |
|        3 |  3769 | `		pNos->x.iVal = r;` |
|        3 |  3770 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3771 | `	}` |
|        3 |  3772 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3773 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3774 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3775 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3776 | `	}` |
|        3 |  3777 | `	VmPopOperand(&pTos,1);` |
|        3 |  3778 | `	break;` |
|        - |  3779 | `				 }` |
|        - |  3780 |  |
|        - |  3781 | `/*` |
|        - |  3782 | ` * OP_MOD * * *` |
|        - |  3783 | ` *` |
|        - |  3784 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3785 | ` * first (what was next on the stack) from the second (the` |
|        - |  3786 | ` * top of the stack) and push the remainder after division` |
|        - |  3787 | ` * onto the stack.` |
|        - |  3788 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3789 | ` */` |
|      296 |  3790 | `case PH7_OP_MOD:{` |
|      594 |  3791 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3792 | `	sxi64 a,b,r;` |
|        - |  3793 | `#ifdef UNTRUST` |
|        - |  3794 | `	if( pNos < pStack ){` |
|        - |  3795 | `		goto Abort;` |
|        - |  3796 | `	}` |
|        - |  3797 | `#endif` |
|        - |  3798 | `	/* Force the operands to be integer */` |
|      594 |  3799 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3800 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3801 | `	}` |
|      594 |  3802 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3803 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3804 | `	}` |
|        - |  3805 | `	/* Perform the requested operation */` |
|      594 |  3806 | `	a = pNos->x.iVal;` |
|      594 |  3807 | `	b = pTos->x.iVal;` |
|      594 |  3808 | `	if( b == 0 ){` |
|        3 |  3809 | `		r = 0;` |
|        3 |  3810 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3811 | `		/* goto Abort; */` |
|        2 |  3812 | `	}else{` |
|      591 |  3813 | `		r = a%b;` |
|        - |  3814 | `	}` |
|        - |  3815 | `	/* Push the result */` |
|      594 |  3816 | `	pNos->x.iVal = r;` |
|      594 |  3817 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      594 |  3818 | `	VmPopOperand(&pTos,1);` |
|      594 |  3819 | `	break;` |
|        - |  3820 | `				}` |
|        - |  3821 | `/*` |
|        - |  3822 | ` * OP_MOD_STORE * * *` |
|        - |  3823 | ` *` |
|        - |  3824 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3825 | ` * first (what was next on the stack) from the second (the` |
|        - |  3826 | ` * top of the stack) and push the remainder after division` |
|        - |  3827 | ` * onto the stack.` |
|        - |  3828 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3829 | ` */` |
|        1 |  3830 | `case PH7_OP_MOD_STORE: {` |
|        3 |  3831 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3832 | `	ph7_value *pObj;` |
|        - |  3833 | `	sxi64 a,b,r;` |
|        - |  3834 | `#ifdef UNTRUST` |
|        - |  3835 | `	if( pNos < pStack ){` |
|        - |  3836 | `		goto Abort;` |
|        - |  3837 | `	}` |
|        - |  3838 | `#endif` |
|        - |  3839 | `	/* Force the operands to be integer */` |
|        3 |  3840 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3841 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3842 | `	}` |
|        3 |  3843 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3844 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3845 | `	}` |
|        - |  3846 | `	/* Perform the requested operation */` |
|        3 |  3847 | `	a = pTos->x.iVal;` |
|        3 |  3848 | `	b = pNos->x.iVal;` |
|        3 |  3849 | `	if( b == 0 ){` |
|      ! 0 |  3850 | `		r = 0;` |
|      ! 0 |  3851 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3852 | `		/* goto Abort; */` |
|      ! 0 |  3853 | `	}else{` |
|        3 |  3854 | `		r = a%b;` |
|        - |  3855 | `	}` |
|        - |  3856 | `	/* Push the result */` |
|        3 |  3857 | `	pNos->x.iVal = r;` |
|        3 |  3858 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  3859 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3860 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3861 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3862 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3863 | `	}` |
|        3 |  3864 | `	VmPopOperand(&pTos,1);` |
|        3 |  3865 | `	break;` |
|        - |  3866 | `				}` |
|        - |  3867 | `/*` |
|        - |  3868 | ` * OP_DIV * * *` |
|        - |  3869 | ` *` |
|        - |  3870 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3871 | ` * first (what was next on the stack) from the second (the` |
|        - |  3872 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3873 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3874 | ` */` |
|       28 |  3875 | `case PH7_OP_DIV:{` |
|       58 |  3876 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3877 | `	ph7_real a,b,r;` |
|        - |  3878 | `#ifdef UNTRUST` |
|        - |  3879 | `	if( pNos < pStack ){` |
|        - |  3880 | `		goto Abort;` |
|        - |  3881 | `	}` |
|        - |  3882 | `#endif` |
|        - |  3883 | `	/* Force the operands to be real */` |
|       58 |  3884 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  3885 | `		PH7_MemObjToReal(pTos);` |
|       26 |  3886 | `	}` |
|       58 |  3887 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  3888 | `		PH7_MemObjToReal(pNos);` |
|        9 |  3889 | `	}` |
|        - |  3890 | `	/* Perform the requested operation */` |
|       58 |  3891 | `	a = pNos->rVal;` |
|       58 |  3892 | `	b = pTos->rVal;` |
|       58 |  3893 | `	if( b == 0 ){` |
|        - |  3894 | `		/* Division by zero */` |
|        3 |  3895 | `		pNos->rVal = 0;` |
|        3 |  3896 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  3897 | `		/* goto Abort; */` |
|        2 |  3898 | `	}else{` |
|       55 |  3899 | `		r = a/b;` |
|        - |  3900 | `		/* Push the result */` |
|       55 |  3901 | `		pNos->rVal = r;` |
|       55 |  3902 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3903 | `		/* Try to get an integer representation */` |
|       55 |  3904 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3905 | `	}` |
|       58 |  3906 | `	VmPopOperand(&pTos,1);` |
|       58 |  3907 | `	break;` |
|        - |  3908 | `				}` |
|        - |  3909 | `/*` |
|        - |  3910 | ` * OP_DIV_STORE * * *` |
|        - |  3911 | ` *` |
|        - |  3912 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3913 | ` * first (what was next on the stack) from the second (the` |
|        - |  3914 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3915 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3916 | ` */` |
|        1 |  3917 | `case PH7_OP_DIV_STORE:{` |
|        3 |  3918 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3919 | `	ph7_value *pObj;` |
|        - |  3920 | `	ph7_real a,b,r;` |
|        - |  3921 | `#ifdef UNTRUST` |
|        - |  3922 | `	if( pNos < pStack ){` |
|        - |  3923 | `		goto Abort;` |
|        - |  3924 | `	}` |
|        - |  3925 | `#endif` |
|        - |  3926 | `	/* Force the operands to be real */` |
|        3 |  3927 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3928 | `		PH7_MemObjToReal(pTos);` |
|        1 |  3929 | `	}` |
|        3 |  3930 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3931 | `		PH7_MemObjToReal(pNos);` |
|        1 |  3932 | `	}` |
|        - |  3933 | `	/* Perform the requested operation */` |
|        3 |  3934 | `	a = pTos->rVal;` |
|        3 |  3935 | `	b = pNos->rVal;` |
|        3 |  3936 | `	if( b == 0 ){` |
|        - |  3937 | `		/* Division by zero */` |
|      ! 0 |  3938 | `		r = 0;` |
|      ! 0 |  3939 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  3940 | `		/* goto Abort; */` |
|      ! 0 |  3941 | `	}else{` |
|        3 |  3942 | `		r = a/b;` |
|        - |  3943 | `		/* Push the result */` |
|        3 |  3944 | `		pNos->rVal = r;` |
|        3 |  3945 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3946 | `		/* Try to get an integer representation */` |
|        3 |  3947 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3948 | `	}` |
|        3 |  3949 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3950 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3951 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3952 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3953 | `	}` |
|        3 |  3954 | `	VmPopOperand(&pTos,1);` |
|        3 |  3955 | `	break;` |
|        - |  3956 | `				}` |
|        - |  3957 | `/* OP_BAND * * *` |
|        - |  3958 | ` *` |
|        - |  3959 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3960 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  3961 | ` * two elements.` |
|        - |  3962 | `*/` |
|        - |  3963 | `/* OP_BOR * * *` |
|        - |  3964 | ` *` |
|        - |  3965 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3966 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  3967 | ` * two elements.` |
|        - |  3968 | ` */` |
|        - |  3969 | `/* OP_BXOR * * *` |
|        - |  3970 | ` *` |
|        - |  3971 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3972 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  3973 | ` * two elements.` |
|        - |  3974 | ` */` |
|       19 |  3975 | `case PH7_OP_BAND:` |
|        - |  3976 | `case PH7_OP_BOR:` |
|        - |  3977 | `case PH7_OP_BXOR:{` |
|       39 |  3978 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3979 | `	sxi64 a,b,r;` |
|        - |  3980 | `#ifdef UNTRUST` |
|        - |  3981 | `	if( pNos < pStack ){` |
|        - |  3982 | `		goto Abort;` |
|        - |  3983 | `	}` |
|        - |  3984 | `#endif` |
|        - |  3985 | `	/* Force the operands to be integer */` |
|       39 |  3986 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3987 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3988 | `	}` |
|       39 |  3989 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3990 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3991 | `	}` |
|        - |  3992 | `	/* Perform the requested operation */` |
|       39 |  3993 | `	a = pNos->x.iVal;` |
|       39 |  3994 | `	b = pTos->x.iVal;` |
|       39 |  3995 | `	switch(pInstr->iOp){` |
|        6 |  3996 | `	case PH7_OP_BOR_STORE:` |
|       13 |  3997 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  3998 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  3999 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        7 |  4000 | `	case PH7_OP_BAND_STORE:` |
|        7 |  4001 | `	case PH7_OP_BAND:` |
|       15 |  4002 | `	default:          r = a&b; break;` |
|        - |  4003 | `	}` |
|        - |  4004 | `	/* Push the result */` |
|       39 |  4005 | `	pNos->x.iVal = r;` |
|       39 |  4006 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       39 |  4007 | `	VmPopOperand(&pTos,1);` |
|       39 |  4008 | `	break;` |
|        - |  4009 | `				 }` |
|        - |  4010 | `/* OP_BAND_STORE * * *` |
|        - |  4011 | ` *` |
|        - |  4012 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4013 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4014 | ` * two elements.` |
|        - |  4015 | `*/` |
|        - |  4016 | `/* OP_BOR_STORE * * *` |
|        - |  4017 | ` *` |
|        - |  4018 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4019 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4020 | ` * two elements.` |
|        - |  4021 | ` */` |
|        - |  4022 | `/* OP_BXOR_STORE * * *` |
|        - |  4023 | ` *` |
|        - |  4024 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4025 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4026 | ` * two elements.` |
|        - |  4027 | ` */` |
|        7 |  4028 | `case PH7_OP_BAND_STORE:` |
|        - |  4029 | `case PH7_OP_BOR_STORE:` |
|        - |  4030 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4031 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4032 | `	ph7_value *pObj;` |
|        - |  4033 | `	sxi64 a,b,r;` |
|        - |  4034 | `#ifdef UNTRUST` |
|        - |  4035 | `	if( pNos < pStack ){` |
|        - |  4036 | `		goto Abort;` |
|        - |  4037 | `	}` |
|        - |  4038 | `#endif` |
|        - |  4039 | `	/* Force the operands to be integer */` |
|       15 |  4040 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4041 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4042 | `	}` |
|       15 |  4043 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4044 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4045 | `	}` |
|        - |  4046 | `	/* Perform the requested operation */` |
|       15 |  4047 | `	a = pTos->x.iVal;` |
|       15 |  4048 | `	b = pNos->x.iVal;` |
|       15 |  4049 | `	switch(pInstr->iOp){` |
|        2 |  4050 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4051 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4052 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4053 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4054 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4055 | `	case PH7_OP_BAND:` |
|        5 |  4056 | `	default:          r = a&b; break;` |
|        - |  4057 | `	}` |
|        - |  4058 | `	/* Push the result */` |
|       15 |  4059 | `	pNos->x.iVal = r;` |
|       15 |  4060 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4061 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4062 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4063 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4064 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4065 | `	}` |
|       15 |  4066 | `	VmPopOperand(&pTos,1);` |
|       15 |  4067 | `	break;` |
|        - |  4068 | `				 }` |
|        - |  4069 | `/* OP_SHL * * *` |
|        - |  4070 | ` *` |
|        - |  4071 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4072 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4073 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4074 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4075 | ` */` |
|        - |  4076 | `/* OP_SHR * * *` |
|        - |  4077 | ` *` |
|        - |  4078 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4079 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4080 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4081 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4082 | ` */` |
|        9 |  4083 | `case PH7_OP_SHL:` |
|        - |  4084 | `case PH7_OP_SHR: {` |
|       19 |  4085 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4086 | `	sxi64 a,r;` |
|        - |  4087 | `	sxi32 b;` |
|        - |  4088 | `#ifdef UNTRUST` |
|        - |  4089 | `	if( pNos < pStack ){` |
|        - |  4090 | `		goto Abort;` |
|        - |  4091 | `	}` |
|        - |  4092 | `#endif` |
|        - |  4093 | `	/* Force the operands to be integer */` |
|       19 |  4094 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4095 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4096 | `	}` |
|       19 |  4097 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4098 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4099 | `	}` |
|        - |  4100 | `	/* Perform the requested operation */` |
|       19 |  4101 | `	a = pNos->x.iVal;` |
|       19 |  4102 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4103 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4104 | `		r = a << b;` |
|        6 |  4105 | `	}else{` |
|        9 |  4106 | `		r = a >> b;` |
|        - |  4107 | `	}` |
|        - |  4108 | `	/* Push the result */` |
|       19 |  4109 | `	pNos->x.iVal = r;` |
|       19 |  4110 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4111 | `	VmPopOperand(&pTos,1);` |
|       19 |  4112 | `	break;` |
|        - |  4113 | `				 }` |
|        - |  4114 | `/*  OP_SHL_STORE * * *` |
|        - |  4115 | ` *` |
|        - |  4116 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4117 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4118 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4119 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4120 | ` */` |
|        - |  4121 | `/* OP_SHR_STORE * * *` |
|        - |  4122 | ` *` |
|        - |  4123 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4124 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4125 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4126 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4127 | ` */` |
|        7 |  4128 | `case PH7_OP_SHL_STORE:` |
|        - |  4129 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4130 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4131 | `	ph7_value *pObj;` |
|        - |  4132 | `	sxi64 a,r;` |
|        - |  4133 | `	sxi32 b;` |
|        - |  4134 | `#ifdef UNTRUST` |
|        - |  4135 | `	if( pNos < pStack ){` |
|        - |  4136 | `		goto Abort;` |
|        - |  4137 | `	}` |
|        - |  4138 | `#endif` |
|        - |  4139 | `	/* Force the operands to be integer */` |
|       15 |  4140 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4141 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4142 | `	}` |
|       15 |  4143 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4144 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4145 | `	}` |
|        - |  4146 | `	/* Perform the requested operation */` |
|       15 |  4147 | `	a = pTos->x.iVal;` |
|       15 |  4148 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4149 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4150 | `		r = a << b;` |
|        4 |  4151 | `	}else{` |
|        9 |  4152 | `		r = a >> b;` |
|        - |  4153 | `	}` |
|        - |  4154 | `	/* Push the result */` |
|       15 |  4155 | `	pNos->x.iVal = r;` |
|       15 |  4156 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4157 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4158 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4159 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4160 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4161 | `	}` |
|       15 |  4162 | `	VmPopOperand(&pTos,1);` |
|       15 |  4163 | `	break;` |
|        - |  4164 | `				 }` |
|        - |  4165 | `/* CAT:  P1 * *` |
|        - |  4166 | ` *` |
|        - |  4167 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4168 | ` * back.` |
|        - |  4169 | ` */` |
|    57423 |  4170 | `case PH7_OP_CAT:{` |
|        - |  4171 | `	ph7_value *pNos,*pCur;` |
|   114848 |  4172 | `	if( pInstr->iP1 < 1 ){` |
|    88076 |  4173 | `		pNos = &pTos[-1];` |
|    44039 |  4174 | `	}else{` |
|    26774 |  4175 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4176 | `	}` |
|        - |  4177 | `#ifdef UNTRUST` |
|        - |  4178 | `	if( pNos < pStack ){` |
|        - |  4179 | `		goto Abort;` |
|        - |  4180 | `	}` |
|        - |  4181 | `#endif` |
|        - |  4182 | `	/* Force a string cast */` |
|   114848 |  4183 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      884 |  4184 | `		PH7_MemObjToString(pNos);` |
|      441 |  4185 | `	}` |
|   114848 |  4186 | `	pCur = &pNos[1];` |
|   231314 |  4187 | `	while( pCur <= pTos ){` |
|   116468 |  4188 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50364 |  4189 | `			PH7_MemObjToString(pCur);` |
|    25181 |  4190 | `		}` |
|        - |  4191 | `		/* Perform the concatenation */` |
|   116468 |  4192 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   116430 |  4193 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    58214 |  4194 | `		}` |
|   116468 |  4195 | `		SyBlobRelease(&pCur->sBlob);` |
|   116468 |  4196 | `		pCur++;` |
|        2 |  4197 | `	}` |
|   114848 |  4198 | `	pTos = pNos;` |
|   114848 |  4199 | `	break;` |
|        - |  4200 | `				}` |
|        - |  4201 | `/*  CAT_STORE: * * *` |
|        - |  4202 | ` *` |
|        - |  4203 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4204 | ` * back.` |
|        - |  4205 | ` */` |
|     2404 |  4206 | `case PH7_OP_CAT_STORE:{` |
|     4810 |  4207 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4208 | `	ph7_value *pObj;` |
|        - |  4209 | `#ifdef UNTRUST` |
|        - |  4210 | `	if( pNos < pStack ){` |
|        - |  4211 | `		goto Abort;` |
|        - |  4212 | `	}` |
|        - |  4213 | `#endif` |
|     4810 |  4214 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4215 | `		/* Force a string cast */` |
|      ! 0 |  4216 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4217 | `	}` |
|     4810 |  4218 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4219 | `		/* Force a string cast */` |
|      ! 0 |  4220 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4221 | `	}` |
|        - |  4222 | `	/* Perform the concatenation (Reverse order) */` |
|     4810 |  4223 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     4810 |  4224 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     2404 |  4225 | `	}` |
|        - |  4226 | `	/* Perform the store operation */` |
|     4810 |  4227 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4228 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     4810 |  4229 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     4810 |  4230 | `		PH7_MemObjStore(pTos,pObj);` |
|     2404 |  4231 | `	}` |
|     4810 |  4232 | `	PH7_MemObjStore(pTos,pNos);` |
|     4810 |  4233 | `	VmPopOperand(&pTos,1);` |
|     4810 |  4234 | `	break;` |
|        - |  4235 | `				}` |
|        - |  4236 | `/* OP_AND: * * *` |
|        - |  4237 | ` *` |
|        - |  4238 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4239 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4240 | ` * stack.` |
|        - |  4241 | ` */` |
|        - |  4242 | `/* OP_OR: * * *` |
|        - |  4243 | ` *` |
|        - |  4244 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4245 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4246 | ` * stack.` |
|        - |  4247 | ` */` |
|    99146 |  4248 | `case PH7_OP_LAND:` |
|        - |  4249 | `case PH7_OP_LOR: {` |
|   198338 |  4250 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4251 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4252 | `#ifdef UNTRUST` |
|        - |  4253 | `	if( pNos < pStack ){` |
|        - |  4254 | `		goto Abort;` |
|        - |  4255 | `	}` |
|        - |  4256 | `#endif` |
|        - |  4257 | `	/* Force a boolean cast */` |
|   198338 |  4258 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4259 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4260 | `	}` |
|   198338 |  4261 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4262 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4263 | `	}` |
|   198338 |  4264 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   198338 |  4265 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   198338 |  4266 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4267 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|   100230 |  4268 | `		v1 = and_logic[v1*3+v2];` |
|    50138 |  4269 | `	}else{` |
|        - |  4270 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|    98110 |  4271 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4272 | `	}` |
|   198338 |  4273 | `	if( v1 == 2 ){` |
|      ! 0 |  4274 | `		v1 = 1;` |
|      ! 0 |  4275 | `	}` |
|   198338 |  4276 | `	VmPopOperand(&pTos,1);` |
|   198338 |  4277 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   198338 |  4278 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   198338 |  4279 | `	break;` |
|        - |  4280 | `				 }` |
|        - |  4281 | `/* OP_LXOR: * * *` |
|        - |  4282 | ` *` |
|        - |  4283 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4284 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4285 | ` * stack.` |
|        - |  4286 | ` * According to the PHP language reference manual:` |
|        - |  4287 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4288 | ` *  TRUE,but not both.` |
|        - |  4289 | ` */` |
|        5 |  4290 | `case PH7_OP_LXOR:{` |
|       11 |  4291 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4292 | `	sxi32 v = 0;` |
|        - |  4293 | `#ifdef UNTRUST` |
|        - |  4294 | `	if( pNos < pStack ){` |
|        - |  4295 | `		goto Abort;` |
|        - |  4296 | `	}` |
|        - |  4297 | `#endif` |
|        - |  4298 | `	/* Force a boolean cast */` |
|       11 |  4299 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4300 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4301 | `	}` |
|       11 |  4302 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4303 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4304 | `	}` |
|       11 |  4305 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4306 | `		v = 1;` |
|        3 |  4307 | `	}` |
|       11 |  4308 | `	VmPopOperand(&pTos,1);` |
|       11 |  4309 | `	pTos->x.iVal = v;` |
|       11 |  4310 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4311 | `	break;` |
|        - |  4312 | `				 }` |
|        - |  4313 | `/* OP_EQ P1 P2 P3` |
|        - |  4314 | ` *` |
|        - |  4315 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4316 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4317 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4318 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4319 | ` */` |
|        - |  4320 | `/* OP_NEQ P1 P2 P3` |
|        - |  4321 | ` *` |
|        - |  4322 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4323 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4324 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4325 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4326 | ` */` |
|     3597 |  4327 | `case PH7_OP_EQ:` |
|        - |  4328 | `case PH7_OP_NEQ: {` |
|     7196 |  4329 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4330 | `	/* Perform the comparison and act accordingly */` |
|        - |  4331 | `#ifdef UNTRUST` |
|        - |  4332 | `	if( pNos < pStack ){` |
|        - |  4333 | `		goto Abort;` |
|        - |  4334 | `	}` |
|        - |  4335 | `#endif` |
|     7196 |  4336 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7196 |  4337 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       11 |  4338 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7191 |  4339 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7160 |  4340 | `		rc = rc == 0;` |
|     3581 |  4341 | `	}else{` |
|       28 |  4342 | `		rc = rc != 0;` |
|        - |  4343 | `	}` |
|     7196 |  4344 | `	VmPopOperand(&pTos,1);` |
|     7196 |  4345 | `	if( !pInstr->iP2 ){` |
|        - |  4346 | `		/* Push comparison result without taking the jump */` |
|     7196 |  4347 | `		PH7_MemObjRelease(pTos);` |
|     7196 |  4348 | `		pTos->x.iVal = rc;` |
|        - |  4349 | `		/* Invalidate any prior representation */` |
|     7196 |  4350 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3599 |  4351 | `	}else{` |
|      ! 0 |  4352 | `		if( rc ){` |
|        - |  4353 | `			/* Jump to the desired location */` |
|      ! 0 |  4354 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4355 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4356 | `		}` |
|        - |  4357 | `	}` |
|     7196 |  4358 | `	break;` |
|        - |  4359 | `				 }` |
|        - |  4360 | `/* OP_TEQ P1 P2 *` |
|        - |  4361 | ` *` |
|        - |  4362 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4363 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4364 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4365 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4366 | ` */` |
|   119636 |  4367 | `case PH7_OP_TEQ: {` |
|   239274 |  4368 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4369 | `	/* Perform the comparison and act accordingly */` |
|        - |  4370 | `#ifdef UNTRUST` |
|        - |  4371 | `	if( pNos < pStack ){` |
|        - |  4372 | `		goto Abort;` |
|        - |  4373 | `	}` |
|        - |  4374 | `#endif` |
|   239274 |  4375 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   239274 |  4376 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4377 | `		rc = 0;` |
|        2 |  4378 | `	}else{` |
|   239272 |  4379 | `		rc = rc == 0;` |
|        - |  4380 | `	}` |
|   239274 |  4381 | `	VmPopOperand(&pTos,1);` |
|   239274 |  4382 | `	if( !pInstr->iP2 ){` |
|        - |  4383 | `		/* Push comparison result without taking the jump */` |
|   239274 |  4384 | `		PH7_MemObjRelease(pTos);` |
|   239274 |  4385 | `		pTos->x.iVal = rc;` |
|        - |  4386 | `		/* Invalidate any prior representation */` |
|   239274 |  4387 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   119638 |  4388 | `	}else{` |
|      ! 0 |  4389 | `		if( rc ){` |
|        - |  4390 | `			/* Jump to the desired location */` |
|      ! 0 |  4391 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4392 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4393 | `		}` |
|        - |  4394 | `	}` |
|   239274 |  4395 | `	break;` |
|        - |  4396 | `				 }` |
|        - |  4397 | `/* OP_TNE P1 P2 *` |
|        - |  4398 | ` *` |
|        - |  4399 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4400 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4401 | ` * instruction.` |
|        - |  4402 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4403 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4404 | ` *` |
|        - |  4405 | ` */` |
|    95057 |  4406 | `case PH7_OP_TNE: {` |
|   190116 |  4407 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4408 | `	/* Perform the comparison and act accordingly */` |
|        - |  4409 | `#ifdef UNTRUST` |
|        - |  4410 | `	if( pNos < pStack ){` |
|        - |  4411 | `		goto Abort;` |
|        - |  4412 | `	}` |
|        - |  4413 | `#endif` |
|   190116 |  4414 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   190116 |  4415 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4416 | `		rc = 1;` |
|        2 |  4417 | `	}else{` |
|   190114 |  4418 | `		rc = rc != 0;` |
|        - |  4419 | `	}` |
|   190116 |  4420 | `	VmPopOperand(&pTos,1);` |
|   190116 |  4421 | `	if( !pInstr->iP2 ){` |
|        - |  4422 | `		/* Push comparison result without taking the jump */` |
|   190116 |  4423 | `		PH7_MemObjRelease(pTos);` |
|   190116 |  4424 | `		pTos->x.iVal = rc;` |
|        - |  4425 | `		/* Invalidate any prior representation */` |
|   190116 |  4426 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    95059 |  4427 | `	}else{` |
|      ! 0 |  4428 | `		if( rc ){` |
|        - |  4429 | `			/* Jump to the desired location */` |
|      ! 0 |  4430 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4431 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4432 | `		}` |
|        - |  4433 | `	}` |
|   190116 |  4434 | `	break;` |
|        - |  4435 | `				 }` |
|        - |  4436 | `/* OP_LT P1 P2 P3` |
|        - |  4437 | ` *` |
|        - |  4438 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4439 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4440 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4441 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4442 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4443 | ` *` |
|        - |  4444 | ` */` |
|        - |  4445 | `/* OP_LE P1 P2 P3` |
|        - |  4446 | ` *` |
|        - |  4447 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4448 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4449 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4450 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4451 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4452 | ` *` |
|        - |  4453 | ` */` |
|   108969 |  4454 | `case PH7_OP_LT:` |
|        - |  4455 | `case PH7_OP_LE: {` |
|   217984 |  4456 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4457 | `	/* Perform the comparison and act accordingly */` |
|        - |  4458 | `#ifdef UNTRUST` |
|        - |  4459 | `	if( pNos < pStack ){` |
|        - |  4460 | `		goto Abort;` |
|        - |  4461 | `	}` |
|        - |  4462 | `#endif` |
|   217984 |  4463 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   217984 |  4464 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4465 | `		rc = 0;` |
|   217980 |  4466 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      395 |  4467 | `		rc = rc < 1;` |
|      198 |  4468 | `	}else{` |
|   217582 |  4469 | `		rc = rc < 0;` |
|        - |  4470 | `	}` |
|   217984 |  4471 | `	VmPopOperand(&pTos,1);` |
|   217984 |  4472 | `	if( !pInstr->iP2 ){` |
|        - |  4473 | `		/* Push comparison result without taking the jump */` |
|   217984 |  4474 | `		PH7_MemObjRelease(pTos);` |
|   217984 |  4475 | `		pTos->x.iVal = rc;` |
|        - |  4476 | `		/* Invalidate any prior representation */` |
|   217984 |  4477 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   109015 |  4478 | `	}else{` |
|      ! 0 |  4479 | `		if( rc ){` |
|        - |  4480 | `			/* Jump to the desired location */` |
|      ! 0 |  4481 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4482 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4483 | `		}` |
|        - |  4484 | `	}` |
|   217984 |  4485 | `	break;` |
|        - |  4486 | `				}` |
|        - |  4487 | `/* OP_GT P1 P2 P3` |
|        - |  4488 | ` *` |
|        - |  4489 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4490 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4491 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4492 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4493 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4494 | ` *` |
|        - |  4495 | ` */` |
|        - |  4496 | `/* OP_GE P1 P2 P3` |
|        - |  4497 | ` *` |
|        - |  4498 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4499 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4500 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4501 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4502 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4503 | ` *` |
|        - |  4504 | ` */` |
|    46737 |  4505 | `case PH7_OP_GT:` |
|        - |  4506 | `case PH7_OP_GE: {` |
|    93476 |  4507 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4508 | `	/* Perform the comparison and act accordingly */` |
|        - |  4509 | `#ifdef UNTRUST` |
|        - |  4510 | `	if( pNos < pStack ){` |
|        - |  4511 | `		goto Abort;` |
|        - |  4512 | `	}` |
|        - |  4513 | `#endif` |
|    93476 |  4514 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    93476 |  4515 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4516 | `		rc = 0;` |
|    93472 |  4517 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    93320 |  4518 | `		rc = rc >= 0;` |
|    46661 |  4519 | `	}else{` |
|      150 |  4520 | `		rc = rc > 0;` |
|        - |  4521 | `	}` |
|    93476 |  4522 | `	VmPopOperand(&pTos,1);` |
|    93476 |  4523 | `	if( !pInstr->iP2 ){` |
|        - |  4524 | `		/* Push comparison result without taking the jump */` |
|    93476 |  4525 | `		PH7_MemObjRelease(pTos);` |
|    93476 |  4526 | `		pTos->x.iVal = rc;` |
|        - |  4527 | `		/* Invalidate any prior representation */` |
|    93476 |  4528 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    46739 |  4529 | `	}else{` |
|      ! 0 |  4530 | `		if( rc ){` |
|        - |  4531 | `			/* Jump to the desired location */` |
|      ! 0 |  4532 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4533 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4534 | `		}` |
|        - |  4535 | `	}` |
|    93476 |  4536 | `	break;` |
|        - |  4537 | `				}` |
|        - |  4538 | `/* OP_SEQ P1 P2 *` |
|        - |  4539 | ` * Strict string comparison.` |
|        - |  4540 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4541 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4542 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4543 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4544 | ` * use PH7_OP_EQ.` |
|        - |  4545 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4546 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4547 | ` */` |
|        - |  4548 | `/* OP_SNE P1 P2 *` |
|        - |  4549 | ` * Strict string comparison.` |
|        - |  4550 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4551 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4552 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4553 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4554 | ` * use PH7_OP_EQ.` |
|        - |  4555 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4556 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4557 | ` */` |
|       18 |  4558 | `case PH7_OP_SEQ:` |
|        - |  4559 | `case PH7_OP_SNE: {` |
|       38 |  4560 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4561 | `	SyString s1,s2;` |
|        - |  4562 | `	/* Perform the comparison and act accordingly */` |
|        - |  4563 | `#ifdef UNTRUST` |
|        - |  4564 | `	if( pNos < pStack ){` |
|        - |  4565 | `		goto Abort;` |
|        - |  4566 | `	}` |
|        - |  4567 | `#endif` |
|        - |  4568 | `	/* Force a string cast */` |
|       38 |  4569 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4570 | `		PH7_MemObjToString(pTos);` |
|        2 |  4571 | `	}` |
|       38 |  4572 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4573 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4574 | `	}` |
|       38 |  4575 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4576 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4577 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4578 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4579 | `		rc = rc != 0;` |
|      ! 0 |  4580 | `	}else{` |
|       38 |  4581 | `		rc = rc == 0;` |
|        - |  4582 | `	}` |
|       38 |  4583 | `	VmPopOperand(&pTos,1);` |
|       38 |  4584 | `	if( !pInstr->iP2 ){` |
|        - |  4585 | `		/* Push comparison result without taking the jump */` |
|       38 |  4586 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4587 | `		pTos->x.iVal = rc;` |
|        - |  4588 | `		/* Invalidate any prior representation */` |
|       38 |  4589 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4590 | `	}else{` |
|      ! 0 |  4591 | `		if( rc ){` |
|        - |  4592 | `			/* Jump to the desired location */` |
|      ! 0 |  4593 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4594 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4595 | `		}` |
|        - |  4596 | `	}` |
|       38 |  4597 | `	break;` |
|        - |  4598 | `				 }` |
|        - |  4599 | `/*` |
|        - |  4600 | ` * OP_LOAD_REF * * *` |
|        - |  4601 | ` * Push the index of a referenced object on the stack.` |
|        - |  4602 | ` */` |
|       57 |  4603 | `case PH7_OP_LOAD_REF: {` |
|        - |  4604 | `	sxu32 nIdx;` |
|        - |  4605 | `#ifdef UNTRUST` |
|        - |  4606 | `	if( pTos < pStack ){` |
|        - |  4607 | `		goto Abort;` |
|        - |  4608 | `	}` |
|        - |  4609 | `#endif` |
|        - |  4610 | `	/* Extract memory object index */` |
|      115 |  4611 | `	nIdx = pTos->nIdx;` |
|      115 |  4612 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4613 | `		/* Nullify the object */` |
|       95 |  4614 | `		PH7_MemObjRelease(pTos);` |
|        - |  4615 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4616 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4617 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4618 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4619 | `	}` |
|      115 |  4620 | `	break;` |
|        - |  4621 | `					  }` |
|        - |  4622 | `/*` |
|        - |  4623 | ` * OP_STORE_REF * * P3` |
|        - |  4624 | ` * Perform an assignment operation by reference.` |
|        - |  4625 | ` */` |
|       14 |  4626 | ` case PH7_OP_STORE_REF: {` |
|       30 |  4627 | `	 SyString sName = { 0 , 0 };` |
|        - |  4628 | `	 VmFrame *pFrameLocal;` |
|        - |  4629 | `	SyHashEntry *pEntry;` |
|        - |  4630 | `	sxu32 nIdx;` |
|        - |  4631 | `#ifdef UNTRUST` |
|        - |  4632 | `	if( pTos < pStack ){` |
|        - |  4633 | `		goto Abort;` |
|        - |  4634 | `	}` |
|        - |  4635 | `#endif` |
|       30 |  4636 | `	if( pInstr->p3 == 0 ){` |
|        - |  4637 | `		char *zName;` |
|        - |  4638 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4639 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4640 | `			/* Force a string cast */` |
|      ! 0 |  4641 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4642 | `		}` |
|      ! 0 |  4643 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4644 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4645 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4646 | `			if( zName ){` |
|      ! 0 |  4647 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4648 | `			}` |
|      ! 0 |  4649 | `		}` |
|      ! 0 |  4650 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4651 | `		pTos--;` |
|      ! 0 |  4652 | `	}else{` |
|       30 |  4653 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4654 | `	}` |
|       30 |  4655 | `	nIdx = pTos->nIdx;` |
|       30 |  4656 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4657 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4658 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4659 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4660 | `		}else{` |
|        - |  4661 | `			ph7_value *pObj;` |
|        - |  4662 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4663 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4664 | `			if( pObj == 0 ){` |
|      ! 0 |  4665 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4666 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4667 | `				goto Abort;` |
|        - |  4668 | `			}` |
|        - |  4669 | `			/* Perform the store operation */` |
|      ! 0 |  4670 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4671 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4672 | `		}` |
|       30 |  4673 | `	}else if( sName.nByte > 0){` |
|       30 |  4674 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4675 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4676 | `		}else{` |
|       30 |  4677 | `			pFrameLocal = pVm->pFrame;` |
|       50 |  4678 | `			while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4679 | `				/* Safely ignore the exception frame */` |
|       21 |  4680 | `				pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4681 | `			}` |
|        - |  4682 | `			/* Query the local frame */` |
|       30 |  4683 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       30 |  4684 | `			if( pEntry ){` |
|      ! 0 |  4685 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4686 | `			}else{` |
|       30 |  4687 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       30 |  4688 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4689 | `					/* Insert in the $GLOBALS array */` |
|       26 |  4690 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       12 |  4691 | `				}` |
|       30 |  4692 | `				if( rc == SXRET_OK ){` |
|       30 |  4693 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       14 |  4694 | `				}` |
|        - |  4695 | `			}` |
|        - |  4696 | `		}` |
|       14 |  4697 | `	}` |
|       30 |  4698 | `	break;` |
|        - |  4699 | `				 }` |
|        - |  4700 | `/*` |
|        - |  4701 | ` * OP_UPLINK P1 * *` |
|        - |  4702 | ` * Link a variable to the top active VM frame.` |
|        - |  4703 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4704 | ` */` |
|       23 |  4705 | `case PH7_OP_UPLINK: {` |
|       47 |  4706 | `	if( pVm->pFrame->pParent ){` |
|       47 |  4707 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4708 | `		SyString sName;` |
|        - |  4709 | `		/* Perform the link */` |
|       95 |  4710 | `		while( pLink <= pTos ){` |
|       49 |  4711 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4712 | `				/* Force a string cast */` |
|      ! 0 |  4713 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4714 | `			}` |
|       49 |  4715 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       49 |  4716 | `			if( sName.nByte > 0 ){` |
|       49 |  4717 | `				VmFrameLink(&(*pVm),&sName);` |
|       24 |  4718 | `			}` |
|       49 |  4719 | `			pLink++;` |
|        1 |  4720 | `		}` |
|       23 |  4721 | `	}` |
|       47 |  4722 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       47 |  4723 | `	break;` |
|        - |  4724 | `					}` |
|        - |  4725 | `/*` |
|        - |  4726 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4727 | ` * Push an exception in the corresponding container so that` |
|        - |  4728 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4729 | ` */` |
|       10 |  4730 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       22 |  4731 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4732 | `	VmFrame *pFrameLocal;` |
|       22 |  4733 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4734 | `	/* Create the exception frame */` |
|       22 |  4735 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       22 |  4736 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4737 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4738 | `		goto Abort;` |
|        - |  4739 | `	}` |
|        - |  4740 | `	/* Mark the special frame */` |
|       22 |  4741 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       22 |  4742 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4743 | `	/* Point to the frame that trigger the exception */` |
|       22 |  4744 | `	pFrameLocal = pFrameLocal->pParent;` |
|       34 |  4745 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|       13 |  4746 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4747 | `	}` |
|       22 |  4748 | `	pException->pFrame = pFrameLocal;` |
|       22 |  4749 | `	break;` |
|        - |  4750 | `							}` |
|        - |  4751 | `/*` |
|        - |  4752 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4753 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4754 | ` */` |
|        9 |  4755 | `case PH7_OP_POP_EXCEPTION: {` |
|       20 |  4756 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       20 |  4757 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4758 | `		ph7_exception **apException;` |
|        - |  4759 | `		/* Pop the loaded exception */` |
|        7 |  4760 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        7 |  4761 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|        7 |  4762 | `			(void)SySetPop(&pVm->aException);` |
|        3 |  4763 | `		}` |
|        3 |  4764 | `	}` |
|       20 |  4765 | `	pException->pFrame = 0;` |
|        - |  4766 | `	/* Leave the exception frame */` |
|       20 |  4767 | `	VmLeaveFrame(&(*pVm));` |
|       20 |  4768 | `	break;` |
|        - |  4769 | `							}` |
|        - |  4770 |  |
|        - |  4771 | `/*` |
|        - |  4772 | ` * OP_THROW * P2 *` |
|        - |  4773 | ` * Throw an user exception.` |
|        - |  4774 | ` */` |
|       10 |  4775 | `case PH7_OP_THROW: {` |
|       22 |  4776 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       22 |  4777 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4778 | `#ifdef UNTRUST` |
|        - |  4779 | `	if( pTos < pStack ){` |
|        - |  4780 | `		goto Abort;` |
|        - |  4781 | `	}` |
|        - |  4782 | `#endif` |
|       28 |  4783 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4784 | `		/* Safely ignore the exception frame */` |
|        8 |  4785 | `		pFrameLocal = pFrameLocal->pParent;` |
|        2 |  4786 | `	}` |
|        - |  4787 | `	/* Tell the upper layer that an exception was thrown */` |
|       22 |  4788 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       22 |  4789 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       22 |  4790 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4791 | `		ph7_class *pException;` |
|        - |  4792 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4793 | `		 */` |
|       22 |  4794 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       22 |  4795 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4796 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4797 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4798 | `			if( rc == SXERR_ABORT ){` |
|        - |  4799 | `				/* Abort processing immediately */` |
|      ! 0 |  4800 | `				goto Abort;` |
|        - |  4801 | `			}` |
|      ! 0 |  4802 | `		}else{` |
|        - |  4803 | `			/* Throw the exception */` |
|       22 |  4804 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       22 |  4805 | `			if( rc == SXERR_ABORT ){` |
|        - |  4806 | `				/* Abort processing immediately */` |
|        7 |  4807 | `				goto Abort;` |
|        - |  4808 | `			}` |
|        - |  4809 | `		}` |
|        9 |  4810 | `	}else{` |
|        - |  4811 | `		/* Expecting a class instance */` |
|      ! 0 |  4812 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  4813 | `		if( rc == SXERR_ABORT ){` |
|        - |  4814 | `			/* Abort processing immediately */` |
|      ! 0 |  4815 | `			goto Abort;` |
|        - |  4816 | `		}` |
|        - |  4817 | `	}` |
|        - |  4818 | `	/* Pop the top entry */` |
|       16 |  4819 | `	VmPopOperand(&pTos,1);` |
|        - |  4820 | `	/* Perform an unconditional jump */` |
|       16 |  4821 | `	pc = nJump - 1;` |
|       16 |  4822 | `	break;` |
|        - |  4823 | `				   }` |
|        - |  4824 | `/*` |
|        - |  4825 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  4826 | ` * Prepare a foreach step.` |
|        - |  4827 | ` */` |
|     4385 |  4828 | `case PH7_OP_FOREACH_INIT: {` |
|     8772 |  4829 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4830 | `	void *pName;` |
|        - |  4831 | `#ifdef UNTRUST` |
|        - |  4832 | `	if( pTos < pStack ){` |
|        - |  4833 | `		goto Abort;` |
|        - |  4834 | `	}` |
|        - |  4835 | `#endif` |
|     8772 |  4836 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4837 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  4838 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4839 | `			/* Force a string cast */` |
|      ! 0 |  4840 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4841 | `		}` |
|        - |  4842 | `		/* Duplicate name */` |
|      ! 0 |  4843 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4844 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4845 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4846 | `		}` |
|      ! 0 |  4847 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4848 | `	}` |
|     8772 |  4849 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  4850 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4851 | `			/* Force a string cast */` |
|      ! 0 |  4852 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4853 | `		}` |
|        - |  4854 | `		/* Duplicate name */` |
|      ! 0 |  4855 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4856 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4857 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4858 | `		}` |
|      ! 0 |  4859 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4860 | `	}` |
|        - |  4861 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     8772 |  4862 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4863 | `		/* Jump out of the loop */` |
|      ! 0 |  4864 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4865 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4866 | `		}` |
|      ! 0 |  4867 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4868 | `	}else{` |
|        - |  4869 | `		ph7_foreach_step *pStep;` |
|     8772 |  4870 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     8772 |  4871 | `		if( pStep == 0 ){` |
|      ! 0 |  4872 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4873 | `			/* Jump out of the loop */` |
|      ! 0 |  4874 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4875 | `		}else{` |
|        - |  4876 | `			/* Zero the structure */` |
|     8772 |  4877 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4878 | `			/* Prepare the step */` |
|     8772 |  4879 | `			pStep->iFlags = pInfo->iFlags;` |
|     8772 |  4880 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     8764 |  4881 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4882 | `				/* Reset the internal loop cursor */` |
|     8764 |  4883 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4884 | `				/* Mark the step */` |
|     8764 |  4885 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     8764 |  4886 | `				pStep->xIter.pMap = pMap;` |
|     8764 |  4887 | `				pMap->iRef++;` |
|     4383 |  4888 | `			}else{` |
|        9 |  4889 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4890 | `				/* Reset the loop cursor */` |
|        9 |  4891 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|        - |  4892 | `				/* Mark the step */` |
|        9 |  4893 | `				pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  4894 | `				pStep->xIter.pThis = pThis;` |
|        9 |  4895 | `				pThis->iRef++;` |
|        - |  4896 | `			}` |
|        - |  4897 | `		}` |
|     8772 |  4898 | `		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  4899 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  4900 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  4901 | `			/* Jump out of the loop */` |
|      ! 0 |  4902 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4903 | `		}` |
|        - |  4904 | `	}` |
|     8772 |  4905 | `	VmPopOperand(&pTos,1);` |
|     8772 |  4906 | `	break;` |
|        - |  4907 | `						  }` |
|        - |  4908 | `/*` |
|        - |  4909 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  4910 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  4911 | ` */` |
|    70974 |  4912 | `case PH7_OP_FOREACH_STEP: {` |
|   141950 |  4913 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4914 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  4915 | `	ph7_value *pValue;` |
|        - |  4916 | `	VmFrame *pFrameLocal;` |
|        - |  4917 | `	/* Peek the last step */` |
|   141950 |  4918 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   141950 |  4919 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   141950 |  4920 | `	pFrameLocal = pVm->pFrame;` |
|   146982 |  4921 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4922 | `		/* Safely ignore the exception frame */` |
|     5033 |  4923 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4924 | `	}` |
|   141950 |  4925 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   141926 |  4926 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  4927 | `		ph7_hashmap_node *pNode;` |
|        - |  4928 | `		/* Extract the current node value */` |
|   141926 |  4929 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   141926 |  4930 | `		if( pNode == 0 ){` |
|        - |  4931 | `			/* No more entry to process */` |
|     8764 |  4932 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     8764 |  4933 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4934 | `				/* Break the reference with the last element */` |
|        5 |  4935 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  4936 | `			}` |
|        - |  4937 | `			/* Automatically reset the loop cursor */` |
|     8764 |  4938 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4939 | `			/* Cleanup the mess left behind */` |
|     8764 |  4940 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     8764 |  4941 | `			SySetPop(&pInfo->aStep);` |
|     8764 |  4942 | `			PH7_HashmapUnref(pMap);` |
|     4383 |  4943 | `		}else{` |
|   133164 |  4944 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      259 |  4945 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      259 |  4946 | `				if( pKey ){` |
|      259 |  4947 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      129 |  4948 | `				}` |
|      129 |  4949 | `			}` |
|   133164 |  4950 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4951 | `				SyHashEntry *pEntry;` |
|        - |  4952 | `				/* Pass by reference */` |
|       13 |  4953 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       13 |  4954 | `				if( pEntry ){` |
|       13 |  4955 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        7 |  4956 | `				}else{` |
|      ! 0 |  4957 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  4958 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  4959 | `				}` |
|        7 |  4960 | `			}else{` |
|        - |  4961 | `				/* Make a copy of the entry value */` |
|   133152 |  4962 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   133152 |  4963 | `				if( pValue ){` |
|   133152 |  4964 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    66575 |  4965 | `				}` |
|        - |  4966 | `			}` |
|        - |  4967 | `		}` |
|    70964 |  4968 | `	}else{` |
|       25 |  4969 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  4970 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  4971 | `		SyHashEntry *pEntry;` |
|        - |  4972 | `		/* Point to the next attribute */` |
|       29 |  4973 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  4974 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  4975 | `			/* Check access permission */` |
|       31 |  4976 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  4977 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  4978 | `					break; /* Access is granted */` |
|        - |  4979 | `			}` |
|        1 |  4980 | `		}` |
|       25 |  4981 | `		if( pEntry == 0 ){` |
|        - |  4982 | `			/* Clean up the mess left behind */` |
|        9 |  4983 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  4984 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4985 | `				/* Break the reference with the last element */` |
|        3 |  4986 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  4987 | `			}` |
|        9 |  4988 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  4989 | `			SySetPop(&pInfo->aStep);` |
|        9 |  4990 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  4991 | `		}else{` |
|       17 |  4992 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  4993 | `			ph7_value *pAttrValue;` |
|       17 |  4994 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  4995 | `				/* Fill with the current attribute name */` |
|       17 |  4996 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  4997 | `				if( pKey ){` |
|       17 |  4998 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  4999 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5000 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5001 | `				}` |
|        8 |  5002 | `			}` |
|        - |  5003 | `			/* Extract attribute value */` |
|       17 |  5004 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5005 | `			if( pAttrValue ){` |
|       17 |  5006 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5007 | `					/* Pass by reference */` |
|        3 |  5008 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5009 | `					if( pEntry ){` |
|        3 |  5010 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5011 | `					}else{` |
|      ! 0 |  5012 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5013 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5014 | `					}` |
|        2 |  5015 | `				}else{` |
|        - |  5016 | `					/* Make a copy of the attribute value */` |
|       15 |  5017 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5018 | `					if( pValue ){` |
|       15 |  5019 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5020 | `					}` |
|        - |  5021 | `				}` |
|        8 |  5022 | `			}` |
|        - |  5023 | `		}` |
|        - |  5024 | `	}` |
|   141950 |  5025 | `	break;` |
|        - |  5026 | `						  }` |
|        - |  5027 | `/*` |
|        - |  5028 | ` * OP_MEMBER P1 P2` |
|        - |  5029 | ` * Load class attribute/method on the stack.` |
|        - |  5030 | ` */` |
|     1488 |  5031 | `case PH7_OP_MEMBER: {` |
|        - |  5032 | `	ph7_class_instance *pThis;` |
|        - |  5033 | `	ph7_value *pNos;` |
|        - |  5034 | `	SyString sName;` |
|     2978 |  5035 | `	if( !pInstr->iP1 ){` |
|     2920 |  5036 | `		pNos = &pTos[-1];` |
|        - |  5037 | `#ifdef UNTRUST` |
|        - |  5038 | `		if( pNos < pStack ){` |
|        - |  5039 | `			goto Abort;` |
|        - |  5040 | `		}` |
|        - |  5041 | `#endif` |
|     2920 |  5042 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5043 | `			ph7_class *pClass;` |
|        - |  5044 | `			/* Class already instantiated */` |
|     2920 |  5045 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5046 | `			/* Point to the instantiated class */` |
|     2920 |  5047 | `			pClass = pThis->pClass;` |
|        - |  5048 | `			/* Extract attribute name first */` |
|     2920 |  5049 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     2920 |  5050 | `			if( pInstr->iP2 ){` |
|        - |  5051 | `				/* Method call */` |
|      120 |  5052 | `				ph7_class_method *pMeth = 0;` |
|      120 |  5053 | `				if( sName.nByte > 0 ){` |
|        - |  5054 | `					/* Extract the target method */` |
|      120 |  5055 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       59 |  5056 | `				}` |
|      120 |  5057 | `				if( pMeth == 0 ){` |
|      ! 0 |  5058 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5059 | `						&pClass->sName,&sName` |
|        - |  5060 | `						);` |
|        - |  5061 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5062 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5063 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5064 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5065 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5066 | `				}else{` |
|        - |  5067 | `					/* Push method name on the stack */` |
|      120 |  5068 | `					PH7_MemObjRelease(pTos);` |
|      120 |  5069 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      120 |  5070 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5071 | `				}` |
|      120 |  5072 | `				pTos->nIdx = SXU32_HIGH;` |
|       61 |  5073 | `			}else{` |
|        - |  5074 | `				/* Attribute access */` |
|     2802 |  5075 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5076 | `				SyHashEntry *pEntry;` |
|        - |  5077 | `				/* Extract the target attribute */` |
|     2802 |  5078 | `				if( sName.nByte > 0 ){` |
|     2802 |  5079 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     2802 |  5080 | `					if( pEntry ){` |
|        - |  5081 | `						/* Point to the attribute value */` |
|     2800 |  5082 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1399 |  5083 | `					}` |
|     1400 |  5084 | `				}` |
|     2802 |  5085 | `				if( pObjAttr == 0 ){` |
|        - |  5086 | `					/* No such attribute,load null */` |
|        4 |  5087 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5088 | `						&pClass->sName,&sName);` |
|        - |  5089 | `					/* Call the __get magic method if available */` |
|        3 |  5090 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5091 | `				}` |
|     2802 |  5092 | `				VmPopOperand(&pTos,1);` |
|        - |  5093 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5094 | `				 * This is due to the following case:` |
|        - |  5095 | `				 *     (new TestClass())->foo;` |
|        - |  5096 | `				 */` |
|     2802 |  5097 | `				pThis->iRef++;` |
|     2802 |  5098 | `				PH7_MemObjRelease(pTos);` |
|     2802 |  5099 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     2802 |  5100 | `				if( pObjAttr ){` |
|     2800 |  5101 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5102 | `					/* Check attribute access */` |
|     2800 |  5103 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5104 | `						/* Load attribute */` |
|     2800 |  5105 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     2800 |  5106 | `						if( pValue ){` |
|     2800 |  5107 | `							if( pThis->iRef < 2 ){` |
|        - |  5108 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5109 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5110 | `								 */` |
|        3 |  5111 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5112 | `							}else{` |
|        - |  5113 | `								/* Simple load */` |
|     2798 |  5114 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5115 | `							}` |
|     2800 |  5116 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     2798 |  5117 | `								if( pThis->iRef > 1 ){` |
|        - |  5118 | `									/* Load attribute index */` |
|     2796 |  5119 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1397 |  5120 | `								}` |
|     1398 |  5121 | `							}` |
|     1399 |  5122 | `						}` |
|     1399 |  5123 | `					}` |
|     1399 |  5124 | `				}` |
|        - |  5125 | `				/* Safely unreference the object */` |
|     2802 |  5126 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5127 | `			}` |
|     1461 |  5128 | `		}else{` |
|      ! 0 |  5129 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5130 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5131 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5132 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5133 | `		}` |
|     1461 |  5134 | `	}else{` |
|        - |  5135 | `		/* Static member access using class name */` |
|       59 |  5136 | `		pNos = pTos;` |
|       59 |  5137 | `		pThis = 0;` |
|       59 |  5138 | `		if( !pInstr->p3 ){` |
|       57 |  5139 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       57 |  5140 | `			pNos--;` |
|        - |  5141 | `#ifdef UNTRUST` |
|        - |  5142 | `			if( pNos < pStack ){` |
|        - |  5143 | `				goto Abort;` |
|        - |  5144 | `			}` |
|        - |  5145 | `#endif` |
|       29 |  5146 | `		}else{` |
|        - |  5147 | `			/* Attribute name already computed */` |
|        3 |  5148 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5149 | `		}` |
|       59 |  5150 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|       59 |  5151 | `			ph7_class *pClass = 0;` |
|       59 |  5152 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5153 | `				/* Class already instantiated */` |
|      ! 0 |  5154 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5155 | `				pClass = pThis->pClass;` |
|      ! 0 |  5156 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5157 | `			}else{` |
|        - |  5158 | `				/* Try to extract the target class */` |
|       59 |  5159 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       88 |  5160 | `					pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pNos->sBlob),` |
|       29 |  5161 | `						SyBlobLength(&pNos->sBlob),FALSE,0);` |
|       29 |  5162 | `				}` |
|        - |  5163 | `			}` |
|       59 |  5164 | `			if( pClass == 0 ){` |
|        - |  5165 | `				/* Undefined class */` |
|      ! 0 |  5166 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5167 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5168 | `					);` |
|      ! 0 |  5169 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5170 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5171 | `				}` |
|      ! 0 |  5172 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5173 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5174 | `			}else{` |
|       59 |  5175 | `				if( pInstr->iP2 ){` |
|        - |  5176 | `					/* Method call */` |
|       25 |  5177 | `					ph7_class_method *pMeth = 0;` |
|       25 |  5178 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5179 | `						/* Extract the target method */` |
|       25 |  5180 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       12 |  5181 | `					}` |
|       25 |  5182 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5183 | `						if( pMeth ){` |
|      ! 0 |  5184 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5185 | `								&pClass->sName,&sName` |
|        - |  5186 | `								);` |
|      ! 0 |  5187 | `						}else{` |
|      ! 0 |  5188 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5189 | `								&pClass->sName,&sName` |
|        - |  5190 | `								);` |
|        - |  5191 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5192 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5193 | `						}` |
|        - |  5194 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5195 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5196 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5197 | `						}` |
|      ! 0 |  5198 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5199 | `					}else{` |
|        - |  5200 | `						/* Push method name on the stack */` |
|       25 |  5201 | `						PH7_MemObjRelease(pTos);` |
|       25 |  5202 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       25 |  5203 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5204 | `					}` |
|       25 |  5205 | `					pTos->nIdx = SXU32_HIGH;` |
|       13 |  5206 | `				}else{` |
|        - |  5207 | `					/* Attribute access */` |
|       35 |  5208 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5209 | `					/* Check for special ::class pseudo-constant */` |
|       49 |  5210 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       28 |  5211 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5212 | `						/* ::class returns the fully qualified class name */` |
|        - |  5213 | `						/* Pop the attribute name from the stack */` |
|       27 |  5214 | `						if( !pInstr->p3 ){` |
|       27 |  5215 | `							VmPopOperand(&pTos,1);` |
|       13 |  5216 | `						}` |
|       27 |  5217 | `						PH7_MemObjRelease(pTos);` |
|        - |  5218 | `						/* Load the class name */` |
|       27 |  5219 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       27 |  5220 | `						pTos->nIdx = SXU32_HIGH;` |
|       14 |  5221 | `					}else{` |
|        - |  5222 | `						/* Extract the target attribute */` |
|        9 |  5223 | `						if( sName.nByte > 0 ){` |
|        9 |  5224 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        4 |  5225 | `						}` |
|        9 |  5226 | `						if( pAttr == 0 ){` |
|        - |  5227 | `							/* No such attribute,load null */` |
|      ! 0 |  5228 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5229 | `								&pClass->sName,&sName);` |
|        - |  5230 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5231 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5232 | `						}` |
|        - |  5233 | `						/* Pop the attribute name from the stack */` |
|        9 |  5234 | `						if( !pInstr->p3 ){` |
|        7 |  5235 | `							VmPopOperand(&pTos,1);` |
|        3 |  5236 | `						}` |
|        9 |  5237 | `						PH7_MemObjRelease(pTos);` |
|        9 |  5238 | `						pTos->nIdx = SXU32_HIGH;` |
|        9 |  5239 | `						if( pAttr ){` |
|        9 |  5240 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5241 | `								/* Access to a non static attribute */` |
|      ! 0 |  5242 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5243 | `									&pClass->sName,&pAttr->sName` |
|        - |  5244 | `									);` |
|      ! 0 |  5245 | `							}else{` |
|        - |  5246 | `								ph7_value *pValue;` |
|        - |  5247 | `								/* Check if the access to the attribute is allowed */` |
|        9 |  5248 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5249 | `									/* Load the desired attribute */` |
|        9 |  5250 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|        9 |  5251 | `									if( pValue ){` |
|        9 |  5252 | `										PH7_MemObjLoad(pValue,pTos);` |
|        9 |  5253 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5254 | `											/* Load index number */` |
|        3 |  5255 | `											pTos->nIdx = pAttr->nIdx;` |
|        1 |  5256 | `										}` |
|        4 |  5257 | `									}` |
|        4 |  5258 | `								}` |
|        - |  5259 | `							}` |
|        4 |  5260 | `						}` |
|        - |  5261 | `					}` |
|        - |  5262 | `				}` |
|       59 |  5263 | `				if( pThis ){` |
|        - |  5264 | `					/* Safely unreference the object */` |
|      ! 0 |  5265 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5266 | `				}` |
|        - |  5267 | `			}` |
|       30 |  5268 | `		}else{` |
|        - |  5269 | `			/* Pop operands */` |
|      ! 0 |  5270 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5271 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5272 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5273 | `			}` |
|      ! 0 |  5274 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5275 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5276 | `		}` |
|        - |  5277 | `	}` |
|     2978 |  5278 | `	break;` |
|        - |  5279 | `					}` |
|        - |  5280 | `/*` |
|        - |  5281 | ` * OP_NEW P1 * * *` |
|        - |  5282 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5283 | ` */` |
|      252 |  5284 | `case PH7_OP_NEW: {` |
|      506 |  5285 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      506 |  5286 | `	ph7_class *pClass = 0;` |
|        - |  5287 | `	ph7_class_instance *pNew;` |
|      506 |  5288 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5289 | `		/* Try to extract the desired class */` |
|      758 |  5290 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      504 |  5291 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      252 |  5292 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5293 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5294 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5295 | `	}` |
|      506 |  5296 | `	if( pClass == 0 ){` |
|        - |  5297 | `		/* No such class */` |
|      ! 0 |  5298 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined,PH7 is loading NULL",` |
|      ! 0 |  5299 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5300 | `			);` |
|      ! 0 |  5301 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5302 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5303 | `			/* Pop given arguments */` |
|      ! 0 |  5304 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5305 | `		}` |
|      ! 0 |  5306 | `	}else{` |
|        - |  5307 | `		ph7_class_method *pCons;` |
|        - |  5308 | `		/* Create a new class instance */` |
|      506 |  5309 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      506 |  5310 | `		if( pNew == 0 ){` |
|      ! 0 |  5311 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5312 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5313 | `				&pClass->sName` |
|        - |  5314 | `			);` |
|      ! 0 |  5315 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5316 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5317 | `				/* Pop given arguments */` |
|      ! 0 |  5318 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5319 | `			}` |
|      ! 0 |  5320 | `			break;` |
|        - |  5321 | `		}` |
|        - |  5322 | `		/* Check if a constructor is available */` |
|      506 |  5323 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      506 |  5324 | `		if( pCons == 0 ){` |
|      450 |  5325 | `			SyString *pName = &pClass->sName;` |
|        - |  5326 | `			/* Check for a constructor with the same base class name */` |
|      450 |  5327 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      224 |  5328 | `		}` |
|      506 |  5329 | `		if( pCons ){` |
|        - |  5330 | `			/* Call the class constructor */` |
|       58 |  5331 | `			SySetReset(&aArg);` |
|      104 |  5332 | `			while( pArg < pTos ){` |
|       48 |  5333 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       48 |  5334 | `				pArg++;` |
|        2 |  5335 | `			}` |
|       58 |  5336 | `			if( pVm->bErrReport ){` |
|        - |  5337 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5338 | `				sxu32 n;` |
|       15 |  5339 | `				n = SySetUsed(&aArg);` |
|        - |  5340 | `				/* Emit a notice for missing arguments */` |
|       39 |  5341 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       25 |  5342 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       25 |  5343 | `					if( pFuncArg ){` |
|       25 |  5344 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5345 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5346 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5347 | `						}` |
|       12 |  5348 | `					}` |
|       25 |  5349 | `					n++;` |
|        1 |  5350 | `				}` |
|        7 |  5351 | `			}` |
|       58 |  5352 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5353 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       58 |  5354 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5355 | `				pNew->iRef = 1;` |
|      ! 0 |  5356 | `			}` |
|       28 |  5357 | `		}` |
|      506 |  5358 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5359 | `			/* Pop given arguments */` |
|       42 |  5360 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       20 |  5361 | `		}` |
|      506 |  5362 | `		PH7_MemObjRelease(pTos);` |
|      506 |  5363 | `		pTos->x.pOther = pNew;` |
|      506 |  5364 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5365 | `	}` |
|      506 |  5366 | `	break;` |
|        - |  5367 | `				 }` |
|        - |  5368 | `/*` |
|        - |  5369 | ` * OP_CLONE * * *` |
|        - |  5370 | ` * Perfome a clone operation.` |
|        - |  5371 | ` */` |
|       23 |  5372 | `case PH7_OP_CLONE: {` |
|        - |  5373 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5374 | `#ifdef UNTRUST` |
|        - |  5375 | `	if( pTos < pStack ){` |
|        - |  5376 | `		goto Abort;` |
|        - |  5377 | `	}` |
|        - |  5378 | `#endif` |
|        - |  5379 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5380 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5381 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5382 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5383 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5384 | `		break;` |
|        - |  5385 | `	}` |
|        - |  5386 | `	/* Point to the source */` |
|       44 |  5387 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5388 | `	/* Perform the clone operation */` |
|       44 |  5389 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5390 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5391 | `	if( pClone == 0 ){` |
|      ! 0 |  5392 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5393 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5394 | `	}else{` |
|        - |  5395 | `		/* Load the cloned object */` |
|       44 |  5396 | `		pTos->x.pOther = pClone;` |
|       44 |  5397 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5398 | `	}` |
|       44 |  5399 | `	break;` |
|        - |  5400 | `				   }` |
|        - |  5401 | `/*` |
|        - |  5402 | ` * OP_SWITCH * * P3` |
|        - |  5403 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5404 | ` */` |
|       18 |  5405 | `case PH7_OP_SWITCH: {` |
|       38 |  5406 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5407 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5408 | `	ph7_value sValue,sCaseValue;` |
|        - |  5409 | `	sxu32 n,nEntry;` |
|        - |  5410 | `#ifdef UNTRUST` |
|        - |  5411 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5412 | `		goto Abort;` |
|        - |  5413 | `	}` |
|        - |  5414 | `#endif` |
|        - |  5415 | `	/* Point to the case table  */` |
|       38 |  5416 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5417 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5418 | `	/* Select the appropriate case block to execute */` |
|       38 |  5419 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5420 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5421 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5422 | `		pCase = &aCase[n];` |
|       92 |  5423 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5424 | `		/* Execute the case expression first */` |
|       92 |  5425 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5426 | `		/* Compare the two expression */` |
|       92 |  5427 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5428 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5429 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5430 | `		if( rc == 0 ){` |
|        - |  5431 | `			/* Value match,jump to this block */` |
|       38 |  5432 | `			pc = pCase->nStart - 1;` |
|       38 |  5433 | `			break;` |
|        - |  5434 | `		}` |
|       29 |  5435 | `	}` |
|       38 |  5436 | `	VmPopOperand(&pTos,1);` |
|       38 |  5437 | `	if( n >= nEntry ){` |
|        - |  5438 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5439 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5440 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5441 | `		}else{` |
|        - |  5442 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5443 | `			pc = pSwitch->nOut - 1;` |
|        - |  5444 | `		}` |
|      ! 0 |  5445 | `	}` |
|       38 |  5446 | `	break;` |
|        - |  5447 | `					}` |
|        - |  5448 | `/*` |
|        - |  5449 | ` * OP_CALL P1 * *` |
|        - |  5450 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5451 | ` *  function on the stack.` |
|        - |  5452 | ` */` |
|   269297 |  5453 | `case PH7_OP_CALL: {` |
|   538640 |  5454 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5455 | `	SyHashEntry *pEntry;` |
|        - |  5456 | `	SyString sName;` |
|        - |  5457 | `	/* Extract function name */` |
|   538640 |  5458 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5459 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5460 | `			ph7_value sResult;` |
|      ! 0 |  5461 | `			SySetReset(&aArg);` |
|      ! 0 |  5462 | `			while( pArg < pTos ){` |
|      ! 0 |  5463 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5464 | `				pArg++;` |
|      ! 0 |  5465 | `			}` |
|      ! 0 |  5466 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5467 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5468 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5469 | `			SySetReset(&aArg);` |
|        - |  5470 | `			/* Pop given arguments */` |
|      ! 0 |  5471 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5472 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5473 | `			}` |
|        - |  5474 | `			/* Copy result */` |
|      ! 0 |  5475 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5476 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5477 | `		}else{` |
|        3 |  5478 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5479 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5480 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5481 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5482 | `			}else{` |
|        - |  5483 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5484 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5485 | `			}` |
|        - |  5486 | `			/* Pop given arguments */` |
|        3 |  5487 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5488 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5489 | `			}` |
|        - |  5490 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5491 | `			PH7_MemObjRelease(pTos);` |
|        - |  5492 | `		}` |
|   269118 |  5493 | `		break;` |
|        - |  5494 | `	}` |
|   538638 |  5495 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5496 | `	/* Check for a compiled function first */` |
|   538638 |  5497 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|   538638 |  5498 | `	if( pEntry ){` |
|        - |  5499 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5500 | `		ph7_class_instance *pThis;` |
|        - |  5501 | `		ph7_value *pFrameStack;` |
|        - |  5502 | `		ph7_vm_func *pVmFunc;` |
|        - |  5503 | `		ph7_class *pSelf;` |
|        - |  5504 | `		VmFrame *pFrame;` |
|        - |  5505 | `		ph7_value *pObj;` |
|        - |  5506 | `		VmSlot sArg;` |
|        - |  5507 | `		sxu32 n;` |
|        - |  5508 | `		/* initialize fields */` |
|    10666 |  5509 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    10666 |  5510 | `		pThis = 0;` |
|    10666 |  5511 | `		pSelf = 0;` |
|    10666 |  5512 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5513 | `			ph7_class_method *pMeth;` |
|        - |  5514 | `			/* Class method call */` |
|     1062 |  5515 | `			ph7_value *pTarget = &pTos[-1];` |
|     1062 |  5516 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5517 | `				/* Extract the 'this' pointer */` |
|     1062 |  5518 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5519 | `					/* Instance already loaded */` |
|     1032 |  5520 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1032 |  5521 | `					pThis->iRef++;` |
|     1032 |  5522 | `					pSelf = pThis->pClass;` |
|      515 |  5523 | `				}` |
|     1062 |  5524 | `				if( pSelf == 0 ){` |
|       31 |  5525 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5526 | `						/* "Late Static Binding" class name */` |
|       37 |  5527 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       12 |  5528 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       12 |  5529 | `					}` |
|       31 |  5530 | `					if( pSelf == 0 ){` |
|        7 |  5531 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 |  5532 | `					}` |
|       15 |  5533 | `				}` |
|     1062 |  5534 | `				if( pThis == 0  ){` |
|       31 |  5535 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       33 |  5536 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5537 | `						/* Safely ignore the exception frame */` |
|        3 |  5538 | `						pFrameLocal = pFrameLocal->pParent;` |
|        1 |  5539 | `					}` |
|       31 |  5540 | `					if( pFrameLocal->pParent ){` |
|        - |  5541 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5542 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5543 | `						if( pThis ){` |
|       13 |  5544 | `							pThis->iRef++;` |
|        6 |  5545 | `						}` |
|        9 |  5546 | `					}` |
|       15 |  5547 | `				}` |
|     1062 |  5548 | `				VmPopOperand(&pTos,1);` |
|     1062 |  5549 | `				PH7_MemObjRelease(pTos);` |
|        - |  5550 | `				/* Synchronize pointers */` |
|     1062 |  5551 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5552 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5553 | `				 * user have already computed the random generated unique class method name` |
|        - |  5554 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5555 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5556 | `				 */` |
|     1062 |  5557 | `				while( pArg < pStack ){` |
|      ! 0 |  5558 | `					pArg++;` |
|      ! 0 |  5559 | `				}` |
|     1062 |  5560 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5561 | `					/* Check if the call is allowed */` |
|     1062 |  5562 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1062 |  5563 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        5 |  5564 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5565 | `							/* Pop given arguments */` |
|      ! 0 |  5566 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5567 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5568 | `							}` |
|        - |  5569 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5570 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5571 | `							break;` |
|        - |  5572 | `						}` |
|        2 |  5573 | `					}` |
|      530 |  5574 | `				}` |
|      530 |  5575 | `			}` |
|      530 |  5576 | `		}` |
|        - |  5577 | `		/* Check The recursion limit */` |
|    10666 |  5578 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5579 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5580 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5581 | `				&pVmFunc->sName);` |
|        - |  5582 | `			/* Pop given arguments */` |
|        3 |  5583 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5584 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5585 | `			}` |
|        - |  5586 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5587 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5588 | `			break;` |
|        - |  5589 | `		}` |
|    10664 |  5590 | `		if( pVmFunc->pNextName ){` |
|        - |  5591 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      123 |  5592 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       61 |  5593 | `		}` |
|        - |  5594 | `		/* Extract the formal argument set */` |
|    10664 |  5595 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5596 | `		/* Create a new VM frame  */` |
|    10664 |  5597 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    10664 |  5598 | `		if( rc != SXRET_OK ){` |
|        - |  5599 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5600 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5601 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5602 | `				&pVmFunc->sName);` |
|        - |  5603 | `			/* Pop given arguments */` |
|      ! 0 |  5604 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5605 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5606 | `			}` |
|        - |  5607 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5608 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5609 | `			break;` |
|        - |  5610 | `		}` |
|    10664 |  5611 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5612 | `			/* Install the '$this' variable */` |
|        - |  5613 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1042 |  5614 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1042 |  5615 | `			if( pObj ){` |
|        - |  5616 | `				/* Reflect the change */` |
|     1042 |  5617 | `				pObj->x.pOther = pThis;` |
|     1042 |  5618 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      520 |  5619 | `			}` |
|      520 |  5620 | `		}` |
|    10664 |  5621 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5622 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5623 | `			/* Install static variables */` |
|      ! 0 |  5624 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5625 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5626 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5627 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5628 | `					/* Initialize the static variables */` |
|      ! 0 |  5629 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5630 | `					if( pObj ){` |
|        - |  5631 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5632 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5633 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5634 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5635 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5636 | `						}` |
|      ! 0 |  5637 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5638 | `					}else{` |
|      ! 0 |  5639 | `						continue;` |
|        - |  5640 | `					}` |
|      ! 0 |  5641 | `				}` |
|        - |  5642 | `				/* Install in the current frame */` |
|      ! 0 |  5643 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5644 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5645 | `			}` |
|      ! 0 |  5646 | `		}` |
|        - |  5647 | `		/* Push arguments in the local frame */` |
|    10664 |  5648 | `		n = 0;` |
|    29994 |  5649 | `		while( pArg < pTos ){` |
|    19332 |  5650 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    19214 |  5651 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5652 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5653 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5654 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5655 | `						goto Abort;` |
|        - |  5656 | `					}` |
|      ! 0 |  5657 | `				}` |
|        - |  5658 | `				/* Make sure the given arguments are of the correct type */` |
|    19214 |  5659 | `				if( aFormalArg[n].nType > 0 ){` |
|     1066 |  5660 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5661 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5662 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5663 | `						ph7_class *pClass;` |
|        - |  5664 | `						/* Try to extract the desired class */` |
|      ! 0 |  5665 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5666 | `						if( pClass ){` |
|      ! 0 |  5667 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5668 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5669 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5670 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5671 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5672 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5673 | `								}` |
|      ! 0 |  5674 | `							}else{` |
|        - |  5675 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5676 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5677 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5678 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5679 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5680 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5681 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5682 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5683 | `								}` |
|        - |  5684 | `							}` |
|      ! 0 |  5685 | `						}` |
|     1066 |  5686 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5687 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5688 | `						/* Cast to the desired type */` |
|      ! 0 |  5689 | `						xCast(pArg);` |
|      ! 0 |  5690 | `					}` |
|      532 |  5691 | `				}` |
|    19214 |  5692 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5693 | `					/* Pass by reference */` |
|       48 |  5694 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5695 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5696 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5697 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5698 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5699 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5700 | `						}` |
|        - |  5701 | `						/* Switch to pass by value */` |
|      ! 0 |  5702 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5703 | `					}else{` |
|        - |  5704 | `						SyHashEntry *pRefEntry;` |
|        - |  5705 | `						/* Install the referenced variable in the private function frame */` |
|       48 |  5706 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       48 |  5707 | `						if( pRefEntry == 0 ){` |
|       71 |  5708 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       46 |  5709 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       48 |  5710 | `							sArg.nIdx = pArg->nIdx;` |
|       48 |  5711 | `							sArg.pUserData = 0;` |
|       48 |  5712 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  5713 | `						}` |
|       48 |  5714 | `						pObj = 0;` |
|        - |  5715 | `					}` |
|       25 |  5716 | `				}else{` |
|        - |  5717 | `					/* Pass by value,make a copy of the given argument */` |
|    19168 |  5718 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5719 | `				}` |
|     9608 |  5720 | `			}else{` |
|        - |  5721 | `				char zName[32];` |
|        - |  5722 | `				SyString sArgName;` |
|        - |  5723 | `				/* Set a dummy name */` |
|      120 |  5724 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      120 |  5725 | `				sArgName.zString = zName;` |
|        - |  5726 | `				/* Annonymous argument */` |
|      120 |  5727 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5728 | `			}` |
|    19332 |  5729 | `			if( pObj ){` |
|    19286 |  5730 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5731 | `				/* Insert argument index  */` |
|    19286 |  5732 | `				sArg.nIdx = pObj->nIdx;` |
|    19286 |  5733 | `				sArg.pUserData = 0;` |
|    19286 |  5734 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|     9642 |  5735 | `			}` |
|    19332 |  5736 | `			PH7_MemObjRelease(pArg);` |
|    19332 |  5737 | `			pArg++;` |
|    19332 |  5738 | `			++n;` |
|        2 |  5739 | `		}` |
|        - |  5740 | `		/* Set up closure environment */` |
|    10664 |  5741 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5742 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  5743 | `			ph7_value *pValue;` |
|        - |  5744 | `			sxu32 iEnv;` |
|        9 |  5745 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  5746 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  5747 | `				pEnv = &aEnv[iEnv];` |
|       17 |  5748 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  5749 | `					/* Do not install null value */` |
|        9 |  5750 | `					continue;` |
|        - |  5751 | `				}` |
|        9 |  5752 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  5753 | `				if( pValue == 0 ){` |
|      ! 0 |  5754 | `					continue;` |
|        - |  5755 | `				}` |
|        - |  5756 | `				/* Invalidate any prior representation */` |
|        9 |  5757 | `				PH7_MemObjRelease(pValue);` |
|        - |  5758 | `				/* Duplicate bound variable value */` |
|        9 |  5759 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  5760 | `			}` |
|        4 |  5761 | `		}` |
|        - |  5762 | `		/* Process default values */` |
|    12252 |  5763 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1590 |  5764 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1580 |  5765 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1580 |  5766 | `				if( pObj ){` |
|        - |  5767 | `					/* Evaluate the default value and extract it's result */` |
|     1580 |  5768 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1580 |  5769 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5770 | `						goto Abort;` |
|        - |  5771 | `					}` |
|        - |  5772 | `					/* Insert argument index */` |
|     1580 |  5773 | `					sArg.nIdx = pObj->nIdx;` |
|     1580 |  5774 | `					sArg.pUserData = 0;` |
|     1580 |  5775 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  5776 | `					/* Make sure the default argument is of the correct type */` |
|     1580 |  5777 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5778 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5779 | `						/* Cast to the desired type */` |
|      ! 0 |  5780 | `						xCast(pObj);` |
|      ! 0 |  5781 | `					}` |
|      789 |  5782 | `				}` |
|      789 |  5783 | `			}` |
|     1590 |  5784 | `			++n;` |
|        2 |  5785 | `		}` |
|        - |  5786 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  5787 | `		 * does not return anything.` |
|        - |  5788 | `		 */` |
|    10664 |  5789 | `		PH7_MemObjRelease(pTos);` |
|    10664 |  5790 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  5791 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    10664 |  5792 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    10664 |  5793 | `		if( pFrameStack == 0 ){` |
|        - |  5794 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5795 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5796 | `				&pVmFunc->sName);` |
|      ! 0 |  5797 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5798 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5799 | `			}` |
|      ! 0 |  5800 | `			break;` |
|        - |  5801 | `		}` |
|    10664 |  5802 | `		if( pSelf ){` |
|        - |  5803 | `			/* Push class name */` |
|     1060 |  5804 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      529 |  5805 | `		}` |
|        - |  5806 | `		/* Increment nesting level */` |
|    10664 |  5807 | `		pVm->nRecursionDepth++;` |
|        - |  5808 | `		/* Execute function body */` |
|    10664 |  5809 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  5810 | `		/* Decrement nesting level */` |
|    10664 |  5811 | `		pVm->nRecursionDepth--;` |
|    10664 |  5812 | `		if( pSelf ){` |
|        - |  5813 | `			/* Pop class name */` |
|     1060 |  5814 | `			(void)SySetPop(&pVm->aSelf);` |
|      529 |  5815 | `		}` |
|        - |  5816 | `		/* Cleanup the mess left behind */` |
|    10664 |  5817 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  5818 | `			/* Return by reference,reflect that */` |
|        9 |  5819 | `			if( n != SXU32_HIGH ){` |
|        9 |  5820 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  5821 | `				sxu32 i;` |
|        - |  5822 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  5823 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  5824 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  5825 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  5826 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5827 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5828 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  5829 | `								&pVmFunc->sName);` |
|      ! 0 |  5830 | `						}` |
|      ! 0 |  5831 | `						n = SXU32_HIGH;` |
|      ! 0 |  5832 | `						break;` |
|        - |  5833 | `					}` |
|        3 |  5834 | `				}` |
|        5 |  5835 | `			}else{` |
|      ! 0 |  5836 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5837 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5838 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  5839 | `						&pVmFunc->sName);` |
|      ! 0 |  5840 | `				}` |
|        - |  5841 | `			}` |
|        9 |  5842 | `			pTos->nIdx = n;` |
|        4 |  5843 | `		}` |
|        - |  5844 | `		/* Cleanup the mess left behind */` |
|    10664 |  5845 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  5846 | `			/* An exception was throw in this frame */` |
|        7 |  5847 | `			pFrame = pFrame->pParent;` |
|        7 |  5848 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  5849 | `				/* Pop the resutlt */` |
|        5 |  5850 | `				VmPopOperand(&pTos,1);` |
|        - |  5851 | `				/* Jump to this destination */` |
|        5 |  5852 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  5853 | `				rc = PH7_OK;` |
|        3 |  5854 | `			}else{` |
|        3 |  5855 | `				if( pFrame->pParent ){` |
|        3 |  5856 | `					rc = PH7_EXCEPTION;` |
|        2 |  5857 | `				}else{` |
|        - |  5858 | `					/* Continue normal execution */` |
|      ! 0 |  5859 | `					rc = PH7_OK;` |
|        - |  5860 | `				}` |
|        - |  5861 | `			}` |
|        3 |  5862 | `		}` |
|        - |  5863 | `		/* Free the operand stack */` |
|    10664 |  5864 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  5865 | `		/* Leave the frame */` |
|    10664 |  5866 | `		VmLeaveFrame(&(*pVm));` |
|    10664 |  5867 | `		if( rc == PH7_ABORT ){` |
|        - |  5868 | `			/* Abort processing immeditaley */` |
|        5 |  5869 | `			goto Abort;` |
|    10660 |  5870 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5871 | `			goto Exception;` |
|        - |  5872 | `		}` |
|     5330 |  5873 | `	}else{` |
|        - |  5874 | `		ph7_user_func *pFunc;` |
|        - |  5875 | `		ph7_context sCtx;` |
|        - |  5876 | `		ph7_value sRet;` |
|        - |  5877 | `		/* Look for an installed foreign function */` |
|   527974 |  5878 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   527974 |  5879 | `		if( pEntry == 0 ){` |
|        - |  5880 | `			/* Call to undefined function */` |
|        5 |  5881 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  5882 | `			/* Pop given arguments */` |
|        5 |  5883 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5884 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5885 | `			}` |
|        - |  5886 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  5887 | `			PH7_MemObjRelease(pTos);` |
|        5 |  5888 | `			break;` |
|        - |  5889 | `		}` |
|   527970 |  5890 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  5891 | `		/* Start collecting function arguments */` |
|   527970 |  5892 | `		SySetReset(&aArg);` |
|  1405090 |  5893 | `		while( pArg < pTos ){` |
|   877122 |  5894 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   877122 |  5895 | `			pArg++;` |
|        2 |  5896 | `		}` |
|        - |  5897 | `		/* Assume a null return value */` |
|   527970 |  5898 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  5899 | `		/* Init the call context */` |
|   527970 |  5900 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  5901 | `		/* Call the foreign function */` |
|   527970 |  5902 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5903 | `		/* Release the call context */` |
|   527970 |  5904 | `		VmReleaseCallContext(&sCtx);` |
|   527970 |  5905 | `		if( rc == PH7_ABORT ){` |
|      355 |  5906 | `			goto Abort;` |
|   527616 |  5907 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5908 | `			goto Exception;` |
|        - |  5909 | `		}` |
|   527614 |  5910 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5911 | `			/* Pop function name and arguments */` |
|   510622 |  5912 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   255332 |  5913 | `		}` |
|        - |  5914 | `		/* Save foreign function return value */` |
|   527614 |  5915 | `		PH7_MemObjStore(&sRet,pTos);` |
|   527614 |  5916 | `		PH7_MemObjRelease(&sRet);` |
|        - |  5917 | `	}` |
|   538270 |  5918 | `	break;` |
|        - |  5919 | `				  }` |
|        - |  5920 | `/*` |
|        - |  5921 | ` * OP_CONSUME: P1 * *` |
|        - |  5922 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  5923 | ` */` |
|     9821 |  5924 | `case PH7_OP_CONSUME: {` |
|    19644 |  5925 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    19644 |  5926 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  5927 |  |
|    19644 |  5928 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    19644 |  5929 | `	pCur = pOut;` |
|        - |  5930 | `	/* Start the consume process  */` |
|    39286 |  5931 | `	while( pOut <= pTos ){` |
|        - |  5932 | `		/* Force a string cast */` |
|    19644 |  5933 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      158 |  5934 | `			PH7_MemObjToString(pOut);` |
|       78 |  5935 | `		}` |
|    19644 |  5936 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  5937 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  5938 | `			/* Invoke the output consumer callback */` |
|    10498 |  5939 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    10498 |  5940 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  5941 | `				/* Increment output length */` |
|     4148 |  5942 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     2073 |  5943 | `			}` |
|    10498 |  5944 | `			SyBlobRelease(&pOut->sBlob);` |
|    10498 |  5945 | `			if( rc == SXERR_ABORT ){` |
|        - |  5946 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  5947 | `				goto Abort;` |
|        - |  5948 | `			}` |
|     5248 |  5949 | `		}` |
|    19644 |  5950 | `		pOut++;` |
|        2 |  5951 | `	}` |
|    19644 |  5952 | `	pTos = &pCur[-1];` |
|    19642 |  5953 | `	break;` |
|        - |  5954 | `					 }` |
|        - |  5955 |  |
|        - |  5956 | `		} /* Switch() */` |
|  9392686 |  5957 | `		pc++; /* Next instruction in the stream */` |
|        2 |  5958 | `	} /* For(;;) */` |
|    13291 |  5959 | `Done:` |
|    26584 |  5960 | `	SySetRelease(&aArg);` |
|    26584 |  5961 | `	return SXRET_OK;` |
|      182 |  5962 | `Abort:` |
|      365 |  5963 | `	SySetRelease(&aArg);` |
|     1271 |  5964 | `	while( pTos >= pStack ){` |
|      907 |  5965 | `		PH7_MemObjRelease(pTos);` |
|      907 |  5966 | `		pTos--;` |
|        1 |  5967 | `	}` |
|      365 |  5968 | `	return PH7_ABORT;` |
|        2 |  5969 | `Exception:` |
|        5 |  5970 | `	SySetRelease(&aArg);` |
|        9 |  5971 | `	while( pTos >= pStack ){` |
|        5 |  5972 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5973 | `		pTos--;` |
|        1 |  5974 | `	}` |
|        5 |  5975 | `	return PH7_EXCEPTION;` |
|    13477 |  5976 |  |
|        - |  5977 | `/*` |
|        - |  5978 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  5979 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  5980 | ` * See block-comment on that function for additional information.` |
|        - |  5981 | ` */` |
|    13274 |  5982 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  5983 |  |
|        - |  5984 | `	ph7_value *pStack;` |
|        - |  5985 | `	sxi32 rc;` |
|        - |  5986 | `	/* Allocate a new operand stack */` |
|    13276 |  5987 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    13276 |  5988 | `	if( pStack == 0 ){` |
|      ! 0 |  5989 | `		return SXERR_MEM;` |
|        - |  5990 | `	}` |
|        - |  5991 | `	/* Execute the program */` |
|    13276 |  5992 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  5993 | `	/* Free the operand stack */` |
|    13276 |  5994 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  5995 | `	/* Execution result */` |
|    13276 |  5996 | `	return rc;` |
|     6639 |  5997 |  |
|        - |  5998 | `/*` |
|        - |  5999 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6000 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6001 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6002 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6003 | ` * execution ends.` |
|        - |  6004 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6005 | ` * additional information.` |
|        - |  6006 | ` */` |
|     1664 |  6007 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6008 |  |
|        - |  6009 | `	VmShutdownCB *pEntry;` |
|        - |  6010 | `	ph7_value *apArg[10];` |
|        - |  6011 | `	sxu32 n,nEntry;` |
|        - |  6012 | `	int i;` |
|        - |  6013 | `	/* Point to the stack of registered callbacks */` |
|     1666 |  6014 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    18306 |  6015 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    16642 |  6016 | `		apArg[i] = 0;` |
|     8322 |  6017 | `	}` |
|     1668 |  6018 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6019 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6020 | `		if( pEntry ){` |
|        - |  6021 | `			/* Prepare callback arguments if any */` |
|        3 |  6022 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6023 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6024 | `					break;` |
|        - |  6025 | `				}` |
|      ! 0 |  6026 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6027 | `			}` |
|        - |  6028 | `			/* Invoke the callback */` |
|        3 |  6029 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6030 | `			/*` |
|        - |  6031 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6032 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6033 | `			 */` |
|        3 |  6034 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6035 | `			if( pEntry ){` |
|        3 |  6036 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6037 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6038 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6039 | `				}` |
|        1 |  6040 | `			}` |
|        1 |  6041 | `		}` |
|        2 |  6042 | `	}` |
|     1666 |  6043 | `	SySetReset(&pVm->aShutdown);` |
|     1666 |  6044 |  |
|        - |  6045 | `/*` |
|        - |  6046 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6047 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6048 | ` * See block-comment on that function for additional information.` |
|        - |  6049 | ` */` |
|     1672 |  6050 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6051 |  |
|        - |  6052 | `	/* Make sure we are ready to execute this program */` |
|     1674 |  6053 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6054 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6055 | `	}` |
|        - |  6056 | `	/* Set the execution magic number  */` |
|     1674 |  6057 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6058 | `	/* Execute the program */` |
|     1674 |  6059 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6060 | `	/* Invoke any shutdown callbacks */` |
|     1670 |  6061 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6062 | `	/*` |
|        - |  6063 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6064 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6065 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6066 | `	 */` |
|     1670 |  6067 | `	return SXRET_OK;` |
|      838 |  6068 |  |
|        - |  6069 | `/*` |
|        - |  6070 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6071 | ` * the desired message.` |
|        - |  6072 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6073 | ` * in 'api.c' for additional information.` |
|        - |  6074 | ` */` |
|      352 |  6075 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6076 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6077 | `	SyString *pString /* Message to output */` |
|        - |  6078 | `	)` |
|        2 |  6079 |  |
|      354 |  6080 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      354 |  6081 | `	sxi32 rc = SXRET_OK;` |
|        - |  6082 | `	/* Call the output consumer */` |
|      354 |  6083 | `	if( pString->nByte > 0 ){` |
|      354 |  6084 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      354 |  6085 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6086 | `			/* Increment output length */` |
|       17 |  6087 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6088 | `		}` |
|      176 |  6089 | `	}` |
|      354 |  6090 | `	return rc;` |
|        2 |  6091 |  |
|        - |  6092 | `/*` |
|        - |  6093 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6094 | ` * callback to consume the formatted message.` |
|        - |  6095 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6096 | ` * in 'api.c' for additional information.` |
|        - |  6097 | ` */` |
|        2 |  6098 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6099 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6100 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6101 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6102 | `	)` |
|        1 |  6103 |  |
|        3 |  6104 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6105 | `	sxi32 rc = SXRET_OK;` |
|        - |  6106 | `	SyBlob sWorker;` |
|        - |  6107 | `	/* Format the message and call the output consumer */` |
|        3 |  6108 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6109 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6110 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6111 | `		/* Consume the formatted message */` |
|        3 |  6112 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6113 | `	}` |
|        3 |  6114 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6115 | `		/* Increment output length */` |
|      ! 0 |  6116 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6117 | `	}` |
|        - |  6118 | `	/* Release the working buffer */` |
|        3 |  6119 | `	SyBlobRelease(&sWorker);` |
|        3 |  6120 | `	return rc;` |
|        1 |  6121 |  |
|        - |  6122 | `/*` |
|        - |  6123 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6124 | ` * This function never fail and always return a pointer` |
|        - |  6125 | ` * to a null terminated string.` |
|        - |  6126 | ` */` |
|       10 |  6127 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6128 |  |
|       11 |  6129 | `	const char *zOp = "Unknown     ";` |
|       11 |  6130 | `	switch(nOp){` |
|        3 |  6131 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6132 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6133 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6134 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6135 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6136 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6137 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6138 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6139 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6140 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6141 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6142 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6143 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6144 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6145 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6146 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6147 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6148 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6149 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6150 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6151 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6152 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6153 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6154 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6155 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6156 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6157 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6158 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6159 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6160 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6161 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6162 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6163 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6164 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6165 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6166 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6167 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6168 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6169 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6170 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6171 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6172 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6173 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6174 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6175 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6176 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6177 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6178 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6179 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6180 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6181 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6182 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6183 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6184 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6185 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6186 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6187 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6188 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6189 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6190 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6191 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6192 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6193 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6194 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6195 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6196 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6197 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6198 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6199 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6200 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6201 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6202 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6203 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6204 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6205 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6206 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6207 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6208 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6209 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6210 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6211 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6212 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6213 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6214 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6215 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6216 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6217 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6218 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6219 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6220 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6221 | `	default:` |
|      ! 0 |  6222 | `		break;` |
|        - |  6223 | `	}` |
|       11 |  6224 | `	return zOp;` |
|        1 |  6225 |  |
|        - |  6226 | `/*` |
|        - |  6227 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6228 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6229 | ` * is responsible of consuming the generated dump.` |
|        - |  6230 | ` */` |
|        2 |  6231 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6232 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6233 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6234 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6235 | `	)` |
|        1 |  6236 |  |
|        - |  6237 | `	sxi32 rc;` |
|        3 |  6238 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6239 | `	return rc;` |
|        1 |  6240 |  |
|        - |  6241 | `/*` |
|        - |  6242 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6243 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6244 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6245 | ` * in 'compile.c' for additional information.` |
|        - |  6246 | ` */` |
|        8 |  6247 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6248 |  |
|        9 |  6249 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6250 | `	/* Evaluate and expand constant value */` |
|        9 |  6251 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6252 |  |
|        - |  6253 | `/*` |
|        - |  6254 | ` * Section:` |
|        - |  6255 | ` *  Function handling functions.` |
|        - |  6256 | ` * Status:` |
|        - |  6257 | ` *    Stable.` |
|        - |  6258 | ` */` |
|        - |  6259 | `/*` |
|        - |  6260 | ` * int func_num_args(void)` |
|        - |  6261 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6262 | ` * Parameters` |
|        - |  6263 | ` *   None.` |
|        - |  6264 | ` * Return` |
|        - |  6265 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6266 | ` *  or -1 if called from the globe scope.` |
|        - |  6267 | ` */` |
|      868 |  6268 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6269 |  |
|        - |  6270 | `	VmFrame *pFrame;` |
|        - |  6271 | `	ph7_vm *pVm;` |
|        - |  6272 | `	/* Point to the target VM */` |
|      870 |  6273 | `	pVm = pCtx->pVm;` |
|        - |  6274 | `	/* Current frame */` |
|      870 |  6275 | `	pFrame = pVm->pFrame;` |
|      870 |  6276 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6277 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6278 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6279 | `	}` |
|      870 |  6280 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6281 | `		SXUNUSED(nArg);` |
|      ! 0 |  6282 | `		SXUNUSED(apArg);` |
|        - |  6283 | `		/* Global frame,return -1 */` |
|      ! 0 |  6284 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6285 | `		return SXRET_OK;` |
|        - |  6286 | `	}` |
|        - |  6287 | `	/* Total number of arguments passed to the enclosing function */` |
|      870 |  6288 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      870 |  6289 | `	ph7_result_int(pCtx,nArg);` |
|      870 |  6290 | `	return SXRET_OK;` |
|      436 |  6291 |  |
|        - |  6292 | `/*` |
|        - |  6293 | ` * value func_get_arg(int $arg_num)` |
|        - |  6294 | ` *   Return an item from the argument list.` |
|        - |  6295 | ` * Parameters` |
|        - |  6296 | ` *  Argument number(index start from zero).` |
|        - |  6297 | ` * Return` |
|        - |  6298 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6299 | ` */` |
|       22 |  6300 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6301 |  |
|       24 |  6302 | `	ph7_value *pObj = 0;` |
|       24 |  6303 | `	VmSlot *pSlot = 0;` |
|        - |  6304 | `	VmFrame *pFrame;` |
|        - |  6305 | `	ph7_vm *pVm;` |
|        - |  6306 | `	/* Point to the target VM */` |
|       24 |  6307 | `	pVm = pCtx->pVm;` |
|        - |  6308 | `	/* Current frame */` |
|       24 |  6309 | `	pFrame = pVm->pFrame;` |
|       24 |  6310 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6311 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6312 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6313 | `	}` |
|       24 |  6314 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6315 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6316 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6317 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6318 | `		return SXRET_OK;` |
|        - |  6319 | `	}` |
|        - |  6320 | `	/* Extract the desired index */` |
|       21 |  6321 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  6322 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6323 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6324 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6325 | `		return SXRET_OK;` |
|        - |  6326 | `	}` |
|        - |  6327 | `	/* Extract the desired argument */` |
|       21 |  6328 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  6329 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6330 | `			/* Return the desired argument */` |
|       21 |  6331 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  6332 | `		}else{` |
|        - |  6333 | `			/* No such argument,return false */` |
|      ! 0 |  6334 | `			ph7_result_bool(pCtx,0);` |
|        - |  6335 | `		}` |
|       11 |  6336 | `	}else{` |
|        - |  6337 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6338 | `		ph7_result_bool(pCtx,0);` |
|        - |  6339 | `	}` |
|       21 |  6340 | `	return SXRET_OK;` |
|       13 |  6341 |  |
|        - |  6342 | `/*` |
|        - |  6343 | ` * array func_get_args_byref(void)` |
|        - |  6344 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6345 | ` * Parameters` |
|        - |  6346 | ` *  None.` |
|        - |  6347 | ` * Return` |
|        - |  6348 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6349 | ` *  member of the current user-defined function's argument list.` |
|        - |  6350 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6351 | ` * NOTE:` |
|        - |  6352 | ` *  Arguments are returned to the array by reference.` |
|        - |  6353 | ` */` |
|        2 |  6354 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6355 |  |
|        - |  6356 | `	ph7_value *pArray;` |
|        - |  6357 | `	VmFrame *pFrame;` |
|        - |  6358 | `	VmSlot *aSlot;` |
|        - |  6359 | `	sxu32 n;` |
|        - |  6360 | `	/* Point to the current frame */` |
|        3 |  6361 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6362 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6363 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6364 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6365 | `	}` |
|        3 |  6366 | `	if( pFrame->pParent == 0 ){` |
|        - |  6367 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6368 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6369 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6370 | `		return SXRET_OK;` |
|        - |  6371 | `	}` |
|        - |  6372 | `	/* Create a new array */` |
|        3 |  6373 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6374 | `	if( pArray == 0 ){` |
|      ! 0 |  6375 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6376 | `		SXUNUSED(apArg);` |
|      ! 0 |  6377 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6378 | `		return SXRET_OK;` |
|        - |  6379 | `	}` |
|        - |  6380 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6381 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6382 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6383 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6384 | `	}` |
|        - |  6385 | `	/* Return the freshly created array */` |
|        3 |  6386 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6387 | `	return SXRET_OK;` |
|        2 |  6388 |  |
|        - |  6389 | `/*` |
|        - |  6390 | ` * array func_get_args(void)` |
|        - |  6391 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6392 | ` * Parameters` |
|        - |  6393 | ` *  None.` |
|        - |  6394 | ` * Return` |
|        - |  6395 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6396 | ` *  member of the current user-defined function's argument list.` |
|        - |  6397 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6398 | ` */` |
|       46 |  6399 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6400 |  |
|       47 |  6401 | `	ph7_value *pObj = 0;` |
|        - |  6402 | `	ph7_value *pArray;` |
|        - |  6403 | `	VmFrame *pFrame;` |
|        - |  6404 | `	VmSlot *aSlot;` |
|        - |  6405 | `	sxu32 n;` |
|        - |  6406 | `	/* Point to the current frame */` |
|       47 |  6407 | `	pFrame = pCtx->pVm->pFrame;` |
|       47 |  6408 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6409 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6410 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6411 | `	}` |
|       47 |  6412 | `	if( pFrame->pParent == 0 ){` |
|        - |  6413 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6414 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6415 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6416 | `		return SXRET_OK;` |
|        - |  6417 | `	}` |
|        - |  6418 | `	/* Create a new array */` |
|       47 |  6419 | `	pArray = ph7_context_new_array(pCtx);` |
|       47 |  6420 | `	if( pArray == 0 ){` |
|      ! 0 |  6421 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6422 | `		SXUNUSED(apArg);` |
|      ! 0 |  6423 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6424 | `		return SXRET_OK;` |
|        - |  6425 | `	}` |
|        - |  6426 | `	/* Start filling the array with the given arguments */` |
|       47 |  6427 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      143 |  6428 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|       97 |  6429 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|       97 |  6430 | `		if( pObj ){` |
|       97 |  6431 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       48 |  6432 | `		}` |
|       49 |  6433 | `	}` |
|        - |  6434 | `	/* Return the freshly created array */` |
|       47 |  6435 | `	ph7_result_value(pCtx,pArray);` |
|       47 |  6436 | `	return SXRET_OK;` |
|       24 |  6437 |  |
|        - |  6438 | `/*` |
|        - |  6439 | ` * bool function_exists(string $name)` |
|        - |  6440 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6441 | ` * Parameters` |
|        - |  6442 | ` *  The name of the desired function.` |
|        - |  6443 | ` * Return` |
|        - |  6444 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6445 | ` */` |
|     1666 |  6446 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6447 |  |
|        - |  6448 | `	const char *zName;` |
|        - |  6449 | `	ph7_vm *pVm;` |
|        - |  6450 | `	int nLen;` |
|        - |  6451 | `	int res;` |
|     1668 |  6452 | `	if( nArg < 1 ){` |
|        - |  6453 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6454 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6455 | `		return SXRET_OK;` |
|        - |  6456 | `	}` |
|        - |  6457 | `	/* Point to the target VM */` |
|     1668 |  6458 | `	pVm = pCtx->pVm;` |
|        - |  6459 | `	/* Extract the function name */` |
|     1668 |  6460 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6461 | `	/* Assume the function is not defined */` |
|     1668 |  6462 | `	res = 0;` |
|        - |  6463 | `	/* Perform the lookup */` |
|     2499 |  6464 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1662 |  6465 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6466 | `			/* Function is defined */` |
|      212 |  6467 | `			res = 1;` |
|      105 |  6468 | `	}` |
|     1668 |  6469 | `	ph7_result_bool(pCtx,res);` |
|     1668 |  6470 | `	return SXRET_OK;` |
|      835 |  6471 |  |
|        - |  6472 | `/*` |
|        - |  6473 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6474 | ` * [i.e: Whether it is callable or not].` |
|        - |  6475 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6476 | ` */` |
|    15836 |  6477 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6478 |  |
|    15838 |  6479 | `	int res = 0;` |
|    15838 |  6480 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6481 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6482 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6483 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6484 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6485 | `		if( pMethod && CallInvoke ){` |
|        - |  6486 | `			ph7_value sResult;` |
|        - |  6487 | `			sxi32 rc;` |
|        - |  6488 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6489 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6490 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6491 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6492 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6493 | `			}` |
|      ! 0 |  6494 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6495 | `		}` |
|    15838 |  6496 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  6497 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       20 |  6498 | `		if( pMap->nEntry == 2 ){` |
|        - |  6499 | `			ph7_class *pClass;` |
|        - |  6500 | `			ph7_value *pV;` |
|        - |  6501 | `			/* Extract the target class */` |
|        7 |  6502 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|        7 |  6503 | `			if( pV ){` |
|        7 |  6504 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|        7 |  6505 | `				if( pClass ){` |
|        - |  6506 | `					ph7_class_method *pMethod;` |
|        - |  6507 | `					/* Extract the target method */` |
|        7 |  6508 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|        7 |  6509 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6510 | `						/* Perform the lookup */` |
|        7 |  6511 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|        7 |  6512 | `						if( pMethod ){` |
|        - |  6513 | `							/* Method is callable */` |
|        5 |  6514 | `							res = 1;` |
|        2 |  6515 | `						}` |
|        3 |  6516 | `					}` |
|        3 |  6517 | `				}` |
|        3 |  6518 | `			}` |
|        5 |  6519 | `		}` |
|    15829 |  6520 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6521 | `		const char *zName;` |
|        - |  6522 | `		int nLen;` |
|        - |  6523 | `		/* Extract the name */` |
|     4658 |  6524 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6525 | `		/* Perform the lookup */` |
|     4671 |  6526 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       26 |  6527 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6528 | `				/* Function is callable */` |
|     4644 |  6529 | `				res = 1;` |
|     2321 |  6530 | `		}` |
|     2328 |  6531 | `	}` |
|    15838 |  6532 | `	return res;` |
|        2 |  6533 |  |
|        - |  6534 | `/*` |
|        - |  6535 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6536 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6537 | ` * Parameters` |
|        - |  6538 | ` * $name` |
|        - |  6539 | ` *    The callback function to check` |
|        - |  6540 | ` * $syntax_only` |
|        - |  6541 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6542 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6543 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6544 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6545 | ` *    a string.` |
|        - |  6546 | ` * Return` |
|        - |  6547 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6548 | ` */` |
|       14 |  6549 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6550 |  |
|        - |  6551 | `	ph7_vm *pVm;` |
|        - |  6552 | `	int res;` |
|       15 |  6553 | `	if( nArg < 1 ){` |
|        - |  6554 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6555 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6556 | `		return SXRET_OK;` |
|        - |  6557 | `	}` |
|        - |  6558 | `	/* Point to the target VM */` |
|       15 |  6559 | `	pVm = pCtx->pVm;` |
|        - |  6560 | `	/* Perform the requested operation */` |
|       15 |  6561 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6562 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6563 | `	return SXRET_OK;` |
|        8 |  6564 |  |
|        - |  6565 | `/*` |
|        - |  6566 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6567 | ` * defined below.` |
|        - |  6568 | ` */` |
|     1074 |  6569 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6570 |  |
|     1075 |  6571 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6572 | `	ph7_value sName;` |
|        - |  6573 | `	sxi32 rc;` |
|        - |  6574 | `	/* Prepare the function name for insertion */` |
|     1075 |  6575 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1075 |  6576 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6577 | `	/* Perform the insertion */` |
|     1075 |  6578 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1075 |  6579 | `	PH7_MemObjRelease(&sName);` |
|     1075 |  6580 | `	return rc;` |
|        1 |  6581 |  |
|        - |  6582 | `/*` |
|        - |  6583 | ` * array get_defined_functions(void)` |
|        - |  6584 | ` *  Returns an array of all defined functions.` |
|        - |  6585 | ` * Parameter` |
|        - |  6586 | ` *  None.` |
|        - |  6587 | ` * Return` |
|        - |  6588 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6589 | ` *  both built-in (internal) and user-defined.` |
|        - |  6590 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6591 | ` *  defined ones using $arr["user"].` |
|        - |  6592 | ` * Note:` |
|        - |  6593 | ` *  NULL is returned on failure.` |
|        - |  6594 | ` */` |
|        2 |  6595 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6596 |  |
|        - |  6597 | `	ph7_value *pArray,*pEntry;` |
|        - |  6598 | `	/* NOTE:` |
|        - |  6599 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6600 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6601 | `	 */` |
|        3 |  6602 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6603 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6604 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6605 | `		SXUNUSED(apArg);` |
|        - |  6606 | `		/* Return NULL */` |
|      ! 0 |  6607 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6608 | `		return SXRET_OK;` |
|        - |  6609 | `	}` |
|        3 |  6610 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6611 | `	if( pEntry == 0 ){` |
|        - |  6612 | `		/* Return NULL */` |
|      ! 0 |  6613 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6614 | `		return SXRET_OK;` |
|        - |  6615 | `	}` |
|        - |  6616 | `	/* Fill with the appropriate information */` |
|        3 |  6617 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6618 | `	/* Create the 'internal' index */` |
|        3 |  6619 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6620 | `	/* Create the user-func array */` |
|        3 |  6621 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6622 | `	if( pEntry == 0 ){` |
|        - |  6623 | `		/* Return NULL */` |
|      ! 0 |  6624 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6625 | `		return SXRET_OK;` |
|        - |  6626 | `	}` |
|        - |  6627 | `	/* Fill with the appropriate information */` |
|        3 |  6628 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6629 | `	/* Create the 'user' index */` |
|        3 |  6630 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6631 | `	/* Return the multi-dimensional array */` |
|        3 |  6632 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6633 | `	return SXRET_OK;` |
|        2 |  6634 |  |
|        - |  6635 | `/*` |
|        - |  6636 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6637 | ` *  Register a function for execution on shutdown.` |
|        - |  6638 | ` * Note` |
|        - |  6639 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6640 | ` *  be called in the same order as they were registered.` |
|        - |  6641 | ` * Parameters` |
|        - |  6642 | ` *  $callback` |
|        - |  6643 | ` *   The shutdown callback to register.` |
|        - |  6644 | ` * $param` |
|        - |  6645 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6646 | ` * Return` |
|        - |  6647 | ` *  Nothing.` |
|        - |  6648 | ` */` |
|        2 |  6649 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6650 |  |
|        - |  6651 | `	VmShutdownCB sEntry;` |
|        - |  6652 | `	int i,j;` |
|        3 |  6653 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6654 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6655 | `		return PH7_OK;` |
|        - |  6656 | `	}` |
|        - |  6657 | `	/* Zero the Entry */` |
|        3 |  6658 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6659 | `	/* Initialize fields */` |
|        3 |  6660 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6661 | `	/* Save the callback name for later invocation name */` |
|        3 |  6662 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6663 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6664 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6665 | `	}` |
|        - |  6666 | `	/* Copy arguments */` |
|        3 |  6667 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6668 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6669 | `			/* Limit reached */` |
|      ! 0 |  6670 | `			break;` |
|        - |  6671 | `		}` |
|      ! 0 |  6672 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6673 | `	}` |
|        3 |  6674 | `	sEntry.nArg = j;` |
|        - |  6675 | `	/* Install the callback */` |
|        3 |  6676 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6677 | `	return PH7_OK;` |
|        2 |  6678 |  |
|        - |  6679 | `/*` |
|        - |  6680 | ` * Section:` |
|        - |  6681 | ` *  Class handling functions.` |
|        - |  6682 | ` * Status:` |
|        - |  6683 | ` *    Stable.` |
|        - |  6684 | ` */` |
|        - |  6685 | `/*` |
|        - |  6686 | ` * Extract the top active class. NULL is returned` |
|        - |  6687 | ` * if the class stack is empty.` |
|        - |  6688 | ` */` |
|      400 |  6689 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6690 |  |
|      402 |  6691 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6692 | `	ph7_class **apClass;` |
|      402 |  6693 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6694 | `		/* Empty stack,return NULL */` |
|       15 |  6695 | `		return 0;` |
|        - |  6696 | `	}` |
|        - |  6697 | `	/* Peek the last entry */` |
|      388 |  6698 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      388 |  6699 | `	return apClass[pSet->nUsed - 1];` |
|      202 |  6700 |  |
|        - |  6701 | `/*` |
|        - |  6702 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  6703 | ` *   Get the class that declared the currently executing method.` |
|        - |  6704 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  6705 | ` *` |
|        - |  6706 | ` * Parameters` |
|        - |  6707 | ` *   pVm: Target VM` |
|        - |  6708 | ` *` |
|        - |  6709 | ` * Return` |
|        - |  6710 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  6711 | ` *   - Not executing within a class method` |
|        - |  6712 | ` *` |
|        - |  6713 | ` * Note` |
|        - |  6714 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  6715 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  6716 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  6717 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  6718 | ` *   declaring class.` |
|        - |  6719 | ` */` |
|       18 |  6720 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        1 |  6721 |  |
|       19 |  6722 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  6723 | `	ph7_vm_func *pVmFunc;` |
|        - |  6724 |  |
|        - |  6725 | `	/* Skip exception frames to find the actual method frame */` |
|       19 |  6726 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  6727 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6728 | `	}` |
|        - |  6729 |  |
|        - |  6730 | `	/* Check if we're in a method context */` |
|       19 |  6731 | `	if( pFrame->pParent ){` |
|       15 |  6732 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       15 |  6733 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  6734 | `			/* Return the declaring class */` |
|       15 |  6735 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  6736 | `		}` |
|      ! 0 |  6737 | `	}` |
|        - |  6738 |  |
|        5 |  6739 | `	return 0;` |
|       10 |  6740 |  |
|        - |  6741 |  |
|        - |  6742 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  6743 | `/*` |
|        - |  6744 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  6745 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  6746 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  6747 | ` * return value indicates failure.` |
|        - |  6748 | ` */` |
|      918 |  6749 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  6750 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  6751 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  6752 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  6753 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  6754 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  6755 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  6756 | `	)` |
|        2 |  6757 |  |
|        - |  6758 | `	ph7_value *aStack;` |
|        - |  6759 | `	VmInstr aInstr[2];` |
|        - |  6760 | `	int iCursor;` |
|        - |  6761 | `	int i;` |
|        - |  6762 | `	/* Create a new operand stack */` |
|      920 |  6763 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|      920 |  6764 | `	if( aStack == 0 ){` |
|      ! 0 |  6765 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6766 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  6767 | `		return SXERR_MEM;` |
|        - |  6768 | `	}` |
|        - |  6769 | `	/* Fill the operand stack with the given arguments */` |
|     1350 |  6770 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      432 |  6771 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  6772 | `		/*` |
|        - |  6773 | `		 * Symisc eXtension:` |
|        - |  6774 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  6775 | `		 */` |
|      432 |  6776 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      217 |  6777 | `	}` |
|      920 |  6778 | `	iCursor = nArg + 1;` |
|      920 |  6779 | `	if( pThis ){` |
|        - |  6780 | `		/*` |
|        - |  6781 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  6782 | `		 */` |
|      914 |  6783 | `		pThis->iRef++; /* Increment reference count */` |
|      914 |  6784 | `		aStack[i].x.pOther = pThis;` |
|      914 |  6785 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      456 |  6786 | `	}` |
|      920 |  6787 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|      920 |  6788 | `	i++;` |
|        - |  6789 | `	/* Push method name */` |
|      920 |  6790 | `	SyBlobReset(&aStack[i].sBlob);` |
|      920 |  6791 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|      920 |  6792 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|      920 |  6793 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  6794 | `	/* Emit the CALL istruction */` |
|      920 |  6795 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      920 |  6796 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      920 |  6797 | `	aInstr[0].iP2 = 0;` |
|      920 |  6798 | `	aInstr[0].p3  = 0;` |
|        - |  6799 | `	/* Emit the DONE instruction */` |
|      920 |  6800 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      920 |  6801 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|      920 |  6802 | `	aInstr[1].iP2 = 0;` |
|      920 |  6803 | `	aInstr[1].p3  = 0;` |
|        - |  6804 | `	/* Execute the method body (if available) */` |
|      920 |  6805 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  6806 | `	/* Clean up the mess left behind */` |
|      920 |  6807 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      920 |  6808 | `	return PH7_OK;` |
|      461 |  6809 |  |
|        - |  6810 | `/*` |
|        - |  6811 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  6812 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  6813 | ` * in the apArg[] array.` |
|        - |  6814 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  6815 | ` * return value indicates failure.` |
|        - |  6816 | ` */` |
|      800 |  6817 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  6818 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  6819 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  6820 | `	int nArg,          /* Total number of given arguments */` |
|        - |  6821 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  6822 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  6823 | `	)` |
|        2 |  6824 |  |
|        - |  6825 | `	ph7_value *aStack;` |
|        - |  6826 | `	VmInstr aInstr[2];` |
|        - |  6827 | `	int i;` |
|      802 |  6828 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6829 | `		/* Don't bother processing,it's invalid anyway */` |
|      359 |  6830 | `		if( pResult ){` |
|        - |  6831 | `			/* Assume a null return value */` |
|      ! 0 |  6832 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  6833 | `		}` |
|      359 |  6834 | `		return SXERR_INVALID;` |
|        - |  6835 | `	}` |
|      444 |  6836 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6837 | `		/* Class method */` |
|       11 |  6838 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  6839 | `		ph7_class_method *pMethod = 0;` |
|       11 |  6840 | `		ph7_class_instance *pThis = 0;` |
|       11 |  6841 | `		ph7_class *pClass = 0;` |
|        - |  6842 | `		ph7_value *pValue;` |
|        - |  6843 | `		sxi32 rc;` |
|       11 |  6844 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  6845 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  6846 | `			if( pResult ){` |
|        - |  6847 | `				/* Assume a null return value */` |
|      ! 0 |  6848 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  6849 | `			}` |
|      ! 0 |  6850 | `			return SXRET_OK;` |
|        - |  6851 | `		}` |
|        - |  6852 | `		/* Extract the class name or an instance of it */` |
|       11 |  6853 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  6854 | `		if( pValue ){` |
|       11 |  6855 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  6856 | `		}` |
|       11 |  6857 | `		if( pClass == 0 ){` |
|        - |  6858 | `			/* No such class,return NULL */` |
|      ! 0 |  6859 | `			if( pResult ){` |
|      ! 0 |  6860 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  6861 | `			}` |
|      ! 0 |  6862 | `			return SXRET_OK;` |
|        - |  6863 | `		}` |
|       11 |  6864 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6865 | `			/* Point to the class instance */` |
|        5 |  6866 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  6867 | `		}` |
|        - |  6868 | `		/* Try to extract the method */` |
|       11 |  6869 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  6870 | `		if( pValue ){` |
|       11 |  6871 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  6872 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  6873 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  6874 | `			}` |
|        5 |  6875 | `		}` |
|       11 |  6876 | `		if( pMethod == 0 ){` |
|        - |  6877 | `			/* No such method,return NULL */` |
|      ! 0 |  6878 | `			if( pResult ){` |
|      ! 0 |  6879 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  6880 | `			}` |
|      ! 0 |  6881 | `			return SXRET_OK;` |
|        - |  6882 | `		}` |
|        - |  6883 | `		/* Call the class method */` |
|       11 |  6884 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  6885 | `		return rc;` |
|        - |  6886 | `	}` |
|        - |  6887 | `	/* Create a new operand stack */` |
|      434 |  6888 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      434 |  6889 | `	if( aStack == 0 ){` |
|      ! 0 |  6890 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6891 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  6892 | `		if( pResult ){` |
|        - |  6893 | `			/* Assume a null return value */` |
|      ! 0 |  6894 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  6895 | `		}` |
|      ! 0 |  6896 | `		return SXERR_MEM;` |
|        - |  6897 | `	}` |
|        - |  6898 | `	/* Fill the operand stack with the given arguments */` |
|     1428 |  6899 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      996 |  6900 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  6901 | `		/*` |
|        - |  6902 | `		 * Symisc eXtension:` |
|        - |  6903 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  6904 | `		 */` |
|      996 |  6905 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      499 |  6906 | `	}` |
|        - |  6907 | `	/* Push the function name */` |
|      434 |  6908 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      434 |  6909 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  6910 | `	/* Emit the CALL istruction */` |
|      434 |  6911 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      434 |  6912 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      434 |  6913 | `	aInstr[0].iP2 = 0;` |
|      434 |  6914 | `	aInstr[0].p3  = 0;` |
|        - |  6915 | `	/* Emit the DONE instruction */` |
|      434 |  6916 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      434 |  6917 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      434 |  6918 | `	aInstr[1].iP2 = 0;` |
|      434 |  6919 | `	aInstr[1].p3  = 0;` |
|        - |  6920 | `	/* Execute the function body (if available) */` |
|      434 |  6921 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  6922 | `	/* Clean up the mess left behind */` |
|      434 |  6923 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      434 |  6924 | `	return PH7_OK;` |
|      402 |  6925 |  |
|        - |  6926 | `/*` |
|        - |  6927 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  6928 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  6929 | ` * parameter.` |
|        - |  6930 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  6931 | ` * return value indicates failure.` |
|        - |  6932 | ` */` |
|      236 |  6933 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  6934 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  6935 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  6936 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  6937 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  6938 | `	)` |
|        1 |  6939 |  |
|        - |  6940 | `	ph7_value *pArg;` |
|        - |  6941 | `	SySet aArg;` |
|        - |  6942 | `	va_list ap;` |
|        - |  6943 | `	sxi32 rc;` |
|      237 |  6944 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  6945 | `	/* Copy arguments one after one */` |
|      237 |  6946 | `	va_start(ap,pResult);` |
|      393 |  6947 | `	for(;;){` |
|      787 |  6948 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  6949 | `		if( pArg == 0 ){` |
|      237 |  6950 | `			break;` |
|        - |  6951 | `		}` |
|      551 |  6952 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  6953 | `	}` |
|        - |  6954 | `	/* Call the core routine */` |
|      237 |  6955 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  6956 | `	/* Cleanup */` |
|      237 |  6957 | `	SySetRelease(&aArg);` |
|      237 |  6958 | `	return rc;` |
|        1 |  6959 |  |
|        - |  6960 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  6961 | `/*` |
|        - |  6962 | ` * bool defined(string $name)` |
|        - |  6963 | ` *  Checks whether a given named constant exists.` |
|        - |  6964 | ` * Parameter:` |
|        - |  6965 | ` *  Name of the desired constant.` |
|        - |  6966 | ` * Return` |
|        - |  6967 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  6968 | ` */` |
|       14 |  6969 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6970 |  |
|        - |  6971 | `	const char *zName;` |
|       16 |  6972 | `	int nLen = 0;` |
|       16 |  6973 | `	int res = 0;` |
|       16 |  6974 | `	if( nArg < 1 ){` |
|        - |  6975 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  6976 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  6977 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6978 | `		return SXRET_OK;` |
|        - |  6979 | `	}` |
|        - |  6980 | `	/* Extract constant name */` |
|       16 |  6981 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6982 | `	/* Perform the lookup */` |
|       16 |  6983 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6984 | `		/* Already defined */` |
|       10 |  6985 | `		res = 1;` |
|        4 |  6986 | `	}` |
|       16 |  6987 | `	ph7_result_bool(pCtx,res);` |
|       16 |  6988 | `	return SXRET_OK;` |
|        9 |  6989 |  |
|        - |  6990 | `/*` |
|        - |  6991 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  6992 | ` * below.` |
|        - |  6993 | ` */` |
|        8 |  6994 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  6995 |  |
|       10 |  6996 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  6997 | `	/* Expand constant value */` |
|       10 |  6998 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  6999 |  |
|        - |  7000 | `/*` |
|        - |  7001 | ` * bool define(string $constant_name,expression value)` |
|        - |  7002 | ` *  Defines a named constant at runtime.` |
|        - |  7003 | ` * Parameter:` |
|        - |  7004 | ` *  $constant_name` |
|        - |  7005 | ` *   The name of the constant` |
|        - |  7006 | ` *  $value` |
|        - |  7007 | ` *   Constant value` |
|        - |  7008 | ` * Return:` |
|        - |  7009 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7010 | ` */` |
|       10 |  7011 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7012 |  |
|        - |  7013 | `	const char *zName;  /* Constant name */` |
|        - |  7014 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7015 | `	int nLen = 0;       /* Name length */` |
|        - |  7016 | `	sxi32 rc;` |
|       12 |  7017 | `	if( nArg < 2 ){` |
|        - |  7018 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7019 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7020 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7021 | `		return SXRET_OK;` |
|        - |  7022 | `	}` |
|       12 |  7023 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7024 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7025 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7026 | `		return SXRET_OK;` |
|        - |  7027 | `	}` |
|        - |  7028 | `	/* Extract constant name */` |
|       12 |  7029 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7030 | `	if( nLen < 1 ){` |
|      ! 0 |  7031 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7032 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7033 | `		return SXRET_OK;` |
|        - |  7034 | `	}` |
|        - |  7035 | `	/* Duplicate constant value */` |
|       12 |  7036 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7037 | `	if( pValue == 0 ){` |
|      ! 0 |  7038 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7039 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7040 | `		return SXRET_OK;` |
|        - |  7041 | `	}` |
|        - |  7042 | `	/* Initialize the memory object */` |
|       12 |  7043 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7044 | `	/* Register the constant */` |
|       12 |  7045 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7046 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7047 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7048 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7049 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7050 | `		return SXRET_OK;` |
|        - |  7051 | `	}` |
|        - |  7052 | `	/* Duplicate constant value */` |
|       12 |  7053 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7054 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7055 | `		/* Lower case the constant name */` |
|      ! 0 |  7056 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7057 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7058 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7059 | `				/* UTF-8 stream */` |
|      ! 0 |  7060 | `				zCur++;` |
|      ! 0 |  7061 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  7062 | `					zCur++;` |
|      ! 0 |  7063 | `				}` |
|      ! 0 |  7064 | `				continue;` |
|        - |  7065 | `			}` |
|      ! 0 |  7066 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  7067 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  7068 | `				zCur[0] = (char)c;` |
|      ! 0 |  7069 | `			}` |
|      ! 0 |  7070 | `			zCur++;` |
|      ! 0 |  7071 | `		}` |
|        - |  7072 | `		/* Finally,register the constant */` |
|      ! 0 |  7073 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  7074 | `	}` |
|        - |  7075 | `	/* All done,return TRUE */` |
|       12 |  7076 | `	ph7_result_bool(pCtx,1);` |
|       12 |  7077 | `	return SXRET_OK;` |
|        7 |  7078 |  |
|        - |  7079 | `/*` |
|        - |  7080 | ` * value constant(string $name)` |
|        - |  7081 | ` *  Returns the value of a constant` |
|        - |  7082 | ` * Parameter` |
|        - |  7083 | ` *  $name` |
|        - |  7084 | ` *    Name of the constant.` |
|        - |  7085 | ` * Return` |
|        - |  7086 | ` *  Constant value or NULL if not defined.` |
|        - |  7087 | ` */` |
|        8 |  7088 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7089 |  |
|        - |  7090 | `	SyHashEntry *pEntry;` |
|        - |  7091 | `	ph7_constant *pCons;` |
|        - |  7092 | `	const char *zName; /* Constant name */` |
|        - |  7093 | `	ph7_value sVal;    /* Constant value */` |
|        - |  7094 | `	int nLen;` |
|       10 |  7095 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  7096 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  7097 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  7098 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7099 | `		return SXRET_OK;` |
|        - |  7100 | `	}` |
|        - |  7101 | `	/* Extract the constant name */` |
|       10 |  7102 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7103 | `	/* Perform the query */` |
|       10 |  7104 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  7105 | `	if( pEntry == 0 ){` |
|        3 |  7106 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  7107 | `		ph7_result_null(pCtx);` |
|        3 |  7108 | `		return SXRET_OK;` |
|        - |  7109 | `	}` |
|        8 |  7110 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  7111 | `	/* Point to the structure that describe the constant */` |
|        8 |  7112 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  7113 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  7114 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  7115 | `	/* Return that value */` |
|        8 |  7116 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  7117 | `	/* Cleanup */` |
|        8 |  7118 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  7119 | `	return SXRET_OK;` |
|        6 |  7120 |  |
|        - |  7121 | `/*` |
|        - |  7122 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  7123 | ` * defined below.` |
|        - |  7124 | ` */` |
|      414 |  7125 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7126 |  |
|      415 |  7127 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  7128 | `	ph7_value sName;` |
|        - |  7129 | `	sxi32 rc;` |
|        - |  7130 | `	/* Prepare the constant name for insertion */` |
|      415 |  7131 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      415 |  7132 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  7133 | `	/* Perform the insertion */` |
|      415 |  7134 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      415 |  7135 | `	PH7_MemObjRelease(&sName);` |
|      415 |  7136 | `	return rc;` |
|        1 |  7137 |  |
|        - |  7138 | `/*` |
|        - |  7139 | ` * array get_defined_constants(void)` |
|        - |  7140 | ` *  Returns an associative array with the names of all defined` |
|        - |  7141 | ` *  constants.` |
|        - |  7142 | ` * Parameters` |
|        - |  7143 | ` *  NONE.` |
|        - |  7144 | ` * Returns` |
|        - |  7145 | ` *  Returns the names of all the constants currently defined.` |
|        - |  7146 | ` */` |
|        2 |  7147 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7148 |  |
|        - |  7149 | `	ph7_value *pArray;` |
|        - |  7150 | `	/* Create the array first*/` |
|        3 |  7151 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7152 | `	if( pArray == 0 ){` |
|      ! 0 |  7153 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7154 | `		SXUNUSED(apArg);` |
|        - |  7155 | `		/* Return NULL */` |
|      ! 0 |  7156 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7157 | `		return SXRET_OK;` |
|        - |  7158 | `	}` |
|        - |  7159 | `	/* Fill the array with the defined constants */` |
|        3 |  7160 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  7161 | `	/* Return the created array */` |
|        3 |  7162 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7163 | `	return SXRET_OK;` |
|        2 |  7164 |  |
|        - |  7165 | `/*` |
|        - |  7166 | ` * Section:` |
|        - |  7167 | ` *  Output Control (OB) functions.` |
|        - |  7168 | ` * Status:` |
|        - |  7169 | ` *    Stable.` |
|        - |  7170 | ` */` |
|        - |  7171 | `/* Forward declaration */` |
|        - |  7172 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry);` |
|        - |  7173 | `/*` |
|        - |  7174 | ` * void ob_clean(void)` |
|        - |  7175 | ` *  This function discards the contents of the output buffer.` |
|        - |  7176 | ` *  This function does not destroy the output buffer like ob_end_clean() does.` |
|        - |  7177 | ` * Parameter` |
|        - |  7178 | ` *  None` |
|        - |  7179 | ` * Return` |
|        - |  7180 | ` *  No value is returned.` |
|        - |  7181 | ` */` |
|        2 |  7182 | `static int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7183 |  |
|        3 |  7184 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7185 | `	VmObEntry *pOb;` |
|        1 |  7186 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  7187 | `	SXUNUSED(apArg);` |
|        - |  7188 | `	/* Peek the top most OB */` |
|        3 |  7189 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  7190 | `	if( pOb ){` |
|        3 |  7191 | `		SyBlobRelease(&pOb->sOB);` |
|        1 |  7192 | `	}` |
|        3 |  7193 | `	return PH7_OK;` |
|        1 |  7194 |  |
|        - |  7195 | `/*` |
|        - |  7196 | ` * bool ob_end_clean(void)` |
|        - |  7197 | ` *  Clean (erase) the output buffer and turn off output buffering` |
|        - |  7198 | ` *  This function discards the contents of the topmost output buffer and turns` |
|        - |  7199 | ` *  off this output buffering. If you want to further process the buffer's contents` |
|        - |  7200 | ` *  you have to call ob_get_contents() before ob_end_clean() as the buffer contents` |
|        - |  7201 | ` *  are discarded when ob_end_clean() is called.` |
|        - |  7202 | ` * Parameter` |
|        - |  7203 | ` *  None` |
|        - |  7204 | ` * Return` |
|        - |  7205 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first that you called` |
|        - |  7206 | ` *  the function without an active buffer or that for some reason a buffer could not be deleted` |
|        - |  7207 | ` * (possible for special buffer)` |
|        - |  7208 | ` */` |
|     3140 |  7209 | `static int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7210 |  |
|     3142 |  7211 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7212 | `	VmObEntry *pOb;` |
|        - |  7213 | `	/* Pop the top most OB */` |
|     3142 |  7214 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     3142 |  7215 | `	if( pOb == 0){` |
|        - |  7216 | `		/* No such OB,return FALSE */` |
|      ! 0 |  7217 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7218 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7219 | `		SXUNUSED(apArg);` |
|      ! 0 |  7220 | `	}else{` |
|        - |  7221 | `		/* Release */` |
|     3142 |  7222 | `		VmObRestore(pVm,pOb);` |
|        - |  7223 | `		/* Return true */` |
|     3142 |  7224 | `		ph7_result_bool(pCtx,1);` |
|        - |  7225 | `	}` |
|     3142 |  7226 | `	return PH7_OK;` |
|        2 |  7227 |  |
|        - |  7228 | `/*` |
|        - |  7229 | ` * string ob_get_contents(void)` |
|        - |  7230 | ` *  Gets the contents of the output buffer without clearing it.` |
|        - |  7231 | ` * Parameter` |
|        - |  7232 | ` *  None` |
|        - |  7233 | ` * Return` |
|        - |  7234 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|        - |  7235 | ` */` |
|        6 |  7236 | `static int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7237 |  |
|        7 |  7238 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7239 | `	VmObEntry *pOb;` |
|        - |  7240 | `	/* Peek the top most OB */` |
|        7 |  7241 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        7 |  7242 | `	if( pOb == 0 ){` |
|        - |  7243 | `		/* No active OB,return FALSE */` |
|      ! 0 |  7244 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7245 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7246 | `		SXUNUSED(apArg);` |
|      ! 0 |  7247 | `	}else{` |
|        - |  7248 | `		/* Return contents */` |
|        7 |  7249 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB));` |
|        - |  7250 | `	}` |
|        7 |  7251 | `	return PH7_OK;` |
|        1 |  7252 |  |
|        - |  7253 | `/*` |
|        - |  7254 | ` * string ob_get_clean(void)` |
|        - |  7255 | ` * string ob_get_flush(void)` |
|        - |  7256 | ` *  Get current buffer contents and delete current output buffer.` |
|        - |  7257 | ` * Parameter` |
|        - |  7258 | ` *  None` |
|        - |  7259 | ` * Return` |
|        - |  7260 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|        - |  7261 | ` */` |
|     4346 |  7262 | `static int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7263 |  |
|     4348 |  7264 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7265 | `	VmObEntry *pOb;` |
|        - |  7266 | `	/* Pop the top most OB */` |
|     4348 |  7267 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     4348 |  7268 | `	if( pOb == 0 ){` |
|        - |  7269 | `		/* No active OB,return FALSE */` |
|      ! 0 |  7270 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7271 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7272 | `		SXUNUSED(apArg);` |
|      ! 0 |  7273 | `	}else{` |
|        - |  7274 | `		/* Return contents */` |
|     4348 |  7275 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB)); /* Will make it's own copy */` |
|        - |  7276 | `		/* Release */` |
|     4348 |  7277 | `		VmObRestore(pVm,pOb);` |
|        - |  7278 | `	}` |
|     4348 |  7279 | `	return PH7_OK;` |
|        2 |  7280 |  |
|        - |  7281 | `/*` |
|        - |  7282 | ` * int ob_get_length(void)` |
|        - |  7283 | ` *  Return the length of the output buffer.` |
|        - |  7284 | ` * Parameter` |
|        - |  7285 | ` *  None` |
|        - |  7286 | ` * Return` |
|        - |  7287 | ` *  Returns the length of the output buffer contents or FALSE if no buffering is active.` |
|        - |  7288 | ` */` |
|        2 |  7289 | `static int vm_builtin_ob_get_length(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7290 |  |
|        3 |  7291 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7292 | `	VmObEntry *pOb;` |
|        - |  7293 | `	/* Peek the top most OB */` |
|        3 |  7294 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  7295 | `	if( pOb == 0 ){` |
|        - |  7296 | `		/* No active OB,return FALSE */` |
|      ! 0 |  7297 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7298 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7299 | `		SXUNUSED(apArg);` |
|      ! 0 |  7300 | `	}else{` |
|        - |  7301 | `		/* Return OB length */` |
|        3 |  7302 | `		ph7_result_int64(pCtx,(ph7_int64)SyBlobLength(&pOb->sOB));` |
|        - |  7303 | `	}` |
|        3 |  7304 | `	return PH7_OK;` |
|        1 |  7305 |  |
|        - |  7306 | `/*` |
|        - |  7307 | ` * int ob_get_level(void)` |
|        - |  7308 | ` *  Returns the nesting level of the output buffering mechanism.` |
|        - |  7309 | ` * Parameter` |
|        - |  7310 | ` *  None` |
|        - |  7311 | ` * Return` |
|        - |  7312 | ` *  Returns the level of nested output buffering handlers or zero if output buffering is not active.` |
|        - |  7313 | ` */` |
|        6 |  7314 | `static int vm_builtin_ob_get_level(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7315 |  |
|        7 |  7316 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7317 | `	int iNest;` |
|        3 |  7318 | `	SXUNUSED(nArg); /* cc warning */` |
|        3 |  7319 | `	SXUNUSED(apArg);` |
|        - |  7320 | `	/* Nesting level */` |
|        7 |  7321 | `	iNest = (int)SySetUsed(&pVm->aOB);` |
|        - |  7322 | `	/* Return the nesting value */` |
|        7 |  7323 | `	ph7_result_int(pCtx,iNest);` |
|        7 |  7324 | `	return PH7_OK;` |
|        1 |  7325 |  |
|        - |  7326 | `/*` |
|        - |  7327 | ` * Output Buffer(OB) default VM consumer routine.All VM output is now redirected` |
|        - |  7328 | ` * to a stackable internal buffer,until the user call [ob_get_clean(),ob_end_clean(),...].` |
|        - |  7329 | ` * Refer to the implementation of [ob_start()] for more information.` |
|        - |  7330 | ` */` |
|     6690 |  7331 | `static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData)` |
|        2 |  7332 |  |
|     6692 |  7333 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|        - |  7334 | `	VmObEntry *pEntry;` |
|        - |  7335 | `	ph7_value sResult;` |
|        - |  7336 | `	/* Peek the top most entry */` |
|     6692 |  7337 | `	pEntry = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|     6692 |  7338 | `	if( pEntry == 0 ){` |
|        - |  7339 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  7340 | `		return PH7_OK;` |
|        - |  7341 | `	}` |
|     6692 |  7342 | `	PH7_MemObjInit(pVm,&sResult);` |
|     6692 |  7343 | `	if( ph7_value_is_callable(&pEntry->sCallback) && pVm->nObDepth < 15 ){` |
|        - |  7344 | `		ph7_value sArg,*apArg[2];` |
|        - |  7345 | `		/* Fill the first argument */` |
|      ! 0 |  7346 | `		PH7_MemObjInitFromString(pVm,&sArg,0);` |
|      ! 0 |  7347 | `		PH7_MemObjStringAppend(&sArg,(const char *)pData,nDataLen);` |
|      ! 0 |  7348 | `		apArg[0] = &sArg;` |
|        - |  7349 | `		/* Call the 'filter' callback */` |
|      ! 0 |  7350 | `		pVm->nObDepth++;` |
|      ! 0 |  7351 | `		PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult);` |
|      ! 0 |  7352 | `		pVm->nObDepth--;` |
|      ! 0 |  7353 | `		if( sResult.iFlags & MEMOBJ_STRING ){` |
|        - |  7354 | `			/* Extract the function result */` |
|      ! 0 |  7355 | `			pData = SyBlobData(&sResult.sBlob);` |
|      ! 0 |  7356 | `			nDataLen = SyBlobLength(&sResult.sBlob);` |
|      ! 0 |  7357 | `		}` |
|      ! 0 |  7358 | `		PH7_MemObjRelease(&sArg);` |
|      ! 0 |  7359 | `	}` |
|     6692 |  7360 | `	if( nDataLen > 0 ){` |
|        - |  7361 | `		/* Redirect the VM output to the internal buffer */` |
|     6692 |  7362 | `		SyBlobAppend(&pEntry->sOB,pData,nDataLen);` |
|     3345 |  7363 | `	}` |
|        - |  7364 | `	/* Release */` |
|     6692 |  7365 | `	PH7_MemObjRelease(&sResult);` |
|     6692 |  7366 | `	return PH7_OK;` |
|     3347 |  7367 |  |
|        - |  7368 | `/*` |
|        - |  7369 | ` * Restore the default consumer.` |
|        - |  7370 | ` * Refer to the implementation of [ob_end_clean()] for more` |
|        - |  7371 | ` * information.` |
|        - |  7372 | ` */` |
|     7488 |  7373 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry)` |
|        2 |  7374 |  |
|     7490 |  7375 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     7490 |  7376 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|        - |  7377 | `		/* No more stackable OB */` |
|     7472 |  7378 | `		pCons->xConsumer = pCons->xDef;` |
|     7472 |  7379 | `		pCons->pUserData = pCons->pDefData;` |
|     3735 |  7380 | `	}` |
|        - |  7381 | `	/* Release OB data */` |
|     7490 |  7382 | `	PH7_MemObjRelease(&pEntry->sCallback);` |
|     7490 |  7383 | `	SyBlobRelease(&pEntry->sOB);` |
|     7490 |  7384 |  |
|        - |  7385 | `/*` |
|        - |  7386 | ` * bool ob_start([ callback $output_callback] )` |
|        - |  7387 | ` * This function will turn output buffering on. While output buffering is active no output` |
|        - |  7388 | ` *  is sent from the script (other than headers), instead the output is stored in an internal` |
|        - |  7389 | ` *  buffer.` |
|        - |  7390 | ` * Parameter` |
|        - |  7391 | ` *  $output_callback` |
|        - |  7392 | ` *   An optional output_callback function may be specified. This function takes a string` |
|        - |  7393 | ` *   as a parameter and should return a string. The function will be called when the output` |
|        - |  7394 | ` *   buffer is flushed (sent) or cleaned (with ob_flush(), ob_clean() or similar function)` |
|        - |  7395 | ` *   or when the output buffer is flushed to the browser at the end of the request.` |
|        - |  7396 | ` *   When output_callback is called, it will receive the contents of the output buffer` |
|        - |  7397 | ` *   as its parameter and is expected to return a new output buffer as a result, which will` |
|        - |  7398 | ` *   be sent to the browser. If the output_callback is not a callable function, this function` |
|        - |  7399 | ` *   will return FALSE.` |
|        - |  7400 | ` *   If the callback function has two parameters, the second parameter is filled with` |
|        - |  7401 | ` *   a bit-field consisting of PHP_OUTPUT_HANDLER_START, PHP_OUTPUT_HANDLER_CONT` |
|        - |  7402 | ` *   and PHP_OUTPUT_HANDLER_END.` |
|        - |  7403 | ` *   If output_callback returns FALSE original input is sent to the browser.` |
|        - |  7404 | ` *   The output_callback parameter may be bypassed by passing a NULL value.` |
|        - |  7405 | ` * Return` |
|        - |  7406 | ` *   Returns TRUE on success or FALSE on failure.` |
|        - |  7407 | ` */` |
|     7488 |  7408 | `static int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7409 |  |
|     7490 |  7410 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7411 | `	VmObEntry sOb;` |
|        - |  7412 | `	sxi32 rc;` |
|        - |  7413 | `	/* Initialize the OB entry */` |
|     7490 |  7414 | `	PH7_MemObjInit(pCtx->pVm,&sOb.sCallback);` |
|     7490 |  7415 | `	SyBlobInit(&sOb.sOB,&pVm->sAllocator);` |
|     7490 |  7416 | `	if( nArg > 0 && (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) ){` |
|        - |  7417 | `		/* Save the callback name for later invocation */` |
|      ! 0 |  7418 | `		PH7_MemObjStore(apArg[0],&sOb.sCallback);` |
|      ! 0 |  7419 | `	}` |
|        - |  7420 | `	/* Push in the stack */` |
|     7490 |  7421 | `	rc = SySetPut(&pVm->aOB,(const void *)&sOb);` |
|     7490 |  7422 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7423 | `		PH7_MemObjRelease(&sOb.sCallback);` |
|      ! 0 |  7424 | `	}else{` |
|     7490 |  7425 | `		ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        - |  7426 | `		/* Substitute the default VM consumer */` |
|     7490 |  7427 | `		if( pCons->xConsumer != VmObConsumer ){` |
|     7472 |  7428 | `			pCons->xDef = pCons->xConsumer;` |
|     7472 |  7429 | `			pCons->pDefData = pCons->pUserData;` |
|        - |  7430 | `			/* Install the new consumer */` |
|     7472 |  7431 | `			pCons->xConsumer = VmObConsumer;` |
|     7472 |  7432 | `			pCons->pUserData = pVm;` |
|     3735 |  7433 | `		}` |
|        - |  7434 | `	}` |
|     7490 |  7435 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     7490 |  7436 | `	return PH7_OK;` |
|        2 |  7437 |  |
|        - |  7438 | `/*` |
|        - |  7439 | ` * Flush Output buffer to the default VM output consumer.` |
|        - |  7440 | ` * Refer to the implementation of [ob_flush()] for more` |
|        - |  7441 | ` * information.` |
|        - |  7442 | ` */` |
|        4 |  7443 | `static sxi32 VmObFlush(ph7_vm *pVm,VmObEntry *pEntry,int bRelease)` |
|        1 |  7444 |  |
|        5 |  7445 | `	SyBlob *pBlob = &pEntry->sOB;` |
|        - |  7446 | `	sxi32 rc;` |
|        - |  7447 | `	/* Flush contents */` |
|        5 |  7448 | `	rc = PH7_OK;` |
|        5 |  7449 | `	if( SyBlobLength(pBlob) > 0 ){` |
|        - |  7450 | `		/* Call the VM output consumer */` |
|        5 |  7451 | `		rc = pVm->sVmConsumer.xDef(SyBlobData(pBlob),SyBlobLength(pBlob),pVm->sVmConsumer.pDefData);` |
|        - |  7452 | `		/* Increment VM output counter */` |
|        5 |  7453 | `		pVm->nOutputLen += SyBlobLength(pBlob);` |
|        5 |  7454 | `		if( rc != PH7_ABORT ){` |
|        5 |  7455 | `			rc = PH7_OK;` |
|        2 |  7456 | `		}` |
|        2 |  7457 | `	}` |
|        5 |  7458 | `	if( bRelease ){` |
|        3 |  7459 | `		VmObRestore(&(*pVm),pEntry);` |
|        2 |  7460 | `	}else{` |
|        - |  7461 | `		/* Reset the blob */` |
|        3 |  7462 | `		SyBlobReset(pBlob);` |
|        - |  7463 | `	}` |
|        5 |  7464 | `	return rc;` |
|        1 |  7465 |  |
|        - |  7466 | `/*` |
|        - |  7467 | ` * void ob_flush(void)` |
|        - |  7468 | ` * void flush(void)` |
|        - |  7469 | ` *  Flush (send) the output buffer.` |
|        - |  7470 | ` * Parameter` |
|        - |  7471 | ` *  None` |
|        - |  7472 | ` * Return` |
|        - |  7473 | ` *  No return value.` |
|        - |  7474 | ` */` |
|        2 |  7475 | `static int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7476 |  |
|        3 |  7477 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7478 | `	VmObEntry *pOb;` |
|        - |  7479 | `	sxi32 rc;` |
|        - |  7480 | `	/* Peek the top most OB entry */` |
|        3 |  7481 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  7482 | `	if( pOb == 0 ){` |
|        - |  7483 | `		/* Empty stack,return immediately */` |
|      ! 0 |  7484 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7485 | `		SXUNUSED(apArg);` |
|      ! 0 |  7486 | `		return PH7_OK;` |
|        - |  7487 | `	}` |
|        - |  7488 | `	/* Flush contents */` |
|        3 |  7489 | `	rc = VmObFlush(pVm,pOb,FALSE);` |
|        3 |  7490 | `	return rc;` |
|        2 |  7491 |  |
|        - |  7492 | `/*` |
|        - |  7493 | ` * bool ob_end_flush(void)` |
|        - |  7494 | ` *  Flush (send) the output buffer and turn off output buffering.` |
|        - |  7495 | ` * Parameter` |
|        - |  7496 | ` *  None` |
|        - |  7497 | ` * Return` |
|        - |  7498 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first` |
|        - |  7499 | ` *  that you called the function without an active buffer or that for some reason` |
|        - |  7500 | ` *  a buffer could not be deleted (possible for special buffer).` |
|        - |  7501 | ` */` |
|        2 |  7502 | `static int vm_builtin_ob_end_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7503 |  |
|        3 |  7504 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7505 | `	VmObEntry *pOb;` |
|        - |  7506 | `	sxi32 rc;` |
|        - |  7507 | `	/* Pop the top most OB entry */` |
|        3 |  7508 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|        3 |  7509 | `	if( pOb == 0 ){` |
|        - |  7510 | `		/* Empty stack,return FALSE */` |
|      ! 0 |  7511 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7512 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7513 | `		SXUNUSED(apArg);` |
|      ! 0 |  7514 | `		return PH7_OK;` |
|        - |  7515 | `	}` |
|        - |  7516 | `	/* Flush contents */` |
|        3 |  7517 | `	rc = VmObFlush(pVm,pOb,TRUE);` |
|        - |  7518 | `	/* Return true */` |
|        3 |  7519 | `	ph7_result_bool(pCtx,1);` |
|        3 |  7520 | `	return rc;` |
|        2 |  7521 |  |
|        - |  7522 | `/*` |
|        - |  7523 | ` * void ob_implicit_flush([int $flag = true ])` |
|        - |  7524 | ` *  ob_implicit_flush() will turn implicit flushing on or off.` |
|        - |  7525 | ` *  Implicit flushing will result in a flush operation after every` |
|        - |  7526 | ` *  output call, so that explicit calls to flush() will no longer be needed.` |
|        - |  7527 | ` * Parameter` |
|        - |  7528 | ` *  $flag` |
|        - |  7529 | ` *   TRUE to turn implicit flushing on, FALSE otherwise.` |
|        - |  7530 | ` * Return` |
|        - |  7531 | ` *   Nothing` |
|        - |  7532 | ` */` |
|        4 |  7533 | `static int vm_builtin_ob_implicit_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7534 |  |
|        - |  7535 | `	/* NOTE: As of this version,this function is a no-op.` |
|        - |  7536 | `	 * PH7 is smart enough to flush it's internal buffer when appropriate.` |
|        - |  7537 | `	 */` |
|        2 |  7538 | `	SXUNUSED(pCtx);` |
|        2 |  7539 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  7540 | `	SXUNUSED(apArg);` |
|        5 |  7541 | `	return PH7_OK;` |
|        1 |  7542 |  |
|        - |  7543 | `/*` |
|        - |  7544 | ` * array ob_list_handlers(void)` |
|        - |  7545 | ` *  Lists all output handlers in use.` |
|        - |  7546 | ` * Parameter` |
|        - |  7547 | ` *  None` |
|        - |  7548 | ` * Return` |
|        - |  7549 | ` *  This will return an array with the output handlers in use (if any).` |
|        - |  7550 | ` */` |
|        2 |  7551 | `static int vm_builtin_ob_list_handlers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7552 |  |
|        3 |  7553 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7554 | `	ph7_value *pArray;` |
|        - |  7555 | `	VmObEntry *aEntry;` |
|        - |  7556 | `	ph7_value sVal;` |
|        - |  7557 | `	sxu32 n;` |
|        3 |  7558 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|        - |  7559 | `		/* Empty stack,return null */` |
|      ! 0 |  7560 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7561 | `		return PH7_OK;` |
|        - |  7562 | `	}` |
|        - |  7563 | `	/* Create a new array */` |
|        3 |  7564 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7565 | `	if( pArray == 0 ){` |
|        - |  7566 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7567 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7568 | `		SXUNUSED(apArg);` |
|      ! 0 |  7569 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7570 | `		return PH7_OK;` |
|        - |  7571 | `	}` |
|        3 |  7572 | `	PH7_MemObjInit(pVm,&sVal);` |
|        - |  7573 | `	/* Point to the installed OB entries */` |
|        3 |  7574 | `	aEntry = (VmObEntry *)SySetBasePtr(&pVm->aOB);` |
|        - |  7575 | `	/* Perform the requested operation */` |
|        5 |  7576 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; n++ ){` |
|        3 |  7577 | `		VmObEntry *pEntry = &aEntry[n];` |
|        - |  7578 | `		/* Extract handler name */` |
|        3 |  7579 | `		SyBlobReset(&sVal.sBlob);` |
|        3 |  7580 | `		if( pEntry->sCallback.iFlags & MEMOBJ_STRING ){` |
|        - |  7581 | `			/* Callback,dup it's name */` |
|      ! 0 |  7582 | `			SyBlobDup(&pEntry->sCallback.sBlob,&sVal.sBlob);` |
|        3 |  7583 | `		}else if( pEntry->sCallback.iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  7584 | `			SyBlobAppend(&sVal.sBlob,"Class Method",sizeof("Class Method")-1);` |
|      ! 0 |  7585 | `		}else{` |
|        3 |  7586 | `			SyBlobAppend(&sVal.sBlob,"default output handler",sizeof("default output handler")-1);` |
|        - |  7587 | `		}` |
|        3 |  7588 | `		sVal.iFlags = MEMOBJ_STRING;` |
|        - |  7589 | `		/* Perform the insertion */` |
|        3 |  7590 | `		ph7_array_add_elem(pArray,0/* Automatic index assign */,&sVal /* Will make it's own copy */);` |
|        2 |  7591 | `	}` |
|        3 |  7592 | `	PH7_MemObjRelease(&sVal);` |
|        - |  7593 | `	/* Return the freshly created array */` |
|        3 |  7594 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7595 | `	return PH7_OK;` |
|        2 |  7596 |  |
|        - |  7597 | `/*` |
|        - |  7598 | ` * Section:` |
|        - |  7599 | ` *  Random numbers/string generators.` |
|        - |  7600 | ` * Status:` |
|        - |  7601 | ` *    Stable.` |
|        - |  7602 | ` */` |
|        - |  7603 | `/*` |
|        - |  7604 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  7605 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  7606 | ` * used by te SQLite3 library.` |
|        - |  7607 | ` */` |
|     1745 |  7608 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  7609 |  |
|        - |  7610 | `	sxu32 iNum;` |
|     1747 |  7611 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     1747 |  7612 | `	return iNum;` |
|        2 |  7613 |  |
|        - |  7614 | `/*` |
|        - |  7615 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  7616 | ` * Note that the generated string is NOT null terminated.` |
|        - |  7617 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  7618 | ` * by te SQLite3 library.` |
|        - |  7619 | ` */` |
|    55744 |  7620 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  7621 |  |
|        - |  7622 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  7623 | `	int i;` |
|        - |  7624 | `	/* Generate a binary string first */` |
|    55746 |  7625 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  7626 | `	/* Turn the binary string into english based alphabet */` |
|   613354 |  7627 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   557610 |  7628 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   278806 |  7629 | `	 }` |
|    55746 |  7630 |  |
|        - |  7631 | `/*` |
|        - |  7632 | ` * int rand()` |
|        - |  7633 | ` * int mt_rand()` |
|        - |  7634 | ` * int rand(int $min,int $max)` |
|        - |  7635 | ` * int mt_rand(int $min,int $max)` |
|        - |  7636 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  7637 | ` * Parameter` |
|        - |  7638 | ` *  $min` |
|        - |  7639 | ` *    The lowest value to return (default: 0)` |
|        - |  7640 | ` *  $max` |
|        - |  7641 | ` *   The highest value to return (default: getrandmax())` |
|        - |  7642 | ` * Return` |
|        - |  7643 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  7644 | ` * Note:` |
|        - |  7645 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7646 | ` *  by te SQLite3 library.` |
|        - |  7647 | ` */` |
|       20 |  7648 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7649 |  |
|        - |  7650 | `	sxu32 iNum;` |
|        - |  7651 | `	/* Generate the random number */` |
|       21 |  7652 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  7653 | `	if( nArg > 1 ){` |
|        - |  7654 | `		sxu32 iMin,iMax;` |
|        3 |  7655 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  7656 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  7657 | `		if( iMin < iMax ){` |
|        3 |  7658 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  7659 | `			if( iDiv > 0 ){` |
|        3 |  7660 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  7661 | `			}` |
|        1 |  7662 | `		}else if(iMax > 0 ){` |
|      ! 0 |  7663 | `			iNum %= iMax;` |
|      ! 0 |  7664 | `		}` |
|        1 |  7665 | `	}` |
|        - |  7666 | `	/* Return the number */` |
|       21 |  7667 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  7668 | `	return SXRET_OK;` |
|        1 |  7669 |  |
|        - |  7670 | `/*` |
|        - |  7671 | ` * int getrandmax(void)` |
|        - |  7672 | ` * int mt_getrandmax(void)` |
|        - |  7673 | ` * int rc4_getrandmax(void)` |
|        - |  7674 | ` *   Show largest possible random value` |
|        - |  7675 | ` * Return` |
|        - |  7676 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  7677 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  7678 | ` * Note:` |
|        - |  7679 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7680 | ` *  by te SQLite3 library.` |
|        - |  7681 | ` */` |
|        4 |  7682 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7683 |  |
|        2 |  7684 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  7685 | `	SXUNUSED(apArg);` |
|        5 |  7686 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  7687 | `	return SXRET_OK;` |
|        1 |  7688 |  |
|        - |  7689 | `/*` |
|        - |  7690 | ` * string rand_str()` |
|        - |  7691 | ` * string rand_str(int $len)` |
|        - |  7692 | ` *  Generate a random string (English alphabet).` |
|        - |  7693 | ` * Parameter` |
|        - |  7694 | ` *  $len` |
|        - |  7695 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  7696 | ` * Return` |
|        - |  7697 | ` *   A pseudo random string.` |
|        - |  7698 | ` * Note:` |
|        - |  7699 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7700 | ` *  by te SQLite3 library.` |
|        - |  7701 | ` *  This function is a symisc extension.` |
|        - |  7702 | ` */` |
|      120 |  7703 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7704 |  |
|        - |  7705 | `	char zString[1024];` |
|      122 |  7706 | `	int iLen = 0x10;` |
|      122 |  7707 | `	if( nArg > 0 ){` |
|        - |  7708 | `		/* Get the desired length */` |
|      122 |  7709 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  7710 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  7711 | `			/* Default length */` |
|        3 |  7712 | `			iLen = 0x10;` |
|        1 |  7713 | `		}` |
|       60 |  7714 | `	}` |
|        - |  7715 | `	/* Generate the random string */` |
|      122 |  7716 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  7717 | `	/* Return the generated string */` |
|      122 |  7718 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  7719 | `	return SXRET_OK;` |
|        2 |  7720 |  |
|        - |  7721 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  7722 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  7723 | `/* Unique ID private data */` |
|        - |  7724 | `struct unique_id_data` |
|        - |  7725 |  |
|        - |  7726 | `	ph7_context *pCtx; /* Call context */` |
|        - |  7727 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  7728 | `};` |
|        - |  7729 | `/*` |
|        - |  7730 | ` * Binary to hex consumer callback.` |
|        - |  7731 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  7732 | ` * defined below.` |
|        - |  7733 | ` */` |
|      192 |  7734 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  7735 |  |
|      193 |  7736 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  7737 | `	sxu32 nBuflen;` |
|        - |  7738 | `	/* Extract result buffer length */` |
|      193 |  7739 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  7740 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  7741 | `			/*` |
|        - |  7742 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  7743 | `			 * string will be 13 characters long` |
|        - |  7744 | `			 */` |
|       25 |  7745 | `		return SXERR_ABORT;` |
|        - |  7746 | `	}` |
|      169 |  7747 | `	if( nBuflen > 22 ){` |
|      ! 0 |  7748 | `		return SXERR_ABORT;` |
|        - |  7749 | `	}` |
|        - |  7750 | `	/* Safely Consume the hex stream */` |
|      169 |  7751 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  7752 | `	return SXRET_OK;` |
|       97 |  7753 |  |
|        - |  7754 | `/*` |
|        - |  7755 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  7756 | ` *  Generate a unique ID` |
|        - |  7757 | ` * Parameter` |
|        - |  7758 | ` * $prefix` |
|        - |  7759 | ` *  Append this prefix to the generated unique ID.` |
|        - |  7760 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  7761 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  7762 | ` * $more_entropy` |
|        - |  7763 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  7764 | ` *  that the result will be unique.` |
|        - |  7765 | ` * Return` |
|        - |  7766 | ` *  Returns the unique identifier, as a string.` |
|        - |  7767 | ` */` |
|       24 |  7768 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7769 |  |
|        - |  7770 | `	struct unique_id_data sUniq;` |
|        - |  7771 | `	unsigned char zDigest[20];` |
|       25 |  7772 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7773 | `	const char *zPrefix;` |
|        - |  7774 | `	SHA1Context sCtx;` |
|        - |  7775 | `	char zRandom[7];` |
|        - |  7776 | `	int nPrefix;` |
|        - |  7777 | `	int entropy;` |
|        - |  7778 | `	/* Generate a random string first */` |
|       25 |  7779 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  7780 | `	/* Initialize fields */` |
|       25 |  7781 | `	zPrefix = 0;` |
|       25 |  7782 | `	nPrefix = 0;` |
|       25 |  7783 | `	entropy = 0;` |
|       25 |  7784 | `	if( nArg > 0 ){` |
|        - |  7785 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  7786 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  7787 | `		if( nArg > 1 ){` |
|      ! 0 |  7788 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  7789 | `		}` |
|      ! 0 |  7790 | `	}` |
|       25 |  7791 | `	SHA1Init(&sCtx);` |
|        - |  7792 | `	/* Generate the random ID */` |
|       25 |  7793 | `	if( nPrefix > 0 ){` |
|      ! 0 |  7794 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  7795 | `	}` |
|        - |  7796 | `	/* Append the random ID */` |
|       25 |  7797 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  7798 | `	/* Append the random string */` |
|       25 |  7799 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  7800 | `	/* Increment the number */` |
|       25 |  7801 | `	pVm->unique_id++;` |
|       25 |  7802 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  7803 | `	/* Hexify the digest */` |
|       25 |  7804 | `	sUniq.pCtx = pCtx;` |
|       25 |  7805 | `	sUniq.entropy = entropy;` |
|       25 |  7806 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  7807 | `	/* All done */` |
|       25 |  7808 | `	return PH7_OK;` |
|        1 |  7809 |  |
|        - |  7810 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  7811 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  7812 | `/*` |
|        - |  7813 | ` * Section:` |
|        - |  7814 | ` *  Language construct implementation as foreign functions.` |
|        - |  7815 | ` * Status:` |
|        - |  7816 | ` *    Stable.` |
|        - |  7817 | ` */` |
|        - |  7818 | `/*` |
|        - |  7819 | ` * void echo($string...)` |
|        - |  7820 | ` *  Output one or more messages.` |
|        - |  7821 | ` * Parameters` |
|        - |  7822 | ` *  $string` |
|        - |  7823 | ` *   Message to output.` |
|        - |  7824 | ` * Return` |
|        - |  7825 | ` *  NULL.` |
|        - |  7826 | ` */` |
|      ! 0 |  7827 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7828 |  |
|        - |  7829 | `	const char *zData;` |
|      ! 0 |  7830 | `	int nDataLen = 0;` |
|        - |  7831 | `	ph7_vm *pVm;` |
|        - |  7832 | `	int i,rc;` |
|        - |  7833 | `	/* Point to the target VM */` |
|      ! 0 |  7834 | `	pVm = pCtx->pVm;` |
|        - |  7835 | `	/* Output */` |
|      ! 0 |  7836 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  7837 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  7838 | `		if( nDataLen > 0 ){` |
|      ! 0 |  7839 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  7840 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7841 | `				/* Increment output length */` |
|      ! 0 |  7842 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  7843 | `			}` |
|      ! 0 |  7844 | `			if( rc == SXERR_ABORT ){` |
|        - |  7845 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7846 | `				return PH7_ABORT;` |
|        - |  7847 | `			}` |
|      ! 0 |  7848 | `		}` |
|      ! 0 |  7849 | `	}` |
|      ! 0 |  7850 | `	return SXRET_OK;` |
|      ! 0 |  7851 |  |
|        - |  7852 | `/*` |
|        - |  7853 | ` * int print($string...)` |
|        - |  7854 | ` *  Output one or more messages.` |
|        - |  7855 | ` * Parameters` |
|        - |  7856 | ` *  $string` |
|        - |  7857 | ` *   Message to output.` |
|        - |  7858 | ` * Return` |
|        - |  7859 | ` *  1 always.` |
|        - |  7860 | ` */` |
|        2 |  7861 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7862 |  |
|        - |  7863 | `	const char *zData;` |
|        3 |  7864 | `	int nDataLen = 0;` |
|        - |  7865 | `	ph7_vm *pVm;` |
|        - |  7866 | `	int i,rc;` |
|        - |  7867 | `	/* Point to the target VM */` |
|        3 |  7868 | `	pVm = pCtx->pVm;` |
|        - |  7869 | `	/* Output */` |
|        5 |  7870 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  7871 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  7872 | `		if( nDataLen > 0 ){` |
|        3 |  7873 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  7874 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7875 | `				/* Increment output length */` |
|        3 |  7876 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  7877 | `			}` |
|        3 |  7878 | `			if( rc == SXERR_ABORT ){` |
|        - |  7879 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7880 | `				return PH7_ABORT;` |
|        - |  7881 | `			}` |
|        1 |  7882 | `		}` |
|        2 |  7883 | `	}` |
|        - |  7884 | `	/* Return 1 */` |
|        3 |  7885 | `	ph7_result_int(pCtx,1);` |
|        3 |  7886 | `	return SXRET_OK;` |
|        2 |  7887 |  |
|        - |  7888 | `/*` |
|        - |  7889 | ` * void exit(string $msg)` |
|        - |  7890 | ` * void exit(int $status)` |
|        - |  7891 | ` * void die(string $ms)` |
|        - |  7892 | ` * void die(int $status)` |
|        - |  7893 | ` *   Output a message and terminate program execution.` |
|        - |  7894 | ` * Parameter` |
|        - |  7895 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  7896 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  7897 | ` *  and not printed` |
|        - |  7898 | ` * Return` |
|        - |  7899 | ` *  NULL` |
|        - |  7900 | ` */` |
|      ! 0 |  7901 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7902 |  |
|      ! 0 |  7903 | `	if( nArg > 0 ){` |
|      ! 0 |  7904 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  7905 | `			const char *zData;` |
|      ! 0 |  7906 | `			int iLen = 0;` |
|        - |  7907 | `			/* Print exit message */` |
|      ! 0 |  7908 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  7909 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  7910 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  7911 | `			sxi32 iExitStatus;` |
|        - |  7912 | `			/* Record exit status code */` |
|      ! 0 |  7913 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  7914 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  7915 | `		}` |
|      ! 0 |  7916 | `	}` |
|        - |  7917 | `	/* Check if we are in an included file */` |
|      ! 0 |  7918 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  7919 | `		/* Exit the entire process */` |
|      ! 0 |  7920 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  7921 | `	}` |
|        - |  7922 | `	/* Abort processing immediately */` |
|      ! 0 |  7923 | `	return PH7_ABORT;` |
|      ! 0 |  7924 |  |
|        - |  7925 | `/*` |
|        - |  7926 | ` * bool isset($var,...)` |
|        - |  7927 | ` *  Finds out whether a variable is set.` |
|        - |  7928 | ` * Parameters` |
|        - |  7929 | ` *  One or more variable to check.` |
|        - |  7930 | ` * Return` |
|        - |  7931 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  7932 | ` */` |
|    65266 |  7933 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7934 |  |
|        - |  7935 | `	ph7_value *pObj;` |
|    65268 |  7936 | `	int res = 0;` |
|        - |  7937 | `	int i;` |
|    65268 |  7938 | `	if( nArg < 1 ){` |
|        - |  7939 | `		/* Missing arguments,return false */` |
|      ! 0 |  7940 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  7941 | `		return SXRET_OK;` |
|        - |  7942 | `	}` |
|        - |  7943 | `	/* Iterate over available arguments */` |
|    86580 |  7944 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    65268 |  7945 | `		pObj = apArg[i];` |
|    65268 |  7946 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    43576 |  7947 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7948 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  7949 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  7950 | `			}` |
|    21787 |  7951 | `		}` |
|    65268 |  7952 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    65268 |  7953 | `		if( !res ){` |
|        - |  7954 | `			/* Variable not set,return FALSE */` |
|    43956 |  7955 | `			ph7_result_bool(pCtx,0);` |
|    43956 |  7956 | `			return SXRET_OK;` |
|        - |  7957 | `		}` |
|    10658 |  7958 | `	}` |
|        - |  7959 | `	/* All given variable are set,return TRUE */` |
|    21314 |  7960 | `	ph7_result_bool(pCtx,1);` |
|    21314 |  7961 | `	return SXRET_OK;` |
|    32635 |  7962 |  |
|        - |  7963 | `/*` |
|        - |  7964 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  7965 | ` * frame,the reference table and discard it's contents.` |
|        - |  7966 | ` * This function never fail and always return SXRET_OK.` |
|        - |  7967 | ` */` |
|  2914064 |  7968 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  7969 |  |
|        - |  7970 | `	ph7_value *pObj;` |
|        - |  7971 | `	VmRefObj *pRef;` |
|  2914066 |  7972 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2914066 |  7973 | `	if( pObj ){` |
|        - |  7974 | `		/* Release the object */` |
|  2914066 |  7975 | `		PH7_MemObjRelease(pObj);` |
|  1457032 |  7976 | `	}` |
|        - |  7977 | `	/* Remove old reference links */` |
|  2914066 |  7978 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2914066 |  7979 | `	if( pRef ){` |
|  2914046 |  7980 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  7981 | `		/* Unlink from the reference table */` |
|  2914046 |  7982 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2914046 |  7983 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  7984 | `			VmSlot sFree;` |
|        - |  7985 | `			/* Restore to the free list */` |
|  2914040 |  7986 | `			sFree.nIdx = nObjIdx;` |
|  2914040 |  7987 | `			sFree.pUserData = 0;` |
|  2914040 |  7988 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1457019 |  7989 | `		}` |
|  1457022 |  7990 | `	}` |
|  2914066 |  7991 | `	return SXRET_OK;` |
|        2 |  7992 |  |
|        - |  7993 | `/*` |
|        - |  7994 | ` * void unset($var,...)` |
|        - |  7995 | ` *   Unset one or more given variable.` |
|        - |  7996 | ` * Parameters` |
|        - |  7997 | ` *  One or more variable to unset.` |
|        - |  7998 | ` * Return` |
|        - |  7999 | ` *  Nothing.` |
|        - |  8000 | ` */` |
|     3164 |  8001 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8002 |  |
|        - |  8003 | `	ph7_value *pObj;` |
|        - |  8004 | `	ph7_vm *pVm;` |
|        - |  8005 | `	int i;` |
|        - |  8006 | `	/* Point to the target VM */` |
|     3166 |  8007 | `	pVm = pCtx->pVm;` |
|        - |  8008 | `	/* Iterate and unset */` |
|     9472 |  8009 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6308 |  8010 | `		pObj = apArg[i];` |
|     6308 |  8011 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      812 |  8012 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  8013 | `				/* Throw an error */` |
|      ! 0 |  8014 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  8015 | `			}` |
|      407 |  8016 | `		}else{` |
|     5497 |  8017 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  8018 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     5497 |  8019 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     5491 |  8020 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2745 |  8021 | `			}` |
|        - |  8022 | `		}` |
|     3155 |  8023 | `	}` |
|     3166 |  8024 | `	return SXRET_OK;` |
|        2 |  8025 |  |
|        - |  8026 | `/*` |
|        - |  8027 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  8028 | ` */` |
|      110 |  8029 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8030 |  |
|      111 |  8031 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  8032 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  8033 | `	ph7_value *pObj;` |
|        - |  8034 | `	sxu32 nIdx;` |
|        - |  8035 | `	/* Extract the memory object */` |
|      111 |  8036 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  8037 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  8038 | `	if( pObj ){` |
|      111 |  8039 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  8040 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  8041 | `				SyString sName;` |
|        - |  8042 | `				ph7_value sKey;` |
|        - |  8043 | `				/* Perform the insertion */` |
|      109 |  8044 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  8045 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  8046 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  8047 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  8048 | `			}` |
|       54 |  8049 | `		}` |
|       55 |  8050 | `	}` |
|      111 |  8051 | `	return SXRET_OK;` |
|        1 |  8052 |  |
|        - |  8053 | `/*` |
|        - |  8054 | ` * array get_defined_vars(void)` |
|        - |  8055 | ` *  Returns an array of all defined variables.` |
|        - |  8056 | ` * Parameter` |
|        - |  8057 | ` *  None` |
|        - |  8058 | ` * Return` |
|        - |  8059 | ` *  An array with all the variables defined in the current scope.` |
|        - |  8060 | ` */` |
|        2 |  8061 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8062 |  |
|        3 |  8063 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8064 | `	ph7_value *pArray;` |
|        - |  8065 | `	/* Create a new array */` |
|        3 |  8066 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8067 | ` 	if( pArray == 0 ){` |
|      ! 0 |  8068 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8069 | `		SXUNUSED(apArg);` |
|        - |  8070 | `		/* Return NULL */` |
|      ! 0 |  8071 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8072 | `		return SXRET_OK;` |
|        - |  8073 | `	}` |
|        - |  8074 | `	/* Superglobals first */` |
|        3 |  8075 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  8076 | `	/* Then variable defined in the current frame */` |
|        3 |  8077 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  8078 | `	/* Finally,return the created array */` |
|        3 |  8079 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8080 | `	return SXRET_OK;` |
|        2 |  8081 |  |
|        - |  8082 | `/*` |
|        - |  8083 | ` * bool gettype($var)` |
|        - |  8084 | ` *  Get the type of a variable` |
|        - |  8085 | ` * Parameters` |
|        - |  8086 | ` *   $var` |
|        - |  8087 | ` *    The variable being type checked.` |
|        - |  8088 | ` * Return` |
|        - |  8089 | ` *   String representation of the given variable type.` |
|        - |  8090 | ` */` |
|       30 |  8091 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8092 |  |
|       32 |  8093 | `	const char *zType = "Empty";` |
|       32 |  8094 | `	if( nArg > 0 ){` |
|       32 |  8095 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       15 |  8096 | `	}` |
|        - |  8097 | `	/* Return the variable type */` |
|       32 |  8098 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       32 |  8099 | `	return SXRET_OK;` |
|        2 |  8100 |  |
|        - |  8101 | `/*` |
|        - |  8102 | ` * string get_resource_type(resource $handle)` |
|        - |  8103 | ` *  This function gets the type of the given resource.` |
|        - |  8104 | ` * Parameters` |
|        - |  8105 | ` *  $handle` |
|        - |  8106 | ` *  The evaluated resource handle.` |
|        - |  8107 | ` * Return` |
|        - |  8108 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  8109 | ` *  representing its type. If the type is not identified by this function` |
|        - |  8110 | ` *  the return value will be the string Unknown.` |
|        - |  8111 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  8112 | ` *  is not a resource.` |
|        - |  8113 | ` */` |
|        2 |  8114 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8115 |  |
|        3 |  8116 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  8117 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  8118 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8119 | `		return PH7_OK;` |
|        - |  8120 | `	}` |
|        3 |  8121 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  8122 | `	return SXRET_OK;` |
|        2 |  8123 |  |
|        - |  8124 | `/*` |
|        - |  8125 | ` * void var_dump(expression,....)` |
|        - |  8126 | ` *   var_dump � Dumps information about a variable` |
|        - |  8127 | ` * Parameters` |
|        - |  8128 | ` *   One or more expression to dump.` |
|        - |  8129 | ` * Returns` |
|        - |  8130 | ` *  Nothing.` |
|        - |  8131 | ` */` |
|      220 |  8132 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8133 |  |
|        - |  8134 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  8135 | `	int i;` |
|      222 |  8136 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  8137 | `	/* Dump one or more expressions */` |
|      448 |  8138 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      228 |  8139 | `		ph7_value *pObj = apArg[i];` |
|        - |  8140 | `		/* Reset the working buffer */` |
|      228 |  8141 | `		SyBlobReset(&sDump);` |
|        - |  8142 | `		/* Dump the given expression */` |
|      228 |  8143 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  8144 | `		/* Output */` |
|      228 |  8145 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      228 |  8146 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      113 |  8147 | `		}` |
|      115 |  8148 | `	}` |
|        - |  8149 | `	/* Release the working buffer */` |
|      222 |  8150 | `	SyBlobRelease(&sDump);` |
|      222 |  8151 | `	return SXRET_OK;` |
|        2 |  8152 |  |
|        - |  8153 | `/*` |
|        - |  8154 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  8155 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  8156 | ` * Parameters` |
|        - |  8157 | ` *   expression: Expression to dump` |
|        - |  8158 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  8159 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  8160 | ` *            print_r() will return the information rather than print it.` |
|        - |  8161 | ` * Return` |
|        - |  8162 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  8163 | ` *  Otherwise, the return value is TRUE.` |
|        - |  8164 | ` */` |
|       16 |  8165 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8166 |  |
|       17 |  8167 | `	int ret_string = 0;` |
|        - |  8168 | `	SyBlob sDump;` |
|       17 |  8169 | `	if( nArg < 1 ){` |
|        - |  8170 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  8171 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8172 | `		return SXRET_OK;` |
|        - |  8173 | `	}` |
|       17 |  8174 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  8175 | `	if ( nArg > 1 ){` |
|        - |  8176 | `		/* Where to redirect output */` |
|       11 |  8177 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  8178 | `	}` |
|        - |  8179 | `	/* Generate dump */` |
|       17 |  8180 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  8181 | `	if( !ret_string ){` |
|        - |  8182 | `		/* Output dump */` |
|        7 |  8183 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8184 | `		/* Return true */` |
|        7 |  8185 | `		ph7_result_bool(pCtx,1);` |
|        4 |  8186 | `	}else{` |
|        - |  8187 | `		/* Generated dump as return value */` |
|       11 |  8188 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8189 | `	}` |
|        - |  8190 | `	/* Release the working buffer */` |
|       17 |  8191 | `	SyBlobRelease(&sDump);` |
|       17 |  8192 | `	return SXRET_OK;` |
|        9 |  8193 |  |
|        - |  8194 | `/*` |
|        - |  8195 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  8196 | ` * Same job as print_r. (see coment above)` |
|        - |  8197 | ` */` |
|        2 |  8198 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8199 |  |
|        3 |  8200 | `	int ret_string = 0;` |
|        - |  8201 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  8202 | `	if( nArg < 1 ){` |
|        - |  8203 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  8204 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8205 | `		return SXRET_OK;` |
|        - |  8206 | `	}` |
|        3 |  8207 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  8208 | `	if ( nArg > 1 ){` |
|        - |  8209 | `		/* Where to redirect output */` |
|        3 |  8210 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  8211 | `	}` |
|        - |  8212 | `	/* Generate dump */` |
|        3 |  8213 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  8214 | `	if( !ret_string ){` |
|        - |  8215 | `		/* Output dump */` |
|      ! 0 |  8216 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8217 | `		/* Return NULL */` |
|      ! 0 |  8218 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8219 | `	}else{` |
|        - |  8220 | `		/* Generated dump as return value */` |
|        3 |  8221 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8222 | `	}` |
|        - |  8223 | `	/* Release the working buffer */` |
|        3 |  8224 | `	SyBlobRelease(&sDump);` |
|        3 |  8225 | `	return SXRET_OK;` |
|        2 |  8226 |  |
|        - |  8227 | `/*` |
|        - |  8228 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  8229 | ` *  Set/get the various assert flags.` |
|        - |  8230 | ` * Parameter` |
|        - |  8231 | ` * $what` |
|        - |  8232 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  8233 | ` *   ASSERT_WARNING         Issue a warning for each failed assertion` |
|        - |  8234 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  8235 | ` *   ASSERT_QUIET_EVAL      Not used` |
|        - |  8236 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  8237 | ` * $value` |
|        - |  8238 | ` *   An optional new value for the option.` |
|        - |  8239 | ` * Return` |
|        - |  8240 | ` *  Old setting on success or FALSE on failure.` |
|        - |  8241 | ` */` |
|        8 |  8242 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8243 |  |
|        9 |  8244 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8245 | `	int iOld,iNew,iValue;` |
|        9 |  8246 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|        - |  8247 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  8248 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8249 | `		return PH7_OK;` |
|        - |  8250 | `	}` |
|        - |  8251 | `	/* Save old assertion flags */` |
|        9 |  8252 | `	iOld = pVm->iAssertFlags;` |
|        - |  8253 | `	/* Extract the new flags */` |
|        9 |  8254 | `	iNew = ph7_value_to_int(apArg[0]);` |
|        9 |  8255 | `	if( iNew == PH7_ASSERT_DISABLE ){` |
|        7 |  8256 | `		pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        7 |  8257 | `		if( nArg > 1 ){` |
|        5 |  8258 | `			iValue = !ph7_value_to_bool(apArg[1]);` |
|        5 |  8259 | `			if( iValue ){` |
|        - |  8260 | `				/* Disable assertion */` |
|        3 |  8261 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        1 |  8262 | `			}` |
|        3 |  8263 | `		}` |
|        6 |  8264 | `	}else if( iNew == PH7_ASSERT_WARNING ){` |
|      ! 0 |  8265 | `		pVm->iAssertFlags &= ~PH7_ASSERT_WARNING;` |
|      ! 0 |  8266 | `		if( nArg > 1 ){` |
|      ! 0 |  8267 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  8268 | `			if( iValue ){` |
|        - |  8269 | `				/* Issue a warning for each failed assertion */` |
|      ! 0 |  8270 | `				pVm->iAssertFlags \|= PH7_ASSERT_WARNING;` |
|      ! 0 |  8271 | `			}` |
|      ! 0 |  8272 | `		}` |
|        3 |  8273 | `	}else if( iNew == PH7_ASSERT_BAIL ){` |
|        3 |  8274 | `		pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        3 |  8275 | `		if( nArg > 1 ){` |
|        3 |  8276 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|        3 |  8277 | `			if( iValue ){` |
|        - |  8278 | `				/* Terminate execution on failed assertions */` |
|        3 |  8279 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        1 |  8280 | `			}` |
|        2 |  8281 | `		}` |
|        1 |  8282 | `	}else if( iNew == PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  8283 | `		pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|      ! 0 |  8284 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|        - |  8285 | `			/* Callback to call on failed assertions */` |
|      ! 0 |  8286 | `			PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  8287 | `			pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  8288 | `		}` |
|      ! 0 |  8289 | `	}` |
|        - |  8290 | `	/* Return the old flags */` |
|        9 |  8291 | `	ph7_result_int(pCtx,iOld);` |
|        9 |  8292 | `	return PH7_OK;` |
|        5 |  8293 |  |
|        - |  8294 | `/*` |
|        - |  8295 | ` * bool assert(mixed $assertion)` |
|        - |  8296 | ` *  Checks if assertion is FALSE.` |
|        - |  8297 | ` * Parameter` |
|        - |  8298 | ` *  $assertion` |
|        - |  8299 | ` *    The assertion to test.` |
|        - |  8300 | ` * Return` |
|        - |  8301 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  8302 | ` */` |
|       14 |  8303 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8304 |  |
|       15 |  8305 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8306 | `	ph7_value *pAssert;` |
|        - |  8307 | `	int iFlags,iResult;` |
|       15 |  8308 | `	if( nArg < 1 ){` |
|        - |  8309 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  8310 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8311 | `		return PH7_OK;` |
|        - |  8312 | `	}` |
|       15 |  8313 | `	iFlags = pVm->iAssertFlags;` |
|       15 |  8314 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  8315 | `		/* Assertion is disabled,return FALSE */` |
|      ! 0 |  8316 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8317 | `		return PH7_OK;` |
|        - |  8318 | `	}` |
|       15 |  8319 | `	pAssert = apArg[0];` |
|       15 |  8320 | `	iResult = 1; /* cc warning */` |
|       15 |  8321 | `	if( pAssert->iFlags & MEMOBJ_STRING ){` |
|        - |  8322 | `		SyString sChunk;` |
|        7 |  8323 | `		SyStringInitFromBuf(&sChunk,SyBlobData(&pAssert->sBlob),SyBlobLength(&pAssert->sBlob));` |
|        7 |  8324 | `		if( sChunk.nByte > 0 ){` |
|        5 |  8325 | `			VmEvalChunk(pVm,pCtx,&sChunk,PH7_PHP_ONLY\|PH7_PHP_EXPR,FALSE);` |
|        - |  8326 | `			/* Extract evaluation result */` |
|        5 |  8327 | `			iResult = ph7_value_to_bool(pCtx->pRet);` |
|        3 |  8328 | `		}else{` |
|        3 |  8329 | `			iResult = 0;` |
|        - |  8330 | `		}` |
|        4 |  8331 | `	}else{` |
|        - |  8332 | `		/* Perform a boolean cast */` |
|        9 |  8333 | `		iResult = ph7_value_to_bool(apArg[0]);` |
|        - |  8334 | `	}` |
|       15 |  8335 | `	if( !iResult ){` |
|        - |  8336 | `		/* Assertion failed */` |
|        9 |  8337 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  8338 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  8339 | `			ph7_value sFile,sLine;` |
|        - |  8340 | `			ph7_value *apCbArg[3];` |
|        - |  8341 | `			SyString *pFile;` |
|        - |  8342 | `			/* Extract the processed script */` |
|      ! 0 |  8343 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  8344 | `			if( pFile == 0 ){` |
|      ! 0 |  8345 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  8346 | `			}` |
|        - |  8347 | `			/* Invoke the callback */` |
|      ! 0 |  8348 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  8349 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  8350 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  8351 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  8352 | `			apCbArg[2] = pAssert;` |
|      ! 0 |  8353 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  8354 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  8355 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  8356 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  8357 | `		}` |
|        9 |  8358 | `		if( iFlags & PH7_ASSERT_WARNING ){` |
|        - |  8359 | `			/* Emit a warning */` |
|        9 |  8360 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Assertion failed");` |
|        4 |  8361 | `		}` |
|        9 |  8362 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  8363 | `			/* Abort VM execution immediately */` |
|        3 |  8364 | `			return PH7_ABORT;` |
|        - |  8365 | `		}` |
|        3 |  8366 | `	}` |
|        - |  8367 | `	/* Assertion result */` |
|       13 |  8368 | `	ph7_result_bool(pCtx,iResult);` |
|       13 |  8369 | `	return PH7_OK;` |
|        8 |  8370 |  |
|        - |  8371 | `/*` |
|        - |  8372 | ` * Section:` |
|        - |  8373 | ` *  Error reporting functions.` |
|        - |  8374 | ` * Status:` |
|        - |  8375 | ` *    Stable.` |
|        - |  8376 | ` */` |
|        - |  8377 | `/*` |
|        - |  8378 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  8379 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  8380 | ` * Parameters` |
|        - |  8381 | ` *  $error_msg` |
|        - |  8382 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  8383 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  8384 | ` * $error_type` |
|        - |  8385 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  8386 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  8387 | ` * Return` |
|        - |  8388 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  8389 | ` */` |
|       12 |  8390 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8391 |  |
|       14 |  8392 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  8393 | `	int rc = PH7_OK;` |
|       14 |  8394 | `	if( nArg > 0 ){` |
|        - |  8395 | `		const char *zErr;` |
|        - |  8396 | `		int nLen;` |
|        - |  8397 | `		/* Extract the error message */` |
|       12 |  8398 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8399 | `		if( nArg > 1 ){` |
|        - |  8400 | `			/* Extract the error type */` |
|       12 |  8401 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  8402 | `			switch( nErr ){` |
|        1 |  8403 | `			case 1:   /* E_ERROR */` |
|        - |  8404 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  8405 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  8406 | `			case 256: /* E_USER_ERROR */` |
|        3 |  8407 | `				nErr = PH7_CTX_ERR;` |
|        3 |  8408 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  8409 | `				break;` |
|        1 |  8410 | `			case 2:   /* E_WARNING */` |
|        - |  8411 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  8412 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  8413 | `			case 512: /* E_USER_WARNING */` |
|        3 |  8414 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  8415 | `				break;` |
|        3 |  8416 | `			default:` |
|        8 |  8417 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  8418 | `				break;` |
|        - |  8419 | `			}` |
|        5 |  8420 | `		}` |
|        - |  8421 | `		/* Report error */` |
|       12 |  8422 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  8423 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  8424 | `			return rc;` |
|        - |  8425 | `		}` |
|        - |  8426 | `		/* Return true */` |
|       12 |  8427 | `		ph7_result_bool(pCtx,1);` |
|        7 |  8428 | `	}else{` |
|        - |  8429 | `		/* Missing arguments,return FALSE */` |
|        3 |  8430 | `		ph7_result_bool(pCtx,0);` |
|        - |  8431 | `	}` |
|       14 |  8432 | `	return rc;` |
|        8 |  8433 |  |
|        - |  8434 | `/*` |
|        - |  8435 | ` * int error_reporting([int $level])` |
|        - |  8436 | ` *  Sets which PHP errors are reported.` |
|        - |  8437 | ` * Parameters` |
|        - |  8438 | ` *  $level` |
|        - |  8439 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  8440 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  8441 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  8442 | ` *   levels will not always behave as expected.` |
|        - |  8443 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  8444 | ` *   in the predefined constants.` |
|        - |  8445 | ` * Return` |
|        - |  8446 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  8447 | ` *   parameter is given.` |
|        - |  8448 | ` */` |
|       18 |  8449 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8450 |  |
|       19 |  8451 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8452 | `	int nOld;` |
|        - |  8453 | `	/* Extract the old reporting level */` |
|       19 |  8454 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       19 |  8455 | `	if( nArg > 0 ){` |
|        - |  8456 | `		int nNew;` |
|        - |  8457 | `		/* Extract the desired error reporting level */` |
|       11 |  8458 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       11 |  8459 | `		if( !nNew ){` |
|        - |  8460 | `			/* Do not report errors at all */` |
|        5 |  8461 | `			pVm->bErrReport = 0;` |
|        3 |  8462 | `		}else{` |
|        - |  8463 | `			/* Report all errors */` |
|        7 |  8464 | `			pVm->bErrReport = 1;` |
|        - |  8465 | `		}` |
|        5 |  8466 | `	}` |
|        - |  8467 | `	/* Return the old level */` |
|       19 |  8468 | `	ph7_result_int(pCtx,nOld);` |
|       19 |  8469 | `	return PH7_OK;` |
|        1 |  8470 |  |
|        - |  8471 | `/*` |
|        - |  8472 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  8473 | ` *  Send an error message somewhere.` |
|        - |  8474 | ` * Parameter` |
|        - |  8475 | ` *  $message` |
|        - |  8476 | ` *   The error message that should be logged.` |
|        - |  8477 | ` *  $message_type` |
|        - |  8478 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  8479 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  8480 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  8481 | ` *       This is the default option.` |
|        - |  8482 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  8483 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  8484 | ` *    2  No longer an option.` |
|        - |  8485 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  8486 | ` *       to the end of the message string.` |
|        - |  8487 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  8488 | ` *  $destination` |
|        - |  8489 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  8490 | ` *  $extra_headers` |
|        - |  8491 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  8492 | ` * Return` |
|        - |  8493 | ` *  TRUE on success or FALSE on failure.` |
|        - |  8494 | ` * NOTE:` |
|        - |  8495 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  8496 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  8497 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  8498 | ` *  Otherwise this function is no-op.` |
|        - |  8499 | ` */` |
|        4 |  8500 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8501 |  |
|        - |  8502 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  8503 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  8504 | `	int iType = 0;` |
|        5 |  8505 | `	if( nArg < 1 ){` |
|        - |  8506 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  8507 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8508 | `		return PH7_OK;` |
|        - |  8509 | `	}` |
|        5 |  8510 | `	if( pVm->xErrLog  ){` |
|        - |  8511 | `		/* Invoke the user callback */` |
|      ! 0 |  8512 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  8513 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  8514 | `		if( nArg > 1 ){` |
|      ! 0 |  8515 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  8516 | `			if( nArg > 2 ){` |
|      ! 0 |  8517 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  8518 | `				if( nArg > 3 ){` |
|      ! 0 |  8519 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  8520 | `				}` |
|      ! 0 |  8521 | `			}` |
|      ! 0 |  8522 | `		}` |
|      ! 0 |  8523 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  8524 | `	}` |
|        - |  8525 | `	/* Retun TRUE */` |
|        5 |  8526 | `	ph7_result_bool(pCtx,1);` |
|        5 |  8527 | `	return PH7_OK;` |
|        3 |  8528 |  |
|        - |  8529 | `/*` |
|        - |  8530 | ` * bool restore_exception_handler(void)` |
|        - |  8531 | ` *  Restores the previously defined exception handler function.` |
|        - |  8532 | ` * Parameter` |
|        - |  8533 | ` *  None` |
|        - |  8534 | ` * Return` |
|        - |  8535 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  8536 | ` */` |
|        4 |  8537 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8538 |  |
|        5 |  8539 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8540 | `	ph7_value *pOld,*pNew;` |
|        - |  8541 | `	/* Point to the old and the new handler */` |
|        5 |  8542 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  8543 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  8544 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8545 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8546 | `		SXUNUSED(apArg);` |
|        - |  8547 | `		/* No installed handler,return FALSE */` |
|        5 |  8548 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8549 | `		return PH7_OK;` |
|        - |  8550 | `	}` |
|        - |  8551 | `	/* Copy the old handler */` |
|      ! 0 |  8552 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8553 | `	PH7_MemObjRelease(pOld);` |
|        - |  8554 | `	/* Return TRUE */` |
|      ! 0 |  8555 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8556 | `	return PH7_OK;` |
|        3 |  8557 |  |
|        - |  8558 | `/*` |
|        - |  8559 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  8560 | ` *  Sets a user-defined exception handler function.` |
|        - |  8561 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  8562 | ` * NOTE` |
|        - |  8563 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  8564 | ` *  the satndard PHP engine.` |
|        - |  8565 | ` * Parameters` |
|        - |  8566 | ` *  $exception_handler` |
|        - |  8567 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  8568 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  8569 | ` *   that was thrown.` |
|        - |  8570 | ` *  Note:` |
|        - |  8571 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8572 | ` * Return` |
|        - |  8573 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  8574 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8575 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8576 | ` */` |
|        4 |  8577 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8578 |  |
|        6 |  8579 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8580 | `	ph7_value *pOld,*pNew;` |
|        - |  8581 | `	/* Point to the old and the new handler */` |
|        6 |  8582 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  8583 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  8584 | `	/* Return the old handler */` |
|        6 |  8585 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  8586 | `	if( nArg > 0 ){` |
|        6 |  8587 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8588 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  8589 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  8590 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  8591 | `		}else{` |
|        6 |  8592 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8593 | `			/* Install the new handler */` |
|        6 |  8594 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8595 | `		}` |
|        2 |  8596 | `	}` |
|        6 |  8597 | `	return PH7_OK;` |
|        2 |  8598 |  |
|        - |  8599 | `/*` |
|        - |  8600 | ` * bool restore_error_handler(void)` |
|        - |  8601 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8602 | ` * Parameters:` |
|        - |  8603 | ` *  None.` |
|        - |  8604 | ` * Return` |
|        - |  8605 | ` *  Always TRUE.` |
|        - |  8606 | ` */` |
|        4 |  8607 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8608 |  |
|        5 |  8609 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8610 | `	ph7_value *pOld,*pNew;` |
|        - |  8611 | `	/* Point to the old and the new handler */` |
|        5 |  8612 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  8613 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  8614 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8615 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8616 | `		SXUNUSED(apArg);` |
|        - |  8617 | `		/* No installed callback,return FALSE */` |
|        5 |  8618 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8619 | `		return PH7_OK;` |
|        - |  8620 | `	}` |
|        - |  8621 | `	/* Copy the old callback */` |
|      ! 0 |  8622 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8623 | `	PH7_MemObjRelease(pOld);` |
|        - |  8624 | `	/* Return TRUE */` |
|      ! 0 |  8625 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8626 | `	return PH7_OK;` |
|        3 |  8627 |  |
|        - |  8628 | `/*` |
|        - |  8629 | ` * value set_error_handler(callable $error_handler)` |
|        - |  8630 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8631 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8632 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8633 | ` *  Sets a user-defined error handler function.` |
|        - |  8634 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  8635 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  8636 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  8637 | ` *  conditions (using trigger_error()).` |
|        - |  8638 | ` * Parameters` |
|        - |  8639 | ` *  $error_handler` |
|        - |  8640 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  8641 | ` *   describing the error.` |
|        - |  8642 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  8643 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  8644 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  8645 | ` *   The function can be shown as:` |
|        - |  8646 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  8647 | ` *     errno` |
|        - |  8648 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  8649 | ` *   errstr` |
|        - |  8650 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  8651 | ` *   errfile` |
|        - |  8652 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  8653 | ` *     was raised in, as a string.` |
|        - |  8654 | ` *  Note:` |
|        - |  8655 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8656 | ` * Return` |
|        - |  8657 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  8658 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8659 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8660 | ` */` |
|     8670 |  8661 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8662 |  |
|     8672 |  8663 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8664 | `	ph7_value *pOld,*pNew;` |
|        - |  8665 | `	/* Point to the old and the new handler */` |
|     8672 |  8666 | `	pOld = &pVm->aErrCB[0];` |
|     8672 |  8667 | `	pNew = &pVm->aErrCB[1];` |
|        - |  8668 | `	/* Return the old handler */` |
|     8672 |  8669 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8672 |  8670 | `	if( nArg > 0 ){` |
|     8672 |  8671 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8672 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4335 |  8673 | `			PH7_MemObjRelease(pNew);` |
|     4335 |  8674 | `			ph7_result_bool(pCtx,1);` |
|     2168 |  8675 | `		}else{` |
|     4338 |  8676 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8677 | `			/* Install the new handler */` |
|     4338 |  8678 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8679 | `		}` |
|     4335 |  8680 | `	}` |
|     8672 |  8681 | `	return PH7_OK;` |
|        2 |  8682 |  |
|        - |  8683 | `/*` |
|        - |  8684 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  8685 | ` *  Generates a backtrace.` |
|        - |  8686 | ` * Paramaeter` |
|        - |  8687 | ` *  $options` |
|        - |  8688 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  8689 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  8690 | ` *   all the function/method arguments, to save memory.` |
|        - |  8691 | ` * $limit` |
|        - |  8692 | ` *   (Not Used)` |
|        - |  8693 | ` * Return` |
|        - |  8694 | ` *  An array.The possible returned elements are as follows:` |
|        - |  8695 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  8696 | ` *          Name        Type      Description` |
|        - |  8697 | ` *          ------      ------     -----------` |
|        - |  8698 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  8699 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  8700 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  8701 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  8702 | ` *          object      object    The current object.` |
|        - |  8703 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  8704 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  8705 | ` */` |
|      376 |  8706 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8707 |  |
|      378 |  8708 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8709 | `	ph7_value *pArray;` |
|        - |  8710 | `	ph7_class *pClass;` |
|        - |  8711 | `	ph7_value *pValue;` |
|        - |  8712 | `	SyString *pFile;` |
|        - |  8713 | `	/* Create a new array */` |
|      378 |  8714 | `	pArray = ph7_context_new_array(pCtx);` |
|      378 |  8715 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      378 |  8716 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8717 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8718 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  8719 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8720 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8721 | `		SXUNUSED(apArg);` |
|      ! 0 |  8722 | `		return PH7_OK;` |
|        - |  8723 | `	}` |
|        - |  8724 | `	/* Dump running function name and it's arguments  */` |
|      378 |  8725 | `	if( pVm->pFrame->pParent ){` |
|      378 |  8726 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8727 | `		ph7_vm_func *pFunc;` |
|        - |  8728 | `		ph7_value *pArg;` |
|      378 |  8729 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8730 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  8731 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  8732 | `		}` |
|      378 |  8733 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      378 |  8734 | `		if( pFrame->pParent && pFunc ){` |
|      378 |  8735 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      378 |  8736 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      378 |  8737 | `			ph7_value_reset_string_cursor(pValue);` |
|      188 |  8738 | `		}` |
|        - |  8739 | `		/* Function arguments */` |
|      378 |  8740 | `		pArg = ph7_context_new_array(pCtx);` |
|      378 |  8741 | `		if( pArg  ){` |
|        - |  8742 | `			ph7_value *pObj;` |
|        - |  8743 | `			VmSlot *aSlot;` |
|        - |  8744 | `			sxu32 n;` |
|        - |  8745 | `			/* Start filling the array with the given arguments */` |
|      378 |  8746 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     1498 |  8747 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1122 |  8748 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1122 |  8749 | `				if( pObj ){` |
|     1122 |  8750 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      560 |  8751 | `				}` |
|      562 |  8752 | `			}` |
|        - |  8753 | `			/* Save the array */` |
|      378 |  8754 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      188 |  8755 | `		}` |
|      188 |  8756 | `	}` |
|      378 |  8757 | `	ph7_value_int(pValue,1);` |
|        - |  8758 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  8759 | `	 * line numbers at run-time. )` |
|        - |  8760 | `	 */` |
|      378 |  8761 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  8762 | `	/* Current processed script */` |
|      378 |  8763 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      378 |  8764 | `	if( pFile ){` |
|      378 |  8765 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      378 |  8766 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      378 |  8767 | `		ph7_value_reset_string_cursor(pValue);` |
|      188 |  8768 | `	}` |
|        - |  8769 | `	/* Top class */` |
|      378 |  8770 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      378 |  8771 | `	if( pClass ){` |
|      374 |  8772 | `		ph7_value_reset_string_cursor(pValue);` |
|      374 |  8773 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      374 |  8774 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      186 |  8775 | `	}` |
|        - |  8776 | `	/* Return the freshly created array */` |
|      378 |  8777 | `	ph7_result_value(pCtx,pArray);` |
|        - |  8778 | `	/*` |
|        - |  8779 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  8780 | `	 * as soon we return from this function.` |
|        - |  8781 | `	 */` |
|      378 |  8782 | `	return PH7_OK;` |
|      190 |  8783 |  |
|        - |  8784 | `/*` |
|        - |  8785 | ` * Generate a small backtrace.` |
|        - |  8786 | ` * Store the generated dump in the given BLOB` |
|        - |  8787 | ` */` |
|        4 |  8788 | `static int VmMiniBacktrace(` |
|        - |  8789 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8790 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  8791 | `	)` |
|        1 |  8792 |  |
|        5 |  8793 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8794 | `	ph7_vm_func *pFunc;` |
|        - |  8795 | `	ph7_class *pClass;` |
|        - |  8796 | `	SyString *pFile;` |
|        - |  8797 | `	/* Called function */` |
|        5 |  8798 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8799 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  8800 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  8801 | `	}` |
|        5 |  8802 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  8803 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8804 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  8805 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  8806 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  8807 | `	}else{` |
|      ! 0 |  8808 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  8809 | `	}` |
|        5 |  8810 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  8811 | `	/* Current processed script */` |
|        5 |  8812 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  8813 | `	if( pFile ){` |
|        5 |  8814 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8815 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  8816 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  8817 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  8818 | `	}` |
|        - |  8819 | `	/* Top class */` |
|        5 |  8820 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  8821 | `	if( pClass ){` |
|      ! 0 |  8822 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  8823 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  8824 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  8825 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  8826 | `	}` |
|        5 |  8827 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  8828 | `	/* All done */` |
|        5 |  8829 | `	return SXRET_OK;` |
|        1 |  8830 |  |
|        - |  8831 | `/*` |
|        - |  8832 | ` * void debug_print_backtrace()` |
|        - |  8833 | ` *  Prints a backtrace` |
|        - |  8834 | ` * Parameters` |
|        - |  8835 | ` * None` |
|        - |  8836 | ` * Return` |
|        - |  8837 | ` * NULL` |
|        - |  8838 | ` */` |
|        2 |  8839 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8840 |  |
|        3 |  8841 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8842 | `	SyBlob sDump;` |
|        3 |  8843 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8844 | `	/* Generate the backtrace */` |
|        3 |  8845 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8846 | `	/* Output backtrace */` |
|        3 |  8847 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8848 | `	/* All done,cleanup */` |
|        3 |  8849 | `	SyBlobRelease(&sDump);` |
|        1 |  8850 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8851 | `	SXUNUSED(apArg);` |
|        3 |  8852 | `	return PH7_OK;` |
|        1 |  8853 |  |
|        - |  8854 | `/*` |
|        - |  8855 | ` * string debug_string_backtrace()` |
|        - |  8856 | ` *  Generate a backtrace` |
|        - |  8857 | ` * Parameters` |
|        - |  8858 | ` * None` |
|        - |  8859 | ` * Return` |
|        - |  8860 | ` *  A mini backtrace().` |
|        - |  8861 | ` * Note that this is a symisc extension.` |
|        - |  8862 | ` */` |
|        2 |  8863 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8864 |  |
|        3 |  8865 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8866 | `	SyBlob sDump;` |
|        3 |  8867 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8868 | `	/* Generate the backtrace */` |
|        3 |  8869 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8870 | `	/* Return the backtrace */` |
|        3 |  8871 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  8872 | `	/* All done,cleanup */` |
|        3 |  8873 | `	SyBlobRelease(&sDump);` |
|        1 |  8874 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8875 | `	SXUNUSED(apArg);` |
|        3 |  8876 | `	return PH7_OK;` |
|        1 |  8877 |  |
|        - |  8878 | `/*` |
|        - |  8879 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  8880 | ` * exception is triggered.` |
|        - |  8881 | ` */` |
|      360 |  8882 | `static sxi32 VmUncaughtException(` |
|        - |  8883 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8884 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8885 | `	)` |
|        1 |  8886 |  |
|        - |  8887 | `	ph7_value *apArg[2],sArg;` |
|      361 |  8888 | `	int nArg = 1;` |
|        - |  8889 | `	sxi32 rc;` |
|      361 |  8890 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  8891 | `		/* Nesting limit reached */` |
|      ! 0 |  8892 | `		return SXRET_OK;` |
|        - |  8893 | `	}` |
|        - |  8894 | `	/* Call any exception handler if available */` |
|      361 |  8895 | `	PH7_MemObjInit(pVm,&sArg);` |
|      361 |  8896 | `	if( pThis ){` |
|        - |  8897 | `		/* Load the exception instance */` |
|      361 |  8898 | `		sArg.x.pOther = pThis;` |
|      361 |  8899 | `		pThis->iRef++;` |
|      361 |  8900 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      181 |  8901 | `	}else{` |
|      ! 0 |  8902 | `		nArg = 0;` |
|        - |  8903 | `	}` |
|      361 |  8904 | `	apArg[0] = &sArg;` |
|        - |  8905 | `	/* Call the exception handler if available */` |
|      361 |  8906 | `	pVm->nExceptDepth++;` |
|      361 |  8907 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      361 |  8908 | `	pVm->nExceptDepth--;` |
|      361 |  8909 | `	if( rc != SXRET_OK ){` |
|        - |  8910 | `		SyBlob sMsgBuf;` |
|      359 |  8911 | `		const char *zClass = "Exception";` |
|      359 |  8912 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  8913 | `		const char *zMsg;` |
|        - |  8914 | `		sxu32 nMsg;` |
|        - |  8915 | `		const char *zFuncName;` |
|        - |  8916 | `		int nFuncLen;` |
|      359 |  8917 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      359 |  8918 | `		if( pThis ){` |
|        - |  8919 | `			ph7_class_method *pGetMessage;` |
|        - |  8920 | `			ph7_value sMsg;` |
|        - |  8921 | `			const char *zTmp;` |
|        - |  8922 | `			int nTmp;` |
|      359 |  8923 | `			zClass = pThis->pClass->sName.zString;` |
|      359 |  8924 | `			nClass = pThis->pClass->sName.nByte;` |
|      359 |  8925 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      359 |  8926 | `			if( pGetMessage ){` |
|      359 |  8927 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      359 |  8928 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      359 |  8929 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      359 |  8930 | `					if( zTmp && nTmp > 0 ){` |
|      359 |  8931 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      179 |  8932 | `					}` |
|      179 |  8933 | `				}` |
|      359 |  8934 | `				PH7_MemObjRelease(&sMsg);` |
|      179 |  8935 | `			}` |
|      179 |  8936 | `		}` |
|      359 |  8937 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  8938 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  8939 | `		}` |
|      359 |  8940 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      359 |  8941 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      359 |  8942 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      359 |  8943 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      359 |  8944 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  8945 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      359 |  8946 | `		rc = SXERR_ABORT;` |
|      179 |  8947 | `	}` |
|      361 |  8948 | `	PH7_MemObjRelease(&sArg);` |
|      361 |  8949 | `	return rc;` |
|      181 |  8950 |  |
|        - |  8951 | `/*` |
|        - |  8952 | ` * Throw an user exception.` |
|        - |  8953 | ` */` |
|      374 |  8954 | `static sxi32 VmThrowException(` |
|        - |  8955 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  8956 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8957 | `	)` |
|        2 |  8958 |  |
|        - |  8959 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  8960 | `	ph7_exception **apException;` |
|        - |  8961 | `	ph7_exception *pException;` |
|        - |  8962 | `	/* Point to the stack of loaded exceptions */` |
|      376 |  8963 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      376 |  8964 | `	pException = 0;` |
|      376 |  8965 | `	pCatch = 0;` |
|      376 |  8966 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8967 | `		ph7_exception_block *aCatch;` |
|        - |  8968 | `		ph7_class *pClass;` |
|        - |  8969 | `		sxu32 j;` |
|        - |  8970 | `		/* Locate the appropriate block to execute */` |
|       16 |  8971 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       16 |  8972 | `		(void)SySetPop(&pVm->aException);` |
|       16 |  8973 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       16 |  8974 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       16 |  8975 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  8976 | `			/* Extract the target class */` |
|       16 |  8977 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       16 |  8978 | `			if( pClass == 0 ){` |
|        - |  8979 | `				/* No such class */` |
|      ! 0 |  8980 | `				continue;` |
|        - |  8981 | `			}` |
|       16 |  8982 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  8983 | `				/* Catch block found,break immeditaley */` |
|       16 |  8984 | `				pCatch = &aCatch[j];` |
|       16 |  8985 | `				break;` |
|        - |  8986 | `			}` |
|      ! 0 |  8987 | `		}` |
|        7 |  8988 | `	}` |
|        - |  8989 | `	/* Execute the cached block if available */` |
|      376 |  8990 | `	if( pCatch == 0 ){` |
|        - |  8991 | `		sxi32 rc;` |
|      361 |  8992 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      361 |  8993 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  8994 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  8995 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8996 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  8997 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  8998 | `			}` |
|      ! 0 |  8999 | `			if( pException->pFrame == pFrame ){` |
|        - |  9000 | `				/* Tell the upper layer that the exception was caught */` |
|      ! 0 |  9001 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  9002 | `			}` |
|      ! 0 |  9003 | `		}` |
|      361 |  9004 | `		return rc;` |
|      ! 0 |  9005 | `	}else{` |
|       16 |  9006 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  9007 | `		sxi32 rc;` |
|       24 |  9008 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9009 | `			/* Safely ignore the exception frame */` |
|       10 |  9010 | `			pFrame = pFrame->pParent;` |
|        2 |  9011 | `		}` |
|       16 |  9012 | `		if( pException->pFrame == pFrame ){` |
|        - |  9013 | `			/* Tell the upper layer that the exception was caught */` |
|        8 |  9014 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        3 |  9015 | `		}` |
|        - |  9016 | `		/* Create a private frame first */` |
|       16 |  9017 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       16 |  9018 | `		if( rc == SXRET_OK ){` |
|        - |  9019 | `			/* Mark as catch frame */` |
|       16 |  9020 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       16 |  9021 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       16 |  9022 | `			if( pObj ){` |
|        - |  9023 | `				/* Install the exception instance */` |
|       16 |  9024 | `				pThis->iRef++; /* Increment reference count */` |
|       16 |  9025 | `				pObj->x.pOther = pThis;` |
|       16 |  9026 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        7 |  9027 | `			}` |
|        - |  9028 | `			/* Exceute the block */` |
|       16 |  9029 | `			VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  9030 | `			/* Leave the frame */` |
|       16 |  9031 | `			VmLeaveFrame(&(*pVm));` |
|        7 |  9032 | `		}` |
|        - |  9033 | `	}` |
|        - |  9034 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  9035 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  9036 | `	 */` |
|       16 |  9037 | `	return SXRET_OK;` |
|      189 |  9038 |  |
|        - |  9039 | `/*` |
|        - |  9040 | ` * Section:` |
|        - |  9041 | ` *  Version,Credits and Copyright related functions.` |
|        - |  9042 | ` * Status:` |
|        - |  9043 | ` *    Stable.` |
|        - |  9044 | ` */` |
|        - |  9045 | `/*` |
|        - |  9046 | ` * string ph7version(void)` |
|        - |  9047 | ` *  Returns the running version of the PH7 version.` |
|        - |  9048 | ` * Parameters` |
|        - |  9049 | ` *  None` |
|        - |  9050 | ` * Return` |
|        - |  9051 | ` * Current PH7 version.` |
|        - |  9052 | ` */` |
|        2 |  9053 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9054 |  |
|        1 |  9055 | `	SXUNUSED(nArg);` |
|        1 |  9056 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  9057 | `	/* Current engine version */` |
|        3 |  9058 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  9059 | `	return PH7_OK;` |
|        1 |  9060 |  |
|        - |  9061 | `/*` |
|        - |  9062 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  9063 | ` */` |
|        - |  9064 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  9065 | ` "<html><head>"\` |
|        - |  9066 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  9067 | ` "<style type=\"text/css\">"\` |
|        - |  9068 | ` "div {"\` |
|        - |  9069 | `     "border: 1px solid #cccccc;"\` |
|        - |  9070 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  9071 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  9072 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  9073 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  9074 | `     "-webkit-border-radius: 10px;"\` |
|        - |  9075 | `     "-o-border-radius: 10px;"\` |
|        - |  9076 | `     "border-radius: 10px;"\` |
|        - |  9077 | `     "padding-left: 2em;"\` |
|        - |  9078 | `     "background-color: white;"\` |
|        - |  9079 | `     "margin-left: auto;"\` |
|        - |  9080 | `     "font-family: verdana;"\` |
|        - |  9081 | `     "padding-right: 2em;"\` |
|        - |  9082 | `     "margin-right: auto;"\` |
|        - |  9083 | `     "}"\` |
|        - |  9084 | `     "body {"\` |
|        - |  9085 | `     "padding: 0.2em;"\` |
|        - |  9086 | `     "font-style: normal;"\` |
|        - |  9087 | `     "font-size: medium;"\` |
|        - |  9088 | `     "background-color: #f2f2f2;"\` |
|        - |  9089 | `     "}"\` |
|        - |  9090 | `     "hr {"\` |
|        - |  9091 | `     "border-style: solid none none;"\` |
|        - |  9092 | `     "border-width: 1px medium medium;"\` |
|        - |  9093 | `     "border-top: 1px solid #cccccc;"\` |
|        - |  9094 | `     "height: 1px;"\` |
|        - |  9095 | `     "}"\` |
|        - |  9096 | `     "a {"\` |
|        - |  9097 | `     "color: #3366cc;"\` |
|        - |  9098 | `     "text-decoration: none;"\` |
|        - |  9099 | `     "}"\` |
|        - |  9100 | `     "a:hover {"\` |
|        - |  9101 | `     "color: #999999;"\` |
|        - |  9102 | `     "}"\` |
|        - |  9103 | `     "a:active {"\` |
|        - |  9104 | `     "color: #663399;"\` |
|        - |  9105 | `     "}"\` |
|        - |  9106 | `     "h1 {"\` |
|        - |  9107 | `     "margin: 0;"\` |
|        - |  9108 | `     "padding: 0;"\` |
|        - |  9109 | `     "font-family: Verdana;"\` |
|        - |  9110 | `     "font-weight: bold;"\` |
|        - |  9111 | `     "font-style: normal;"\` |
|        - |  9112 | `     "font-size: medium;"\` |
|        - |  9113 | `     "text-transform: capitalize;"\` |
|        - |  9114 | `     "color: #0a328c;"\` |
|        - |  9115 | `     "}"\` |
|        - |  9116 | `     "p {"\` |
|        - |  9117 | `     "margin: 0 auto;"\` |
|        - |  9118 | `     "font-size: medium;"\` |
|        - |  9119 | `     "font-style: normal;"\` |
|        - |  9120 | `     "font-family: verdana;"\` |
|        - |  9121 | `     "}"\` |
|        - |  9122 | `"</style></head><body>"\` |
|        - |  9123 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - |  9124 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - |  9125 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - |  9126 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - |  9127 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - |  9128 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - |  9129 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - |  9130 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - |  9131 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - |  9132 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - |  9133 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - |  9134 |  |
|        - |  9135 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9136 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - |  9137 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - |  9138 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - |  9139 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9140 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - |  9141 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  9142 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - |  9143 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  9144 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - |  9145 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9146 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - |  9147 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - |  9148 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - |  9149 |  |
|        - |  9150 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - |  9151 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - |  9152 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - |  9153 | `"&nbsp;*<br>"\` |
|        - |  9154 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - |  9155 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - |  9156 | `"&nbsp;* are met:<br>"\` |
|        - |  9157 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - |  9158 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - |  9159 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - |  9160 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - |  9161 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - |  9162 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - |  9163 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - |  9164 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - |  9165 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - |  9166 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - |  9167 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - |  9168 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - |  9169 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - |  9170 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - |  9171 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - |  9172 | `"&nbsp;*<br>"\` |
|        - |  9173 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - |  9174 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - |  9175 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - |  9176 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - |  9177 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - |  9178 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - |  9179 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - |  9180 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - |  9181 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - |  9182 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - |  9183 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - |  9184 | `"&nbsp;*/<br>"\` |
|        - |  9185 | `"</span></small></small></p>"\` |
|        - |  9186 | `"</div></body></html>"` |
|        - |  9187 | `/*` |
|        - |  9188 | ` * bool ph7credits(void)` |
|        - |  9189 | ` * bool ph7info(void)` |
|        - |  9190 | ` * bool ph7copyright(void)` |
|        - |  9191 | ` *  Prints out the credits for PH7 engine` |
|        - |  9192 | ` * Parameters` |
|        - |  9193 | ` *  None` |
|        - |  9194 | ` * Return` |
|        - |  9195 | ` *  Always TRUE` |
|        - |  9196 | ` */` |
|        2 |  9197 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9198 |  |
|        3 |  9199 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - |  9200 | `	/* Expand the HTML page above*/` |
|        3 |  9201 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 |  9202 | `	ph7_context_output_format(` |
|        1 |  9203 | `		pCtx,` |
|        - |  9204 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 |  9205 | `		ph7_lib_version(),   /* Engine version */` |
|        1 |  9206 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 |  9207 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 |  9208 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 |  9209 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 |  9210 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - |  9211 | `#ifdef __WINNT__` |
|        - |  9212 | `		"Windows NT"` |
|        - |  9213 | `#elif defined(__UNIXES__)` |
|        - |  9214 | `		"UNIX-Like"` |
|        - |  9215 | `#else` |
|        - |  9216 | `		"Other OS"` |
|        - |  9217 | `#endif` |
|        - |  9218 | `		);` |
|        3 |  9219 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 |  9220 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9221 | `	SXUNUSED(apArg);` |
|        - |  9222 | `	/* Return TRUE */` |
|        - |  9223 | `	//ph7_result_bool(pCtx,1);` |
|        3 |  9224 | `	return PH7_OK;` |
|        1 |  9225 |  |
|        - |  9226 | `/*` |
|        - |  9227 | ` * Section:` |
|        - |  9228 | ` *    URL related routines.` |
|        - |  9229 | ` * Status:` |
|        - |  9230 | ` *    Stable.` |
|        - |  9231 | ` */` |
|        - |  9232 | `/*` |
|        - |  9233 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - |  9234 | ` *  Parse a URL and return its fields.` |
|        - |  9235 | ` * Parameters` |
|        - |  9236 | ` *  $url` |
|        - |  9237 | ` *   The URL to parse.` |
|        - |  9238 | ` * $component` |
|        - |  9239 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - |  9240 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - |  9241 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - |  9242 | ` *  in which case the return value will be an integer).` |
|        - |  9243 | ` * Return` |
|        - |  9244 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - |  9245 | ` *  At least one element will be present within the array. Potential keys within` |
|        - |  9246 | ` *  this array are:` |
|        - |  9247 | ` *   scheme - e.g. http` |
|        - |  9248 | ` *   host` |
|        - |  9249 | ` *   port` |
|        - |  9250 | ` *   user` |
|        - |  9251 | ` *   pass` |
|        - |  9252 | ` *   path` |
|        - |  9253 | ` *   query - after the question mark ?` |
|        - |  9254 | ` *   fragment - after the hashmark #` |
|        - |  9255 | ` * Note:` |
|        - |  9256 | ` *  FALSE is returned on failure.` |
|        - |  9257 | ` *  This function work with relative URL unlike the one shipped` |
|        - |  9258 | ` *  with the standard PHP engine.` |
|        - |  9259 | ` */` |
|       28 |  9260 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9261 |  |
|        - |  9262 | `	const char *zStr; /* Input string */` |
|        - |  9263 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - |  9264 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - |  9265 | `	int nLen;` |
|        - |  9266 | `	sxi32 rc;` |
|       29 |  9267 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  9268 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  9269 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9270 | `		return PH7_OK;` |
|        - |  9271 | `	}` |
|        - |  9272 | `	/* Extract the given URI */` |
|       29 |  9273 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 |  9274 | `	if( nLen < 1 ){` |
|        - |  9275 | `		/* Nothing to process,return FALSE */` |
|        3 |  9276 | `		ph7_result_bool(pCtx,0);` |
|        3 |  9277 | `		return PH7_OK;` |
|        - |  9278 | `	}` |
|        - |  9279 | `	/* Get a parse */` |
|       27 |  9280 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 |  9281 | `	if( rc != SXRET_OK ){` |
|        - |  9282 | `		/* Malformed input,return FALSE */` |
|      ! 0 |  9283 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9284 | `		return PH7_OK;` |
|        - |  9285 | `	}` |
|       27 |  9286 | `	if( nArg > 1 ){` |
|      ! 0 |  9287 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - |  9288 | `		/* Refer to constant.c for constants values */` |
|      ! 0 |  9289 | `		switch(nComponent){` |
|      ! 0 |  9290 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 |  9291 | `			pComp = &sURI.sScheme;` |
|      ! 0 |  9292 | `			if( pComp->nByte < 1 ){` |
|        - |  9293 | `				/* No available value,return NULL */` |
|      ! 0 |  9294 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9295 | `			}else{` |
|      ! 0 |  9296 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9297 | `			}` |
|      ! 0 |  9298 | `			break;` |
|      ! 0 |  9299 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 |  9300 | `			pComp = &sURI.sHost;` |
|      ! 0 |  9301 | `			if( pComp->nByte < 1 ){` |
|        - |  9302 | `				/* No available value,return NULL */` |
|      ! 0 |  9303 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9304 | `			}else{` |
|      ! 0 |  9305 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9306 | `			}` |
|      ! 0 |  9307 | `			break;` |
|      ! 0 |  9308 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 |  9309 | `			pComp = &sURI.sPort;` |
|      ! 0 |  9310 | `			if( pComp->nByte < 1 ){` |
|        - |  9311 | `				/* No available value,return NULL */` |
|      ! 0 |  9312 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9313 | `			}else{` |
|      ! 0 |  9314 | `				int iPort = 0;` |
|        - |  9315 | `				/* Cast the value to integer */` |
|      ! 0 |  9316 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 |  9317 | `				ph7_result_int(pCtx,iPort);` |
|        - |  9318 | `			}` |
|      ! 0 |  9319 | `			break;` |
|      ! 0 |  9320 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 |  9321 | `			pComp = &sURI.sUser;` |
|      ! 0 |  9322 | `			if( pComp->nByte < 1 ){` |
|        - |  9323 | `				/* No available value,return NULL */` |
|      ! 0 |  9324 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9325 | `			}else{` |
|      ! 0 |  9326 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9327 | `			}` |
|      ! 0 |  9328 | `			break;` |
|      ! 0 |  9329 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 |  9330 | `			pComp = &sURI.sPass;` |
|      ! 0 |  9331 | `			if( pComp->nByte < 1 ){` |
|        - |  9332 | `				/* No available value,return NULL */` |
|      ! 0 |  9333 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9334 | `			}else{` |
|      ! 0 |  9335 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9336 | `			}` |
|      ! 0 |  9337 | `			break;` |
|      ! 0 |  9338 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 |  9339 | `			pComp = &sURI.sQuery;` |
|      ! 0 |  9340 | `			if( pComp->nByte < 1 ){` |
|        - |  9341 | `				/* No available value,return NULL */` |
|      ! 0 |  9342 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9343 | `			}else{` |
|      ! 0 |  9344 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9345 | `			}` |
|      ! 0 |  9346 | `			break;` |
|      ! 0 |  9347 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 |  9348 | `			pComp = &sURI.sFragment;` |
|      ! 0 |  9349 | `			if( pComp->nByte < 1 ){` |
|        - |  9350 | `				/* No available value,return NULL */` |
|      ! 0 |  9351 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9352 | `			}else{` |
|      ! 0 |  9353 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9354 | `			}` |
|      ! 0 |  9355 | `			break;` |
|      ! 0 |  9356 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 |  9357 | `			pComp = &sURI.sPath;` |
|      ! 0 |  9358 | `			if( pComp->nByte < 1 ){` |
|        - |  9359 | `				/* No available value,return NULL */` |
|      ! 0 |  9360 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9361 | `			}else{` |
|      ! 0 |  9362 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9363 | `			}` |
|      ! 0 |  9364 | `			break;` |
|      ! 0 |  9365 | `		default:` |
|        - |  9366 | `			/* No such entry,return NULL */` |
|      ! 0 |  9367 | `			ph7_result_null(pCtx);` |
|      ! 0 |  9368 | `			break;` |
|        - |  9369 | `		}` |
|      ! 0 |  9370 | `	}else{` |
|        - |  9371 | `		ph7_value *pArray,*pValue;` |
|        - |  9372 | `		/* Return an associative array */` |
|       27 |  9373 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 |  9374 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 |  9375 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9376 | `			/* Out of memory */` |
|      ! 0 |  9377 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9378 | `			/* Return false */` |
|      ! 0 |  9379 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |  9380 | `			return PH7_OK;` |
|        - |  9381 | `		}` |
|        - |  9382 | `		/* Fill the array */` |
|       27 |  9383 | `		pComp = &sURI.sScheme;` |
|       27 |  9384 | `		if( pComp->nByte > 0 ){` |
|       19 |  9385 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 |  9386 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 |  9387 | `		}` |
|        - |  9388 | `		/* Reset the string cursor */` |
|       27 |  9389 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9390 | `		pComp = &sURI.sHost;` |
|       27 |  9391 | `		if( pComp->nByte > 0 ){` |
|       25 |  9392 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 |  9393 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 |  9394 | `		}` |
|        - |  9395 | `		/* Reset the string cursor */` |
|       27 |  9396 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9397 | `		pComp = &sURI.sPort;` |
|       27 |  9398 | `		if( pComp->nByte > 0 ){` |
|       11 |  9399 | `			int iPort = 0;/* cc warning */` |
|        - |  9400 | `			/* Convert to integer */` |
|       11 |  9401 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 |  9402 | `			ph7_value_int(pValue,iPort);` |
|       11 |  9403 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 |  9404 | `		}` |
|        - |  9405 | `		/* Reset the string cursor */` |
|       27 |  9406 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9407 | `		pComp = &sURI.sUser;` |
|       27 |  9408 | `		if( pComp->nByte > 0 ){` |
|        7 |  9409 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9410 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 |  9411 | `		}` |
|        - |  9412 | `		/* Reset the string cursor */` |
|       27 |  9413 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9414 | `		pComp = &sURI.sPass;` |
|       27 |  9415 | `		if( pComp->nByte > 0 ){` |
|        7 |  9416 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9417 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 |  9418 | `		}` |
|        - |  9419 | `		/* Reset the string cursor */` |
|       27 |  9420 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9421 | `		pComp = &sURI.sPath;` |
|       27 |  9422 | `		if( pComp->nByte > 0 ){` |
|       17 |  9423 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 |  9424 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 |  9425 | `		}` |
|        - |  9426 | `		/* Reset the string cursor */` |
|       27 |  9427 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9428 | `		pComp = &sURI.sQuery;` |
|       27 |  9429 | `		if( pComp->nByte > 0 ){` |
|        5 |  9430 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9431 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 |  9432 | `		}` |
|        - |  9433 | `		/* Reset the string cursor */` |
|       27 |  9434 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9435 | `		pComp = &sURI.sFragment;` |
|       27 |  9436 | `		if( pComp->nByte > 0 ){` |
|        5 |  9437 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9438 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 |  9439 | `		}` |
|        - |  9440 | `		/* Return the created array */` |
|       27 |  9441 | `		ph7_result_value(pCtx,pArray);` |
|        - |  9442 | `		/* NOTE:` |
|        - |  9443 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - |  9444 | `		 * automatically as soon we return from this function.` |
|        - |  9445 | `		 */` |
|        - |  9446 | `	}` |
|        - |  9447 | `	/* All done */` |
|       27 |  9448 | `	return PH7_OK;` |
|       15 |  9449 |  |
|        - |  9450 | `/*` |
|        - |  9451 | ` * Section:` |
|        - |  9452 | ` *   Array related routines.` |
|        - |  9453 | ` * Status:` |
|        - |  9454 | ` *    Stable.` |
|        - |  9455 | ` * Note 2012-5-21 01:04:15:` |
|        - |  9456 | ` *  Array related functions that need access to the underlying` |
|        - |  9457 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - |  9458 | ` */` |
|        - |  9459 | `/*` |
|        - |  9460 | ` * The [compact()] function store it's state information in an instance` |
|        - |  9461 | ` * of the following structure.` |
|        - |  9462 | ` */` |
|        - |  9463 | `struct compact_data` |
|        - |  9464 |  |
|        - |  9465 | `	ph7_value *pArray;  /* Target array */` |
|        - |  9466 | `	int nRecCount;      /* Recursion count */` |
|        - |  9467 | `};` |
|        - |  9468 | `/*` |
|        - |  9469 | ` * Walker callback for the [compact()] function defined below.` |
|        - |  9470 | ` */` |
|      ! 0 |  9471 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 |  9472 |  |
|      ! 0 |  9473 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 |  9474 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 |  9475 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9476 | `	/* Act according to the hashmap value */` |
|      ! 0 |  9477 | `	if( ph7_value_is_string(pValue) ){` |
|        - |  9478 | `		SyString sVar;` |
|      ! 0 |  9479 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 |  9480 | `		if( sVar.nByte > 0 ){` |
|        - |  9481 | `			/* Query the current frame */` |
|      ! 0 |  9482 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - |  9483 | `			/* ^` |
|        - |  9484 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - |  9485 | `			 */` |
|      ! 0 |  9486 | `			if( pKey ){` |
|        - |  9487 | `				/* Perform the insertion */` |
|      ! 0 |  9488 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 |  9489 | `			}` |
|      ! 0 |  9490 | `		}` |
|      ! 0 |  9491 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - |  9492 | `		int rc;` |
|        - |  9493 | `		/* Recursively traverse this array */` |
|      ! 0 |  9494 | `		pData->nRecCount++;` |
|      ! 0 |  9495 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 |  9496 | `		pData->nRecCount--;` |
|      ! 0 |  9497 | `		return rc;` |
|        - |  9498 | `	}` |
|      ! 0 |  9499 | `	return SXRET_OK;` |
|      ! 0 |  9500 |  |
|        - |  9501 | `/*` |
|        - |  9502 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - |  9503 | ` *  Create array containing variables and their values.` |
|        - |  9504 | ` *  For each of these, compact() looks for a variable with that name` |
|        - |  9505 | ` *  in the current symbol table and adds it to the output array such` |
|        - |  9506 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - |  9507 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - |  9508 | ` *  Any strings that are not set will simply be skipped.` |
|        - |  9509 | ` * Parameters` |
|        - |  9510 | ` *  $varname` |
|        - |  9511 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - |  9512 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - |  9513 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - |  9514 | ` *   it recursively.` |
|        - |  9515 | ` * Return` |
|        - |  9516 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - |  9517 | ` */` |
|        2 |  9518 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9519 |  |
|        - |  9520 | `	ph7_value *pArray,*pObj;` |
|        3 |  9521 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9522 | `	const char *zName;` |
|        - |  9523 | `	SyString sVar;` |
|        - |  9524 | `	int i,nLen;` |
|        3 |  9525 | `	if( nArg < 1 ){` |
|        - |  9526 | `		/* Missing arguments,return NULL */` |
|      ! 0 |  9527 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9528 | `		return PH7_OK;` |
|        - |  9529 | `	}` |
|        - |  9530 | `	/* Create the array */` |
|        3 |  9531 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9532 | `	if( pArray == 0 ){` |
|        - |  9533 | `		/* Out of memory */` |
|      ! 0 |  9534 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9535 | `		/* Return NULL */` |
|      ! 0 |  9536 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9537 | `		return PH7_OK;` |
|        - |  9538 | `	}` |
|        - |  9539 | `	/* Perform the requested operation */` |
|        7 |  9540 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 |  9541 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 |  9542 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - |  9543 | `				struct compact_data sData;` |
|      ! 0 |  9544 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - |  9545 | `				/* Recursively walk the array */` |
|      ! 0 |  9546 | `				sData.nRecCount = 0;` |
|      ! 0 |  9547 | `				sData.pArray = pArray;` |
|      ! 0 |  9548 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 |  9549 | `			}` |
|      ! 0 |  9550 | `		}else{` |
|        - |  9551 | `			/* Extract variable name */` |
|        5 |  9552 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 |  9553 | `			if( nLen > 0 ){` |
|        5 |  9554 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - |  9555 | `				/* Check if the variable is available in the current frame */` |
|        5 |  9556 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 |  9557 | `				if( pObj ){` |
|        5 |  9558 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 |  9559 | `				}` |
|        2 |  9560 | `			}` |
|        - |  9561 | `		}` |
|        3 |  9562 | `	}` |
|        - |  9563 | `	/* Return the array */` |
|        3 |  9564 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9565 | `	return PH7_OK;` |
|        2 |  9566 |  |
|        - |  9567 | `/*` |
|        - |  9568 | ` * The [extract()] function store it's state information in an instance` |
|        - |  9569 | ` * of the following structure.` |
|        - |  9570 | ` */` |
|        - |  9571 | `typedef struct extract_aux_data extract_aux_data;` |
|        - |  9572 | `struct extract_aux_data` |
|        - |  9573 |  |
|        - |  9574 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - |  9575 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - |  9576 | `	const char *zPrefix;  /* Prefix name */` |
|        - |  9577 | `	int Prefixlen;        /* Prefix  length */` |
|        - |  9578 | `	int iFlags;           /* Control flags */` |
|        - |  9579 | `	char zWorker[1024];   /* Working buffer */` |
|        - |  9580 | `};` |
|        - |  9581 | `/* Forward declaration */` |
|        - |  9582 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - |  9583 | `/*` |
|        - |  9584 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - |  9585 | ` *   Import variables into the current symbol table from an array.` |
|        - |  9586 | ` * Parameters` |
|        - |  9587 | ` * $var_array` |
|        - |  9588 | ` *  An associative array. This function treats keys as variable names and values` |
|        - |  9589 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - |  9590 | ` *  table, subject to extract_type and prefix parameters.` |
|        - |  9591 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - |  9592 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - |  9593 | ` * $extract_type` |
|        - |  9594 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - |  9595 | ` *  It can be one of the following values:` |
|        - |  9596 | ` *   EXTR_OVERWRITE` |
|        - |  9597 | ` *       If there is a collision, overwrite the existing variable.` |
|        - |  9598 | ` *   EXTR_SKIP` |
|        - |  9599 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - |  9600 | ` *   EXTR_PREFIX_SAME` |
|        - |  9601 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - |  9602 | ` *   EXTR_PREFIX_ALL` |
|        - |  9603 | ` *       Prefix all variable names with prefix.` |
|        - |  9604 | ` *   EXTR_PREFIX_INVALID` |
|        - |  9605 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - |  9606 | ` *   EXTR_IF_EXISTS` |
|        - |  9607 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - |  9608 | ` *       otherwise do nothing.` |
|        - |  9609 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - |  9610 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - |  9611 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - |  9612 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - |  9613 | ` *      the current symbol table.` |
|        - |  9614 | ` * $prefix` |
|        - |  9615 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - |  9616 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - |  9617 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - |  9618 | ` *  underscore character.` |
|        - |  9619 | ` * Return` |
|        - |  9620 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - |  9621 | ` */` |
|        4 |  9622 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9623 |  |
|        - |  9624 | `	extract_aux_data sAux;` |
|        - |  9625 | `	ph7_hashmap *pMap;` |
|        5 |  9626 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - |  9627 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 |  9628 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9629 | `		return PH7_OK;` |
|        - |  9630 | `	}` |
|        - |  9631 | `	/* Point to the target hashmap */` |
|        5 |  9632 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 |  9633 | `	if( pMap->nEntry < 1 ){` |
|        - |  9634 | `		/* Empty map,return  0 */` |
|      ! 0 |  9635 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9636 | `		return PH7_OK;` |
|        - |  9637 | `	}` |
|        - |  9638 | `	/* Prepare the aux data */` |
|        5 |  9639 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 |  9640 | `	if( nArg > 1 ){` |
|        3 |  9641 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 |  9642 | `		if( nArg > 2 ){` |
|      ! 0 |  9643 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 |  9644 | `		}` |
|        1 |  9645 | `	}` |
|        5 |  9646 | `	sAux.pVm = pCtx->pVm;` |
|        - |  9647 | `	/* Invoke the worker callback */` |
|        5 |  9648 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - |  9649 | `	/* Number of variables successfully imported */` |
|        5 |  9650 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 |  9651 | `	return PH7_OK;` |
|        3 |  9652 |  |
|        - |  9653 | `/*` |
|        - |  9654 | ` * Worker callback for the [extract()] function defined` |
|        - |  9655 | ` * below.` |
|        - |  9656 | ` */` |
|        8 |  9657 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9658 |  |
|        9 |  9659 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 |  9660 | `	int iFlags = pAux->iFlags;` |
|        9 |  9661 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9662 | `	ph7_value *pObj;` |
|        - |  9663 | `	SyString sVar;` |
|        9 |  9664 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 |  9665 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 |  9666 | `	}` |
|        - |  9667 | `	/* Perform a string cast */` |
|        9 |  9668 | `	PH7_MemObjToString(pKey);` |
|        9 |  9669 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9670 | `		/* Unavailable variable name */` |
|      ! 0 |  9671 | `		return SXRET_OK;` |
|        - |  9672 | `	}` |
|        9 |  9673 | `	sVar.nByte = 0; /* cc warning */` |
|        9 |  9674 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 |  9675 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9676 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9677 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9678 | `			);` |
|      ! 0 |  9679 | `	}else{` |
|       13 |  9680 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 |  9681 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9682 | `	}` |
|        9 |  9683 | `	sVar.zString = pAux->zWorker;` |
|        - |  9684 | `	/* Try to extract the variable */` |
|        9 |  9685 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 |  9686 | `	if( pObj ){` |
|        - |  9687 | `		/* Collision */` |
|        5 |  9688 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 |  9689 | `			return SXRET_OK;` |
|        - |  9690 | `		}` |
|        5 |  9691 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 |  9692 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - |  9693 | `				/* Already prefixed */` |
|      ! 0 |  9694 | `				return SXRET_OK;` |
|        - |  9695 | `			}` |
|      ! 0 |  9696 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9697 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9698 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9699 | `				);` |
|      ! 0 |  9700 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 |  9701 | `		}` |
|        3 |  9702 | `	}else{` |
|        - |  9703 | `		/* Create the variable */` |
|        5 |  9704 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - |  9705 | `	}` |
|        9 |  9706 | `	if( pObj ){` |
|        - |  9707 | `		/* Overwrite the old value */` |
|        9 |  9708 | `		PH7_MemObjStore(pValue,pObj);` |
|        - |  9709 | `		/* Increment counter */` |
|        9 |  9710 | `		pAux->iCount++;` |
|        4 |  9711 | `	}` |
|        9 |  9712 | `	return SXRET_OK;` |
|        5 |  9713 |  |
|        - |  9714 | `/*` |
|        - |  9715 | ` * Worker callback for the [import_request_variables()] function` |
|        - |  9716 | ` * defined below.` |
|        - |  9717 | ` */` |
|        2 |  9718 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9719 |  |
|        3 |  9720 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 |  9721 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9722 | `	ph7_value *pObj;` |
|        - |  9723 | `	SyString sVar;` |
|        - |  9724 | `	/* Perform a string cast */` |
|        3 |  9725 | `	PH7_MemObjToString(pKey);` |
|        3 |  9726 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9727 | `		/* Unavailable variable name */` |
|      ! 0 |  9728 | `		return SXRET_OK;` |
|        - |  9729 | `	}` |
|        3 |  9730 | `	sVar.nByte = 0; /* cc warning */` |
|        3 |  9731 | `	if( pAux->Prefixlen > 0 ){` |
|        4 |  9732 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 |  9733 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 |  9734 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9735 | `			);` |
|        2 |  9736 | `	}else{` |
|      ! 0 |  9737 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 |  9738 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9739 | `	}` |
|        3 |  9740 | `	sVar.zString = pAux->zWorker;` |
|        - |  9741 | `	/* Extract the variable */` |
|        3 |  9742 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 |  9743 | `	if( pObj ){` |
|        3 |  9744 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 |  9745 | `	}` |
|        3 |  9746 | `	return SXRET_OK;` |
|        2 |  9747 |  |
|        - |  9748 | `/*` |
|        - |  9749 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - |  9750 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - |  9751 | ` * Parameters` |
|        - |  9752 | ` * $types` |
|        - |  9753 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - |  9754 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - |  9755 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - |  9756 | ` *  POST includes the POST uploaded file information.` |
|        - |  9757 | ` *  Note:` |
|        - |  9758 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - |  9759 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - |  9760 | ` * $prefix` |
|        - |  9761 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - |  9762 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - |  9763 | ` *  variable named $pref_userid.` |
|        - |  9764 | ` * Return` |
|        - |  9765 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9766 | ` */` |
|        2 |  9767 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9768 |  |
|        - |  9769 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - |  9770 | `	extract_aux_data sAux;` |
|        - |  9771 | `	int nLen,nPrefixLen;` |
|        - |  9772 | `	ph7_value *pSuper;` |
|        - |  9773 | `	ph7_vm *pVm;` |
|        - |  9774 | `	/* By default import only $_GET variables  */` |
|        3 |  9775 | `	zImport = "G";` |
|        3 |  9776 | `	nLen = (int)sizeof(char);` |
|        3 |  9777 | `	zPrefix = 0;` |
|        3 |  9778 | `	nPrefixLen = 0;` |
|        3 |  9779 | `	if( nArg > 0 ){` |
|        3 |  9780 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 |  9781 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 |  9782 | `		}` |
|        3 |  9783 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9784 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 |  9785 | `		}` |
|        1 |  9786 | `	}` |
|        - |  9787 | `	/* Point to the underlying VM */` |
|        3 |  9788 | `	pVm = pCtx->pVm;` |
|        - |  9789 | `	/* Initialize the aux data */` |
|        3 |  9790 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 |  9791 | `	sAux.zPrefix = zPrefix;` |
|        3 |  9792 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 |  9793 | `	sAux.pVm = pVm;` |
|        - |  9794 | `	/* Extract */` |
|        3 |  9795 | `	zEnd = &zImport[nLen];` |
|        5 |  9796 | `	while( zImport < zEnd ){` |
|        3 |  9797 | `		int c = zImport[0];` |
|        3 |  9798 | `		pSuper = 0;` |
|        3 |  9799 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - |  9800 | `			/* Import $_GET variables */` |
|        3 |  9801 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 |  9802 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - |  9803 | `			/* Import $_POST variables */` |
|      ! 0 |  9804 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 |  9805 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - |  9806 | `			/* Import $_COOKIE variables */` |
|      ! 0 |  9807 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 |  9808 | `		}` |
|        3 |  9809 | `		if( pSuper ){` |
|        - |  9810 | `			/* Iterate throw array entries */` |
|        3 |  9811 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 |  9812 | `		}` |
|        - |  9813 | `		/* Advance the cursor */` |
|        3 |  9814 | `		zImport++;` |
|        1 |  9815 | `	}` |
|        - |  9816 | `	/* All done,return TRUE*/` |
|        3 |  9817 | `	ph7_result_bool(pCtx,0);` |
|        3 |  9818 | `	return PH7_OK;` |
|        1 |  9819 |  |
|        - |  9820 | `/*` |
|        - |  9821 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |  9822 | ` * Refer to the eval() language construct implementation for more` |
|        - |  9823 | ` * information.` |
|        - |  9824 | ` */` |
|     9396 |  9825 | `static sxi32 VmEvalChunk(` |
|        - |  9826 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |  9827 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |  9828 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |  9829 | `	int iFlags,         /* Compile flag */` |
|        - |  9830 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |  9831 | `	)` |
|        2 |  9832 |  |
|        - |  9833 | `	SySet *pByteCode,aByteCode;` |
|     9398 |  9834 | `	ProcConsumer xErr = 0;` |
|     9398 |  9835 | `	void *pErrData = 0;` |
|        - |  9836 | `	/* Initialize bytecode container */` |
|     9398 |  9837 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     9398 |  9838 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |  9839 | `	/* Reset the code generator */` |
|     9398 |  9840 | `	if( bTrueReturn ){` |
|        - |  9841 | `		/* Included file,log compile-time errors */` |
|     7469 |  9842 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7469 |  9843 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3734 |  9844 | `	}` |
|     9398 |  9845 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |  9846 | `	/* Swap bytecode container */` |
|     9398 |  9847 | `	pByteCode = pVm->pByteContainer;` |
|     9398 |  9848 | `	pVm->pByteContainer = &aByteCode;` |
|        - |  9849 | `	/* Compile the chunk */` |
|     9398 |  9850 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    14096 |  9851 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |  9852 | `		/* Compilation error,return false */` |
|        3 |  9853 | `		if( pCtx ){` |
|        3 |  9854 | `			ph7_result_bool(pCtx,0);` |
|        1 |  9855 | `		}` |
|        2 |  9856 | `	}else{` |
|        - |  9857 | `		/* Mount any newly defined classes */` |
|        - |  9858 | `		SyHashEntry *pEntry;` |
|        - |  9859 | `		ph7_class *pClass;` |
|        - |  9860 | `		ph7_value sResult; /* Return value */` |
|        - |  9861 | `		sxi32 rc;` |
|     9396 |  9862 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   257271 |  9863 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   243180 |  9864 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  9865 | `			/* Only mount classes that haven't been mounted yet */` |
|   243180 |  9866 | `			if( !pClass->bMounted ){` |
|    52912 |  9867 | `				rc = VmMountUserClass(pVm,pClass);` |
|    52912 |  9868 | `				if( rc != SXRET_OK ){` |
|        - |  9869 | `					/* Mount failure (likely memory error) */` |
|      ! 0 |  9870 | `					if( pCtx ){` |
|      ! 0 |  9871 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 |  9872 | `					}` |
|      ! 0 |  9873 | `					goto Cleanup;` |
|        - |  9874 | `				}` |
|    26455 |  9875 | `			}` |
|        2 |  9876 | `		}` |
|     9396 |  9877 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  9878 | `			/* Out of memory */` |
|      ! 0 |  9879 | `			if( pCtx ){` |
|      ! 0 |  9880 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  9881 | `			}` |
|      ! 0 |  9882 | `			goto Cleanup;` |
|        - |  9883 | `		}` |
|     9396 |  9884 | `		if( bTrueReturn ){` |
|        - |  9885 | `			/* Assume a boolean true return value */` |
|     7469 |  9886 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3735 |  9887 | `		}else{` |
|        - |  9888 | `			/* Assume a null return value */` |
|     1928 |  9889 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  9890 | `		}` |
|        - |  9891 | `		/* Execute the compiled chunk */` |
|     9396 |  9892 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|     9396 |  9893 | `		if( pCtx ){` |
|        - |  9894 | `			/* Set the execution result */` |
|     7486 |  9895 | `			ph7_result_value(pCtx,&sResult);` |
|     3742 |  9896 | `		}` |
|     9396 |  9897 | `		PH7_MemObjRelease(&sResult);` |
|        - |  9898 | `	}` |
|     4698 |  9899 | `Cleanup:` |
|        - |  9900 | `	/* Cleanup the mess left behind */` |
|     9398 |  9901 | `	pVm->pByteContainer = pByteCode;` |
|     9398 |  9902 | `	SySetRelease(&aByteCode);` |
|     9398 |  9903 | `	return SXRET_OK;` |
|        2 |  9904 |  |
|        - |  9905 | `/*` |
|        - |  9906 | ` * value eval(string $code)` |
|        - |  9907 | ` *   Evaluate a string as PHP code.` |
|        - |  9908 | ` * Parameter` |
|        - |  9909 | ` *  code: PHP code to evaluate.` |
|        - |  9910 | ` * Return` |
|        - |  9911 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  9912 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  9913 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  9914 | ` */` |
|       16 |  9915 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9916 |  |
|        - |  9917 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 |  9918 | `	if( nArg < 1 ){` |
|        - |  9919 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9920 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9921 | `		return SXRET_OK;` |
|        - |  9922 | `	}` |
|        - |  9923 | `	/* Chunk to evaluate */` |
|       18 |  9924 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 |  9925 | `	if( sChunk.nByte < 1 ){` |
|        - |  9926 | `		/* Empty string,return NULL */` |
|        3 |  9927 | `		ph7_result_null(pCtx);` |
|        3 |  9928 | `		return SXRET_OK;` |
|        - |  9929 | `	}` |
|        - |  9930 | `	/* Eval the chunk */` |
|       16 |  9931 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 |  9932 | `	return SXRET_OK;` |
|       10 |  9933 |  |
|        - |  9934 | `/*` |
|        - |  9935 | ` * Check if a file path is already included.` |
|        - |  9936 | ` */` |
|    14932 |  9937 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 |  9938 |  |
|        - |  9939 | `	SyString *aEntries;` |
|        - |  9940 | `	sxu32 n;` |
|    14933 |  9941 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  9942 | `	/* Perform a linear search */` |
| 55730729 |  9943 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 55715803 |  9944 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  9945 | `			/* Already included */` |
|        7 |  9946 | `			return TRUE;` |
|        - |  9947 | `		}` |
| 27857899 |  9948 | `	}` |
|    14927 |  9949 | `	return FALSE;` |
|     7467 |  9950 |  |
|        - |  9951 | `/*` |
|        - |  9952 | ` * Push a file path in the appropriate VM container.` |
|        - |  9953 | ` */` |
|    16834 |  9954 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 |  9955 |  |
|        - |  9956 | `	SyString sPath;` |
|        - |  9957 | `	char *zDup;` |
|        - |  9958 | `#ifdef __WINNT__` |
|        - |  9959 | `	char *zCur;` |
|        - |  9960 | `#endif` |
|        - |  9961 | `	sxi32 rc;` |
|    16836 |  9962 | `	if( nLen < 0 ){` |
|     1904 |  9963 | `		nLen = SyStrlen(zPath);` |
|      951 |  9964 | `	}` |
|        - |  9965 | `	/* Duplicate the file path first */` |
|    16836 |  9966 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    16836 |  9967 | `	if( zDup == 0 ){` |
|      ! 0 |  9968 | `		return SXERR_MEM;` |
|        - |  9969 | `	}` |
|        - |  9970 | `#ifdef __WINNT__` |
|        - |  9971 | `	/* Normalize path on windows` |
|        - |  9972 | `	 * Example:` |
|        - |  9973 | `	 *    Path/To/File.php` |
|        - |  9974 | `	 * becomes` |
|        - |  9975 | `	 *   path\to\file.php` |
|        - |  9976 | `	 */` |
|        2 |  9977 | `	zCur = zDup;` |
|        2 |  9978 | `	while( zCur[0] != 0 ){` |
|        2 |  9979 | `		if( zCur[0] == '/' ){` |
|        2 |  9980 | `			zCur[0] = '\\';` |
|        2 |  9981 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 |  9982 | `			int c = SyToLower(zCur[0]);` |
|        1 |  9983 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - |  9984 | `		}` |
|        2 |  9985 | `		zCur++;` |
|        2 |  9986 | `	}` |
|        - |  9987 | `#endif` |
|        - |  9988 | `	/* Install the file path */` |
|    16836 |  9989 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    16836 |  9990 | `	if( !bMain ){` |
|    14933 |  9991 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  9992 | `			/* Already included */` |
|        7 |  9993 | `			*pNew = 0;` |
|        4 |  9994 | `		}else{` |
|        - |  9995 | `			/* Insert in the corresponding container */` |
|    14927 |  9996 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    14927 |  9997 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9998 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  9999 | `				return rc;` |
|        - | 10000 | `			}` |
|    14927 | 10001 | `			*pNew = 1;` |
|        - | 10002 | `		}` |
|     7466 | 10003 | `	}` |
|    16836 | 10004 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    16836 | 10005 | `	return SXRET_OK;` |
|     8419 | 10006 |  |
|        - | 10007 | `/*` |
|        - | 10008 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 10009 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 10010 | ` * indicates failure.` |
|        - | 10011 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 10012 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 10013 | ` * operations.` |
|        - | 10014 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 10015 | ` * this function is a no-op.` |
|        - | 10016 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 10017 | ` * constructs for more information.` |
|        - | 10018 | ` */` |
|     7474 | 10019 | `static sxi32 VmExecIncludedFile(` |
|        - | 10020 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 10021 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 10022 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 10023 | `	 )` |
|        2 | 10024 |  |
|        - | 10025 | `	sxi32 rc;` |
|        - | 10026 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10027 | `	const ph7_io_stream *pStream;` |
|        - | 10028 | `	SyBlob sContents;` |
|        - | 10029 | `	void *pHandle;` |
|        - | 10030 | `	ph7_vm *pVm;` |
|        - | 10031 | `	int isNew;` |
|        - | 10032 | `	/* Initialize fields */` |
|     7476 | 10033 | `	pVm = pCtx->pVm;` |
|     7476 | 10034 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7476 | 10035 | `	isNew = 0;` |
|        - | 10036 | `	/* Extract the associated stream */` |
|     7476 | 10037 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 10038 | `	/*` |
|        - | 10039 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 10040 | `	 * in a read-only mode.` |
|        - | 10041 | `	 */` |
|     7476 | 10042 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7476 | 10043 | `	if( pHandle == 0 ){` |
|        3 | 10044 | `		return SXERR_IO;` |
|        - | 10045 | `	}` |
|     7473 | 10046 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7473 | 10047 | `	if( IncludeOnce && !isNew ){` |
|        - | 10048 | `		/* Already included */` |
|        5 | 10049 | `		rc = SXERR_EXISTS;` |
|        3 | 10050 | `	}else{` |
|        - | 10051 | `		/* Read the whole file contents */` |
|     7469 | 10052 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7469 | 10053 | `		if( rc == SXRET_OK ){` |
|        - | 10054 | `			SyString sScript;` |
|        - | 10055 | `			/* Compile and execute the script */` |
|     7469 | 10056 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7469 | 10057 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3734 | 10058 | `		}` |
|        - | 10059 | `	}` |
|        - | 10060 | `	/* Pop from the set of included file */` |
|     7473 | 10061 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 10062 | `	/* Close the handle */` |
|     7473 | 10063 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 10064 | `	/* Release the working buffer */` |
|     7473 | 10065 | `	SyBlobRelease(&sContents);` |
|        - | 10066 | `#else` |
|        - | 10067 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 10068 | `	SXUNUSED(pPath);` |
|        - | 10069 | `	SXUNUSED(IncludeOnce);` |
|        - | 10070 | `	rc = SXERR_IO;` |
|        - | 10071 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7473 | 10072 | `	return rc;` |
|     3739 | 10073 |  |
|        - | 10074 | `/*` |
|        - | 10075 | ` * string get_include_path(void)` |
|        - | 10076 | ` *  Gets the current include_path configuration option.` |
|        - | 10077 | ` * Parameter` |
|        - | 10078 | ` *  None` |
|        - | 10079 | ` * Return` |
|        - | 10080 | ` *  Included paths as a string` |
|        - | 10081 | ` */` |
|        2 | 10082 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10083 |  |
|        3 | 10084 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10085 | `	SyString *aEntry;` |
|        - | 10086 | `	int dir_sep;` |
|        - | 10087 | `	sxu32 n;` |
|        - | 10088 | `#ifdef __WINNT__` |
|        1 | 10089 | `	dir_sep = ';';` |
|        - | 10090 | `#else` |
|        - | 10091 | `	/* Assume UNIX path separator */` |
|        2 | 10092 | `	dir_sep = ':';` |
|        - | 10093 | `#endif` |
|        1 | 10094 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10095 | `	SXUNUSED(apArg);` |
|        - | 10096 | `	/* Point to the list of import paths */` |
|        3 | 10097 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 10098 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 10099 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 10100 | `		if( n > 0 ){` |
|        - | 10101 | `			/* Append dir seprator */` |
|      ! 0 | 10102 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 10103 | `		}` |
|        - | 10104 | `		/* Append path */` |
|        3 | 10105 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 10106 | `	}` |
|        3 | 10107 | `	return PH7_OK;` |
|        1 | 10108 |  |
|        - | 10109 | `/*` |
|        - | 10110 | ` * string get_get_included_files(void)` |
|        - | 10111 | ` *  Gets the current include_path configuration option.` |
|        - | 10112 | ` * Parameter` |
|        - | 10113 | ` *  None` |
|        - | 10114 | ` * Return` |
|        - | 10115 | ` *  Included paths as a string` |
|        - | 10116 | ` */` |
|        2 | 10117 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10118 |  |
|        3 | 10119 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 10120 | `	ph7_value *pArray,*pWorker;` |
|        - | 10121 | `	SyString *pEntry;` |
|        - | 10122 | `	int c,d;` |
|        - | 10123 | `	/* Create an array and a working value */` |
|        3 | 10124 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 10125 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 10126 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 10127 | `		/* Out of memory,return null */` |
|      ! 0 | 10128 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10129 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10130 | `		SXUNUSED(apArg);` |
|      ! 0 | 10131 | `		return PH7_OK;` |
|        - | 10132 | `	}` |
|        3 | 10133 | `	c = d = '/';` |
|        - | 10134 | `#ifdef __WINNT__` |
|        1 | 10135 | `	d = '\\';` |
|        - | 10136 | `#endif` |
|        - | 10137 | `	/* Iterate throw entries */` |
|        3 | 10138 | `	SySetResetCursor(pFiles);` |
|     3627 | 10139 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 10140 | `		const char *zBase,*zEnd;` |
|        - | 10141 | `		int iLen;` |
|        - | 10142 | `		/* reset the string cursor */` |
|     3625 | 10143 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 10144 | `		/* Extract base name */` |
|     3625 | 10145 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 10146 | `		/* Ignore trailing '/' */` |
|     5437 | 10147 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 10148 | `			zEnd--;` |
|      ! 0 | 10149 | `		}` |
|     3625 | 10150 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   111273 | 10151 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   105837 | 10152 | `			zEnd--;` |
|        1 | 10153 | `		}` |
|     3625 | 10154 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3625 | 10155 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 10156 | `		/* Copy entry name */` |
|     3625 | 10157 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 10158 | `		/* Perform the insertion */` |
|     3625 | 10159 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 10160 | `	}` |
|        - | 10161 | `	/* All done,return the created array */` |
|        3 | 10162 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10163 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 10164 | `	 * by the engine as soon we return from this foreign` |
|        - | 10165 | `	 * function.` |
|        - | 10166 | `	 */` |
|        3 | 10167 | `	return PH7_OK;` |
|        2 | 10168 |  |
|        - | 10169 | `/*` |
|        - | 10170 | ` * include:` |
|        - | 10171 | ` * According to the PHP reference manual.` |
|        - | 10172 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 10173 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 10174 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 10175 | ` *  include() will finally check in the calling script's own directory` |
|        - | 10176 | ` *  and the current working directory before failing. The include()` |
|        - | 10177 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 10178 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 10179 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 10180 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 10181 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 10182 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 10183 | ` *  directory to find the requested file.` |
|        - | 10184 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 10185 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 10186 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 10187 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 10188 | ` */` |
|     7462 | 10189 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10190 |  |
|        - | 10191 | `	SyString sFile;` |
|        - | 10192 | `	sxi32 rc;` |
|     7464 | 10193 | `	if( nArg < 1 ){` |
|        - | 10194 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10195 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10196 | `		return SXRET_OK;` |
|        - | 10197 | `	}` |
|        - | 10198 | `	/* File to include */` |
|     7464 | 10199 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7464 | 10200 | `	if( sFile.nByte < 1 ){` |
|        - | 10201 | `		/* Empty string,return NULL */` |
|      ! 0 | 10202 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10203 | `		return SXRET_OK;` |
|        - | 10204 | `	}` |
|        - | 10205 | `	/* Open,compile and execute the desired script */` |
|     7464 | 10206 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7464 | 10207 | `	if( rc != SXRET_OK ){` |
|        - | 10208 | `		/* Emit a warning and return false */` |
|        3 | 10209 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 10210 | `		ph7_result_bool(pCtx,0);` |
|        1 | 10211 | `	}` |
|     7464 | 10212 | `	return SXRET_OK;` |
|     3733 | 10213 |  |
|        - | 10214 | `/*` |
|        - | 10215 | ` * include_once:` |
|        - | 10216 | ` *  According to the PHP reference manual.` |
|        - | 10217 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 10218 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 10219 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 10220 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 10221 | ` *   just once.` |
|        - | 10222 | ` */` |
|        4 | 10223 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10224 |  |
|        - | 10225 | `	SyString sFile;` |
|        - | 10226 | `	sxi32 rc;` |
|        5 | 10227 | `	if( nArg < 1 ){` |
|        - | 10228 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10229 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10230 | `		return SXRET_OK;` |
|        - | 10231 | `	}` |
|        - | 10232 | `	/* File to include */` |
|        5 | 10233 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10234 | `	if( sFile.nByte < 1 ){` |
|        - | 10235 | `		/* Empty string,return NULL */` |
|      ! 0 | 10236 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10237 | `		return SXRET_OK;` |
|        - | 10238 | `	}` |
|        - | 10239 | `	/* Open,compile and execute the desired script */` |
|        5 | 10240 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 10241 | `	if( rc == SXERR_EXISTS ){` |
|        - | 10242 | `		/* File already included,return TRUE */` |
|        3 | 10243 | `		ph7_result_bool(pCtx,1);` |
|        3 | 10244 | `		return SXRET_OK;` |
|        - | 10245 | `	}` |
|        3 | 10246 | `	if( rc != SXRET_OK ){` |
|        - | 10247 | `		/* Emit a warning and return false */` |
|      ! 0 | 10248 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10249 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10250 | ` 	}` |
|        3 | 10251 | `	return SXRET_OK;` |
|        3 | 10252 |  |
|        - | 10253 | `/*` |
|        - | 10254 | ` * require.` |
|        - | 10255 | ` *  According to the PHP reference manual.` |
|        - | 10256 | ` *   require() is identical to include() except upon failure it will` |
|        - | 10257 | ` *   also produce a fatal level error.` |
|        - | 10258 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 10259 | ` *   emits a warning  which allows the script to continue.` |
|        - | 10260 | ` */` |
|        4 | 10261 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10262 |  |
|        - | 10263 | `	SyString sFile;` |
|        - | 10264 | `	sxi32 rc;` |
|        5 | 10265 | `	if( nArg < 1 ){` |
|        - | 10266 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10267 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10268 | `		return SXRET_OK;` |
|        - | 10269 | `	}` |
|        - | 10270 | `	/* File to include */` |
|        5 | 10271 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10272 | `	if( sFile.nByte < 1 ){` |
|        - | 10273 | `		/* Empty string,return NULL */` |
|      ! 0 | 10274 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10275 | `		return SXRET_OK;` |
|        - | 10276 | `	}` |
|        - | 10277 | `	/* Open,compile and execute the desired script */` |
|        5 | 10278 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 10279 | `	if( rc != SXRET_OK ){` |
|        - | 10280 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10281 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10282 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10283 | `		return PH7_ABORT;` |
|        - | 10284 | `	}` |
|        5 | 10285 | `	return SXRET_OK;` |
|        3 | 10286 |  |
|        - | 10287 | `/*` |
|        - | 10288 | ` * require_once:` |
|        - | 10289 | ` *  According to the PHP reference manual.` |
|        - | 10290 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 10291 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 10292 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 10293 | ` *   and how it differs from its non _once siblings.` |
|        - | 10294 | ` */` |
|        4 | 10295 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
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
|        - | 10319 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10320 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10321 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10322 | `		return PH7_ABORT;` |
|        - | 10323 | `	}` |
|        3 | 10324 | `	return SXRET_OK;` |
|        3 | 10325 |  |
|        - | 10326 | `/*` |
|        - | 10327 | ` * Section:` |
|        - | 10328 | ` *  Command line arguments processing.` |
|        - | 10329 | ` * Status:` |
|        - | 10330 | ` *    Stable.` |
|        - | 10331 | ` */` |
|        - | 10332 | `/*` |
|        - | 10333 | ` * Check if a short option argument [i.e: -c] is available in the command` |
|        - | 10334 | ` * line string. Return a pointer to the start of the stream on success.` |
|        - | 10335 | ` * NULL otherwise.` |
|        - | 10336 | ` */` |
|        6 | 10337 | `static const char * VmFindShortOpt(int c,const char *zIn,const char *zEnd)` |
|        1 | 10338 |  |
|      319 | 10339 | `	while( zIn < zEnd ){` |
|      313 | 10340 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == c ){` |
|        - | 10341 | `			/* Got one */` |
|      ! 0 | 10342 | `			return &zIn[1];` |
|        - | 10343 | `		}` |
|        - | 10344 | `		/* Advance the cursor */` |
|      313 | 10345 | `		zIn++;` |
|        1 | 10346 | `	}` |
|        - | 10347 | `	/* No such option */` |
|        7 | 10348 | `	return 0;` |
|        4 | 10349 |  |
|        - | 10350 | `/*` |
|        - | 10351 | ` * Check if a long option argument [i.e: --opt] is available in the command` |
|        - | 10352 | ` * line string. Return a pointer to the start of the stream on success.` |
|        - | 10353 | ` * NULL otherwise.` |
|        - | 10354 | ` */` |
|      ! 0 | 10355 | `static const char * VmFindLongOpt(const char *zLong,int nByte,const char *zIn,const char *zEnd)` |
|      ! 0 | 10356 |  |
|        - | 10357 | `	const char *zOpt;` |
|      ! 0 | 10358 | `	while( zIn < zEnd ){` |
|      ! 0 | 10359 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == '-' ){` |
|      ! 0 | 10360 | `			zIn += 2;` |
|      ! 0 | 10361 | `			zOpt = zIn;` |
|      ! 0 | 10362 | `			while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 10363 | `				if( zIn[0] == '=' /* --opt=val */){` |
|      ! 0 | 10364 | `					break;` |
|        - | 10365 | `				}` |
|      ! 0 | 10366 | `				zIn++;` |
|      ! 0 | 10367 | `			}` |
|        - | 10368 | `			/* Test */` |
|      ! 0 | 10369 | `			if( (int)(zIn-zOpt) == nByte && SyMemcmp(zOpt,zLong,nByte) == 0 ){` |
|        - | 10370 | `				/* Got one,return it's value */` |
|      ! 0 | 10371 | `				return zIn;` |
|        - | 10372 | `			}` |
|        - | 10373 |  |
|      ! 0 | 10374 | `		}else{` |
|      ! 0 | 10375 | `			zIn++;` |
|        - | 10376 | `		}` |
|      ! 0 | 10377 | `	}` |
|        - | 10378 | `	/* No such option */` |
|      ! 0 | 10379 | `	return 0;` |
|      ! 0 | 10380 |  |
|        - | 10381 | `/*` |
|        - | 10382 | ` * Long option [i.e: --opt] arguments private data structure.` |
|        - | 10383 | ` */` |
|        - | 10384 | `struct getopt_long_opt` |
|        - | 10385 |  |
|        - | 10386 | `	const char *zArgIn,*zArgEnd; /* Command line arguments */` |
|        - | 10387 | `	ph7_value *pWorker;  /* Worker variable*/` |
|        - | 10388 | `	ph7_value *pArray;   /* getopt() return value */` |
|        - | 10389 | `	ph7_context *pCtx;   /* Call Context */` |
|        - | 10390 | `};` |
|        - | 10391 | `/* Forward declaration */` |
|        - | 10392 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 10393 | `/*` |
|        - | 10394 | ` * Extract short or long argument option values.` |
|        - | 10395 | ` */` |
|      ! 0 | 10396 | `static void VmExtractOptArgValue(` |
|        - | 10397 | `	ph7_value *pArray,  /* getopt() return value */` |
|        - | 10398 | `	ph7_value *pWorker, /* Worker variable */` |
|        - | 10399 | `	const char *zArg,   /* Argument stream */` |
|        - | 10400 | `	const char *zArgEnd,/* End of the argument stream  */` |
|        - | 10401 | `	int need_val,       /* TRUE to fetch option argument */` |
|        - | 10402 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 10403 | `	const char *zName   /* Option name */)` |
|      ! 0 | 10404 |  |
|      ! 0 | 10405 | `	ph7_value_bool(pWorker,0);` |
|      ! 0 | 10406 | `	if( !need_val ){` |
|        - | 10407 | `		/*` |
|        - | 10408 | `		 * Option does not need arguments.` |
|        - | 10409 | `		 * Insert the option name and a boolean FALSE.` |
|        - | 10410 | `		 */` |
|      ! 0 | 10411 | `		ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|      ! 0 | 10412 | `	}else{` |
|        - | 10413 | `		const char *zCur;` |
|        - | 10414 | `		/* Extract option argument */` |
|      ! 0 | 10415 | `		zArg++;` |
|      ! 0 | 10416 | `		if( zArg < zArgEnd && zArg[0] == '=' ){` |
|      ! 0 | 10417 | `			zArg++;` |
|      ! 0 | 10418 | `		}` |
|      ! 0 | 10419 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 10420 | `			zArg++;` |
|      ! 0 | 10421 | `		}` |
|      ! 0 | 10422 | `		if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|        - | 10423 | `			/*` |
|        - | 10424 | `			 * Argument not found.` |
|        - | 10425 | `			 * Insert the option name and a boolean FALSE.` |
|        - | 10426 | `			 */` |
|      ! 0 | 10427 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|      ! 0 | 10428 | `			return;` |
|        - | 10429 | `		}` |
|        - | 10430 | `		/* Delimit the value */` |
|      ! 0 | 10431 | `		zCur = zArg;` |
|      ! 0 | 10432 | `		if( zArg[0] == '\'' \|\| zArg[0] == '"' ){` |
|      ! 0 | 10433 | `			int d = zArg[0];` |
|        - | 10434 | `			/* Delimt the argument */` |
|      ! 0 | 10435 | `			zArg++;` |
|      ! 0 | 10436 | `			zCur = zArg;` |
|      ! 0 | 10437 | `			while( zArg < zArgEnd ){` |
|      ! 0 | 10438 | `				if( zArg[0] == d && zArg[-1] != '\\' ){` |
|        - | 10439 | `					/* Delimiter found,exit the loop  */` |
|      ! 0 | 10440 | `					break;` |
|        - | 10441 | `				}` |
|      ! 0 | 10442 | `				zArg++;` |
|      ! 0 | 10443 | `			}` |
|        - | 10444 | `			/* Save the value */` |
|      ! 0 | 10445 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|      ! 0 | 10446 | `			if( zArg < zArgEnd ){ zArg++; }` |
|      ! 0 | 10447 | `		}else{` |
|      ! 0 | 10448 | `			while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|      ! 0 | 10449 | `				zArg++;` |
|      ! 0 | 10450 | `			}` |
|        - | 10451 | `			/* Save the value */` |
|      ! 0 | 10452 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|        - | 10453 | `		}` |
|        - | 10454 | `		/*` |
|        - | 10455 | `		 * Check if we are dealing with multiple values.` |
|        - | 10456 | `		 * If so,create an array to hold them,rather than a scalar variable.` |
|        - | 10457 | `		 */` |
|      ! 0 | 10458 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 10459 | `			zArg++;` |
|      ! 0 | 10460 | `		}` |
|      ! 0 | 10461 | `		if( zArg < zArgEnd && zArg[0] != '-' ){` |
|        - | 10462 | `			ph7_value *pOptArg; /* Array of option arguments */` |
|      ! 0 | 10463 | `			pOptArg = ph7_context_new_array(pCtx);` |
|      ! 0 | 10464 | `			if( pOptArg == 0 ){` |
|      ! 0 | 10465 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 10466 | `			}else{` |
|        - | 10467 | `				/* Insert the first value */` |
|      ! 0 | 10468 | `				ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|      ! 0 | 10469 | `				for(;;){` |
|      ! 0 | 10470 | `					if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|        - | 10471 | `						/* No more value */` |
|      ! 0 | 10472 | `						break;` |
|        - | 10473 | `					}` |
|        - | 10474 | `					/* Delimit the value */` |
|      ! 0 | 10475 | `					zCur = zArg;` |
|      ! 0 | 10476 | `					if( zArg < zArgEnd && zArg[0] == '\\' ){` |
|      ! 0 | 10477 | `						zArg++;` |
|      ! 0 | 10478 | `						zCur = zArg;` |
|      ! 0 | 10479 | `					}` |
|      ! 0 | 10480 | `					while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|      ! 0 | 10481 | `						zArg++;` |
|      ! 0 | 10482 | `					}` |
|        - | 10483 | `					/* Reset the string cursor */` |
|      ! 0 | 10484 | `					ph7_value_reset_string_cursor(pWorker);` |
|        - | 10485 | `					/* Save the value */` |
|      ! 0 | 10486 | `					ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|        - | 10487 | `					/* Insert */` |
|      ! 0 | 10488 | `					ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|        - | 10489 | `					/* Jump trailing white spaces */` |
|      ! 0 | 10490 | `					while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 10491 | `						zArg++;` |
|      ! 0 | 10492 | `					}` |
|      ! 0 | 10493 | `				}` |
|        - | 10494 | `				/* Insert the option arg array */` |
|      ! 0 | 10495 | `				ph7_array_add_strkey_elem(pArray,(const char *)zName,pOptArg); /* Will make it's own copy */` |
|        - | 10496 | `				/* Safely release */` |
|      ! 0 | 10497 | `				ph7_context_release_value(pCtx,pOptArg);` |
|        - | 10498 | `			}` |
|      ! 0 | 10499 | `		}else{` |
|        - | 10500 | `			/* Single value */` |
|      ! 0 | 10501 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|        - | 10502 | `		}` |
|        - | 10503 | `	}` |
|      ! 0 | 10504 |  |
|        - | 10505 | `/*` |
|        - | 10506 | ` * array getopt(string $options[,array $longopts ])` |
|        - | 10507 | ` *   Gets options from the command line argument list.` |
|        - | 10508 | ` * Parameters` |
|        - | 10509 | ` *  $options` |
|        - | 10510 | ` *   Each character in this string will be used as option characters` |
|        - | 10511 | ` *   and matched against options passed to the script starting with` |
|        - | 10512 | ` *   a single hyphen (-). For example, an option string "x" recognizes` |
|        - | 10513 | ` *   an option -x. Only a-z, A-Z and 0-9 are allowed.` |
|        - | 10514 | ` *  $longopts` |
|        - | 10515 | ` *   An array of options. Each element in this array will be used as option` |
|        - | 10516 | ` *   strings and matched against options passed to the script starting with` |
|        - | 10517 | ` *   two hyphens (--). For example, an longopts element "opt" recognizes an` |
|        - | 10518 | ` *   option --opt.` |
|        - | 10519 | ` * Return` |
|        - | 10520 | ` *  This function will return an array of option / argument pairs or FALSE` |
|        - | 10521 | ` *  on failure.` |
|        - | 10522 | ` */` |
|        2 | 10523 | `static int vm_builtin_getopt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10524 |  |
|        - | 10525 | `	const char *zIn,*zEnd,*zArg,*zArgIn,*zArgEnd;` |
|        - | 10526 | `	struct getopt_long_opt sLong;` |
|        - | 10527 | `	ph7_value *pArray,*pWorker;` |
|        - | 10528 | `	SyBlob *pArg;` |
|        - | 10529 | `	int nByte;` |
|        3 | 10530 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10531 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 10532 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Missing/Invalid option arguments");` |
|      ! 0 | 10533 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10534 | `		return PH7_OK;` |
|        - | 10535 | `	}` |
|        - | 10536 | `	/* Extract option arguments */` |
|        3 | 10537 | `	zIn  = ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 10538 | `	zEnd = &zIn[nByte];` |
|        - | 10539 | `	/* Point to the string representation of the $argv[] array */` |
|        3 | 10540 | `	pArg = &pCtx->pVm->sArgv;` |
|        - | 10541 | `	/* Create a new empty array and a worker variable */` |
|        3 | 10542 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10543 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 10544 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|      ! 0 | 10545 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 10546 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10547 | `		return PH7_OK;` |
|        - | 10548 | `	}` |
|        3 | 10549 | `	if( SyBlobLength(pArg) < 1 ){` |
|        - | 10550 | `		/* Empty command line,return the empty array*/` |
|      ! 0 | 10551 | `		ph7_result_value(pCtx,pArray);` |
|        - | 10552 | `		/* Everything will be released automatically when we return` |
|        - | 10553 | `		 * from this function.` |
|        - | 10554 | `		 */` |
|      ! 0 | 10555 | `		return PH7_OK;` |
|        - | 10556 | `	}` |
|        3 | 10557 | `	zArgIn = (const char *)SyBlobData(pArg);` |
|        3 | 10558 | `	zArgEnd = &zArgIn[SyBlobLength(pArg)];` |
|        - | 10559 | `	/* Fill the long option structure */` |
|        3 | 10560 | `	sLong.pArray = pArray;` |
|        3 | 10561 | `	sLong.pWorker = pWorker;` |
|        3 | 10562 | `	sLong.zArgIn =  zArgIn;` |
|        3 | 10563 | `	sLong.zArgEnd = zArgEnd;` |
|        3 | 10564 | `	sLong.pCtx = pCtx;` |
|        - | 10565 | `	/* Start processing */` |
|        9 | 10566 | `	while( zIn < zEnd ){` |
|        7 | 10567 | `		int c = zIn[0];` |
|        7 | 10568 | `		int need_val = 0;` |
|        - | 10569 | `		/* Advance the stream cursor */` |
|        7 | 10570 | `		zIn++;` |
|        - | 10571 | `		/* Ignore non-alphanum characters */` |
|        7 | 10572 | `		if( !SyisAlphaNum(c) ){` |
|      ! 0 | 10573 | `			continue;` |
|        - | 10574 | `		}` |
|        7 | 10575 | `		if( zIn < zEnd && zIn[0] == ':' ){` |
|        5 | 10576 | `			zIn++;` |
|        5 | 10577 | `			need_val = 1;` |
|        5 | 10578 | `			if( zIn < zEnd && zIn[0] == ':' ){` |
|      ! 0 | 10579 | `				zIn++;` |
|      ! 0 | 10580 | `			}` |
|        2 | 10581 | `		}` |
|        - | 10582 | `		/* Find option */` |
|        7 | 10583 | `		zArg = VmFindShortOpt(c,zArgIn,zArgEnd);` |
|        7 | 10584 | `		if( zArg == 0 ){` |
|        - | 10585 | `			/* No such option */` |
|        7 | 10586 | `			continue;` |
|        - | 10587 | `		}` |
|        - | 10588 | `		/* Extract option argument value */` |
|      ! 0 | 10589 | `		VmExtractOptArgValue(pArray,pWorker,zArg,zArgEnd,need_val,pCtx,(const char *)&c);` |
|      ! 0 | 10590 | `	}` |
|        3 | 10591 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) && ph7_array_count(apArg[1]) > 0 ){` |
|        - | 10592 | `		/* Process long options */` |
|      ! 0 | 10593 | `		ph7_array_walk(apArg[1],VmProcessLongOpt,&sLong);` |
|      ! 0 | 10594 | `	}` |
|        - | 10595 | `	/* Return the option array */` |
|        3 | 10596 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10597 | `	/*` |
|        - | 10598 | `	 * Don't worry about freeing memory, everything will be released` |
|        - | 10599 | `	 * automatically as soon we return from this foreign function.` |
|        - | 10600 | `	 */` |
|        3 | 10601 | `	return PH7_OK;` |
|        2 | 10602 |  |
|        - | 10603 | `/*` |
|        - | 10604 | ` * Array walker callback used for processing long options values.` |
|        - | 10605 | ` */` |
|      ! 0 | 10606 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 10607 |  |
|      ! 0 | 10608 | `	struct getopt_long_opt *pOpt = (struct getopt_long_opt *)pUserData;` |
|        - | 10609 | `	const char *zArg,*zOpt,*zEnd;` |
|      ! 0 | 10610 | `	int need_value = 0;` |
|        - | 10611 | `	int nByte;` |
|        - | 10612 | `	/* Value must be of type string */` |
|      ! 0 | 10613 | `	if( !ph7_value_is_string(pValue) ){` |
|        - | 10614 | `		/* Simply ignore */` |
|      ! 0 | 10615 | `		return PH7_OK;` |
|        - | 10616 | `	}` |
|      ! 0 | 10617 | `	zOpt = ph7_value_to_string(pValue,&nByte);` |
|      ! 0 | 10618 | `	if( nByte < 1 ){` |
|        - | 10619 | `		/* Empty string,ignore */` |
|      ! 0 | 10620 | `		return PH7_OK;` |
|        - | 10621 | `	}` |
|      ! 0 | 10622 | `	zEnd = &zOpt[nByte - 1];` |
|      ! 0 | 10623 | `	if( zEnd[0] == ':' ){` |
|        - | 10624 | `		char *zTerm;` |
|        - | 10625 | `		/* Try to extract a value */` |
|      ! 0 | 10626 | `		need_value = 1;` |
|      ! 0 | 10627 | `		while( zEnd >= zOpt && zEnd[0] == ':' ){` |
|      ! 0 | 10628 | `			zEnd--;` |
|      ! 0 | 10629 | `		}` |
|      ! 0 | 10630 | `		if( zOpt >= zEnd ){` |
|        - | 10631 | `			/* Empty string,ignore */` |
|      ! 0 | 10632 | `			SXUNUSED(pKey);` |
|      ! 0 | 10633 | `			return PH7_OK;` |
|        - | 10634 | `		}` |
|      ! 0 | 10635 | `		zEnd++;` |
|      ! 0 | 10636 | `		zTerm = (char *)zEnd;` |
|      ! 0 | 10637 | `		zTerm[0] = 0;` |
|      ! 0 | 10638 | `	}else{` |
|      ! 0 | 10639 | `		zEnd = &zOpt[nByte];` |
|        - | 10640 | `	}` |
|        - | 10641 | `	/* Find the option */` |
|      ! 0 | 10642 | `	zArg = VmFindLongOpt(zOpt,(int)(zEnd-zOpt),pOpt->zArgIn,pOpt->zArgEnd);` |
|      ! 0 | 10643 | `	if( zArg == 0 ){` |
|        - | 10644 | `		/* No such option,return immediately */` |
|      ! 0 | 10645 | `		return PH7_OK;` |
|        - | 10646 | `	}` |
|        - | 10647 | `	/* Try to extract a value */` |
|      ! 0 | 10648 | `	VmExtractOptArgValue(pOpt->pArray,pOpt->pWorker,zArg,pOpt->zArgEnd,need_value,pOpt->pCtx,zOpt);` |
|      ! 0 | 10649 | `	return PH7_OK;` |
|      ! 0 | 10650 |  |
|        - | 10651 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 10652 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 10653 | `/* Table of built-in VM functions. */` |
|        - | 10654 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 10655 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 10656 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 10657 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 10658 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 10659 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 10660 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 10661 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 10662 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 10663 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 10664 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 10665 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 10666 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 10667 | `	    /* Constants management */` |
|        - | 10668 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 10669 | `	{ "define",   vm_builtin_define               },` |
|        - | 10670 | `	{ "constant", vm_builtin_constant             },` |
|        - | 10671 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 10672 | `	   /* Class/Object functions */` |
|        - | 10673 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 10674 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 10675 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 10676 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 10677 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 10678 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 10679 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 10680 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 10681 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 10682 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 10683 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 10684 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 10685 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 10686 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 10687 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 10688 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 10689 | `	   /* Random numbers/strings generators */` |
|        - | 10690 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 10691 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 10692 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 10693 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 10694 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 10695 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10696 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 10697 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 10698 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 10699 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10700 | `	   /* Language constructs functions */` |
|        - | 10701 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 10702 | `	{ "print", vm_builtin_print                   },` |
|        - | 10703 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 10704 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 10705 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 10706 | `	  /* Variable handling functions */` |
|        - | 10707 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 10708 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 10709 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 10710 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 10711 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 10712 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 10713 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 10714 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 10715 | `	  /* Ouput control functions */` |
|        - | 10716 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 10717 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 10718 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 10719 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 10720 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 10721 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 10722 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 10723 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 10724 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 10725 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 10726 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 10727 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 10728 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 10729 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 10730 | `	  /* Assertion functions */` |
|        - | 10731 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 10732 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 10733 | `	  /* Error reporting functions */` |
|        - | 10734 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 10735 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 10736 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 10737 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 10738 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 10739 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 10740 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 10741 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 10742 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 10743 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 10744 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 10745 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 10746 | `	  /* Release info */` |
|        - | 10747 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 10748 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 10749 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 10750 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 10751 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 10752 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 10753 | `	  /* hashmap */` |
|        - | 10754 | `	{"compact",          vm_builtin_compact       },` |
|        - | 10755 | `	{"extract",          vm_builtin_extract       },` |
|        - | 10756 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 10757 | `	  /* URL related function */` |
|        - | 10758 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 10759 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 10760 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10761 | `	   /* XML processing functions */` |
|        - | 10762 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 10763 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 10764 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 10765 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 10766 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 10767 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 10768 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 10769 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 10770 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 10771 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 10772 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 10773 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 10774 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 10775 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 10776 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 10777 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 10778 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 10779 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 10780 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 10781 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 10782 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 10783 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10784 | `	   /* UTF-8 encoding/decoding */` |
|        - | 10785 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 10786 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 10787 | `	   /* Command line processing */` |
|        - | 10788 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 10789 | `	   /* JSON encoding/decoding */` |
|        - | 10790 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 10791 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 10792 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 10793 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 10794 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 10795 | `	   /* Files/URI inclusion facility */` |
|        - | 10796 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 10797 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 10798 | `	{ "include",      vm_builtin_include          },` |
|        - | 10799 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 10800 | `	{ "require",      vm_builtin_require          },` |
|        - | 10801 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 10802 | `};` |
|        - | 10803 | `/*` |
|        - | 10804 | ` * Register the built-in VM functions defined above.` |
|        - | 10805 | ` */` |
|     1672 | 10806 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 10807 |  |
|        - | 10808 | `	sxi32 rc;` |
|        - | 10809 | `	sxu32 n;` |
|   209002 | 10810 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 10811 | `		/* Note that these special functions have access` |
|        - | 10812 | `		 * to the underlying virtual machine as their` |
|        - | 10813 | `		 * private data.` |
|        - | 10814 | `		 */` |
|   207330 | 10815 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   207330 | 10816 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 10817 | `			return rc;` |
|        - | 10818 | `		}` |
|   103666 | 10819 | `	}` |
|     1674 | 10820 | `	return SXRET_OK;` |
|      838 | 10821 |  |
|        - | 10822 | `/*` |
|        - | 10823 | ` * Check if the given name refer to an installed class.` |
|        - | 10824 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 10825 | ` */` |
|    10604 | 10826 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 10827 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 10828 | `	const char *zName,  /* Name of the target class */` |
|        - | 10829 | `	sxu32 nByte,        /* zName length */` |
|        - | 10830 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 10831 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 10832 | `						 */` |
|        - | 10833 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 10834 | `	)` |
|        2 | 10835 |  |
|        - | 10836 | `	SyHashEntry *pEntry;` |
|        - | 10837 | `	ph7_class *pClass;` |
|     5302 | 10838 | `		SXUNUSED(iNest);` |
|        - | 10839 | `	/* Perform a hash lookup */` |
|    10606 | 10840 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|        - | 10841 |  |
|    10606 | 10842 | `	if( pEntry == 0 ){` |
|        - | 10843 | `		/* No such entry,return NULL */` |
|      ! 0 | 10844 | `		return 0;` |
|        - | 10845 | `	}` |
|    10606 | 10846 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    10606 | 10847 | `	if( !iLoadable ){` |
|        - | 10848 | `		/* Return the first class seen */` |
|     9714 | 10849 | `		return pClass;` |
|      ! 0 | 10850 | `	}else{` |
|        - | 10851 | `		/* Check the collision list */` |
|      894 | 10852 | `		while(pClass){` |
|      894 | 10853 | `			if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) == 0 ){` |
|        - | 10854 | `				/* Class is loadable */` |
|      894 | 10855 | `				return pClass;` |
|        - | 10856 | `			}` |
|        - | 10857 | `			/* Point to the next entry */` |
|      ! 0 | 10858 | `			pClass = pClass->pNextName;` |
|      ! 0 | 10859 | `		}` |
|        - | 10860 | `	}` |
|        - | 10861 | `	/* No such loadable class */` |
|      ! 0 | 10862 | `	return 0;` |
|     5304 | 10863 |  |
|        - | 10864 | `/*` |
|        - | 10865 | ` * Reference Table Implementation` |
|        - | 10866 | ` * Status: stable <chm@symisc.net>` |
|        - | 10867 | ` * Intro` |
|        - | 10868 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 10869 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 10870 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 10871 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 10872 | ` *  Refer to the official for more information on this powerful` |
|        - | 10873 | ` *  extension.` |
|        - | 10874 | ` */` |
|        - | 10875 | `/*` |
|        - | 10876 | ` * Allocate a new reference entry.` |
|        - | 10877 | ` */` |
|  2938686 | 10878 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 10879 |  |
|        - | 10880 | `	VmRefObj *pRef;` |
|        - | 10881 | `	/* Allocate a new instance */` |
|  2938688 | 10882 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  2938688 | 10883 | `	if( pRef == 0 ){` |
|      ! 0 | 10884 | `		return 0;` |
|        - | 10885 | `	}` |
|        - | 10886 | `	/* Zero the structure */` |
|  2938688 | 10887 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 10888 | `	/* Initialize fields */` |
|  2938688 | 10889 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  2938688 | 10890 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  2938688 | 10891 | `	pRef->nIdx = nIdx;` |
|  2938688 | 10892 | `	return pRef;` |
|  1469345 | 10893 |  |
|        - | 10894 | `/*` |
|        - | 10895 | ` * Default hash function used by the reference table` |
|        - | 10896 | ` * for lookup/insertion operations.` |
|        - | 10897 | ` */` |
| 16369942 | 10898 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 10899 |  |
|        - | 10900 | `	/* Calculate the hash based on the memory object index */` |
| 16369944 | 10901 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 10902 |  |
|        - | 10903 | `/*` |
|        - | 10904 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 10905 | ` * in the reference table.` |
|        - | 10906 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 10907 | ` * otherwise.` |
|        - | 10908 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10909 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10910 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10911 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10912 | ` * Refer to the official for more information on this powerful` |
|        - | 10913 | ` * extension.` |
|        - | 10914 | ` */` |
|  8781172 | 10915 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 10916 |  |
|        - | 10917 | `	VmRefObj *pRef;` |
|        - | 10918 | `	sxu32 nBucket;` |
|        - | 10919 | `	/* Point to the appropriate bucket */` |
|  8781174 | 10920 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 10921 | `	/* Perform the lookup */` |
|  8781174 | 10922 | `	pRef = pVm->apRefObj[nBucket];` |
| 18519249 | 10923 | `	for(;;){` |
| 37040129 | 10924 | `		if( pRef == 0 ){` |
|  3005734 | 10925 | `			break;` |
|        - | 10926 | `		}` |
| 34034397 | 10927 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 10928 | `			/* Entry found */` |
|  5775442 | 10929 | `			return pRef;` |
|        - | 10930 | `		}` |
|        - | 10931 | `		/* Point to the next entry */` |
| 28258957 | 10932 | `		pRef = pRef->pNextCollide;` |
|        2 | 10933 | `	}` |
|        - | 10934 | `	/* No such entry,return NULL */` |
|  3005734 | 10935 | `	return 0;` |
|  4390588 | 10936 |  |
|        - | 10937 | `/*` |
|        - | 10938 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10939 | ` *` |
|        - | 10940 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10941 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10942 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10943 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10944 | ` * Refer to the official for more information on this powerful` |
|        - | 10945 | ` * extension.` |
|        - | 10946 | ` */` |
|  2938686 | 10947 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10948 |  |
|        - | 10949 | `	sxu32 nBucket;` |
|  2938688 | 10950 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 10951 | `		VmRefObj **apNew;` |
|        - | 10952 | `		sxu32 nNew;` |
|        - | 10953 | `		/* Allocate a larger table */` |
|     2572 | 10954 | `		nNew = pVm->nRefSize << 1;` |
|     2572 | 10955 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     2572 | 10956 | `		if( apNew ){` |
|     2572 | 10957 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 10958 | `			sxu32 n;` |
|        - | 10959 | `			/* Zero the structure */` |
|     2572 | 10960 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 10961 | `			/* Rehash all referenced entries */` |
|  2825344 | 10962 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 10963 | `				/* Remove old collision links */` |
|  2822774 | 10964 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 10965 | `				/* Point to the appropriate bucket */` |
|  2822774 | 10966 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 10967 | `				/* Insert the entry  */` |
|  2822774 | 10968 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2822774 | 10969 | `				if( apNew[nBucket] ){` |
|  2298896 | 10970 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 10971 | `				}` |
|  2822774 | 10972 | `				apNew[nBucket] = pEntry;` |
|        - | 10973 | `				/* Point to the next entry */` |
|  2822774 | 10974 | `				pEntry = pEntry->pNext;` |
|  1411388 | 10975 | `			}` |
|        - | 10976 | `			/* Release the old table */` |
|     2572 | 10977 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 10978 | `			/* Install the new one */` |
|     2572 | 10979 | `			pVm->apRefObj = apNew;` |
|     2572 | 10980 | `			pVm->nRefSize = nNew;` |
|     1285 | 10981 | `		}` |
|     1285 | 10982 | `	}` |
|        - | 10983 | `	/* Point to the appropriate bucket */` |
|  2938688 | 10984 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 10985 | `	/* Insert the entry */` |
|  2938688 | 10986 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  2938688 | 10987 | `	if( pVm->apRefObj[nBucket] ){` |
|  2431871 | 10988 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1216413 | 10989 | `	}` |
|  2938688 | 10990 | `	pVm->apRefObj[nBucket] = pRef;` |
|  2938688 | 10991 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  2938688 | 10992 | `	pVm->nRefUsed++;` |
|  2938688 | 10993 | `	return SXRET_OK;` |
|        2 | 10994 |  |
|        - | 10995 | `/*` |
|        - | 10996 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 10997 | ` * the reference table.` |
|        - | 10998 | ` * This function is invoked when the user perform an unset` |
|        - | 10999 | ` * call [i.e: unset($var); ].` |
|        - | 11000 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 11001 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 11002 | ` * the reference implementation is consistent,solid and it's` |
|        - | 11003 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 11004 | ` * Refer to the official for more information on this powerful` |
|        - | 11005 | ` * extension.` |
|        - | 11006 | ` */` |
|  2914044 | 11007 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 11008 |  |
|        - | 11009 | `	ph7_hashmap_node **apNode;` |
|        - | 11010 | `	SyHashEntry **apEntry;` |
|        - | 11011 | `	sxu32 n;` |
|        - | 11012 | `	/* Point to the reference table */` |
|  2914046 | 11013 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2914046 | 11014 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 11015 | `	/* Unlink the entry from the reference table */` |
|  2986014 | 11016 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    71970 | 11017 | `		if( apEntry[n] ){` |
|    71920 | 11018 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    35959 | 11019 | `		}` |
|    35986 | 11020 | `	}` |
|  5758678 | 11021 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2844634 | 11022 | `		if( apNode[n] ){` |
|     5595 | 11023 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2797 | 11024 | `		}` |
|  1422318 | 11025 | `	}` |
|  2914046 | 11026 | `	if( pRef->pPrevCollide ){` |
|  1086734 | 11027 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   543348 | 11028 | `	}else{` |
|  1827314 | 11029 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 11030 | `	}` |
|  2914046 | 11031 | `	if( pRef->pNextCollide ){` |
|  1626659 | 11032 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   813838 | 11033 | `	}` |
|  2914046 | 11034 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 11035 | `	/* Release the node */` |
|  2914046 | 11036 | `	SySetRelease(&pRef->aReference);` |
|  2914046 | 11037 | `	SySetRelease(&pRef->aArrEntries);` |
|  2914046 | 11038 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2914046 | 11039 | `	pVm->nRefUsed--;` |
|  2914046 | 11040 | `	return SXRET_OK;` |
|        2 | 11041 |  |
|        - | 11042 | `/*` |
|        - | 11043 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 11044 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 11045 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 11046 | ` * the reference implementation is consistent,solid and it's` |
|        - | 11047 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 11048 | ` * Refer to the official for more information on this powerful` |
|        - | 11049 | ` * extension.` |
|        - | 11050 | ` */` |
|  2960988 | 11051 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 11052 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 11053 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 11054 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 11055 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 11056 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 11057 | `	)` |
|        2 | 11058 |  |
|  2960990 | 11059 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 11060 | `	VmRefObj *pRef;` |
|        - | 11061 | `	/* Check if the referenced object already exists */` |
|  2960990 | 11062 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2960990 | 11063 | `	if( pRef == 0 ){` |
|        - | 11064 | `		/* Create a new entry */` |
|  2938688 | 11065 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  2938688 | 11066 | `		if( pRef == 0 ){` |
|      ! 0 | 11067 | `			return SXERR_MEM;` |
|        - | 11068 | `		}` |
|  2938688 | 11069 | `		pRef->iFlags = iFlags;` |
|        - | 11070 | `		/* Install the entry */` |
|  2938688 | 11071 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1469343 | 11072 | `	}` |
|  2965902 | 11073 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 11074 | `		/* Safely ignore the exception frame */` |
|     4914 | 11075 | `		pFrame = pFrame->pParent;` |
|        2 | 11076 | `	}` |
|  2960990 | 11077 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 11078 | `		VmSlot sRef;` |
|        - | 11079 | `		/* Local frame,record referenced entry so that it can` |
|        - | 11080 | `		 * be deleted when we leave this frame.` |
|        - | 11081 | `		 */` |
|    67078 | 11082 | `		sRef.nIdx = nIdx;` |
|    67078 | 11083 | `		sRef.pUserData = pEntry;` |
|    67078 | 11084 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 11085 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 11086 | `		}` |
|    33538 | 11087 | `	}` |
|  2960990 | 11088 | `	if( pEntry ){` |
|        - | 11089 | `		/* Address of the hash-entry */` |
|    89194 | 11090 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    44596 | 11091 | `	}` |
|  2960990 | 11092 | `	if( pMapEntry ){` |
|        - | 11093 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2867730 | 11094 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1433864 | 11095 | `	}` |
|  2960990 | 11096 | `	return SXRET_OK;` |
|  1480496 | 11097 |  |
|        - | 11098 | `/*` |
|        - | 11099 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 11100 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 11101 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 11102 | ` * the reference implementation is consistent,solid and it's` |
|        - | 11103 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 11104 | ` * Refer to the official for more information on this powerful` |
|        - | 11105 | ` * extension.` |
|        - | 11106 | ` */` |
|  2906120 | 11107 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 11108 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 11109 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 11110 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 11111 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 11112 | `	)` |
|        2 | 11113 |  |
|        - | 11114 | `	VmRefObj *pRef;` |
|        - | 11115 | `	sxu32 n;` |
|        - | 11116 | `	/* Check if the referenced object already exists */` |
|  2906122 | 11117 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2906122 | 11118 | `	if( pRef == 0 ){` |
|        - | 11119 | `		/* Not such entry */` |
|    67028 | 11120 | `		return SXERR_NOTFOUND;` |
|        - | 11121 | `	}` |
|        - | 11122 | `	/* Remove the desired entry */` |
|  2839096 | 11123 | `	if( pEntry ){` |
|        - | 11124 | `		SyHashEntry **apEntry;` |
|       51 | 11125 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      195 | 11126 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      145 | 11127 | `			if( apEntry[n] == pEntry ){` |
|        - | 11128 | `				/* Nullify the entry */` |
|       51 | 11129 | `				apEntry[n] = 0;` |
|        - | 11130 | `				/*` |
|        - | 11131 | `				 * NOTE:` |
|        - | 11132 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 11133 | `				 * we avoid wasting spaces.` |
|        - | 11134 | `				 */` |
|       25 | 11135 | `			}` |
|       73 | 11136 | `		}` |
|       25 | 11137 | `	}` |
|  2839096 | 11138 | `	if( pMapEntry ){` |
|        - | 11139 | `		ph7_hashmap_node **apNode;` |
|  2839046 | 11140 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5678178 | 11141 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2839134 | 11142 | `			if( apNode[n] == pMapEntry ){` |
|        - | 11143 | `				/* nullify the entry */` |
|  2839046 | 11144 | `				apNode[n] = 0;` |
|  1419522 | 11145 | `			}` |
|  1419568 | 11146 | `		}` |
|  1419522 | 11147 | `	}` |
|  2839096 | 11148 | `	return SXRET_OK;` |
|  1453062 | 11149 |  |
|        - | 11150 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 11151 | `/*` |
|        - | 11152 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 11153 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 11154 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 11155 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 11156 | ` * For more information on how to register IO stream devices,please` |
|        - | 11157 | ` * refer to the official documentation.` |
|        - | 11158 | ` */` |
|    21928 | 11159 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 11160 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 11161 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 11162 | `	int nByte              /* *pzDevice length*/` |
|        - | 11163 | `	)` |
|        2 | 11164 |  |
|        - | 11165 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 11166 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 11167 | `	SyString sDev,sCur;` |
|        - | 11168 | `	sxu32 n,nEntry;` |
|        - | 11169 | `	int rc;` |
|        - | 11170 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    21930 | 11171 | `	zNext = zCur = zIn = *pzDevice;` |
|    21930 | 11172 | `	zEnd = &zIn[nByte];` |
|  1394797 | 11173 | `	while( zIn < zEnd ){` |
|  1372871 | 11174 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 11175 | `			/* Got one */` |
|        3 | 11176 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 11177 | `			break;` |
|        - | 11178 | `		}` |
|        - | 11179 | `		/* Advance the cursor */` |
|  1372869 | 11180 | `		zIn++;` |
|        2 | 11181 | `	}` |
|    21930 | 11182 | `	if( zIn >= zEnd ){` |
|        - | 11183 | `		/* No such scheme,return the default stream */` |
|    21928 | 11184 | `		return pVm->pDefStream;` |
|        - | 11185 | `	}` |
|        3 | 11186 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 11187 | `	/* Remove leading and trailing white spaces */` |
|        3 | 11188 | `	SyStringFullTrim(&sDev);` |
|        - | 11189 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 11190 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 11191 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 11192 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 11193 | `		pStream = apStream[n];` |
|        3 | 11194 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 11195 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 11196 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 11197 | `		if( rc == 0 ){` |
|        - | 11198 | `			/* Stream device found */` |
|        3 | 11199 | `			*pzDevice = zNext;` |
|        3 | 11200 | `			return pStream;` |
|        - | 11201 | `		}` |
|      ! 0 | 11202 | `	}` |
|        - | 11203 | `	/* No such stream,return NULL */` |
|      ! 0 | 11204 | `	return 0;` |
|    10966 | 11205 |  |
|        - | 11206 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 11207 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 11208 |  |
