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
|   752764 |    86 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    87 |  |
|   752766 |    88 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       23 |    89 | `		return TRUE;` |
|        - |    90 | `	}` |
|   752744 |    91 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |    92 | `		return TRUE;` |
|        - |    93 | `	}` |
|   752736 |    94 | `	return FALSE;` |
|   376406 |    95 |  |
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
|   338570 |   110 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   338572 |   121 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   338572 |   122 | `	if( pEntry ){` |
|        - |   123 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   124 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   125 | `		pCons->xExpand = xExpand;` |
|        6 |   126 | `		pCons->pUserData = pUserData;` |
|        6 |   127 | `		return SXRET_OK;` |
|        - |   128 | `	}` |
|        - |   129 | `	/* Allocate a new constant instance */` |
|   338568 |   130 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   338568 |   131 | `	if( pCons == 0 ){` |
|      ! 0 |   132 | `		return 0;` |
|        - |   133 | `	}` |
|        - |   134 | `	/* Duplicate constant name */` |
|   338568 |   135 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   338568 |   136 | `	if( zDupName == 0 ){` |
|      ! 0 |   137 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   138 | `		return 0;` |
|        - |   139 | `	}` |
|        - |   140 | `	/* Install the constant */` |
|   338568 |   141 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   338568 |   142 | `	pCons->xExpand = xExpand;` |
|   338568 |   143 | `	pCons->pUserData = pUserData;` |
|   338568 |   144 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   338568 |   145 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   146 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return rc;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* All done,constant can be invoked from PHP code */` |
|   338568 |   151 | `	return SXRET_OK;` |
|   169287 |   152 |  |
|        - |   153 | `/*` |
|        - |   154 | ` * Allocate a new foreign function instance.` |
|        - |   155 | ` * This function return SXRET_OK on success. Any other` |
|        - |   156 | ` * return value indicates failure.` |
|        - |   157 | ` * Please refer to the official documentation for an introduction to` |
|        - |   158 | ` * the foreign function mechanism.` |
|        - |   159 | ` */` |
|   729060 |   160 | `static sxi32 PH7_NewForeignFunction(` |
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
|   729062 |   171 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   729062 |   172 | `	if( pFunc == 0 ){` |
|      ! 0 |   173 | `		return SXERR_MEM;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* Duplicate function name */` |
|   729062 |   176 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   729062 |   177 | `	if( zDup == 0 ){` |
|      ! 0 |   178 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   179 | `		return SXERR_MEM;` |
|        - |   180 | `	}` |
|        - |   181 | `	/* Zero the structure */` |
|   729062 |   182 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   183 | `	/* Initialize structure fields */` |
|   729062 |   184 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   729062 |   185 | `	pFunc->pVm   = pVm;` |
|   729062 |   186 | `	pFunc->xFunc = xFunc;` |
|   729062 |   187 | `	pFunc->pUserData = pUserData;` |
|   729062 |   188 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   189 | `	/* Write a pointer to the new function */` |
|   729062 |   190 | `	*ppOut = pFunc;` |
|   729062 |   191 | `	return SXRET_OK;` |
|   364532 |   192 |  |
|        - |   193 | `/*` |
|        - |   194 | ` * Install a foreign function and it's associated callback so that` |
|        - |   195 | ` * it can be invoked from the target PHP code.` |
|        - |   196 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   197 | ` * return value indicates failure.` |
|        - |   198 | ` * Please refer to the official documentation for an introduction to` |
|        - |   199 | ` * the foreign function mechanism.` |
|        - |   200 | ` */` |
|   730736 |   201 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|   730738 |   212 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   730738 |   213 | `	if( pEntry ){` |
|     1678 |   214 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     1678 |   215 | `		pFunc->pUserData = pUserData;` |
|     1678 |   216 | `		pFunc->xFunc = xFunc;` |
|     1678 |   217 | `		SySetReset(&pFunc->aAux);` |
|     1678 |   218 | `		return SXRET_OK;` |
|        - |   219 | `	}` |
|        - |   220 | `	/* Create a new user function */` |
|   729062 |   221 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   729062 |   222 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   223 | `		return rc;` |
|        - |   224 | `	}` |
|        - |   225 | `	/* Install the function in the corresponding hashtable */` |
|   729062 |   226 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   729062 |   227 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   228 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   229 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   230 | `		return rc;` |
|        - |   231 | `	}` |
|        - |   232 | `	/* User function successfully installed */` |
|   729062 |   233 | `	return SXRET_OK;` |
|   365370 |   234 |  |
|        - |   235 | `/*` |
|        - |   236 | ` * Initialize a VM function.` |
|        - |   237 | ` */` |
|    81058 |   238 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   239 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   240 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   241 | `	const char *zName,  /* Function name */` |
|        - |   242 | `	sxu32 nByte,        /* zName length */` |
|        - |   243 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   244 | `	void *pUserData     /* Function private data */` |
|        - |   245 | `	)` |
|        2 |   246 |  |
|        - |   247 | `	/* Zero the structure */` |
|    81060 |   248 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   249 | `	/* Initialize structure fields */` |
|        - |   250 | `	/* Arguments container */` |
|    81060 |   251 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   252 | `	/* Static variable container */` |
|    81060 |   253 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   254 | `	/* Bytecode container */` |
|    81060 |   255 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   256 | `    /* Preallocate some instruction slots */` |
|    81060 |   257 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   258 | `	/* Closure environment */` |
|    81060 |   259 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|    81060 |   260 | `	pFunc->iFlags = iFlags;` |
|    81060 |   261 | `	pFunc->pUserData = pUserData;` |
|    81060 |   262 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|    81060 |   263 | `	return SXRET_OK;` |
|        2 |   264 |  |
|        - |   265 | `/*` |
|        - |   266 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   267 | ` */` |
|   258920 |   268 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   269 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   270 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   271 | `	SyString *pName     /* Function name */` |
|        - |   272 | `	)` |
|        2 |   273 |  |
|        - |   274 | `	SyHashEntry *pEntry;` |
|        - |   275 | `	sxi32 rc;` |
|   258922 |   276 | `	if( pName == 0 ){` |
|        - |   277 | `		/* Use the built-in name */` |
|    25338 |   278 | `		pName = &pFunc->sName;` |
|    12668 |   279 | `	}` |
|        - |   280 | `	/* Check for duplicates (functions with the same name) first */` |
|   258922 |   281 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   258922 |   282 | `	if( pEntry ){` |
|   193382 |   283 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   193382 |   284 | `		if( pLink != pFunc ){` |
|        - |   285 | `			/* Link */` |
|      179 |   286 | `			pFunc->pNextName = pLink;` |
|      179 |   287 | `			pEntry->pUserData = pFunc;` |
|       89 |   288 | `		}` |
|   193382 |   289 | `		return SXRET_OK;` |
|        - |   290 | `	}` |
|        - |   291 | `	/* First time seen */` |
|    65542 |   292 | `	pFunc->pNextName = 0;` |
|    65542 |   293 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    65542 |   294 | `	return rc;` |
|   129462 |   295 |  |
|        - |   296 | `/*` |
|        - |   297 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   298 | ` */` |
|    21312 |   299 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   300 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   301 | `	ph7_class *pClass /* Target Class */` |
|        - |   302 | `	)` |
|        2 |   303 |  |
|    21314 |   304 | `	SyString *pName = &pClass->sName;` |
|        - |   305 | `	SyHashEntry *pEntry;` |
|        - |   306 | `	sxi32 rc;` |
|        - |   307 | `	/* Check for duplicates */` |
|    21314 |   308 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    21314 |   309 | `	if( pEntry ){` |
|       31 |   310 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   311 | `		/* Link entry with the same name */` |
|       31 |   312 | `		pClass->pNextName = pLink;` |
|       31 |   313 | `		pEntry->pUserData = pClass;` |
|       31 |   314 | `		return SXRET_OK;` |
|        - |   315 | `	}` |
|    21284 |   316 | `	pClass->pNextName = 0;` |
|        - |   317 | `	/* Perform a simple hashtable insertion */` |
|    21284 |   318 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    21284 |   319 | `	return rc;` |
|    10658 |   320 |  |
|        - |   321 | `/*` |
|        - |   322 | ` * Instruction builder interface.` |
|        - |   323 | ` */` |
|  2173358 |   324 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  2173360 |   336 | `	sInstr.iOp = (sxu8)iOp;` |
|  2173360 |   337 | `	sInstr.iP1 = iP1;` |
|  2173360 |   338 | `	sInstr.iP2 = iP2;` |
|  2173360 |   339 | `	sInstr.p3  = p3;` |
|  2173360 |   340 | `	if( pIndex ){` |
|        - |   341 | `		/* Instruction index in the bytecode array */` |
|   138390 |   342 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    69194 |   343 | `	}` |
|        - |   344 | `	/* Finally,record the instruction */` |
|  2173360 |   345 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  2173360 |   346 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   347 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   348 | `		/* Fall throw */` |
|      ! 0 |   349 | `	}` |
|  2173360 |   350 | `	return rc;` |
|        2 |   351 |  |
|        - |   352 | `/*` |
|        - |   353 | ` * Swap the current bytecode container with the given one.` |
|        - |   354 | ` */` |
|   197064 |   355 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   356 |  |
|   197066 |   357 | `	if( pContainer == 0 ){` |
|        - |   358 | `		/* Point to the default container */` |
|      ! 0 |   359 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   360 | `	}else{` |
|        - |   361 | `		/* Change container */` |
|   197066 |   362 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   363 | `	}` |
|   197066 |   364 | `	return SXRET_OK;` |
|        2 |   365 |  |
|        - |   366 | `/*` |
|        - |   367 | ` * Return the current bytecode container.` |
|        - |   368 | ` */` |
|    98532 |   369 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   370 |  |
|    98534 |   371 | `	return pVm->pByteContainer;` |
|        2 |   372 |  |
|        - |   373 | `/*` |
|        - |   374 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   375 | ` */` |
|   136398 |   376 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   377 |  |
|        - |   378 | `	VmInstr *pInstr;` |
|   136400 |   379 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   136400 |   380 | `	return pInstr;` |
|        2 |   381 |  |
|        - |   382 | `/*` |
|        - |   383 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   384 | ` */` |
|   606594 |   385 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   386 |  |
|   606596 |   387 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   388 |  |
|        - |   389 | `/*` |
|        - |   390 | ` * Pop the last VM instruction.` |
|        - |   391 | ` */` |
|   129258 |   392 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   393 |  |
|   129260 |   394 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   395 |  |
|        - |   396 | `/*` |
|        - |   397 | ` * Peek the last VM instruction.` |
|        - |   398 | ` */` |
|   342522 |   399 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   400 |  |
|   342524 |   401 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   402 |  |
|     9796 |   403 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   404 |  |
|        - |   405 | `	VmInstr *aInstr;` |
|        - |   406 | `	sxu32 n;` |
|     9798 |   407 | `	n = SySetUsed(pVm->pByteContainer);` |
|     9798 |   408 | `	if( n < 2 ){` |
|      ! 0 |   409 | `		return 0;` |
|        - |   410 | `	}` |
|     9798 |   411 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|     9798 |   412 | `	return &aInstr[n - 2];` |
|     4900 |   413 |  |
|        - |   414 | `/*` |
|        - |   415 | ` * Allocate a new virtual machine frame.` |
|        - |   416 | ` */` |
|    12656 |   417 | `static VmFrame * VmNewFrame(` |
|        - |   418 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   419 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   420 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   421 | `	)` |
|        2 |   422 |  |
|        - |   423 | `	VmFrame *pFrame;` |
|        - |   424 | `	/* Allocate a new vm frame */` |
|    12658 |   425 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    12658 |   426 | `	if( pFrame == 0 ){` |
|      ! 0 |   427 | `		return 0;` |
|        - |   428 | `	}` |
|        - |   429 | `	/* Zero the structure */` |
|    12658 |   430 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   431 | `	/* Initialize frame fields */` |
|    12658 |   432 | `	pFrame->pUserData = pUserData;` |
|    12658 |   433 | `	pFrame->pThis = pThis;` |
|    12658 |   434 | `	pFrame->pVm = pVm;` |
|    12658 |   435 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    12658 |   436 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    12658 |   437 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    12658 |   438 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    12658 |   439 | `	return pFrame;` |
|     6330 |   440 |  |
|        - |   441 | `/*` |
|        - |   442 | ` * Enter a VM frame.` |
|        - |   443 | ` */` |
|    12656 |   444 | `static sxi32 VmEnterFrame(` |
|        - |   445 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   446 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   447 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   448 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   449 | `	)` |
|        2 |   450 |  |
|        - |   451 | `	VmFrame *pFrame;` |
|        - |   452 | `	/* Allocate a new frame */` |
|    12658 |   453 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    12658 |   454 | `	if( pFrame == 0 ){` |
|      ! 0 |   455 | `		return SXERR_MEM;` |
|        - |   456 | `	}` |
|        - |   457 | `	/* Link to the list of active VM frame */` |
|    12658 |   458 | `	pFrame->pParent = pVm->pFrame;` |
|    12658 |   459 | `	pVm->pFrame = pFrame;` |
|    12658 |   460 | `	if( ppFrame ){` |
|        - |   461 | `		/* Write a pointer to the new VM frame */` |
|    10744 |   462 | `		*ppFrame = pFrame;` |
|     5371 |   463 | `	}` |
|    12658 |   464 | `	return SXRET_OK;` |
|     6330 |   465 |  |
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
|    10740 |   512 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   513 |  |
|    10742 |   514 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    10742 |   515 | `	if( pCurFrame ){` |
|        - |   516 | `		/* Unlink from the list of active VM frame */` |
|    10742 |   517 | `		pVm->pFrame = pCurFrame->pParent;` |
|    10742 |   518 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   519 | `			VmSlot  *aSlot;` |
|        - |   520 | `			sxu32 n;` |
|        - |   521 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    10724 |   522 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    78110 |   523 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   524 | `				/* Unset the local variable */` |
|    67388 |   525 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    33695 |   526 | `			}` |
|        - |   527 | `			/* Remove local reference */` |
|    10724 |   528 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    78162 |   529 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    67440 |   530 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    33721 |   531 | `			}` |
|     5361 |   532 | `		}` |
|        - |   533 | `		/* Release internal containers */` |
|    10742 |   534 | `		SyHashRelease(&pCurFrame->hVar);` |
|    10742 |   535 | `		SySetRelease(&pCurFrame->sArg);` |
|    10742 |   536 | `		SySetRelease(&pCurFrame->sLocal);` |
|    10742 |   537 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   538 | `		/* Release the whole structure */` |
|    10742 |   539 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     5370 |   540 | `	}` |
|    10742 |   541 |  |
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
|    71486 |   658 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   659 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   660 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   661 | `	)` |
|        2 |   662 |  |
|        - |   663 | `	ph7_class_method *pMeth;` |
|        - |   664 | `	ph7_class_attr *pAttr;` |
|        - |   665 | `	SyHashEntry *pEntry;` |
|        - |   666 | `	sxi32 rc;` |
|        - |   667 | `	/* Reset the loop cursor */` |
|    71488 |   668 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   669 | `	/* Process only static and constant attribute */` |
|   252307 |   670 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   671 | `		/* Extract the current attribute */` |
|   145078 |   672 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   145078 |   673 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|    71488 |   695 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |   696 | `		/* Do not mount interface methods since they are signatures only.` |
|        - |   697 | `		 */` |
|    42534 |   698 | `		return SXRET_OK;` |
|        - |   699 | `	}` |
|        - |   700 | `	/* Create constructor alias if not yet done */` |
|    28956 |   701 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   702 | `		/* User constructor with the same base class name */` |
|      206 |   703 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      206 |   704 | `		if( pEntry ){` |
|      ! 0 |   705 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   706 | `			/* Create the alias */` |
|      ! 0 |   707 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   708 | `		}` |
|      102 |   709 | `	}` |
|        - |   710 | `	/* Install the methods now */` |
|    28956 |   711 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   277023 |   712 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   233592 |   713 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   233592 |   714 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   233586 |   715 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   233586 |   716 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   717 | `				return rc;` |
|        - |   718 | `			}` |
|   116792 |   719 | `		}` |
|        2 |   720 | `	}` |
|        - |   721 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    28956 |   722 | `	pClass->bMounted = TRUE;` |
|    28956 |   723 | `	return SXRET_OK;` |
|    35745 |   724 |  |
|        - |   725 | `/*` |
|        - |   726 | ` * Allocate a private frame for attributes of the given` |
|        - |   727 | ` * class instance (Object in the PHP jargon).` |
|        - |   728 | ` */` |
|      918 |   729 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   730 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   731 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   732 | `	)` |
|        2 |   733 |  |
|      920 |   734 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   735 | `	ph7_class_attr *pAttr;` |
|        - |   736 | `	SyHashEntry *pEntry;` |
|        - |   737 | `	sxi32 rc;` |
|        - |   738 | `	/* Install class attribute in the private frame associated with this instance */` |
|      920 |   739 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     3718 |   740 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   741 | `		VmClassAttr *pVmAttr;` |
|        - |   742 | `		/* Extract the current attribute */` |
|     2800 |   743 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     2800 |   744 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     2800 |   745 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   746 | `			return SXERR_MEM;` |
|        - |   747 | `		}` |
|     2800 |   748 | `		pVmAttr->pAttr = pAttr;` |
|     2800 |   749 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   750 | `			ph7_value *pMemObj;` |
|        - |   751 | `			/* Reserve a memory object for this attribute */` |
|     2794 |   752 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     2794 |   753 | `			if( pMemObj == 0 ){` |
|      ! 0 |   754 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   755 | `				return SXERR_MEM;` |
|        - |   756 | `			}` |
|     2794 |   757 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     2794 |   758 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   759 | `				/* Initialize attribute default value (any complex expression) */` |
|      908 |   760 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      453 |   761 | `			}` |
|     2794 |   762 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     2794 |   763 | `			if( rc != SXRET_OK ){` |
|        - |   764 | `				VmSlot sSlot;` |
|        - |   765 | `				/* Restore memory object */` |
|      ! 0 |   766 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   767 | `				sSlot.pUserData = 0;` |
|      ! 0 |   768 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   769 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   770 | `				return SXERR_MEM;` |
|        - |   771 | `			}` |
|        - |   772 | `			/* Install attribute in the reference table */` |
|     2794 |   773 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1398 |   774 | `		}else{` |
|        - |   775 | `			/* Install static/constant attribute */` |
|        8 |   776 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   777 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   778 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   779 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   780 | `				return SXERR_MEM;` |
|        - |   781 | `			}` |
|        - |   782 | `		}` |
|        2 |   783 | `	}` |
|      920 |   784 | `	return SXRET_OK;` |
|      461 |   785 |  |
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
|   239562 |   797 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   798 |  |
|        - |   799 | `	ph7_value *pObj;` |
|        - |   800 | `	sxi32 rc;` |
|   239564 |   801 | `	if( pIndex ){` |
|        - |   802 | `		/* Object index in the object table */` |
|   233822 |   803 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   116910 |   804 | `	}` |
|        - |   805 | `	/* Reserve a slot for the new object */` |
|   239564 |   806 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   239564 |   807 | `	if( rc != SXRET_OK ){` |
|        - |   808 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   809 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   810 | `		 */` |
|      ! 0 |   811 | `		return 0;` |
|        - |   812 | `	}` |
|   239564 |   813 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   239564 |   814 | `	return pObj;` |
|   119783 |   815 |  |
|        - |   816 | `/*` |
|        - |   817 | ` * Reserve a memory object.` |
|        - |   818 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   819 | ` */` |
|  2129750 |   820 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   821 |  |
|        - |   822 | `	ph7_value *pObj;` |
|        - |   823 | `	sxi32 rc;` |
|  2129752 |   824 | `	if( pIndex ){` |
|        - |   825 | `		/* Object index in the object table */` |
|  2129752 |   826 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1064875 |   827 | `	}` |
|        - |   828 | `	/* Reserve a slot for the new object */` |
|  2129752 |   829 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2129752 |   830 | `	if( rc != SXRET_OK ){` |
|        - |   831 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   832 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   833 | `		 */` |
|      ! 0 |   834 | `		return 0;` |
|        - |   835 | `	}` |
|  2129752 |   836 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2129752 |   837 | `	return pObj;` |
|  1064877 |   838 |  |
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
|        - |  1067 | `	"function array_merge_recursive(){"\` |
|        - |  1068 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1069 | `    "$arrays = func_get_args();"\` |
|        - |  1070 | `    "$narrays = count($arrays);"\` |
|        - |  1071 | `    "$ret = array();"\` |
|        - |  1072 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1073 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1074 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1075 | `	 " }"\` |
|        - |  1076 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1077 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1078 | `     "  if( $keyIsInt ) {"\` |
|        - |  1079 | `     "   $ret[] = $value;"\` |
|        - |  1080 | `     "  } else {"\` |
|        - |  1081 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1082 | `     "    $cur = $ret[$key];"\` |
|        - |  1083 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1084 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1085 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1086 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1087 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1088 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1089 | `     "    } else {"\` |
|        - |  1090 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1091 | `     "    }"\` |
|        - |  1092 | `     "   } else {"\` |
|        - |  1093 | `     "    $ret[$key] = $value;"\` |
|        - |  1094 | `     "   }"\` |
|        - |  1095 | `     "  }"\` |
|        - |  1096 | `     " }"\` |
|        - |  1097 | `	 " }"\` |
|        - |  1098 | `	 " return $ret;"\` |
|        - |  1099 | `    "}"\` |
|        - |  1100 | `	"function max(){"\` |
|        - |  1101 | `    "  $pArgs = func_get_args();"\` |
|        - |  1102 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1103 | `	"  return null;"\` |
|        - |  1104 | `    " }"\` |
|        - |  1105 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1106 | `    " $pArg = $pArgs[0];"\` |
|        - |  1107 | `	" if( !is_array($pArg) ){"\` |
|        - |  1108 | `	"   return $pArg; "\` |
|        - |  1109 | `	" }"\` |
|        - |  1110 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1111 | `	"   return null;"\` |
|        - |  1112 | `	" }"\` |
|        - |  1113 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1114 | `	" reset($pArg);"\` |
|        - |  1115 | `	" $max = current($pArg);"\` |
|        - |  1116 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1117 | `	"   if( $val > $max ){"\` |
|        - |  1118 | `	"     $max = $val;"\` |
|        - |  1119 | `    " }"\` |
|        - |  1120 | `	" }"\` |
|        - |  1121 | `	" return $max;"\` |
|        - |  1122 | `    " }"\` |
|        - |  1123 | `    " $max = $pArgs[0];"\` |
|        - |  1124 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1125 | `    " $val = $pArgs[$i];"\` |
|        - |  1126 | `	"if( $val > $max ){"\` |
|        - |  1127 | `	" $max = $val;"\` |
|        - |  1128 | `	"}"\` |
|        - |  1129 | `    " }"\` |
|        - |  1130 | `	" return $max;"\` |
|        - |  1131 | `    "}"\` |
|        - |  1132 | `	"function min(){"\` |
|        - |  1133 | `    "  $pArgs = func_get_args();"\` |
|        - |  1134 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1135 | `	"  return null;"\` |
|        - |  1136 | `    " }"\` |
|        - |  1137 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1138 | `    " $pArg = $pArgs[0];"\` |
|        - |  1139 | `	" if( !is_array($pArg) ){"\` |
|        - |  1140 | `	"   return $pArg; "\` |
|        - |  1141 | `	" }"\` |
|        - |  1142 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1143 | `	"   return null;"\` |
|        - |  1144 | `	" }"\` |
|        - |  1145 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1146 | `	" reset($pArg);"\` |
|        - |  1147 | `	" $min = current($pArg);"\` |
|        - |  1148 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1149 | `	"   if( $val < $min ){"\` |
|        - |  1150 | `	"     $min = $val;"\` |
|        - |  1151 | `    " }"\` |
|        - |  1152 | `	" }"\` |
|        - |  1153 | `	" return $min;"\` |
|        - |  1154 | `    " }"\` |
|        - |  1155 | `    " $min = $pArgs[0];"\` |
|        - |  1156 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1157 | `    " $val = $pArgs[$i];"\` |
|        - |  1158 | `	"if( $val < $min ){"\` |
|        - |  1159 | `	" $min = $val;"\` |
|        - |  1160 | `	" }"\` |
|        - |  1161 | `    " }"\` |
|        - |  1162 | `	" return $min;"\` |
|        - |  1163 | `	"}"\` |
|        - |  1164 | `	"function fileowner(string $file){"\` |
|        - |  1165 | `    " $a = stat($file);"\` |
|        - |  1166 | `	" if( !is_array($a) ){"\` |
|        - |  1167 | `	"	return false;"\` |
|        - |  1168 | `	" }"\` |
|        - |  1169 | `	" return $a['uid'];"\` |
|        - |  1170 | `    "}"\` |
|        - |  1171 | `    "function filegroup(string $file){"\` |
|        - |  1172 | `	" $a = stat($file);"\` |
|        - |  1173 | `	" if( !is_array($a) ){"\` |
|        - |  1174 | `	"	return false;"\` |
|        - |  1175 | `	" }"\` |
|        - |  1176 | `	" return $a['gid'];"\` |
|        - |  1177 | `    "}"\` |
|        - |  1178 | `	 "function fileinode(string $file){"\` |
|        - |  1179 | `	" $a = stat($file);"\` |
|        - |  1180 | `	" if( !is_array($a) ){"\` |
|        - |  1181 | `	"	return false;"\` |
|        - |  1182 | `	" }"\` |
|        - |  1183 | `	" return $a['ino'];"\` |
|        - |  1184 | `    "}"` |
|        - |  1185 |  |
|        - |  1186 | `/*` |
|        - |  1187 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1188 | ` * start compiling the target PHP program.` |
|        - |  1189 | ` */` |
|     1914 |  1190 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1191 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1192 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1193 | `	 )` |
|        2 |  1194 |  |
|        - |  1195 | `	SyString sBuiltin;` |
|        - |  1196 | `	ph7_value *pObj;` |
|        - |  1197 | `	sxi32 rc;` |
|        - |  1198 | `	/* Zero the structure */` |
|     1916 |  1199 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1200 | `	/* Initialize VM fields */` |
|     1916 |  1201 | `	pVm->pEngine = &(*pEngine);` |
|     1916 |  1202 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1203 | `	/* Instructions containers */` |
|     1916 |  1204 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     1916 |  1205 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     1916 |  1206 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1207 | `	/* Object containers */` |
|     1916 |  1208 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1916 |  1209 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1210 | `	/* Virtual machine internal containers */` |
|     1916 |  1211 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     1916 |  1212 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     1916 |  1213 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     1916 |  1214 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1916 |  1215 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     1916 |  1216 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     1916 |  1217 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     1916 |  1218 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     1916 |  1219 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     1916 |  1220 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     1916 |  1221 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     1916 |  1222 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     1916 |  1223 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     1916 |  1224 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     1916 |  1225 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|        - |  1226 | `	/* Configuration containers */` |
|     1916 |  1227 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     1916 |  1228 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     1916 |  1229 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     1916 |  1230 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     1916 |  1231 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1232 | `	/* Error callbacks containers */` |
|     1916 |  1233 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     1916 |  1234 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     1916 |  1235 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     1916 |  1236 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     1916 |  1237 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1238 | `	/* Set a default recursion limit */` |
|        - |  1239 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     1916 |  1240 | `	pVm->nMaxDepth = 32;` |
|        - |  1241 | `#else` |
|        - |  1242 | `	pVm->nMaxDepth = 16;` |
|        - |  1243 | `#endif` |
|        - |  1244 | `	/* Default assertion flags */` |
|     1916 |  1245 | `	pVm->iAssertFlags = PH7_ASSERT_WARNING; /* Issue a warning for each failed assertion */` |
|        - |  1246 | `	/* JSON return status */` |
|     1916 |  1247 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1248 | `	/* PRNG context */` |
|     1916 |  1249 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1250 | `	/* Install the null constant */` |
|     1916 |  1251 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1916 |  1252 | `	if( pObj == 0 ){` |
|      ! 0 |  1253 | `		rc = SXERR_MEM;` |
|      ! 0 |  1254 | `		goto Err;` |
|        - |  1255 | `	}` |
|     1916 |  1256 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1257 | `	/* Install the boolean TRUE constant */` |
|     1916 |  1258 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1916 |  1259 | `	if( pObj == 0 ){` |
|      ! 0 |  1260 | `		rc = SXERR_MEM;` |
|      ! 0 |  1261 | `		goto Err;` |
|        - |  1262 | `	}` |
|     1916 |  1263 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1264 | `	/* Install the boolean FALSE constant */` |
|     1916 |  1265 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1916 |  1266 | `	if( pObj == 0 ){` |
|      ! 0 |  1267 | `		rc = SXERR_MEM;` |
|      ! 0 |  1268 | `		goto Err;` |
|        - |  1269 | `	}` |
|     1916 |  1270 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1271 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1272 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1273 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     1916 |  1274 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     1916 |  1275 | `	if( pObj == 0 ){` |
|      ! 0 |  1276 | `		rc = SXERR_MEM;` |
|      ! 0 |  1277 | `		goto Err;` |
|        - |  1278 | `	}` |
|     1916 |  1279 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1280 | `	/* Create the global frame */` |
|     1916 |  1281 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     1916 |  1282 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1283 | `		goto Err;` |
|        - |  1284 | `	}` |
|        - |  1285 | `	/* Initialize the code generator */` |
|     1916 |  1286 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1916 |  1287 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1288 | `		goto Err;` |
|        - |  1289 | `	}` |
|        - |  1290 | `	/* VM correctly initialized,set the magic number */` |
|     1916 |  1291 | `	pVm->nMagic = PH7_VM_INIT;` |
|     1916 |  1292 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1293 | `	/* Compile the built-in library */` |
|     1916 |  1294 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1295 | `	/* Reset the code generator */` |
|     1916 |  1296 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1916 |  1297 | `	return SXRET_OK;` |
|      ! 0 |  1298 | `Err:` |
|      ! 0 |  1299 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1300 | `	return rc;` |
|      959 |  1301 |  |
|        - |  1302 | `/*` |
|        - |  1303 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1304 | ` * routine which store the output in an internal blob.` |
|        - |  1305 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1306 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1307 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1308 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1309 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1310 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1311 | ` * to finish executing and extracting the output.` |
|        - |  1312 | ` */` |
|      ! 0 |  1313 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1314 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1315 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1316 | `	void *pUserData     /* User private data */` |
|        - |  1317 | `	)` |
|      ! 0 |  1318 |  |
|        - |  1319 | `	 sxi32 rc;` |
|        - |  1320 | `	 /* Store the output in an internal BLOB */` |
|      ! 0 |  1321 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|      ! 0 |  1322 | `	 return rc;` |
|      ! 0 |  1323 |  |
|        - |  1324 | `#define VM_STACK_GUARD 16` |
|        - |  1325 | `/*` |
|        - |  1326 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1327 | ` * our compiled PHP program.` |
|        - |  1328 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1329 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1330 | ` */` |
|    27048 |  1331 | `static ph7_value * VmNewOperandStack(` |
|        - |  1332 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1333 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1334 | `	)` |
|        2 |  1335 |  |
|        - |  1336 | `	ph7_value *pStack;` |
|        - |  1337 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1338 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1339 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1340 | `  ** on the maximum stack depth required.` |
|        - |  1341 | `  **` |
|        - |  1342 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1343 | `  */` |
|    27050 |  1344 | `	nInstr += VM_STACK_GUARD;` |
|    27050 |  1345 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    27050 |  1346 | `	if( pStack == 0 ){` |
|      ! 0 |  1347 | `		return 0;` |
|        - |  1348 | `	}` |
|        - |  1349 | `	/* Initialize the operand stack */` |
|  1728634 |  1350 | `	while( nInstr > 0 ){` |
|  1701586 |  1351 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1701586 |  1352 | `		--nInstr;` |
|        2 |  1353 | `	}` |
|        - |  1354 | `	/* Ready for bytecode execution */` |
|    27050 |  1355 | `	return pStack;` |
|    13526 |  1356 |  |
|        - |  1357 | `/* Forward declaration */` |
|        - |  1358 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1359 | `/*` |
|        - |  1360 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1361 | ` * This routine gets called by the PH7 engine after` |
|        - |  1362 | ` * successful compilation of the target PHP program.` |
|        - |  1363 | ` */` |
|     1676 |  1364 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1365 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1366 | `	)` |
|        2 |  1367 |  |
|        - |  1368 | `	SyHashEntry *pEntry;` |
|        - |  1369 | `	sxi32 rc;` |
|     1678 |  1370 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1371 | `		/* Initialize your VM first */` |
|      ! 0 |  1372 | `		return SXERR_CORRUPT;` |
|        - |  1373 | `	}` |
|        - |  1374 | `	/* Mark the VM ready for byte-code execution */` |
|     1678 |  1375 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1376 | `	/* Release the code generator now we have compiled our program */` |
|     1678 |  1377 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1378 | `	/* Emit the DONE instruction */` |
|     1678 |  1379 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     1678 |  1380 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1381 | `		return SXERR_MEM;` |
|        - |  1382 | `	}` |
|        - |  1383 | `	/* Script return value */` |
|     1678 |  1384 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1385 | `	/* Allocate a new operand stack */` |
|     1678 |  1386 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     1678 |  1387 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1388 | `		return SXERR_MEM;` |
|        - |  1389 | `	}` |
|        - |  1390 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1391 | `	 * private data. */` |
|     1678 |  1392 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     1678 |  1393 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1394 | `	/* Allocate the reference table */` |
|     1678 |  1395 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     1678 |  1396 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     1678 |  1397 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1398 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1399 | `		return SXERR_MEM;` |
|        - |  1400 | `	}` |
|        - |  1401 | `	/* Zero the reference table */` |
|     1678 |  1402 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1403 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     1678 |  1404 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     1678 |  1405 | `	if( rc != SXRET_OK ){` |
|        - |  1406 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1407 | `		return rc;` |
|        - |  1408 | `	}` |
|        - |  1409 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     1678 |  1410 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     1678 |  1411 | `	if( rc != SXRET_OK ){` |
|        - |  1412 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1413 | `		return rc;` |
|        - |  1414 | `	}` |
|        - |  1415 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     1678 |  1416 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1417 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     1678 |  1418 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1419 | `	/* Initialize and install static and constants class attributes */` |
|     1678 |  1420 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    20144 |  1421 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    18468 |  1422 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    18468 |  1423 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1424 | `			return rc;` |
|        - |  1425 | `		}` |
|        2 |  1426 | `	}` |
|        - |  1427 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     1678 |  1428 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1429 | `	/* VM is ready for bytecode execution */` |
|     1678 |  1430 | `	return SXRET_OK;` |
|      840 |  1431 |  |
|        - |  1432 | `/*` |
|        - |  1433 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1434 | ` */` |
|      ! 0 |  1435 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1436 |  |
|      ! 0 |  1437 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1438 | `		return SXERR_CORRUPT;` |
|        - |  1439 | `	}` |
|        - |  1440 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1441 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1442 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1443 | `	/* Set the ready flag */` |
|      ! 0 |  1444 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1445 | `	return SXRET_OK;` |
|      ! 0 |  1446 |  |
|        - |  1447 | `/*` |
|        - |  1448 | ` * Release a Virtual Machine.` |
|        - |  1449 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1450 | ` */` |
|     1668 |  1451 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1452 |  |
|        - |  1453 | `	/* Set the stale magic number */` |
|     1670 |  1454 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1455 | `	/* Release the private memory subsystem */` |
|     1670 |  1456 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     1670 |  1457 | `	return SXRET_OK;` |
|        2 |  1458 |  |
|        - |  1459 | `/*` |
|        - |  1460 | ` * Initialize a foreign function call context.` |
|        - |  1461 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1462 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1463 | ` * functions.` |
|        - |  1464 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1465 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1466 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1467 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1468 | ` */` |
|   529772 |  1469 | `static sxi32 VmInitCallContext(` |
|        - |  1470 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1471 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1472 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1473 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1474 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1475 | `	)` |
|        2 |  1476 |  |
|   529774 |  1477 | `	pOut->pFunc = pFunc;` |
|   529774 |  1478 | `	pOut->pVm   = pVm;` |
|   529774 |  1479 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   529774 |  1480 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1481 | `	/* Assume a null return value */` |
|   529774 |  1482 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   529774 |  1483 | `	pOut->pRet = pRet;` |
|   529774 |  1484 | `	pOut->iFlags = iFlags;` |
|   529774 |  1485 | `	return SXRET_OK;` |
|        2 |  1486 |  |
|        - |  1487 | `/*` |
|        - |  1488 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1489 | ` * left behind.` |
|        - |  1490 | ` */` |
|   529772 |  1491 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1492 |  |
|        - |  1493 | `	sxu32 n;` |
|   529774 |  1494 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6198 |  1495 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    17506 |  1496 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    11310 |  1497 | `			if( apObj[n] == 0 ){` |
|        - |  1498 | `				/* Already released */` |
|      250 |  1499 | `				continue;` |
|        - |  1500 | `			}` |
|    11062 |  1501 | `			PH7_MemObjRelease(apObj[n]);` |
|    11062 |  1502 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     5532 |  1503 | `		}` |
|     6198 |  1504 | `		SySetRelease(&pCtx->sVar);` |
|     3098 |  1505 | `	}` |
|   529774 |  1506 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1507 | `		ph7_aux_data *aAux;` |
|        - |  1508 | `		void *pChunk;` |
|        - |  1509 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1510 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1511 | `		 */` |
|        9 |  1512 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1513 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1514 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1515 | `			/* Release the chunk */` |
|       25 |  1516 | `			if( pChunk ){` |
|       25 |  1517 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1518 | `			}` |
|       13 |  1519 | `		}` |
|        9 |  1520 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1521 | `	}` |
|   529774 |  1522 |  |
|        - |  1523 | `/*` |
|        - |  1524 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1525 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1526 | ` */` |
|      248 |  1527 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1528 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1529 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1530 | `	)` |
|        2 |  1531 |  |
|      250 |  1532 | `	if( pValue == 0 ){` |
|        - |  1533 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1534 | `		return;` |
|        - |  1535 | `	}` |
|      250 |  1536 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      250 |  1537 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1538 | `		sxu32 n;` |
|      936 |  1539 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      936 |  1540 | `			if( apObj[n] == pValue ){` |
|      250 |  1541 | `				PH7_MemObjRelease(pValue);` |
|      250 |  1542 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1543 | `				/* Mark as released */` |
|      250 |  1544 | `				apObj[n] = 0;` |
|      250 |  1545 | `				break;` |
|        - |  1546 | `			}` |
|      345 |  1547 | `		}` |
|      124 |  1548 | `	}` |
|      126 |  1549 |  |
|        - |  1550 | `/*` |
|        - |  1551 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1552 | ` */` |
|  3157240 |  1553 | `static void VmPopOperand(` |
|        - |  1554 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1555 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1556 | `	)` |
|        2 |  1557 |  |
|  3157242 |  1558 | `	ph7_value *pTos = *ppTos;` |
|  6687612 |  1559 | `	while( nPop > 0 ){` |
|  3530372 |  1560 | `		PH7_MemObjRelease(pTos);` |
|  3530372 |  1561 | `		pTos--;` |
|  3530372 |  1562 | `		nPop--;` |
|        2 |  1563 | `	}` |
|        - |  1564 | `	/* Top of the stack */` |
|  3157242 |  1565 | `	*ppTos = pTos;` |
|  3157242 |  1566 |  |
|        - |  1567 | `/*` |
|        - |  1568 | ` * Reserve a memory object.` |
|        - |  1569 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1570 | ` */` |
|  2945406 |  1571 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1572 |  |
|  2945408 |  1573 | `	ph7_value *pObj = 0;` |
|        - |  1574 | `	VmSlot *pSlot;` |
|        - |  1575 | `	sxu32 nIdx;` |
|        - |  1576 | `	/* Check for a free slot */` |
|  2945408 |  1577 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  2945408 |  1578 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  2945408 |  1579 | `	if( pSlot ){` |
|   815658 |  1580 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   815658 |  1581 | `		nIdx = pSlot->nIdx;` |
|   407828 |  1582 | `	}` |
|  2945408 |  1583 | `	if( pObj == 0 ){` |
|        - |  1584 | `		/* Reserve a new memory object */` |
|  2129752 |  1585 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2129752 |  1586 | `		if( pObj == 0 ){` |
|      ! 0 |  1587 | `			return 0;` |
|        - |  1588 | `		}` |
|  1064875 |  1589 | `	}` |
|        - |  1590 | `	/* Set a null default value */` |
|  2945408 |  1591 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  2945408 |  1592 | `	pObj->nIdx = nIdx;` |
|  2945408 |  1593 | `	return pObj;` |
|  1472705 |  1594 |  |
|        - |  1595 | `/*` |
|        - |  1596 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1597 | ` */` |
|    22174 |  1598 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1599 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1600 | `	const char *zKey,  /* Entry key */` |
|        - |  1601 | `	sxu32 nByte,       /* Key length */` |
|        - |  1602 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1603 | `	)` |
|        2 |  1604 |  |
|        - |  1605 | `	ph7_value sKey;` |
|        - |  1606 | `	sxi32 rc;` |
|    22176 |  1607 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    22176 |  1608 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1609 | `	/* Perform the insertion */` |
|    22176 |  1610 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    22176 |  1611 | `	PH7_MemObjRelease(&sKey);` |
|    22176 |  1612 | `	return rc;` |
|        2 |  1613 |  |
|        - |  1614 | `/*` |
|        - |  1615 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1616 | ` * Return a pointer to the variable value on success.` |
|        - |  1617 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1618 | ` */` |
|  2974142 |  1619 | `static ph7_value * VmExtractMemObj(` |
|        - |  1620 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1621 | `	const SyString *pName, /* Variable name */` |
|        - |  1622 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1623 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1624 | `	)` |
|        2 |  1625 |  |
|  2974144 |  1626 | `	int bNullify = FALSE;` |
|        - |  1627 | `	SyHashEntry *pEntry;` |
|        - |  1628 | `	VmFrame *pFrame;` |
|        - |  1629 | `	ph7_value *pObj;` |
|        - |  1630 | `	sxu32 nIdx;` |
|        - |  1631 | `	sxi32 rc;` |
|        - |  1632 | `	/* Point to the top active frame */` |
|  2974144 |  1633 | `	pFrame = pVm->pFrame;` |
|  3023496 |  1634 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1635 | `		/* Safely ignore the exception frame */` |
|    49353 |  1636 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        1 |  1637 | `	}` |
|        - |  1638 | `	/* Perform the lookup */` |
|  2974144 |  1639 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1640 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1641 | `		pName = &sAnnon;` |
|        - |  1642 | `		/* Always nullify the object */` |
|      ! 0 |  1643 | `		bNullify = TRUE;` |
|      ! 0 |  1644 | `		bDup = FALSE;` |
|      ! 0 |  1645 | `	}` |
|        - |  1646 | `	/* Check the superglobals table first */` |
|  2974144 |  1647 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  2974144 |  1648 | `	if( pEntry == 0 ){` |
|        - |  1649 | `		/* Query the top active frame */` |
|  2974108 |  1650 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  2974108 |  1651 | `		if( pEntry == 0 ){` |
|    73352 |  1652 | `			char *zName = (char *)pName->zString;` |
|        - |  1653 | `			VmSlot sLocal;` |
|    73352 |  1654 | `			if( !bCreate ){` |
|        - |  1655 | `				/* Do not create the variable,return NULL instead */` |
|      576 |  1656 | `				return 0;` |
|        - |  1657 | `			}` |
|        - |  1658 | `			/* No such variable,automatically create a new one and install` |
|        - |  1659 | `			 * it in the current frame.` |
|        - |  1660 | `			 */` |
|    72778 |  1661 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    72778 |  1662 | `			if( pObj == 0 ){` |
|      ! 0 |  1663 | `				return 0;` |
|        - |  1664 | `			}` |
|    72778 |  1665 | `			nIdx = pObj->nIdx;` |
|    72778 |  1666 | `			if( bDup ){` |
|        - |  1667 | `				/* Duplicate name */` |
|      164 |  1668 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      164 |  1669 | `				if( zName == 0 ){` |
|      ! 0 |  1670 | `					return 0;` |
|        - |  1671 | `				}` |
|       81 |  1672 | `			}` |
|        - |  1673 | `			/* Link to the top active VM frame */` |
|    72778 |  1674 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    72778 |  1675 | `			if( rc != SXRET_OK ){` |
|        - |  1676 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1677 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1678 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1679 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1680 | `				return 0;` |
|        - |  1681 | `			}` |
|    72778 |  1682 | `			if( pFrame->pParent != 0 ){` |
|        - |  1683 | `				/* Local variable */` |
|    67388 |  1684 | `				sLocal.nIdx = nIdx;` |
|    67388 |  1685 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    33695 |  1686 | `			}else{` |
|        - |  1687 | `				/* Register in the $GLOBALS array */` |
|     5392 |  1688 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1689 | `			}` |
|        - |  1690 | `			/* Install in the reference table */` |
|    72778 |  1691 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1692 | `			/* Save object index */` |
|    72778 |  1693 | `			pObj->nIdx = nIdx;` |
|    36390 |  1694 | `		}else{` |
|        - |  1695 | `			/* Extract variable contents */` |
|  2900758 |  1696 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  2900758 |  1697 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2900758 |  1698 | `			if( bNullify && pObj ){` |
|      ! 0 |  1699 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1700 | `			}` |
|        - |  1701 | `		}` |
|  1486878 |  1702 | `	}else{` |
|        - |  1703 | `		/* Superglobal */` |
|       38 |  1704 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1705 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1706 | `	}` |
|  2973570 |  1707 | `	return pObj;` |
|  1487183 |  1708 |  |
|        - |  1709 | `/*` |
|        - |  1710 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1711 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1712 | ` */` |
|     1702 |  1713 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1714 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1715 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1716 | `	sxu32 nByte        /* zName length */` |
|        - |  1717 | `	)` |
|        2 |  1718 |  |
|        - |  1719 | `	SyHashEntry *pEntry;` |
|        - |  1720 | `	ph7_value *pValue;` |
|        - |  1721 | `	sxu32 nIdx;` |
|        - |  1722 | `	/* Query the superglobal table */` |
|     1704 |  1723 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     1704 |  1724 | `	if( pEntry == 0 ){` |
|        - |  1725 | `		/* No such entry */` |
|      ! 0 |  1726 | `		return 0;` |
|        - |  1727 | `	}` |
|        - |  1728 | `	/* Extract the superglobal index in the global object pool */` |
|     1704 |  1729 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1730 | `	/* Extract the variable value  */` |
|     1704 |  1731 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     1704 |  1732 | `	return pValue;` |
|      853 |  1733 |  |
|        - |  1734 | `/*` |
|        - |  1735 | ` * Perform a raw hashmap insertion.` |
|        - |  1736 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1737 | ` */` |
|     1700 |  1738 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1739 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1740 | `	const char *zKey,   /* Entry key */` |
|        - |  1741 | `	int nKeylen,        /* zKey length*/` |
|        - |  1742 | `	const char *zData,  /* Entry data */` |
|        - |  1743 | `	int nLen            /* zData length */` |
|        - |  1744 | `	)` |
|        2 |  1745 |  |
|        - |  1746 | `	ph7_value sKey,sValue;` |
|        - |  1747 | `	sxi32 rc;` |
|     1702 |  1748 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     1702 |  1749 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     1702 |  1750 | `	if( zKey ){` |
|     1680 |  1751 | `		if( nKeylen < 0 ){` |
|     1680 |  1752 | `			nKeylen = (int)SyStrlen(zKey);` |
|      839 |  1753 | `		}` |
|     1680 |  1754 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|      839 |  1755 | `	}` |
|     1702 |  1756 | `	if( zData ){` |
|     1702 |  1757 | `		if( nLen < 0 ){` |
|        - |  1758 | `			/* Compute length automatically */` |
|      ! 0 |  1759 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1760 | `		}` |
|     1702 |  1761 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|      850 |  1762 | `	}` |
|        - |  1763 | `	/* Perform the insertion */` |
|     1702 |  1764 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     1702 |  1765 | `	PH7_MemObjRelease(&sKey);` |
|     1702 |  1766 | `	PH7_MemObjRelease(&sValue);` |
|     1702 |  1767 | `	return rc;` |
|        2 |  1768 |  |
|        - |  1769 | `/*` |
|        - |  1770 | ` * Configure a working virtual machine instance.` |
|        - |  1771 | ` *` |
|        - |  1772 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1773 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1774 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1775 | ` * The second argument to this function is an integer configuration option` |
|        - |  1776 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1777 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1778 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1779 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1780 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1781 | ` */` |
|    26840 |  1782 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1783 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1784 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1785 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1786 | `	)` |
|        2 |  1787 |  |
|    26842 |  1788 | `	sxi32 rc = SXRET_OK;` |
|    26842 |  1789 | `	switch(nOp){` |
|      838 |  1790 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     1678 |  1791 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     1678 |  1792 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1793 | `		/* VM output consumer callback */` |
|        - |  1794 | `#ifdef UNTRUST` |
|        - |  1795 | `		if( xConsumer == 0 ){` |
|        - |  1796 | `			rc = SXERR_CORRUPT;` |
|        - |  1797 | `			break;` |
|        - |  1798 | `		}` |
|        - |  1799 | `#endif` |
|        - |  1800 | `		/* Install the output consumer */` |
|     1678 |  1801 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     1678 |  1802 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     1678 |  1803 | `		break;` |
|        - |  1804 | `							   }` |
|      838 |  1805 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1806 | `		/* Import path */` |
|        - |  1807 | `		  const char *zPath;` |
|        - |  1808 | `		  SyString sPath;` |
|     1678 |  1809 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1810 | `#if defined(UNTRUST)` |
|        - |  1811 | `		  if( zPath == 0 ){` |
|        - |  1812 | `			  rc = SXERR_EMPTY;` |
|        - |  1813 | `			  break;` |
|        - |  1814 | `		  }` |
|        - |  1815 | `#endif` |
|     1678 |  1816 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1817 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1818 | `#ifdef __WINNT__` |
|        2 |  1819 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1820 | `#endif` |
|     3354 |  1821 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1822 | `		  /* Remove leading and trailing white spaces */` |
|     1678 |  1823 | `		  SyStringFullTrim(&sPath);` |
|     1678 |  1824 | `		  if( sPath.nByte > 0 ){` |
|        - |  1825 | `			  /* Store the path in the corresponding conatiner */` |
|     1678 |  1826 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      838 |  1827 | `		  }` |
|     1678 |  1828 | `		  break;` |
|        - |  1829 | `									 }` |
|      838 |  1830 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1831 | `		/* Run-Time Error report */` |
|     1678 |  1832 | `		pVm->bErrReport = 1;` |
|     1678 |  1833 | `		break;` |
|      ! 0 |  1834 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1835 | `		/* Recursion depth */` |
|      ! 0 |  1836 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1837 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1838 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1839 | `		}` |
|      ! 0 |  1840 | `		break;` |
|        - |  1841 | `									   }` |
|      ! 0 |  1842 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1843 | `		/* VM output length in bytes */` |
|      ! 0 |  1844 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1845 | `#ifdef UNTRUST` |
|        - |  1846 | `		if( pOut == 0 ){` |
|        - |  1847 | `			rc = SXERR_CORRUPT;` |
|        - |  1848 | `			break;` |
|        - |  1849 | `		}` |
|        - |  1850 | `#endif` |
|      ! 0 |  1851 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1852 | `		break;` |
|        - |  1853 | `							   }` |
|        - |  1854 |  |
|     8380 |  1855 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1856 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1857 | `		/* Create a new superglobal/global variable */` |
|    16762 |  1858 | `		const char *zName = va_arg(ap,const char *);` |
|    16762 |  1859 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1860 | `		SyHashEntry *pEntry;` |
|        - |  1861 | `		ph7_value *pObj;` |
|        - |  1862 | `		sxu32 nByte;` |
|        - |  1863 | `		sxu32 nIdx;` |
|        - |  1864 | `#ifdef UNTRUST` |
|        - |  1865 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1866 | `			rc = SXERR_CORRUPT;` |
|        - |  1867 | `			break;` |
|        - |  1868 | `		}` |
|        - |  1869 | `#endif` |
|    16762 |  1870 | `		nByte = SyStrlen(zName);` |
|    16762 |  1871 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1872 | `			/* Check if the superglobal is already installed */` |
|    16762 |  1873 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     8382 |  1874 | `		}else{` |
|        - |  1875 | `			/* Query the top active VM frame */` |
|      ! 0 |  1876 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1877 | `		}` |
|    16762 |  1878 | `		if( pEntry ){` |
|        - |  1879 | `			/* Variable already installed */` |
|      ! 0 |  1880 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1881 | `			/* Extract contents */` |
|      ! 0 |  1882 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  1883 | `			if( pObj ){` |
|        - |  1884 | `				/* Overwrite old contents */` |
|      ! 0 |  1885 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  1886 | `			}` |
|      ! 0 |  1887 | `		}else{` |
|        - |  1888 | `			/* Install a new variable */` |
|    16762 |  1889 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    16762 |  1890 | `			if( pObj == 0 ){` |
|      ! 0 |  1891 | `				rc = SXERR_MEM;` |
|      ! 0 |  1892 | `				break;` |
|        - |  1893 | `			}` |
|    16762 |  1894 | `			nIdx = pObj->nIdx;` |
|        - |  1895 | `			/* Copy value */` |
|    16762 |  1896 | `			PH7_MemObjStore(pValue,pObj);` |
|    16762 |  1897 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1898 | `				/* Install the superglobal */` |
|    16762 |  1899 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     8382 |  1900 | `			}else{` |
|        - |  1901 | `				/* Install in the current frame */` |
|      ! 0 |  1902 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1903 | `			}` |
|    16762 |  1904 | `			if( rc == SXRET_OK ){` |
|        - |  1905 | `				SyHashEntry *pRef;` |
|    16762 |  1906 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    16762 |  1907 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     8382 |  1908 | `				}else{` |
|      ! 0 |  1909 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1910 | `				}` |
|        - |  1911 | `				/* Install in the reference table */` |
|    16762 |  1912 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    16762 |  1913 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1914 | `					/* Register in the $GLOBALS array */` |
|    16762 |  1915 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     8380 |  1916 | `				}` |
|     8380 |  1917 | `			}` |
|        - |  1918 | `		}` |
|    16762 |  1919 | `		break;` |
|        - |  1920 | `									}` |
|      839 |  1921 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1922 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1923 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1924 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1925 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1926 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1927 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     1680 |  1928 | `		const char *zKey   = va_arg(ap,const char *);` |
|     1680 |  1929 | `		const char *zValue = va_arg(ap,const char *);` |
|     1680 |  1930 | `		int nLen = va_arg(ap,int);` |
|        - |  1931 | `		ph7_hashmap *pMap;` |
|        - |  1932 | `		ph7_value *pValue;` |
|     1680 |  1933 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1934 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1935 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     1679 |  1936 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  1937 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  1938 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     1678 |  1939 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  1940 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  1941 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     1678 |  1942 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  1943 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  1944 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     1678 |  1945 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  1946 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  1947 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     1678 |  1948 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  1949 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  1950 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  1951 | `		}else{` |
|        - |  1952 | `			/* Extract the $_SERVER superglobal */` |
|     1678 |  1953 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  1954 | `		}` |
|     1680 |  1955 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1956 | `			/* No such entry */` |
|      ! 0 |  1957 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1958 | `			break;` |
|        - |  1959 | `		}` |
|        - |  1960 | `		/* Point to the hashmap */` |
|     1680 |  1961 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1962 | `		/* Perform the insertion */` |
|     1680 |  1963 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     1680 |  1964 | `		break;` |
|        - |  1965 | `								   }` |
|       11 |  1966 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  1967 | `		/* Script arguments */` |
|       24 |  1968 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  1969 | `		ph7_hashmap *pMap;` |
|        - |  1970 | `		ph7_value *pValue;` |
|        - |  1971 | `		sxu32 n;` |
|       24 |  1972 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  1973 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  1974 | `			break;` |
|        - |  1975 | `		}` |
|        - |  1976 | `		/* Extract the $argv array */` |
|       24 |  1977 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  1978 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1979 | `			/* No such entry */` |
|      ! 0 |  1980 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1981 | `			break;` |
|        - |  1982 | `		}` |
|        - |  1983 | `		/* Point to the hashmap */` |
|       24 |  1984 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1985 | `		/* Perform the insertion */` |
|       24 |  1986 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  1987 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  1988 | `		if( rc == SXRET_OK ){` |
|       24 |  1989 | `			if( pMap->nEntry > 1 ){` |
|        - |  1990 | `				/* Append space separator first */` |
|       18 |  1991 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  1992 | `			}` |
|       24 |  1993 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  1994 | `		}` |
|       24 |  1995 | `		break;` |
|        - |  1996 | `								  }` |
|      ! 0 |  1997 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  1998 | `		/* error_log() consumer */` |
|      ! 0 |  1999 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2000 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2001 | `		break;` |
|        - |  2002 | `										}` |
|      ! 0 |  2003 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2004 | `		/* Script return value */` |
|      ! 0 |  2005 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2006 | `#ifdef UNTRUST` |
|        - |  2007 | `		if( ppValue == 0 ){` |
|        - |  2008 | `			rc = SXERR_CORRUPT;` |
|        - |  2009 | `			break;` |
|        - |  2010 | `		}` |
|        - |  2011 | `#endif` |
|      ! 0 |  2012 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2013 | `		break;` |
|        - |  2014 | `								   }` |
|     1676 |  2015 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2016 | `		/* Register an IO stream device */` |
|     3354 |  2017 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2018 | `		/* Make sure we are dealing with a valid IO stream */` |
|     5028 |  2019 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     3354 |  2020 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2021 | `				/* Invalid stream */` |
|      ! 0 |  2022 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2023 | `				break;` |
|        - |  2024 | `		}` |
|     3354 |  2025 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2026 | `			/* Make the 'file://' stream the defaut stream device */` |
|     1678 |  2027 | `			pVm->pDefStream = pStream;` |
|      838 |  2028 | `		}` |
|        - |  2029 | `		/* Insert in the appropriate container */` |
|     3354 |  2030 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     3354 |  2031 | `		break;` |
|        - |  2032 | `								  }` |
|      ! 0 |  2033 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2034 | `		/* Point to the VM internal output consumer buffer */` |
|      ! 0 |  2035 | `		const void **ppOut = va_arg(ap,const void **);` |
|      ! 0 |  2036 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2037 | `#ifdef UNTRUST` |
|        - |  2038 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2039 | `			rc = SXERR_CORRUPT;` |
|        - |  2040 | `			break;` |
|        - |  2041 | `		}` |
|        - |  2042 | `#endif` |
|      ! 0 |  2043 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|      ! 0 |  2044 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|      ! 0 |  2045 | `		break;` |
|        - |  2046 | `									   }` |
|      ! 0 |  2047 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2048 | `		/* Raw HTTP request*/` |
|      ! 0 |  2049 | `		const char *zRequest = va_arg(ap,const char *);` |
|      ! 0 |  2050 | `		int nByte = va_arg(ap,int);` |
|      ! 0 |  2051 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2052 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2053 | `			break;` |
|        - |  2054 | `		}` |
|      ! 0 |  2055 | `		if( nByte < 0 ){` |
|        - |  2056 | `			/* Compute length automatically */` |
|      ! 0 |  2057 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2058 | `		}` |
|        - |  2059 | `		/* Process the request */` |
|      ! 0 |  2060 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|      ! 0 |  2061 | `		break;` |
|        - |  2062 | `									}` |
|      ! 0 |  2063 | `	default:` |
|        - |  2064 | `		/* Unknown configuration option */` |
|      ! 0 |  2065 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2066 | `		break;` |
|        - |  2067 | `	}` |
|    26842 |  2068 | `	return rc;` |
|        2 |  2069 |  |
|        - |  2070 | `/* Forward declaration */` |
|        - |  2071 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2072 | `/*` |
|        - |  2073 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2074 | ` * format.` |
|        - |  2075 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2076 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2077 | ` * (STDOUT).` |
|        - |  2078 | ` */` |
|        2 |  2079 | `static sxi32 VmByteCodeDump(` |
|        - |  2080 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2081 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2082 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2083 | `	)` |
|        1 |  2084 |  |
|        - |  2085 | `	static const char zDump[] = {` |
|        - |  2086 | `		"====================================================\n"` |
|        - |  2087 | `		"PH7 VM Dump\n"` |
|        - |  2088 | `		"====================================================\n"` |
|        - |  2089 | `	};` |
|        - |  2090 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2091 | `	sxi32 rc = SXRET_OK;` |
|        - |  2092 | `	sxu32 n;` |
|        - |  2093 | `	/* Point to the PH7 instructions */` |
|        3 |  2094 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2095 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2096 | `	n = 0;` |
|        3 |  2097 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2098 | `	/* Dump instructions */` |
|        6 |  2099 | `	for(;;){` |
|       13 |  2100 | `		if( pInstr >= pEnd ){` |
|        - |  2101 | `			/* No more instructions */` |
|        3 |  2102 | `			break;` |
|        - |  2103 | `		}` |
|        - |  2104 | `		/* Format and call the consumer callback */` |
|       16 |  2105 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       10 |  2106 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       10 |  2107 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       11 |  2108 | `		if( rc != SXRET_OK ){` |
|        - |  2109 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2110 | `			return rc;` |
|        - |  2111 | `		}` |
|       11 |  2112 | `		++n;` |
|       11 |  2113 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2114 | `	}` |
|        3 |  2115 | `	return rc;` |
|        2 |  2116 |  |
|        - |  2117 | `/* Forward declaration */` |
|        - |  2118 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2119 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2120 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2121 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2122 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2123 | `/*` |
|        - |  2124 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2125 | ` * consumer callback.` |
|        - |  2126 | ` */` |
|      438 |  2127 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2128 |  |
|      439 |  2129 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      439 |  2130 | `	sxi32 rc = SXRET_OK;` |
|        - |  2131 | `	/* Append a new line */` |
|        - |  2132 | `#ifdef __WINNT__` |
|        1 |  2133 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2134 | `#else` |
|      438 |  2135 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2136 | `#endif` |
|        - |  2137 | `	/* Invoke the output consumer callback */` |
|      439 |  2138 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      439 |  2139 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  2140 | `		/* Increment output length */` |
|      439 |  2141 | `		pVm->nOutputLen += SyBlobLength(pMsg);` |
|      219 |  2142 | `	}` |
|      439 |  2143 | `	return rc;` |
|        1 |  2144 |  |
|        - |  2145 | `/*` |
|        - |  2146 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2147 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2148 | ` * information.` |
|        - |  2149 | ` */` |
|      138 |  2150 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2151 |  |
|      140 |  2152 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2153 | `		ph7_value apArg[4];` |
|        - |  2154 | `		ph7_value *apArgPtr[4];` |
|        - |  2155 | `		ph7_value sResult;` |
|        - |  2156 | `		SyString sErr;` |
|        - |  2157 | `		/* Prepare arguments */` |
|       61 |  2158 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2159 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2160 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2161 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2162 | `		if( pFile ){` |
|       61 |  2163 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2164 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2165 | `		}else{` |
|      ! 0 |  2166 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2167 | `		}` |
|       61 |  2168 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2169 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2170 | `		/* Set up pointer array */` |
|       61 |  2171 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2172 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2173 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2174 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2175 | `		/* Call the handler */` |
|       61 |  2176 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2177 | `		/* Check return value */` |
|       61 |  2178 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2179 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2180 | `		}` |
|        - |  2181 | `		/* Release */` |
|       61 |  2182 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2183 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2184 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2185 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2186 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2187 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2188 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2189 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2190 | `	}` |
|        - |  2191 | `	/* No handler, always call error handler */` |
|       79 |  2192 | `	return TRUE;` |
|       71 |  2193 |  |
|      102 |  2194 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2195 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2196 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2197 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2198 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2199 | `	)` |
|        2 |  2200 |  |
|      104 |  2201 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2202 | `	SyString *pFile;` |
|        - |  2203 | `	char *zErr;` |
|      104 |  2204 | `	sxi32 rc = SXRET_OK;` |
|      104 |  2205 | `	if( !pVm->bErrReport ){` |
|        - |  2206 | `		/* Don't bother reporting errors */` |
|        3 |  2207 | `		return SXRET_OK;` |
|        - |  2208 | `	}` |
|        - |  2209 | `	/* Reset the working buffer */` |
|      102 |  2210 | `	SyBlobReset(pWorker);` |
|        - |  2211 | `	/* Peek the processed file if available */` |
|      102 |  2212 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      102 |  2213 | `	if( pFile ){` |
|        - |  2214 | `		/* Append file name */` |
|      102 |  2215 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|      102 |  2216 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       50 |  2217 | `	}` |
|        - |  2218 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2219 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2220 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2221 | `	 * E_DEPRECATED). */` |
|      102 |  2222 | `	zErr = "Error:  ";` |
|      102 |  2223 | `	switch(iErr){` |
|       21 |  2224 | `	case PH7_CTX_WARNING:` |
|       44 |  2225 | `		zErr = "Warning:  ";` |
|       44 |  2226 | `		break;` |
|        6 |  2227 | `	case PH7_CTX_NOTICE:` |
|       14 |  2228 | `		zErr = "Notice:  ";` |
|       12 |  2229 | `		break;` |
|       23 |  2230 | `	default:` |
|        - |  2231 | `		/* keep iErr unchanged */` |
|       46 |  2232 | `		break;` |
|        - |  2233 | `	}` |
|      102 |  2234 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|      102 |  2235 | `	if( pFuncName ){` |
|        - |  2236 | `		/* Append function name first */` |
|       29 |  2237 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       29 |  2238 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       14 |  2239 | `	}` |
|      102 |  2240 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2241 | `	/* Check for user error handler.  compute length of C string */` |
|      102 |  2242 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       53 |  2243 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       26 |  2244 | `	}` |
|      102 |  2245 | `	return rc;` |
|       53 |  2246 |  |
|        - |  2247 | `/*` |
|        - |  2248 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2249 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2250 | ` * information.` |
|        - |  2251 | ` */` |
|       38 |  2252 | `static sxi32 VmThrowErrorAp(` |
|        - |  2253 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2254 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2255 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2256 | `	const char *zFormat, /* Format message */` |
|        - |  2257 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2258 | `	)` |
|        2 |  2259 |  |
|       40 |  2260 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2261 | `	SyBlob sMsg;` |
|        - |  2262 | `	SyString *pFile;` |
|        - |  2263 | `	char *zErr;` |
|       40 |  2264 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2265 | `	if( !pVm->bErrReport ){` |
|        - |  2266 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2267 | `		return SXRET_OK;` |
|        - |  2268 | `	}` |
|        - |  2269 | `	/* Reset the working buffer */` |
|       40 |  2270 | `	SyBlobReset(pWorker);` |
|        - |  2271 | `	/* Peek the processed file if available */` |
|       40 |  2272 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2273 | `	if( pFile ){` |
|        - |  2274 | `		/* Append file name */` |
|       40 |  2275 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2276 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2277 | `	}` |
|        - |  2278 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2279 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2280 | `	 * the correct errno value. */` |
|       40 |  2281 | `	zErr = "Error:  ";` |
|       40 |  2282 | `	switch(iErr){` |
|        4 |  2283 | `	case PH7_CTX_WARNING:` |
|        9 |  2284 | `		zErr = "Warning:  ";` |
|        9 |  2285 | `		break;` |
|        3 |  2286 | `	case PH7_CTX_NOTICE:` |
|        7 |  2287 | `		zErr = "Notice:  ";` |
|        6 |  2288 | `		break;` |
|       12 |  2289 | `	default:` |
|        - |  2290 | `		/* do not change iErr */` |
|       24 |  2291 | `		break;` |
|        - |  2292 | `	}` |
|       40 |  2293 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2294 | `	if( pFuncName ){` |
|        - |  2295 | `		/* Append function name first */` |
|       26 |  2296 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2297 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2298 | `	}` |
|        - |  2299 | `	/* Format the raw message */` |
|       40 |  2300 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2301 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2302 | `	/* Check if a user error handler is installed */` |
|       40 |  2303 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2304 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2305 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2306 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2307 | `	}` |
|       40 |  2308 | `	SyBlobRelease(&sMsg);` |
|       40 |  2309 | `	return rc;` |
|       21 |  2310 |  |
|        - |  2311 | `/*` |
|        - |  2312 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2313 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2314 | ` * information.` |
|        - |  2315 | ` * ------------------------------------` |
|        - |  2316 | ` * Simple boring wrapper function.` |
|        - |  2317 | ` * ------------------------------------` |
|        - |  2318 | ` */` |
|       14 |  2319 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2320 |  |
|        - |  2321 | `	va_list ap;` |
|        - |  2322 | `	sxi32 rc;` |
|       15 |  2323 | `	va_start(ap,zFormat);` |
|       15 |  2324 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2325 | `	va_end(ap);` |
|       15 |  2326 | `	return rc;` |
|        1 |  2327 |  |
|        - |  2328 | `/*` |
|        - |  2329 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2330 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2331 | ` * information.` |
|        - |  2332 | ` * ------------------------------------` |
|        - |  2333 | ` * Simple boring wrapper function.` |
|        - |  2334 | ` * ------------------------------------` |
|        - |  2335 | ` */` |
|       24 |  2336 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2337 |  |
|        - |  2338 | `	sxi32 rc;` |
|       26 |  2339 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2340 | `	return rc;` |
|        2 |  2341 |  |
|        - |  2342 | `/*` |
|        - |  2343 | ` * Resolve function context from the current frame.` |
|        - |  2344 | ` */` |
|      714 |  2345 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2346 |  |
|        - |  2347 | `	VmFrame *pFrame;` |
|        - |  2348 | `	ph7_vm_func *pFunc;` |
|      715 |  2349 | `	*pzFuncName = 0;` |
|      715 |  2350 | `	*pnFuncLen = 0;` |
|      715 |  2351 | `	pFrame = pVm->pFrame;` |
|      715 |  2352 | `	if( pFrame == 0 ){` |
|      ! 0 |  2353 | `		return;` |
|        - |  2354 | `	}` |
|      715 |  2355 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  2356 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  2357 | `	}` |
|      715 |  2358 | `	if( pFrame->pParent == 0 ){` |
|      709 |  2359 | `		return;` |
|        - |  2360 | `	}` |
|        7 |  2361 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        7 |  2362 | `	if( pFunc == 0 ){` |
|      ! 0 |  2363 | `		return;` |
|        - |  2364 | `	}` |
|        7 |  2365 | `	*pzFuncName = pFunc->sName.zString;` |
|        7 |  2366 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      358 |  2367 |  |
|        - |  2368 | `/*` |
|        - |  2369 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2370 | ` */` |
|      360 |  2371 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2372 |  |
|        - |  2373 | `	SyBlob sOut;` |
|        - |  2374 | `	SyString *pFile;` |
|      361 |  2375 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2376 | `		return PH7_OK;` |
|        - |  2377 | `	}` |
|      361 |  2378 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2379 | `		zClass = "Exception";` |
|      ! 0 |  2380 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2381 | `	}` |
|      361 |  2382 | `	if( zMsg == 0 ){` |
|      ! 0 |  2383 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2384 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2385 | `	}` |
|      361 |  2386 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      355 |  2387 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      177 |  2388 | `	}` |
|      361 |  2389 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      361 |  2390 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      361 |  2391 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      361 |  2392 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      361 |  2393 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      361 |  2394 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      361 |  2395 | `	if( pFile ){` |
|      361 |  2396 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      361 |  2397 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      361 |  2398 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      180 |  2399 | `	}` |
|      361 |  2400 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      361 |  2401 | `	if( pFile ){` |
|      361 |  2402 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      361 |  2403 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      361 |  2404 | `		if( zFuncName && nFuncLen > 0 ){` |
|        7 |  2405 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        4 |  2406 | `		}else{` |
|      355 |  2407 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2408 | `		}` |
|      180 |  2409 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2410 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2411 | `	}else{` |
|      ! 0 |  2412 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2413 | `	}` |
|      361 |  2414 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      361 |  2415 | `	if( pFile ){` |
|      361 |  2416 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      361 |  2417 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      361 |  2418 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      361 |  2419 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      180 |  2420 | `	}` |
|      361 |  2421 | `	VmCallErrorHandler(pVm,&sOut);` |
|      361 |  2422 | `	SyBlobRelease(&sOut);` |
|      361 |  2423 | `	return PH7_ABORT;` |
|      181 |  2424 |  |
|        - |  2425 | `/*` |
|        - |  2426 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2427 | ` */` |
|      354 |  2428 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2429 |  |
|        - |  2430 | `	ph7_vm *pVm;` |
|        - |  2431 | `	ph7_class *pClass;` |
|        - |  2432 | `	ph7_class_instance *pThis;` |
|        - |  2433 | `	ph7_class_method *pCons;` |
|        - |  2434 | `	ph7_value sArg;` |
|        - |  2435 | `	ph7_value *apArg[1];` |
|        - |  2436 | `	SyBlob sMsg;` |
|        - |  2437 | `	SyString sMsgStr;` |
|        - |  2438 | `	VmFrame *pFrame;` |
|        - |  2439 | `	va_list ap;` |
|        - |  2440 | `	sxi32 rc;` |
|        - |  2441 |  |
|      356 |  2442 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2443 | `		return PH7_ABORT;` |
|        - |  2444 | `	}` |
|      356 |  2445 | `	pVm = pCtx->pVm;` |
|      356 |  2446 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2447 | `		zClass = "Error";` |
|      ! 0 |  2448 | `	}` |
|      356 |  2449 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      356 |  2450 | `	if( pClass == 0 ){` |
|      ! 0 |  2451 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2452 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2453 | `			zClass` |
|        - |  2454 | `			);` |
|        - |  2455 | `	}` |
|      356 |  2456 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      356 |  2457 | `	if( pThis == 0 ){` |
|      ! 0 |  2458 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2459 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2460 | `			);` |
|        - |  2461 | `	}` |
|        - |  2462 |  |
|      356 |  2463 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      356 |  2464 | `	va_start(ap,zFormat);` |
|      356 |  2465 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      356 |  2466 | `	va_end(ap);` |
|        - |  2467 |  |
|      356 |  2468 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      356 |  2469 | `	if( pCons ){` |
|      356 |  2470 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      356 |  2471 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      356 |  2472 | `		apArg[0] = &sArg;` |
|      356 |  2473 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      356 |  2474 | `		PH7_MemObjRelease(&sArg);` |
|      177 |  2475 | `	}` |
|      356 |  2476 | `	SyBlobRelease(&sMsg);` |
|        - |  2477 |  |
|      356 |  2478 | `	pFrame = pVm->pFrame;` |
|      356 |  2479 | `	if( pFrame ){` |
|      358 |  2480 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        3 |  2481 | `			pFrame = pFrame->pParent;` |
|        1 |  2482 | `		}` |
|      356 |  2483 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      177 |  2484 | `	}` |
|      356 |  2485 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      356 |  2486 | `	PH7_ClassInstanceUnref(pThis);` |
|      356 |  2487 | `	if( rc == SXERR_ABORT ){` |
|      353 |  2488 | `		return PH7_ABORT;` |
|        - |  2489 | `	}` |
|        3 |  2490 | `	return PH7_EXCEPTION;` |
|      179 |  2491 |  |
|        - |  2492 | `/*` |
|        - |  2493 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2494 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2495 | ` */` |
|      ! 0 |  2496 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2497 |  |
|        - |  2498 | `	ph7_vm *pVm;` |
|        - |  2499 | `	SyBlob sMsg;` |
|      ! 0 |  2500 | `	const char *zFuncName = 0;` |
|      ! 0 |  2501 | `	int nFuncLen = 0;` |
|        - |  2502 | `	va_list ap;` |
|        - |  2503 | `	sxi32 rc;` |
|        - |  2504 |  |
|      ! 0 |  2505 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2506 | `		return PH7_OK;` |
|        - |  2507 | `	}` |
|      ! 0 |  2508 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2509 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2510 | `		zClass = "Error";` |
|      ! 0 |  2511 | `	}` |
|        - |  2512 |  |
|      ! 0 |  2513 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2514 |  |
|      ! 0 |  2515 | `	va_start(ap,zFormat);` |
|      ! 0 |  2516 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2517 | `	va_end(ap);` |
|        - |  2518 |  |
|      ! 0 |  2519 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2520 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2521 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2522 | `	}` |
|      ! 0 |  2523 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2524 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2525 | `	}` |
|      ! 0 |  2526 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2527 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2528 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2529 | `	return rc;` |
|      ! 0 |  2530 |  |
|        - |  2531 | `/*` |
|        - |  2532 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2533 | ` *` |
|        - |  2534 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2535 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2536 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2537 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2538 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2539 | ` * then the program execution is halted.` |
|        - |  2540 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2541 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2542 | ` * or to reset the VM to it's initial state.` |
|        - |  2543 | ` */` |
|    27048 |  2544 | `static sxi32 VmByteCodeExec(` |
|        - |  2545 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2546 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2547 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2548 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2549 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2550 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2551 | `	int is_callback      /* TRUE if we are executing a callback */` |
|        - |  2552 | `	)` |
|        2 |  2553 |  |
|        - |  2554 | `	VmInstr *pInstr;` |
|        - |  2555 | `	ph7_value *pTos;` |
|        - |  2556 | `	SySet aArg;` |
|        - |  2557 | `	sxi32 pc;` |
|        - |  2558 | `	sxi32 rc;` |
|        - |  2559 | `	/* Argument container */` |
|    27050 |  2560 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    27050 |  2561 | `	if( nTos < 0 ){` |
|    25696 |  2562 | `		pTos = &pStack[-1];` |
|    12849 |  2563 | `	}else{` |
|     1356 |  2564 | `		pTos = &pStack[nTos];` |
|        - |  2565 | `	}` |
|    27050 |  2566 | `	pc = 0;` |
|        - |  2567 | `	/* Execute as much as we can */` |
|  4734168 |  2568 | `	for(;;){` |
|        - |  2569 | `		/* Fetch the instruction to execute */` |
|  9467634 |  2570 | `		pInstr = &aInstr[pc];` |
|  9467634 |  2571 | `		rc = SXRET_OK;` |
|        - |  2572 | `/*` |
|        - |  2573 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2574 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2575 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2576 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2577 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2578 | ` */` |
|  9467634 |  2579 | `		switch(pInstr->iOp){` |
|        - |  2580 | `/*` |
|        - |  2581 | ` * DONE: P1 * *` |
|        - |  2582 | ` *` |
|        - |  2583 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2584 | ` * and return immediately.` |
|        - |  2585 | ` */` |
|    13334 |  2586 | `case PH7_OP_DONE:` |
|    26670 |  2587 | `	if( pInstr->iP1 ){` |
|        - |  2588 | `#ifdef UNTRUST` |
|        - |  2589 | `		if( pTos < pStack ){` |
|        - |  2590 | `			goto Abort;` |
|        - |  2591 | `		}` |
|        - |  2592 | `#endif` |
|    15184 |  2593 | `		if( pLastRef ){` |
|     9942 |  2594 | `			*pLastRef = pTos->nIdx;` |
|     4970 |  2595 | `		}` |
|    15184 |  2596 | `		if( pResult ){` |
|        - |  2597 | `			/* Execution result */` |
|    14506 |  2598 | `			PH7_MemObjStore(pTos,pResult);` |
|     7252 |  2599 | `		}` |
|    15184 |  2600 | `		VmPopOperand(&pTos,1);` |
|    19079 |  2601 | `	}else if( pLastRef ){` |
|        - |  2602 | `		/* Nothing referenced */` |
|      764 |  2603 | `		*pLastRef = SXU32_HIGH;` |
|      381 |  2604 | `	}` |
|    26670 |  2605 | `	goto Done;` |
|        - |  2606 | `/*` |
|        - |  2607 | ` * HALT: P1 * *` |
|        - |  2608 | ` *` |
|        - |  2609 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2610 | ` * and abort immediately.` |
|        - |  2611 | ` */` |
|        4 |  2612 | `case PH7_OP_HALT:` |
|        9 |  2613 | `	if( pInstr->iP1 ){` |
|        - |  2614 | `#ifdef UNTRUST` |
|        - |  2615 | `		if( pTos < pStack ){` |
|        - |  2616 | `			goto Abort;` |
|        - |  2617 | `		}` |
|        - |  2618 | `#endif` |
|        9 |  2619 | `		if( pLastRef ){` |
|      ! 0 |  2620 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2621 | `		}` |
|        9 |  2622 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2623 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2624 | `				/* Output the exit message */` |
|        7 |  2625 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2626 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2627 | `				if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  2628 | `					/* Increment output length */` |
|        5 |  2629 | `					pVm->nOutputLen += SyBlobLength(&pTos->sBlob);` |
|        2 |  2630 | `				}` |
|        3 |  2631 | `			}` |
|        7 |  2632 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2633 | `			/* Record exit status */` |
|        5 |  2634 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2635 | `		}` |
|        9 |  2636 | `		VmPopOperand(&pTos,1);` |
|        4 |  2637 | `	}else if( pLastRef ){` |
|        - |  2638 | `		/* Nothing referenced */` |
|      ! 0 |  2639 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2640 | `	}` |
|        - |  2641 | `	/* Check if we're in an included file context */` |
|        9 |  2642 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2643 | `		/* Terminate the entire process */` |
|        9 |  2644 | `		exit(pVm->iExitStatus);` |
|        - |  2645 | `	}` |
|      ! 0 |  2646 | `	goto Abort;` |
|        - |  2647 | `/*` |
|        - |  2648 | ` * JMP: * P2 *` |
|        - |  2649 | ` *` |
|        - |  2650 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2651 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2652 | ` */` |
|   207927 |  2653 | `case PH7_OP_JMP:` |
|   415900 |  2654 | `	pc = pInstr->iP2 - 1;` |
|   415900 |  2655 | `	break;` |
|        - |  2656 | `/*` |
|        - |  2657 | ` * JZ: P1 P2 *` |
|        - |  2658 | ` *` |
|        - |  2659 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2660 | ` * entry in the stack if P1 is zero.` |
|        - |  2661 | ` */` |
|   480494 |  2662 | `case PH7_OP_JZ:` |
|        - |  2663 | `#ifdef UNTRUST` |
|        - |  2664 | `	if( pTos < pStack ){` |
|        - |  2665 | `		goto Abort;` |
|        - |  2666 | `	}` |
|        - |  2667 | `#endif` |
|        - |  2668 | `	/* Get a boolean value */` |
|   961078 |  2669 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       77 |  2670 | `		PH7_MemObjToBool(pTos);` |
|       38 |  2671 | `	}` |
|   961078 |  2672 | `	if( !pTos->x.iVal ){` |
|        - |  2673 | `		/* Take the jump */` |
|   461584 |  2674 | `		pc = pInstr->iP2 - 1;` |
|   230791 |  2675 | `	}` |
|   961078 |  2676 | `	if( !pInstr->iP1 ){` |
|   756846 |  2677 | `		VmPopOperand(&pTos,1);` |
|   378444 |  2678 | `	}` |
|   961078 |  2679 | `	break;` |
|        - |  2680 | `/*` |
|        - |  2681 | ` * JNZ: P1 P2 *` |
|        - |  2682 | ` *` |
|        - |  2683 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2684 | ` * entry in the stack if P1 is zero.` |
|        - |  2685 | ` */` |
|    51507 |  2686 | `case PH7_OP_JNZ:` |
|        - |  2687 | `#ifdef UNTRUST` |
|        - |  2688 | `	if( pTos < pStack ){` |
|        - |  2689 | `		goto Abort;` |
|        - |  2690 | `	}` |
|        - |  2691 | `#endif` |
|        - |  2692 | `	/* Get a boolean value */` |
|   103016 |  2693 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2694 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2695 | `	}` |
|   103016 |  2696 | `	if( pTos->x.iVal ){` |
|        - |  2697 | `		/* Take the jump */` |
|     3964 |  2698 | `		pc = pInstr->iP2 - 1;` |
|     1981 |  2699 | `	}` |
|   103016 |  2700 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2701 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2702 | `	}` |
|   103016 |  2703 | `	break;` |
|        - |  2704 | `/*` |
|        - |  2705 | ` * NOOP: * * *` |
|        - |  2706 | ` *` |
|        - |  2707 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2708 | ` * destination.` |
|        - |  2709 | ` */` |
|      ! 0 |  2710 | `case PH7_OP_NOOP:` |
|      ! 0 |  2711 | `	break;` |
|        - |  2712 | `/*` |
|        - |  2713 | ` * POP: P1 * *` |
|        - |  2714 | ` *` |
|        - |  2715 | ` * Pop P1 elements from the operand stack.` |
|        - |  2716 | ` */` |
|   370284 |  2717 | `case PH7_OP_POP: {` |
|   740614 |  2718 | `	sxi32 n = pInstr->iP1;` |
|   740614 |  2719 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2720 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2721 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2722 | `	}` |
|   740614 |  2723 | `	VmPopOperand(&pTos,n);` |
|   740614 |  2724 | `	break;` |
|        - |  2725 | `				 }` |
|        - |  2726 | `/*` |
|        - |  2727 | ` * CVT_INT: * * *` |
|        - |  2728 | ` *` |
|        - |  2729 | ` * Force the top of the stack to be an integer.` |
|        - |  2730 | ` */` |
|       35 |  2731 | `case PH7_OP_CVT_INT:` |
|        - |  2732 | `#ifdef UNTRUST` |
|        - |  2733 | `	if( pTos < pStack ){` |
|        - |  2734 | `		goto Abort;` |
|        - |  2735 | `	}` |
|        - |  2736 | `#endif` |
|       72 |  2737 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2738 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2739 | `	}` |
|        - |  2740 | `	/* Invalidate any prior representation */` |
|       72 |  2741 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2742 | `	break;` |
|        - |  2743 | `/*` |
|        - |  2744 | ` * CVT_REAL: * * *` |
|        - |  2745 | ` *` |
|        - |  2746 | ` * Force the top of the stack to be a real.` |
|        - |  2747 | ` */` |
|        4 |  2748 | `case PH7_OP_CVT_REAL:` |
|        - |  2749 | `#ifdef UNTRUST` |
|        - |  2750 | `	if( pTos < pStack ){` |
|        - |  2751 | `		goto Abort;` |
|        - |  2752 | `	}` |
|        - |  2753 | `#endif` |
|        9 |  2754 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2755 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2756 | `	}` |
|        - |  2757 | `	/* Invalidate any prior representation */` |
|        9 |  2758 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2759 | `	break;` |
|        - |  2760 | `/*` |
|        - |  2761 | ` * CVT_STR: * * *` |
|        - |  2762 | ` *` |
|        - |  2763 | ` * Force the top of the stack to be a string.` |
|        - |  2764 | ` */` |
|      146 |  2765 | `case PH7_OP_CVT_STR:` |
|        - |  2766 | `#ifdef UNTRUST` |
|        - |  2767 | `	if( pTos < pStack ){` |
|        - |  2768 | `		goto Abort;` |
|        - |  2769 | `	}` |
|        - |  2770 | `#endif` |
|      294 |  2771 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  2772 | `		PH7_MemObjToString(pTos);` |
|      146 |  2773 | `	}` |
|      294 |  2774 | `	break;` |
|        - |  2775 | `/*` |
|        - |  2776 | ` * CVT_BOOL: * * *` |
|        - |  2777 | ` *` |
|        - |  2778 | ` * Force the top of the stack to be a boolean.` |
|        - |  2779 | ` */` |
|        5 |  2780 | `case PH7_OP_CVT_BOOL:` |
|        - |  2781 | `#ifdef UNTRUST` |
|        - |  2782 | `	if( pTos < pStack ){` |
|        - |  2783 | `		goto Abort;` |
|        - |  2784 | `	}` |
|        - |  2785 | `#endif` |
|       11 |  2786 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2787 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2788 | `	}` |
|       11 |  2789 | `	break;` |
|        - |  2790 | `/*` |
|        - |  2791 | ` * CVT_NULL: * * *` |
|        - |  2792 | ` *` |
|        - |  2793 | ` * Nullify the top of the stack.` |
|        - |  2794 | ` */` |
|        3 |  2795 | `case PH7_OP_CVT_NULL:` |
|        - |  2796 | `#ifdef UNTRUST` |
|        - |  2797 | `	if( pTos < pStack ){` |
|        - |  2798 | `		goto Abort;` |
|        - |  2799 | `	}` |
|        - |  2800 | `#endif` |
|        7 |  2801 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2802 | `	break;` |
|        - |  2803 | `/*` |
|        - |  2804 | ` * CVT_NUMC: * * *` |
|        - |  2805 | ` *` |
|        - |  2806 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2807 | ` */` |
|      ! 0 |  2808 | `case PH7_OP_CVT_NUMC:` |
|        - |  2809 | `#ifdef UNTRUST` |
|        - |  2810 | `	if( pTos < pStack ){` |
|        - |  2811 | `		goto Abort;` |
|        - |  2812 | `	}` |
|        - |  2813 | `#endif` |
|        - |  2814 | `	/* Force a numeric cast */` |
|      ! 0 |  2815 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2816 | `	break;` |
|        - |  2817 | `/*` |
|        - |  2818 | ` * CVT_ARRAY: * * *` |
|        - |  2819 | ` *` |
|        - |  2820 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2821 | ` */` |
|       10 |  2822 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2823 | `#ifdef UNTRUST` |
|        - |  2824 | `	if( pTos < pStack ){` |
|        - |  2825 | `		goto Abort;` |
|        - |  2826 | `	}` |
|        - |  2827 | `#endif` |
|        - |  2828 | `	/* Force a hashmap cast */` |
|       21 |  2829 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  2830 | `	if( rc != SXRET_OK ){` |
|        - |  2831 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  2832 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  2833 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  2834 | `	}` |
|       21 |  2835 | `	break;` |
|        - |  2836 | `/*` |
|        - |  2837 | ` * CVT_OBJ: * * *` |
|        - |  2838 | ` *` |
|        - |  2839 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  2840 | ` */` |
|        8 |  2841 | `case PH7_OP_CVT_OBJ:` |
|        - |  2842 | `#ifdef UNTRUST` |
|        - |  2843 | `	if( pTos < pStack ){` |
|        - |  2844 | `		goto Abort;` |
|        - |  2845 | `	}` |
|        - |  2846 | `#endif` |
|       17 |  2847 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  2848 | `		/* Force a 'stdClass()' cast */` |
|       17 |  2849 | `		PH7_MemObjToObject(pTos);` |
|        8 |  2850 | `	}` |
|       17 |  2851 | `	break;` |
|        - |  2852 | `/*` |
|        - |  2853 | ` * ERR_CTRL * * *` |
|        - |  2854 | ` *` |
|        - |  2855 | ` * Error control operator.` |
|        - |  2856 | ` */` |
|    11469 |  2857 | `case PH7_OP_ERR_CTRL:` |
|        - |  2858 | `	/*` |
|        - |  2859 | `	 * TICKET 1433-038:` |
|        - |  2860 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2861 | `	 * use the public API,to control error output.` |
|        - |  2862 | `	 */` |
|    22938 |  2863 | `	break;` |
|        - |  2864 | `/*` |
|        - |  2865 | ` * IS_A * * *` |
|        - |  2866 | ` *` |
|        - |  2867 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  2868 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  2869 | ` * holding a class name or an object).` |
|        - |  2870 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  2871 | ` */` |
|       11 |  2872 | `case PH7_OP_IS_A:{` |
|       23 |  2873 | `	ph7_value *pNos = &pTos[-1];` |
|       23 |  2874 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  2875 | `#ifdef UNTRUST` |
|        - |  2876 | `	if( pNos < pStack ){` |
|        - |  2877 | `		goto Abort;` |
|        - |  2878 | `	}` |
|        - |  2879 | `#endif` |
|       23 |  2880 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       21 |  2881 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       21 |  2882 | `		ph7_class *pClass = 0;` |
|        - |  2883 | `		/* Extract the target class */` |
|       21 |  2884 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  2885 | `			/* Instance already loaded */` |
|      ! 0 |  2886 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       21 |  2887 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2888 | `			/* Perform the query */` |
|       31 |  2889 | `			pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|       20 |  2890 | `				SyBlobLength(&pTos->sBlob),FALSE,0);` |
|       10 |  2891 | `		}` |
|       21 |  2892 | `		if( pClass ){` |
|        - |  2893 | `			/* Perform the query */` |
|       21 |  2894 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       10 |  2895 | `		}` |
|       10 |  2896 | `	}` |
|        - |  2897 | `	/* Push result */` |
|       23 |  2898 | `	VmPopOperand(&pTos,1);` |
|       23 |  2899 | `	PH7_MemObjRelease(pTos);` |
|       23 |  2900 | `	pTos->x.iVal = iRes;` |
|       23 |  2901 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       23 |  2902 | `	break;` |
|        - |  2903 | `				 }` |
|        - |  2904 |  |
|        - |  2905 | `/*` |
|        - |  2906 | ` * LOADC P1 P2 *` |
|        - |  2907 | ` *` |
|        - |  2908 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  2909 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  2910 | ` */` |
|   758564 |  2911 | `case PH7_OP_LOADC: {` |
|        - |  2912 | `	ph7_value *pObj;` |
|        - |  2913 | `	/* Reserve a room */` |
|  1517174 |  2914 | `	pTos++;` |
|  1517174 |  2915 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1517174 |  2916 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2917 | `			SyHashEntry *pEntry;` |
|        - |  2918 | `			/* Candidate for expansion via user defined callbacks */` |
|    17344 |  2919 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    17344 |  2920 | `			if( pEntry ){` |
|    14250 |  2921 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2922 | `				/* Set a NULL default value */` |
|    14250 |  2923 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    14250 |  2924 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2925 | `				/* Invoke the callback and deal with the expanded value */` |
|    14250 |  2926 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  2927 | `				/* Mark as constant */` |
|    14250 |  2928 | `				pTos->nIdx = SXU32_HIGH;` |
|    14250 |  2929 | `				break;` |
|        - |  2930 | `			}` |
|     1547 |  2931 | `		}` |
|  1502926 |  2932 | `		PH7_MemObjLoad(pObj,pTos);` |
|   751486 |  2933 | `	}else{` |
|        - |  2934 | `		/* Set a NULL value */` |
|      ! 0 |  2935 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2936 | `	}` |
|        - |  2937 | `	/* Mark as constant */` |
|  1502926 |  2938 | `	pTos->nIdx = SXU32_HIGH;` |
|  1502926 |  2939 | `	break;` |
|        - |  2940 | `				  }` |
|        - |  2941 | `/*` |
|        - |  2942 | ` * LOAD: P1 * P3` |
|        - |  2943 | ` *` |
|        - |  2944 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  2945 | ` * from the P3 operand.` |
|        - |  2946 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  2947 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  2948 | ` */` |
|  1309507 |  2949 | `case PH7_OP_LOAD:{` |
|        - |  2950 | `	ph7_value *pObj;` |
|        - |  2951 | `	SyString sName;` |
|  2619236 |  2952 | `	if( pInstr->p3 == 0 ){` |
|        - |  2953 | `		/* Take the variable name from the top of the stack */` |
|        - |  2954 | `#ifdef UNTRUST` |
|        - |  2955 | `		if( pTos < pStack ){` |
|        - |  2956 | `			goto Abort;` |
|        - |  2957 | `		}` |
|        - |  2958 | `#endif` |
|        - |  2959 | `		/* Force a string cast */` |
|       19 |  2960 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2961 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  2962 | `		}` |
|       19 |  2963 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  2964 | `	}else{` |
|  2619218 |  2965 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  2966 | `		/* Reserve a room for the target object */` |
|  2619218 |  2967 | `		pTos++;` |
|        - |  2968 | `	}` |
|        - |  2969 | `	/* Extract the requested memory object */` |
|  2619236 |  2970 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2619236 |  2971 | `	if( pObj == 0 ){` |
|      568 |  2972 | `		if( pInstr->iP1 ){` |
|        - |  2973 | `			/* Variable not found,load NULL */` |
|      568 |  2974 | `			if( !pInstr->p3 ){` |
|      ! 0 |  2975 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  2976 | `			}else{` |
|      568 |  2977 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2978 | `			}` |
|      568 |  2979 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1309792 |  2980 | `			break;` |
|      ! 0 |  2981 | `		}else{` |
|        - |  2982 | `			/* Fatal error */` |
|      ! 0 |  2983 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  2984 | `			goto Abort;` |
|        - |  2985 | `		}` |
|        - |  2986 | `	}` |
|        - |  2987 | `	/* Load variable contents */` |
|  2618670 |  2988 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2618670 |  2989 | `	pTos->nIdx = pObj->nIdx;` |
|  2618670 |  2990 | `	break;` |
|        - |  2991 | `				   }` |
|        - |  2992 | `/*` |
|        - |  2993 | ` * LOAD_MAP P1 * *` |
|        - |  2994 | ` *` |
|        - |  2995 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  2996 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  2997 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  2998 | ` */` |
|    16523 |  2999 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3000 | `	ph7_hashmap *pMap;` |
|        - |  3001 | `	/* Allocate a new hashmap instance */` |
|    33048 |  3002 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    33048 |  3003 | `	if( pMap == 0 ){` |
|      ! 0 |  3004 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3005 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3006 | `		goto Abort;` |
|        - |  3007 | `	}` |
|    33048 |  3008 | `	if( pInstr->iP1 > 0 ){` |
|     1970 |  3009 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3010 | `		/* Perform the insertion */` |
|     5980 |  3011 | `		while( pEntry < pTos ){` |
|     4012 |  3012 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3013 | `				/* Insertion by reference */` |
|      142 |  3014 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3015 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3016 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3017 | `					);` |
|       48 |  3018 | `			}else{` |
|        - |  3019 | `				/* Standard insertion */` |
|     5876 |  3020 | `				PH7_HashmapInsert(pMap,` |
|     3916 |  3021 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     1958 |  3022 | `					&pEntry[1]` |
|        - |  3023 | `				);` |
|        - |  3024 | `			}` |
|        - |  3025 | `			/* Next pair on the stack */` |
|     4012 |  3026 | `			pEntry += 2;` |
|        2 |  3027 | `		}` |
|        - |  3028 | `		/* Pop P1 elements */` |
|     1970 |  3029 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|      984 |  3030 | `	}` |
|        - |  3031 | `	/* Push the hashmap */` |
|    33048 |  3032 | `	pTos++;` |
|    33048 |  3033 | `	pTos->nIdx = SXU32_HIGH;` |
|    33048 |  3034 | `	pTos->x.pOther = pMap;` |
|    33048 |  3035 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    33048 |  3036 | `	break;` |
|        - |  3037 | `					  }` |
|        - |  3038 | `/*` |
|        - |  3039 | ` * LOAD_LIST: P1 * *` |
|        - |  3040 | ` *` |
|        - |  3041 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3042 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3043 | ` * Caveats:` |
|        - |  3044 | ` *  This implementation support only a single nesting level.` |
|        - |  3045 | ` */` |
|       17 |  3046 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3047 | `	ph7_value *pEntry;` |
|       35 |  3048 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3049 | `		/* Empty list,break immediately */` |
|      ! 0 |  3050 | `		break;` |
|        - |  3051 | `	}` |
|       35 |  3052 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3053 | `#ifdef UNTRUST` |
|        - |  3054 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3055 | `		goto Abort;` |
|        - |  3056 | `	}` |
|        - |  3057 | `#endif` |
|       35 |  3058 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       31 |  3059 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3060 | `		ph7_hashmap_node *pNode;` |
|        - |  3061 | `		ph7_value sKey,*pObj;` |
|        - |  3062 | `		/* Start Copying */` |
|       31 |  3063 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|       99 |  3064 | `		while( pEntry <= pTos ){` |
|       69 |  3065 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       65 |  3066 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       65 |  3067 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       65 |  3068 | `					if( rc == SXRET_OK ){` |
|        - |  3069 | `						/* Store node value */` |
|       65 |  3070 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       33 |  3071 | `					}else{` |
|        - |  3072 | `						/* Nullify the variable */` |
|      ! 0 |  3073 | `						PH7_MemObjRelease(pObj);` |
|        - |  3074 | `					}` |
|       32 |  3075 | `				}` |
|       32 |  3076 | `			}` |
|       69 |  3077 | `			sKey.x.iVal++; /* Next numeric index */` |
|       69 |  3078 | `			pEntry++;` |
|        1 |  3079 | `		}` |
|       15 |  3080 | `	}` |
|       35 |  3081 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       35 |  3082 | `	break;` |
|        - |  3083 | `					   }` |
|        - |  3084 | `/*` |
|        - |  3085 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3086 | ` *` |
|        - |  3087 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3088 | ` * from the stack.` |
|        - |  3089 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3090 | ` * instead.` |
|        - |  3091 | ` */` |
|   215480 |  3092 | `case PH7_OP_LOAD_IDX: {` |
|   431006 |  3093 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   431006 |  3094 | `	ph7_hashmap *pMap = 0;` |
|        - |  3095 | `	ph7_value *pIdx;` |
|   431006 |  3096 | `	pIdx = 0;` |
|   431006 |  3097 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3098 | `		if( !pInstr->iP2){` |
|        - |  3099 | `			/* No available index,load NULL */` |
|      ! 0 |  3100 | `			if( pTos >= pStack ){` |
|      ! 0 |  3101 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3102 | `			}else{` |
|        - |  3103 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3104 | `				pTos++;` |
|      ! 0 |  3105 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3106 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3107 | `			}` |
|        - |  3108 | `			/* Emit a notice */` |
|      ! 0 |  3109 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3110 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3111 | `			break;` |
|        - |  3112 | `		}` |
|      ! 0 |  3113 | `	}else{` |
|   431006 |  3114 | `		pIdx = pTos;` |
|   431006 |  3115 | `		pTos--;` |
|        - |  3116 | `	}` |
|   431006 |  3117 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3118 | `		/* String access */` |
|   349100 |  3119 | `		if( pIdx ){` |
|        - |  3120 | `			sxu32 nOfft;` |
|   349100 |  3121 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3122 | `				/* Force an int cast */` |
|      ! 0 |  3123 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3124 | `			}` |
|   349100 |  3125 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   349100 |  3126 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3127 | `				/* Invalid offset,load null */` |
|      ! 0 |  3128 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3129 | `			}else{` |
|   349100 |  3130 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   349100 |  3131 | `				int c = zData[nOfft];` |
|   349100 |  3132 | `				PH7_MemObjRelease(pTos);` |
|   349100 |  3133 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   349100 |  3134 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3135 | `			}` |
|   174573 |  3136 | `		}else{` |
|        - |  3137 | `			/* No available index,load NULL */` |
|      ! 0 |  3138 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3139 | `		}` |
|   349100 |  3140 | `		break;` |
|        - |  3141 | `	}` |
|    81908 |  3142 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3143 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3144 | `			ph7_value *pObj;` |
|      ! 0 |  3145 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3146 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3147 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3148 | `			}` |
|      ! 0 |  3149 | `		}` |
|      ! 0 |  3150 | `	}` |
|    81908 |  3151 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    81908 |  3152 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3153 | `		/* Point to the hashmap */` |
|    81908 |  3154 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    81908 |  3155 | `		if( pIdx ){` |
|        - |  3156 | `			/* Load the desired entry */` |
|    81908 |  3157 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    40953 |  3158 | `		}` |
|    81908 |  3159 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3160 | `			/* Create a new empty entry */` |
|      ! 0 |  3161 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3162 | `			if( rc == SXRET_OK ){` |
|        - |  3163 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3164 | `				pNode = pMap->pLast;` |
|      ! 0 |  3165 | `			}` |
|      ! 0 |  3166 | `		}` |
|    40953 |  3167 | `	}` |
|    81908 |  3168 | `	if( pIdx ){` |
|    81908 |  3169 | `		PH7_MemObjRelease(pIdx);` |
|    40953 |  3170 | `	}` |
|    81908 |  3171 | `	if( rc == SXRET_OK ){` |
|        - |  3172 | `		/* Load entry contents */` |
|    37888 |  3173 | `		if( pMap->iRef < 2 ){` |
|        - |  3174 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3175 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3176 | `			 */` |
|        7 |  3177 | `			pTos->nIdx = SXU32_HIGH;` |
|        7 |  3178 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|        4 |  3179 | `		}else{` |
|    37882 |  3180 | `			pTos->nIdx = pNode->nValIdx;` |
|    37882 |  3181 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    37882 |  3182 | `			PH7_HashmapUnref(pMap);` |
|        - |  3183 | `		}` |
|    18945 |  3184 | `	}else{` |
|        - |  3185 | `		/* No such entry,load NULL */` |
|    44022 |  3186 | `		PH7_MemObjRelease(pTos);` |
|    44022 |  3187 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3188 | `	}` |
|    81908 |  3189 | `	break;` |
|        - |  3190 | `					  }` |
|        - |  3191 | `/*` |
|        - |  3192 | ` * LOAD_CLOSURE * * P3` |
|        - |  3193 | ` *` |
|        - |  3194 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3195 | ` * name in the stack.` |
|        - |  3196 | ` */` |
|        2 |  3197 | `case PH7_OP_LOAD_CLOSURE:{` |
|        5 |  3198 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        5 |  3199 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3200 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3201 | `		ph7_vm_func *pClosure;` |
|        - |  3202 | `		char *zName;` |
|        - |  3203 | `		sxu32 mLen;` |
|        - |  3204 | `		sxu32 n;` |
|        - |  3205 | `		/* Create a new VM function */` |
|        5 |  3206 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3207 | `		/* Generate an unique closure name */` |
|        5 |  3208 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        5 |  3209 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3210 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3211 | `			goto Abort;` |
|        - |  3212 | `		}` |
|        5 |  3213 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        5 |  3214 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3215 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3216 | `		}` |
|        - |  3217 | `		/* Zero the stucture */` |
|        5 |  3218 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3219 | `		/* Perform a structure assignment on read-only items */` |
|        5 |  3220 | `		pClosure->aArgs = pFunc->aArgs;` |
|        5 |  3221 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        5 |  3222 | `		pClosure->aStatic = pFunc->aStatic;` |
|        5 |  3223 | `		pClosure->iFlags = pFunc->iFlags;` |
|        5 |  3224 | `		pClosure->pUserData = pFunc->pUserData;` |
|        5 |  3225 | `		pClosure->sSignature = pFunc->sSignature;` |
|        5 |  3226 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3227 | `		/* Register the closure */` |
|        5 |  3228 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3229 | `		/* Set up closure environment */` |
|        5 |  3230 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        5 |  3231 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       13 |  3232 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3233 | `			ph7_value *pValue;` |
|        9 |  3234 | `			pEnv = &aEnv[n];` |
|        9 |  3235 | `			sEnv.sName  = pEnv->sName;` |
|        9 |  3236 | `			sEnv.iFlags = pEnv->iFlags;` |
|        9 |  3237 | `			sEnv.nIdx = SXU32_HIGH;` |
|        9 |  3238 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|        9 |  3239 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3240 | `				/* Pass by reference */` |
|      ! 0 |  3241 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3242 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3243 | `					);` |
|      ! 0 |  3244 | `			}` |
|        - |  3245 | `			/* Standard pass by value */` |
|        9 |  3246 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|        9 |  3247 | `			if( pValue ){` |
|        - |  3248 | `				/* Copy imported value */` |
|        5 |  3249 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        2 |  3250 | `			}` |
|        - |  3251 | `			/* Insert the imported variable */` |
|        9 |  3252 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        5 |  3253 | `		}` |
|        - |  3254 | `		/* Finally,load the closure name on the stack */` |
|        5 |  3255 | `		pTos++;` |
|        5 |  3256 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        2 |  3257 | `	}` |
|        5 |  3258 | `	break;` |
|        - |  3259 | `						 }` |
|        - |  3260 | `/*` |
|        - |  3261 | ` * STORE * P2 P3` |
|        - |  3262 | ` *` |
|        - |  3263 | ` * Perform a store (Assignment) operation.` |
|        - |  3264 | ` */` |
|   100605 |  3265 | `case PH7_OP_STORE: {` |
|        - |  3266 | `	ph7_value *pObj;` |
|        - |  3267 | `	SyString sName;` |
|        - |  3268 | `#ifdef UNTRUST` |
|        - |  3269 | `	if( pTos < pStack ){` |
|        - |  3270 | `		goto Abort;` |
|        - |  3271 | `	}` |
|        - |  3272 | `#endif` |
|   201212 |  3273 | `	if( pInstr->iP2 ){` |
|        - |  3274 | `		sxu32 nIdx;` |
|        - |  3275 | `		/* Member store operation */` |
|     2268 |  3276 | `		nIdx = pTos->nIdx;` |
|     2268 |  3277 | `		VmPopOperand(&pTos,1);` |
|     2268 |  3278 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3279 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3280 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3281 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3282 | `		}else{` |
|        - |  3283 | `			/* Point to the desired memory object */` |
|     2264 |  3284 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2264 |  3285 | `			if( pObj ){` |
|        - |  3286 | `				/* Perform the store operation */` |
|     2264 |  3287 | `				PH7_MemObjStore(pTos,pObj);` |
|     1131 |  3288 | `			}` |
|        - |  3289 | `		}` |
|   101740 |  3290 | `		break;` |
|   198946 |  3291 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3292 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3293 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3294 | `			/* Force a string cast */` |
|      ! 0 |  3295 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3296 | `		}` |
|        7 |  3297 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3298 | `		pTos--;` |
|        - |  3299 | `#ifdef UNTRUST` |
|        - |  3300 | `		if( pTos < pStack  ){` |
|        - |  3301 | `			goto Abort;` |
|        - |  3302 | `		}` |
|        - |  3303 | `#endif` |
|        4 |  3304 | `	}else{` |
|   198940 |  3305 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3306 | `	}` |
|        - |  3307 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   198946 |  3308 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   198946 |  3309 | `	if( pObj == 0 ){` |
|      ! 0 |  3310 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3311 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3312 | `		goto Abort;` |
|        - |  3313 | `	}` |
|   198946 |  3314 | `	if( !pInstr->p3 ){` |
|        7 |  3315 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3316 | `	}` |
|        - |  3317 | `	/* Perform the store operation */` |
|   198946 |  3318 | `	PH7_MemObjStore(pTos,pObj);` |
|   198946 |  3319 | `	break;` |
|        - |  3320 | `				   }` |
|        - |  3321 | `/*` |
|        - |  3322 | ` * STORE_IDX:   P1 * P3` |
|        - |  3323 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3324 | ` *` |
|        - |  3325 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3326 | ` */` |
|    76097 |  3327 | `case PH7_OP_STORE_IDX:` |
|        - |  3328 | `case PH7_OP_STORE_IDX_REF: {` |
|   152196 |  3329 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3330 | `	ph7_value *pKey;` |
|        - |  3331 | `	sxu32 nIdx;` |
|   152196 |  3332 | `	if( pInstr->iP1 ){` |
|        - |  3333 | `		/* Key is next on stack */` |
|    55036 |  3334 | `		pKey = pTos;` |
|    55036 |  3335 | `		pTos--;` |
|    27519 |  3336 | `	}else{` |
|    97162 |  3337 | `		pKey = 0;` |
|        - |  3338 | `	}` |
|   152196 |  3339 | `	nIdx = pTos->nIdx;` |
|   152196 |  3340 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3341 | `		/* Hashmap already loaded */` |
|   152144 |  3342 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   152144 |  3343 | `		if( pMap->iRef < 2 ){` |
|        - |  3344 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3345 | `			pMap->iRef = 2;` |
|      ! 0 |  3346 | `		}` |
|    76073 |  3347 | `	}else{` |
|        - |  3348 | `		ph7_value *pObj;` |
|       53 |  3349 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3350 | `		if( pObj == 0 ){` |
|      ! 0 |  3351 | `			if( pKey ){` |
|      ! 0 |  3352 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3353 | `			}` |
|      ! 0 |  3354 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3355 | `			break;` |
|        - |  3356 | `		}` |
|        - |  3357 | `		/* Phase#1: Load the array */` |
|       53 |  3358 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3359 | `			VmPopOperand(&pTos,1);` |
|       53 |  3360 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3361 | `				/* Force a string cast */` |
|      ! 0 |  3362 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3363 | `			}` |
|       53 |  3364 | `			if( pKey == 0 ){` |
|        - |  3365 | `				/* Append string */` |
|        3 |  3366 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3367 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3368 | `				}` |
|        2 |  3369 | `			}else{` |
|        - |  3370 | `				sxu32 nOfft;` |
|       51 |  3371 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3372 | `					/* Force an int cast */` |
|       51 |  3373 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3374 | `				}` |
|       51 |  3375 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3376 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3377 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3378 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3379 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3380 | `				}else{` |
|      ! 0 |  3381 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3382 | `						/* Perform an append operation */` |
|      ! 0 |  3383 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3384 | `					}` |
|        - |  3385 | `				}` |
|        - |  3386 | `			}` |
|       53 |  3387 | `			if( pKey ){` |
|       51 |  3388 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3389 | `			}` |
|       53 |  3390 | `			break;` |
|      ! 0 |  3391 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3392 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3393 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3394 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3395 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3396 | `				goto Abort;` |
|        - |  3397 | `			}` |
|      ! 0 |  3398 | `		}` |
|      ! 0 |  3399 | `		pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - |  3400 | `	}` |
|   152144 |  3401 | `	VmPopOperand(&pTos,1);` |
|        - |  3402 | `	/* Phase#2: Perform the insertion */` |
|   152144 |  3403 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3404 | `		/* Insertion by reference */` |
|       15 |  3405 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3406 | `	}else{` |
|   152130 |  3407 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3408 | `	}` |
|   152144 |  3409 | `	if( pKey ){` |
|    54986 |  3410 | `		PH7_MemObjRelease(pKey);` |
|    27492 |  3411 | `	}` |
|   152144 |  3412 | `	break;` |
|        - |  3413 | `					   }` |
|        - |  3414 | `/*` |
|        - |  3415 | ` * INCR: P1 * *` |
|        - |  3416 | ` *` |
|        - |  3417 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3418 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3419 | ` * the stack and increment after that.` |
|        - |  3420 | ` */` |
|   156808 |  3421 | `case PH7_OP_INCR:` |
|        - |  3422 | `#ifdef UNTRUST` |
|        - |  3423 | `	if( pTos < pStack ){` |
|        - |  3424 | `		goto Abort;` |
|        - |  3425 | `	}` |
|        - |  3426 | `#endif` |
|   313662 |  3427 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   313662 |  3428 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3429 | `			ph7_value *pObj;` |
|   313662 |  3430 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3431 | `				/* Force a numeric cast */` |
|   313662 |  3432 | `				PH7_MemObjToNumeric(pObj);` |
|   313662 |  3433 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3434 | `					pObj->rVal++;` |
|        - |  3435 | `					/* Try to get an integer representation */` |
|      ! 0 |  3436 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3437 | `				}else{` |
|   313662 |  3438 | `					pObj->x.iVal++;` |
|   313662 |  3439 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3440 | `				}` |
|   313662 |  3441 | `				if( pInstr->iP1 ){` |
|        - |  3442 | `					/* Pre-icrement */` |
|       71 |  3443 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3444 | `				}` |
|   156852 |  3445 | `			}` |
|   156854 |  3446 | `		}else{` |
|      ! 0 |  3447 | `			if( pInstr->iP1 ){` |
|        - |  3448 | `				/* Force a numeric cast */` |
|      ! 0 |  3449 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3450 | `				/* Pre-increment */` |
|      ! 0 |  3451 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3452 | `					pTos->rVal++;` |
|        - |  3453 | `					/* Try to get an integer representation */` |
|      ! 0 |  3454 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3455 | `				}else{` |
|      ! 0 |  3456 | `					pTos->x.iVal++;` |
|      ! 0 |  3457 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3458 | `				}` |
|      ! 0 |  3459 | `			}` |
|        - |  3460 | `		}` |
|   156852 |  3461 | `	}` |
|   313662 |  3462 | `	break;` |
|        - |  3463 | `/*` |
|        - |  3464 | ` * DECR: P1 * *` |
|        - |  3465 | ` *` |
|        - |  3466 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3467 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3468 | ` * and decrement after that.` |
|        - |  3469 | ` */` |
|        2 |  3470 | `case PH7_OP_DECR:` |
|        - |  3471 | `#ifdef UNTRUST` |
|        - |  3472 | `	if( pTos < pStack ){` |
|        - |  3473 | `		goto Abort;` |
|        - |  3474 | `	}` |
|        - |  3475 | `#endif` |
|        5 |  3476 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3477 | `		/* Force a numeric cast */` |
|        5 |  3478 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3479 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3480 | `			ph7_value *pObj;` |
|        5 |  3481 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3482 | `				/* Force a numeric cast */` |
|        5 |  3483 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3484 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3485 | `					pObj->rVal--;` |
|        - |  3486 | `					/* Try to get an integer representation */` |
|      ! 0 |  3487 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3488 | `				}else{` |
|        5 |  3489 | `					pObj->x.iVal--;` |
|        5 |  3490 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3491 | `				}` |
|        5 |  3492 | `				if( pInstr->iP1 ){` |
|        - |  3493 | `					/* Pre-icrement */` |
|      ! 0 |  3494 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3495 | `				}` |
|        2 |  3496 | `			}` |
|        3 |  3497 | `		}else{` |
|      ! 0 |  3498 | `			if( pInstr->iP1 ){` |
|        - |  3499 | `				/* Pre-increment */` |
|      ! 0 |  3500 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3501 | `					pTos->rVal--;` |
|        - |  3502 | `					/* Try to get an integer representation */` |
|      ! 0 |  3503 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3504 | `				}else{` |
|      ! 0 |  3505 | `					pTos->x.iVal--;` |
|      ! 0 |  3506 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3507 | `				}` |
|      ! 0 |  3508 | `			}` |
|        - |  3509 | `		}` |
|        2 |  3510 | `	}` |
|        5 |  3511 | `	break;` |
|        - |  3512 | `/*` |
|        - |  3513 | ` * UMINUS: * * *` |
|        - |  3514 | ` *` |
|        - |  3515 | ` * Perform a unary minus operation.` |
|        - |  3516 | ` */` |
|    21361 |  3517 | `case PH7_OP_UMINUS:` |
|        - |  3518 | `#ifdef UNTRUST` |
|        - |  3519 | `	if( pTos < pStack ){` |
|        - |  3520 | `		goto Abort;` |
|        - |  3521 | `	}` |
|        - |  3522 | `#endif` |
|        - |  3523 | `	/* Force a numeric (integer,real or both) cast */` |
|    42724 |  3524 | `	PH7_MemObjToNumeric(pTos);` |
|    42724 |  3525 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       25 |  3526 | `		pTos->rVal = -pTos->rVal;` |
|       12 |  3527 | `	}` |
|    42724 |  3528 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    42700 |  3529 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    21349 |  3530 | `	}` |
|    42724 |  3531 | `	break;` |
|        - |  3532 | `/*` |
|        - |  3533 | ` * UPLUS: * * *` |
|        - |  3534 | ` *` |
|        - |  3535 | ` * Perform a unary plus operation.` |
|        - |  3536 | ` */` |
|       16 |  3537 | `case PH7_OP_UPLUS:` |
|        - |  3538 | `#ifdef UNTRUST` |
|        - |  3539 | `	if( pTos < pStack ){` |
|        - |  3540 | `		goto Abort;` |
|        - |  3541 | `	}` |
|        - |  3542 | `#endif` |
|        - |  3543 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3544 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3545 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3546 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3547 | `	}` |
|       33 |  3548 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3549 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3550 | `	}` |
|       33 |  3551 | `	break;` |
|        - |  3552 | `/*` |
|        - |  3553 | ` * OP_LNOT: * * *` |
|        - |  3554 | ` *` |
|        - |  3555 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3556 | ` * with its complement.` |
|        - |  3557 | ` */` |
|    46674 |  3558 | `case PH7_OP_LNOT:` |
|        - |  3559 | `#ifdef UNTRUST` |
|        - |  3560 | `	if( pTos < pStack ){` |
|        - |  3561 | `		goto Abort;` |
|        - |  3562 | `	}` |
|        - |  3563 | `#endif` |
|        - |  3564 | `	/* Force a boolean cast */` |
|    93394 |  3565 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3566 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3567 | `	}` |
|    93394 |  3568 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    93394 |  3569 | `	break;` |
|        - |  3570 | `/*` |
|        - |  3571 | ` * OP_BITNOT: * * *` |
|        - |  3572 | ` *` |
|        - |  3573 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3574 | ` * with its ones-complement.` |
|        - |  3575 | ` */` |
|        3 |  3576 | `case PH7_OP_BITNOT:` |
|        - |  3577 | `#ifdef UNTRUST` |
|        - |  3578 | `	if( pTos < pStack ){` |
|        - |  3579 | `		goto Abort;` |
|        - |  3580 | `	}` |
|        - |  3581 | `#endif` |
|        - |  3582 | `	/* Force an integer cast */` |
|        7 |  3583 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3584 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3585 | `	}` |
|        7 |  3586 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|        7 |  3587 | `	break;` |
|        - |  3588 | `/* OP_MUL * * *` |
|        - |  3589 | ` * OP_MUL_STORE * * *` |
|        - |  3590 | ` *` |
|        - |  3591 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3592 | ` * and push the result back onto the stack.` |
|        - |  3593 | ` */` |
|     1234 |  3594 | `case PH7_OP_MUL:` |
|        - |  3595 | `case PH7_OP_MUL_STORE: {` |
|     2470 |  3596 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3597 | `	/* Force the operand to be numeric */` |
|        - |  3598 | `#ifdef UNTRUST` |
|        - |  3599 | `	if( pNos < pStack ){` |
|        - |  3600 | `		goto Abort;` |
|        - |  3601 | `	}` |
|        - |  3602 | `#endif` |
|     2470 |  3603 | `	PH7_MemObjToNumeric(pTos);` |
|     2470 |  3604 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3605 | `	/* Perform the requested operation */` |
|     2470 |  3606 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3607 | `		/* Floating point arithemic */` |
|        - |  3608 | `		ph7_real a,b,r;` |
|       17 |  3609 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3610 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3611 | `		}` |
|       17 |  3612 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3613 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3614 | `		}` |
|       17 |  3615 | `		a = pNos->rVal;` |
|       17 |  3616 | `		b = pTos->rVal;` |
|       17 |  3617 | `		r = a * b;` |
|        - |  3618 | `		/* Push the result */` |
|       17 |  3619 | `		pNos->rVal = r;` |
|       17 |  3620 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3621 | `		/* Try to get an integer representation */` |
|       17 |  3622 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3623 | `	}else{` |
|        - |  3624 | `		/* Integer arithmetic */` |
|        - |  3625 | `		sxi64 a,b,r;` |
|     2454 |  3626 | `		a = pNos->x.iVal;` |
|     2454 |  3627 | `		b = pTos->x.iVal;` |
|     2454 |  3628 | `		r = a * b;` |
|        - |  3629 | `		/* Push the result */` |
|     2454 |  3630 | `		pNos->x.iVal = r;` |
|     2454 |  3631 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3632 | `	}` |
|     2470 |  3633 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3634 | `		ph7_value *pObj;` |
|       19 |  3635 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3636 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  3637 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  3638 | `			PH7_MemObjStore(pNos,pObj);` |
|        9 |  3639 | `		}` |
|        9 |  3640 | `	}` |
|     2470 |  3641 | `	VmPopOperand(&pTos,1);` |
|     2470 |  3642 | `	break;` |
|        - |  3643 | `				 }` |
|        - |  3644 | `/* OP_ADD * * *` |
|        - |  3645 | ` *` |
|        - |  3646 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3647 | ` * and push the result back onto the stack.` |
|        - |  3648 | ` */` |
|      426 |  3649 | `case PH7_OP_ADD:{` |
|      854 |  3650 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3651 | `#ifdef UNTRUST` |
|        - |  3652 | `	if( pNos < pStack ){` |
|        - |  3653 | `		goto Abort;` |
|        - |  3654 | `	}` |
|        - |  3655 | `#endif` |
|        - |  3656 | `	/* Perform the addition */` |
|      854 |  3657 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      854 |  3658 | `	VmPopOperand(&pTos,1);` |
|      854 |  3659 | `	break;` |
|        - |  3660 | `				}` |
|        - |  3661 | `/*` |
|        - |  3662 | ` * OP_ADD_STORE * * *` |
|        - |  3663 | ` *` |
|        - |  3664 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3665 | ` * and push the result back onto the stack.` |
|        - |  3666 | ` */` |
|      481 |  3667 | `case PH7_OP_ADD_STORE:{` |
|      963 |  3668 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3669 | `	ph7_value *pObj;` |
|        - |  3670 | `	sxu32 nIdx;` |
|        - |  3671 | `#ifdef UNTRUST` |
|        - |  3672 | `	if( pNos < pStack ){` |
|        - |  3673 | `		goto Abort;` |
|        - |  3674 | `	}` |
|        - |  3675 | `#endif` |
|        - |  3676 | `	/* Perform the addition */` |
|      963 |  3677 | `	nIdx = pTos->nIdx;` |
|      963 |  3678 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3679 | `	/* Peform the store operation */` |
|      963 |  3680 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3681 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      963 |  3682 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      963 |  3683 | `		PH7_MemObjStore(pTos,pObj);` |
|      481 |  3684 | `	}` |
|        - |  3685 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      963 |  3686 | `	PH7_MemObjStore(pTos,pNos);` |
|      963 |  3687 | `	VmPopOperand(&pTos,1);` |
|      963 |  3688 | `	break;` |
|        - |  3689 | `				}` |
|        - |  3690 | `/* OP_SUB * * *` |
|        - |  3691 | ` *` |
|        - |  3692 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3693 | ` * first (what was next on the stack) from the second (the` |
|        - |  3694 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3695 | ` */` |
|      294 |  3696 | `case PH7_OP_SUB: {` |
|      589 |  3697 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3698 | `#ifdef UNTRUST` |
|        - |  3699 | `	if( pNos < pStack ){` |
|        - |  3700 | `		goto Abort;` |
|        - |  3701 | `	}` |
|        - |  3702 | `#endif` |
|      589 |  3703 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3704 | `		/* Floating point arithemic */` |
|        - |  3705 | `		ph7_real a,b,r;` |
|       95 |  3706 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3707 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3708 | `		}` |
|       95 |  3709 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3710 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3711 | `		}` |
|       95 |  3712 | `		a = pNos->rVal;` |
|       95 |  3713 | `		b = pTos->rVal;` |
|       95 |  3714 | `		r = a - b;` |
|        - |  3715 | `		/* Push the result */` |
|       95 |  3716 | `		pNos->rVal = r;` |
|       95 |  3717 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3718 | `		/* Try to get an integer representation */` |
|       95 |  3719 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  3720 | `	}else{` |
|        - |  3721 | `		/* Integer arithmetic */` |
|        - |  3722 | `		sxi64 a,b,r;` |
|      495 |  3723 | `		a = pNos->x.iVal;` |
|      495 |  3724 | `		b = pTos->x.iVal;` |
|      495 |  3725 | `		r = a - b;` |
|        - |  3726 | `		/* Push the result */` |
|      495 |  3727 | `		pNos->x.iVal = r;` |
|      495 |  3728 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3729 | `	}` |
|      589 |  3730 | `	VmPopOperand(&pTos,1);` |
|      589 |  3731 | `	break;` |
|        - |  3732 | `				 }` |
|        - |  3733 | `/* OP_SUB_STORE * * *` |
|        - |  3734 | ` *` |
|        - |  3735 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3736 | ` * first (what was next on the stack) from the second (the` |
|        - |  3737 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3738 | ` */` |
|        1 |  3739 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3740 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3741 | `	ph7_value *pObj;` |
|        - |  3742 | `#ifdef UNTRUST` |
|        - |  3743 | `	if( pNos < pStack ){` |
|        - |  3744 | `		goto Abort;` |
|        - |  3745 | `	}` |
|        - |  3746 | `#endif` |
|        3 |  3747 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3748 | `		/* Floating point arithemic */` |
|        - |  3749 | `		ph7_real a,b,r;` |
|      ! 0 |  3750 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3751 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3752 | `		}` |
|      ! 0 |  3753 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3754 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3755 | `		}` |
|      ! 0 |  3756 | `		a = pTos->rVal;` |
|      ! 0 |  3757 | `		b = pNos->rVal;` |
|      ! 0 |  3758 | `		r = a - b;` |
|        - |  3759 | `		/* Push the result */` |
|      ! 0 |  3760 | `		pNos->rVal = r;` |
|      ! 0 |  3761 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3762 | `		/* Try to get an integer representation */` |
|      ! 0 |  3763 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3764 | `	}else{` |
|        - |  3765 | `		/* Integer arithmetic */` |
|        - |  3766 | `		sxi64 a,b,r;` |
|        3 |  3767 | `		a = pTos->x.iVal;` |
|        3 |  3768 | `		b = pNos->x.iVal;` |
|        3 |  3769 | `		r = a - b;` |
|        - |  3770 | `		/* Push the result */` |
|        3 |  3771 | `		pNos->x.iVal = r;` |
|        3 |  3772 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3773 | `	}` |
|        3 |  3774 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3775 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3776 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3777 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3778 | `	}` |
|        3 |  3779 | `	VmPopOperand(&pTos,1);` |
|        3 |  3780 | `	break;` |
|        - |  3781 | `				 }` |
|        - |  3782 |  |
|        - |  3783 | `/*` |
|        - |  3784 | ` * OP_MOD * * *` |
|        - |  3785 | ` *` |
|        - |  3786 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3787 | ` * first (what was next on the stack) from the second (the` |
|        - |  3788 | ` * top of the stack) and push the remainder after division` |
|        - |  3789 | ` * onto the stack.` |
|        - |  3790 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3791 | ` */` |
|      296 |  3792 | `case PH7_OP_MOD:{` |
|      594 |  3793 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3794 | `	sxi64 a,b,r;` |
|        - |  3795 | `#ifdef UNTRUST` |
|        - |  3796 | `	if( pNos < pStack ){` |
|        - |  3797 | `		goto Abort;` |
|        - |  3798 | `	}` |
|        - |  3799 | `#endif` |
|        - |  3800 | `	/* Force the operands to be integer */` |
|      594 |  3801 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3802 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3803 | `	}` |
|      594 |  3804 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3805 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3806 | `	}` |
|        - |  3807 | `	/* Perform the requested operation */` |
|      594 |  3808 | `	a = pNos->x.iVal;` |
|      594 |  3809 | `	b = pTos->x.iVal;` |
|      594 |  3810 | `	if( b == 0 ){` |
|        3 |  3811 | `		r = 0;` |
|        3 |  3812 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3813 | `		/* goto Abort; */` |
|        2 |  3814 | `	}else{` |
|      591 |  3815 | `		r = a%b;` |
|        - |  3816 | `	}` |
|        - |  3817 | `	/* Push the result */` |
|      594 |  3818 | `	pNos->x.iVal = r;` |
|      594 |  3819 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      594 |  3820 | `	VmPopOperand(&pTos,1);` |
|      594 |  3821 | `	break;` |
|        - |  3822 | `				}` |
|        - |  3823 | `/*` |
|        - |  3824 | ` * OP_MOD_STORE * * *` |
|        - |  3825 | ` *` |
|        - |  3826 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3827 | ` * first (what was next on the stack) from the second (the` |
|        - |  3828 | ` * top of the stack) and push the remainder after division` |
|        - |  3829 | ` * onto the stack.` |
|        - |  3830 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3831 | ` */` |
|        1 |  3832 | `case PH7_OP_MOD_STORE: {` |
|        3 |  3833 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3834 | `	ph7_value *pObj;` |
|        - |  3835 | `	sxi64 a,b,r;` |
|        - |  3836 | `#ifdef UNTRUST` |
|        - |  3837 | `	if( pNos < pStack ){` |
|        - |  3838 | `		goto Abort;` |
|        - |  3839 | `	}` |
|        - |  3840 | `#endif` |
|        - |  3841 | `	/* Force the operands to be integer */` |
|        3 |  3842 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3843 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3844 | `	}` |
|        3 |  3845 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3846 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3847 | `	}` |
|        - |  3848 | `	/* Perform the requested operation */` |
|        3 |  3849 | `	a = pTos->x.iVal;` |
|        3 |  3850 | `	b = pNos->x.iVal;` |
|        3 |  3851 | `	if( b == 0 ){` |
|      ! 0 |  3852 | `		r = 0;` |
|      ! 0 |  3853 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3854 | `		/* goto Abort; */` |
|      ! 0 |  3855 | `	}else{` |
|        3 |  3856 | `		r = a%b;` |
|        - |  3857 | `	}` |
|        - |  3858 | `	/* Push the result */` |
|        3 |  3859 | `	pNos->x.iVal = r;` |
|        3 |  3860 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  3861 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3862 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3863 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3864 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3865 | `	}` |
|        3 |  3866 | `	VmPopOperand(&pTos,1);` |
|        3 |  3867 | `	break;` |
|        - |  3868 | `				}` |
|        - |  3869 | `/*` |
|        - |  3870 | ` * OP_DIV * * *` |
|        - |  3871 | ` *` |
|        - |  3872 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3873 | ` * first (what was next on the stack) from the second (the` |
|        - |  3874 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3875 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3876 | ` */` |
|       28 |  3877 | `case PH7_OP_DIV:{` |
|       58 |  3878 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3879 | `	ph7_real a,b,r;` |
|        - |  3880 | `#ifdef UNTRUST` |
|        - |  3881 | `	if( pNos < pStack ){` |
|        - |  3882 | `		goto Abort;` |
|        - |  3883 | `	}` |
|        - |  3884 | `#endif` |
|        - |  3885 | `	/* Force the operands to be real */` |
|       58 |  3886 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  3887 | `		PH7_MemObjToReal(pTos);` |
|       26 |  3888 | `	}` |
|       58 |  3889 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  3890 | `		PH7_MemObjToReal(pNos);` |
|        9 |  3891 | `	}` |
|        - |  3892 | `	/* Perform the requested operation */` |
|       58 |  3893 | `	a = pNos->rVal;` |
|       58 |  3894 | `	b = pTos->rVal;` |
|       58 |  3895 | `	if( b == 0 ){` |
|        - |  3896 | `		/* Division by zero */` |
|        3 |  3897 | `		pNos->rVal = 0;` |
|        3 |  3898 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  3899 | `		/* goto Abort; */` |
|        2 |  3900 | `	}else{` |
|       55 |  3901 | `		r = a/b;` |
|        - |  3902 | `		/* Push the result */` |
|       55 |  3903 | `		pNos->rVal = r;` |
|       55 |  3904 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3905 | `		/* Try to get an integer representation */` |
|       55 |  3906 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3907 | `	}` |
|       58 |  3908 | `	VmPopOperand(&pTos,1);` |
|       58 |  3909 | `	break;` |
|        - |  3910 | `				}` |
|        - |  3911 | `/*` |
|        - |  3912 | ` * OP_DIV_STORE * * *` |
|        - |  3913 | ` *` |
|        - |  3914 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3915 | ` * first (what was next on the stack) from the second (the` |
|        - |  3916 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3917 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3918 | ` */` |
|        1 |  3919 | `case PH7_OP_DIV_STORE:{` |
|        3 |  3920 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3921 | `	ph7_value *pObj;` |
|        - |  3922 | `	ph7_real a,b,r;` |
|        - |  3923 | `#ifdef UNTRUST` |
|        - |  3924 | `	if( pNos < pStack ){` |
|        - |  3925 | `		goto Abort;` |
|        - |  3926 | `	}` |
|        - |  3927 | `#endif` |
|        - |  3928 | `	/* Force the operands to be real */` |
|        3 |  3929 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3930 | `		PH7_MemObjToReal(pTos);` |
|        1 |  3931 | `	}` |
|        3 |  3932 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3933 | `		PH7_MemObjToReal(pNos);` |
|        1 |  3934 | `	}` |
|        - |  3935 | `	/* Perform the requested operation */` |
|        3 |  3936 | `	a = pTos->rVal;` |
|        3 |  3937 | `	b = pNos->rVal;` |
|        3 |  3938 | `	if( b == 0 ){` |
|        - |  3939 | `		/* Division by zero */` |
|      ! 0 |  3940 | `		r = 0;` |
|      ! 0 |  3941 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  3942 | `		/* goto Abort; */` |
|      ! 0 |  3943 | `	}else{` |
|        3 |  3944 | `		r = a/b;` |
|        - |  3945 | `		/* Push the result */` |
|        3 |  3946 | `		pNos->rVal = r;` |
|        3 |  3947 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3948 | `		/* Try to get an integer representation */` |
|        3 |  3949 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3950 | `	}` |
|        3 |  3951 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3952 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3953 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3954 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3955 | `	}` |
|        3 |  3956 | `	VmPopOperand(&pTos,1);` |
|        3 |  3957 | `	break;` |
|        - |  3958 | `				}` |
|        - |  3959 | `/* OP_BAND * * *` |
|        - |  3960 | ` *` |
|        - |  3961 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3962 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  3963 | ` * two elements.` |
|        - |  3964 | `*/` |
|        - |  3965 | `/* OP_BOR * * *` |
|        - |  3966 | ` *` |
|        - |  3967 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3968 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  3969 | ` * two elements.` |
|        - |  3970 | ` */` |
|        - |  3971 | `/* OP_BXOR * * *` |
|        - |  3972 | ` *` |
|        - |  3973 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3974 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  3975 | ` * two elements.` |
|        - |  3976 | ` */` |
|       19 |  3977 | `case PH7_OP_BAND:` |
|        - |  3978 | `case PH7_OP_BOR:` |
|        - |  3979 | `case PH7_OP_BXOR:{` |
|       39 |  3980 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3981 | `	sxi64 a,b,r;` |
|        - |  3982 | `#ifdef UNTRUST` |
|        - |  3983 | `	if( pNos < pStack ){` |
|        - |  3984 | `		goto Abort;` |
|        - |  3985 | `	}` |
|        - |  3986 | `#endif` |
|        - |  3987 | `	/* Force the operands to be integer */` |
|       39 |  3988 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3989 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3990 | `	}` |
|       39 |  3991 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3992 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3993 | `	}` |
|        - |  3994 | `	/* Perform the requested operation */` |
|       39 |  3995 | `	a = pNos->x.iVal;` |
|       39 |  3996 | `	b = pTos->x.iVal;` |
|       39 |  3997 | `	switch(pInstr->iOp){` |
|        6 |  3998 | `	case PH7_OP_BOR_STORE:` |
|       13 |  3999 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4000 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4001 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        7 |  4002 | `	case PH7_OP_BAND_STORE:` |
|        7 |  4003 | `	case PH7_OP_BAND:` |
|       15 |  4004 | `	default:          r = a&b; break;` |
|        - |  4005 | `	}` |
|        - |  4006 | `	/* Push the result */` |
|       39 |  4007 | `	pNos->x.iVal = r;` |
|       39 |  4008 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       39 |  4009 | `	VmPopOperand(&pTos,1);` |
|       39 |  4010 | `	break;` |
|        - |  4011 | `				 }` |
|        - |  4012 | `/* OP_BAND_STORE * * *` |
|        - |  4013 | ` *` |
|        - |  4014 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4015 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4016 | ` * two elements.` |
|        - |  4017 | `*/` |
|        - |  4018 | `/* OP_BOR_STORE * * *` |
|        - |  4019 | ` *` |
|        - |  4020 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4021 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4022 | ` * two elements.` |
|        - |  4023 | ` */` |
|        - |  4024 | `/* OP_BXOR_STORE * * *` |
|        - |  4025 | ` *` |
|        - |  4026 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4027 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4028 | ` * two elements.` |
|        - |  4029 | ` */` |
|        7 |  4030 | `case PH7_OP_BAND_STORE:` |
|        - |  4031 | `case PH7_OP_BOR_STORE:` |
|        - |  4032 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4033 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4034 | `	ph7_value *pObj;` |
|        - |  4035 | `	sxi64 a,b,r;` |
|        - |  4036 | `#ifdef UNTRUST` |
|        - |  4037 | `	if( pNos < pStack ){` |
|        - |  4038 | `		goto Abort;` |
|        - |  4039 | `	}` |
|        - |  4040 | `#endif` |
|        - |  4041 | `	/* Force the operands to be integer */` |
|       15 |  4042 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4043 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4044 | `	}` |
|       15 |  4045 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4046 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4047 | `	}` |
|        - |  4048 | `	/* Perform the requested operation */` |
|       15 |  4049 | `	a = pTos->x.iVal;` |
|       15 |  4050 | `	b = pNos->x.iVal;` |
|       15 |  4051 | `	switch(pInstr->iOp){` |
|        2 |  4052 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4053 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4054 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4055 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4056 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4057 | `	case PH7_OP_BAND:` |
|        5 |  4058 | `	default:          r = a&b; break;` |
|        - |  4059 | `	}` |
|        - |  4060 | `	/* Push the result */` |
|       15 |  4061 | `	pNos->x.iVal = r;` |
|       15 |  4062 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4063 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4064 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4065 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4066 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4067 | `	}` |
|       15 |  4068 | `	VmPopOperand(&pTos,1);` |
|       15 |  4069 | `	break;` |
|        - |  4070 | `				 }` |
|        - |  4071 | `/* OP_SHL * * *` |
|        - |  4072 | ` *` |
|        - |  4073 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4074 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4075 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4076 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4077 | ` */` |
|        - |  4078 | `/* OP_SHR * * *` |
|        - |  4079 | ` *` |
|        - |  4080 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4081 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4082 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4083 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4084 | ` */` |
|        9 |  4085 | `case PH7_OP_SHL:` |
|        - |  4086 | `case PH7_OP_SHR: {` |
|       19 |  4087 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4088 | `	sxi64 a,r;` |
|        - |  4089 | `	sxi32 b;` |
|        - |  4090 | `#ifdef UNTRUST` |
|        - |  4091 | `	if( pNos < pStack ){` |
|        - |  4092 | `		goto Abort;` |
|        - |  4093 | `	}` |
|        - |  4094 | `#endif` |
|        - |  4095 | `	/* Force the operands to be integer */` |
|       19 |  4096 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4097 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4098 | `	}` |
|       19 |  4099 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4100 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4101 | `	}` |
|        - |  4102 | `	/* Perform the requested operation */` |
|       19 |  4103 | `	a = pNos->x.iVal;` |
|       19 |  4104 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4105 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4106 | `		r = a << b;` |
|        6 |  4107 | `	}else{` |
|        9 |  4108 | `		r = a >> b;` |
|        - |  4109 | `	}` |
|        - |  4110 | `	/* Push the result */` |
|       19 |  4111 | `	pNos->x.iVal = r;` |
|       19 |  4112 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4113 | `	VmPopOperand(&pTos,1);` |
|       19 |  4114 | `	break;` |
|        - |  4115 | `				 }` |
|        - |  4116 | `/*  OP_SHL_STORE * * *` |
|        - |  4117 | ` *` |
|        - |  4118 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4119 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4120 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4121 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4122 | ` */` |
|        - |  4123 | `/* OP_SHR_STORE * * *` |
|        - |  4124 | ` *` |
|        - |  4125 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4126 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4127 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4128 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4129 | ` */` |
|        7 |  4130 | `case PH7_OP_SHL_STORE:` |
|        - |  4131 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4132 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4133 | `	ph7_value *pObj;` |
|        - |  4134 | `	sxi64 a,r;` |
|        - |  4135 | `	sxi32 b;` |
|        - |  4136 | `#ifdef UNTRUST` |
|        - |  4137 | `	if( pNos < pStack ){` |
|        - |  4138 | `		goto Abort;` |
|        - |  4139 | `	}` |
|        - |  4140 | `#endif` |
|        - |  4141 | `	/* Force the operands to be integer */` |
|       15 |  4142 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4143 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4144 | `	}` |
|       15 |  4145 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4146 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4147 | `	}` |
|        - |  4148 | `	/* Perform the requested operation */` |
|       15 |  4149 | `	a = pTos->x.iVal;` |
|       15 |  4150 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4151 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4152 | `		r = a << b;` |
|        4 |  4153 | `	}else{` |
|        9 |  4154 | `		r = a >> b;` |
|        - |  4155 | `	}` |
|        - |  4156 | `	/* Push the result */` |
|       15 |  4157 | `	pNos->x.iVal = r;` |
|       15 |  4158 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4159 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4160 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4161 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4162 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4163 | `	}` |
|       15 |  4164 | `	VmPopOperand(&pTos,1);` |
|       15 |  4165 | `	break;` |
|        - |  4166 | `				 }` |
|        - |  4167 | `/* CAT:  P1 * *` |
|        - |  4168 | ` *` |
|        - |  4169 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4170 | ` * back.` |
|        - |  4171 | ` */` |
|    57530 |  4172 | `case PH7_OP_CAT:{` |
|        - |  4173 | `	ph7_value *pNos,*pCur;` |
|   115062 |  4174 | `	if( pInstr->iP1 < 1 ){` |
|    88290 |  4175 | `		pNos = &pTos[-1];` |
|    44146 |  4176 | `	}else{` |
|    26774 |  4177 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4178 | `	}` |
|        - |  4179 | `#ifdef UNTRUST` |
|        - |  4180 | `	if( pNos < pStack ){` |
|        - |  4181 | `		goto Abort;` |
|        - |  4182 | `	}` |
|        - |  4183 | `#endif` |
|        - |  4184 | `	/* Force a string cast */` |
|   115062 |  4185 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      910 |  4186 | `		PH7_MemObjToString(pNos);` |
|      454 |  4187 | `	}` |
|   115062 |  4188 | `	pCur = &pNos[1];` |
|   231742 |  4189 | `	while( pCur <= pTos ){` |
|   116682 |  4190 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50366 |  4191 | `			PH7_MemObjToString(pCur);` |
|    25182 |  4192 | `		}` |
|        - |  4193 | `		/* Perform the concatenation */` |
|   116682 |  4194 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   116644 |  4195 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    58321 |  4196 | `		}` |
|   116682 |  4197 | `		SyBlobRelease(&pCur->sBlob);` |
|   116682 |  4198 | `		pCur++;` |
|        2 |  4199 | `	}` |
|   115062 |  4200 | `	pTos = pNos;` |
|   115062 |  4201 | `	break;` |
|        - |  4202 | `				}` |
|        - |  4203 | `/*  CAT_STORE: * * *` |
|        - |  4204 | ` *` |
|        - |  4205 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4206 | ` * back.` |
|        - |  4207 | ` */` |
|     2411 |  4208 | `case PH7_OP_CAT_STORE:{` |
|     4824 |  4209 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4210 | `	ph7_value *pObj;` |
|        - |  4211 | `#ifdef UNTRUST` |
|        - |  4212 | `	if( pNos < pStack ){` |
|        - |  4213 | `		goto Abort;` |
|        - |  4214 | `	}` |
|        - |  4215 | `#endif` |
|     4824 |  4216 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4217 | `		/* Force a string cast */` |
|      ! 0 |  4218 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4219 | `	}` |
|     4824 |  4220 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4221 | `		/* Force a string cast */` |
|      ! 0 |  4222 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4223 | `	}` |
|        - |  4224 | `	/* Perform the concatenation (Reverse order) */` |
|     4824 |  4225 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     4824 |  4226 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     2411 |  4227 | `	}` |
|        - |  4228 | `	/* Perform the store operation */` |
|     4824 |  4229 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4230 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     4824 |  4231 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     4824 |  4232 | `		PH7_MemObjStore(pTos,pObj);` |
|     2411 |  4233 | `	}` |
|     4824 |  4234 | `	PH7_MemObjStore(pTos,pNos);` |
|     4824 |  4235 | `	VmPopOperand(&pTos,1);` |
|     4824 |  4236 | `	break;` |
|        - |  4237 | `				}` |
|        - |  4238 | `/* OP_AND: * * *` |
|        - |  4239 | ` *` |
|        - |  4240 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4241 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4242 | ` * stack.` |
|        - |  4243 | ` */` |
|        - |  4244 | `/* OP_OR: * * *` |
|        - |  4245 | ` *` |
|        - |  4246 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4247 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4248 | ` * stack.` |
|        - |  4249 | ` */` |
|    99801 |  4250 | `case PH7_OP_LAND:` |
|        - |  4251 | `case PH7_OP_LOR: {` |
|   199648 |  4252 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4253 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4254 | `#ifdef UNTRUST` |
|        - |  4255 | `	if( pNos < pStack ){` |
|        - |  4256 | `		goto Abort;` |
|        - |  4257 | `	}` |
|        - |  4258 | `#endif` |
|        - |  4259 | `	/* Force a boolean cast */` |
|   199648 |  4260 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4261 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4262 | `	}` |
|   199648 |  4263 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4264 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4265 | `	}` |
|   199648 |  4266 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   199648 |  4267 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   199648 |  4268 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4269 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|   100596 |  4270 | `		v1 = and_logic[v1*3+v2];` |
|    50321 |  4271 | `	}else{` |
|        - |  4272 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|    99054 |  4273 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4274 | `	}` |
|   199648 |  4275 | `	if( v1 == 2 ){` |
|      ! 0 |  4276 | `		v1 = 1;` |
|      ! 0 |  4277 | `	}` |
|   199648 |  4278 | `	VmPopOperand(&pTos,1);` |
|   199648 |  4279 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   199648 |  4280 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   199648 |  4281 | `	break;` |
|        - |  4282 | `				 }` |
|        - |  4283 | `/* OP_LXOR: * * *` |
|        - |  4284 | ` *` |
|        - |  4285 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4286 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4287 | ` * stack.` |
|        - |  4288 | ` * According to the PHP language reference manual:` |
|        - |  4289 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4290 | ` *  TRUE,but not both.` |
|        - |  4291 | ` */` |
|        5 |  4292 | `case PH7_OP_LXOR:{` |
|       11 |  4293 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4294 | `	sxi32 v = 0;` |
|        - |  4295 | `#ifdef UNTRUST` |
|        - |  4296 | `	if( pNos < pStack ){` |
|        - |  4297 | `		goto Abort;` |
|        - |  4298 | `	}` |
|        - |  4299 | `#endif` |
|        - |  4300 | `	/* Force a boolean cast */` |
|       11 |  4301 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4302 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4303 | `	}` |
|       11 |  4304 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4305 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4306 | `	}` |
|       11 |  4307 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4308 | `		v = 1;` |
|        3 |  4309 | `	}` |
|       11 |  4310 | `	VmPopOperand(&pTos,1);` |
|       11 |  4311 | `	pTos->x.iVal = v;` |
|       11 |  4312 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4313 | `	break;` |
|        - |  4314 | `				 }` |
|        - |  4315 | `/* OP_EQ P1 P2 P3` |
|        - |  4316 | ` *` |
|        - |  4317 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4318 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4319 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4320 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4321 | ` */` |
|        - |  4322 | `/* OP_NEQ P1 P2 P3` |
|        - |  4323 | ` *` |
|        - |  4324 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4325 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4326 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4327 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4328 | ` */` |
|     3607 |  4329 | `case PH7_OP_EQ:` |
|        - |  4330 | `case PH7_OP_NEQ: {` |
|     7216 |  4331 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4332 | `	/* Perform the comparison and act accordingly */` |
|        - |  4333 | `#ifdef UNTRUST` |
|        - |  4334 | `	if( pNos < pStack ){` |
|        - |  4335 | `		goto Abort;` |
|        - |  4336 | `	}` |
|        - |  4337 | `#endif` |
|     7216 |  4338 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7216 |  4339 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       11 |  4340 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7211 |  4341 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7180 |  4342 | `		rc = rc == 0;` |
|     3591 |  4343 | `	}else{` |
|       28 |  4344 | `		rc = rc != 0;` |
|        - |  4345 | `	}` |
|     7216 |  4346 | `	VmPopOperand(&pTos,1);` |
|     7216 |  4347 | `	if( !pInstr->iP2 ){` |
|        - |  4348 | `		/* Push comparison result without taking the jump */` |
|     7216 |  4349 | `		PH7_MemObjRelease(pTos);` |
|     7216 |  4350 | `		pTos->x.iVal = rc;` |
|        - |  4351 | `		/* Invalidate any prior representation */` |
|     7216 |  4352 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3609 |  4353 | `	}else{` |
|      ! 0 |  4354 | `		if( rc ){` |
|        - |  4355 | `			/* Jump to the desired location */` |
|      ! 0 |  4356 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4357 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4358 | `		}` |
|        - |  4359 | `	}` |
|     7216 |  4360 | `	break;` |
|        - |  4361 | `				 }` |
|        - |  4362 | `/* OP_TEQ P1 P2 *` |
|        - |  4363 | ` *` |
|        - |  4364 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4365 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4366 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4367 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4368 | ` */` |
|   120324 |  4369 | `case PH7_OP_TEQ: {` |
|   240650 |  4370 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4371 | `	/* Perform the comparison and act accordingly */` |
|        - |  4372 | `#ifdef UNTRUST` |
|        - |  4373 | `	if( pNos < pStack ){` |
|        - |  4374 | `		goto Abort;` |
|        - |  4375 | `	}` |
|        - |  4376 | `#endif` |
|   240650 |  4377 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   240650 |  4378 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4379 | `		rc = 0;` |
|        2 |  4380 | `	}else{` |
|   240648 |  4381 | `		rc = rc == 0;` |
|        - |  4382 | `	}` |
|   240650 |  4383 | `	VmPopOperand(&pTos,1);` |
|   240650 |  4384 | `	if( !pInstr->iP2 ){` |
|        - |  4385 | `		/* Push comparison result without taking the jump */` |
|   240650 |  4386 | `		PH7_MemObjRelease(pTos);` |
|   240650 |  4387 | `		pTos->x.iVal = rc;` |
|        - |  4388 | `		/* Invalidate any prior representation */` |
|   240650 |  4389 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   120326 |  4390 | `	}else{` |
|      ! 0 |  4391 | `		if( rc ){` |
|        - |  4392 | `			/* Jump to the desired location */` |
|      ! 0 |  4393 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4394 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4395 | `		}` |
|        - |  4396 | `	}` |
|   240650 |  4397 | `	break;` |
|        - |  4398 | `				 }` |
|        - |  4399 | `/* OP_TNE P1 P2 *` |
|        - |  4400 | ` *` |
|        - |  4401 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4402 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4403 | ` * instruction.` |
|        - |  4404 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4405 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4406 | ` *` |
|        - |  4407 | ` */` |
|    95653 |  4408 | `case PH7_OP_TNE: {` |
|   191308 |  4409 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4410 | `	/* Perform the comparison and act accordingly */` |
|        - |  4411 | `#ifdef UNTRUST` |
|        - |  4412 | `	if( pNos < pStack ){` |
|        - |  4413 | `		goto Abort;` |
|        - |  4414 | `	}` |
|        - |  4415 | `#endif` |
|   191308 |  4416 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   191308 |  4417 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4418 | `		rc = 1;` |
|        2 |  4419 | `	}else{` |
|   191306 |  4420 | `		rc = rc != 0;` |
|        - |  4421 | `	}` |
|   191308 |  4422 | `	VmPopOperand(&pTos,1);` |
|   191308 |  4423 | `	if( !pInstr->iP2 ){` |
|        - |  4424 | `		/* Push comparison result without taking the jump */` |
|   191308 |  4425 | `		PH7_MemObjRelease(pTos);` |
|   191308 |  4426 | `		pTos->x.iVal = rc;` |
|        - |  4427 | `		/* Invalidate any prior representation */` |
|   191308 |  4428 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    95655 |  4429 | `	}else{` |
|      ! 0 |  4430 | `		if( rc ){` |
|        - |  4431 | `			/* Jump to the desired location */` |
|      ! 0 |  4432 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4433 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4434 | `		}` |
|        - |  4435 | `	}` |
|   191308 |  4436 | `	break;` |
|        - |  4437 | `				 }` |
|        - |  4438 | `/* OP_LT P1 P2 P3` |
|        - |  4439 | ` *` |
|        - |  4440 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4441 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4442 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4443 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4444 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4445 | ` *` |
|        - |  4446 | ` */` |
|        - |  4447 | `/* OP_LE P1 P2 P3` |
|        - |  4448 | ` *` |
|        - |  4449 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4450 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4451 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4452 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4453 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4454 | ` *` |
|        - |  4455 | ` */` |
|   109581 |  4456 | `case PH7_OP_LT:` |
|        - |  4457 | `case PH7_OP_LE: {` |
|   219208 |  4458 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4459 | `	/* Perform the comparison and act accordingly */` |
|        - |  4460 | `#ifdef UNTRUST` |
|        - |  4461 | `	if( pNos < pStack ){` |
|        - |  4462 | `		goto Abort;` |
|        - |  4463 | `	}` |
|        - |  4464 | `#endif` |
|   219208 |  4465 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   219208 |  4466 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4467 | `		rc = 0;` |
|   219204 |  4468 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      395 |  4469 | `		rc = rc < 1;` |
|      198 |  4470 | `	}else{` |
|   218806 |  4471 | `		rc = rc < 0;` |
|        - |  4472 | `	}` |
|   219208 |  4473 | `	VmPopOperand(&pTos,1);` |
|   219208 |  4474 | `	if( !pInstr->iP2 ){` |
|        - |  4475 | `		/* Push comparison result without taking the jump */` |
|   219208 |  4476 | `		PH7_MemObjRelease(pTos);` |
|   219208 |  4477 | `		pTos->x.iVal = rc;` |
|        - |  4478 | `		/* Invalidate any prior representation */` |
|   219208 |  4479 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   109627 |  4480 | `	}else{` |
|      ! 0 |  4481 | `		if( rc ){` |
|        - |  4482 | `			/* Jump to the desired location */` |
|      ! 0 |  4483 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4484 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4485 | `		}` |
|        - |  4486 | `	}` |
|   219208 |  4487 | `	break;` |
|        - |  4488 | `				}` |
|        - |  4489 | `/* OP_GT P1 P2 P3` |
|        - |  4490 | ` *` |
|        - |  4491 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4492 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4493 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4494 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4495 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4496 | ` *` |
|        - |  4497 | ` */` |
|        - |  4498 | `/* OP_GE P1 P2 P3` |
|        - |  4499 | ` *` |
|        - |  4500 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4501 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4502 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4503 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4504 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4505 | ` *` |
|        - |  4506 | ` */` |
|    47195 |  4507 | `case PH7_OP_GT:` |
|        - |  4508 | `case PH7_OP_GE: {` |
|    94392 |  4509 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4510 | `	/* Perform the comparison and act accordingly */` |
|        - |  4511 | `#ifdef UNTRUST` |
|        - |  4512 | `	if( pNos < pStack ){` |
|        - |  4513 | `		goto Abort;` |
|        - |  4514 | `	}` |
|        - |  4515 | `#endif` |
|    94392 |  4516 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    94392 |  4517 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4518 | `		rc = 0;` |
|    94388 |  4519 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    94236 |  4520 | `		rc = rc >= 0;` |
|    47119 |  4521 | `	}else{` |
|      150 |  4522 | `		rc = rc > 0;` |
|        - |  4523 | `	}` |
|    94392 |  4524 | `	VmPopOperand(&pTos,1);` |
|    94392 |  4525 | `	if( !pInstr->iP2 ){` |
|        - |  4526 | `		/* Push comparison result without taking the jump */` |
|    94392 |  4527 | `		PH7_MemObjRelease(pTos);` |
|    94392 |  4528 | `		pTos->x.iVal = rc;` |
|        - |  4529 | `		/* Invalidate any prior representation */` |
|    94392 |  4530 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    47197 |  4531 | `	}else{` |
|      ! 0 |  4532 | `		if( rc ){` |
|        - |  4533 | `			/* Jump to the desired location */` |
|      ! 0 |  4534 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4535 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4536 | `		}` |
|        - |  4537 | `	}` |
|    94392 |  4538 | `	break;` |
|        - |  4539 | `				}` |
|        - |  4540 | `/* OP_SEQ P1 P2 *` |
|        - |  4541 | ` * Strict string comparison.` |
|        - |  4542 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4543 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4544 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4545 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4546 | ` * use PH7_OP_EQ.` |
|        - |  4547 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4548 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4549 | ` */` |
|        - |  4550 | `/* OP_SNE P1 P2 *` |
|        - |  4551 | ` * Strict string comparison.` |
|        - |  4552 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4553 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4554 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4555 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4556 | ` * use PH7_OP_EQ.` |
|        - |  4557 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4558 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4559 | ` */` |
|       18 |  4560 | `case PH7_OP_SEQ:` |
|        - |  4561 | `case PH7_OP_SNE: {` |
|       38 |  4562 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4563 | `	SyString s1,s2;` |
|        - |  4564 | `	/* Perform the comparison and act accordingly */` |
|        - |  4565 | `#ifdef UNTRUST` |
|        - |  4566 | `	if( pNos < pStack ){` |
|        - |  4567 | `		goto Abort;` |
|        - |  4568 | `	}` |
|        - |  4569 | `#endif` |
|        - |  4570 | `	/* Force a string cast */` |
|       38 |  4571 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4572 | `		PH7_MemObjToString(pTos);` |
|        2 |  4573 | `	}` |
|       38 |  4574 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4575 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4576 | `	}` |
|       38 |  4577 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4578 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4579 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4580 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4581 | `		rc = rc != 0;` |
|      ! 0 |  4582 | `	}else{` |
|       38 |  4583 | `		rc = rc == 0;` |
|        - |  4584 | `	}` |
|       38 |  4585 | `	VmPopOperand(&pTos,1);` |
|       38 |  4586 | `	if( !pInstr->iP2 ){` |
|        - |  4587 | `		/* Push comparison result without taking the jump */` |
|       38 |  4588 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4589 | `		pTos->x.iVal = rc;` |
|        - |  4590 | `		/* Invalidate any prior representation */` |
|       38 |  4591 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4592 | `	}else{` |
|      ! 0 |  4593 | `		if( rc ){` |
|        - |  4594 | `			/* Jump to the desired location */` |
|      ! 0 |  4595 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4596 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4597 | `		}` |
|        - |  4598 | `	}` |
|       38 |  4599 | `	break;` |
|        - |  4600 | `				 }` |
|        - |  4601 | `/*` |
|        - |  4602 | ` * OP_LOAD_REF * * *` |
|        - |  4603 | ` * Push the index of a referenced object on the stack.` |
|        - |  4604 | ` */` |
|       57 |  4605 | `case PH7_OP_LOAD_REF: {` |
|        - |  4606 | `	sxu32 nIdx;` |
|        - |  4607 | `#ifdef UNTRUST` |
|        - |  4608 | `	if( pTos < pStack ){` |
|        - |  4609 | `		goto Abort;` |
|        - |  4610 | `	}` |
|        - |  4611 | `#endif` |
|        - |  4612 | `	/* Extract memory object index */` |
|      115 |  4613 | `	nIdx = pTos->nIdx;` |
|      115 |  4614 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4615 | `		/* Nullify the object */` |
|       95 |  4616 | `		PH7_MemObjRelease(pTos);` |
|        - |  4617 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4618 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4619 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4620 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4621 | `	}` |
|      115 |  4622 | `	break;` |
|        - |  4623 | `					  }` |
|        - |  4624 | `/*` |
|        - |  4625 | ` * OP_STORE_REF * * P3` |
|        - |  4626 | ` * Perform an assignment operation by reference.` |
|        - |  4627 | ` */` |
|       14 |  4628 | ` case PH7_OP_STORE_REF: {` |
|       30 |  4629 | `	 SyString sName = { 0 , 0 };` |
|        - |  4630 | `	 VmFrame *pFrameLocal;` |
|        - |  4631 | `	SyHashEntry *pEntry;` |
|        - |  4632 | `	sxu32 nIdx;` |
|        - |  4633 | `#ifdef UNTRUST` |
|        - |  4634 | `	if( pTos < pStack ){` |
|        - |  4635 | `		goto Abort;` |
|        - |  4636 | `	}` |
|        - |  4637 | `#endif` |
|       30 |  4638 | `	if( pInstr->p3 == 0 ){` |
|        - |  4639 | `		char *zName;` |
|        - |  4640 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4641 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4642 | `			/* Force a string cast */` |
|      ! 0 |  4643 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4644 | `		}` |
|      ! 0 |  4645 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4646 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4647 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4648 | `			if( zName ){` |
|      ! 0 |  4649 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4650 | `			}` |
|      ! 0 |  4651 | `		}` |
|      ! 0 |  4652 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4653 | `		pTos--;` |
|      ! 0 |  4654 | `	}else{` |
|       30 |  4655 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4656 | `	}` |
|       30 |  4657 | `	nIdx = pTos->nIdx;` |
|       30 |  4658 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4659 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4660 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4661 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4662 | `		}else{` |
|        - |  4663 | `			ph7_value *pObj;` |
|        - |  4664 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4665 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4666 | `			if( pObj == 0 ){` |
|      ! 0 |  4667 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4668 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4669 | `				goto Abort;` |
|        - |  4670 | `			}` |
|        - |  4671 | `			/* Perform the store operation */` |
|      ! 0 |  4672 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4673 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4674 | `		}` |
|       30 |  4675 | `	}else if( sName.nByte > 0){` |
|       30 |  4676 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4677 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4678 | `		}else{` |
|       30 |  4679 | `			pFrameLocal = pVm->pFrame;` |
|       50 |  4680 | `			while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4681 | `				/* Safely ignore the exception frame */` |
|       21 |  4682 | `				pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4683 | `			}` |
|        - |  4684 | `			/* Query the local frame */` |
|       30 |  4685 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       30 |  4686 | `			if( pEntry ){` |
|      ! 0 |  4687 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4688 | `			}else{` |
|       30 |  4689 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       30 |  4690 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4691 | `					/* Insert in the $GLOBALS array */` |
|       26 |  4692 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       12 |  4693 | `				}` |
|       30 |  4694 | `				if( rc == SXRET_OK ){` |
|       30 |  4695 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       14 |  4696 | `				}` |
|        - |  4697 | `			}` |
|        - |  4698 | `		}` |
|       14 |  4699 | `	}` |
|       30 |  4700 | `	break;` |
|        - |  4701 | `				 }` |
|        - |  4702 | `/*` |
|        - |  4703 | ` * OP_UPLINK P1 * *` |
|        - |  4704 | ` * Link a variable to the top active VM frame.` |
|        - |  4705 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4706 | ` */` |
|       23 |  4707 | `case PH7_OP_UPLINK: {` |
|       47 |  4708 | `	if( pVm->pFrame->pParent ){` |
|       47 |  4709 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4710 | `		SyString sName;` |
|        - |  4711 | `		/* Perform the link */` |
|       95 |  4712 | `		while( pLink <= pTos ){` |
|       49 |  4713 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4714 | `				/* Force a string cast */` |
|      ! 0 |  4715 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4716 | `			}` |
|       49 |  4717 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       49 |  4718 | `			if( sName.nByte > 0 ){` |
|       49 |  4719 | `				VmFrameLink(&(*pVm),&sName);` |
|       24 |  4720 | `			}` |
|       49 |  4721 | `			pLink++;` |
|        1 |  4722 | `		}` |
|       23 |  4723 | `	}` |
|       47 |  4724 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       47 |  4725 | `	break;` |
|        - |  4726 | `					}` |
|        - |  4727 | `/*` |
|        - |  4728 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4729 | ` * Push an exception in the corresponding container so that` |
|        - |  4730 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4731 | ` */` |
|       10 |  4732 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       22 |  4733 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4734 | `	VmFrame *pFrameLocal;` |
|       22 |  4735 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4736 | `	/* Create the exception frame */` |
|       22 |  4737 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       22 |  4738 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4739 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4740 | `		goto Abort;` |
|        - |  4741 | `	}` |
|        - |  4742 | `	/* Mark the special frame */` |
|       22 |  4743 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       22 |  4744 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4745 | `	/* Point to the frame that trigger the exception */` |
|       22 |  4746 | `	pFrameLocal = pFrameLocal->pParent;` |
|       34 |  4747 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|       13 |  4748 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4749 | `	}` |
|       22 |  4750 | `	pException->pFrame = pFrameLocal;` |
|       22 |  4751 | `	break;` |
|        - |  4752 | `							}` |
|        - |  4753 | `/*` |
|        - |  4754 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4755 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4756 | ` */` |
|        9 |  4757 | `case PH7_OP_POP_EXCEPTION: {` |
|       20 |  4758 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       20 |  4759 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4760 | `		ph7_exception **apException;` |
|        - |  4761 | `		/* Pop the loaded exception */` |
|        7 |  4762 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        7 |  4763 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|        7 |  4764 | `			(void)SySetPop(&pVm->aException);` |
|        3 |  4765 | `		}` |
|        3 |  4766 | `	}` |
|       20 |  4767 | `	pException->pFrame = 0;` |
|        - |  4768 | `	/* Leave the exception frame */` |
|       20 |  4769 | `	VmLeaveFrame(&(*pVm));` |
|       20 |  4770 | `	break;` |
|        - |  4771 | `							}` |
|        - |  4772 |  |
|        - |  4773 | `/*` |
|        - |  4774 | ` * OP_THROW * P2 *` |
|        - |  4775 | ` * Throw an user exception.` |
|        - |  4776 | ` */` |
|       11 |  4777 | `case PH7_OP_THROW: {` |
|       24 |  4778 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       24 |  4779 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4780 | `#ifdef UNTRUST` |
|        - |  4781 | `	if( pTos < pStack ){` |
|        - |  4782 | `		goto Abort;` |
|        - |  4783 | `	}` |
|        - |  4784 | `#endif` |
|       30 |  4785 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4786 | `		/* Safely ignore the exception frame */` |
|        8 |  4787 | `		pFrameLocal = pFrameLocal->pParent;` |
|        2 |  4788 | `	}` |
|        - |  4789 | `	/* Tell the upper layer that an exception was thrown */` |
|       24 |  4790 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       24 |  4791 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       24 |  4792 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4793 | `		ph7_class *pException;` |
|        - |  4794 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4795 | `		 */` |
|       24 |  4796 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       24 |  4797 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4798 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4799 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4800 | `			if( rc == SXERR_ABORT ){` |
|        - |  4801 | `				/* Abort processing immediately */` |
|      ! 0 |  4802 | `				goto Abort;` |
|        - |  4803 | `			}` |
|      ! 0 |  4804 | `		}else{` |
|        - |  4805 | `			/* Throw the exception */` |
|       24 |  4806 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       24 |  4807 | `			if( rc == SXERR_ABORT ){` |
|        - |  4808 | `				/* Abort processing immediately */` |
|        9 |  4809 | `				goto Abort;` |
|        - |  4810 | `			}` |
|        - |  4811 | `		}` |
|        9 |  4812 | `	}else{` |
|        - |  4813 | `		/* Expecting a class instance */` |
|      ! 0 |  4814 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  4815 | `		if( rc == SXERR_ABORT ){` |
|        - |  4816 | `			/* Abort processing immediately */` |
|      ! 0 |  4817 | `			goto Abort;` |
|        - |  4818 | `		}` |
|        - |  4819 | `	}` |
|        - |  4820 | `	/* Pop the top entry */` |
|       16 |  4821 | `	VmPopOperand(&pTos,1);` |
|        - |  4822 | `	/* Perform an unconditional jump */` |
|       16 |  4823 | `	pc = nJump - 1;` |
|       16 |  4824 | `	break;` |
|        - |  4825 | `				   }` |
|        - |  4826 | `/*` |
|        - |  4827 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  4828 | ` * Prepare a foreach step.` |
|        - |  4829 | ` */` |
|     4415 |  4830 | `case PH7_OP_FOREACH_INIT: {` |
|     8832 |  4831 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4832 | `	void *pName;` |
|        - |  4833 | `#ifdef UNTRUST` |
|        - |  4834 | `	if( pTos < pStack ){` |
|        - |  4835 | `		goto Abort;` |
|        - |  4836 | `	}` |
|        - |  4837 | `#endif` |
|     8832 |  4838 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4839 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  4840 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4841 | `			/* Force a string cast */` |
|      ! 0 |  4842 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4843 | `		}` |
|        - |  4844 | `		/* Duplicate name */` |
|      ! 0 |  4845 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4846 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4847 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4848 | `		}` |
|      ! 0 |  4849 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4850 | `	}` |
|     8832 |  4851 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  4852 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4853 | `			/* Force a string cast */` |
|      ! 0 |  4854 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4855 | `		}` |
|        - |  4856 | `		/* Duplicate name */` |
|      ! 0 |  4857 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4858 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4859 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4860 | `		}` |
|      ! 0 |  4861 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4862 | `	}` |
|        - |  4863 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     8832 |  4864 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4865 | `		/* Jump out of the loop */` |
|      ! 0 |  4866 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4867 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4868 | `		}` |
|      ! 0 |  4869 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4870 | `	}else{` |
|        - |  4871 | `		ph7_foreach_step *pStep;` |
|     8832 |  4872 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     8832 |  4873 | `		if( pStep == 0 ){` |
|      ! 0 |  4874 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4875 | `			/* Jump out of the loop */` |
|      ! 0 |  4876 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4877 | `		}else{` |
|        - |  4878 | `			/* Zero the structure */` |
|     8832 |  4879 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4880 | `			/* Prepare the step */` |
|     8832 |  4881 | `			pStep->iFlags = pInfo->iFlags;` |
|     8832 |  4882 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     8824 |  4883 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4884 | `				/* Reset the internal loop cursor */` |
|     8824 |  4885 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4886 | `				/* Mark the step */` |
|     8824 |  4887 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     8824 |  4888 | `				pStep->xIter.pMap = pMap;` |
|     8824 |  4889 | `				pMap->iRef++;` |
|     4413 |  4890 | `			}else{` |
|        9 |  4891 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4892 | `				/* Reset the loop cursor */` |
|        9 |  4893 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|        - |  4894 | `				/* Mark the step */` |
|        9 |  4895 | `				pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  4896 | `				pStep->xIter.pThis = pThis;` |
|        9 |  4897 | `				pThis->iRef++;` |
|        - |  4898 | `			}` |
|        - |  4899 | `		}` |
|     8832 |  4900 | `		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  4901 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  4902 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  4903 | `			/* Jump out of the loop */` |
|      ! 0 |  4904 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4905 | `		}` |
|        - |  4906 | `	}` |
|     8832 |  4907 | `	VmPopOperand(&pTos,1);` |
|     8832 |  4908 | `	break;` |
|        - |  4909 | `						  }` |
|        - |  4910 | `/*` |
|        - |  4911 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  4912 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  4913 | ` */` |
|    71228 |  4914 | `case PH7_OP_FOREACH_STEP: {` |
|   142458 |  4915 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4916 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  4917 | `	ph7_value *pValue;` |
|        - |  4918 | `	VmFrame *pFrameLocal;` |
|        - |  4919 | `	/* Peek the last step */` |
|   142458 |  4920 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   142458 |  4921 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   142458 |  4922 | `	pFrameLocal = pVm->pFrame;` |
|   147490 |  4923 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4924 | `		/* Safely ignore the exception frame */` |
|     5033 |  4925 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4926 | `	}` |
|   142458 |  4927 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   142434 |  4928 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  4929 | `		ph7_hashmap_node *pNode;` |
|        - |  4930 | `		/* Extract the current node value */` |
|   142434 |  4931 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   142434 |  4932 | `		if( pNode == 0 ){` |
|        - |  4933 | `			/* No more entry to process */` |
|     8824 |  4934 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     8824 |  4935 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4936 | `				/* Break the reference with the last element */` |
|        5 |  4937 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  4938 | `			}` |
|        - |  4939 | `			/* Automatically reset the loop cursor */` |
|     8824 |  4940 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4941 | `			/* Cleanup the mess left behind */` |
|     8824 |  4942 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     8824 |  4943 | `			SySetPop(&pInfo->aStep);` |
|     8824 |  4944 | `			PH7_HashmapUnref(pMap);` |
|     4413 |  4945 | `		}else{` |
|   133612 |  4946 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      292 |  4947 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      292 |  4948 | `				if( pKey ){` |
|      292 |  4949 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      145 |  4950 | `				}` |
|      145 |  4951 | `			}` |
|   133612 |  4952 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4953 | `				SyHashEntry *pEntry;` |
|        - |  4954 | `				/* Pass by reference */` |
|       13 |  4955 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       13 |  4956 | `				if( pEntry ){` |
|       13 |  4957 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        7 |  4958 | `				}else{` |
|      ! 0 |  4959 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  4960 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  4961 | `				}` |
|        7 |  4962 | `			}else{` |
|        - |  4963 | `				/* Make a copy of the entry value */` |
|   133600 |  4964 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   133600 |  4965 | `				if( pValue ){` |
|   133600 |  4966 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    66799 |  4967 | `				}` |
|        - |  4968 | `			}` |
|        - |  4969 | `		}` |
|    71218 |  4970 | `	}else{` |
|       25 |  4971 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  4972 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  4973 | `		SyHashEntry *pEntry;` |
|        - |  4974 | `		/* Point to the next attribute */` |
|       29 |  4975 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  4976 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  4977 | `			/* Check access permission */` |
|       31 |  4978 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  4979 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  4980 | `					break; /* Access is granted */` |
|        - |  4981 | `			}` |
|        1 |  4982 | `		}` |
|       25 |  4983 | `		if( pEntry == 0 ){` |
|        - |  4984 | `			/* Clean up the mess left behind */` |
|        9 |  4985 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  4986 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4987 | `				/* Break the reference with the last element */` |
|        3 |  4988 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  4989 | `			}` |
|        9 |  4990 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  4991 | `			SySetPop(&pInfo->aStep);` |
|        9 |  4992 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  4993 | `		}else{` |
|       17 |  4994 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  4995 | `			ph7_value *pAttrValue;` |
|       17 |  4996 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  4997 | `				/* Fill with the current attribute name */` |
|       17 |  4998 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  4999 | `				if( pKey ){` |
|       17 |  5000 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5001 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5002 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5003 | `				}` |
|        8 |  5004 | `			}` |
|        - |  5005 | `			/* Extract attribute value */` |
|       17 |  5006 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5007 | `			if( pAttrValue ){` |
|       17 |  5008 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5009 | `					/* Pass by reference */` |
|        3 |  5010 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5011 | `					if( pEntry ){` |
|        3 |  5012 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5013 | `					}else{` |
|      ! 0 |  5014 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5015 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5016 | `					}` |
|        2 |  5017 | `				}else{` |
|        - |  5018 | `					/* Make a copy of the attribute value */` |
|       15 |  5019 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5020 | `					if( pValue ){` |
|       15 |  5021 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5022 | `					}` |
|        - |  5023 | `				}` |
|        8 |  5024 | `			}` |
|        - |  5025 | `		}` |
|        - |  5026 | `	}` |
|   142458 |  5027 | `	break;` |
|        - |  5028 | `						  }` |
|        - |  5029 | `/*` |
|        - |  5030 | ` * OP_MEMBER P1 P2` |
|        - |  5031 | ` * Load class attribute/method on the stack.` |
|        - |  5032 | ` */` |
|     1494 |  5033 | `case PH7_OP_MEMBER: {` |
|        - |  5034 | `	ph7_class_instance *pThis;` |
|        - |  5035 | `	ph7_value *pNos;` |
|        - |  5036 | `	SyString sName;` |
|     2990 |  5037 | `	if( !pInstr->iP1 ){` |
|     2932 |  5038 | `		pNos = &pTos[-1];` |
|        - |  5039 | `#ifdef UNTRUST` |
|        - |  5040 | `		if( pNos < pStack ){` |
|        - |  5041 | `			goto Abort;` |
|        - |  5042 | `		}` |
|        - |  5043 | `#endif` |
|     2932 |  5044 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5045 | `			ph7_class *pClass;` |
|        - |  5046 | `			/* Class already instantiated */` |
|     2932 |  5047 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5048 | `			/* Point to the instantiated class */` |
|     2932 |  5049 | `			pClass = pThis->pClass;` |
|        - |  5050 | `			/* Extract attribute name first */` |
|     2932 |  5051 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     2932 |  5052 | `			if( pInstr->iP2 ){` |
|        - |  5053 | `				/* Method call */` |
|      120 |  5054 | `				ph7_class_method *pMeth = 0;` |
|      120 |  5055 | `				if( sName.nByte > 0 ){` |
|        - |  5056 | `					/* Extract the target method */` |
|      120 |  5057 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       59 |  5058 | `				}` |
|      120 |  5059 | `				if( pMeth == 0 ){` |
|      ! 0 |  5060 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5061 | `						&pClass->sName,&sName` |
|        - |  5062 | `						);` |
|        - |  5063 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5064 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5065 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5066 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5067 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5068 | `				}else{` |
|        - |  5069 | `					/* Push method name on the stack */` |
|      120 |  5070 | `					PH7_MemObjRelease(pTos);` |
|      120 |  5071 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      120 |  5072 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5073 | `				}` |
|      120 |  5074 | `				pTos->nIdx = SXU32_HIGH;` |
|       61 |  5075 | `			}else{` |
|        - |  5076 | `				/* Attribute access */` |
|     2814 |  5077 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5078 | `				SyHashEntry *pEntry;` |
|        - |  5079 | `				/* Extract the target attribute */` |
|     2814 |  5080 | `				if( sName.nByte > 0 ){` |
|     2814 |  5081 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     2814 |  5082 | `					if( pEntry ){` |
|        - |  5083 | `						/* Point to the attribute value */` |
|     2812 |  5084 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1405 |  5085 | `					}` |
|     1406 |  5086 | `				}` |
|     2814 |  5087 | `				if( pObjAttr == 0 ){` |
|        - |  5088 | `					/* No such attribute,load null */` |
|        4 |  5089 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5090 | `						&pClass->sName,&sName);` |
|        - |  5091 | `					/* Call the __get magic method if available */` |
|        3 |  5092 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5093 | `				}` |
|     2814 |  5094 | `				VmPopOperand(&pTos,1);` |
|        - |  5095 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5096 | `				 * This is due to the following case:` |
|        - |  5097 | `				 *     (new TestClass())->foo;` |
|        - |  5098 | `				 */` |
|     2814 |  5099 | `				pThis->iRef++;` |
|     2814 |  5100 | `				PH7_MemObjRelease(pTos);` |
|     2814 |  5101 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     2814 |  5102 | `				if( pObjAttr ){` |
|     2812 |  5103 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5104 | `					/* Check attribute access */` |
|     2812 |  5105 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5106 | `						/* Load attribute */` |
|     2812 |  5107 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     2812 |  5108 | `						if( pValue ){` |
|     2812 |  5109 | `							if( pThis->iRef < 2 ){` |
|        - |  5110 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5111 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5112 | `								 */` |
|        3 |  5113 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5114 | `							}else{` |
|        - |  5115 | `								/* Simple load */` |
|     2810 |  5116 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5117 | `							}` |
|     2812 |  5118 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     2810 |  5119 | `								if( pThis->iRef > 1 ){` |
|        - |  5120 | `									/* Load attribute index */` |
|     2808 |  5121 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1403 |  5122 | `								}` |
|     1404 |  5123 | `							}` |
|     1405 |  5124 | `						}` |
|     1405 |  5125 | `					}` |
|     1405 |  5126 | `				}` |
|        - |  5127 | `				/* Safely unreference the object */` |
|     2814 |  5128 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5129 | `			}` |
|     1467 |  5130 | `		}else{` |
|      ! 0 |  5131 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5132 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5133 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5134 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5135 | `		}` |
|     1467 |  5136 | `	}else{` |
|        - |  5137 | `		/* Static member access using class name */` |
|       59 |  5138 | `		pNos = pTos;` |
|       59 |  5139 | `		pThis = 0;` |
|       59 |  5140 | `		if( !pInstr->p3 ){` |
|       57 |  5141 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       57 |  5142 | `			pNos--;` |
|        - |  5143 | `#ifdef UNTRUST` |
|        - |  5144 | `			if( pNos < pStack ){` |
|        - |  5145 | `				goto Abort;` |
|        - |  5146 | `			}` |
|        - |  5147 | `#endif` |
|       29 |  5148 | `		}else{` |
|        - |  5149 | `			/* Attribute name already computed */` |
|        3 |  5150 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5151 | `		}` |
|       59 |  5152 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|       59 |  5153 | `			ph7_class *pClass = 0;` |
|       59 |  5154 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5155 | `				/* Class already instantiated */` |
|      ! 0 |  5156 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5157 | `				pClass = pThis->pClass;` |
|      ! 0 |  5158 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5159 | `			}else{` |
|        - |  5160 | `				/* Try to extract the target class */` |
|       59 |  5161 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       88 |  5162 | `					pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pNos->sBlob),` |
|       29 |  5163 | `						SyBlobLength(&pNos->sBlob),FALSE,0);` |
|       29 |  5164 | `				}` |
|        - |  5165 | `			}` |
|       59 |  5166 | `			if( pClass == 0 ){` |
|        - |  5167 | `				/* Undefined class */` |
|      ! 0 |  5168 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5169 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5170 | `					);` |
|      ! 0 |  5171 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5172 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5173 | `				}` |
|      ! 0 |  5174 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5175 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5176 | `			}else{` |
|       59 |  5177 | `				if( pInstr->iP2 ){` |
|        - |  5178 | `					/* Method call */` |
|       25 |  5179 | `					ph7_class_method *pMeth = 0;` |
|       25 |  5180 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5181 | `						/* Extract the target method */` |
|       25 |  5182 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       12 |  5183 | `					}` |
|       25 |  5184 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5185 | `						if( pMeth ){` |
|      ! 0 |  5186 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5187 | `								&pClass->sName,&sName` |
|        - |  5188 | `								);` |
|      ! 0 |  5189 | `						}else{` |
|      ! 0 |  5190 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5191 | `								&pClass->sName,&sName` |
|        - |  5192 | `								);` |
|        - |  5193 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5194 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5195 | `						}` |
|        - |  5196 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5197 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5198 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5199 | `						}` |
|      ! 0 |  5200 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5201 | `					}else{` |
|        - |  5202 | `						/* Push method name on the stack */` |
|       25 |  5203 | `						PH7_MemObjRelease(pTos);` |
|       25 |  5204 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       25 |  5205 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5206 | `					}` |
|       25 |  5207 | `					pTos->nIdx = SXU32_HIGH;` |
|       13 |  5208 | `				}else{` |
|        - |  5209 | `					/* Attribute access */` |
|       35 |  5210 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5211 | `					/* Check for special ::class pseudo-constant */` |
|       49 |  5212 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       28 |  5213 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5214 | `						/* ::class returns the fully qualified class name */` |
|        - |  5215 | `						/* Pop the attribute name from the stack */` |
|       27 |  5216 | `						if( !pInstr->p3 ){` |
|       27 |  5217 | `							VmPopOperand(&pTos,1);` |
|       13 |  5218 | `						}` |
|       27 |  5219 | `						PH7_MemObjRelease(pTos);` |
|        - |  5220 | `						/* Load the class name */` |
|       27 |  5221 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       27 |  5222 | `						pTos->nIdx = SXU32_HIGH;` |
|       14 |  5223 | `					}else{` |
|        - |  5224 | `						/* Extract the target attribute */` |
|        9 |  5225 | `						if( sName.nByte > 0 ){` |
|        9 |  5226 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        4 |  5227 | `						}` |
|        9 |  5228 | `						if( pAttr == 0 ){` |
|        - |  5229 | `							/* No such attribute,load null */` |
|      ! 0 |  5230 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5231 | `								&pClass->sName,&sName);` |
|        - |  5232 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5233 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5234 | `						}` |
|        - |  5235 | `						/* Pop the attribute name from the stack */` |
|        9 |  5236 | `						if( !pInstr->p3 ){` |
|        7 |  5237 | `							VmPopOperand(&pTos,1);` |
|        3 |  5238 | `						}` |
|        9 |  5239 | `						PH7_MemObjRelease(pTos);` |
|        9 |  5240 | `						pTos->nIdx = SXU32_HIGH;` |
|        9 |  5241 | `						if( pAttr ){` |
|        9 |  5242 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5243 | `								/* Access to a non static attribute */` |
|      ! 0 |  5244 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5245 | `									&pClass->sName,&pAttr->sName` |
|        - |  5246 | `									);` |
|      ! 0 |  5247 | `							}else{` |
|        - |  5248 | `								ph7_value *pValue;` |
|        - |  5249 | `								/* Check if the access to the attribute is allowed */` |
|        9 |  5250 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5251 | `									/* Load the desired attribute */` |
|        9 |  5252 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|        9 |  5253 | `									if( pValue ){` |
|        9 |  5254 | `										PH7_MemObjLoad(pValue,pTos);` |
|        9 |  5255 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5256 | `											/* Load index number */` |
|        3 |  5257 | `											pTos->nIdx = pAttr->nIdx;` |
|        1 |  5258 | `										}` |
|        4 |  5259 | `									}` |
|        4 |  5260 | `								}` |
|        - |  5261 | `							}` |
|        4 |  5262 | `						}` |
|        - |  5263 | `					}` |
|        - |  5264 | `				}` |
|       59 |  5265 | `				if( pThis ){` |
|        - |  5266 | `					/* Safely unreference the object */` |
|      ! 0 |  5267 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5268 | `				}` |
|        - |  5269 | `			}` |
|       30 |  5270 | `		}else{` |
|        - |  5271 | `			/* Pop operands */` |
|      ! 0 |  5272 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5273 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5274 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5275 | `			}` |
|      ! 0 |  5276 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5277 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5278 | `		}` |
|        - |  5279 | `	}` |
|     2990 |  5280 | `	break;` |
|        - |  5281 | `					}` |
|        - |  5282 | `/*` |
|        - |  5283 | ` * OP_NEW P1 * * *` |
|        - |  5284 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5285 | ` */` |
|      253 |  5286 | `case PH7_OP_NEW: {` |
|      508 |  5287 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      508 |  5288 | `	ph7_class *pClass = 0;` |
|        - |  5289 | `	ph7_class_instance *pNew;` |
|      508 |  5290 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5291 | `		/* Try to extract the desired class */` |
|      761 |  5292 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      506 |  5293 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      253 |  5294 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5295 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5296 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5297 | `	}` |
|      508 |  5298 | `	if( pClass == 0 ){` |
|        - |  5299 | `		/* No such class */` |
|      ! 0 |  5300 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined,PH7 is loading NULL",` |
|      ! 0 |  5301 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5302 | `			);` |
|      ! 0 |  5303 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5304 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5305 | `			/* Pop given arguments */` |
|      ! 0 |  5306 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5307 | `		}` |
|      ! 0 |  5308 | `	}else{` |
|        - |  5309 | `		ph7_class_method *pCons;` |
|        - |  5310 | `		/* Create a new class instance */` |
|      508 |  5311 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      508 |  5312 | `		if( pNew == 0 ){` |
|      ! 0 |  5313 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5314 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5315 | `				&pClass->sName` |
|        - |  5316 | `			);` |
|      ! 0 |  5317 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5318 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5319 | `				/* Pop given arguments */` |
|      ! 0 |  5320 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5321 | `			}` |
|      ! 0 |  5322 | `			break;` |
|        - |  5323 | `		}` |
|        - |  5324 | `		/* Check if a constructor is available */` |
|      508 |  5325 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      508 |  5326 | `		if( pCons == 0 ){` |
|      450 |  5327 | `			SyString *pName = &pClass->sName;` |
|        - |  5328 | `			/* Check for a constructor with the same base class name */` |
|      450 |  5329 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      224 |  5330 | `		}` |
|      508 |  5331 | `		if( pCons ){` |
|        - |  5332 | `			/* Call the class constructor */` |
|       60 |  5333 | `			SySetReset(&aArg);` |
|      108 |  5334 | `			while( pArg < pTos ){` |
|       50 |  5335 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       50 |  5336 | `				pArg++;` |
|        2 |  5337 | `			}` |
|       60 |  5338 | `			if( pVm->bErrReport ){` |
|        - |  5339 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5340 | `				sxu32 n;` |
|       17 |  5341 | `				n = SySetUsed(&aArg);` |
|        - |  5342 | `				/* Emit a notice for missing arguments */` |
|       45 |  5343 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       29 |  5344 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       29 |  5345 | `					if( pFuncArg ){` |
|       29 |  5346 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5347 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5348 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5349 | `						}` |
|       14 |  5350 | `					}` |
|       29 |  5351 | `					n++;` |
|        1 |  5352 | `				}` |
|        8 |  5353 | `			}` |
|       60 |  5354 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5355 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       60 |  5356 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5357 | `				pNew->iRef = 1;` |
|      ! 0 |  5358 | `			}` |
|       29 |  5359 | `		}` |
|      508 |  5360 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5361 | `			/* Pop given arguments */` |
|       44 |  5362 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       21 |  5363 | `		}` |
|      508 |  5364 | `		PH7_MemObjRelease(pTos);` |
|      508 |  5365 | `		pTos->x.pOther = pNew;` |
|      508 |  5366 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5367 | `	}` |
|      508 |  5368 | `	break;` |
|        - |  5369 | `				 }` |
|        - |  5370 | `/*` |
|        - |  5371 | ` * OP_CLONE * * *` |
|        - |  5372 | ` * Perfome a clone operation.` |
|        - |  5373 | ` */` |
|       23 |  5374 | `case PH7_OP_CLONE: {` |
|        - |  5375 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5376 | `#ifdef UNTRUST` |
|        - |  5377 | `	if( pTos < pStack ){` |
|        - |  5378 | `		goto Abort;` |
|        - |  5379 | `	}` |
|        - |  5380 | `#endif` |
|        - |  5381 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5382 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5383 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5384 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5385 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5386 | `		break;` |
|        - |  5387 | `	}` |
|        - |  5388 | `	/* Point to the source */` |
|       44 |  5389 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5390 | `	/* Perform the clone operation */` |
|       44 |  5391 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5392 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5393 | `	if( pClone == 0 ){` |
|      ! 0 |  5394 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5395 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5396 | `	}else{` |
|        - |  5397 | `		/* Load the cloned object */` |
|       44 |  5398 | `		pTos->x.pOther = pClone;` |
|       44 |  5399 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5400 | `	}` |
|       44 |  5401 | `	break;` |
|        - |  5402 | `				   }` |
|        - |  5403 | `/*` |
|        - |  5404 | ` * OP_SWITCH * * P3` |
|        - |  5405 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5406 | ` */` |
|       18 |  5407 | `case PH7_OP_SWITCH: {` |
|       38 |  5408 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5409 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5410 | `	ph7_value sValue,sCaseValue;` |
|        - |  5411 | `	sxu32 n,nEntry;` |
|        - |  5412 | `#ifdef UNTRUST` |
|        - |  5413 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5414 | `		goto Abort;` |
|        - |  5415 | `	}` |
|        - |  5416 | `#endif` |
|        - |  5417 | `	/* Point to the case table  */` |
|       38 |  5418 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5419 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5420 | `	/* Select the appropriate case block to execute */` |
|       38 |  5421 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5422 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5423 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5424 | `		pCase = &aCase[n];` |
|       92 |  5425 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5426 | `		/* Execute the case expression first */` |
|       92 |  5427 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5428 | `		/* Compare the two expression */` |
|       92 |  5429 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5430 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5431 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5432 | `		if( rc == 0 ){` |
|        - |  5433 | `			/* Value match,jump to this block */` |
|       38 |  5434 | `			pc = pCase->nStart - 1;` |
|       38 |  5435 | `			break;` |
|        - |  5436 | `		}` |
|       29 |  5437 | `	}` |
|       38 |  5438 | `	VmPopOperand(&pTos,1);` |
|       38 |  5439 | `	if( n >= nEntry ){` |
|        - |  5440 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5441 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5442 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5443 | `		}else{` |
|        - |  5444 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5445 | `			pc = pSwitch->nOut - 1;` |
|        - |  5446 | `		}` |
|      ! 0 |  5447 | `	}` |
|       38 |  5448 | `	break;` |
|        - |  5449 | `					}` |
|        - |  5450 | `/*` |
|        - |  5451 | ` * OP_CALL P1 * *` |
|        - |  5452 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5453 | ` *  function on the stack.` |
|        - |  5454 | ` */` |
|   270222 |  5455 | `case PH7_OP_CALL: {` |
|   540490 |  5456 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5457 | `	SyHashEntry *pEntry;` |
|        - |  5458 | `	SyString sName;` |
|        - |  5459 | `	/* Extract function name */` |
|   540490 |  5460 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5461 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5462 | `			ph7_value sResult;` |
|      ! 0 |  5463 | `			SySetReset(&aArg);` |
|      ! 0 |  5464 | `			while( pArg < pTos ){` |
|      ! 0 |  5465 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5466 | `				pArg++;` |
|      ! 0 |  5467 | `			}` |
|      ! 0 |  5468 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5469 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5470 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5471 | `			SySetReset(&aArg);` |
|        - |  5472 | `			/* Pop given arguments */` |
|      ! 0 |  5473 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5474 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5475 | `			}` |
|        - |  5476 | `			/* Copy result */` |
|      ! 0 |  5477 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5478 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5479 | `		}else{` |
|        3 |  5480 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5481 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5482 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5483 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5484 | `			}else{` |
|        - |  5485 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5486 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5487 | `			}` |
|        - |  5488 | `			/* Pop given arguments */` |
|        3 |  5489 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5490 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5491 | `			}` |
|        - |  5492 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5493 | `			PH7_MemObjRelease(pTos);` |
|        - |  5494 | `		}` |
|   270042 |  5495 | `		break;` |
|        - |  5496 | `	}` |
|   540488 |  5497 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5498 | `	/* Check for a compiled function first */` |
|   540488 |  5499 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|   540488 |  5500 | `	if( pEntry ){` |
|        - |  5501 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5502 | `		ph7_class_instance *pThis;` |
|        - |  5503 | `		ph7_value *pFrameStack;` |
|        - |  5504 | `		ph7_vm_func *pVmFunc;` |
|        - |  5505 | `		ph7_class *pSelf;` |
|        - |  5506 | `		VmFrame *pFrame;` |
|        - |  5507 | `		ph7_value *pObj;` |
|        - |  5508 | `		VmSlot sArg;` |
|        - |  5509 | `		sxu32 n;` |
|        - |  5510 | `		/* initialize fields */` |
|    10712 |  5511 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    10712 |  5512 | `		pThis = 0;` |
|    10712 |  5513 | `		pSelf = 0;` |
|    10712 |  5514 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5515 | `			ph7_class_method *pMeth;` |
|        - |  5516 | `			/* Class method call */` |
|     1066 |  5517 | `			ph7_value *pTarget = &pTos[-1];` |
|     1066 |  5518 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5519 | `				/* Extract the 'this' pointer */` |
|     1066 |  5520 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5521 | `					/* Instance already loaded */` |
|     1036 |  5522 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1036 |  5523 | `					pThis->iRef++;` |
|     1036 |  5524 | `					pSelf = pThis->pClass;` |
|      517 |  5525 | `				}` |
|     1066 |  5526 | `				if( pSelf == 0 ){` |
|       31 |  5527 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5528 | `						/* "Late Static Binding" class name */` |
|       37 |  5529 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       12 |  5530 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       12 |  5531 | `					}` |
|       31 |  5532 | `					if( pSelf == 0 ){` |
|        7 |  5533 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 |  5534 | `					}` |
|       15 |  5535 | `				}` |
|     1066 |  5536 | `				if( pThis == 0  ){` |
|       31 |  5537 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       33 |  5538 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5539 | `						/* Safely ignore the exception frame */` |
|        3 |  5540 | `						pFrameLocal = pFrameLocal->pParent;` |
|        1 |  5541 | `					}` |
|       31 |  5542 | `					if( pFrameLocal->pParent ){` |
|        - |  5543 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5544 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5545 | `						if( pThis ){` |
|       13 |  5546 | `							pThis->iRef++;` |
|        6 |  5547 | `						}` |
|        9 |  5548 | `					}` |
|       15 |  5549 | `				}` |
|     1066 |  5550 | `				VmPopOperand(&pTos,1);` |
|     1066 |  5551 | `				PH7_MemObjRelease(pTos);` |
|        - |  5552 | `				/* Synchronize pointers */` |
|     1066 |  5553 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5554 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5555 | `				 * user have already computed the random generated unique class method name` |
|        - |  5556 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5557 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5558 | `				 */` |
|     1066 |  5559 | `				while( pArg < pStack ){` |
|      ! 0 |  5560 | `					pArg++;` |
|      ! 0 |  5561 | `				}` |
|     1066 |  5562 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5563 | `					/* Check if the call is allowed */` |
|     1066 |  5564 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1066 |  5565 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        5 |  5566 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5567 | `							/* Pop given arguments */` |
|      ! 0 |  5568 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5569 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5570 | `							}` |
|        - |  5571 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5572 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5573 | `							break;` |
|        - |  5574 | `						}` |
|        2 |  5575 | `					}` |
|      532 |  5576 | `				}` |
|      532 |  5577 | `			}` |
|      532 |  5578 | `		}` |
|        - |  5579 | `		/* Check The recursion limit */` |
|    10712 |  5580 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5581 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5582 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5583 | `				&pVmFunc->sName);` |
|        - |  5584 | `			/* Pop given arguments */` |
|        3 |  5585 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5586 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5587 | `			}` |
|        - |  5588 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5589 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5590 | `			break;` |
|        - |  5591 | `		}` |
|    10710 |  5592 | `		if( pVmFunc->pNextName ){` |
|        - |  5593 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      123 |  5594 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       61 |  5595 | `		}` |
|        - |  5596 | `		/* Extract the formal argument set */` |
|    10710 |  5597 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5598 | `		/* Create a new VM frame  */` |
|    10710 |  5599 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    10710 |  5600 | `		if( rc != SXRET_OK ){` |
|        - |  5601 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5602 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5603 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5604 | `				&pVmFunc->sName);` |
|        - |  5605 | `			/* Pop given arguments */` |
|      ! 0 |  5606 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5607 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5608 | `			}` |
|        - |  5609 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5610 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5611 | `			break;` |
|        - |  5612 | `		}` |
|    10710 |  5613 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5614 | `			/* Install the '$this' variable */` |
|        - |  5615 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1046 |  5616 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1046 |  5617 | `			if( pObj ){` |
|        - |  5618 | `				/* Reflect the change */` |
|     1046 |  5619 | `				pObj->x.pOther = pThis;` |
|     1046 |  5620 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      522 |  5621 | `			}` |
|      522 |  5622 | `		}` |
|    10710 |  5623 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5624 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5625 | `			/* Install static variables */` |
|      ! 0 |  5626 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5627 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5628 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5629 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5630 | `					/* Initialize the static variables */` |
|      ! 0 |  5631 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5632 | `					if( pObj ){` |
|        - |  5633 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5634 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5635 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5636 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5637 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5638 | `						}` |
|      ! 0 |  5639 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5640 | `					}else{` |
|      ! 0 |  5641 | `						continue;` |
|        - |  5642 | `					}` |
|      ! 0 |  5643 | `				}` |
|        - |  5644 | `				/* Install in the current frame */` |
|      ! 0 |  5645 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5646 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5647 | `			}` |
|      ! 0 |  5648 | `		}` |
|        - |  5649 | `		/* Push arguments in the local frame */` |
|    10710 |  5650 | `		n = 0;` |
|    30126 |  5651 | `		while( pArg < pTos ){` |
|    19418 |  5652 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    19268 |  5653 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5654 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5655 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5656 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5657 | `						goto Abort;` |
|        - |  5658 | `					}` |
|      ! 0 |  5659 | `				}` |
|        - |  5660 | `				/* Make sure the given arguments are of the correct type */` |
|    19268 |  5661 | `				if( aFormalArg[n].nType > 0 ){` |
|     1068 |  5662 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5663 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5664 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5665 | `						ph7_class *pClass;` |
|        - |  5666 | `						/* Try to extract the desired class */` |
|      ! 0 |  5667 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5668 | `						if( pClass ){` |
|      ! 0 |  5669 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5670 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5671 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5672 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5673 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5674 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5675 | `								}` |
|      ! 0 |  5676 | `							}else{` |
|        - |  5677 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5678 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5679 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5680 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5681 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5682 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5683 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5684 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5685 | `								}` |
|        - |  5686 | `							}` |
|      ! 0 |  5687 | `						}` |
|     1068 |  5688 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5689 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5690 | `						/* Cast to the desired type */` |
|      ! 0 |  5691 | `						xCast(pArg);` |
|      ! 0 |  5692 | `					}` |
|      533 |  5693 | `				}` |
|    19268 |  5694 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5695 | `					/* Pass by reference */` |
|       48 |  5696 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5697 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5698 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5699 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5700 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5701 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5702 | `						}` |
|        - |  5703 | `						/* Switch to pass by value */` |
|      ! 0 |  5704 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5705 | `					}else{` |
|        - |  5706 | `						SyHashEntry *pRefEntry;` |
|        - |  5707 | `						/* Install the referenced variable in the private function frame */` |
|       48 |  5708 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       48 |  5709 | `						if( pRefEntry == 0 ){` |
|       71 |  5710 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       46 |  5711 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       48 |  5712 | `							sArg.nIdx = pArg->nIdx;` |
|       48 |  5713 | `							sArg.pUserData = 0;` |
|       48 |  5714 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  5715 | `						}` |
|       48 |  5716 | `						pObj = 0;` |
|        - |  5717 | `					}` |
|       25 |  5718 | `				}else{` |
|        - |  5719 | `					/* Pass by value,make a copy of the given argument */` |
|    19222 |  5720 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5721 | `				}` |
|     9635 |  5722 | `			}else{` |
|        - |  5723 | `				char zName[32];` |
|        - |  5724 | `				SyString sArgName;` |
|        - |  5725 | `				/* Set a dummy name */` |
|      152 |  5726 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      152 |  5727 | `				sArgName.zString = zName;` |
|        - |  5728 | `				/* Annonymous argument */` |
|      152 |  5729 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5730 | `			}` |
|    19418 |  5731 | `			if( pObj ){` |
|    19372 |  5732 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5733 | `				/* Insert argument index  */` |
|    19372 |  5734 | `				sArg.nIdx = pObj->nIdx;` |
|    19372 |  5735 | `				sArg.pUserData = 0;` |
|    19372 |  5736 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|     9685 |  5737 | `			}` |
|    19418 |  5738 | `			PH7_MemObjRelease(pArg);` |
|    19418 |  5739 | `			pArg++;` |
|    19418 |  5740 | `			++n;` |
|        2 |  5741 | `		}` |
|        - |  5742 | `		/* Set up closure environment */` |
|    10710 |  5743 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5744 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  5745 | `			ph7_value *pValue;` |
|        - |  5746 | `			sxu32 iEnv;` |
|        9 |  5747 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  5748 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  5749 | `				pEnv = &aEnv[iEnv];` |
|       17 |  5750 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  5751 | `					/* Do not install null value */` |
|        9 |  5752 | `					continue;` |
|        - |  5753 | `				}` |
|        9 |  5754 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  5755 | `				if( pValue == 0 ){` |
|      ! 0 |  5756 | `					continue;` |
|        - |  5757 | `				}` |
|        - |  5758 | `				/* Invalidate any prior representation */` |
|        9 |  5759 | `				PH7_MemObjRelease(pValue);` |
|        - |  5760 | `				/* Duplicate bound variable value */` |
|        9 |  5761 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  5762 | `			}` |
|        4 |  5763 | `		}` |
|        - |  5764 | `		/* Process default values */` |
|    12300 |  5765 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1592 |  5766 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1586 |  5767 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1586 |  5768 | `				if( pObj ){` |
|        - |  5769 | `					/* Evaluate the default value and extract it's result */` |
|     1586 |  5770 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1586 |  5771 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5772 | `						goto Abort;` |
|        - |  5773 | `					}` |
|        - |  5774 | `					/* Insert argument index */` |
|     1586 |  5775 | `					sArg.nIdx = pObj->nIdx;` |
|     1586 |  5776 | `					sArg.pUserData = 0;` |
|     1586 |  5777 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  5778 | `					/* Make sure the default argument is of the correct type */` |
|     1586 |  5779 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5780 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5781 | `						/* Cast to the desired type */` |
|      ! 0 |  5782 | `						xCast(pObj);` |
|      ! 0 |  5783 | `					}` |
|      792 |  5784 | `				}` |
|      792 |  5785 | `			}` |
|     1592 |  5786 | `			++n;` |
|        2 |  5787 | `		}` |
|        - |  5788 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  5789 | `		 * does not return anything.` |
|        - |  5790 | `		 */` |
|    10710 |  5791 | `		PH7_MemObjRelease(pTos);` |
|    10710 |  5792 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  5793 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    10710 |  5794 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    10710 |  5795 | `		if( pFrameStack == 0 ){` |
|        - |  5796 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5797 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5798 | `				&pVmFunc->sName);` |
|      ! 0 |  5799 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5800 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5801 | `			}` |
|      ! 0 |  5802 | `			break;` |
|        - |  5803 | `		}` |
|    10710 |  5804 | `		if( pSelf ){` |
|        - |  5805 | `			/* Push class name */` |
|     1064 |  5806 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      531 |  5807 | `		}` |
|        - |  5808 | `		/* Increment nesting level */` |
|    10710 |  5809 | `		pVm->nRecursionDepth++;` |
|        - |  5810 | `		/* Execute function body */` |
|    10710 |  5811 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  5812 | `		/* Decrement nesting level */` |
|    10710 |  5813 | `		pVm->nRecursionDepth--;` |
|    10710 |  5814 | `		if( pSelf ){` |
|        - |  5815 | `			/* Pop class name */` |
|     1064 |  5816 | `			(void)SySetPop(&pVm->aSelf);` |
|      531 |  5817 | `		}` |
|        - |  5818 | `		/* Cleanup the mess left behind */` |
|    10710 |  5819 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  5820 | `			/* Return by reference,reflect that */` |
|        9 |  5821 | `			if( n != SXU32_HIGH ){` |
|        9 |  5822 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  5823 | `				sxu32 i;` |
|        - |  5824 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  5825 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  5826 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  5827 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  5828 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5829 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5830 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  5831 | `								&pVmFunc->sName);` |
|      ! 0 |  5832 | `						}` |
|      ! 0 |  5833 | `						n = SXU32_HIGH;` |
|      ! 0 |  5834 | `						break;` |
|        - |  5835 | `					}` |
|        3 |  5836 | `				}` |
|        5 |  5837 | `			}else{` |
|      ! 0 |  5838 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5839 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5840 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  5841 | `						&pVmFunc->sName);` |
|      ! 0 |  5842 | `				}` |
|        - |  5843 | `			}` |
|        9 |  5844 | `			pTos->nIdx = n;` |
|        4 |  5845 | `		}` |
|        - |  5846 | `		/* Cleanup the mess left behind */` |
|    10710 |  5847 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  5848 | `			/* An exception was throw in this frame */` |
|        7 |  5849 | `			pFrame = pFrame->pParent;` |
|        7 |  5850 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  5851 | `				/* Pop the resutlt */` |
|        5 |  5852 | `				VmPopOperand(&pTos,1);` |
|        - |  5853 | `				/* Jump to this destination */` |
|        5 |  5854 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  5855 | `				rc = PH7_OK;` |
|        3 |  5856 | `			}else{` |
|        3 |  5857 | `				if( pFrame->pParent ){` |
|        3 |  5858 | `					rc = PH7_EXCEPTION;` |
|        2 |  5859 | `				}else{` |
|        - |  5860 | `					/* Continue normal execution */` |
|      ! 0 |  5861 | `					rc = PH7_OK;` |
|        - |  5862 | `				}` |
|        - |  5863 | `			}` |
|        3 |  5864 | `		}` |
|        - |  5865 | `		/* Free the operand stack */` |
|    10710 |  5866 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  5867 | `		/* Leave the frame */` |
|    10710 |  5868 | `		VmLeaveFrame(&(*pVm));` |
|    10710 |  5869 | `		if( rc == PH7_ABORT ){` |
|        - |  5870 | `			/* Abort processing immeditaley */` |
|        7 |  5871 | `			goto Abort;` |
|    10704 |  5872 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5873 | `			goto Exception;` |
|        - |  5874 | `		}` |
|     5352 |  5875 | `	}else{` |
|        - |  5876 | `		ph7_user_func *pFunc;` |
|        - |  5877 | `		ph7_context sCtx;` |
|        - |  5878 | `		ph7_value sRet;` |
|        - |  5879 | `		/* Look for an installed foreign function */` |
|   529778 |  5880 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   529778 |  5881 | `		if( pEntry == 0 ){` |
|        - |  5882 | `			/* Call to undefined function */` |
|        5 |  5883 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  5884 | `			/* Pop given arguments */` |
|        5 |  5885 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5886 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5887 | `			}` |
|        - |  5888 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  5889 | `			PH7_MemObjRelease(pTos);` |
|        5 |  5890 | `			break;` |
|        - |  5891 | `		}` |
|   529774 |  5892 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  5893 | `		/* Start collecting function arguments */` |
|   529774 |  5894 | `		SySetReset(&aArg);` |
|  1409688 |  5895 | `		while( pArg < pTos ){` |
|   879916 |  5896 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   879916 |  5897 | `			pArg++;` |
|        2 |  5898 | `		}` |
|        - |  5899 | `		/* Assume a null return value */` |
|   529774 |  5900 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  5901 | `		/* Init the call context */` |
|   529774 |  5902 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  5903 | `		/* Call the foreign function */` |
|   529774 |  5904 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5905 | `		/* Release the call context */` |
|   529774 |  5906 | `		VmReleaseCallContext(&sCtx);` |
|   529774 |  5907 | `		if( rc == PH7_ABORT ){` |
|      355 |  5908 | `			goto Abort;` |
|   529420 |  5909 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5910 | `			goto Exception;` |
|        - |  5911 | `		}` |
|   529418 |  5912 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5913 | `			/* Pop function name and arguments */` |
|   512346 |  5914 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   256194 |  5915 | `		}` |
|        - |  5916 | `		/* Save foreign function return value */` |
|   529418 |  5917 | `		PH7_MemObjStore(&sRet,pTos);` |
|   529418 |  5918 | `		PH7_MemObjRelease(&sRet);` |
|        - |  5919 | `	}` |
|   540118 |  5920 | `	break;` |
|        - |  5921 | `				  }` |
|        - |  5922 | `/*` |
|        - |  5923 | ` * OP_CONSUME: P1 * *` |
|        - |  5924 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  5925 | ` */` |
|     9858 |  5926 | `case PH7_OP_CONSUME: {` |
|    19718 |  5927 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    19718 |  5928 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  5929 |  |
|    19718 |  5930 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    19718 |  5931 | `	pCur = pOut;` |
|        - |  5932 | `	/* Start the consume process  */` |
|    39434 |  5933 | `	while( pOut <= pTos ){` |
|        - |  5934 | `		/* Force a string cast */` |
|    19718 |  5935 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      160 |  5936 | `			PH7_MemObjToString(pOut);` |
|       79 |  5937 | `		}` |
|    19718 |  5938 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  5939 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  5940 | `			/* Invoke the output consumer callback */` |
|    10546 |  5941 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    10546 |  5942 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  5943 | `				/* Increment output length */` |
|     4162 |  5944 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     2080 |  5945 | `			}` |
|    10546 |  5946 | `			SyBlobRelease(&pOut->sBlob);` |
|    10546 |  5947 | `			if( rc == SXERR_ABORT ){` |
|        - |  5948 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  5949 | `				goto Abort;` |
|        - |  5950 | `			}` |
|     5272 |  5951 | `		}` |
|    19718 |  5952 | `		pOut++;` |
|        2 |  5953 | `	}` |
|    19718 |  5954 | `	pTos = &pCur[-1];` |
|    19716 |  5955 | `	break;` |
|        - |  5956 | `					 }` |
|        - |  5957 |  |
|        - |  5958 | `		} /* Switch() */` |
|  9440586 |  5959 | `		pc++; /* Next instruction in the stream */` |
|        2 |  5960 | `	} /* For(;;) */` |
|    13334 |  5961 | `Done:` |
|    26670 |  5962 | `	SySetRelease(&aArg);` |
|    26670 |  5963 | `	return SXRET_OK;` |
|      184 |  5964 | `Abort:` |
|      369 |  5965 | `	SySetRelease(&aArg);` |
|     1279 |  5966 | `	while( pTos >= pStack ){` |
|      911 |  5967 | `		PH7_MemObjRelease(pTos);` |
|      911 |  5968 | `		pTos--;` |
|        1 |  5969 | `	}` |
|      369 |  5970 | `	return PH7_ABORT;` |
|        2 |  5971 | `Exception:` |
|        5 |  5972 | `	SySetRelease(&aArg);` |
|        9 |  5973 | `	while( pTos >= pStack ){` |
|        5 |  5974 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5975 | `		pTos--;` |
|        1 |  5976 | `	}` |
|        5 |  5977 | `	return PH7_EXCEPTION;` |
|    13522 |  5978 |  |
|        - |  5979 | `/*` |
|        - |  5980 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  5981 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  5982 | ` * See block-comment on that function for additional information.` |
|        - |  5983 | ` */` |
|    13310 |  5984 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  5985 |  |
|        - |  5986 | `	ph7_value *pStack;` |
|        - |  5987 | `	sxi32 rc;` |
|        - |  5988 | `	/* Allocate a new operand stack */` |
|    13312 |  5989 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    13312 |  5990 | `	if( pStack == 0 ){` |
|      ! 0 |  5991 | `		return SXERR_MEM;` |
|        - |  5992 | `	}` |
|        - |  5993 | `	/* Execute the program */` |
|    13312 |  5994 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  5995 | `	/* Free the operand stack */` |
|    13312 |  5996 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  5997 | `	/* Execution result */` |
|    13312 |  5998 | `	return rc;` |
|     6657 |  5999 |  |
|        - |  6000 | `/*` |
|        - |  6001 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6002 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6003 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6004 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6005 | ` * execution ends.` |
|        - |  6006 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6007 | ` * additional information.` |
|        - |  6008 | ` */` |
|     1668 |  6009 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6010 |  |
|        - |  6011 | `	VmShutdownCB *pEntry;` |
|        - |  6012 | `	ph7_value *apArg[10];` |
|        - |  6013 | `	sxu32 n,nEntry;` |
|        - |  6014 | `	int i;` |
|        - |  6015 | `	/* Point to the stack of registered callbacks */` |
|     1670 |  6016 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    18350 |  6017 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    16682 |  6018 | `		apArg[i] = 0;` |
|     8342 |  6019 | `	}` |
|     1672 |  6020 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6021 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6022 | `		if( pEntry ){` |
|        - |  6023 | `			/* Prepare callback arguments if any */` |
|        3 |  6024 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6025 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6026 | `					break;` |
|        - |  6027 | `				}` |
|      ! 0 |  6028 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6029 | `			}` |
|        - |  6030 | `			/* Invoke the callback */` |
|        3 |  6031 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6032 | `			/*` |
|        - |  6033 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6034 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6035 | `			 */` |
|        3 |  6036 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6037 | `			if( pEntry ){` |
|        3 |  6038 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6039 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6040 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6041 | `				}` |
|        1 |  6042 | `			}` |
|        1 |  6043 | `		}` |
|        2 |  6044 | `	}` |
|     1670 |  6045 | `	SySetReset(&pVm->aShutdown);` |
|     1670 |  6046 |  |
|        - |  6047 | `/*` |
|        - |  6048 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6049 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6050 | ` * See block-comment on that function for additional information.` |
|        - |  6051 | ` */` |
|     1676 |  6052 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6053 |  |
|        - |  6054 | `	/* Make sure we are ready to execute this program */` |
|     1678 |  6055 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6056 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6057 | `	}` |
|        - |  6058 | `	/* Set the execution magic number  */` |
|     1678 |  6059 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6060 | `	/* Execute the program */` |
|     1678 |  6061 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6062 | `	/* Invoke any shutdown callbacks */` |
|     1674 |  6063 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6064 | `	/*` |
|        - |  6065 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6066 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6067 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6068 | `	 */` |
|     1674 |  6069 | `	return SXRET_OK;` |
|      840 |  6070 |  |
|        - |  6071 | `/*` |
|        - |  6072 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6073 | ` * the desired message.` |
|        - |  6074 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6075 | ` * in 'api.c' for additional information.` |
|        - |  6076 | ` */` |
|      350 |  6077 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6078 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6079 | `	SyString *pString /* Message to output */` |
|        - |  6080 | `	)` |
|        2 |  6081 |  |
|      352 |  6082 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      352 |  6083 | `	sxi32 rc = SXRET_OK;` |
|        - |  6084 | `	/* Call the output consumer */` |
|      352 |  6085 | `	if( pString->nByte > 0 ){` |
|      352 |  6086 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      352 |  6087 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6088 | `			/* Increment output length */` |
|       17 |  6089 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6090 | `		}` |
|      175 |  6091 | `	}` |
|      352 |  6092 | `	return rc;` |
|        2 |  6093 |  |
|        - |  6094 | `/*` |
|        - |  6095 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6096 | ` * callback to consume the formatted message.` |
|        - |  6097 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6098 | ` * in 'api.c' for additional information.` |
|        - |  6099 | ` */` |
|        2 |  6100 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6101 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6102 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6103 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6104 | `	)` |
|        1 |  6105 |  |
|        3 |  6106 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6107 | `	sxi32 rc = SXRET_OK;` |
|        - |  6108 | `	SyBlob sWorker;` |
|        - |  6109 | `	/* Format the message and call the output consumer */` |
|        3 |  6110 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6111 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6112 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6113 | `		/* Consume the formatted message */` |
|        3 |  6114 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6115 | `	}` |
|        3 |  6116 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6117 | `		/* Increment output length */` |
|      ! 0 |  6118 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6119 | `	}` |
|        - |  6120 | `	/* Release the working buffer */` |
|        3 |  6121 | `	SyBlobRelease(&sWorker);` |
|        3 |  6122 | `	return rc;` |
|        1 |  6123 |  |
|        - |  6124 | `/*` |
|        - |  6125 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6126 | ` * This function never fail and always return a pointer` |
|        - |  6127 | ` * to a null terminated string.` |
|        - |  6128 | ` */` |
|       10 |  6129 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6130 |  |
|       11 |  6131 | `	const char *zOp = "Unknown     ";` |
|       11 |  6132 | `	switch(nOp){` |
|        3 |  6133 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6134 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6135 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6136 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6137 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6138 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6139 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6140 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6141 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6142 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6143 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6144 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6145 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6146 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6147 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6148 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6149 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6150 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6151 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6152 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6153 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6154 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6155 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6156 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6157 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6158 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6159 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6160 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6161 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6162 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6163 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6164 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6165 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6166 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6167 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6168 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6169 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6170 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6171 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6172 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6173 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6174 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6175 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6176 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6177 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6178 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6179 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6180 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6181 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6182 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6183 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6184 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6185 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6186 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6187 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6188 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6189 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6190 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6191 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6192 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6193 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6194 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6195 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6196 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6197 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6198 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6199 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6200 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6201 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6202 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6203 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6204 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6205 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6206 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6207 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6208 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6209 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6210 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6211 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6212 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6213 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6214 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6215 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6216 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6217 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6218 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6219 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6220 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6221 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6222 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6223 | `	default:` |
|      ! 0 |  6224 | `		break;` |
|        - |  6225 | `	}` |
|       11 |  6226 | `	return zOp;` |
|        1 |  6227 |  |
|        - |  6228 | `/*` |
|        - |  6229 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6230 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6231 | ` * is responsible of consuming the generated dump.` |
|        - |  6232 | ` */` |
|        2 |  6233 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6234 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6235 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6236 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6237 | `	)` |
|        1 |  6238 |  |
|        - |  6239 | `	sxi32 rc;` |
|        3 |  6240 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6241 | `	return rc;` |
|        1 |  6242 |  |
|        - |  6243 | `/*` |
|        - |  6244 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6245 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6246 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6247 | ` * in 'compile.c' for additional information.` |
|        - |  6248 | ` */` |
|        8 |  6249 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6250 |  |
|        9 |  6251 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6252 | `	/* Evaluate and expand constant value */` |
|        9 |  6253 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6254 |  |
|        - |  6255 | `/*` |
|        - |  6256 | ` * Section:` |
|        - |  6257 | ` *  Function handling functions.` |
|        - |  6258 | ` * Status:` |
|        - |  6259 | ` *    Stable.` |
|        - |  6260 | ` */` |
|        - |  6261 | `/*` |
|        - |  6262 | ` * int func_num_args(void)` |
|        - |  6263 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6264 | ` * Parameters` |
|        - |  6265 | ` *   None.` |
|        - |  6266 | ` * Return` |
|        - |  6267 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6268 | ` *  or -1 if called from the globe scope.` |
|        - |  6269 | ` */` |
|      886 |  6270 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6271 |  |
|        - |  6272 | `	VmFrame *pFrame;` |
|        - |  6273 | `	ph7_vm *pVm;` |
|        - |  6274 | `	/* Point to the target VM */` |
|      888 |  6275 | `	pVm = pCtx->pVm;` |
|        - |  6276 | `	/* Current frame */` |
|      888 |  6277 | `	pFrame = pVm->pFrame;` |
|      888 |  6278 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6279 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6280 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6281 | `	}` |
|      888 |  6282 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6283 | `		SXUNUSED(nArg);` |
|      ! 0 |  6284 | `		SXUNUSED(apArg);` |
|        - |  6285 | `		/* Global frame,return -1 */` |
|      ! 0 |  6286 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6287 | `		return SXRET_OK;` |
|        - |  6288 | `	}` |
|        - |  6289 | `	/* Total number of arguments passed to the enclosing function */` |
|      888 |  6290 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      888 |  6291 | `	ph7_result_int(pCtx,nArg);` |
|      888 |  6292 | `	return SXRET_OK;` |
|      445 |  6293 |  |
|        - |  6294 | `/*` |
|        - |  6295 | ` * value func_get_arg(int $arg_num)` |
|        - |  6296 | ` *   Return an item from the argument list.` |
|        - |  6297 | ` * Parameters` |
|        - |  6298 | ` *  Argument number(index start from zero).` |
|        - |  6299 | ` * Return` |
|        - |  6300 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6301 | ` */` |
|       22 |  6302 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6303 |  |
|       24 |  6304 | `	ph7_value *pObj = 0;` |
|       24 |  6305 | `	VmSlot *pSlot = 0;` |
|        - |  6306 | `	VmFrame *pFrame;` |
|        - |  6307 | `	ph7_vm *pVm;` |
|        - |  6308 | `	/* Point to the target VM */` |
|       24 |  6309 | `	pVm = pCtx->pVm;` |
|        - |  6310 | `	/* Current frame */` |
|       24 |  6311 | `	pFrame = pVm->pFrame;` |
|       24 |  6312 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6313 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6314 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6315 | `	}` |
|       24 |  6316 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6317 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6318 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6319 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6320 | `		return SXRET_OK;` |
|        - |  6321 | `	}` |
|        - |  6322 | `	/* Extract the desired index */` |
|       21 |  6323 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  6324 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6325 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6326 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6327 | `		return SXRET_OK;` |
|        - |  6328 | `	}` |
|        - |  6329 | `	/* Extract the desired argument */` |
|       21 |  6330 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  6331 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6332 | `			/* Return the desired argument */` |
|       21 |  6333 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  6334 | `		}else{` |
|        - |  6335 | `			/* No such argument,return false */` |
|      ! 0 |  6336 | `			ph7_result_bool(pCtx,0);` |
|        - |  6337 | `		}` |
|       11 |  6338 | `	}else{` |
|        - |  6339 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6340 | `		ph7_result_bool(pCtx,0);` |
|        - |  6341 | `	}` |
|       21 |  6342 | `	return SXRET_OK;` |
|       13 |  6343 |  |
|        - |  6344 | `/*` |
|        - |  6345 | ` * array func_get_args_byref(void)` |
|        - |  6346 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6347 | ` * Parameters` |
|        - |  6348 | ` *  None.` |
|        - |  6349 | ` * Return` |
|        - |  6350 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6351 | ` *  member of the current user-defined function's argument list.` |
|        - |  6352 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6353 | ` * NOTE:` |
|        - |  6354 | ` *  Arguments are returned to the array by reference.` |
|        - |  6355 | ` */` |
|        2 |  6356 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6357 |  |
|        - |  6358 | `	ph7_value *pArray;` |
|        - |  6359 | `	VmFrame *pFrame;` |
|        - |  6360 | `	VmSlot *aSlot;` |
|        - |  6361 | `	sxu32 n;` |
|        - |  6362 | `	/* Point to the current frame */` |
|        3 |  6363 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6364 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6365 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6366 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6367 | `	}` |
|        3 |  6368 | `	if( pFrame->pParent == 0 ){` |
|        - |  6369 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6370 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6371 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6372 | `		return SXRET_OK;` |
|        - |  6373 | `	}` |
|        - |  6374 | `	/* Create a new array */` |
|        3 |  6375 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6376 | `	if( pArray == 0 ){` |
|      ! 0 |  6377 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6378 | `		SXUNUSED(apArg);` |
|      ! 0 |  6379 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6380 | `		return SXRET_OK;` |
|        - |  6381 | `	}` |
|        - |  6382 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6383 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6384 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6385 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6386 | `	}` |
|        - |  6387 | `	/* Return the freshly created array */` |
|        3 |  6388 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6389 | `	return SXRET_OK;` |
|        2 |  6390 |  |
|        - |  6391 | `/*` |
|        - |  6392 | ` * array func_get_args(void)` |
|        - |  6393 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6394 | ` * Parameters` |
|        - |  6395 | ` *  None.` |
|        - |  6396 | ` * Return` |
|        - |  6397 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6398 | ` *  member of the current user-defined function's argument list.` |
|        - |  6399 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6400 | ` */` |
|       62 |  6401 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6402 |  |
|       64 |  6403 | `	ph7_value *pObj = 0;` |
|        - |  6404 | `	ph7_value *pArray;` |
|        - |  6405 | `	VmFrame *pFrame;` |
|        - |  6406 | `	VmSlot *aSlot;` |
|        - |  6407 | `	sxu32 n;` |
|        - |  6408 | `	/* Point to the current frame */` |
|       64 |  6409 | `	pFrame = pCtx->pVm->pFrame;` |
|       64 |  6410 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6411 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6412 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6413 | `	}` |
|       64 |  6414 | `	if( pFrame->pParent == 0 ){` |
|        - |  6415 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6416 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6417 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6418 | `		return SXRET_OK;` |
|        - |  6419 | `	}` |
|        - |  6420 | `	/* Create a new array */` |
|       64 |  6421 | `	pArray = ph7_context_new_array(pCtx);` |
|       64 |  6422 | `	if( pArray == 0 ){` |
|      ! 0 |  6423 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6424 | `		SXUNUSED(apArg);` |
|      ! 0 |  6425 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6426 | `		return SXRET_OK;` |
|        - |  6427 | `	}` |
|        - |  6428 | `	/* Start filling the array with the given arguments */` |
|       64 |  6429 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      192 |  6430 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      130 |  6431 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      130 |  6432 | `		if( pObj ){` |
|      130 |  6433 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       64 |  6434 | `		}` |
|       66 |  6435 | `	}` |
|        - |  6436 | `	/* Return the freshly created array */` |
|       64 |  6437 | `	ph7_result_value(pCtx,pArray);` |
|       64 |  6438 | `	return SXRET_OK;` |
|       33 |  6439 |  |
|        - |  6440 | `/*` |
|        - |  6441 | ` * bool function_exists(string $name)` |
|        - |  6442 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6443 | ` * Parameters` |
|        - |  6444 | ` *  The name of the desired function.` |
|        - |  6445 | ` * Return` |
|        - |  6446 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6447 | ` */` |
|     1664 |  6448 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6449 |  |
|        - |  6450 | `	const char *zName;` |
|        - |  6451 | `	ph7_vm *pVm;` |
|        - |  6452 | `	int nLen;` |
|        - |  6453 | `	int res;` |
|     1666 |  6454 | `	if( nArg < 1 ){` |
|        - |  6455 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6456 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6457 | `		return SXRET_OK;` |
|        - |  6458 | `	}` |
|        - |  6459 | `	/* Point to the target VM */` |
|     1666 |  6460 | `	pVm = pCtx->pVm;` |
|        - |  6461 | `	/* Extract the function name */` |
|     1666 |  6462 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6463 | `	/* Assume the function is not defined */` |
|     1666 |  6464 | `	res = 0;` |
|        - |  6465 | `	/* Perform the lookup */` |
|     2496 |  6466 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1660 |  6467 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6468 | `			/* Function is defined */` |
|      212 |  6469 | `			res = 1;` |
|      105 |  6470 | `	}` |
|     1666 |  6471 | `	ph7_result_bool(pCtx,res);` |
|     1666 |  6472 | `	return SXRET_OK;` |
|      834 |  6473 |  |
|        - |  6474 | `/*` |
|        - |  6475 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6476 | ` * [i.e: Whether it is callable or not].` |
|        - |  6477 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6478 | ` */` |
|    15888 |  6479 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6480 |  |
|    15890 |  6481 | `	int res = 0;` |
|    15890 |  6482 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6483 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6484 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6485 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6486 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6487 | `		if( pMethod && CallInvoke ){` |
|        - |  6488 | `			ph7_value sResult;` |
|        - |  6489 | `			sxi32 rc;` |
|        - |  6490 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6491 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6492 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6493 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6494 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6495 | `			}` |
|      ! 0 |  6496 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6497 | `		}` |
|    15890 |  6498 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  6499 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       20 |  6500 | `		if( pMap->nEntry == 2 ){` |
|        - |  6501 | `			ph7_class *pClass;` |
|        - |  6502 | `			ph7_value *pV;` |
|        - |  6503 | `			/* Extract the target class */` |
|        7 |  6504 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|        7 |  6505 | `			if( pV ){` |
|        7 |  6506 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|        7 |  6507 | `				if( pClass ){` |
|        - |  6508 | `					ph7_class_method *pMethod;` |
|        - |  6509 | `					/* Extract the target method */` |
|        7 |  6510 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|        7 |  6511 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6512 | `						/* Perform the lookup */` |
|        7 |  6513 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|        7 |  6514 | `						if( pMethod ){` |
|        - |  6515 | `							/* Method is callable */` |
|        5 |  6516 | `							res = 1;` |
|        2 |  6517 | `						}` |
|        3 |  6518 | `					}` |
|        3 |  6519 | `				}` |
|        3 |  6520 | `			}` |
|        5 |  6521 | `		}` |
|    15881 |  6522 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6523 | `		const char *zName;` |
|        - |  6524 | `		int nLen;` |
|        - |  6525 | `		/* Extract the name */` |
|     4668 |  6526 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6527 | `		/* Perform the lookup */` |
|     4681 |  6528 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       26 |  6529 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6530 | `				/* Function is callable */` |
|     4654 |  6531 | `				res = 1;` |
|     2326 |  6532 | `		}` |
|     2333 |  6533 | `	}` |
|    15890 |  6534 | `	return res;` |
|        2 |  6535 |  |
|        - |  6536 | `/*` |
|        - |  6537 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6538 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6539 | ` * Parameters` |
|        - |  6540 | ` * $name` |
|        - |  6541 | ` *    The callback function to check` |
|        - |  6542 | ` * $syntax_only` |
|        - |  6543 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6544 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6545 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6546 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6547 | ` *    a string.` |
|        - |  6548 | ` * Return` |
|        - |  6549 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6550 | ` */` |
|       14 |  6551 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6552 |  |
|        - |  6553 | `	ph7_vm *pVm;` |
|        - |  6554 | `	int res;` |
|       15 |  6555 | `	if( nArg < 1 ){` |
|        - |  6556 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6557 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6558 | `		return SXRET_OK;` |
|        - |  6559 | `	}` |
|        - |  6560 | `	/* Point to the target VM */` |
|       15 |  6561 | `	pVm = pCtx->pVm;` |
|        - |  6562 | `	/* Perform the requested operation */` |
|       15 |  6563 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6564 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6565 | `	return SXRET_OK;` |
|        8 |  6566 |  |
|        - |  6567 | `/*` |
|        - |  6568 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6569 | ` * defined below.` |
|        - |  6570 | ` */` |
|     1074 |  6571 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6572 |  |
|     1075 |  6573 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6574 | `	ph7_value sName;` |
|        - |  6575 | `	sxi32 rc;` |
|        - |  6576 | `	/* Prepare the function name for insertion */` |
|     1075 |  6577 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1075 |  6578 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6579 | `	/* Perform the insertion */` |
|     1075 |  6580 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1075 |  6581 | `	PH7_MemObjRelease(&sName);` |
|     1075 |  6582 | `	return rc;` |
|        1 |  6583 |  |
|        - |  6584 | `/*` |
|        - |  6585 | ` * array get_defined_functions(void)` |
|        - |  6586 | ` *  Returns an array of all defined functions.` |
|        - |  6587 | ` * Parameter` |
|        - |  6588 | ` *  None.` |
|        - |  6589 | ` * Return` |
|        - |  6590 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6591 | ` *  both built-in (internal) and user-defined.` |
|        - |  6592 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6593 | ` *  defined ones using $arr["user"].` |
|        - |  6594 | ` * Note:` |
|        - |  6595 | ` *  NULL is returned on failure.` |
|        - |  6596 | ` */` |
|        2 |  6597 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6598 |  |
|        - |  6599 | `	ph7_value *pArray,*pEntry;` |
|        - |  6600 | `	/* NOTE:` |
|        - |  6601 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6602 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6603 | `	 */` |
|        3 |  6604 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6605 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6606 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6607 | `		SXUNUSED(apArg);` |
|        - |  6608 | `		/* Return NULL */` |
|      ! 0 |  6609 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6610 | `		return SXRET_OK;` |
|        - |  6611 | `	}` |
|        3 |  6612 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6613 | `	if( pEntry == 0 ){` |
|        - |  6614 | `		/* Return NULL */` |
|      ! 0 |  6615 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6616 | `		return SXRET_OK;` |
|        - |  6617 | `	}` |
|        - |  6618 | `	/* Fill with the appropriate information */` |
|        3 |  6619 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6620 | `	/* Create the 'internal' index */` |
|        3 |  6621 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6622 | `	/* Create the user-func array */` |
|        3 |  6623 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6624 | `	if( pEntry == 0 ){` |
|        - |  6625 | `		/* Return NULL */` |
|      ! 0 |  6626 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6627 | `		return SXRET_OK;` |
|        - |  6628 | `	}` |
|        - |  6629 | `	/* Fill with the appropriate information */` |
|        3 |  6630 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6631 | `	/* Create the 'user' index */` |
|        3 |  6632 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6633 | `	/* Return the multi-dimensional array */` |
|        3 |  6634 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6635 | `	return SXRET_OK;` |
|        2 |  6636 |  |
|        - |  6637 | `/*` |
|        - |  6638 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6639 | ` *  Register a function for execution on shutdown.` |
|        - |  6640 | ` * Note` |
|        - |  6641 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6642 | ` *  be called in the same order as they were registered.` |
|        - |  6643 | ` * Parameters` |
|        - |  6644 | ` *  $callback` |
|        - |  6645 | ` *   The shutdown callback to register.` |
|        - |  6646 | ` * $param` |
|        - |  6647 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6648 | ` * Return` |
|        - |  6649 | ` *  Nothing.` |
|        - |  6650 | ` */` |
|        2 |  6651 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6652 |  |
|        - |  6653 | `	VmShutdownCB sEntry;` |
|        - |  6654 | `	int i,j;` |
|        3 |  6655 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6656 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6657 | `		return PH7_OK;` |
|        - |  6658 | `	}` |
|        - |  6659 | `	/* Zero the Entry */` |
|        3 |  6660 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6661 | `	/* Initialize fields */` |
|        3 |  6662 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6663 | `	/* Save the callback name for later invocation name */` |
|        3 |  6664 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6665 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6666 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6667 | `	}` |
|        - |  6668 | `	/* Copy arguments */` |
|        3 |  6669 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6670 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6671 | `			/* Limit reached */` |
|      ! 0 |  6672 | `			break;` |
|        - |  6673 | `		}` |
|      ! 0 |  6674 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6675 | `	}` |
|        3 |  6676 | `	sEntry.nArg = j;` |
|        - |  6677 | `	/* Install the callback */` |
|        3 |  6678 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6679 | `	return PH7_OK;` |
|        2 |  6680 |  |
|        - |  6681 | `/*` |
|        - |  6682 | ` * Section:` |
|        - |  6683 | ` *  Class handling functions.` |
|        - |  6684 | ` * Status:` |
|        - |  6685 | ` *    Stable.` |
|        - |  6686 | ` */` |
|        - |  6687 | `/*` |
|        - |  6688 | ` * Extract the top active class. NULL is returned` |
|        - |  6689 | ` * if the class stack is empty.` |
|        - |  6690 | ` */` |
|      402 |  6691 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6692 |  |
|      404 |  6693 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6694 | `	ph7_class **apClass;` |
|      404 |  6695 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6696 | `		/* Empty stack,return NULL */` |
|       15 |  6697 | `		return 0;` |
|        - |  6698 | `	}` |
|        - |  6699 | `	/* Peek the last entry */` |
|      390 |  6700 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      390 |  6701 | `	return apClass[pSet->nUsed - 1];` |
|      203 |  6702 |  |
|        - |  6703 | `/*` |
|        - |  6704 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  6705 | ` *   Get the class that declared the currently executing method.` |
|        - |  6706 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  6707 | ` *` |
|        - |  6708 | ` * Parameters` |
|        - |  6709 | ` *   pVm: Target VM` |
|        - |  6710 | ` *` |
|        - |  6711 | ` * Return` |
|        - |  6712 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  6713 | ` *   - Not executing within a class method` |
|        - |  6714 | ` *` |
|        - |  6715 | ` * Note` |
|        - |  6716 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  6717 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  6718 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  6719 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  6720 | ` *   declaring class.` |
|        - |  6721 | ` */` |
|       18 |  6722 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        1 |  6723 |  |
|       19 |  6724 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  6725 | `	ph7_vm_func *pVmFunc;` |
|        - |  6726 |  |
|        - |  6727 | `	/* Skip exception frames to find the actual method frame */` |
|       19 |  6728 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  6729 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6730 | `	}` |
|        - |  6731 |  |
|        - |  6732 | `	/* Check if we're in a method context */` |
|       19 |  6733 | `	if( pFrame->pParent ){` |
|       15 |  6734 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       15 |  6735 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  6736 | `			/* Return the declaring class */` |
|       15 |  6737 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  6738 | `		}` |
|      ! 0 |  6739 | `	}` |
|        - |  6740 |  |
|        5 |  6741 | `	return 0;` |
|       10 |  6742 |  |
|        - |  6743 |  |
|        - |  6744 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  6745 | `/*` |
|        - |  6746 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  6747 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  6748 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  6749 | ` * return value indicates failure.` |
|        - |  6750 | ` */` |
|      922 |  6751 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  6752 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  6753 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  6754 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  6755 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  6756 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  6757 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  6758 | `	)` |
|        2 |  6759 |  |
|        - |  6760 | `	ph7_value *aStack;` |
|        - |  6761 | `	VmInstr aInstr[2];` |
|        - |  6762 | `	int iCursor;` |
|        - |  6763 | `	int i;` |
|        - |  6764 | `	/* Create a new operand stack */` |
|      924 |  6765 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|      924 |  6766 | `	if( aStack == 0 ){` |
|      ! 0 |  6767 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6768 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  6769 | `		return SXERR_MEM;` |
|        - |  6770 | `	}` |
|        - |  6771 | `	/* Fill the operand stack with the given arguments */` |
|     1356 |  6772 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      434 |  6773 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  6774 | `		/*` |
|        - |  6775 | `		 * Symisc eXtension:` |
|        - |  6776 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  6777 | `		 */` |
|      434 |  6778 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      218 |  6779 | `	}` |
|      924 |  6780 | `	iCursor = nArg + 1;` |
|      924 |  6781 | `	if( pThis ){` |
|        - |  6782 | `		/*` |
|        - |  6783 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  6784 | `		 */` |
|      918 |  6785 | `		pThis->iRef++; /* Increment reference count */` |
|      918 |  6786 | `		aStack[i].x.pOther = pThis;` |
|      918 |  6787 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      458 |  6788 | `	}` |
|      924 |  6789 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|      924 |  6790 | `	i++;` |
|        - |  6791 | `	/* Push method name */` |
|      924 |  6792 | `	SyBlobReset(&aStack[i].sBlob);` |
|      924 |  6793 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|      924 |  6794 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|      924 |  6795 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  6796 | `	/* Emit the CALL istruction */` |
|      924 |  6797 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      924 |  6798 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      924 |  6799 | `	aInstr[0].iP2 = 0;` |
|      924 |  6800 | `	aInstr[0].p3  = 0;` |
|        - |  6801 | `	/* Emit the DONE instruction */` |
|      924 |  6802 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      924 |  6803 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|      924 |  6804 | `	aInstr[1].iP2 = 0;` |
|      924 |  6805 | `	aInstr[1].p3  = 0;` |
|        - |  6806 | `	/* Execute the method body (if available) */` |
|      924 |  6807 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  6808 | `	/* Clean up the mess left behind */` |
|      924 |  6809 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      924 |  6810 | `	return PH7_OK;` |
|      463 |  6811 |  |
|        - |  6812 | `/*` |
|        - |  6813 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  6814 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  6815 | ` * in the apArg[] array.` |
|        - |  6816 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  6817 | ` * return value indicates failure.` |
|        - |  6818 | ` */` |
|      802 |  6819 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  6820 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  6821 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  6822 | `	int nArg,          /* Total number of given arguments */` |
|        - |  6823 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  6824 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  6825 | `	)` |
|        2 |  6826 |  |
|        - |  6827 | `	ph7_value *aStack;` |
|        - |  6828 | `	VmInstr aInstr[2];` |
|        - |  6829 | `	int i;` |
|      804 |  6830 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6831 | `		/* Don't bother processing,it's invalid anyway */` |
|      361 |  6832 | `		if( pResult ){` |
|        - |  6833 | `			/* Assume a null return value */` |
|      ! 0 |  6834 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  6835 | `		}` |
|      361 |  6836 | `		return SXERR_INVALID;` |
|        - |  6837 | `	}` |
|      444 |  6838 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6839 | `		/* Class method */` |
|       11 |  6840 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  6841 | `		ph7_class_method *pMethod = 0;` |
|       11 |  6842 | `		ph7_class_instance *pThis = 0;` |
|       11 |  6843 | `		ph7_class *pClass = 0;` |
|        - |  6844 | `		ph7_value *pValue;` |
|        - |  6845 | `		sxi32 rc;` |
|       11 |  6846 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  6847 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  6848 | `			if( pResult ){` |
|        - |  6849 | `				/* Assume a null return value */` |
|      ! 0 |  6850 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  6851 | `			}` |
|      ! 0 |  6852 | `			return SXRET_OK;` |
|        - |  6853 | `		}` |
|        - |  6854 | `		/* Extract the class name or an instance of it */` |
|       11 |  6855 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  6856 | `		if( pValue ){` |
|       11 |  6857 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  6858 | `		}` |
|       11 |  6859 | `		if( pClass == 0 ){` |
|        - |  6860 | `			/* No such class,return NULL */` |
|      ! 0 |  6861 | `			if( pResult ){` |
|      ! 0 |  6862 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  6863 | `			}` |
|      ! 0 |  6864 | `			return SXRET_OK;` |
|        - |  6865 | `		}` |
|       11 |  6866 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6867 | `			/* Point to the class instance */` |
|        5 |  6868 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  6869 | `		}` |
|        - |  6870 | `		/* Try to extract the method */` |
|       11 |  6871 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  6872 | `		if( pValue ){` |
|       11 |  6873 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  6874 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  6875 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  6876 | `			}` |
|        5 |  6877 | `		}` |
|       11 |  6878 | `		if( pMethod == 0 ){` |
|        - |  6879 | `			/* No such method,return NULL */` |
|      ! 0 |  6880 | `			if( pResult ){` |
|      ! 0 |  6881 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  6882 | `			}` |
|      ! 0 |  6883 | `			return SXRET_OK;` |
|        - |  6884 | `		}` |
|        - |  6885 | `		/* Call the class method */` |
|       11 |  6886 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  6887 | `		return rc;` |
|        - |  6888 | `	}` |
|        - |  6889 | `	/* Create a new operand stack */` |
|      434 |  6890 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      434 |  6891 | `	if( aStack == 0 ){` |
|      ! 0 |  6892 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6893 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  6894 | `		if( pResult ){` |
|        - |  6895 | `			/* Assume a null return value */` |
|      ! 0 |  6896 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  6897 | `		}` |
|      ! 0 |  6898 | `		return SXERR_MEM;` |
|        - |  6899 | `	}` |
|        - |  6900 | `	/* Fill the operand stack with the given arguments */` |
|     1428 |  6901 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      996 |  6902 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  6903 | `		/*` |
|        - |  6904 | `		 * Symisc eXtension:` |
|        - |  6905 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  6906 | `		 */` |
|      996 |  6907 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      499 |  6908 | `	}` |
|        - |  6909 | `	/* Push the function name */` |
|      434 |  6910 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      434 |  6911 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  6912 | `	/* Emit the CALL istruction */` |
|      434 |  6913 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      434 |  6914 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      434 |  6915 | `	aInstr[0].iP2 = 0;` |
|      434 |  6916 | `	aInstr[0].p3  = 0;` |
|        - |  6917 | `	/* Emit the DONE instruction */` |
|      434 |  6918 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      434 |  6919 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      434 |  6920 | `	aInstr[1].iP2 = 0;` |
|      434 |  6921 | `	aInstr[1].p3  = 0;` |
|        - |  6922 | `	/* Execute the function body (if available) */` |
|      434 |  6923 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  6924 | `	/* Clean up the mess left behind */` |
|      434 |  6925 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      434 |  6926 | `	return PH7_OK;` |
|      403 |  6927 |  |
|        - |  6928 | `/*` |
|        - |  6929 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  6930 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  6931 | ` * parameter.` |
|        - |  6932 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  6933 | ` * return value indicates failure.` |
|        - |  6934 | ` */` |
|      236 |  6935 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  6936 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  6937 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  6938 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  6939 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  6940 | `	)` |
|        1 |  6941 |  |
|        - |  6942 | `	ph7_value *pArg;` |
|        - |  6943 | `	SySet aArg;` |
|        - |  6944 | `	va_list ap;` |
|        - |  6945 | `	sxi32 rc;` |
|      237 |  6946 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  6947 | `	/* Copy arguments one after one */` |
|      237 |  6948 | `	va_start(ap,pResult);` |
|      393 |  6949 | `	for(;;){` |
|      787 |  6950 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  6951 | `		if( pArg == 0 ){` |
|      237 |  6952 | `			break;` |
|        - |  6953 | `		}` |
|      551 |  6954 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  6955 | `	}` |
|        - |  6956 | `	/* Call the core routine */` |
|      237 |  6957 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  6958 | `	/* Cleanup */` |
|      237 |  6959 | `	SySetRelease(&aArg);` |
|      237 |  6960 | `	return rc;` |
|        1 |  6961 |  |
|        - |  6962 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  6963 | `/*` |
|        - |  6964 | ` * bool defined(string $name)` |
|        - |  6965 | ` *  Checks whether a given named constant exists.` |
|        - |  6966 | ` * Parameter:` |
|        - |  6967 | ` *  Name of the desired constant.` |
|        - |  6968 | ` * Return` |
|        - |  6969 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  6970 | ` */` |
|       14 |  6971 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6972 |  |
|        - |  6973 | `	const char *zName;` |
|       16 |  6974 | `	int nLen = 0;` |
|       16 |  6975 | `	int res = 0;` |
|       16 |  6976 | `	if( nArg < 1 ){` |
|        - |  6977 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  6978 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  6979 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6980 | `		return SXRET_OK;` |
|        - |  6981 | `	}` |
|        - |  6982 | `	/* Extract constant name */` |
|       16 |  6983 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6984 | `	/* Perform the lookup */` |
|       16 |  6985 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6986 | `		/* Already defined */` |
|       10 |  6987 | `		res = 1;` |
|        4 |  6988 | `	}` |
|       16 |  6989 | `	ph7_result_bool(pCtx,res);` |
|       16 |  6990 | `	return SXRET_OK;` |
|        9 |  6991 |  |
|        - |  6992 | `/*` |
|        - |  6993 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  6994 | ` * below.` |
|        - |  6995 | ` */` |
|        8 |  6996 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  6997 |  |
|       10 |  6998 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  6999 | `	/* Expand constant value */` |
|       10 |  7000 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7001 |  |
|        - |  7002 | `/*` |
|        - |  7003 | ` * bool define(string $constant_name,expression value)` |
|        - |  7004 | ` *  Defines a named constant at runtime.` |
|        - |  7005 | ` * Parameter:` |
|        - |  7006 | ` *  $constant_name` |
|        - |  7007 | ` *   The name of the constant` |
|        - |  7008 | ` *  $value` |
|        - |  7009 | ` *   Constant value` |
|        - |  7010 | ` * Return:` |
|        - |  7011 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7012 | ` */` |
|       10 |  7013 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7014 |  |
|        - |  7015 | `	const char *zName;  /* Constant name */` |
|        - |  7016 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7017 | `	int nLen = 0;       /* Name length */` |
|        - |  7018 | `	sxi32 rc;` |
|       12 |  7019 | `	if( nArg < 2 ){` |
|        - |  7020 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7021 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7022 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7023 | `		return SXRET_OK;` |
|        - |  7024 | `	}` |
|       12 |  7025 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7026 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7027 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7028 | `		return SXRET_OK;` |
|        - |  7029 | `	}` |
|        - |  7030 | `	/* Extract constant name */` |
|       12 |  7031 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7032 | `	if( nLen < 1 ){` |
|      ! 0 |  7033 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7034 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7035 | `		return SXRET_OK;` |
|        - |  7036 | `	}` |
|        - |  7037 | `	/* Duplicate constant value */` |
|       12 |  7038 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7039 | `	if( pValue == 0 ){` |
|      ! 0 |  7040 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7041 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7042 | `		return SXRET_OK;` |
|        - |  7043 | `	}` |
|        - |  7044 | `	/* Initialize the memory object */` |
|       12 |  7045 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7046 | `	/* Register the constant */` |
|       12 |  7047 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7048 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7049 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7050 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7051 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7052 | `		return SXRET_OK;` |
|        - |  7053 | `	}` |
|        - |  7054 | `	/* Duplicate constant value */` |
|       12 |  7055 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7056 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7057 | `		/* Lower case the constant name */` |
|      ! 0 |  7058 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7059 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7060 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7061 | `				/* UTF-8 stream */` |
|      ! 0 |  7062 | `				zCur++;` |
|      ! 0 |  7063 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  7064 | `					zCur++;` |
|      ! 0 |  7065 | `				}` |
|      ! 0 |  7066 | `				continue;` |
|        - |  7067 | `			}` |
|      ! 0 |  7068 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  7069 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  7070 | `				zCur[0] = (char)c;` |
|      ! 0 |  7071 | `			}` |
|      ! 0 |  7072 | `			zCur++;` |
|      ! 0 |  7073 | `		}` |
|        - |  7074 | `		/* Finally,register the constant */` |
|      ! 0 |  7075 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  7076 | `	}` |
|        - |  7077 | `	/* All done,return TRUE */` |
|       12 |  7078 | `	ph7_result_bool(pCtx,1);` |
|       12 |  7079 | `	return SXRET_OK;` |
|        7 |  7080 |  |
|        - |  7081 | `/*` |
|        - |  7082 | ` * value constant(string $name)` |
|        - |  7083 | ` *  Returns the value of a constant` |
|        - |  7084 | ` * Parameter` |
|        - |  7085 | ` *  $name` |
|        - |  7086 | ` *    Name of the constant.` |
|        - |  7087 | ` * Return` |
|        - |  7088 | ` *  Constant value or NULL if not defined.` |
|        - |  7089 | ` */` |
|        8 |  7090 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7091 |  |
|        - |  7092 | `	SyHashEntry *pEntry;` |
|        - |  7093 | `	ph7_constant *pCons;` |
|        - |  7094 | `	const char *zName; /* Constant name */` |
|        - |  7095 | `	ph7_value sVal;    /* Constant value */` |
|        - |  7096 | `	int nLen;` |
|       10 |  7097 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  7098 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  7099 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  7100 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7101 | `		return SXRET_OK;` |
|        - |  7102 | `	}` |
|        - |  7103 | `	/* Extract the constant name */` |
|       10 |  7104 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7105 | `	/* Perform the query */` |
|       10 |  7106 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  7107 | `	if( pEntry == 0 ){` |
|        3 |  7108 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  7109 | `		ph7_result_null(pCtx);` |
|        3 |  7110 | `		return SXRET_OK;` |
|        - |  7111 | `	}` |
|        8 |  7112 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  7113 | `	/* Point to the structure that describe the constant */` |
|        8 |  7114 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  7115 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  7116 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  7117 | `	/* Return that value */` |
|        8 |  7118 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  7119 | `	/* Cleanup */` |
|        8 |  7120 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  7121 | `	return SXRET_OK;` |
|        6 |  7122 |  |
|        - |  7123 | `/*` |
|        - |  7124 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  7125 | ` * defined below.` |
|        - |  7126 | ` */` |
|      414 |  7127 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7128 |  |
|      415 |  7129 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  7130 | `	ph7_value sName;` |
|        - |  7131 | `	sxi32 rc;` |
|        - |  7132 | `	/* Prepare the constant name for insertion */` |
|      415 |  7133 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      415 |  7134 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  7135 | `	/* Perform the insertion */` |
|      415 |  7136 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      415 |  7137 | `	PH7_MemObjRelease(&sName);` |
|      415 |  7138 | `	return rc;` |
|        1 |  7139 |  |
|        - |  7140 | `/*` |
|        - |  7141 | ` * array get_defined_constants(void)` |
|        - |  7142 | ` *  Returns an associative array with the names of all defined` |
|        - |  7143 | ` *  constants.` |
|        - |  7144 | ` * Parameters` |
|        - |  7145 | ` *  NONE.` |
|        - |  7146 | ` * Returns` |
|        - |  7147 | ` *  Returns the names of all the constants currently defined.` |
|        - |  7148 | ` */` |
|        2 |  7149 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7150 |  |
|        - |  7151 | `	ph7_value *pArray;` |
|        - |  7152 | `	/* Create the array first*/` |
|        3 |  7153 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7154 | `	if( pArray == 0 ){` |
|      ! 0 |  7155 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7156 | `		SXUNUSED(apArg);` |
|        - |  7157 | `		/* Return NULL */` |
|      ! 0 |  7158 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7159 | `		return SXRET_OK;` |
|        - |  7160 | `	}` |
|        - |  7161 | `	/* Fill the array with the defined constants */` |
|        3 |  7162 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  7163 | `	/* Return the created array */` |
|        3 |  7164 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7165 | `	return SXRET_OK;` |
|        2 |  7166 |  |
|        - |  7167 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  7168 | `/*` |
|        - |  7169 | ` * Section:` |
|        - |  7170 | ` *  Random numbers/string generators.` |
|        - |  7171 | ` * Status:` |
|        - |  7172 | ` *    Stable.` |
|        - |  7173 | ` */` |
|        - |  7174 | `/*` |
|        - |  7175 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  7176 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  7177 | ` * used by te SQLite3 library.` |
|        - |  7178 | ` */` |
|     1750 |  7179 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  7180 |  |
|        - |  7181 | `	sxu32 iNum;` |
|     1752 |  7182 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     1752 |  7183 | `	return iNum;` |
|        2 |  7184 |  |
|        - |  7185 | `/*` |
|        - |  7186 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  7187 | ` * Note that the generated string is NOT null terminated.` |
|        - |  7188 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  7189 | ` * by te SQLite3 library.` |
|        - |  7190 | ` */` |
|    55860 |  7191 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  7192 |  |
|        - |  7193 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  7194 | `	int i;` |
|        - |  7195 | `	/* Generate a binary string first */` |
|    55862 |  7196 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  7197 | `	/* Turn the binary string into english based alphabet */` |
|   614630 |  7198 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   558770 |  7199 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   279386 |  7200 | `	 }` |
|    55862 |  7201 |  |
|        - |  7202 | `/*` |
|        - |  7203 | ` * int rand()` |
|        - |  7204 | ` * int mt_rand()` |
|        - |  7205 | ` * int rand(int $min,int $max)` |
|        - |  7206 | ` * int mt_rand(int $min,int $max)` |
|        - |  7207 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  7208 | ` * Parameter` |
|        - |  7209 | ` *  $min` |
|        - |  7210 | ` *    The lowest value to return (default: 0)` |
|        - |  7211 | ` *  $max` |
|        - |  7212 | ` *   The highest value to return (default: getrandmax())` |
|        - |  7213 | ` * Return` |
|        - |  7214 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  7215 | ` * Note:` |
|        - |  7216 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7217 | ` *  by te SQLite3 library.` |
|        - |  7218 | ` */` |
|       20 |  7219 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7220 |  |
|        - |  7221 | `	sxu32 iNum;` |
|        - |  7222 | `	/* Generate the random number */` |
|       21 |  7223 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  7224 | `	if( nArg > 1 ){` |
|        - |  7225 | `		sxu32 iMin,iMax;` |
|        3 |  7226 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  7227 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  7228 | `		if( iMin < iMax ){` |
|        3 |  7229 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  7230 | `			if( iDiv > 0 ){` |
|        3 |  7231 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  7232 | `			}` |
|        1 |  7233 | `		}else if(iMax > 0 ){` |
|      ! 0 |  7234 | `			iNum %= iMax;` |
|      ! 0 |  7235 | `		}` |
|        1 |  7236 | `	}` |
|        - |  7237 | `	/* Return the number */` |
|       21 |  7238 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  7239 | `	return SXRET_OK;` |
|        1 |  7240 |  |
|        - |  7241 | `/*` |
|        - |  7242 | ` * int getrandmax(void)` |
|        - |  7243 | ` * int mt_getrandmax(void)` |
|        - |  7244 | ` * int rc4_getrandmax(void)` |
|        - |  7245 | ` *   Show largest possible random value` |
|        - |  7246 | ` * Return` |
|        - |  7247 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  7248 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  7249 | ` * Note:` |
|        - |  7250 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7251 | ` *  by te SQLite3 library.` |
|        - |  7252 | ` */` |
|        4 |  7253 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7254 |  |
|        2 |  7255 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  7256 | `	SXUNUSED(apArg);` |
|        5 |  7257 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  7258 | `	return SXRET_OK;` |
|        1 |  7259 |  |
|        - |  7260 | `/*` |
|        - |  7261 | ` * string rand_str()` |
|        - |  7262 | ` * string rand_str(int $len)` |
|        - |  7263 | ` *  Generate a random string (English alphabet).` |
|        - |  7264 | ` * Parameter` |
|        - |  7265 | ` *  $len` |
|        - |  7266 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  7267 | ` * Return` |
|        - |  7268 | ` *   A pseudo random string.` |
|        - |  7269 | ` * Note:` |
|        - |  7270 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7271 | ` *  by te SQLite3 library.` |
|        - |  7272 | ` *  This function is a symisc extension.` |
|        - |  7273 | ` */` |
|      120 |  7274 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7275 |  |
|        - |  7276 | `	char zString[1024];` |
|      122 |  7277 | `	int iLen = 0x10;` |
|      122 |  7278 | `	if( nArg > 0 ){` |
|        - |  7279 | `		/* Get the desired length */` |
|      122 |  7280 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  7281 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  7282 | `			/* Default length */` |
|        3 |  7283 | `			iLen = 0x10;` |
|        1 |  7284 | `		}` |
|       60 |  7285 | `	}` |
|        - |  7286 | `	/* Generate the random string */` |
|      122 |  7287 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  7288 | `	/* Return the generated string */` |
|      122 |  7289 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  7290 | `	return SXRET_OK;` |
|        2 |  7291 |  |
|        - |  7292 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  7293 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  7294 | `/* Unique ID private data */` |
|        - |  7295 | `struct unique_id_data` |
|        - |  7296 |  |
|        - |  7297 | `	ph7_context *pCtx; /* Call context */` |
|        - |  7298 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  7299 | `};` |
|        - |  7300 | `/*` |
|        - |  7301 | ` * Binary to hex consumer callback.` |
|        - |  7302 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  7303 | ` * defined below.` |
|        - |  7304 | ` */` |
|      192 |  7305 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  7306 |  |
|      193 |  7307 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  7308 | `	sxu32 nBuflen;` |
|        - |  7309 | `	/* Extract result buffer length */` |
|      193 |  7310 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  7311 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  7312 | `			/*` |
|        - |  7313 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  7314 | `			 * string will be 13 characters long` |
|        - |  7315 | `			 */` |
|       25 |  7316 | `		return SXERR_ABORT;` |
|        - |  7317 | `	}` |
|      169 |  7318 | `	if( nBuflen > 22 ){` |
|      ! 0 |  7319 | `		return SXERR_ABORT;` |
|        - |  7320 | `	}` |
|        - |  7321 | `	/* Safely Consume the hex stream */` |
|      169 |  7322 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  7323 | `	return SXRET_OK;` |
|       97 |  7324 |  |
|        - |  7325 | `/*` |
|        - |  7326 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  7327 | ` *  Generate a unique ID` |
|        - |  7328 | ` * Parameter` |
|        - |  7329 | ` * $prefix` |
|        - |  7330 | ` *  Append this prefix to the generated unique ID.` |
|        - |  7331 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  7332 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  7333 | ` * $more_entropy` |
|        - |  7334 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  7335 | ` *  that the result will be unique.` |
|        - |  7336 | ` * Return` |
|        - |  7337 | ` *  Returns the unique identifier, as a string.` |
|        - |  7338 | ` */` |
|       24 |  7339 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7340 |  |
|        - |  7341 | `	struct unique_id_data sUniq;` |
|        - |  7342 | `	unsigned char zDigest[20];` |
|       25 |  7343 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7344 | `	const char *zPrefix;` |
|        - |  7345 | `	SHA1Context sCtx;` |
|        - |  7346 | `	char zRandom[7];` |
|        - |  7347 | `	int nPrefix;` |
|        - |  7348 | `	int entropy;` |
|        - |  7349 | `	/* Generate a random string first */` |
|       25 |  7350 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  7351 | `	/* Initialize fields */` |
|       25 |  7352 | `	zPrefix = 0;` |
|       25 |  7353 | `	nPrefix = 0;` |
|       25 |  7354 | `	entropy = 0;` |
|       25 |  7355 | `	if( nArg > 0 ){` |
|        - |  7356 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  7357 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  7358 | `		if( nArg > 1 ){` |
|      ! 0 |  7359 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  7360 | `		}` |
|      ! 0 |  7361 | `	}` |
|       25 |  7362 | `	SHA1Init(&sCtx);` |
|        - |  7363 | `	/* Generate the random ID */` |
|       25 |  7364 | `	if( nPrefix > 0 ){` |
|      ! 0 |  7365 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  7366 | `	}` |
|        - |  7367 | `	/* Append the random ID */` |
|       25 |  7368 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  7369 | `	/* Append the random string */` |
|       25 |  7370 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  7371 | `	/* Increment the number */` |
|       25 |  7372 | `	pVm->unique_id++;` |
|       25 |  7373 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  7374 | `	/* Hexify the digest */` |
|       25 |  7375 | `	sUniq.pCtx = pCtx;` |
|       25 |  7376 | `	sUniq.entropy = entropy;` |
|       25 |  7377 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  7378 | `	/* All done */` |
|       25 |  7379 | `	return PH7_OK;` |
|        1 |  7380 |  |
|        - |  7381 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  7382 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  7383 | `/*` |
|        - |  7384 | ` * Section:` |
|        - |  7385 | ` *  Language construct implementation as foreign functions.` |
|        - |  7386 | ` * Status:` |
|        - |  7387 | ` *    Stable.` |
|        - |  7388 | ` */` |
|        - |  7389 | `/*` |
|        - |  7390 | ` * void echo($string...)` |
|        - |  7391 | ` *  Output one or more messages.` |
|        - |  7392 | ` * Parameters` |
|        - |  7393 | ` *  $string` |
|        - |  7394 | ` *   Message to output.` |
|        - |  7395 | ` * Return` |
|        - |  7396 | ` *  NULL.` |
|        - |  7397 | ` */` |
|      ! 0 |  7398 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7399 |  |
|        - |  7400 | `	const char *zData;` |
|      ! 0 |  7401 | `	int nDataLen = 0;` |
|        - |  7402 | `	ph7_vm *pVm;` |
|        - |  7403 | `	int i,rc;` |
|        - |  7404 | `	/* Point to the target VM */` |
|      ! 0 |  7405 | `	pVm = pCtx->pVm;` |
|        - |  7406 | `	/* Output */` |
|      ! 0 |  7407 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  7408 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  7409 | `		if( nDataLen > 0 ){` |
|      ! 0 |  7410 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  7411 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7412 | `				/* Increment output length */` |
|      ! 0 |  7413 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  7414 | `			}` |
|      ! 0 |  7415 | `			if( rc == SXERR_ABORT ){` |
|        - |  7416 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7417 | `				return PH7_ABORT;` |
|        - |  7418 | `			}` |
|      ! 0 |  7419 | `		}` |
|      ! 0 |  7420 | `	}` |
|      ! 0 |  7421 | `	return SXRET_OK;` |
|      ! 0 |  7422 |  |
|        - |  7423 | `/*` |
|        - |  7424 | ` * int print($string...)` |
|        - |  7425 | ` *  Output one or more messages.` |
|        - |  7426 | ` * Parameters` |
|        - |  7427 | ` *  $string` |
|        - |  7428 | ` *   Message to output.` |
|        - |  7429 | ` * Return` |
|        - |  7430 | ` *  1 always.` |
|        - |  7431 | ` */` |
|        2 |  7432 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7433 |  |
|        - |  7434 | `	const char *zData;` |
|        3 |  7435 | `	int nDataLen = 0;` |
|        - |  7436 | `	ph7_vm *pVm;` |
|        - |  7437 | `	int i,rc;` |
|        - |  7438 | `	/* Point to the target VM */` |
|        3 |  7439 | `	pVm = pCtx->pVm;` |
|        - |  7440 | `	/* Output */` |
|        5 |  7441 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  7442 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  7443 | `		if( nDataLen > 0 ){` |
|        3 |  7444 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  7445 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7446 | `				/* Increment output length */` |
|        3 |  7447 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  7448 | `			}` |
|        3 |  7449 | `			if( rc == SXERR_ABORT ){` |
|        - |  7450 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7451 | `				return PH7_ABORT;` |
|        - |  7452 | `			}` |
|        1 |  7453 | `		}` |
|        2 |  7454 | `	}` |
|        - |  7455 | `	/* Return 1 */` |
|        3 |  7456 | `	ph7_result_int(pCtx,1);` |
|        3 |  7457 | `	return SXRET_OK;` |
|        2 |  7458 |  |
|        - |  7459 | `/*` |
|        - |  7460 | ` * void exit(string $msg)` |
|        - |  7461 | ` * void exit(int $status)` |
|        - |  7462 | ` * void die(string $ms)` |
|        - |  7463 | ` * void die(int $status)` |
|        - |  7464 | ` *   Output a message and terminate program execution.` |
|        - |  7465 | ` * Parameter` |
|        - |  7466 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  7467 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  7468 | ` *  and not printed` |
|        - |  7469 | ` * Return` |
|        - |  7470 | ` *  NULL` |
|        - |  7471 | ` */` |
|      ! 0 |  7472 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7473 |  |
|      ! 0 |  7474 | `	if( nArg > 0 ){` |
|      ! 0 |  7475 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  7476 | `			const char *zData;` |
|      ! 0 |  7477 | `			int iLen = 0;` |
|        - |  7478 | `			/* Print exit message */` |
|      ! 0 |  7479 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  7480 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  7481 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  7482 | `			sxi32 iExitStatus;` |
|        - |  7483 | `			/* Record exit status code */` |
|      ! 0 |  7484 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  7485 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  7486 | `		}` |
|      ! 0 |  7487 | `	}` |
|        - |  7488 | `	/* Check if we are in an included file */` |
|      ! 0 |  7489 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  7490 | `		/* Exit the entire process */` |
|      ! 0 |  7491 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  7492 | `	}` |
|        - |  7493 | `	/* Abort processing immediately */` |
|      ! 0 |  7494 | `	return PH7_ABORT;` |
|      ! 0 |  7495 |  |
|        - |  7496 | `/*` |
|        - |  7497 | ` * bool isset($var,...)` |
|        - |  7498 | ` *  Finds out whether a variable is set.` |
|        - |  7499 | ` * Parameters` |
|        - |  7500 | ` *  One or more variable to check.` |
|        - |  7501 | ` * Return` |
|        - |  7502 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  7503 | ` */` |
|    65494 |  7504 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7505 |  |
|        - |  7506 | `	ph7_value *pObj;` |
|    65496 |  7507 | `	int res = 0;` |
|        - |  7508 | `	int i;` |
|    65496 |  7509 | `	if( nArg < 1 ){` |
|        - |  7510 | `		/* Missing arguments,return false */` |
|      ! 0 |  7511 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  7512 | `		return SXRET_OK;` |
|        - |  7513 | `	}` |
|        - |  7514 | `	/* Iterate over available arguments */` |
|    86862 |  7515 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    65496 |  7516 | `		pObj = apArg[i];` |
|    65496 |  7517 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    43748 |  7518 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7519 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  7520 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  7521 | `			}` |
|    21873 |  7522 | `		}` |
|    65496 |  7523 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    65496 |  7524 | `		if( !res ){` |
|        - |  7525 | `			/* Variable not set,return FALSE */` |
|    44130 |  7526 | `			ph7_result_bool(pCtx,0);` |
|    44130 |  7527 | `			return SXRET_OK;` |
|        - |  7528 | `		}` |
|    10685 |  7529 | `	}` |
|        - |  7530 | `	/* All given variable are set,return TRUE */` |
|    21368 |  7531 | `	ph7_result_bool(pCtx,1);` |
|    21368 |  7532 | `	return SXRET_OK;` |
|    32749 |  7533 |  |
|        - |  7534 | `/*` |
|        - |  7535 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  7536 | ` * frame,the reference table and discard it's contents.` |
|        - |  7537 | ` * This function never fail and always return SXRET_OK.` |
|        - |  7538 | ` */` |
|  2919050 |  7539 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  7540 |  |
|        - |  7541 | `	ph7_value *pObj;` |
|        - |  7542 | `	VmRefObj *pRef;` |
|  2919052 |  7543 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2919052 |  7544 | `	if( pObj ){` |
|        - |  7545 | `		/* Release the object */` |
|  2919052 |  7546 | `		PH7_MemObjRelease(pObj);` |
|  1459525 |  7547 | `	}` |
|        - |  7548 | `	/* Remove old reference links */` |
|  2919052 |  7549 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2919052 |  7550 | `	if( pRef ){` |
|  2919032 |  7551 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  7552 | `		/* Unlink from the reference table */` |
|  2919032 |  7553 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2919032 |  7554 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  7555 | `			VmSlot sFree;` |
|        - |  7556 | `			/* Restore to the free list */` |
|  2919026 |  7557 | `			sFree.nIdx = nObjIdx;` |
|  2919026 |  7558 | `			sFree.pUserData = 0;` |
|  2919026 |  7559 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1459512 |  7560 | `		}` |
|  1459515 |  7561 | `	}` |
|  2919052 |  7562 | `	return SXRET_OK;` |
|        2 |  7563 |  |
|        - |  7564 | `/*` |
|        - |  7565 | ` * void unset($var,...)` |
|        - |  7566 | ` *   Unset one or more given variable.` |
|        - |  7567 | ` * Parameters` |
|        - |  7568 | ` *  One or more variable to unset.` |
|        - |  7569 | ` * Return` |
|        - |  7570 | ` *  Nothing.` |
|        - |  7571 | ` */` |
|     3178 |  7572 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7573 |  |
|        - |  7574 | `	ph7_value *pObj;` |
|        - |  7575 | `	ph7_vm *pVm;` |
|        - |  7576 | `	int i;` |
|        - |  7577 | `	/* Point to the target VM */` |
|     3180 |  7578 | `	pVm = pCtx->pVm;` |
|        - |  7579 | `	/* Iterate and unset */` |
|     9504 |  7580 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6326 |  7581 | `		pObj = apArg[i];` |
|     6326 |  7582 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      812 |  7583 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7584 | `				/* Throw an error */` |
|      ! 0 |  7585 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  7586 | `			}` |
|      407 |  7587 | `		}else{` |
|     5515 |  7588 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  7589 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     5515 |  7590 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     5509 |  7591 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2754 |  7592 | `			}` |
|        - |  7593 | `		}` |
|     3164 |  7594 | `	}` |
|     3180 |  7595 | `	return SXRET_OK;` |
|        2 |  7596 |  |
|        - |  7597 | `/*` |
|        - |  7598 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  7599 | ` */` |
|      110 |  7600 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7601 |  |
|      111 |  7602 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  7603 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  7604 | `	ph7_value *pObj;` |
|        - |  7605 | `	sxu32 nIdx;` |
|        - |  7606 | `	/* Extract the memory object */` |
|      111 |  7607 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  7608 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  7609 | `	if( pObj ){` |
|      111 |  7610 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  7611 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  7612 | `				SyString sName;` |
|        - |  7613 | `				ph7_value sKey;` |
|        - |  7614 | `				/* Perform the insertion */` |
|      109 |  7615 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  7616 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  7617 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  7618 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  7619 | `			}` |
|       54 |  7620 | `		}` |
|       55 |  7621 | `	}` |
|      111 |  7622 | `	return SXRET_OK;` |
|        1 |  7623 |  |
|        - |  7624 | `/*` |
|        - |  7625 | ` * array get_defined_vars(void)` |
|        - |  7626 | ` *  Returns an array of all defined variables.` |
|        - |  7627 | ` * Parameter` |
|        - |  7628 | ` *  None` |
|        - |  7629 | ` * Return` |
|        - |  7630 | ` *  An array with all the variables defined in the current scope.` |
|        - |  7631 | ` */` |
|        2 |  7632 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7633 |  |
|        3 |  7634 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7635 | `	ph7_value *pArray;` |
|        - |  7636 | `	/* Create a new array */` |
|        3 |  7637 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7638 | ` 	if( pArray == 0 ){` |
|      ! 0 |  7639 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7640 | `		SXUNUSED(apArg);` |
|        - |  7641 | `		/* Return NULL */` |
|      ! 0 |  7642 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7643 | `		return SXRET_OK;` |
|        - |  7644 | `	}` |
|        - |  7645 | `	/* Superglobals first */` |
|        3 |  7646 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  7647 | `	/* Then variable defined in the current frame */` |
|        3 |  7648 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  7649 | `	/* Finally,return the created array */` |
|        3 |  7650 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7651 | `	return SXRET_OK;` |
|        2 |  7652 |  |
|        - |  7653 | `/*` |
|        - |  7654 | ` * bool gettype($var)` |
|        - |  7655 | ` *  Get the type of a variable` |
|        - |  7656 | ` * Parameters` |
|        - |  7657 | ` *   $var` |
|        - |  7658 | ` *    The variable being type checked.` |
|        - |  7659 | ` * Return` |
|        - |  7660 | ` *   String representation of the given variable type.` |
|        - |  7661 | ` */` |
|       32 |  7662 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7663 |  |
|       34 |  7664 | `	const char *zType = "Empty";` |
|       34 |  7665 | `	if( nArg > 0 ){` |
|       34 |  7666 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  7667 | `	}` |
|        - |  7668 | `	/* Return the variable type */` |
|       34 |  7669 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  7670 | `	return SXRET_OK;` |
|        2 |  7671 |  |
|        - |  7672 | `/*` |
|        - |  7673 | ` * string get_resource_type(resource $handle)` |
|        - |  7674 | ` *  This function gets the type of the given resource.` |
|        - |  7675 | ` * Parameters` |
|        - |  7676 | ` *  $handle` |
|        - |  7677 | ` *  The evaluated resource handle.` |
|        - |  7678 | ` * Return` |
|        - |  7679 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  7680 | ` *  representing its type. If the type is not identified by this function` |
|        - |  7681 | ` *  the return value will be the string Unknown.` |
|        - |  7682 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  7683 | ` *  is not a resource.` |
|        - |  7684 | ` */` |
|        2 |  7685 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7686 |  |
|        3 |  7687 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  7688 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  7689 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7690 | `		return PH7_OK;` |
|        - |  7691 | `	}` |
|        3 |  7692 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  7693 | `	return SXRET_OK;` |
|        2 |  7694 |  |
|        - |  7695 | `/*` |
|        - |  7696 | ` * void var_dump(expression,....)` |
|        - |  7697 | ` *   var_dump � Dumps information about a variable` |
|        - |  7698 | ` * Parameters` |
|        - |  7699 | ` *   One or more expression to dump.` |
|        - |  7700 | ` * Returns` |
|        - |  7701 | ` *  Nothing.` |
|        - |  7702 | ` */` |
|      218 |  7703 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7704 |  |
|        - |  7705 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  7706 | `	int i;` |
|      220 |  7707 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  7708 | `	/* Dump one or more expressions */` |
|      444 |  7709 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  7710 | `		ph7_value *pObj = apArg[i];` |
|        - |  7711 | `		/* Reset the working buffer */` |
|      226 |  7712 | `		SyBlobReset(&sDump);` |
|        - |  7713 | `		/* Dump the given expression */` |
|      226 |  7714 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  7715 | `		/* Output */` |
|      226 |  7716 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  7717 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  7718 | `		}` |
|      114 |  7719 | `	}` |
|        - |  7720 | `	/* Release the working buffer */` |
|      220 |  7721 | `	SyBlobRelease(&sDump);` |
|      220 |  7722 | `	return SXRET_OK;` |
|        2 |  7723 |  |
|        - |  7724 | `/*` |
|        - |  7725 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  7726 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  7727 | ` * Parameters` |
|        - |  7728 | ` *   expression: Expression to dump` |
|        - |  7729 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  7730 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  7731 | ` *            print_r() will return the information rather than print it.` |
|        - |  7732 | ` * Return` |
|        - |  7733 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  7734 | ` *  Otherwise, the return value is TRUE.` |
|        - |  7735 | ` */` |
|       16 |  7736 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7737 |  |
|       17 |  7738 | `	int ret_string = 0;` |
|        - |  7739 | `	SyBlob sDump;` |
|       17 |  7740 | `	if( nArg < 1 ){` |
|        - |  7741 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  7742 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7743 | `		return SXRET_OK;` |
|        - |  7744 | `	}` |
|       17 |  7745 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  7746 | `	if ( nArg > 1 ){` |
|        - |  7747 | `		/* Where to redirect output */` |
|       11 |  7748 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  7749 | `	}` |
|        - |  7750 | `	/* Generate dump */` |
|       17 |  7751 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  7752 | `	if( !ret_string ){` |
|        - |  7753 | `		/* Output dump */` |
|        7 |  7754 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7755 | `		/* Return true */` |
|        7 |  7756 | `		ph7_result_bool(pCtx,1);` |
|        4 |  7757 | `	}else{` |
|        - |  7758 | `		/* Generated dump as return value */` |
|       11 |  7759 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7760 | `	}` |
|        - |  7761 | `	/* Release the working buffer */` |
|       17 |  7762 | `	SyBlobRelease(&sDump);` |
|       17 |  7763 | `	return SXRET_OK;` |
|        9 |  7764 |  |
|        - |  7765 | `/*` |
|        - |  7766 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  7767 | ` * Same job as print_r. (see coment above)` |
|        - |  7768 | ` */` |
|        2 |  7769 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7770 |  |
|        3 |  7771 | `	int ret_string = 0;` |
|        - |  7772 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  7773 | `	if( nArg < 1 ){` |
|        - |  7774 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  7775 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7776 | `		return SXRET_OK;` |
|        - |  7777 | `	}` |
|        3 |  7778 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  7779 | `	if ( nArg > 1 ){` |
|        - |  7780 | `		/* Where to redirect output */` |
|        3 |  7781 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  7782 | `	}` |
|        - |  7783 | `	/* Generate dump */` |
|        3 |  7784 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  7785 | `	if( !ret_string ){` |
|        - |  7786 | `		/* Output dump */` |
|      ! 0 |  7787 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7788 | `		/* Return NULL */` |
|      ! 0 |  7789 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7790 | `	}else{` |
|        - |  7791 | `		/* Generated dump as return value */` |
|        3 |  7792 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7793 | `	}` |
|        - |  7794 | `	/* Release the working buffer */` |
|        3 |  7795 | `	SyBlobRelease(&sDump);` |
|        3 |  7796 | `	return SXRET_OK;` |
|        2 |  7797 |  |
|        - |  7798 | `/*` |
|        - |  7799 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  7800 | ` *  Set/get the various assert flags.` |
|        - |  7801 | ` * Parameter` |
|        - |  7802 | ` * $what` |
|        - |  7803 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  7804 | ` *   ASSERT_WARNING         Issue a warning for each failed assertion` |
|        - |  7805 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  7806 | ` *   ASSERT_QUIET_EVAL      Not used` |
|        - |  7807 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  7808 | ` * $value` |
|        - |  7809 | ` *   An optional new value for the option.` |
|        - |  7810 | ` * Return` |
|        - |  7811 | ` *  Old setting on success or FALSE on failure.` |
|        - |  7812 | ` */` |
|        8 |  7813 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7814 |  |
|        9 |  7815 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7816 | `	int iOld,iNew,iValue;` |
|        9 |  7817 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|        - |  7818 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  7819 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7820 | `		return PH7_OK;` |
|        - |  7821 | `	}` |
|        - |  7822 | `	/* Save old assertion flags */` |
|        9 |  7823 | `	iOld = pVm->iAssertFlags;` |
|        - |  7824 | `	/* Extract the new flags */` |
|        9 |  7825 | `	iNew = ph7_value_to_int(apArg[0]);` |
|        9 |  7826 | `	if( iNew == PH7_ASSERT_DISABLE ){` |
|        7 |  7827 | `		pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        7 |  7828 | `		if( nArg > 1 ){` |
|        5 |  7829 | `			iValue = !ph7_value_to_bool(apArg[1]);` |
|        5 |  7830 | `			if( iValue ){` |
|        - |  7831 | `				/* Disable assertion */` |
|        3 |  7832 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        1 |  7833 | `			}` |
|        3 |  7834 | `		}` |
|        6 |  7835 | `	}else if( iNew == PH7_ASSERT_WARNING ){` |
|      ! 0 |  7836 | `		pVm->iAssertFlags &= ~PH7_ASSERT_WARNING;` |
|      ! 0 |  7837 | `		if( nArg > 1 ){` |
|      ! 0 |  7838 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  7839 | `			if( iValue ){` |
|        - |  7840 | `				/* Issue a warning for each failed assertion */` |
|      ! 0 |  7841 | `				pVm->iAssertFlags \|= PH7_ASSERT_WARNING;` |
|      ! 0 |  7842 | `			}` |
|      ! 0 |  7843 | `		}` |
|        3 |  7844 | `	}else if( iNew == PH7_ASSERT_BAIL ){` |
|        3 |  7845 | `		pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        3 |  7846 | `		if( nArg > 1 ){` |
|        3 |  7847 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|        3 |  7848 | `			if( iValue ){` |
|        - |  7849 | `				/* Terminate execution on failed assertions */` |
|        3 |  7850 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        1 |  7851 | `			}` |
|        2 |  7852 | `		}` |
|        1 |  7853 | `	}else if( iNew == PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  7854 | `		pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|      ! 0 |  7855 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|        - |  7856 | `			/* Callback to call on failed assertions */` |
|      ! 0 |  7857 | `			PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  7858 | `			pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  7859 | `		}` |
|      ! 0 |  7860 | `	}` |
|        - |  7861 | `	/* Return the old flags */` |
|        9 |  7862 | `	ph7_result_int(pCtx,iOld);` |
|        9 |  7863 | `	return PH7_OK;` |
|        5 |  7864 |  |
|        - |  7865 | `/*` |
|        - |  7866 | ` * bool assert(mixed $assertion)` |
|        - |  7867 | ` *  Checks if assertion is FALSE.` |
|        - |  7868 | ` * Parameter` |
|        - |  7869 | ` *  $assertion` |
|        - |  7870 | ` *    The assertion to test.` |
|        - |  7871 | ` * Return` |
|        - |  7872 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  7873 | ` */` |
|       14 |  7874 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7875 |  |
|       15 |  7876 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7877 | `	ph7_value *pAssert;` |
|        - |  7878 | `	int iFlags,iResult;` |
|       15 |  7879 | `	if( nArg < 1 ){` |
|        - |  7880 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  7881 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7882 | `		return PH7_OK;` |
|        - |  7883 | `	}` |
|       15 |  7884 | `	iFlags = pVm->iAssertFlags;` |
|       15 |  7885 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  7886 | `		/* Assertion is disabled,return FALSE */` |
|      ! 0 |  7887 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7888 | `		return PH7_OK;` |
|        - |  7889 | `	}` |
|       15 |  7890 | `	pAssert = apArg[0];` |
|       15 |  7891 | `	iResult = 1; /* cc warning */` |
|       15 |  7892 | `	if( pAssert->iFlags & MEMOBJ_STRING ){` |
|        - |  7893 | `		SyString sChunk;` |
|        7 |  7894 | `		SyStringInitFromBuf(&sChunk,SyBlobData(&pAssert->sBlob),SyBlobLength(&pAssert->sBlob));` |
|        7 |  7895 | `		if( sChunk.nByte > 0 ){` |
|        5 |  7896 | `			VmEvalChunk(pVm,pCtx,&sChunk,PH7_PHP_ONLY\|PH7_PHP_EXPR,FALSE);` |
|        - |  7897 | `			/* Extract evaluation result */` |
|        5 |  7898 | `			iResult = ph7_value_to_bool(pCtx->pRet);` |
|        3 |  7899 | `		}else{` |
|        3 |  7900 | `			iResult = 0;` |
|        - |  7901 | `		}` |
|        4 |  7902 | `	}else{` |
|        - |  7903 | `		/* Perform a boolean cast */` |
|        9 |  7904 | `		iResult = ph7_value_to_bool(apArg[0]);` |
|        - |  7905 | `	}` |
|       15 |  7906 | `	if( !iResult ){` |
|        - |  7907 | `		/* Assertion failed */` |
|        9 |  7908 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  7909 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  7910 | `			ph7_value sFile,sLine;` |
|        - |  7911 | `			ph7_value *apCbArg[3];` |
|        - |  7912 | `			SyString *pFile;` |
|        - |  7913 | `			/* Extract the processed script */` |
|      ! 0 |  7914 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  7915 | `			if( pFile == 0 ){` |
|      ! 0 |  7916 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  7917 | `			}` |
|        - |  7918 | `			/* Invoke the callback */` |
|      ! 0 |  7919 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  7920 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  7921 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  7922 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  7923 | `			apCbArg[2] = pAssert;` |
|      ! 0 |  7924 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  7925 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  7926 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  7927 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  7928 | `		}` |
|        9 |  7929 | `		if( iFlags & PH7_ASSERT_WARNING ){` |
|        - |  7930 | `			/* Emit a warning */` |
|        9 |  7931 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Assertion failed");` |
|        4 |  7932 | `		}` |
|        9 |  7933 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  7934 | `			/* Abort VM execution immediately */` |
|        3 |  7935 | `			return PH7_ABORT;` |
|        - |  7936 | `		}` |
|        3 |  7937 | `	}` |
|        - |  7938 | `	/* Assertion result */` |
|       13 |  7939 | `	ph7_result_bool(pCtx,iResult);` |
|       13 |  7940 | `	return PH7_OK;` |
|        8 |  7941 |  |
|        - |  7942 | `/*` |
|        - |  7943 | ` * Section:` |
|        - |  7944 | ` *  Error reporting functions.` |
|        - |  7945 | ` * Status:` |
|        - |  7946 | ` *    Stable.` |
|        - |  7947 | ` */` |
|        - |  7948 | `/*` |
|        - |  7949 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  7950 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  7951 | ` * Parameters` |
|        - |  7952 | ` *  $error_msg` |
|        - |  7953 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  7954 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  7955 | ` * $error_type` |
|        - |  7956 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  7957 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  7958 | ` * Return` |
|        - |  7959 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  7960 | ` */` |
|       12 |  7961 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7962 |  |
|       14 |  7963 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  7964 | `	int rc = PH7_OK;` |
|       14 |  7965 | `	if( nArg > 0 ){` |
|        - |  7966 | `		const char *zErr;` |
|        - |  7967 | `		int nLen;` |
|        - |  7968 | `		/* Extract the error message */` |
|       12 |  7969 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7970 | `		if( nArg > 1 ){` |
|        - |  7971 | `			/* Extract the error type */` |
|       12 |  7972 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  7973 | `			switch( nErr ){` |
|        1 |  7974 | `			case 1:   /* E_ERROR */` |
|        - |  7975 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  7976 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  7977 | `			case 256: /* E_USER_ERROR */` |
|        3 |  7978 | `				nErr = PH7_CTX_ERR;` |
|        3 |  7979 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  7980 | `				break;` |
|        1 |  7981 | `			case 2:   /* E_WARNING */` |
|        - |  7982 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  7983 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  7984 | `			case 512: /* E_USER_WARNING */` |
|        3 |  7985 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  7986 | `				break;` |
|        3 |  7987 | `			default:` |
|        8 |  7988 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  7989 | `				break;` |
|        - |  7990 | `			}` |
|        5 |  7991 | `		}` |
|        - |  7992 | `		/* Report error */` |
|       12 |  7993 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  7994 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  7995 | `			return rc;` |
|        - |  7996 | `		}` |
|        - |  7997 | `		/* Return true */` |
|       12 |  7998 | `		ph7_result_bool(pCtx,1);` |
|        7 |  7999 | `	}else{` |
|        - |  8000 | `		/* Missing arguments,return FALSE */` |
|        3 |  8001 | `		ph7_result_bool(pCtx,0);` |
|        - |  8002 | `	}` |
|       14 |  8003 | `	return rc;` |
|        8 |  8004 |  |
|        - |  8005 | `/*` |
|        - |  8006 | ` * int error_reporting([int $level])` |
|        - |  8007 | ` *  Sets which PHP errors are reported.` |
|        - |  8008 | ` * Parameters` |
|        - |  8009 | ` *  $level` |
|        - |  8010 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  8011 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  8012 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  8013 | ` *   levels will not always behave as expected.` |
|        - |  8014 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  8015 | ` *   in the predefined constants.` |
|        - |  8016 | ` * Return` |
|        - |  8017 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  8018 | ` *   parameter is given.` |
|        - |  8019 | ` */` |
|       18 |  8020 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8021 |  |
|       19 |  8022 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8023 | `	int nOld;` |
|        - |  8024 | `	/* Extract the old reporting level */` |
|       19 |  8025 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       19 |  8026 | `	if( nArg > 0 ){` |
|        - |  8027 | `		int nNew;` |
|        - |  8028 | `		/* Extract the desired error reporting level */` |
|       11 |  8029 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       11 |  8030 | `		if( !nNew ){` |
|        - |  8031 | `			/* Do not report errors at all */` |
|        5 |  8032 | `			pVm->bErrReport = 0;` |
|        3 |  8033 | `		}else{` |
|        - |  8034 | `			/* Report all errors */` |
|        7 |  8035 | `			pVm->bErrReport = 1;` |
|        - |  8036 | `		}` |
|        5 |  8037 | `	}` |
|        - |  8038 | `	/* Return the old level */` |
|       19 |  8039 | `	ph7_result_int(pCtx,nOld);` |
|       19 |  8040 | `	return PH7_OK;` |
|        1 |  8041 |  |
|        - |  8042 | `/*` |
|        - |  8043 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  8044 | ` *  Send an error message somewhere.` |
|        - |  8045 | ` * Parameter` |
|        - |  8046 | ` *  $message` |
|        - |  8047 | ` *   The error message that should be logged.` |
|        - |  8048 | ` *  $message_type` |
|        - |  8049 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  8050 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  8051 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  8052 | ` *       This is the default option.` |
|        - |  8053 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  8054 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  8055 | ` *    2  No longer an option.` |
|        - |  8056 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  8057 | ` *       to the end of the message string.` |
|        - |  8058 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  8059 | ` *  $destination` |
|        - |  8060 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  8061 | ` *  $extra_headers` |
|        - |  8062 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  8063 | ` * Return` |
|        - |  8064 | ` *  TRUE on success or FALSE on failure.` |
|        - |  8065 | ` * NOTE:` |
|        - |  8066 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  8067 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  8068 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  8069 | ` *  Otherwise this function is no-op.` |
|        - |  8070 | ` */` |
|        4 |  8071 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8072 |  |
|        - |  8073 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  8074 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  8075 | `	int iType = 0;` |
|        5 |  8076 | `	if( nArg < 1 ){` |
|        - |  8077 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  8078 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8079 | `		return PH7_OK;` |
|        - |  8080 | `	}` |
|        5 |  8081 | `	if( pVm->xErrLog  ){` |
|        - |  8082 | `		/* Invoke the user callback */` |
|      ! 0 |  8083 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  8084 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  8085 | `		if( nArg > 1 ){` |
|      ! 0 |  8086 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  8087 | `			if( nArg > 2 ){` |
|      ! 0 |  8088 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  8089 | `				if( nArg > 3 ){` |
|      ! 0 |  8090 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  8091 | `				}` |
|      ! 0 |  8092 | `			}` |
|      ! 0 |  8093 | `		}` |
|      ! 0 |  8094 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  8095 | `	}` |
|        - |  8096 | `	/* Retun TRUE */` |
|        5 |  8097 | `	ph7_result_bool(pCtx,1);` |
|        5 |  8098 | `	return PH7_OK;` |
|        3 |  8099 |  |
|        - |  8100 | `/*` |
|        - |  8101 | ` * bool restore_exception_handler(void)` |
|        - |  8102 | ` *  Restores the previously defined exception handler function.` |
|        - |  8103 | ` * Parameter` |
|        - |  8104 | ` *  None` |
|        - |  8105 | ` * Return` |
|        - |  8106 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  8107 | ` */` |
|        4 |  8108 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8109 |  |
|        5 |  8110 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8111 | `	ph7_value *pOld,*pNew;` |
|        - |  8112 | `	/* Point to the old and the new handler */` |
|        5 |  8113 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  8114 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  8115 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8116 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8117 | `		SXUNUSED(apArg);` |
|        - |  8118 | `		/* No installed handler,return FALSE */` |
|        5 |  8119 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8120 | `		return PH7_OK;` |
|        - |  8121 | `	}` |
|        - |  8122 | `	/* Copy the old handler */` |
|      ! 0 |  8123 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8124 | `	PH7_MemObjRelease(pOld);` |
|        - |  8125 | `	/* Return TRUE */` |
|      ! 0 |  8126 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8127 | `	return PH7_OK;` |
|        3 |  8128 |  |
|        - |  8129 | `/*` |
|        - |  8130 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  8131 | ` *  Sets a user-defined exception handler function.` |
|        - |  8132 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  8133 | ` * NOTE` |
|        - |  8134 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  8135 | ` *  the satndard PHP engine.` |
|        - |  8136 | ` * Parameters` |
|        - |  8137 | ` *  $exception_handler` |
|        - |  8138 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  8139 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  8140 | ` *   that was thrown.` |
|        - |  8141 | ` *  Note:` |
|        - |  8142 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8143 | ` * Return` |
|        - |  8144 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  8145 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8146 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8147 | ` */` |
|        4 |  8148 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8149 |  |
|        6 |  8150 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8151 | `	ph7_value *pOld,*pNew;` |
|        - |  8152 | `	/* Point to the old and the new handler */` |
|        6 |  8153 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  8154 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  8155 | `	/* Return the old handler */` |
|        6 |  8156 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  8157 | `	if( nArg > 0 ){` |
|        6 |  8158 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8159 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  8160 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  8161 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  8162 | `		}else{` |
|        6 |  8163 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8164 | `			/* Install the new handler */` |
|        6 |  8165 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8166 | `		}` |
|        2 |  8167 | `	}` |
|        6 |  8168 | `	return PH7_OK;` |
|        2 |  8169 |  |
|        - |  8170 | `/*` |
|        - |  8171 | ` * bool restore_error_handler(void)` |
|        - |  8172 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8173 | ` * Parameters:` |
|        - |  8174 | ` *  None.` |
|        - |  8175 | ` * Return` |
|        - |  8176 | ` *  Always TRUE.` |
|        - |  8177 | ` */` |
|        4 |  8178 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8179 |  |
|        5 |  8180 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8181 | `	ph7_value *pOld,*pNew;` |
|        - |  8182 | `	/* Point to the old and the new handler */` |
|        5 |  8183 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  8184 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  8185 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8186 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8187 | `		SXUNUSED(apArg);` |
|        - |  8188 | `		/* No installed callback,return FALSE */` |
|        5 |  8189 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8190 | `		return PH7_OK;` |
|        - |  8191 | `	}` |
|        - |  8192 | `	/* Copy the old callback */` |
|      ! 0 |  8193 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8194 | `	PH7_MemObjRelease(pOld);` |
|        - |  8195 | `	/* Return TRUE */` |
|      ! 0 |  8196 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8197 | `	return PH7_OK;` |
|        3 |  8198 |  |
|        - |  8199 | `/*` |
|        - |  8200 | ` * value set_error_handler(callable $error_handler)` |
|        - |  8201 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8202 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8203 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8204 | ` *  Sets a user-defined error handler function.` |
|        - |  8205 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  8206 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  8207 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  8208 | ` *  conditions (using trigger_error()).` |
|        - |  8209 | ` * Parameters` |
|        - |  8210 | ` *  $error_handler` |
|        - |  8211 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  8212 | ` *   describing the error.` |
|        - |  8213 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  8214 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  8215 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  8216 | ` *   The function can be shown as:` |
|        - |  8217 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  8218 | ` *     errno` |
|        - |  8219 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  8220 | ` *   errstr` |
|        - |  8221 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  8222 | ` *   errfile` |
|        - |  8223 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  8224 | ` *     was raised in, as a string.` |
|        - |  8225 | ` *  Note:` |
|        - |  8226 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8227 | ` * Return` |
|        - |  8228 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  8229 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8230 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8231 | ` */` |
|     8690 |  8232 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8233 |  |
|     8692 |  8234 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8235 | `	ph7_value *pOld,*pNew;` |
|        - |  8236 | `	/* Point to the old and the new handler */` |
|     8692 |  8237 | `	pOld = &pVm->aErrCB[0];` |
|     8692 |  8238 | `	pNew = &pVm->aErrCB[1];` |
|        - |  8239 | `	/* Return the old handler */` |
|     8692 |  8240 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8692 |  8241 | `	if( nArg > 0 ){` |
|     8692 |  8242 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8243 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4345 |  8244 | `			PH7_MemObjRelease(pNew);` |
|     4345 |  8245 | `			ph7_result_bool(pCtx,1);` |
|     2173 |  8246 | `		}else{` |
|     4348 |  8247 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8248 | `			/* Install the new handler */` |
|     4348 |  8249 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8250 | `		}` |
|     4345 |  8251 | `	}` |
|     8692 |  8252 | `	return PH7_OK;` |
|        2 |  8253 |  |
|        - |  8254 | `/*` |
|        - |  8255 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  8256 | ` *  Generates a backtrace.` |
|        - |  8257 | ` * Paramaeter` |
|        - |  8258 | ` *  $options` |
|        - |  8259 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  8260 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  8261 | ` *   all the function/method arguments, to save memory.` |
|        - |  8262 | ` * $limit` |
|        - |  8263 | ` *   (Not Used)` |
|        - |  8264 | ` * Return` |
|        - |  8265 | ` *  An array.The possible returned elements are as follows:` |
|        - |  8266 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  8267 | ` *          Name        Type      Description` |
|        - |  8268 | ` *          ------      ------     -----------` |
|        - |  8269 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  8270 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  8271 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  8272 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  8273 | ` *          object      object    The current object.` |
|        - |  8274 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  8275 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  8276 | ` */` |
|      378 |  8277 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8278 |  |
|      380 |  8279 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8280 | `	ph7_value *pArray;` |
|        - |  8281 | `	ph7_class *pClass;` |
|        - |  8282 | `	ph7_value *pValue;` |
|        - |  8283 | `	SyString *pFile;` |
|        - |  8284 | `	/* Create a new array */` |
|      380 |  8285 | `	pArray = ph7_context_new_array(pCtx);` |
|      380 |  8286 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      380 |  8287 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8288 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8289 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  8290 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8291 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8292 | `		SXUNUSED(apArg);` |
|      ! 0 |  8293 | `		return PH7_OK;` |
|        - |  8294 | `	}` |
|        - |  8295 | `	/* Dump running function name and it's arguments  */` |
|      380 |  8296 | `	if( pVm->pFrame->pParent ){` |
|      380 |  8297 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8298 | `		ph7_vm_func *pFunc;` |
|        - |  8299 | `		ph7_value *pArg;` |
|      380 |  8300 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8301 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  8302 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  8303 | `		}` |
|      380 |  8304 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      380 |  8305 | `		if( pFrame->pParent && pFunc ){` |
|      380 |  8306 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      380 |  8307 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      380 |  8308 | `			ph7_value_reset_string_cursor(pValue);` |
|      189 |  8309 | `		}` |
|        - |  8310 | `		/* Function arguments */` |
|      380 |  8311 | `		pArg = ph7_context_new_array(pCtx);` |
|      380 |  8312 | `		if( pArg  ){` |
|        - |  8313 | `			ph7_value *pObj;` |
|        - |  8314 | `			VmSlot *aSlot;` |
|        - |  8315 | `			sxu32 n;` |
|        - |  8316 | `			/* Start filling the array with the given arguments */` |
|      380 |  8317 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     1506 |  8318 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1128 |  8319 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1128 |  8320 | `				if( pObj ){` |
|     1128 |  8321 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      563 |  8322 | `				}` |
|      565 |  8323 | `			}` |
|        - |  8324 | `			/* Save the array */` |
|      380 |  8325 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      189 |  8326 | `		}` |
|      189 |  8327 | `	}` |
|      380 |  8328 | `	ph7_value_int(pValue,1);` |
|        - |  8329 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  8330 | `	 * line numbers at run-time. )` |
|        - |  8331 | `	 */` |
|      380 |  8332 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  8333 | `	/* Current processed script */` |
|      380 |  8334 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      380 |  8335 | `	if( pFile ){` |
|      380 |  8336 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      380 |  8337 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      380 |  8338 | `		ph7_value_reset_string_cursor(pValue);` |
|      189 |  8339 | `	}` |
|        - |  8340 | `	/* Top class */` |
|      380 |  8341 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      380 |  8342 | `	if( pClass ){` |
|      376 |  8343 | `		ph7_value_reset_string_cursor(pValue);` |
|      376 |  8344 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      376 |  8345 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      187 |  8346 | `	}` |
|        - |  8347 | `	/* Return the freshly created array */` |
|      380 |  8348 | `	ph7_result_value(pCtx,pArray);` |
|        - |  8349 | `	/*` |
|        - |  8350 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  8351 | `	 * as soon we return from this function.` |
|        - |  8352 | `	 */` |
|      380 |  8353 | `	return PH7_OK;` |
|      191 |  8354 |  |
|        - |  8355 | `/*` |
|        - |  8356 | ` * Generate a small backtrace.` |
|        - |  8357 | ` * Store the generated dump in the given BLOB` |
|        - |  8358 | ` */` |
|        4 |  8359 | `static int VmMiniBacktrace(` |
|        - |  8360 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8361 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  8362 | `	)` |
|        1 |  8363 |  |
|        5 |  8364 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8365 | `	ph7_vm_func *pFunc;` |
|        - |  8366 | `	ph7_class *pClass;` |
|        - |  8367 | `	SyString *pFile;` |
|        - |  8368 | `	/* Called function */` |
|        5 |  8369 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8370 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  8371 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  8372 | `	}` |
|        5 |  8373 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  8374 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8375 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  8376 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  8377 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  8378 | `	}else{` |
|      ! 0 |  8379 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  8380 | `	}` |
|        5 |  8381 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  8382 | `	/* Current processed script */` |
|        5 |  8383 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  8384 | `	if( pFile ){` |
|        5 |  8385 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8386 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  8387 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  8388 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  8389 | `	}` |
|        - |  8390 | `	/* Top class */` |
|        5 |  8391 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  8392 | `	if( pClass ){` |
|      ! 0 |  8393 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  8394 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  8395 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  8396 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  8397 | `	}` |
|        5 |  8398 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  8399 | `	/* All done */` |
|        5 |  8400 | `	return SXRET_OK;` |
|        1 |  8401 |  |
|        - |  8402 | `/*` |
|        - |  8403 | ` * void debug_print_backtrace()` |
|        - |  8404 | ` *  Prints a backtrace` |
|        - |  8405 | ` * Parameters` |
|        - |  8406 | ` * None` |
|        - |  8407 | ` * Return` |
|        - |  8408 | ` * NULL` |
|        - |  8409 | ` */` |
|        2 |  8410 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8411 |  |
|        3 |  8412 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8413 | `	SyBlob sDump;` |
|        3 |  8414 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8415 | `	/* Generate the backtrace */` |
|        3 |  8416 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8417 | `	/* Output backtrace */` |
|        3 |  8418 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8419 | `	/* All done,cleanup */` |
|        3 |  8420 | `	SyBlobRelease(&sDump);` |
|        1 |  8421 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8422 | `	SXUNUSED(apArg);` |
|        3 |  8423 | `	return PH7_OK;` |
|        1 |  8424 |  |
|        - |  8425 | `/*` |
|        - |  8426 | ` * string debug_string_backtrace()` |
|        - |  8427 | ` *  Generate a backtrace` |
|        - |  8428 | ` * Parameters` |
|        - |  8429 | ` * None` |
|        - |  8430 | ` * Return` |
|        - |  8431 | ` *  A mini backtrace().` |
|        - |  8432 | ` * Note that this is a symisc extension.` |
|        - |  8433 | ` */` |
|        2 |  8434 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8435 |  |
|        3 |  8436 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8437 | `	SyBlob sDump;` |
|        3 |  8438 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8439 | `	/* Generate the backtrace */` |
|        3 |  8440 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8441 | `	/* Return the backtrace */` |
|        3 |  8442 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  8443 | `	/* All done,cleanup */` |
|        3 |  8444 | `	SyBlobRelease(&sDump);` |
|        1 |  8445 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8446 | `	SXUNUSED(apArg);` |
|        3 |  8447 | `	return PH7_OK;` |
|        1 |  8448 |  |
|        - |  8449 | `/*` |
|        - |  8450 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  8451 | ` * exception is triggered.` |
|        - |  8452 | ` */` |
|      362 |  8453 | `static sxi32 VmUncaughtException(` |
|        - |  8454 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8455 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8456 | `	)` |
|        1 |  8457 |  |
|        - |  8458 | `	ph7_value *apArg[2],sArg;` |
|      363 |  8459 | `	int nArg = 1;` |
|        - |  8460 | `	sxi32 rc;` |
|      363 |  8461 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  8462 | `		/* Nesting limit reached */` |
|      ! 0 |  8463 | `		return SXRET_OK;` |
|        - |  8464 | `	}` |
|        - |  8465 | `	/* Call any exception handler if available */` |
|      363 |  8466 | `	PH7_MemObjInit(pVm,&sArg);` |
|      363 |  8467 | `	if( pThis ){` |
|        - |  8468 | `		/* Load the exception instance */` |
|      363 |  8469 | `		sArg.x.pOther = pThis;` |
|      363 |  8470 | `		pThis->iRef++;` |
|      363 |  8471 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      182 |  8472 | `	}else{` |
|      ! 0 |  8473 | `		nArg = 0;` |
|        - |  8474 | `	}` |
|      363 |  8475 | `	apArg[0] = &sArg;` |
|        - |  8476 | `	/* Call the exception handler if available */` |
|      363 |  8477 | `	pVm->nExceptDepth++;` |
|      363 |  8478 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      363 |  8479 | `	pVm->nExceptDepth--;` |
|      363 |  8480 | `	if( rc != SXRET_OK ){` |
|        - |  8481 | `		SyBlob sMsgBuf;` |
|      361 |  8482 | `		const char *zClass = "Exception";` |
|      361 |  8483 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  8484 | `		const char *zMsg;` |
|        - |  8485 | `		sxu32 nMsg;` |
|        - |  8486 | `		const char *zFuncName;` |
|        - |  8487 | `		int nFuncLen;` |
|      361 |  8488 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      361 |  8489 | `		if( pThis ){` |
|        - |  8490 | `			ph7_class_method *pGetMessage;` |
|        - |  8491 | `			ph7_value sMsg;` |
|        - |  8492 | `			const char *zTmp;` |
|        - |  8493 | `			int nTmp;` |
|      361 |  8494 | `			zClass = pThis->pClass->sName.zString;` |
|      361 |  8495 | `			nClass = pThis->pClass->sName.nByte;` |
|      361 |  8496 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      361 |  8497 | `			if( pGetMessage ){` |
|      361 |  8498 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      361 |  8499 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      361 |  8500 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      361 |  8501 | `					if( zTmp && nTmp > 0 ){` |
|      361 |  8502 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      180 |  8503 | `					}` |
|      180 |  8504 | `				}` |
|      361 |  8505 | `				PH7_MemObjRelease(&sMsg);` |
|      180 |  8506 | `			}` |
|      180 |  8507 | `		}` |
|      361 |  8508 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  8509 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  8510 | `		}` |
|      361 |  8511 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      361 |  8512 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      361 |  8513 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      361 |  8514 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      361 |  8515 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  8516 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      361 |  8517 | `		rc = SXERR_ABORT;` |
|      180 |  8518 | `	}` |
|      363 |  8519 | `	PH7_MemObjRelease(&sArg);` |
|      363 |  8520 | `	return rc;` |
|      182 |  8521 |  |
|        - |  8522 | `/*` |
|        - |  8523 | ` * Throw an user exception.` |
|        - |  8524 | ` */` |
|      376 |  8525 | `static sxi32 VmThrowException(` |
|        - |  8526 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  8527 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8528 | `	)` |
|        2 |  8529 |  |
|        - |  8530 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  8531 | `	ph7_exception **apException;` |
|        - |  8532 | `	ph7_exception *pException;` |
|        - |  8533 | `	/* Point to the stack of loaded exceptions */` |
|      378 |  8534 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      378 |  8535 | `	pException = 0;` |
|      378 |  8536 | `	pCatch = 0;` |
|      378 |  8537 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8538 | `		ph7_exception_block *aCatch;` |
|        - |  8539 | `		ph7_class *pClass;` |
|        - |  8540 | `		sxu32 j;` |
|        - |  8541 | `		/* Locate the appropriate block to execute */` |
|       16 |  8542 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       16 |  8543 | `		(void)SySetPop(&pVm->aException);` |
|       16 |  8544 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       16 |  8545 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       16 |  8546 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  8547 | `			/* Extract the target class */` |
|       16 |  8548 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       16 |  8549 | `			if( pClass == 0 ){` |
|        - |  8550 | `				/* No such class */` |
|      ! 0 |  8551 | `				continue;` |
|        - |  8552 | `			}` |
|       16 |  8553 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  8554 | `				/* Catch block found,break immeditaley */` |
|       16 |  8555 | `				pCatch = &aCatch[j];` |
|       16 |  8556 | `				break;` |
|        - |  8557 | `			}` |
|      ! 0 |  8558 | `		}` |
|        7 |  8559 | `	}` |
|        - |  8560 | `	/* Execute the cached block if available */` |
|      378 |  8561 | `	if( pCatch == 0 ){` |
|        - |  8562 | `		sxi32 rc;` |
|      363 |  8563 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      363 |  8564 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  8565 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  8566 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8567 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  8568 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  8569 | `			}` |
|      ! 0 |  8570 | `			if( pException->pFrame == pFrame ){` |
|        - |  8571 | `				/* Tell the upper layer that the exception was caught */` |
|      ! 0 |  8572 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  8573 | `			}` |
|      ! 0 |  8574 | `		}` |
|      363 |  8575 | `		return rc;` |
|      ! 0 |  8576 | `	}else{` |
|       16 |  8577 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8578 | `		sxi32 rc;` |
|       24 |  8579 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8580 | `			/* Safely ignore the exception frame */` |
|       10 |  8581 | `			pFrame = pFrame->pParent;` |
|        2 |  8582 | `		}` |
|       16 |  8583 | `		if( pException->pFrame == pFrame ){` |
|        - |  8584 | `			/* Tell the upper layer that the exception was caught */` |
|        8 |  8585 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        3 |  8586 | `		}` |
|        - |  8587 | `		/* Create a private frame first */` |
|       16 |  8588 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       16 |  8589 | `		if( rc == SXRET_OK ){` |
|        - |  8590 | `			/* Mark as catch frame */` |
|       16 |  8591 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       16 |  8592 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       16 |  8593 | `			if( pObj ){` |
|        - |  8594 | `				/* Install the exception instance */` |
|       16 |  8595 | `				pThis->iRef++; /* Increment reference count */` |
|       16 |  8596 | `				pObj->x.pOther = pThis;` |
|       16 |  8597 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        7 |  8598 | `			}` |
|        - |  8599 | `			/* Exceute the block */` |
|       16 |  8600 | `			VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  8601 | `			/* Leave the frame */` |
|       16 |  8602 | `			VmLeaveFrame(&(*pVm));` |
|        7 |  8603 | `		}` |
|        - |  8604 | `	}` |
|        - |  8605 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  8606 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  8607 | `	 */` |
|       16 |  8608 | `	return SXRET_OK;` |
|      190 |  8609 |  |
|        - |  8610 | `/*` |
|        - |  8611 | ` * Section:` |
|        - |  8612 | ` *  Version,Credits and Copyright related functions.` |
|        - |  8613 | ` * Status:` |
|        - |  8614 | ` *    Stable.` |
|        - |  8615 | ` */` |
|        - |  8616 | `/*` |
|        - |  8617 | ` * string ph7version(void)` |
|        - |  8618 | ` *  Returns the running version of the PH7 version.` |
|        - |  8619 | ` * Parameters` |
|        - |  8620 | ` *  None` |
|        - |  8621 | ` * Return` |
|        - |  8622 | ` * Current PH7 version.` |
|        - |  8623 | ` */` |
|        2 |  8624 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8625 |  |
|        1 |  8626 | `	SXUNUSED(nArg);` |
|        1 |  8627 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  8628 | `	/* Current engine version */` |
|        3 |  8629 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  8630 | `	return PH7_OK;` |
|        1 |  8631 |  |
|        - |  8632 | `/*` |
|        - |  8633 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  8634 | ` */` |
|        - |  8635 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  8636 | ` "<html><head>"\` |
|        - |  8637 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  8638 | ` "<style type=\"text/css\">"\` |
|        - |  8639 | ` "div {"\` |
|        - |  8640 | `     "border: 1px solid #cccccc;"\` |
|        - |  8641 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  8642 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  8643 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  8644 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  8645 | `     "-webkit-border-radius: 10px;"\` |
|        - |  8646 | `     "-o-border-radius: 10px;"\` |
|        - |  8647 | `     "border-radius: 10px;"\` |
|        - |  8648 | `     "padding-left: 2em;"\` |
|        - |  8649 | `     "background-color: white;"\` |
|        - |  8650 | `     "margin-left: auto;"\` |
|        - |  8651 | `     "font-family: verdana;"\` |
|        - |  8652 | `     "padding-right: 2em;"\` |
|        - |  8653 | `     "margin-right: auto;"\` |
|        - |  8654 | `     "}"\` |
|        - |  8655 | `     "body {"\` |
|        - |  8656 | `     "padding: 0.2em;"\` |
|        - |  8657 | `     "font-style: normal;"\` |
|        - |  8658 | `     "font-size: medium;"\` |
|        - |  8659 | `     "background-color: #f2f2f2;"\` |
|        - |  8660 | `     "}"\` |
|        - |  8661 | `     "hr {"\` |
|        - |  8662 | `     "border-style: solid none none;"\` |
|        - |  8663 | `     "border-width: 1px medium medium;"\` |
|        - |  8664 | `     "border-top: 1px solid #cccccc;"\` |
|        - |  8665 | `     "height: 1px;"\` |
|        - |  8666 | `     "}"\` |
|        - |  8667 | `     "a {"\` |
|        - |  8668 | `     "color: #3366cc;"\` |
|        - |  8669 | `     "text-decoration: none;"\` |
|        - |  8670 | `     "}"\` |
|        - |  8671 | `     "a:hover {"\` |
|        - |  8672 | `     "color: #999999;"\` |
|        - |  8673 | `     "}"\` |
|        - |  8674 | `     "a:active {"\` |
|        - |  8675 | `     "color: #663399;"\` |
|        - |  8676 | `     "}"\` |
|        - |  8677 | `     "h1 {"\` |
|        - |  8678 | `     "margin: 0;"\` |
|        - |  8679 | `     "padding: 0;"\` |
|        - |  8680 | `     "font-family: Verdana;"\` |
|        - |  8681 | `     "font-weight: bold;"\` |
|        - |  8682 | `     "font-style: normal;"\` |
|        - |  8683 | `     "font-size: medium;"\` |
|        - |  8684 | `     "text-transform: capitalize;"\` |
|        - |  8685 | `     "color: #0a328c;"\` |
|        - |  8686 | `     "}"\` |
|        - |  8687 | `     "p {"\` |
|        - |  8688 | `     "margin: 0 auto;"\` |
|        - |  8689 | `     "font-size: medium;"\` |
|        - |  8690 | `     "font-style: normal;"\` |
|        - |  8691 | `     "font-family: verdana;"\` |
|        - |  8692 | `     "}"\` |
|        - |  8693 | `"</style></head><body>"\` |
|        - |  8694 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - |  8695 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - |  8696 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - |  8697 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - |  8698 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - |  8699 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - |  8700 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - |  8701 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - |  8702 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - |  8703 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - |  8704 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - |  8705 |  |
|        - |  8706 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8707 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - |  8708 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - |  8709 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - |  8710 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8711 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - |  8712 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  8713 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - |  8714 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  8715 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - |  8716 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8717 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - |  8718 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - |  8719 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - |  8720 |  |
|        - |  8721 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - |  8722 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - |  8723 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - |  8724 | `"&nbsp;*<br>"\` |
|        - |  8725 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - |  8726 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - |  8727 | `"&nbsp;* are met:<br>"\` |
|        - |  8728 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - |  8729 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - |  8730 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - |  8731 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - |  8732 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - |  8733 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - |  8734 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - |  8735 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - |  8736 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - |  8737 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - |  8738 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - |  8739 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - |  8740 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - |  8741 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - |  8742 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - |  8743 | `"&nbsp;*<br>"\` |
|        - |  8744 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - |  8745 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - |  8746 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - |  8747 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - |  8748 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - |  8749 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - |  8750 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - |  8751 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - |  8752 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - |  8753 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - |  8754 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - |  8755 | `"&nbsp;*/<br>"\` |
|        - |  8756 | `"</span></small></small></p>"\` |
|        - |  8757 | `"</div></body></html>"` |
|        - |  8758 | `/*` |
|        - |  8759 | ` * bool ph7credits(void)` |
|        - |  8760 | ` * bool ph7info(void)` |
|        - |  8761 | ` * bool ph7copyright(void)` |
|        - |  8762 | ` *  Prints out the credits for PH7 engine` |
|        - |  8763 | ` * Parameters` |
|        - |  8764 | ` *  None` |
|        - |  8765 | ` * Return` |
|        - |  8766 | ` *  Always TRUE` |
|        - |  8767 | ` */` |
|        2 |  8768 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8769 |  |
|        3 |  8770 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - |  8771 | `	/* Expand the HTML page above*/` |
|        3 |  8772 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 |  8773 | `	ph7_context_output_format(` |
|        1 |  8774 | `		pCtx,` |
|        - |  8775 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 |  8776 | `		ph7_lib_version(),   /* Engine version */` |
|        1 |  8777 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 |  8778 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 |  8779 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 |  8780 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 |  8781 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - |  8782 | `#ifdef __WINNT__` |
|        - |  8783 | `		"Windows NT"` |
|        - |  8784 | `#elif defined(__UNIXES__)` |
|        - |  8785 | `		"UNIX-Like"` |
|        - |  8786 | `#else` |
|        - |  8787 | `		"Other OS"` |
|        - |  8788 | `#endif` |
|        - |  8789 | `		);` |
|        3 |  8790 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 |  8791 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8792 | `	SXUNUSED(apArg);` |
|        - |  8793 | `	/* Return TRUE */` |
|        - |  8794 | `	//ph7_result_bool(pCtx,1);` |
|        3 |  8795 | `	return PH7_OK;` |
|        1 |  8796 |  |
|        - |  8797 | `/*` |
|        - |  8798 | ` * Section:` |
|        - |  8799 | ` *    URL related routines.` |
|        - |  8800 | ` * Status:` |
|        - |  8801 | ` *    Stable.` |
|        - |  8802 | ` */` |
|        - |  8803 | `/*` |
|        - |  8804 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - |  8805 | ` *  Parse a URL and return its fields.` |
|        - |  8806 | ` * Parameters` |
|        - |  8807 | ` *  $url` |
|        - |  8808 | ` *   The URL to parse.` |
|        - |  8809 | ` * $component` |
|        - |  8810 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - |  8811 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - |  8812 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - |  8813 | ` *  in which case the return value will be an integer).` |
|        - |  8814 | ` * Return` |
|        - |  8815 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - |  8816 | ` *  At least one element will be present within the array. Potential keys within` |
|        - |  8817 | ` *  this array are:` |
|        - |  8818 | ` *   scheme - e.g. http` |
|        - |  8819 | ` *   host` |
|        - |  8820 | ` *   port` |
|        - |  8821 | ` *   user` |
|        - |  8822 | ` *   pass` |
|        - |  8823 | ` *   path` |
|        - |  8824 | ` *   query - after the question mark ?` |
|        - |  8825 | ` *   fragment - after the hashmark #` |
|        - |  8826 | ` * Note:` |
|        - |  8827 | ` *  FALSE is returned on failure.` |
|        - |  8828 | ` *  This function work with relative URL unlike the one shipped` |
|        - |  8829 | ` *  with the standard PHP engine.` |
|        - |  8830 | ` */` |
|       28 |  8831 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8832 |  |
|        - |  8833 | `	const char *zStr; /* Input string */` |
|        - |  8834 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - |  8835 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - |  8836 | `	int nLen;` |
|        - |  8837 | `	sxi32 rc;` |
|       29 |  8838 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  8839 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  8840 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8841 | `		return PH7_OK;` |
|        - |  8842 | `	}` |
|        - |  8843 | `	/* Extract the given URI */` |
|       29 |  8844 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 |  8845 | `	if( nLen < 1 ){` |
|        - |  8846 | `		/* Nothing to process,return FALSE */` |
|        3 |  8847 | `		ph7_result_bool(pCtx,0);` |
|        3 |  8848 | `		return PH7_OK;` |
|        - |  8849 | `	}` |
|        - |  8850 | `	/* Get a parse */` |
|       27 |  8851 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 |  8852 | `	if( rc != SXRET_OK ){` |
|        - |  8853 | `		/* Malformed input,return FALSE */` |
|      ! 0 |  8854 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8855 | `		return PH7_OK;` |
|        - |  8856 | `	}` |
|       27 |  8857 | `	if( nArg > 1 ){` |
|      ! 0 |  8858 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - |  8859 | `		/* Refer to constant.c for constants values */` |
|      ! 0 |  8860 | `		switch(nComponent){` |
|      ! 0 |  8861 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 |  8862 | `			pComp = &sURI.sScheme;` |
|      ! 0 |  8863 | `			if( pComp->nByte < 1 ){` |
|        - |  8864 | `				/* No available value,return NULL */` |
|      ! 0 |  8865 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8866 | `			}else{` |
|      ! 0 |  8867 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8868 | `			}` |
|      ! 0 |  8869 | `			break;` |
|      ! 0 |  8870 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 |  8871 | `			pComp = &sURI.sHost;` |
|      ! 0 |  8872 | `			if( pComp->nByte < 1 ){` |
|        - |  8873 | `				/* No available value,return NULL */` |
|      ! 0 |  8874 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8875 | `			}else{` |
|      ! 0 |  8876 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8877 | `			}` |
|      ! 0 |  8878 | `			break;` |
|      ! 0 |  8879 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 |  8880 | `			pComp = &sURI.sPort;` |
|      ! 0 |  8881 | `			if( pComp->nByte < 1 ){` |
|        - |  8882 | `				/* No available value,return NULL */` |
|      ! 0 |  8883 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8884 | `			}else{` |
|      ! 0 |  8885 | `				int iPort = 0;` |
|        - |  8886 | `				/* Cast the value to integer */` |
|      ! 0 |  8887 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 |  8888 | `				ph7_result_int(pCtx,iPort);` |
|        - |  8889 | `			}` |
|      ! 0 |  8890 | `			break;` |
|      ! 0 |  8891 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 |  8892 | `			pComp = &sURI.sUser;` |
|      ! 0 |  8893 | `			if( pComp->nByte < 1 ){` |
|        - |  8894 | `				/* No available value,return NULL */` |
|      ! 0 |  8895 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8896 | `			}else{` |
|      ! 0 |  8897 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8898 | `			}` |
|      ! 0 |  8899 | `			break;` |
|      ! 0 |  8900 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 |  8901 | `			pComp = &sURI.sPass;` |
|      ! 0 |  8902 | `			if( pComp->nByte < 1 ){` |
|        - |  8903 | `				/* No available value,return NULL */` |
|      ! 0 |  8904 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8905 | `			}else{` |
|      ! 0 |  8906 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8907 | `			}` |
|      ! 0 |  8908 | `			break;` |
|      ! 0 |  8909 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 |  8910 | `			pComp = &sURI.sQuery;` |
|      ! 0 |  8911 | `			if( pComp->nByte < 1 ){` |
|        - |  8912 | `				/* No available value,return NULL */` |
|      ! 0 |  8913 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8914 | `			}else{` |
|      ! 0 |  8915 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8916 | `			}` |
|      ! 0 |  8917 | `			break;` |
|      ! 0 |  8918 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 |  8919 | `			pComp = &sURI.sFragment;` |
|      ! 0 |  8920 | `			if( pComp->nByte < 1 ){` |
|        - |  8921 | `				/* No available value,return NULL */` |
|      ! 0 |  8922 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8923 | `			}else{` |
|      ! 0 |  8924 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8925 | `			}` |
|      ! 0 |  8926 | `			break;` |
|      ! 0 |  8927 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 |  8928 | `			pComp = &sURI.sPath;` |
|      ! 0 |  8929 | `			if( pComp->nByte < 1 ){` |
|        - |  8930 | `				/* No available value,return NULL */` |
|      ! 0 |  8931 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8932 | `			}else{` |
|      ! 0 |  8933 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8934 | `			}` |
|      ! 0 |  8935 | `			break;` |
|      ! 0 |  8936 | `		default:` |
|        - |  8937 | `			/* No such entry,return NULL */` |
|      ! 0 |  8938 | `			ph7_result_null(pCtx);` |
|      ! 0 |  8939 | `			break;` |
|        - |  8940 | `		}` |
|      ! 0 |  8941 | `	}else{` |
|        - |  8942 | `		ph7_value *pArray,*pValue;` |
|        - |  8943 | `		/* Return an associative array */` |
|       27 |  8944 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 |  8945 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 |  8946 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8947 | `			/* Out of memory */` |
|      ! 0 |  8948 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  8949 | `			/* Return false */` |
|      ! 0 |  8950 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |  8951 | `			return PH7_OK;` |
|        - |  8952 | `		}` |
|        - |  8953 | `		/* Fill the array */` |
|       27 |  8954 | `		pComp = &sURI.sScheme;` |
|       27 |  8955 | `		if( pComp->nByte > 0 ){` |
|       19 |  8956 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 |  8957 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 |  8958 | `		}` |
|        - |  8959 | `		/* Reset the string cursor */` |
|       27 |  8960 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8961 | `		pComp = &sURI.sHost;` |
|       27 |  8962 | `		if( pComp->nByte > 0 ){` |
|       25 |  8963 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 |  8964 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 |  8965 | `		}` |
|        - |  8966 | `		/* Reset the string cursor */` |
|       27 |  8967 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8968 | `		pComp = &sURI.sPort;` |
|       27 |  8969 | `		if( pComp->nByte > 0 ){` |
|       11 |  8970 | `			int iPort = 0;/* cc warning */` |
|        - |  8971 | `			/* Convert to integer */` |
|       11 |  8972 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 |  8973 | `			ph7_value_int(pValue,iPort);` |
|       11 |  8974 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 |  8975 | `		}` |
|        - |  8976 | `		/* Reset the string cursor */` |
|       27 |  8977 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8978 | `		pComp = &sURI.sUser;` |
|       27 |  8979 | `		if( pComp->nByte > 0 ){` |
|        7 |  8980 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  8981 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 |  8982 | `		}` |
|        - |  8983 | `		/* Reset the string cursor */` |
|       27 |  8984 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8985 | `		pComp = &sURI.sPass;` |
|       27 |  8986 | `		if( pComp->nByte > 0 ){` |
|        7 |  8987 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  8988 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 |  8989 | `		}` |
|        - |  8990 | `		/* Reset the string cursor */` |
|       27 |  8991 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8992 | `		pComp = &sURI.sPath;` |
|       27 |  8993 | `		if( pComp->nByte > 0 ){` |
|       17 |  8994 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 |  8995 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 |  8996 | `		}` |
|        - |  8997 | `		/* Reset the string cursor */` |
|       27 |  8998 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8999 | `		pComp = &sURI.sQuery;` |
|       27 |  9000 | `		if( pComp->nByte > 0 ){` |
|        5 |  9001 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9002 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 |  9003 | `		}` |
|        - |  9004 | `		/* Reset the string cursor */` |
|       27 |  9005 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9006 | `		pComp = &sURI.sFragment;` |
|       27 |  9007 | `		if( pComp->nByte > 0 ){` |
|        5 |  9008 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9009 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 |  9010 | `		}` |
|        - |  9011 | `		/* Return the created array */` |
|       27 |  9012 | `		ph7_result_value(pCtx,pArray);` |
|        - |  9013 | `		/* NOTE:` |
|        - |  9014 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - |  9015 | `		 * automatically as soon we return from this function.` |
|        - |  9016 | `		 */` |
|        - |  9017 | `	}` |
|        - |  9018 | `	/* All done */` |
|       27 |  9019 | `	return PH7_OK;` |
|       15 |  9020 |  |
|        - |  9021 | `/*` |
|        - |  9022 | ` * Section:` |
|        - |  9023 | ` *   Array related routines.` |
|        - |  9024 | ` * Status:` |
|        - |  9025 | ` *    Stable.` |
|        - |  9026 | ` * Note 2012-5-21 01:04:15:` |
|        - |  9027 | ` *  Array related functions that need access to the underlying` |
|        - |  9028 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - |  9029 | ` */` |
|        - |  9030 | `/*` |
|        - |  9031 | ` * The [compact()] function store it's state information in an instance` |
|        - |  9032 | ` * of the following structure.` |
|        - |  9033 | ` */` |
|        - |  9034 | `struct compact_data` |
|        - |  9035 |  |
|        - |  9036 | `	ph7_value *pArray;  /* Target array */` |
|        - |  9037 | `	int nRecCount;      /* Recursion count */` |
|        - |  9038 | `};` |
|        - |  9039 | `/*` |
|        - |  9040 | ` * Walker callback for the [compact()] function defined below.` |
|        - |  9041 | ` */` |
|      ! 0 |  9042 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 |  9043 |  |
|      ! 0 |  9044 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 |  9045 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 |  9046 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9047 | `	/* Act according to the hashmap value */` |
|      ! 0 |  9048 | `	if( ph7_value_is_string(pValue) ){` |
|        - |  9049 | `		SyString sVar;` |
|      ! 0 |  9050 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 |  9051 | `		if( sVar.nByte > 0 ){` |
|        - |  9052 | `			/* Query the current frame */` |
|      ! 0 |  9053 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - |  9054 | `			/* ^` |
|        - |  9055 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - |  9056 | `			 */` |
|      ! 0 |  9057 | `			if( pKey ){` |
|        - |  9058 | `				/* Perform the insertion */` |
|      ! 0 |  9059 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 |  9060 | `			}` |
|      ! 0 |  9061 | `		}` |
|      ! 0 |  9062 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - |  9063 | `		int rc;` |
|        - |  9064 | `		/* Recursively traverse this array */` |
|      ! 0 |  9065 | `		pData->nRecCount++;` |
|      ! 0 |  9066 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 |  9067 | `		pData->nRecCount--;` |
|      ! 0 |  9068 | `		return rc;` |
|        - |  9069 | `	}` |
|      ! 0 |  9070 | `	return SXRET_OK;` |
|      ! 0 |  9071 |  |
|        - |  9072 | `/*` |
|        - |  9073 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - |  9074 | ` *  Create array containing variables and their values.` |
|        - |  9075 | ` *  For each of these, compact() looks for a variable with that name` |
|        - |  9076 | ` *  in the current symbol table and adds it to the output array such` |
|        - |  9077 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - |  9078 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - |  9079 | ` *  Any strings that are not set will simply be skipped.` |
|        - |  9080 | ` * Parameters` |
|        - |  9081 | ` *  $varname` |
|        - |  9082 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - |  9083 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - |  9084 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - |  9085 | ` *   it recursively.` |
|        - |  9086 | ` * Return` |
|        - |  9087 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - |  9088 | ` */` |
|        2 |  9089 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9090 |  |
|        - |  9091 | `	ph7_value *pArray,*pObj;` |
|        3 |  9092 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9093 | `	const char *zName;` |
|        - |  9094 | `	SyString sVar;` |
|        - |  9095 | `	int i,nLen;` |
|        3 |  9096 | `	if( nArg < 1 ){` |
|        - |  9097 | `		/* Missing arguments,return NULL */` |
|      ! 0 |  9098 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9099 | `		return PH7_OK;` |
|        - |  9100 | `	}` |
|        - |  9101 | `	/* Create the array */` |
|        3 |  9102 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9103 | `	if( pArray == 0 ){` |
|        - |  9104 | `		/* Out of memory */` |
|      ! 0 |  9105 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9106 | `		/* Return NULL */` |
|      ! 0 |  9107 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9108 | `		return PH7_OK;` |
|        - |  9109 | `	}` |
|        - |  9110 | `	/* Perform the requested operation */` |
|        7 |  9111 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 |  9112 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 |  9113 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - |  9114 | `				struct compact_data sData;` |
|      ! 0 |  9115 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - |  9116 | `				/* Recursively walk the array */` |
|      ! 0 |  9117 | `				sData.nRecCount = 0;` |
|      ! 0 |  9118 | `				sData.pArray = pArray;` |
|      ! 0 |  9119 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 |  9120 | `			}` |
|      ! 0 |  9121 | `		}else{` |
|        - |  9122 | `			/* Extract variable name */` |
|        5 |  9123 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 |  9124 | `			if( nLen > 0 ){` |
|        5 |  9125 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - |  9126 | `				/* Check if the variable is available in the current frame */` |
|        5 |  9127 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 |  9128 | `				if( pObj ){` |
|        5 |  9129 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 |  9130 | `				}` |
|        2 |  9131 | `			}` |
|        - |  9132 | `		}` |
|        3 |  9133 | `	}` |
|        - |  9134 | `	/* Return the array */` |
|        3 |  9135 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9136 | `	return PH7_OK;` |
|        2 |  9137 |  |
|        - |  9138 | `/*` |
|        - |  9139 | ` * The [extract()] function store it's state information in an instance` |
|        - |  9140 | ` * of the following structure.` |
|        - |  9141 | ` */` |
|        - |  9142 | `typedef struct extract_aux_data extract_aux_data;` |
|        - |  9143 | `struct extract_aux_data` |
|        - |  9144 |  |
|        - |  9145 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - |  9146 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - |  9147 | `	const char *zPrefix;  /* Prefix name */` |
|        - |  9148 | `	int Prefixlen;        /* Prefix  length */` |
|        - |  9149 | `	int iFlags;           /* Control flags */` |
|        - |  9150 | `	char zWorker[1024];   /* Working buffer */` |
|        - |  9151 | `};` |
|        - |  9152 | `/* Forward declaration */` |
|        - |  9153 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - |  9154 | `/*` |
|        - |  9155 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - |  9156 | ` *   Import variables into the current symbol table from an array.` |
|        - |  9157 | ` * Parameters` |
|        - |  9158 | ` * $var_array` |
|        - |  9159 | ` *  An associative array. This function treats keys as variable names and values` |
|        - |  9160 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - |  9161 | ` *  table, subject to extract_type and prefix parameters.` |
|        - |  9162 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - |  9163 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - |  9164 | ` * $extract_type` |
|        - |  9165 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - |  9166 | ` *  It can be one of the following values:` |
|        - |  9167 | ` *   EXTR_OVERWRITE` |
|        - |  9168 | ` *       If there is a collision, overwrite the existing variable.` |
|        - |  9169 | ` *   EXTR_SKIP` |
|        - |  9170 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - |  9171 | ` *   EXTR_PREFIX_SAME` |
|        - |  9172 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - |  9173 | ` *   EXTR_PREFIX_ALL` |
|        - |  9174 | ` *       Prefix all variable names with prefix.` |
|        - |  9175 | ` *   EXTR_PREFIX_INVALID` |
|        - |  9176 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - |  9177 | ` *   EXTR_IF_EXISTS` |
|        - |  9178 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - |  9179 | ` *       otherwise do nothing.` |
|        - |  9180 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - |  9181 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - |  9182 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - |  9183 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - |  9184 | ` *      the current symbol table.` |
|        - |  9185 | ` * $prefix` |
|        - |  9186 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - |  9187 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - |  9188 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - |  9189 | ` *  underscore character.` |
|        - |  9190 | ` * Return` |
|        - |  9191 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - |  9192 | ` */` |
|        4 |  9193 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9194 |  |
|        - |  9195 | `	extract_aux_data sAux;` |
|        - |  9196 | `	ph7_hashmap *pMap;` |
|        5 |  9197 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - |  9198 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 |  9199 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9200 | `		return PH7_OK;` |
|        - |  9201 | `	}` |
|        - |  9202 | `	/* Point to the target hashmap */` |
|        5 |  9203 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 |  9204 | `	if( pMap->nEntry < 1 ){` |
|        - |  9205 | `		/* Empty map,return  0 */` |
|      ! 0 |  9206 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9207 | `		return PH7_OK;` |
|        - |  9208 | `	}` |
|        - |  9209 | `	/* Prepare the aux data */` |
|        5 |  9210 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 |  9211 | `	if( nArg > 1 ){` |
|        3 |  9212 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 |  9213 | `		if( nArg > 2 ){` |
|      ! 0 |  9214 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 |  9215 | `		}` |
|        1 |  9216 | `	}` |
|        5 |  9217 | `	sAux.pVm = pCtx->pVm;` |
|        - |  9218 | `	/* Invoke the worker callback */` |
|        5 |  9219 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - |  9220 | `	/* Number of variables successfully imported */` |
|        5 |  9221 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 |  9222 | `	return PH7_OK;` |
|        3 |  9223 |  |
|        - |  9224 | `/*` |
|        - |  9225 | ` * Worker callback for the [extract()] function defined` |
|        - |  9226 | ` * below.` |
|        - |  9227 | ` */` |
|        8 |  9228 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9229 |  |
|        9 |  9230 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 |  9231 | `	int iFlags = pAux->iFlags;` |
|        9 |  9232 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9233 | `	ph7_value *pObj;` |
|        - |  9234 | `	SyString sVar;` |
|        9 |  9235 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 |  9236 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 |  9237 | `	}` |
|        - |  9238 | `	/* Perform a string cast */` |
|        9 |  9239 | `	PH7_MemObjToString(pKey);` |
|        9 |  9240 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9241 | `		/* Unavailable variable name */` |
|      ! 0 |  9242 | `		return SXRET_OK;` |
|        - |  9243 | `	}` |
|        9 |  9244 | `	sVar.nByte = 0; /* cc warning */` |
|        9 |  9245 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 |  9246 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9247 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9248 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9249 | `			);` |
|      ! 0 |  9250 | `	}else{` |
|       13 |  9251 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 |  9252 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9253 | `	}` |
|        9 |  9254 | `	sVar.zString = pAux->zWorker;` |
|        - |  9255 | `	/* Try to extract the variable */` |
|        9 |  9256 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 |  9257 | `	if( pObj ){` |
|        - |  9258 | `		/* Collision */` |
|        5 |  9259 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 |  9260 | `			return SXRET_OK;` |
|        - |  9261 | `		}` |
|        5 |  9262 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 |  9263 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - |  9264 | `				/* Already prefixed */` |
|      ! 0 |  9265 | `				return SXRET_OK;` |
|        - |  9266 | `			}` |
|      ! 0 |  9267 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9268 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9269 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9270 | `				);` |
|      ! 0 |  9271 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 |  9272 | `		}` |
|        3 |  9273 | `	}else{` |
|        - |  9274 | `		/* Create the variable */` |
|        5 |  9275 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - |  9276 | `	}` |
|        9 |  9277 | `	if( pObj ){` |
|        - |  9278 | `		/* Overwrite the old value */` |
|        9 |  9279 | `		PH7_MemObjStore(pValue,pObj);` |
|        - |  9280 | `		/* Increment counter */` |
|        9 |  9281 | `		pAux->iCount++;` |
|        4 |  9282 | `	}` |
|        9 |  9283 | `	return SXRET_OK;` |
|        5 |  9284 |  |
|        - |  9285 | `/*` |
|        - |  9286 | ` * Worker callback for the [import_request_variables()] function` |
|        - |  9287 | ` * defined below.` |
|        - |  9288 | ` */` |
|        2 |  9289 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9290 |  |
|        3 |  9291 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 |  9292 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9293 | `	ph7_value *pObj;` |
|        - |  9294 | `	SyString sVar;` |
|        - |  9295 | `	/* Perform a string cast */` |
|        3 |  9296 | `	PH7_MemObjToString(pKey);` |
|        3 |  9297 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9298 | `		/* Unavailable variable name */` |
|      ! 0 |  9299 | `		return SXRET_OK;` |
|        - |  9300 | `	}` |
|        3 |  9301 | `	sVar.nByte = 0; /* cc warning */` |
|        3 |  9302 | `	if( pAux->Prefixlen > 0 ){` |
|        4 |  9303 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 |  9304 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 |  9305 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9306 | `			);` |
|        2 |  9307 | `	}else{` |
|      ! 0 |  9308 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 |  9309 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9310 | `	}` |
|        3 |  9311 | `	sVar.zString = pAux->zWorker;` |
|        - |  9312 | `	/* Extract the variable */` |
|        3 |  9313 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 |  9314 | `	if( pObj ){` |
|        3 |  9315 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 |  9316 | `	}` |
|        3 |  9317 | `	return SXRET_OK;` |
|        2 |  9318 |  |
|        - |  9319 | `/*` |
|        - |  9320 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - |  9321 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - |  9322 | ` * Parameters` |
|        - |  9323 | ` * $types` |
|        - |  9324 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - |  9325 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - |  9326 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - |  9327 | ` *  POST includes the POST uploaded file information.` |
|        - |  9328 | ` *  Note:` |
|        - |  9329 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - |  9330 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - |  9331 | ` * $prefix` |
|        - |  9332 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - |  9333 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - |  9334 | ` *  variable named $pref_userid.` |
|        - |  9335 | ` * Return` |
|        - |  9336 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9337 | ` */` |
|        2 |  9338 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9339 |  |
|        - |  9340 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - |  9341 | `	extract_aux_data sAux;` |
|        - |  9342 | `	int nLen,nPrefixLen;` |
|        - |  9343 | `	ph7_value *pSuper;` |
|        - |  9344 | `	ph7_vm *pVm;` |
|        - |  9345 | `	/* By default import only $_GET variables  */` |
|        3 |  9346 | `	zImport = "G";` |
|        3 |  9347 | `	nLen = (int)sizeof(char);` |
|        3 |  9348 | `	zPrefix = 0;` |
|        3 |  9349 | `	nPrefixLen = 0;` |
|        3 |  9350 | `	if( nArg > 0 ){` |
|        3 |  9351 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 |  9352 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 |  9353 | `		}` |
|        3 |  9354 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9355 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 |  9356 | `		}` |
|        1 |  9357 | `	}` |
|        - |  9358 | `	/* Point to the underlying VM */` |
|        3 |  9359 | `	pVm = pCtx->pVm;` |
|        - |  9360 | `	/* Initialize the aux data */` |
|        3 |  9361 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 |  9362 | `	sAux.zPrefix = zPrefix;` |
|        3 |  9363 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 |  9364 | `	sAux.pVm = pVm;` |
|        - |  9365 | `	/* Extract */` |
|        3 |  9366 | `	zEnd = &zImport[nLen];` |
|        5 |  9367 | `	while( zImport < zEnd ){` |
|        3 |  9368 | `		int c = zImport[0];` |
|        3 |  9369 | `		pSuper = 0;` |
|        3 |  9370 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - |  9371 | `			/* Import $_GET variables */` |
|        3 |  9372 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 |  9373 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - |  9374 | `			/* Import $_POST variables */` |
|      ! 0 |  9375 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 |  9376 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - |  9377 | `			/* Import $_COOKIE variables */` |
|      ! 0 |  9378 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 |  9379 | `		}` |
|        3 |  9380 | `		if( pSuper ){` |
|        - |  9381 | `			/* Iterate throw array entries */` |
|        3 |  9382 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 |  9383 | `		}` |
|        - |  9384 | `		/* Advance the cursor */` |
|        3 |  9385 | `		zImport++;` |
|        1 |  9386 | `	}` |
|        - |  9387 | `	/* All done,return TRUE*/` |
|        3 |  9388 | `	ph7_result_bool(pCtx,0);` |
|        3 |  9389 | `	return PH7_OK;` |
|        1 |  9390 |  |
|        - |  9391 | `/*` |
|        - |  9392 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |  9393 | ` * Refer to the eval() language construct implementation for more` |
|        - |  9394 | ` * information.` |
|        - |  9395 | ` */` |
|     9422 |  9396 | `static sxi32 VmEvalChunk(` |
|        - |  9397 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |  9398 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |  9399 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |  9400 | `	int iFlags,         /* Compile flag */` |
|        - |  9401 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |  9402 | `	)` |
|        2 |  9403 |  |
|        - |  9404 | `	SySet *pByteCode,aByteCode;` |
|     9424 |  9405 | `	ProcConsumer xErr = 0;` |
|     9424 |  9406 | `	void *pErrData = 0;` |
|        - |  9407 | `	/* Initialize bytecode container */` |
|     9424 |  9408 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     9424 |  9409 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |  9410 | `	/* Reset the code generator */` |
|     9424 |  9411 | `	if( bTrueReturn ){` |
|        - |  9412 | `		/* Included file,log compile-time errors */` |
|     7491 |  9413 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7491 |  9414 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3745 |  9415 | `	}` |
|     9424 |  9416 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |  9417 | `	/* Swap bytecode container */` |
|     9424 |  9418 | `	pByteCode = pVm->pByteContainer;` |
|     9424 |  9419 | `	pVm->pByteContainer = &aByteCode;` |
|        - |  9420 | `	/* Compile the chunk */` |
|     9424 |  9421 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    14135 |  9422 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |  9423 | `		/* Compilation error,return false */` |
|        3 |  9424 | `		if( pCtx ){` |
|        3 |  9425 | `			ph7_result_bool(pCtx,0);` |
|        1 |  9426 | `		}` |
|        2 |  9427 | `	}else{` |
|        - |  9428 | `		/* Mount any newly defined classes */` |
|        - |  9429 | `		SyHashEntry *pEntry;` |
|        - |  9430 | `		ph7_class *pClass;` |
|        - |  9431 | `		ph7_value sResult; /* Return value */` |
|        - |  9432 | `		sxi32 rc;` |
|     9422 |  9433 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   257640 |  9434 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   243510 |  9435 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  9436 | `			/* Only mount classes that haven't been mounted yet */` |
|   243510 |  9437 | `			if( !pClass->bMounted ){` |
|    53022 |  9438 | `				rc = VmMountUserClass(pVm,pClass);` |
|    53022 |  9439 | `				if( rc != SXRET_OK ){` |
|        - |  9440 | `					/* Mount failure (likely memory error) */` |
|      ! 0 |  9441 | `					if( pCtx ){` |
|      ! 0 |  9442 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 |  9443 | `					}` |
|      ! 0 |  9444 | `					goto Cleanup;` |
|        - |  9445 | `				}` |
|    26510 |  9446 | `			}` |
|        2 |  9447 | `		}` |
|     9422 |  9448 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  9449 | `			/* Out of memory */` |
|      ! 0 |  9450 | `			if( pCtx ){` |
|      ! 0 |  9451 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  9452 | `			}` |
|      ! 0 |  9453 | `			goto Cleanup;` |
|        - |  9454 | `		}` |
|     9422 |  9455 | `		if( bTrueReturn ){` |
|        - |  9456 | `			/* Assume a boolean true return value */` |
|     7491 |  9457 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3746 |  9458 | `		}else{` |
|        - |  9459 | `			/* Assume a null return value */` |
|     1932 |  9460 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  9461 | `		}` |
|        - |  9462 | `		/* Execute the compiled chunk */` |
|     9422 |  9463 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|     9422 |  9464 | `		if( pCtx ){` |
|        - |  9465 | `			/* Set the execution result */` |
|     7508 |  9466 | `			ph7_result_value(pCtx,&sResult);` |
|     3753 |  9467 | `		}` |
|     9422 |  9468 | `		PH7_MemObjRelease(&sResult);` |
|        - |  9469 | `	}` |
|     4711 |  9470 | `Cleanup:` |
|        - |  9471 | `	/* Cleanup the mess left behind */` |
|     9424 |  9472 | `	pVm->pByteContainer = pByteCode;` |
|     9424 |  9473 | `	SySetRelease(&aByteCode);` |
|     9424 |  9474 | `	return SXRET_OK;` |
|        2 |  9475 |  |
|        - |  9476 | `/*` |
|        - |  9477 | ` * value eval(string $code)` |
|        - |  9478 | ` *   Evaluate a string as PHP code.` |
|        - |  9479 | ` * Parameter` |
|        - |  9480 | ` *  code: PHP code to evaluate.` |
|        - |  9481 | ` * Return` |
|        - |  9482 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  9483 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  9484 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  9485 | ` */` |
|       16 |  9486 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9487 |  |
|        - |  9488 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 |  9489 | `	if( nArg < 1 ){` |
|        - |  9490 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9491 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9492 | `		return SXRET_OK;` |
|        - |  9493 | `	}` |
|        - |  9494 | `	/* Chunk to evaluate */` |
|       18 |  9495 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 |  9496 | `	if( sChunk.nByte < 1 ){` |
|        - |  9497 | `		/* Empty string,return NULL */` |
|        3 |  9498 | `		ph7_result_null(pCtx);` |
|        3 |  9499 | `		return SXRET_OK;` |
|        - |  9500 | `	}` |
|        - |  9501 | `	/* Eval the chunk */` |
|       16 |  9502 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 |  9503 | `	return SXRET_OK;` |
|       10 |  9504 |  |
|        - |  9505 | `/*` |
|        - |  9506 | ` * Check if a file path is already included.` |
|        - |  9507 | ` */` |
|    14976 |  9508 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 |  9509 |  |
|        - |  9510 | `	SyString *aEntries;` |
|        - |  9511 | `	sxu32 n;` |
|    14977 |  9512 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  9513 | `	/* Perform a linear search */` |
| 56059739 |  9514 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 56044769 |  9515 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  9516 | `			/* Already included */` |
|        7 |  9517 | `			return TRUE;` |
|        - |  9518 | `		}` |
| 28022382 |  9519 | `	}` |
|    14971 |  9520 | `	return FALSE;` |
|     7489 |  9521 |  |
|        - |  9522 | `/*` |
|        - |  9523 | ` * Push a file path in the appropriate VM container.` |
|        - |  9524 | ` */` |
|    16882 |  9525 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 |  9526 |  |
|        - |  9527 | `	SyString sPath;` |
|        - |  9528 | `	char *zDup;` |
|        - |  9529 | `#ifdef __WINNT__` |
|        - |  9530 | `	char *zCur;` |
|        - |  9531 | `#endif` |
|        - |  9532 | `	sxi32 rc;` |
|    16884 |  9533 | `	if( nLen < 0 ){` |
|     1908 |  9534 | `		nLen = SyStrlen(zPath);` |
|      953 |  9535 | `	}` |
|        - |  9536 | `	/* Duplicate the file path first */` |
|    16884 |  9537 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    16884 |  9538 | `	if( zDup == 0 ){` |
|      ! 0 |  9539 | `		return SXERR_MEM;` |
|        - |  9540 | `	}` |
|        - |  9541 | `#ifdef __WINNT__` |
|        - |  9542 | `	/* Normalize path on windows` |
|        - |  9543 | `	 * Example:` |
|        - |  9544 | `	 *    Path/To/File.php` |
|        - |  9545 | `	 * becomes` |
|        - |  9546 | `	 *   path\to\file.php` |
|        - |  9547 | `	 */` |
|        2 |  9548 | `	zCur = zDup;` |
|        2 |  9549 | `	while( zCur[0] != 0 ){` |
|        2 |  9550 | `		if( zCur[0] == '/' ){` |
|        2 |  9551 | `			zCur[0] = '\\';` |
|        2 |  9552 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 |  9553 | `			int c = SyToLower(zCur[0]);` |
|        1 |  9554 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - |  9555 | `		}` |
|        2 |  9556 | `		zCur++;` |
|        2 |  9557 | `	}` |
|        - |  9558 | `#endif` |
|        - |  9559 | `	/* Install the file path */` |
|    16884 |  9560 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    16884 |  9561 | `	if( !bMain ){` |
|    14977 |  9562 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  9563 | `			/* Already included */` |
|        7 |  9564 | `			*pNew = 0;` |
|        4 |  9565 | `		}else{` |
|        - |  9566 | `			/* Insert in the corresponding container */` |
|    14971 |  9567 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    14971 |  9568 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9569 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  9570 | `				return rc;` |
|        - |  9571 | `			}` |
|    14971 |  9572 | `			*pNew = 1;` |
|        - |  9573 | `		}` |
|     7488 |  9574 | `	}` |
|    16884 |  9575 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    16884 |  9576 | `	return SXRET_OK;` |
|     8443 |  9577 |  |
|        - |  9578 | `/*` |
|        - |  9579 | ` * Compile and Execute a PHP script at run-time.` |
|        - |  9580 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - |  9581 | ` * indicates failure.` |
|        - |  9582 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - |  9583 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - |  9584 | ` * operations.` |
|        - |  9585 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - |  9586 | ` * this function is a no-op.` |
|        - |  9587 | ` * Refer to the implementation of the include(),include_once() language` |
|        - |  9588 | ` * constructs for more information.` |
|        - |  9589 | ` */` |
|     7496 |  9590 | `static sxi32 VmExecIncludedFile(` |
|        - |  9591 | `	 ph7_context *pCtx, /* Call Context */` |
|        - |  9592 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - |  9593 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - |  9594 | `	 )` |
|        2 |  9595 |  |
|        - |  9596 | `	sxi32 rc;` |
|        - |  9597 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9598 | `	const ph7_io_stream *pStream;` |
|        - |  9599 | `	SyBlob sContents;` |
|        - |  9600 | `	void *pHandle;` |
|        - |  9601 | `	ph7_vm *pVm;` |
|        - |  9602 | `	int isNew;` |
|        - |  9603 | `	/* Initialize fields */` |
|     7498 |  9604 | `	pVm = pCtx->pVm;` |
|     7498 |  9605 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7498 |  9606 | `	isNew = 0;` |
|        - |  9607 | `	/* Extract the associated stream */` |
|     7498 |  9608 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - |  9609 | `	/*` |
|        - |  9610 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - |  9611 | `	 * in a read-only mode.` |
|        - |  9612 | `	 */` |
|     7498 |  9613 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7498 |  9614 | `	if( pHandle == 0 ){` |
|        3 |  9615 | `		return SXERR_IO;` |
|        - |  9616 | `	}` |
|     7495 |  9617 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7495 |  9618 | `	if( IncludeOnce && !isNew ){` |
|        - |  9619 | `		/* Already included */` |
|        5 |  9620 | `		rc = SXERR_EXISTS;` |
|        3 |  9621 | `	}else{` |
|        - |  9622 | `		/* Read the whole file contents */` |
|     7491 |  9623 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7491 |  9624 | `		if( rc == SXRET_OK ){` |
|        - |  9625 | `			SyString sScript;` |
|        - |  9626 | `			/* Compile and execute the script */` |
|     7491 |  9627 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7491 |  9628 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3745 |  9629 | `		}` |
|        - |  9630 | `	}` |
|        - |  9631 | `	/* Pop from the set of included file */` |
|     7495 |  9632 | `	(void)SySetPop(&pVm->aFiles);` |
|        - |  9633 | `	/* Close the handle */` |
|     7495 |  9634 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - |  9635 | `	/* Release the working buffer */` |
|     7495 |  9636 | `	SyBlobRelease(&sContents);` |
|        - |  9637 | `#else` |
|        - |  9638 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - |  9639 | `	SXUNUSED(pPath);` |
|        - |  9640 | `	SXUNUSED(IncludeOnce);` |
|        - |  9641 | `	rc = SXERR_IO;` |
|        - |  9642 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7495 |  9643 | `	return rc;` |
|     3750 |  9644 |  |
|        - |  9645 | `/*` |
|        - |  9646 | ` * string get_include_path(void)` |
|        - |  9647 | ` *  Gets the current include_path configuration option.` |
|        - |  9648 | ` * Parameter` |
|        - |  9649 | ` *  None` |
|        - |  9650 | ` * Return` |
|        - |  9651 | ` *  Included paths as a string` |
|        - |  9652 | ` */` |
|        2 |  9653 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9654 |  |
|        3 |  9655 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9656 | `	SyString *aEntry;` |
|        - |  9657 | `	int dir_sep;` |
|        - |  9658 | `	sxu32 n;` |
|        - |  9659 | `#ifdef __WINNT__` |
|        1 |  9660 | `	dir_sep = ';';` |
|        - |  9661 | `#else` |
|        - |  9662 | `	/* Assume UNIX path separator */` |
|        2 |  9663 | `	dir_sep = ':';` |
|        - |  9664 | `#endif` |
|        1 |  9665 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9666 | `	SXUNUSED(apArg);` |
|        - |  9667 | `	/* Point to the list of import paths */` |
|        3 |  9668 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 |  9669 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 |  9670 | `		SyString *pEntry = &aEntry[n];` |
|        3 |  9671 | `		if( n > 0 ){` |
|        - |  9672 | `			/* Append dir seprator */` |
|      ! 0 |  9673 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 |  9674 | `		}` |
|        - |  9675 | `		/* Append path */` |
|        3 |  9676 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 |  9677 | `	}` |
|        3 |  9678 | `	return PH7_OK;` |
|        1 |  9679 |  |
|        - |  9680 | `/*` |
|        - |  9681 | ` * string get_get_included_files(void)` |
|        - |  9682 | ` *  Gets the current include_path configuration option.` |
|        - |  9683 | ` * Parameter` |
|        - |  9684 | ` *  None` |
|        - |  9685 | ` * Return` |
|        - |  9686 | ` *  Included paths as a string` |
|        - |  9687 | ` */` |
|        2 |  9688 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9689 |  |
|        3 |  9690 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - |  9691 | `	ph7_value *pArray,*pWorker;` |
|        - |  9692 | `	SyString *pEntry;` |
|        - |  9693 | `	int c,d;` |
|        - |  9694 | `	/* Create an array and a working value */` |
|        3 |  9695 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 |  9696 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 |  9697 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - |  9698 | `		/* Out of memory,return null */` |
|      ! 0 |  9699 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9700 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9701 | `		SXUNUSED(apArg);` |
|      ! 0 |  9702 | `		return PH7_OK;` |
|        - |  9703 | `	}` |
|        3 |  9704 | `	c = d = '/';` |
|        - |  9705 | `#ifdef __WINNT__` |
|        1 |  9706 | `	d = '\\';` |
|        - |  9707 | `#endif` |
|        - |  9708 | `	/* Iterate throw entries */` |
|        3 |  9709 | `	SySetResetCursor(pFiles);` |
|     3649 |  9710 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - |  9711 | `		const char *zBase,*zEnd;` |
|        - |  9712 | `		int iLen;` |
|        - |  9713 | `		/* reset the string cursor */` |
|     3647 |  9714 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - |  9715 | `		/* Extract base name */` |
|     3647 |  9716 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - |  9717 | `		/* Ignore trailing '/' */` |
|     5470 |  9718 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 |  9719 | `			zEnd--;` |
|      ! 0 |  9720 | `		}` |
|     3647 |  9721 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   112320 |  9722 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   106851 |  9723 | `			zEnd--;` |
|        1 |  9724 | `		}` |
|     3647 |  9725 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3647 |  9726 | `		zEnd = &pEntry->zString[iLen];` |
|        - |  9727 | `		/* Copy entry name */` |
|     3647 |  9728 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - |  9729 | `		/* Perform the insertion */` |
|     3647 |  9730 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 |  9731 | `	}` |
|        - |  9732 | `	/* All done,return the created array */` |
|        3 |  9733 | `	ph7_result_value(pCtx,pArray);` |
|        - |  9734 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - |  9735 | `	 * by the engine as soon we return from this foreign` |
|        - |  9736 | `	 * function.` |
|        - |  9737 | `	 */` |
|        3 |  9738 | `	return PH7_OK;` |
|        2 |  9739 |  |
|        - |  9740 | `/*` |
|        - |  9741 | ` * include:` |
|        - |  9742 | ` * According to the PHP reference manual.` |
|        - |  9743 | ` *  The include() function includes and evaluates the specified file.` |
|        - |  9744 | ` *  Files are included based on the file path given or, if none is given` |
|        - |  9745 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - |  9746 | ` *  include() will finally check in the calling script's own directory` |
|        - |  9747 | ` *  and the current working directory before failing. The include()` |
|        - |  9748 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - |  9749 | ` *  behavior from require(), which will emit a fatal error.` |
|        - |  9750 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - |  9751 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - |  9752 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - |  9753 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - |  9754 | ` *  directory to find the requested file.` |
|        - |  9755 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - |  9756 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - |  9757 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - |  9758 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - |  9759 | ` */` |
|     7484 |  9760 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9761 |  |
|        - |  9762 | `	SyString sFile;` |
|        - |  9763 | `	sxi32 rc;` |
|     7486 |  9764 | `	if( nArg < 1 ){` |
|        - |  9765 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9766 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9767 | `		return SXRET_OK;` |
|        - |  9768 | `	}` |
|        - |  9769 | `	/* File to include */` |
|     7486 |  9770 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7486 |  9771 | `	if( sFile.nByte < 1 ){` |
|        - |  9772 | `		/* Empty string,return NULL */` |
|      ! 0 |  9773 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9774 | `		return SXRET_OK;` |
|        - |  9775 | `	}` |
|        - |  9776 | `	/* Open,compile and execute the desired script */` |
|     7486 |  9777 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7486 |  9778 | `	if( rc != SXRET_OK ){` |
|        - |  9779 | `		/* Emit a warning and return false */` |
|        3 |  9780 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 |  9781 | `		ph7_result_bool(pCtx,0);` |
|        1 |  9782 | `	}` |
|     7486 |  9783 | `	return SXRET_OK;` |
|     3744 |  9784 |  |
|        - |  9785 | `/*` |
|        - |  9786 | ` * include_once:` |
|        - |  9787 | ` *  According to the PHP reference manual.` |
|        - |  9788 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - |  9789 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - |  9790 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - |  9791 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - |  9792 | ` *   just once.` |
|        - |  9793 | ` */` |
|        4 |  9794 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9795 |  |
|        - |  9796 | `	SyString sFile;` |
|        - |  9797 | `	sxi32 rc;` |
|        5 |  9798 | `	if( nArg < 1 ){` |
|        - |  9799 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9800 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9801 | `		return SXRET_OK;` |
|        - |  9802 | `	}` |
|        - |  9803 | `	/* File to include */` |
|        5 |  9804 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 |  9805 | `	if( sFile.nByte < 1 ){` |
|        - |  9806 | `		/* Empty string,return NULL */` |
|      ! 0 |  9807 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9808 | `		return SXRET_OK;` |
|        - |  9809 | `	}` |
|        - |  9810 | `	/* Open,compile and execute the desired script */` |
|        5 |  9811 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 |  9812 | `	if( rc == SXERR_EXISTS ){` |
|        - |  9813 | `		/* File already included,return TRUE */` |
|        3 |  9814 | `		ph7_result_bool(pCtx,1);` |
|        3 |  9815 | `		return SXRET_OK;` |
|        - |  9816 | `	}` |
|        3 |  9817 | `	if( rc != SXRET_OK ){` |
|        - |  9818 | `		/* Emit a warning and return false */` |
|      ! 0 |  9819 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 |  9820 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9821 | ` 	}` |
|        3 |  9822 | `	return SXRET_OK;` |
|        3 |  9823 |  |
|        - |  9824 | `/*` |
|        - |  9825 | ` * require.` |
|        - |  9826 | ` *  According to the PHP reference manual.` |
|        - |  9827 | ` *   require() is identical to include() except upon failure it will` |
|        - |  9828 | ` *   also produce a fatal level error.` |
|        - |  9829 | ` *   In other words, it will halt the script whereas include() only` |
|        - |  9830 | ` *   emits a warning  which allows the script to continue.` |
|        - |  9831 | ` */` |
|        4 |  9832 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9833 |  |
|        - |  9834 | `	SyString sFile;` |
|        - |  9835 | `	sxi32 rc;` |
|        5 |  9836 | `	if( nArg < 1 ){` |
|        - |  9837 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9838 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9839 | `		return SXRET_OK;` |
|        - |  9840 | `	}` |
|        - |  9841 | `	/* File to include */` |
|        5 |  9842 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 |  9843 | `	if( sFile.nByte < 1 ){` |
|        - |  9844 | `		/* Empty string,return NULL */` |
|      ! 0 |  9845 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9846 | `		return SXRET_OK;` |
|        - |  9847 | `	}` |
|        - |  9848 | `	/* Open,compile and execute the desired script */` |
|        5 |  9849 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 |  9850 | `	if( rc != SXRET_OK ){` |
|        - |  9851 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  9852 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  9853 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9854 | `		return PH7_ABORT;` |
|        - |  9855 | `	}` |
|        5 |  9856 | `	return SXRET_OK;` |
|        3 |  9857 |  |
|        - |  9858 | `/*` |
|        - |  9859 | ` * require_once:` |
|        - |  9860 | ` *  According to the PHP reference manual.` |
|        - |  9861 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - |  9862 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - |  9863 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - |  9864 | ` *   and how it differs from its non _once siblings.` |
|        - |  9865 | ` */` |
|        4 |  9866 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9867 |  |
|        - |  9868 | `	SyString sFile;` |
|        - |  9869 | `	sxi32 rc;` |
|        5 |  9870 | `	if( nArg < 1 ){` |
|        - |  9871 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9872 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9873 | `		return SXRET_OK;` |
|        - |  9874 | `	}` |
|        - |  9875 | `	/* File to include */` |
|        5 |  9876 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 |  9877 | `	if( sFile.nByte < 1 ){` |
|        - |  9878 | `		/* Empty string,return NULL */` |
|      ! 0 |  9879 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9880 | `		return SXRET_OK;` |
|        - |  9881 | `	}` |
|        - |  9882 | `	/* Open,compile and execute the desired script */` |
|        5 |  9883 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 |  9884 | `	if( rc == SXERR_EXISTS ){` |
|        - |  9885 | `		/* File already included,return TRUE */` |
|        3 |  9886 | `		ph7_result_bool(pCtx,1);` |
|        3 |  9887 | `		return SXRET_OK;` |
|        - |  9888 | `	}` |
|        3 |  9889 | `	if( rc != SXRET_OK ){` |
|        - |  9890 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  9891 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  9892 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9893 | `		return PH7_ABORT;` |
|        - |  9894 | `	}` |
|        3 |  9895 | `	return SXRET_OK;` |
|        3 |  9896 |  |
|        - |  9897 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - |  9898 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - |  9899 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - |  9900 | `/* Table of built-in VM functions. */` |
|        - |  9901 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - |  9902 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - |  9903 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - |  9904 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - |  9905 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - |  9906 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - |  9907 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - |  9908 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - |  9909 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - |  9910 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - |  9911 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - |  9912 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - |  9913 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - |  9914 | `	    /* Constants management */` |
|        - |  9915 | `	{ "defined",  vm_builtin_defined              },` |
|        - |  9916 | `	{ "define",   vm_builtin_define               },` |
|        - |  9917 | `	{ "constant", vm_builtin_constant             },` |
|        - |  9918 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - |  9919 | `	   /* Class/Object functions */` |
|        - |  9920 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - |  9921 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - |  9922 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - |  9923 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - |  9924 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - |  9925 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - |  9926 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - |  9927 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - |  9928 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - |  9929 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - |  9930 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - |  9931 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - |  9932 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - |  9933 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - |  9934 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - |  9935 | `	{ "is_a", vm_builtin_is_a },` |
|        - |  9936 | `	   /* Random numbers/strings generators */` |
|        - |  9937 | `	{ "rand",          vm_builtin_rand            },` |
|        - |  9938 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - |  9939 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - |  9940 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - |  9941 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - |  9942 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9943 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  9944 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - |  9945 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  9946 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  9947 | `	   /* Language constructs functions */` |
|        - |  9948 | `	{ "echo",  vm_builtin_echo                    },` |
|        - |  9949 | `	{ "print", vm_builtin_print                   },` |
|        - |  9950 | `	{ "exit",  vm_builtin_exit                    },` |
|        - |  9951 | `	{ "die",   vm_builtin_exit                    },` |
|        - |  9952 | `	{ "eval",  vm_builtin_eval                    },` |
|        - |  9953 | `	  /* Variable handling functions */` |
|        - |  9954 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - |  9955 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - |  9956 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - |  9957 | `	{ "isset",     vm_builtin_isset                },` |
|        - |  9958 | `	{ "unset",     vm_builtin_unset                },` |
|        - |  9959 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - |  9960 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - |  9961 | `	{ "var_export",vm_builtin_var_export           },` |
|        - |  9962 | `	  /* Ouput control functions */` |
|        - |  9963 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - |  9964 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - |  9965 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - |  9966 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - |  9967 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - |  9968 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - |  9969 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - |  9970 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - |  9971 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - |  9972 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - |  9973 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - |  9974 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - |  9975 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - |  9976 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - |  9977 | `	  /* Assertion functions */` |
|        - |  9978 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - |  9979 | `	{ "assert",          vm_builtin_assert         },` |
|        - |  9980 | `	  /* Error reporting functions */` |
|        - |  9981 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - |  9982 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - |  9983 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - |  9984 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - |  9985 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - |  9986 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - |  9987 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - |  9988 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - |  9989 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - |  9990 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - |  9991 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - |  9992 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - |  9993 | `	  /* Release info */` |
|        - |  9994 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - |  9995 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - |  9996 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - |  9997 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - |  9998 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - |  9999 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 10000 | `	  /* hashmap */` |
|        - | 10001 | `	{"compact",          vm_builtin_compact       },` |
|        - | 10002 | `	{"extract",          vm_builtin_extract       },` |
|        - | 10003 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 10004 | `	  /* URL related function */` |
|        - | 10005 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 10006 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 10007 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10008 | `	   /* XML processing functions */` |
|        - | 10009 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 10010 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 10011 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 10012 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 10013 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 10014 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 10015 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 10016 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 10017 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 10018 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 10019 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 10020 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 10021 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 10022 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 10023 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 10024 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 10025 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 10026 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 10027 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 10028 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 10029 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 10030 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10031 | `	   /* UTF-8 encoding/decoding */` |
|        - | 10032 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 10033 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 10034 | `	   /* Command line processing */` |
|        - | 10035 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 10036 | `	   /* JSON encoding/decoding */` |
|        - | 10037 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 10038 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 10039 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 10040 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 10041 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 10042 | `	   /* Files/URI inclusion facility */` |
|        - | 10043 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 10044 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 10045 | `	{ "include",      vm_builtin_include          },` |
|        - | 10046 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 10047 | `	{ "require",      vm_builtin_require          },` |
|        - | 10048 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 10049 | `};` |
|        - | 10050 | `/*` |
|        - | 10051 | ` * Register the built-in VM functions defined above.` |
|        - | 10052 | ` */` |
|     1676 | 10053 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 10054 |  |
|        - | 10055 | `	sxi32 rc;` |
|        - | 10056 | `	sxu32 n;` |
|   209502 | 10057 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 10058 | `		/* Note that these special functions have access` |
|        - | 10059 | `		 * to the underlying virtual machine as their` |
|        - | 10060 | `		 * private data.` |
|        - | 10061 | `		 */` |
|   207826 | 10062 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   207826 | 10063 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 10064 | `			return rc;` |
|        - | 10065 | `		}` |
|   103914 | 10066 | `	}` |
|     1678 | 10067 | `	return SXRET_OK;` |
|      840 | 10068 |  |
|        - | 10069 | `/*` |
|        - | 10070 | ` * Check if the given name refer to an installed class.` |
|        - | 10071 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 10072 | ` */` |
|    10628 | 10073 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 10074 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 10075 | `	const char *zName,  /* Name of the target class */` |
|        - | 10076 | `	sxu32 nByte,        /* zName length */` |
|        - | 10077 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 10078 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 10079 | `						 */` |
|        - | 10080 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 10081 | `	)` |
|        2 | 10082 |  |
|        - | 10083 | `	SyHashEntry *pEntry;` |
|        - | 10084 | `	ph7_class *pClass;` |
|     5314 | 10085 | `		SXUNUSED(iNest);` |
|        - | 10086 | `	/* Perform a hash lookup */` |
|    10630 | 10087 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|        - | 10088 |  |
|    10630 | 10089 | `	if( pEntry == 0 ){` |
|        - | 10090 | `		/* No such entry,return NULL */` |
|      ! 0 | 10091 | `		return 0;` |
|        - | 10092 | `	}` |
|    10630 | 10093 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    10630 | 10094 | `	if( !iLoadable ){` |
|        - | 10095 | `		/* Return the first class seen */` |
|     9734 | 10096 | `		return pClass;` |
|      ! 0 | 10097 | `	}else{` |
|        - | 10098 | `		/* Check the collision list */` |
|      898 | 10099 | `		while(pClass){` |
|      898 | 10100 | `			if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) == 0 ){` |
|        - | 10101 | `				/* Class is loadable */` |
|      898 | 10102 | `				return pClass;` |
|        - | 10103 | `			}` |
|        - | 10104 | `			/* Point to the next entry */` |
|      ! 0 | 10105 | `			pClass = pClass->pNextName;` |
|      ! 0 | 10106 | `		}` |
|        - | 10107 | `	}` |
|        - | 10108 | `	/* No such loadable class */` |
|      ! 0 | 10109 | `	return 0;` |
|     5316 | 10110 |  |
|        - | 10111 | `/*` |
|        - | 10112 | ` * Reference Table Implementation` |
|        - | 10113 | ` * Status: stable <chm@symisc.net>` |
|        - | 10114 | ` * Intro` |
|        - | 10115 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 10116 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 10117 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 10118 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 10119 | ` *  Refer to the official for more information on this powerful` |
|        - | 10120 | ` *  extension.` |
|        - | 10121 | ` */` |
|        - | 10122 | `/*` |
|        - | 10123 | ` * Allocate a new reference entry.` |
|        - | 10124 | ` */` |
|  2943730 | 10125 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 10126 |  |
|        - | 10127 | `	VmRefObj *pRef;` |
|        - | 10128 | `	/* Allocate a new instance */` |
|  2943732 | 10129 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  2943732 | 10130 | `	if( pRef == 0 ){` |
|      ! 0 | 10131 | `		return 0;` |
|        - | 10132 | `	}` |
|        - | 10133 | `	/* Zero the structure */` |
|  2943732 | 10134 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 10135 | `	/* Initialize fields */` |
|  2943732 | 10136 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  2943732 | 10137 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  2943732 | 10138 | `	pRef->nIdx = nIdx;` |
|  2943732 | 10139 | `	return pRef;` |
|  1471867 | 10140 |  |
|        - | 10141 | `/*` |
|        - | 10142 | ` * Default hash function used by the reference table` |
|        - | 10143 | ` * for lookup/insertion operations.` |
|        - | 10144 | ` */` |
| 16390115 | 10145 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 10146 |  |
|        - | 10147 | `	/* Calculate the hash based on the memory object index */` |
| 16390117 | 10148 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 10149 |  |
|        - | 10150 | `/*` |
|        - | 10151 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 10152 | ` * in the reference table.` |
|        - | 10153 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 10154 | ` * otherwise.` |
|        - | 10155 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10156 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10157 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10158 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10159 | ` * Refer to the official for more information on this powerful` |
|        - | 10160 | ` * extension.` |
|        - | 10161 | ` */` |
|  8796216 | 10162 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 10163 |  |
|        - | 10164 | `	VmRefObj *pRef;` |
|        - | 10165 | `	sxu32 nBucket;` |
|        - | 10166 | `	/* Point to the appropriate bucket */` |
|  8796218 | 10167 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 10168 | `	/* Perform the lookup */` |
|  8796218 | 10169 | `	pRef = pVm->apRefObj[nBucket];` |
| 18587055 | 10170 | `	for(;;){` |
| 37178257 | 10171 | `		if( pRef == 0 ){` |
|  3011140 | 10172 | `			break;` |
|        - | 10173 | `		}` |
| 34167119 | 10174 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 10175 | `			/* Entry found */` |
|  5785080 | 10176 | `			return pRef;` |
|        - | 10177 | `		}` |
|        - | 10178 | `		/* Point to the next entry */` |
| 28382041 | 10179 | `		pRef = pRef->pNextCollide;` |
|        2 | 10180 | `	}` |
|        - | 10181 | `	/* No such entry,return NULL */` |
|  3011140 | 10182 | `	return 0;` |
|  4398110 | 10183 |  |
|        - | 10184 | `/*` |
|        - | 10185 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10186 | ` *` |
|        - | 10187 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10188 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10189 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10190 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10191 | ` * Refer to the official for more information on this powerful` |
|        - | 10192 | ` * extension.` |
|        - | 10193 | ` */` |
|  2943730 | 10194 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10195 |  |
|        - | 10196 | `	sxu32 nBucket;` |
|  2943732 | 10197 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 10198 | `		VmRefObj **apNew;` |
|        - | 10199 | `		sxu32 nNew;` |
|        - | 10200 | `		/* Allocate a larger table */` |
|     2580 | 10201 | `		nNew = pVm->nRefSize << 1;` |
|     2580 | 10202 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     2580 | 10203 | `		if( apNew ){` |
|     2580 | 10204 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 10205 | `			sxu32 n;` |
|        - | 10206 | `			/* Zero the structure */` |
|     2580 | 10207 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 10208 | `			/* Rehash all referenced entries */` |
|  2825442 | 10209 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 10210 | `				/* Remove old collision links */` |
|  2822864 | 10211 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 10212 | `				/* Point to the appropriate bucket */` |
|  2822864 | 10213 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 10214 | `				/* Insert the entry  */` |
|  2822864 | 10215 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2822864 | 10216 | `				if( apNew[nBucket] ){` |
|  2298896 | 10217 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 10218 | `				}` |
|  2822864 | 10219 | `				apNew[nBucket] = pEntry;` |
|        - | 10220 | `				/* Point to the next entry */` |
|  2822864 | 10221 | `				pEntry = pEntry->pNext;` |
|  1411433 | 10222 | `			}` |
|        - | 10223 | `			/* Release the old table */` |
|     2580 | 10224 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 10225 | `			/* Install the new one */` |
|     2580 | 10226 | `			pVm->apRefObj = apNew;` |
|     2580 | 10227 | `			pVm->nRefSize = nNew;` |
|     1289 | 10228 | `		}` |
|     1289 | 10229 | `	}` |
|        - | 10230 | `	/* Point to the appropriate bucket */` |
|  2943732 | 10231 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 10232 | `	/* Insert the entry */` |
|  2943732 | 10233 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  2943732 | 10234 | `	if( pVm->apRefObj[nBucket] ){` |
|  2437654 | 10235 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1219107 | 10236 | `	}` |
|  2943732 | 10237 | `	pVm->apRefObj[nBucket] = pRef;` |
|  2943732 | 10238 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  2943732 | 10239 | `	pVm->nRefUsed++;` |
|  2943732 | 10240 | `	return SXRET_OK;` |
|        2 | 10241 |  |
|        - | 10242 | `/*` |
|        - | 10243 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 10244 | ` * the reference table.` |
|        - | 10245 | ` * This function is invoked when the user perform an unset` |
|        - | 10246 | ` * call [i.e: unset($var); ].` |
|        - | 10247 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10248 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10249 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10250 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10251 | ` * Refer to the official for more information on this powerful` |
|        - | 10252 | ` * extension.` |
|        - | 10253 | ` */` |
|  2919030 | 10254 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10255 |  |
|        - | 10256 | `	ph7_hashmap_node **apNode;` |
|        - | 10257 | `	SyHashEntry **apEntry;` |
|        - | 10258 | `	sxu32 n;` |
|        - | 10259 | `	/* Point to the reference table */` |
|  2919032 | 10260 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2919032 | 10261 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 10262 | `	/* Unlink the entry from the reference table */` |
|  2991380 | 10263 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    72350 | 10264 | `		if( apEntry[n] ){` |
|    72300 | 10265 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    36149 | 10266 | `		}` |
|    36176 | 10267 | `	}` |
|  5768276 | 10268 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2849246 | 10269 | `		if( apNode[n] ){` |
|     5613 | 10270 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2806 | 10271 | `		}` |
|  1424624 | 10272 | `	}` |
|  2919032 | 10273 | `	if( pRef->pPrevCollide ){` |
|  1091725 | 10274 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   545931 | 10275 | `	}else{` |
|  1827309 | 10276 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 10277 | `	}` |
|  2919032 | 10278 | `	if( pRef->pNextCollide ){` |
|  1632333 | 10279 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   816480 | 10280 | `	}` |
|  2919032 | 10281 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 10282 | `	/* Release the node */` |
|  2919032 | 10283 | `	SySetRelease(&pRef->aReference);` |
|  2919032 | 10284 | `	SySetRelease(&pRef->aArrEntries);` |
|  2919032 | 10285 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2919032 | 10286 | `	pVm->nRefUsed--;` |
|  2919032 | 10287 | `	return SXRET_OK;` |
|        2 | 10288 |  |
|        - | 10289 | `/*` |
|        - | 10290 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10291 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10292 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10293 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10294 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10295 | ` * Refer to the official for more information on this powerful` |
|        - | 10296 | ` * extension.` |
|        - | 10297 | ` */` |
|  2966090 | 10298 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 10299 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10300 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10301 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10302 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 10303 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 10304 | `	)` |
|        2 | 10305 |  |
|  2966092 | 10306 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10307 | `	VmRefObj *pRef;` |
|        - | 10308 | `	/* Check if the referenced object already exists */` |
|  2966092 | 10309 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2966092 | 10310 | `	if( pRef == 0 ){` |
|        - | 10311 | `		/* Create a new entry */` |
|  2943732 | 10312 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  2943732 | 10313 | `		if( pRef == 0 ){` |
|      ! 0 | 10314 | `			return SXERR_MEM;` |
|        - | 10315 | `		}` |
|  2943732 | 10316 | `		pRef->iFlags = iFlags;` |
|        - | 10317 | `		/* Install the entry */` |
|  2943732 | 10318 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1471865 | 10319 | `	}` |
|  2971004 | 10320 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 10321 | `		/* Safely ignore the exception frame */` |
|     4914 | 10322 | `		pFrame = pFrame->pParent;` |
|        2 | 10323 | `	}` |
|  2966092 | 10324 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 10325 | `		VmSlot sRef;` |
|        - | 10326 | `		/* Local frame,record referenced entry so that it can` |
|        - | 10327 | `		 * be deleted when we leave this frame.` |
|        - | 10328 | `		 */` |
|    67440 | 10329 | `		sRef.nIdx = nIdx;` |
|    67440 | 10330 | `		sRef.pUserData = pEntry;` |
|    67440 | 10331 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 10332 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 10333 | `		}` |
|    33719 | 10334 | `	}` |
|  2966092 | 10335 | `	if( pEntry ){` |
|        - | 10336 | `		/* Address of the hash-entry */` |
|    89614 | 10337 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    44806 | 10338 | `	}` |
|  2966092 | 10339 | `	if( pMapEntry ){` |
|        - | 10340 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2872400 | 10341 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1436199 | 10342 | `	}` |
|  2966092 | 10343 | `	return SXRET_OK;` |
|  1483047 | 10344 |  |
|        - | 10345 | `/*` |
|        - | 10346 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 10347 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10348 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10349 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10350 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10351 | ` * Refer to the official for more information on this powerful` |
|        - | 10352 | ` * extension.` |
|        - | 10353 | ` */` |
|  2911076 | 10354 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 10355 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10356 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10357 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10358 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 10359 | `	)` |
|        2 | 10360 |  |
|        - | 10361 | `	VmRefObj *pRef;` |
|        - | 10362 | `	sxu32 n;` |
|        - | 10363 | `	/* Check if the referenced object already exists */` |
|  2911078 | 10364 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2911078 | 10365 | `	if( pRef == 0 ){` |
|        - | 10366 | `		/* Not such entry */` |
|    67390 | 10367 | `		return SXERR_NOTFOUND;` |
|        - | 10368 | `	}` |
|        - | 10369 | `	/* Remove the desired entry */` |
|  2843690 | 10370 | `	if( pEntry ){` |
|        - | 10371 | `		SyHashEntry **apEntry;` |
|       51 | 10372 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      195 | 10373 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      145 | 10374 | `			if( apEntry[n] == pEntry ){` |
|        - | 10375 | `				/* Nullify the entry */` |
|       51 | 10376 | `				apEntry[n] = 0;` |
|        - | 10377 | `				/*` |
|        - | 10378 | `				 * NOTE:` |
|        - | 10379 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 10380 | `				 * we avoid wasting spaces.` |
|        - | 10381 | `				 */` |
|       25 | 10382 | `			}` |
|       73 | 10383 | `		}` |
|       25 | 10384 | `	}` |
|  2843690 | 10385 | `	if( pMapEntry ){` |
|        - | 10386 | `		ph7_hashmap_node **apNode;` |
|  2843640 | 10387 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5687366 | 10388 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2843728 | 10389 | `			if( apNode[n] == pMapEntry ){` |
|        - | 10390 | `				/* nullify the entry */` |
|  2843640 | 10391 | `				apNode[n] = 0;` |
|  1421819 | 10392 | `			}` |
|  1421865 | 10393 | `		}` |
|  1421819 | 10394 | `	}` |
|  2843690 | 10395 | `	return SXRET_OK;` |
|  1455540 | 10396 |  |
|        - | 10397 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 10398 | `/*` |
|        - | 10399 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 10400 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 10401 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 10402 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 10403 | ` * For more information on how to register IO stream devices,please` |
|        - | 10404 | ` * refer to the official documentation.` |
|        - | 10405 | ` */` |
|    21992 | 10406 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 10407 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 10408 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 10409 | `	int nByte              /* *pzDevice length*/` |
|        - | 10410 | `	)` |
|        2 | 10411 |  |
|        - | 10412 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 10413 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 10414 | `	SyString sDev,sCur;` |
|        - | 10415 | `	sxu32 n,nEntry;` |
|        - | 10416 | `	int rc;` |
|        - | 10417 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    21994 | 10418 | `	zNext = zCur = zIn = *pzDevice;` |
|    21994 | 10419 | `	zEnd = &zIn[nByte];` |
|  1400995 | 10420 | `	while( zIn < zEnd ){` |
|  1379005 | 10421 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 10422 | `			/* Got one */` |
|        3 | 10423 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 10424 | `			break;` |
|        - | 10425 | `		}` |
|        - | 10426 | `		/* Advance the cursor */` |
|  1379003 | 10427 | `		zIn++;` |
|        2 | 10428 | `	}` |
|    21994 | 10429 | `	if( zIn >= zEnd ){` |
|        - | 10430 | `		/* No such scheme,return the default stream */` |
|    21992 | 10431 | `		return pVm->pDefStream;` |
|        - | 10432 | `	}` |
|        3 | 10433 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 10434 | `	/* Remove leading and trailing white spaces */` |
|        3 | 10435 | `	SyStringFullTrim(&sDev);` |
|        - | 10436 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 10437 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 10438 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 10439 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 10440 | `		pStream = apStream[n];` |
|        3 | 10441 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 10442 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 10443 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 10444 | `		if( rc == 0 ){` |
|        - | 10445 | `			/* Stream device found */` |
|        3 | 10446 | `			*pzDevice = zNext;` |
|        3 | 10447 | `			return pStream;` |
|        - | 10448 | `		}` |
|      ! 0 | 10449 | `	}` |
|        - | 10450 | `	/* No such stream,return NULL */` |
|      ! 0 | 10451 | `	return 0;` |
|    10998 | 10452 |  |
|        - | 10453 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 10454 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 10455 |  |
