# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3903/5151 lines (75.77%)

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
|   810308 |    86 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    87 |  |
|   810310 |    88 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       30 |    89 | `		return TRUE;` |
|        - |    90 | `	}` |
|   810282 |    91 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |    92 | `		return TRUE;` |
|        - |    93 | `	}` |
|   810274 |    94 | `	return FALSE;` |
|   405178 |    95 |  |
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
|   403988 |   110 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   403990 |   121 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   403990 |   122 | `	if( pEntry ){` |
|        - |   123 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   124 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   125 | `		pCons->xExpand = xExpand;` |
|        6 |   126 | `		pCons->pUserData = pUserData;` |
|        6 |   127 | `		return SXRET_OK;` |
|        - |   128 | `	}` |
|        - |   129 | `	/* Allocate a new constant instance */` |
|   403986 |   130 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   403986 |   131 | `	if( pCons == 0 ){` |
|      ! 0 |   132 | `		return 0;` |
|        - |   133 | `	}` |
|        - |   134 | `	/* Duplicate constant name */` |
|   403986 |   135 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   403986 |   136 | `	if( zDupName == 0 ){` |
|      ! 0 |   137 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   138 | `		return 0;` |
|        - |   139 | `	}` |
|        - |   140 | `	/* Install the constant */` |
|   403986 |   141 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   403986 |   142 | `	pCons->xExpand = xExpand;` |
|   403986 |   143 | `	pCons->pUserData = pUserData;` |
|   403986 |   144 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   403986 |   145 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   146 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return rc;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* All done,constant can be invoked from PHP code */` |
|   403986 |   151 | `	return SXRET_OK;` |
|   201996 |   152 |  |
|        - |   153 | `/*` |
|        - |   154 | ` * Allocate a new foreign function instance.` |
|        - |   155 | ` * This function return SXRET_OK on success. Any other` |
|        - |   156 | ` * return value indicates failure.` |
|        - |   157 | ` * Please refer to the official documentation for an introduction to` |
|        - |   158 | ` * the foreign function mechanism.` |
|        - |   159 | ` */` |
|   865650 |   160 | `static sxi32 PH7_NewForeignFunction(` |
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
|   865652 |   171 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   865652 |   172 | `	if( pFunc == 0 ){` |
|      ! 0 |   173 | `		return SXERR_MEM;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* Duplicate function name */` |
|   865652 |   176 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   865652 |   177 | `	if( zDup == 0 ){` |
|      ! 0 |   178 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   179 | `		return SXERR_MEM;` |
|        - |   180 | `	}` |
|        - |   181 | `	/* Zero the structure */` |
|   865652 |   182 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   183 | `	/* Initialize structure fields */` |
|   865652 |   184 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   865652 |   185 | `	pFunc->pVm   = pVm;` |
|   865652 |   186 | `	pFunc->xFunc = xFunc;` |
|   865652 |   187 | `	pFunc->pUserData = pUserData;` |
|   865652 |   188 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   189 | `	/* Write a pointer to the new function */` |
|   865652 |   190 | `	*ppOut = pFunc;` |
|   865652 |   191 | `	return SXRET_OK;` |
|   432827 |   192 |  |
|        - |   193 | `/*` |
|        - |   194 | ` * Install a foreign function and it's associated callback so that` |
|        - |   195 | ` * it can be invoked from the target PHP code.` |
|        - |   196 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   197 | ` * return value indicates failure.` |
|        - |   198 | ` * Please refer to the official documentation for an introduction to` |
|        - |   199 | ` * the foreign function mechanism.` |
|        - |   200 | ` */` |
|   867640 |   201 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|   867642 |   212 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   867642 |   213 | `	if( pEntry ){` |
|     1992 |   214 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     1992 |   215 | `		pFunc->pUserData = pUserData;` |
|     1992 |   216 | `		pFunc->xFunc = xFunc;` |
|     1992 |   217 | `		SySetReset(&pFunc->aAux);` |
|     1992 |   218 | `		return SXRET_OK;` |
|        - |   219 | `	}` |
|        - |   220 | `	/* Create a new user function */` |
|   865652 |   221 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   865652 |   222 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   223 | `		return rc;` |
|        - |   224 | `	}` |
|        - |   225 | `	/* Install the function in the corresponding hashtable */` |
|   865652 |   226 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   865652 |   227 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   228 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   229 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   230 | `		return rc;` |
|        - |   231 | `	}` |
|        - |   232 | `	/* User function successfully installed */` |
|   865652 |   233 | `	return SXRET_OK;` |
|   433822 |   234 |  |
|        - |   235 | `/*` |
|        - |   236 | ` * Initialize a VM function.` |
|        - |   237 | ` */` |
|    94266 |   238 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   239 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   240 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   241 | `	const char *zName,  /* Function name */` |
|        - |   242 | `	sxu32 nByte,        /* zName length */` |
|        - |   243 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   244 | `	void *pUserData     /* Function private data */` |
|        - |   245 | `	)` |
|        2 |   246 |  |
|        - |   247 | `	/* Zero the structure */` |
|    94268 |   248 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   249 | `	/* Initialize structure fields */` |
|        - |   250 | `	/* Arguments container */` |
|    94268 |   251 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   252 | `	/* Static variable container */` |
|    94268 |   253 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   254 | `	/* Bytecode container */` |
|    94268 |   255 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   256 | `    /* Preallocate some instruction slots */` |
|    94268 |   257 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   258 | `	/* Closure environment */` |
|    94268 |   259 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|    94268 |   260 | `	pFunc->iFlags = iFlags;` |
|    94268 |   261 | `	pFunc->pUserData = pUserData;` |
|    94268 |   262 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|    94268 |   263 | `	return SXRET_OK;` |
|        2 |   264 |  |
|        - |   265 | `/*` |
|        - |   266 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   267 | ` */` |
|   341804 |   268 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   269 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   270 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   271 | `	SyString *pName     /* Function name */` |
|        - |   272 | `	)` |
|        2 |   273 |  |
|        - |   274 | `	SyHashEntry *pEntry;` |
|        - |   275 | `	sxi32 rc;` |
|   341806 |   276 | `	if( pName == 0 ){` |
|        - |   277 | `		/* Use the built-in name */` |
|    29440 |   278 | `		pName = &pFunc->sName;` |
|    14719 |   279 | `	}` |
|        - |   280 | `	/* Check for duplicates (functions with the same name) first */` |
|   341806 |   281 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   341806 |   282 | `	if( pEntry ){` |
|   265570 |   283 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   265570 |   284 | `		if( pLink != pFunc ){` |
|        - |   285 | `			/* Link */` |
|      179 |   286 | `			pFunc->pNextName = pLink;` |
|      179 |   287 | `			pEntry->pUserData = pFunc;` |
|       89 |   288 | `		}` |
|   265570 |   289 | `		return SXRET_OK;` |
|        - |   290 | `	}` |
|        - |   291 | `	/* First time seen */` |
|    76238 |   292 | `	pFunc->pNextName = 0;` |
|    76238 |   293 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    76238 |   294 | `	return rc;` |
|   170904 |   295 |  |
|        - |   296 | `/*` |
|        - |   297 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   298 | ` */` |
|    27000 |   299 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   300 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   301 | `	ph7_class *pClass /* Target Class */` |
|        - |   302 | `	)` |
|        2 |   303 |  |
|    27002 |   304 | `	SyString *pName = &pClass->sName;` |
|        - |   305 | `	SyHashEntry *pEntry;` |
|        - |   306 | `	sxi32 rc;` |
|        - |   307 | `	/* Check for duplicates */` |
|    27002 |   308 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    27002 |   309 | `	if( pEntry ){` |
|       31 |   310 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   311 | `		/* Link entry with the same name */` |
|       31 |   312 | `		pClass->pNextName = pLink;` |
|       31 |   313 | `		pEntry->pUserData = pClass;` |
|       31 |   314 | `		return SXRET_OK;` |
|        - |   315 | `	}` |
|    26972 |   316 | `	pClass->pNextName = 0;` |
|        - |   317 | `	/* Perform a simple hashtable insertion */` |
|    26972 |   318 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    26972 |   319 | `	return rc;` |
|    13502 |   320 |  |
|        - |   321 | `/*` |
|        - |   322 | ` * Instruction builder interface.` |
|        - |   323 | ` */` |
|  2507208 |   324 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  2507210 |   336 | `	sInstr.iOp = (sxu8)iOp;` |
|  2507210 |   337 | `	sInstr.iP1 = iP1;` |
|  2507210 |   338 | `	sInstr.iP2 = iP2;` |
|  2507210 |   339 | `	sInstr.p3  = p3;` |
|  2507210 |   340 | `	if( pIndex ){` |
|        - |   341 | `		/* Instruction index in the bytecode array */` |
|   159842 |   342 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    79920 |   343 | `	}` |
|        - |   344 | `	/* Finally,record the instruction */` |
|  2507210 |   345 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  2507210 |   346 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   347 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   348 | `		/* Fall throw */` |
|      ! 0 |   349 | `	}` |
|  2507210 |   350 | `	return rc;` |
|        2 |   351 |  |
|        - |   352 | `/*` |
|        - |   353 | ` * Swap the current bytecode container with the given one.` |
|        - |   354 | ` */` |
|   229140 |   355 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   356 |  |
|   229142 |   357 | `	if( pContainer == 0 ){` |
|        - |   358 | `		/* Point to the default container */` |
|      ! 0 |   359 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   360 | `	}else{` |
|        - |   361 | `		/* Change container */` |
|   229142 |   362 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   363 | `	}` |
|   229142 |   364 | `	return SXRET_OK;` |
|        2 |   365 |  |
|        - |   366 | `/*` |
|        - |   367 | ` * Return the current bytecode container.` |
|        - |   368 | ` */` |
|   114570 |   369 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   370 |  |
|   114572 |   371 | `	return pVm->pByteContainer;` |
|        2 |   372 |  |
|        - |   373 | `/*` |
|        - |   374 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   375 | ` */` |
|   157536 |   376 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   377 |  |
|        - |   378 | `	VmInstr *pInstr;` |
|   157538 |   379 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   157538 |   380 | `	return pInstr;` |
|        2 |   381 |  |
|        - |   382 | `/*` |
|        - |   383 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   384 | ` */` |
|   702158 |   385 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   386 |  |
|   702160 |   387 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   388 |  |
|        - |   389 | `/*` |
|        - |   390 | ` * Pop the last VM instruction.` |
|        - |   391 | ` */` |
|   149502 |   392 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   393 |  |
|   149504 |   394 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   395 |  |
|        - |   396 | `/*` |
|        - |   397 | ` * Peek the last VM instruction.` |
|        - |   398 | ` */` |
|   395964 |   399 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   400 |  |
|   395966 |   401 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   402 |  |
|    11370 |   403 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   404 |  |
|        - |   405 | `	VmInstr *aInstr;` |
|        - |   406 | `	sxu32 n;` |
|    11372 |   407 | `	n = SySetUsed(pVm->pByteContainer);` |
|    11372 |   408 | `	if( n < 2 ){` |
|      ! 0 |   409 | `		return 0;` |
|        - |   410 | `	}` |
|    11372 |   411 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    11372 |   412 | `	return &aInstr[n - 2];` |
|     5687 |   413 |  |
|        - |   414 | `/*` |
|        - |   415 | ` * Allocate a new virtual machine frame.` |
|        - |   416 | ` */` |
|    13822 |   417 | `static VmFrame * VmNewFrame(` |
|        - |   418 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   419 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   420 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   421 | `	)` |
|        2 |   422 |  |
|        - |   423 | `	VmFrame *pFrame;` |
|        - |   424 | `	/* Allocate a new vm frame */` |
|    13824 |   425 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    13824 |   426 | `	if( pFrame == 0 ){` |
|      ! 0 |   427 | `		return 0;` |
|        - |   428 | `	}` |
|        - |   429 | `	/* Zero the structure */` |
|    13824 |   430 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   431 | `	/* Initialize frame fields */` |
|    13824 |   432 | `	pFrame->pUserData = pUserData;` |
|    13824 |   433 | `	pFrame->pThis = pThis;` |
|    13824 |   434 | `	pFrame->pVm = pVm;` |
|    13824 |   435 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    13824 |   436 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    13824 |   437 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    13824 |   438 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    13824 |   439 | `	return pFrame;` |
|     6913 |   440 |  |
|        - |   441 | `/*` |
|        - |   442 | ` * Enter a VM frame.` |
|        - |   443 | ` */` |
|    13822 |   444 | `static sxi32 VmEnterFrame(` |
|        - |   445 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   446 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   447 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   448 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   449 | `	)` |
|        2 |   450 |  |
|        - |   451 | `	VmFrame *pFrame;` |
|        - |   452 | `	/* Allocate a new frame */` |
|    13824 |   453 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    13824 |   454 | `	if( pFrame == 0 ){` |
|      ! 0 |   455 | `		return SXERR_MEM;` |
|        - |   456 | `	}` |
|        - |   457 | `	/* Link to the list of active VM frame */` |
|    13824 |   458 | `	pFrame->pParent = pVm->pFrame;` |
|    13824 |   459 | `	pVm->pFrame = pFrame;` |
|    13824 |   460 | `	if( ppFrame ){` |
|        - |   461 | `		/* Write a pointer to the new VM frame */` |
|    11596 |   462 | `		*ppFrame = pFrame;` |
|     5797 |   463 | `	}` |
|    13824 |   464 | `	return SXRET_OK;` |
|     6913 |   465 |  |
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
|       95 |   484 | `	while( pFrame ){` |
|       95 |   485 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   486 | `			/* Query the current frame */` |
|       49 |   487 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       49 |   488 | `			if( pEntry ){` |
|        - |   489 | `				/* Variable found */` |
|       49 |   490 | `				break;` |
|        - |   491 | `			}` |
|      ! 0 |   492 | `		}` |
|        - |   493 | `		/* Point to the upper frame */` |
|       47 |   494 | `		pFrame = pFrame->pParent;` |
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
|    11588 |   512 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   513 |  |
|    11590 |   514 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    11590 |   515 | `	if( pCurFrame ){` |
|        - |   516 | `		/* Unlink from the list of active VM frame */` |
|    11590 |   517 | `		pVm->pFrame = pCurFrame->pParent;` |
|    11590 |   518 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   519 | `			VmSlot  *aSlot;` |
|        - |   520 | `			sxu32 n;` |
|        - |   521 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    11572 |   522 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    83660 |   523 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   524 | `				/* Unset the local variable */` |
|    72090 |   525 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    36046 |   526 | `			}` |
|        - |   527 | `			/* Remove local reference */` |
|    11572 |   528 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    83712 |   529 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    72142 |   530 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    36072 |   531 | `			}` |
|     5785 |   532 | `		}` |
|        - |   533 | `		/* Release internal containers */` |
|    11590 |   534 | `		SyHashRelease(&pCurFrame->hVar);` |
|    11590 |   535 | `		SySetRelease(&pCurFrame->sArg);` |
|    11590 |   536 | `		SySetRelease(&pCurFrame->sLocal);` |
|    11590 |   537 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   538 | `		/* Release the whole structure */` |
|    11590 |   539 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     5794 |   540 | `	}` |
|    11590 |   541 |  |
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
|    82740 |   658 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   659 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   660 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   661 | `	)` |
|        2 |   662 |  |
|        - |   663 | `	ph7_class_method *pMeth;` |
|        - |   664 | `	ph7_class_attr *pAttr;` |
|        - |   665 | `	SyHashEntry *pEntry;` |
|        - |   666 | `	sxi32 rc;` |
|        - |   667 | `	/* Reset the loop cursor */` |
|    82742 |   668 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   669 | `	/* Process only static and constant attribute */` |
|   319616 |   670 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   671 | `		/* Extract the current attribute */` |
|   195506 |   672 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   195506 |   673 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|    82742 |   695 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |   696 | `		/* Do not mount interface methods since they are signatures only.` |
|        - |   697 | `		 */` |
|    44540 |   698 | `		return SXRET_OK;` |
|        - |   699 | `	}` |
|        - |   700 | `	/* Create constructor alias if not yet done */` |
|    38204 |   701 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   702 | `		/* User constructor with the same base class name */` |
|      212 |   703 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      212 |   704 | `		if( pEntry ){` |
|      ! 0 |   705 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   706 | `			/* Create the alias */` |
|      ! 0 |   707 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   708 | `		}` |
|      105 |   709 | `	}` |
|        - |   710 | `	/* Install the methods now */` |
|    38204 |   711 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   369677 |   712 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   312374 |   713 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   312374 |   714 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   312368 |   715 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   312368 |   716 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   717 | `				return rc;` |
|        - |   718 | `			}` |
|   156183 |   719 | `		}` |
|        2 |   720 | `	}` |
|        - |   721 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    38204 |   722 | `	pClass->bMounted = TRUE;` |
|    38204 |   723 | `	return SXRET_OK;` |
|    41372 |   724 |  |
|        - |   725 | `/*` |
|        - |   726 | ` * Allocate a private frame for attributes of the given` |
|        - |   727 | ` * class instance (Object in the PHP jargon).` |
|        - |   728 | ` */` |
|     1024 |   729 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   730 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   731 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   732 | `	)` |
|        2 |   733 |  |
|     1026 |   734 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   735 | `	ph7_class_attr *pAttr;` |
|        - |   736 | `	SyHashEntry *pEntry;` |
|        - |   737 | `	sxi32 rc;` |
|        - |   738 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1026 |   739 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4424 |   740 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   741 | `		VmClassAttr *pVmAttr;` |
|        - |   742 | `		/* Extract the current attribute */` |
|     3400 |   743 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3400 |   744 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3400 |   745 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   746 | `			return SXERR_MEM;` |
|        - |   747 | `		}` |
|     3400 |   748 | `		pVmAttr->pAttr = pAttr;` |
|     3400 |   749 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   750 | `			ph7_value *pMemObj;` |
|        - |   751 | `			/* Reserve a memory object for this attribute */` |
|     3394 |   752 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3394 |   753 | `			if( pMemObj == 0 ){` |
|      ! 0 |   754 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   755 | `				return SXERR_MEM;` |
|        - |   756 | `			}` |
|     3394 |   757 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3394 |   758 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   759 | `				/* Initialize attribute default value (any complex expression) */` |
|     1108 |   760 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      553 |   761 | `			}` |
|     3394 |   762 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3394 |   763 | `			if( rc != SXRET_OK ){` |
|        - |   764 | `				VmSlot sSlot;` |
|        - |   765 | `				/* Restore memory object */` |
|      ! 0 |   766 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   767 | `				sSlot.pUserData = 0;` |
|      ! 0 |   768 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   769 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   770 | `				return SXERR_MEM;` |
|        - |   771 | `			}` |
|        - |   772 | `			/* Install attribute in the reference table */` |
|     3394 |   773 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1698 |   774 | `		}else{` |
|        - |   775 | `			/* Install static/constant attribute */` |
|        8 |   776 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   777 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   778 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   779 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   780 | `				return SXERR_MEM;` |
|        - |   781 | `			}` |
|        - |   782 | `		}` |
|        2 |   783 | `	}` |
|     1026 |   784 | `	return SXRET_OK;` |
|      514 |   785 |  |
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
|   274394 |   797 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   798 |  |
|        - |   799 | `	ph7_value *pObj;` |
|        - |   800 | `	sxi32 rc;` |
|   274396 |   801 | `	if( pIndex ){` |
|        - |   802 | `		/* Object index in the object table */` |
|   267712 |   803 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   133855 |   804 | `	}` |
|        - |   805 | `	/* Reserve a slot for the new object */` |
|   274396 |   806 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   274396 |   807 | `	if( rc != SXRET_OK ){` |
|        - |   808 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   809 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   810 | `		 */` |
|      ! 0 |   811 | `		return 0;` |
|        - |   812 | `	}` |
|   274396 |   813 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   274396 |   814 | `	return pObj;` |
|   137199 |   815 |  |
|        - |   816 | `/*` |
|        - |   817 | ` * Reserve a memory object.` |
|        - |   818 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   819 | ` */` |
|  2135886 |   820 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   821 |  |
|        - |   822 | `	ph7_value *pObj;` |
|        - |   823 | `	sxi32 rc;` |
|  2135888 |   824 | `	if( pIndex ){` |
|        - |   825 | `		/* Object index in the object table */` |
|  2135888 |   826 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1067943 |   827 | `	}` |
|        - |   828 | `	/* Reserve a slot for the new object */` |
|  2135888 |   829 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2135888 |   830 | `	if( rc != SXRET_OK ){` |
|        - |   831 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   832 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   833 | `		 */` |
|      ! 0 |   834 | `		return 0;` |
|        - |   835 | `	}` |
|  2135888 |   836 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2135888 |   837 | `	return pObj;` |
|  1067945 |   838 |  |
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
|     2228 |  1191 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1192 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1193 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1194 | `	 )` |
|        2 |  1195 |  |
|        - |  1196 | `	SyString sBuiltin;` |
|        - |  1197 | `	ph7_value *pObj;` |
|        - |  1198 | `	sxi32 rc;` |
|        - |  1199 | `	/* Zero the structure */` |
|     2230 |  1200 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1201 | `	/* Initialize VM fields */` |
|     2230 |  1202 | `	pVm->pEngine = &(*pEngine);` |
|     2230 |  1203 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1204 | `	/* Instructions containers */` |
|     2230 |  1205 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2230 |  1206 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2230 |  1207 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1208 | `	/* Object containers */` |
|     2230 |  1209 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2230 |  1210 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1211 | `	/* Virtual machine internal containers */` |
|     2230 |  1212 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2230 |  1213 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2230 |  1214 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2230 |  1215 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2230 |  1216 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2230 |  1217 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2230 |  1218 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2230 |  1219 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2230 |  1220 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2230 |  1221 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2230 |  1222 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2230 |  1223 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2230 |  1224 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2230 |  1225 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2230 |  1226 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|        - |  1227 | `	/* Configuration containers */` |
|     2230 |  1228 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2230 |  1229 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2230 |  1230 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2230 |  1231 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2230 |  1232 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1233 | `	/* Error callbacks containers */` |
|     2230 |  1234 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2230 |  1235 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2230 |  1236 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2230 |  1237 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2230 |  1238 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1239 | `	/* Set a default recursion limit */` |
|        - |  1240 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2230 |  1241 | `	pVm->nMaxDepth = 32;` |
|        - |  1242 | `#else` |
|        - |  1243 | `	pVm->nMaxDepth = 16;` |
|        - |  1244 | `#endif` |
|        - |  1245 | `	/* Default assertion flags */` |
|     2230 |  1246 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1247 | `	/* JSON return status */` |
|     2230 |  1248 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1249 | `	/* PRNG context */` |
|     2230 |  1250 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1251 | `	/* Install the null constant */` |
|     2230 |  1252 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2230 |  1253 | `	if( pObj == 0 ){` |
|      ! 0 |  1254 | `		rc = SXERR_MEM;` |
|      ! 0 |  1255 | `		goto Err;` |
|        - |  1256 | `	}` |
|     2230 |  1257 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1258 | `	/* Install the boolean TRUE constant */` |
|     2230 |  1259 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2230 |  1260 | `	if( pObj == 0 ){` |
|      ! 0 |  1261 | `		rc = SXERR_MEM;` |
|      ! 0 |  1262 | `		goto Err;` |
|        - |  1263 | `	}` |
|     2230 |  1264 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1265 | `	/* Install the boolean FALSE constant */` |
|     2230 |  1266 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2230 |  1267 | `	if( pObj == 0 ){` |
|      ! 0 |  1268 | `		rc = SXERR_MEM;` |
|      ! 0 |  1269 | `		goto Err;` |
|        - |  1270 | `	}` |
|     2230 |  1271 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1272 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1273 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1274 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2230 |  1275 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2230 |  1276 | `	if( pObj == 0 ){` |
|      ! 0 |  1277 | `		rc = SXERR_MEM;` |
|      ! 0 |  1278 | `		goto Err;` |
|        - |  1279 | `	}` |
|     2230 |  1280 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1281 | `	/* Create the global frame */` |
|     2230 |  1282 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2230 |  1283 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1284 | `		goto Err;` |
|        - |  1285 | `	}` |
|        - |  1286 | `	/* Initialize the code generator */` |
|     2230 |  1287 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2230 |  1288 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1289 | `		goto Err;` |
|        - |  1290 | `	}` |
|        - |  1291 | `	/* VM correctly initialized,set the magic number */` |
|     2230 |  1292 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2230 |  1293 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1294 | `	/* Compile the built-in library */` |
|     2230 |  1295 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1296 | `	/* Reset the code generator */` |
|     2230 |  1297 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2230 |  1298 | `	return SXRET_OK;` |
|      ! 0 |  1299 | `Err:` |
|      ! 0 |  1300 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1301 | `	return rc;` |
|     1116 |  1302 |  |
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
|    29194 |  1332 | `static ph7_value * VmNewOperandStack(` |
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
|    29196 |  1345 | `	nInstr += VM_STACK_GUARD;` |
|    29196 |  1346 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    29196 |  1347 | `	if( pStack == 0 ){` |
|      ! 0 |  1348 | `		return 0;` |
|        - |  1349 | `	}` |
|        - |  1350 | `	/* Initialize the operand stack */` |
|  1853366 |  1351 | `	while( nInstr > 0 ){` |
|  1824172 |  1352 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1824172 |  1353 | `		--nInstr;` |
|        2 |  1354 | `	}` |
|        - |  1355 | `	/* Ready for bytecode execution */` |
|    29196 |  1356 | `	return pStack;` |
|    14599 |  1357 |  |
|        - |  1358 | `/* Forward declaration */` |
|        - |  1359 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1360 | `/*` |
|        - |  1361 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1362 | ` * This routine gets called by the PH7 engine after` |
|        - |  1363 | ` * successful compilation of the target PHP program.` |
|        - |  1364 | ` */` |
|     1990 |  1365 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1366 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1367 | `	)` |
|        2 |  1368 |  |
|        - |  1369 | `	SyHashEntry *pEntry;` |
|        - |  1370 | `	sxi32 rc;` |
|     1992 |  1371 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1372 | `		/* Initialize your VM first */` |
|      ! 0 |  1373 | `		return SXERR_CORRUPT;` |
|        - |  1374 | `	}` |
|        - |  1375 | `	/* Mark the VM ready for byte-code execution */` |
|     1992 |  1376 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1377 | `	/* Release the code generator now we have compiled our program */` |
|     1992 |  1378 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1379 | `	/* Emit the DONE instruction */` |
|     1992 |  1380 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     1992 |  1381 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1382 | `		return SXERR_MEM;` |
|        - |  1383 | `	}` |
|        - |  1384 | `	/* Script return value */` |
|     1992 |  1385 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1386 | `	/* Allocate a new operand stack */` |
|     1992 |  1387 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     1992 |  1388 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1389 | `		return SXERR_MEM;` |
|        - |  1390 | `	}` |
|        - |  1391 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1392 | `	 * private data. */` |
|     1992 |  1393 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     1992 |  1394 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1395 | `	/* Allocate the reference table */` |
|     1992 |  1396 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     1992 |  1397 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     1992 |  1398 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1399 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1400 | `		return SXERR_MEM;` |
|        - |  1401 | `	}` |
|        - |  1402 | `	/* Zero the reference table */` |
|     1992 |  1403 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1404 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     1992 |  1405 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     1992 |  1406 | `	if( rc != SXRET_OK ){` |
|        - |  1407 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1408 | `		return rc;` |
|        - |  1409 | `	}` |
|        - |  1410 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     1992 |  1411 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     1992 |  1412 | `	if( rc != SXRET_OK ){` |
|        - |  1413 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1414 | `		return rc;` |
|        - |  1415 | `	}` |
|        - |  1416 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     1992 |  1417 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1418 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     1992 |  1419 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1420 | `	/* Initialize and install static and constants class attributes */` |
|     1992 |  1421 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    25908 |  1422 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    23918 |  1423 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    23918 |  1424 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1425 | `			return rc;` |
|        - |  1426 | `		}` |
|        2 |  1427 | `	}` |
|        - |  1428 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     1992 |  1429 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1430 | `	/* VM is ready for bytecode execution */` |
|     1992 |  1431 | `	return SXRET_OK;` |
|      997 |  1432 |  |
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
|     1982 |  1452 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1453 |  |
|        - |  1454 | `	/* Set the stale magic number */` |
|     1984 |  1455 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1456 | `	/* Release the private memory subsystem */` |
|     1984 |  1457 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     1984 |  1458 | `	return SXRET_OK;` |
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
|   556100 |  1470 | `static sxi32 VmInitCallContext(` |
|        - |  1471 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1472 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1473 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1474 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1475 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1476 | `	)` |
|        2 |  1477 |  |
|   556102 |  1478 | `	pOut->pFunc = pFunc;` |
|   556102 |  1479 | `	pOut->pVm   = pVm;` |
|   556102 |  1480 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   556102 |  1481 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1482 | `	/* Assume a null return value */` |
|   556102 |  1483 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   556102 |  1484 | `	pOut->pRet = pRet;` |
|   556102 |  1485 | `	pOut->iFlags = iFlags;` |
|   556102 |  1486 | `	return SXRET_OK;` |
|        2 |  1487 |  |
|        - |  1488 | `/*` |
|        - |  1489 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1490 | ` * left behind.` |
|        - |  1491 | ` */` |
|   556100 |  1492 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1493 |  |
|        - |  1494 | `	sxu32 n;` |
|   556102 |  1495 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6498 |  1496 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    18486 |  1497 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    11990 |  1498 | `			if( apObj[n] == 0 ){` |
|        - |  1499 | `				/* Already released */` |
|      250 |  1500 | `				continue;` |
|        - |  1501 | `			}` |
|    11742 |  1502 | `			PH7_MemObjRelease(apObj[n]);` |
|    11742 |  1503 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     5872 |  1504 | `		}` |
|     6498 |  1505 | `		SySetRelease(&pCtx->sVar);` |
|     3248 |  1506 | `	}` |
|   556102 |  1507 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
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
|   556102 |  1523 |  |
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
|  3364282 |  1554 | `static void VmPopOperand(` |
|        - |  1555 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1556 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1557 | `	)` |
|        2 |  1558 |  |
|  3364284 |  1559 | `	ph7_value *pTos = *ppTos;` |
|  7115106 |  1560 | `	while( nPop > 0 ){` |
|  3750824 |  1561 | `		PH7_MemObjRelease(pTos);` |
|  3750824 |  1562 | `		pTos--;` |
|  3750824 |  1563 | `		nPop--;` |
|        2 |  1564 | `	}` |
|        - |  1565 | `	/* Top of the stack */` |
|  3364284 |  1566 | `	*ppTos = pTos;` |
|  3364284 |  1567 |  |
|        - |  1568 | `/*` |
|        - |  1569 | ` * Reserve a memory object.` |
|        - |  1570 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1571 | ` */` |
|  2979908 |  1572 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1573 |  |
|  2979910 |  1574 | `	ph7_value *pObj = 0;` |
|        - |  1575 | `	VmSlot *pSlot;` |
|        - |  1576 | `	sxu32 nIdx;` |
|        - |  1577 | `	/* Check for a free slot */` |
|  2979910 |  1578 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  2979910 |  1579 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  2979910 |  1580 | `	if( pSlot ){` |
|   844024 |  1581 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   844024 |  1582 | `		nIdx = pSlot->nIdx;` |
|   422011 |  1583 | `	}` |
|  2979910 |  1584 | `	if( pObj == 0 ){` |
|        - |  1585 | `		/* Reserve a new memory object */` |
|  2135888 |  1586 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2135888 |  1587 | `		if( pObj == 0 ){` |
|      ! 0 |  1588 | `			return 0;` |
|        - |  1589 | `		}` |
|  1067943 |  1590 | `	}` |
|        - |  1591 | `	/* Set a null default value */` |
|  2979910 |  1592 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  2979910 |  1593 | `	pObj->nIdx = nIdx;` |
|  2979910 |  1594 | `	return pObj;` |
|  1489956 |  1595 |  |
|        - |  1596 | `/*` |
|        - |  1597 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1598 | ` */` |
|    25450 |  1599 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1600 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1601 | `	const char *zKey,  /* Entry key */` |
|        - |  1602 | `	sxu32 nByte,       /* Key length */` |
|        - |  1603 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1604 | `	)` |
|        2 |  1605 |  |
|        - |  1606 | `	ph7_value sKey;` |
|        - |  1607 | `	sxi32 rc;` |
|    25452 |  1608 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    25452 |  1609 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1610 | `	/* Perform the insertion */` |
|    25452 |  1611 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    25452 |  1612 | `	PH7_MemObjRelease(&sKey);` |
|    25452 |  1613 | `	return rc;` |
|        2 |  1614 |  |
|        - |  1615 | `/*` |
|        - |  1616 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1617 | ` * Return a pointer to the variable value on success.` |
|        - |  1618 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1619 | ` */` |
|  3189152 |  1620 | `static ph7_value * VmExtractMemObj(` |
|        - |  1621 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1622 | `	const SyString *pName, /* Variable name */` |
|        - |  1623 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1624 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1625 | `	)` |
|        2 |  1626 |  |
|  3189154 |  1627 | `	int bNullify = FALSE;` |
|        - |  1628 | `	SyHashEntry *pEntry;` |
|        - |  1629 | `	VmFrame *pFrame;` |
|        - |  1630 | `	ph7_value *pObj;` |
|        - |  1631 | `	sxu32 nIdx;` |
|        - |  1632 | `	sxi32 rc;` |
|        - |  1633 | `	/* Point to the top active frame */` |
|  3189154 |  1634 | `	pFrame = pVm->pFrame;` |
|  3762582 |  1635 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1636 | `		/* Safely ignore the exception frame */` |
|   573429 |  1637 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        1 |  1638 | `	}` |
|        - |  1639 | `	/* Perform the lookup */` |
|  3189154 |  1640 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1641 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1642 | `		pName = &sAnnon;` |
|        - |  1643 | `		/* Always nullify the object */` |
|      ! 0 |  1644 | `		bNullify = TRUE;` |
|      ! 0 |  1645 | `		bDup = FALSE;` |
|      ! 0 |  1646 | `	}` |
|        - |  1647 | `	/* Check the superglobals table first */` |
|  3189154 |  1648 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3189154 |  1649 | `	if( pEntry == 0 ){` |
|        - |  1650 | `		/* Query the top active frame */` |
|  3189118 |  1651 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3189118 |  1652 | `		if( pEntry == 0 ){` |
|    78246 |  1653 | `			char *zName = (char *)pName->zString;` |
|        - |  1654 | `			VmSlot sLocal;` |
|    78246 |  1655 | `			if( !bCreate ){` |
|        - |  1656 | `				/* Do not create the variable,return NULL instead */` |
|      632 |  1657 | `				return 0;` |
|        - |  1658 | `			}` |
|        - |  1659 | `			/* No such variable,automatically create a new one and install` |
|        - |  1660 | `			 * it in the current frame.` |
|        - |  1661 | `			 */` |
|    77616 |  1662 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    77616 |  1663 | `			if( pObj == 0 ){` |
|      ! 0 |  1664 | `				return 0;` |
|        - |  1665 | `			}` |
|    77616 |  1666 | `			nIdx = pObj->nIdx;` |
|    77616 |  1667 | `			if( bDup ){` |
|        - |  1668 | `				/* Duplicate name */` |
|      164 |  1669 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      164 |  1670 | `				if( zName == 0 ){` |
|      ! 0 |  1671 | `					return 0;` |
|        - |  1672 | `				}` |
|       81 |  1673 | `			}` |
|        - |  1674 | `			/* Link to the top active VM frame */` |
|    77616 |  1675 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    77616 |  1676 | `			if( rc != SXRET_OK ){` |
|        - |  1677 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1678 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1679 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1680 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1681 | `				return 0;` |
|        - |  1682 | `			}` |
|    77616 |  1683 | `			if( pFrame->pParent != 0 ){` |
|        - |  1684 | `				/* Local variable */` |
|    72090 |  1685 | `				sLocal.nIdx = nIdx;` |
|    72090 |  1686 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    36046 |  1687 | `			}else{` |
|        - |  1688 | `				/* Register in the $GLOBALS array */` |
|     5528 |  1689 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1690 | `			}` |
|        - |  1691 | `			/* Install in the reference table */` |
|    77616 |  1692 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1693 | `			/* Save object index */` |
|    77616 |  1694 | `			pObj->nIdx = nIdx;` |
|    38809 |  1695 | `		}else{` |
|        - |  1696 | `			/* Extract variable contents */` |
|  3110874 |  1697 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3110874 |  1698 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3110874 |  1699 | `			if( bNullify && pObj ){` |
|      ! 0 |  1700 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1701 | `			}` |
|        - |  1702 | `		}` |
|  1594355 |  1703 | `	}else{` |
|        - |  1704 | `		/* Superglobal */` |
|       38 |  1705 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1706 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1707 | `	}` |
|  3188524 |  1708 | `	return pObj;` |
|  1594688 |  1709 |  |
|        - |  1710 | `/*` |
|        - |  1711 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1712 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1713 | ` */` |
|     2016 |  1714 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1715 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1716 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1717 | `	sxu32 nByte        /* zName length */` |
|        - |  1718 | `	)` |
|        2 |  1719 |  |
|        - |  1720 | `	SyHashEntry *pEntry;` |
|        - |  1721 | `	ph7_value *pValue;` |
|        - |  1722 | `	sxu32 nIdx;` |
|        - |  1723 | `	/* Query the superglobal table */` |
|     2018 |  1724 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2018 |  1725 | `	if( pEntry == 0 ){` |
|        - |  1726 | `		/* No such entry */` |
|      ! 0 |  1727 | `		return 0;` |
|        - |  1728 | `	}` |
|        - |  1729 | `	/* Extract the superglobal index in the global object pool */` |
|     2018 |  1730 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1731 | `	/* Extract the variable value  */` |
|     2018 |  1732 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2018 |  1733 | `	return pValue;` |
|     1010 |  1734 |  |
|        - |  1735 | `/*` |
|        - |  1736 | ` * Perform a raw hashmap insertion.` |
|        - |  1737 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1738 | ` */` |
|     2014 |  1739 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1740 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1741 | `	const char *zKey,   /* Entry key */` |
|        - |  1742 | `	int nKeylen,        /* zKey length*/` |
|        - |  1743 | `	const char *zData,  /* Entry data */` |
|        - |  1744 | `	int nLen            /* zData length */` |
|        - |  1745 | `	)` |
|        2 |  1746 |  |
|        - |  1747 | `	ph7_value sKey,sValue;` |
|        - |  1748 | `	sxi32 rc;` |
|     2016 |  1749 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2016 |  1750 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2016 |  1751 | `	if( zKey ){` |
|     1994 |  1752 | `		if( nKeylen < 0 ){` |
|     1994 |  1753 | `			nKeylen = (int)SyStrlen(zKey);` |
|      996 |  1754 | `		}` |
|     1994 |  1755 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|      996 |  1756 | `	}` |
|     2016 |  1757 | `	if( zData ){` |
|     2016 |  1758 | `		if( nLen < 0 ){` |
|        - |  1759 | `			/* Compute length automatically */` |
|      ! 0 |  1760 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1761 | `		}` |
|     2016 |  1762 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1007 |  1763 | `	}` |
|        - |  1764 | `	/* Perform the insertion */` |
|     2016 |  1765 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2016 |  1766 | `	PH7_MemObjRelease(&sKey);` |
|     2016 |  1767 | `	PH7_MemObjRelease(&sValue);` |
|     2016 |  1768 | `	return rc;` |
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
|    31864 |  1783 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1784 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1785 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1786 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1787 | `	)` |
|        2 |  1788 |  |
|    31866 |  1789 | `	sxi32 rc = SXRET_OK;` |
|    31866 |  1790 | `	switch(nOp){` |
|      995 |  1791 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     1992 |  1792 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     1992 |  1793 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1794 | `		/* VM output consumer callback */` |
|        - |  1795 | `#ifdef UNTRUST` |
|        - |  1796 | `		if( xConsumer == 0 ){` |
|        - |  1797 | `			rc = SXERR_CORRUPT;` |
|        - |  1798 | `			break;` |
|        - |  1799 | `		}` |
|        - |  1800 | `#endif` |
|        - |  1801 | `		/* Install the output consumer */` |
|     1992 |  1802 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     1992 |  1803 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     1992 |  1804 | `		break;` |
|        - |  1805 | `							   }` |
|      995 |  1806 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1807 | `		/* Import path */` |
|        - |  1808 | `		  const char *zPath;` |
|        - |  1809 | `		  SyString sPath;` |
|     1992 |  1810 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1811 | `#if defined(UNTRUST)` |
|        - |  1812 | `		  if( zPath == 0 ){` |
|        - |  1813 | `			  rc = SXERR_EMPTY;` |
|        - |  1814 | `			  break;` |
|        - |  1815 | `		  }` |
|        - |  1816 | `#endif` |
|     1992 |  1817 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1818 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1819 | `#ifdef __WINNT__` |
|        2 |  1820 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1821 | `#endif` |
|     3982 |  1822 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1823 | `		  /* Remove leading and trailing white spaces */` |
|     1992 |  1824 | `		  SyStringFullTrim(&sPath);` |
|     1992 |  1825 | `		  if( sPath.nByte > 0 ){` |
|        - |  1826 | `			  /* Store the path in the corresponding conatiner */` |
|     1992 |  1827 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      995 |  1828 | `		  }` |
|     1992 |  1829 | `		  break;` |
|        - |  1830 | `									 }` |
|      995 |  1831 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1832 | `		/* Run-Time Error report */` |
|     1992 |  1833 | `		pVm->bErrReport = 1;` |
|     1992 |  1834 | `		break;` |
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
|     9950 |  1856 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1857 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1858 | `		/* Create a new superglobal/global variable */` |
|    19902 |  1859 | `		const char *zName = va_arg(ap,const char *);` |
|    19902 |  1860 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
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
|    19902 |  1871 | `		nByte = SyStrlen(zName);` |
|    19902 |  1872 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1873 | `			/* Check if the superglobal is already installed */` |
|    19902 |  1874 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     9952 |  1875 | `		}else{` |
|        - |  1876 | `			/* Query the top active VM frame */` |
|      ! 0 |  1877 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1878 | `		}` |
|    19902 |  1879 | `		if( pEntry ){` |
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
|    19902 |  1890 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    19902 |  1891 | `			if( pObj == 0 ){` |
|      ! 0 |  1892 | `				rc = SXERR_MEM;` |
|      ! 0 |  1893 | `				break;` |
|        - |  1894 | `			}` |
|    19902 |  1895 | `			nIdx = pObj->nIdx;` |
|        - |  1896 | `			/* Copy value */` |
|    19902 |  1897 | `			PH7_MemObjStore(pValue,pObj);` |
|    19902 |  1898 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1899 | `				/* Install the superglobal */` |
|    19902 |  1900 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     9952 |  1901 | `			}else{` |
|        - |  1902 | `				/* Install in the current frame */` |
|      ! 0 |  1903 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1904 | `			}` |
|    19902 |  1905 | `			if( rc == SXRET_OK ){` |
|        - |  1906 | `				SyHashEntry *pRef;` |
|    19902 |  1907 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    19902 |  1908 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     9952 |  1909 | `				}else{` |
|      ! 0 |  1910 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1911 | `				}` |
|        - |  1912 | `				/* Install in the reference table */` |
|    19902 |  1913 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    19902 |  1914 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1915 | `					/* Register in the $GLOBALS array */` |
|    19902 |  1916 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     9950 |  1917 | `				}` |
|     9950 |  1918 | `			}` |
|        - |  1919 | `		}` |
|    19902 |  1920 | `		break;` |
|        - |  1921 | `									}` |
|      996 |  1922 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1923 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1924 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1925 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1926 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1927 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1928 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     1994 |  1929 | `		const char *zKey   = va_arg(ap,const char *);` |
|     1994 |  1930 | `		const char *zValue = va_arg(ap,const char *);` |
|     1994 |  1931 | `		int nLen = va_arg(ap,int);` |
|        - |  1932 | `		ph7_hashmap *pMap;` |
|        - |  1933 | `		ph7_value *pValue;` |
|     1994 |  1934 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1935 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1936 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     1993 |  1937 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  1938 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  1939 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     1992 |  1940 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  1941 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  1942 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     1992 |  1943 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  1944 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  1945 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     1992 |  1946 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  1947 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  1948 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     1992 |  1949 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  1950 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  1951 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  1952 | `		}else{` |
|        - |  1953 | `			/* Extract the $_SERVER superglobal */` |
|     1992 |  1954 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  1955 | `		}` |
|     1994 |  1956 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1957 | `			/* No such entry */` |
|      ! 0 |  1958 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1959 | `			break;` |
|        - |  1960 | `		}` |
|        - |  1961 | `		/* Point to the hashmap */` |
|     1994 |  1962 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1963 | `		/* Perform the insertion */` |
|     1994 |  1964 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     1994 |  1965 | `		break;` |
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
|     1990 |  2016 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2017 | `		/* Register an IO stream device */` |
|     3982 |  2018 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2019 | `		/* Make sure we are dealing with a valid IO stream */` |
|     5970 |  2020 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     3982 |  2021 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2022 | `				/* Invalid stream */` |
|      ! 0 |  2023 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2024 | `				break;` |
|        - |  2025 | `		}` |
|     3982 |  2026 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2027 | `			/* Make the 'file://' stream the defaut stream device */` |
|     1992 |  2028 | `			pVm->pDefStream = pStream;` |
|      995 |  2029 | `		}` |
|        - |  2030 | `		/* Insert in the appropriate container */` |
|     3982 |  2031 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     3982 |  2032 | `		break;` |
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
|    31866 |  2069 | `	return rc;` |
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
|      526 |  2128 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2129 |  |
|      527 |  2130 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      527 |  2131 | `	sxi32 rc = SXRET_OK;` |
|        - |  2132 | `	/* Append a new line */` |
|        - |  2133 | `#ifdef __WINNT__` |
|        1 |  2134 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2135 | `#else` |
|      526 |  2136 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2137 | `#endif` |
|        - |  2138 | `	/* Invoke the output consumer callback */` |
|      527 |  2139 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      527 |  2140 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  2141 | `		/* Increment output length */` |
|      527 |  2142 | `		pVm->nOutputLen += SyBlobLength(pMsg);` |
|      263 |  2143 | `	}` |
|      527 |  2144 | `	return rc;` |
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
|      906 |  2346 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2347 |  |
|        - |  2348 | `	VmFrame *pFrame;` |
|        - |  2349 | `	ph7_vm_func *pFunc;` |
|      907 |  2350 | `	*pzFuncName = 0;` |
|      907 |  2351 | `	*pnFuncLen = 0;` |
|      907 |  2352 | `	pFrame = pVm->pFrame;` |
|      907 |  2353 | `	if( pFrame == 0 ){` |
|      ! 0 |  2354 | `		return;` |
|        - |  2355 | `	}` |
|      907 |  2356 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  2357 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  2358 | `	}` |
|      907 |  2359 | `	if( pFrame->pParent == 0 ){` |
|      901 |  2360 | `		return;` |
|        - |  2361 | `	}` |
|        7 |  2362 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        7 |  2363 | `	if( pFunc == 0 ){` |
|      ! 0 |  2364 | `		return;` |
|        - |  2365 | `	}` |
|        7 |  2366 | `	*pzFuncName = pFunc->sName.zString;` |
|        7 |  2367 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      454 |  2368 |  |
|        - |  2369 | `/*` |
|        - |  2370 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2371 | ` */` |
|      456 |  2372 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2373 |  |
|        - |  2374 | `	SyBlob sOut;` |
|        - |  2375 | `	SyString *pFile;` |
|      457 |  2376 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2377 | `		return PH7_OK;` |
|        - |  2378 | `	}` |
|      457 |  2379 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2380 | `		zClass = "Exception";` |
|      ! 0 |  2381 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2382 | `	}` |
|      457 |  2383 | `	if( zMsg == 0 ){` |
|      ! 0 |  2384 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2385 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2386 | `	}` |
|      457 |  2387 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      451 |  2388 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      225 |  2389 | `	}` |
|      457 |  2390 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      457 |  2391 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      457 |  2392 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      457 |  2393 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      457 |  2394 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      457 |  2395 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      457 |  2396 | `	if( pFile ){` |
|      457 |  2397 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      457 |  2398 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      457 |  2399 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      228 |  2400 | `	}` |
|      457 |  2401 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      457 |  2402 | `	if( pFile ){` |
|      457 |  2403 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      457 |  2404 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      457 |  2405 | `		if( zFuncName && nFuncLen > 0 ){` |
|        7 |  2406 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        4 |  2407 | `		}else{` |
|      451 |  2408 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2409 | `		}` |
|      228 |  2410 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2411 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2412 | `	}else{` |
|      ! 0 |  2413 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2414 | `	}` |
|      457 |  2415 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      457 |  2416 | `	if( pFile ){` |
|      457 |  2417 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      457 |  2418 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      457 |  2419 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      457 |  2420 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      228 |  2421 | `	}` |
|      457 |  2422 | `	VmCallErrorHandler(pVm,&sOut);` |
|      457 |  2423 | `	SyBlobRelease(&sOut);` |
|      457 |  2424 | `	return PH7_ABORT;` |
|      229 |  2425 |  |
|        - |  2426 | `/*` |
|        - |  2427 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2428 | ` */` |
|      454 |  2429 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
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
|      456 |  2443 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2444 | `		return PH7_ABORT;` |
|        - |  2445 | `	}` |
|      456 |  2446 | `	pVm = pCtx->pVm;` |
|      456 |  2447 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2448 | `		zClass = "Error";` |
|      ! 0 |  2449 | `	}` |
|      456 |  2450 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      456 |  2451 | `	if( pClass == 0 ){` |
|      ! 0 |  2452 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2453 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2454 | `			zClass` |
|        - |  2455 | `			);` |
|        - |  2456 | `	}` |
|      456 |  2457 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      456 |  2458 | `	if( pThis == 0 ){` |
|      ! 0 |  2459 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2460 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2461 | `			);` |
|        - |  2462 | `	}` |
|        - |  2463 |  |
|      456 |  2464 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      456 |  2465 | `	va_start(ap,zFormat);` |
|      456 |  2466 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      456 |  2467 | `	va_end(ap);` |
|        - |  2468 |  |
|      456 |  2469 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      456 |  2470 | `	if( pCons ){` |
|      456 |  2471 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      456 |  2472 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      456 |  2473 | `		apArg[0] = &sArg;` |
|      456 |  2474 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      456 |  2475 | `		PH7_MemObjRelease(&sArg);` |
|      227 |  2476 | `	}` |
|      456 |  2477 | `	SyBlobRelease(&sMsg);` |
|        - |  2478 |  |
|      456 |  2479 | `	pFrame = pVm->pFrame;` |
|      456 |  2480 | `	if( pFrame ){` |
|      468 |  2481 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|       13 |  2482 | `			pFrame = pFrame->pParent;` |
|        1 |  2483 | `		}` |
|      456 |  2484 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      227 |  2485 | `	}` |
|      456 |  2486 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      456 |  2487 | `	PH7_ClassInstanceUnref(pThis);` |
|      456 |  2488 | `	if( rc == SXERR_ABORT ){` |
|      449 |  2489 | `		return PH7_ABORT;` |
|        - |  2490 | `	}` |
|        7 |  2491 | `	return PH7_EXCEPTION;` |
|      229 |  2492 |  |
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
|    29194 |  2545 | `static sxi32 VmByteCodeExec(` |
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
|    29196 |  2561 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    29196 |  2562 | `	if( nTos < 0 ){` |
|    27632 |  2563 | `		pTos = &pStack[-1];` |
|    13817 |  2564 | `	}else{` |
|     1566 |  2565 | `		pTos = &pStack[nTos];` |
|        - |  2566 | `	}` |
|    29196 |  2567 | `	pc = 0;` |
|        - |  2568 | `	/* Execute as much as we can */` |
|  5047364 |  2569 | `	for(;;){` |
|        - |  2570 | `		/* Fetch the instruction to execute */` |
| 10094026 |  2571 | `		pInstr = &aInstr[pc];` |
| 10094026 |  2572 | `		rc = SXRET_OK;` |
|        - |  2573 | `/*` |
|        - |  2574 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2575 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2576 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2577 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2578 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2579 | ` */` |
| 10094026 |  2580 | `		switch(pInstr->iOp){` |
|        - |  2581 | `/*` |
|        - |  2582 | ` * DONE: P1 * *` |
|        - |  2583 | ` *` |
|        - |  2584 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2585 | ` * and return immediately.` |
|        - |  2586 | ` */` |
|    14358 |  2587 | `case PH7_OP_DONE:` |
|    28718 |  2588 | `	if( pInstr->iP1 ){` |
|        - |  2589 | `#ifdef UNTRUST` |
|        - |  2590 | `		if( pTos < pStack ){` |
|        - |  2591 | `			goto Abort;` |
|        - |  2592 | `		}` |
|        - |  2593 | `#endif` |
|    16550 |  2594 | `		if( pLastRef ){` |
|    10684 |  2595 | `			*pLastRef = pTos->nIdx;` |
|     5341 |  2596 | `		}` |
|    16550 |  2597 | `		if( pResult ){` |
|        - |  2598 | `			/* Execution result */` |
|    15772 |  2599 | `			PH7_MemObjStore(pTos,pResult);` |
|     7885 |  2600 | `		}` |
|    16550 |  2601 | `		VmPopOperand(&pTos,1);` |
|    20444 |  2602 | `	}else if( pLastRef ){` |
|        - |  2603 | `		/* Nothing referenced */` |
|      866 |  2604 | `		*pLastRef = SXU32_HIGH;` |
|      432 |  2605 | `	}` |
|    28718 |  2606 | `	goto Done;` |
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
|   221070 |  2654 | `case PH7_OP_JMP:` |
|   442186 |  2655 | `	pc = pInstr->iP2 - 1;` |
|   442186 |  2656 | `	break;` |
|        - |  2657 | `/*` |
|        - |  2658 | ` * JZ: P1 P2 *` |
|        - |  2659 | ` *` |
|        - |  2660 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2661 | ` * entry in the stack if P1 is zero.` |
|        - |  2662 | ` */` |
|   513294 |  2663 | `case PH7_OP_JZ:` |
|        - |  2664 | `#ifdef UNTRUST` |
|        - |  2665 | `	if( pTos < pStack ){` |
|        - |  2666 | `		goto Abort;` |
|        - |  2667 | `	}` |
|        - |  2668 | `#endif` |
|        - |  2669 | `	/* Get a boolean value */` |
|  1026678 |  2670 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       77 |  2671 | `		PH7_MemObjToBool(pTos);` |
|       38 |  2672 | `	}` |
|  1026678 |  2673 | `	if( !pTos->x.iVal ){` |
|        - |  2674 | `		/* Take the jump */` |
|   492892 |  2675 | `		pc = pInstr->iP2 - 1;` |
|   246445 |  2676 | `	}` |
|  1026678 |  2677 | `	if( !pInstr->iP1 ){` |
|   809758 |  2678 | `		VmPopOperand(&pTos,1);` |
|   404900 |  2679 | `	}` |
|  1026678 |  2680 | `	break;` |
|        - |  2681 | `/*` |
|        - |  2682 | ` * JNZ: P1 P2 *` |
|        - |  2683 | ` *` |
|        - |  2684 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2685 | ` * entry in the stack if P1 is zero.` |
|        - |  2686 | ` */` |
|    56598 |  2687 | `case PH7_OP_JNZ:` |
|        - |  2688 | `#ifdef UNTRUST` |
|        - |  2689 | `	if( pTos < pStack ){` |
|        - |  2690 | `		goto Abort;` |
|        - |  2691 | `	}` |
|        - |  2692 | `#endif` |
|        - |  2693 | `	/* Get a boolean value */` |
|   113198 |  2694 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2695 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2696 | `	}` |
|   113198 |  2697 | `	if( pTos->x.iVal ){` |
|        - |  2698 | `		/* Take the jump */` |
|     4144 |  2699 | `		pc = pInstr->iP2 - 1;` |
|     2071 |  2700 | `	}` |
|   113198 |  2701 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2702 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2703 | `	}` |
|   113198 |  2704 | `	break;` |
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
|   392309 |  2718 | `case PH7_OP_POP: {` |
|   784664 |  2719 | `	sxi32 n = pInstr->iP1;` |
|   784664 |  2720 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2721 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2722 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2723 | `	}` |
|   784664 |  2724 | `	VmPopOperand(&pTos,n);` |
|   784664 |  2725 | `	break;` |
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
|    11932 |  2858 | `case PH7_OP_ERR_CTRL:` |
|        - |  2859 | `	/*` |
|        - |  2860 | `	 * TICKET 1433-038:` |
|        - |  2861 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2862 | `	 * use the public API,to control error output.` |
|        - |  2863 | `	 */` |
|    23864 |  2864 | `	break;` |
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
|   792858 |  2912 | `case PH7_OP_LOADC: {` |
|        - |  2913 | `	ph7_value *pObj;` |
|        - |  2914 | `	/* Reserve a room */` |
|  1585762 |  2915 | `	pTos++;` |
|  1585762 |  2916 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1585762 |  2917 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2918 | `			SyHashEntry *pEntry;` |
|        - |  2919 | `			/* Candidate for expansion via user defined callbacks */` |
|    18888 |  2920 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    18888 |  2921 | `			if( pEntry ){` |
|    15188 |  2922 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2923 | `				/* Set a NULL default value */` |
|    15188 |  2924 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    15188 |  2925 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2926 | `				/* Invoke the callback and deal with the expanded value */` |
|    15188 |  2927 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  2928 | `				/* Mark as constant */` |
|    15188 |  2929 | `				pTos->nIdx = SXU32_HIGH;` |
|    15188 |  2930 | `				break;` |
|        - |  2931 | `			}` |
|     1850 |  2932 | `		}` |
|  1570576 |  2933 | `		PH7_MemObjLoad(pObj,pTos);` |
|   785311 |  2934 | `	}else{` |
|        - |  2935 | `		/* Set a NULL value */` |
|      ! 0 |  2936 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2937 | `	}` |
|        - |  2938 | `	/* Mark as constant */` |
|  1570576 |  2939 | `	pTos->nIdx = SXU32_HIGH;` |
|  1570576 |  2940 | `	break;` |
|        - |  2941 | `				  }` |
|        - |  2942 | `/*` |
|        - |  2943 | ` * LOAD: P1 * P3` |
|        - |  2944 | ` *` |
|        - |  2945 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  2946 | ` * from the P3 operand.` |
|        - |  2947 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  2948 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  2949 | ` */` |
|  1408660 |  2950 | `case PH7_OP_LOAD:{` |
|        - |  2951 | `	ph7_value *pObj;` |
|        - |  2952 | `	SyString sName;` |
|  2817542 |  2953 | `	if( pInstr->p3 == 0 ){` |
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
|  2817524 |  2966 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  2967 | `		/* Reserve a room for the target object */` |
|  2817524 |  2968 | `		pTos++;` |
|        - |  2969 | `	}` |
|        - |  2970 | `	/* Extract the requested memory object */` |
|  2817542 |  2971 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2817542 |  2972 | `	if( pObj == 0 ){` |
|      624 |  2973 | `		if( pInstr->iP1 ){` |
|        - |  2974 | `			/* Variable not found,load NULL */` |
|      624 |  2975 | `			if( !pInstr->p3 ){` |
|      ! 0 |  2976 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  2977 | `			}else{` |
|      624 |  2978 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2979 | `			}` |
|      624 |  2980 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1408973 |  2981 | `			break;` |
|      ! 0 |  2982 | `		}else{` |
|        - |  2983 | `			/* Fatal error */` |
|      ! 0 |  2984 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  2985 | `			goto Abort;` |
|        - |  2986 | `		}` |
|        - |  2987 | `	}` |
|        - |  2988 | `	/* Load variable contents */` |
|  2816920 |  2989 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2816920 |  2990 | `	pTos->nIdx = pObj->nIdx;` |
|  2816920 |  2991 | `	break;` |
|        - |  2992 | `				   }` |
|        - |  2993 | `/*` |
|        - |  2994 | ` * LOAD_MAP P1 * *` |
|        - |  2995 | ` *` |
|        - |  2996 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  2997 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  2998 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  2999 | ` */` |
|    17229 |  3000 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3001 | `	ph7_hashmap *pMap;` |
|        - |  3002 | `	/* Allocate a new hashmap instance */` |
|    34460 |  3003 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    34460 |  3004 | `	if( pMap == 0 ){` |
|      ! 0 |  3005 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3006 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3007 | `		goto Abort;` |
|        - |  3008 | `	}` |
|    34460 |  3009 | `	if( pInstr->iP1 > 0 ){` |
|     2078 |  3010 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3011 | `		/* Perform the insertion */` |
|     6298 |  3012 | `		while( pEntry < pTos ){` |
|     4222 |  3013 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3014 | `				/* Insertion by reference */` |
|      142 |  3015 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3016 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3017 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3018 | `					);` |
|       48 |  3019 | `			}else{` |
|        - |  3020 | `				/* Standard insertion */` |
|     6191 |  3021 | `				PH7_HashmapInsert(pMap,` |
|     4126 |  3022 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2063 |  3023 | `					&pEntry[1]` |
|        - |  3024 | `				);` |
|        - |  3025 | `			}` |
|        - |  3026 | `			/* Next pair on the stack */` |
|     4222 |  3027 | `			pEntry += 2;` |
|        2 |  3028 | `		}` |
|        - |  3029 | `		/* Pop P1 elements */` |
|     2078 |  3030 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1038 |  3031 | `	}` |
|        - |  3032 | `	/* Push the hashmap */` |
|    34460 |  3033 | `	pTos++;` |
|    34460 |  3034 | `	pTos->nIdx = SXU32_HIGH;` |
|    34460 |  3035 | `	pTos->x.pOther = pMap;` |
|    34460 |  3036 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    34460 |  3037 | `	break;` |
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
|   235876 |  3093 | `case PH7_OP_LOAD_IDX: {` |
|   471798 |  3094 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   471798 |  3095 | `	ph7_hashmap *pMap = 0;` |
|        - |  3096 | `	ph7_value *pIdx;` |
|   471798 |  3097 | `	pIdx = 0;` |
|   471798 |  3098 | `	if( pInstr->iP1 == 0 ){` |
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
|   471798 |  3115 | `		pIdx = pTos;` |
|   471798 |  3116 | `		pTos--;` |
|        - |  3117 | `	}` |
|   471798 |  3118 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3119 | `		/* String access */` |
|   386472 |  3120 | `		if( pIdx ){` |
|        - |  3121 | `			sxu32 nOfft;` |
|   386472 |  3122 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3123 | `				/* Force an int cast */` |
|      ! 0 |  3124 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3125 | `			}` |
|   386472 |  3126 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   386472 |  3127 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3128 | `				/* Invalid offset,load null */` |
|      ! 0 |  3129 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3130 | `			}else{` |
|   386472 |  3131 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   386472 |  3132 | `				int c = zData[nOfft];` |
|   386472 |  3133 | `				PH7_MemObjRelease(pTos);` |
|   386472 |  3134 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   386472 |  3135 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3136 | `			}` |
|   193259 |  3137 | `		}else{` |
|        - |  3138 | `			/* No available index,load NULL */` |
|      ! 0 |  3139 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3140 | `		}` |
|   386472 |  3141 | `		break;` |
|        - |  3142 | `	}` |
|    85328 |  3143 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3144 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3145 | `			ph7_value *pObj;` |
|      ! 0 |  3146 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3147 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3148 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3149 | `			}` |
|      ! 0 |  3150 | `		}` |
|      ! 0 |  3151 | `	}` |
|    85328 |  3152 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    85328 |  3153 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3154 | `		/* Point to the hashmap */` |
|    85328 |  3155 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    85328 |  3156 | `		if( pIdx ){` |
|        - |  3157 | `			/* Load the desired entry */` |
|    85328 |  3158 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    42663 |  3159 | `		}` |
|    85328 |  3160 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3161 | `			/* Create a new empty entry */` |
|      ! 0 |  3162 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3163 | `			if( rc == SXRET_OK ){` |
|        - |  3164 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3165 | `				pNode = pMap->pLast;` |
|      ! 0 |  3166 | `			}` |
|      ! 0 |  3167 | `		}` |
|    42663 |  3168 | `	}` |
|    85328 |  3169 | `	if( pIdx ){` |
|    85328 |  3170 | `		PH7_MemObjRelease(pIdx);` |
|    42663 |  3171 | `	}` |
|    85328 |  3172 | `	if( rc == SXRET_OK ){` |
|        - |  3173 | `		/* Load entry contents */` |
|    39148 |  3174 | `		if( pMap->iRef < 2 ){` |
|        - |  3175 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3176 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3177 | `			 */` |
|        7 |  3178 | `			pTos->nIdx = SXU32_HIGH;` |
|        7 |  3179 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|        4 |  3180 | `		}else{` |
|    39142 |  3181 | `			pTos->nIdx = pNode->nValIdx;` |
|    39142 |  3182 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    39142 |  3183 | `			PH7_HashmapUnref(pMap);` |
|        - |  3184 | `		}` |
|    19575 |  3185 | `	}else{` |
|        - |  3186 | `		/* No such entry,load NULL */` |
|    46182 |  3187 | `		PH7_MemObjRelease(pTos);` |
|    46182 |  3188 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3189 | `	}` |
|    85328 |  3190 | `	break;` |
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
|   105716 |  3266 | `case PH7_OP_STORE: {` |
|        - |  3267 | `	ph7_value *pObj;` |
|        - |  3268 | `	SyString sName;` |
|        - |  3269 | `#ifdef UNTRUST` |
|        - |  3270 | `	if( pTos < pStack ){` |
|        - |  3271 | `		goto Abort;` |
|        - |  3272 | `	}` |
|        - |  3273 | `#endif` |
|   211434 |  3274 | `	if( pInstr->iP2 ){` |
|        - |  3275 | `		sxu32 nIdx;` |
|        - |  3276 | `		/* Member store operation */` |
|     2768 |  3277 | `		nIdx = pTos->nIdx;` |
|     2768 |  3278 | `		VmPopOperand(&pTos,1);` |
|     2768 |  3279 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3280 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3281 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3282 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3283 | `		}else{` |
|        - |  3284 | `			/* Point to the desired memory object */` |
|     2764 |  3285 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2764 |  3286 | `			if( pObj ){` |
|        - |  3287 | `				/* Perform the store operation */` |
|     2764 |  3288 | `				PH7_MemObjStore(pTos,pObj);` |
|     1381 |  3289 | `			}` |
|        - |  3290 | `		}` |
|   107101 |  3291 | `		break;` |
|   208668 |  3292 | `	}else if( pInstr->p3 == 0 ){` |
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
|   208662 |  3306 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3307 | `	}` |
|        - |  3308 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   208668 |  3309 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   208668 |  3310 | `	if( pObj == 0 ){` |
|      ! 0 |  3311 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3312 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3313 | `		goto Abort;` |
|        - |  3314 | `	}` |
|   208668 |  3315 | `	if( !pInstr->p3 ){` |
|        7 |  3316 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3317 | `	}` |
|        - |  3318 | `	/* Perform the store operation */` |
|   208668 |  3319 | `	PH7_MemObjStore(pTos,pObj);` |
|   208668 |  3320 | `	break;` |
|        - |  3321 | `				   }` |
|        - |  3322 | `/*` |
|        - |  3323 | ` * STORE_IDX:   P1 * P3` |
|        - |  3324 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3325 | ` *` |
|        - |  3326 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3327 | ` */` |
|    77759 |  3328 | `case PH7_OP_STORE_IDX:` |
|        - |  3329 | `case PH7_OP_STORE_IDX_REF: {` |
|   155520 |  3330 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3331 | `	ph7_value *pKey;` |
|        - |  3332 | `	sxu32 nIdx;` |
|   155520 |  3333 | `	if( pInstr->iP1 ){` |
|        - |  3334 | `		/* Key is next on stack */` |
|    55936 |  3335 | `		pKey = pTos;` |
|    55936 |  3336 | `		pTos--;` |
|    27969 |  3337 | `	}else{` |
|    99586 |  3338 | `		pKey = 0;` |
|        - |  3339 | `	}` |
|   155520 |  3340 | `	nIdx = pTos->nIdx;` |
|   155520 |  3341 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3342 | `		/* Hashmap already loaded */` |
|   155468 |  3343 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   155468 |  3344 | `		if( pMap->iRef < 2 ){` |
|        - |  3345 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3346 | `			pMap->iRef = 2;` |
|      ! 0 |  3347 | `		}` |
|    77735 |  3348 | `	}else{` |
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
|   155468 |  3402 | `	VmPopOperand(&pTos,1);` |
|        - |  3403 | `	/* Phase#2: Perform the insertion */` |
|   155468 |  3404 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3405 | `		/* Insertion by reference */` |
|       15 |  3406 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3407 | `	}else{` |
|   155454 |  3408 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3409 | `	}` |
|   155468 |  3410 | `	if( pKey ){` |
|    55886 |  3411 | `		PH7_MemObjRelease(pKey);` |
|    27942 |  3412 | `	}` |
|   155468 |  3413 | `	break;` |
|        - |  3414 | `					   }` |
|        - |  3415 | `/*` |
|        - |  3416 | ` * INCR: P1 * *` |
|        - |  3417 | ` *` |
|        - |  3418 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3419 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3420 | ` * the stack and increment after that.` |
|        - |  3421 | ` */` |
|   170565 |  3422 | `case PH7_OP_INCR:` |
|        - |  3423 | `#ifdef UNTRUST` |
|        - |  3424 | `	if( pTos < pStack ){` |
|        - |  3425 | `		goto Abort;` |
|        - |  3426 | `	}` |
|        - |  3427 | `#endif` |
|   341176 |  3428 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   341176 |  3429 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3430 | `			ph7_value *pObj;` |
|   341176 |  3431 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3432 | `				/* Force a numeric cast */` |
|   341176 |  3433 | `				PH7_MemObjToNumeric(pObj);` |
|   341176 |  3434 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3435 | `					pObj->rVal++;` |
|        - |  3436 | `					/* Try to get an integer representation */` |
|      ! 0 |  3437 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3438 | `				}else{` |
|   341176 |  3439 | `					pObj->x.iVal++;` |
|   341176 |  3440 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3441 | `				}` |
|   341176 |  3442 | `				if( pInstr->iP1 ){` |
|        - |  3443 | `					/* Pre-icrement */` |
|       71 |  3444 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3445 | `				}` |
|   170609 |  3446 | `			}` |
|   170611 |  3447 | `		}else{` |
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
|   170609 |  3462 | `	}` |
|   341176 |  3463 | `	break;` |
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
|    22264 |  3518 | `case PH7_OP_UMINUS:` |
|        - |  3519 | `#ifdef UNTRUST` |
|        - |  3520 | `	if( pTos < pStack ){` |
|        - |  3521 | `		goto Abort;` |
|        - |  3522 | `	}` |
|        - |  3523 | `#endif` |
|        - |  3524 | `	/* Force a numeric (integer,real or both) cast */` |
|    44530 |  3525 | `	PH7_MemObjToNumeric(pTos);` |
|    44530 |  3526 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       28 |  3527 | `		pTos->rVal = -pTos->rVal;` |
|       13 |  3528 | `	}` |
|    44530 |  3529 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    44504 |  3530 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    22251 |  3531 | `	}` |
|    44530 |  3532 | `	break;` |
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
|    51299 |  3559 | `case PH7_OP_LNOT:` |
|        - |  3560 | `#ifdef UNTRUST` |
|        - |  3561 | `	if( pTos < pStack ){` |
|        - |  3562 | `		goto Abort;` |
|        - |  3563 | `	}` |
|        - |  3564 | `#endif` |
|        - |  3565 | `	/* Force a boolean cast */` |
|   102644 |  3566 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3567 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3568 | `	}` |
|   102644 |  3569 | `	pTos->x.iVal = !pTos->x.iVal;` |
|   102644 |  3570 | `	break;` |
|        - |  3571 | `/*` |
|        - |  3572 | ` * OP_BITNOT: * * *` |
|        - |  3573 | ` *` |
|        - |  3574 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3575 | ` * with its ones-complement.` |
|        - |  3576 | ` */` |
|       14 |  3577 | `case PH7_OP_BITNOT:` |
|        - |  3578 | `#ifdef UNTRUST` |
|        - |  3579 | `	if( pTos < pStack ){` |
|        - |  3580 | `		goto Abort;` |
|        - |  3581 | `	}` |
|        - |  3582 | `#endif` |
|        - |  3583 | `	/* Force an integer cast */` |
|       30 |  3584 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3585 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3586 | `	}` |
|       30 |  3587 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  3588 | `	break;` |
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
|       30 |  3978 | `case PH7_OP_BAND:` |
|        - |  3979 | `case PH7_OP_BOR:` |
|        - |  3980 | `case PH7_OP_BXOR:{` |
|       62 |  3981 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3982 | `	sxi64 a,b,r;` |
|        - |  3983 | `#ifdef UNTRUST` |
|        - |  3984 | `	if( pNos < pStack ){` |
|        - |  3985 | `		goto Abort;` |
|        - |  3986 | `	}` |
|        - |  3987 | `#endif` |
|        - |  3988 | `	/* Force the operands to be integer */` |
|       62 |  3989 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3990 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3991 | `	}` |
|       62 |  3992 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3993 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3994 | `	}` |
|        - |  3995 | `	/* Perform the requested operation */` |
|       62 |  3996 | `	a = pNos->x.iVal;` |
|       62 |  3997 | `	b = pTos->x.iVal;` |
|       62 |  3998 | `	switch(pInstr->iOp){` |
|        6 |  3999 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4000 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4001 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4002 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       18 |  4003 | `	case PH7_OP_BAND_STORE:` |
|       18 |  4004 | `	case PH7_OP_BAND:` |
|       38 |  4005 | `	default:          r = a&b; break;` |
|        - |  4006 | `	}` |
|        - |  4007 | `	/* Push the result */` |
|       62 |  4008 | `	pNos->x.iVal = r;` |
|       62 |  4009 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       62 |  4010 | `	VmPopOperand(&pTos,1);` |
|       62 |  4011 | `	break;` |
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
|    59582 |  4173 | `case PH7_OP_CAT:{` |
|        - |  4174 | `	ph7_value *pNos,*pCur;` |
|   119166 |  4175 | `	if( pInstr->iP1 < 1 ){` |
|    92282 |  4176 | `		pNos = &pTos[-1];` |
|    46142 |  4177 | `	}else{` |
|    26886 |  4178 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4179 | `	}` |
|        - |  4180 | `#ifdef UNTRUST` |
|        - |  4181 | `	if( pNos < pStack ){` |
|        - |  4182 | `		goto Abort;` |
|        - |  4183 | `	}` |
|        - |  4184 | `#endif` |
|        - |  4185 | `	/* Force a string cast */` |
|   119166 |  4186 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      946 |  4187 | `		PH7_MemObjToString(pNos);` |
|      472 |  4188 | `	}` |
|   119166 |  4189 | `	pCur = &pNos[1];` |
|   240170 |  4190 | `	while( pCur <= pTos ){` |
|   121006 |  4191 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50450 |  4192 | `			PH7_MemObjToString(pCur);` |
|    25224 |  4193 | `		}` |
|        - |  4194 | `		/* Perform the concatenation */` |
|   121006 |  4195 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   120968 |  4196 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    60483 |  4197 | `		}` |
|   121006 |  4198 | `		SyBlobRelease(&pCur->sBlob);` |
|   121006 |  4199 | `		pCur++;` |
|        2 |  4200 | `	}` |
|   119166 |  4201 | `	pTos = pNos;` |
|   119166 |  4202 | `	break;` |
|        - |  4203 | `				}` |
|        - |  4204 | `/*  CAT_STORE: * * *` |
|        - |  4205 | ` *` |
|        - |  4206 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4207 | ` * back.` |
|        - |  4208 | ` */` |
|     2887 |  4209 | `case PH7_OP_CAT_STORE:{` |
|     5776 |  4210 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4211 | `	ph7_value *pObj;` |
|        - |  4212 | `#ifdef UNTRUST` |
|        - |  4213 | `	if( pNos < pStack ){` |
|        - |  4214 | `		goto Abort;` |
|        - |  4215 | `	}` |
|        - |  4216 | `#endif` |
|     5776 |  4217 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4218 | `		/* Force a string cast */` |
|      ! 0 |  4219 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4220 | `	}` |
|     5776 |  4221 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4222 | `		/* Force a string cast */` |
|      ! 0 |  4223 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4224 | `	}` |
|        - |  4225 | `	/* Perform the concatenation (Reverse order) */` |
|     5776 |  4226 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     5776 |  4227 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     2887 |  4228 | `	}` |
|        - |  4229 | `	/* Perform the store operation */` |
|     5776 |  4230 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4231 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     5776 |  4232 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     5776 |  4233 | `		PH7_MemObjStore(pTos,pObj);` |
|     2887 |  4234 | `	}` |
|     5776 |  4235 | `	PH7_MemObjStore(pTos,pNos);` |
|     5776 |  4236 | `	VmPopOperand(&pTos,1);` |
|     5776 |  4237 | `	break;` |
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
|   109312 |  4251 | `case PH7_OP_LAND:` |
|        - |  4252 | `case PH7_OP_LOR: {` |
|   218670 |  4253 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4254 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4255 | `#ifdef UNTRUST` |
|        - |  4256 | `	if( pNos < pStack ){` |
|        - |  4257 | `		goto Abort;` |
|        - |  4258 | `	}` |
|        - |  4259 | `#endif` |
|        - |  4260 | `	/* Force a boolean cast */` |
|   218670 |  4261 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4262 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4263 | `	}` |
|   218670 |  4264 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4265 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4266 | `	}` |
|   218670 |  4267 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   218670 |  4268 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   218670 |  4269 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4270 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|   109616 |  4271 | `		v1 = and_logic[v1*3+v2];` |
|    54831 |  4272 | `	}else{` |
|        - |  4273 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   109056 |  4274 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4275 | `	}` |
|   218670 |  4276 | `	if( v1 == 2 ){` |
|      ! 0 |  4277 | `		v1 = 1;` |
|      ! 0 |  4278 | `	}` |
|   218670 |  4279 | `	VmPopOperand(&pTos,1);` |
|   218670 |  4280 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   218670 |  4281 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   218670 |  4282 | `	break;` |
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
|     3725 |  4330 | `case PH7_OP_EQ:` |
|        - |  4331 | `case PH7_OP_NEQ: {` |
|     7452 |  4332 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4333 | `	/* Perform the comparison and act accordingly */` |
|        - |  4334 | `#ifdef UNTRUST` |
|        - |  4335 | `	if( pNos < pStack ){` |
|        - |  4336 | `		goto Abort;` |
|        - |  4337 | `	}` |
|        - |  4338 | `#endif` |
|     7452 |  4339 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7452 |  4340 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       18 |  4341 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7444 |  4342 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7410 |  4343 | `		rc = rc == 0;` |
|     3706 |  4344 | `	}else{` |
|       28 |  4345 | `		rc = rc != 0;` |
|        - |  4346 | `	}` |
|     7452 |  4347 | `	VmPopOperand(&pTos,1);` |
|     7452 |  4348 | `	if( !pInstr->iP2 ){` |
|        - |  4349 | `		/* Push comparison result without taking the jump */` |
|     7452 |  4350 | `		PH7_MemObjRelease(pTos);` |
|     7452 |  4351 | `		pTos->x.iVal = rc;` |
|        - |  4352 | `		/* Invalidate any prior representation */` |
|     7452 |  4353 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3727 |  4354 | `	}else{` |
|      ! 0 |  4355 | `		if( rc ){` |
|        - |  4356 | `			/* Jump to the desired location */` |
|      ! 0 |  4357 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4358 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4359 | `		}` |
|        - |  4360 | `	}` |
|     7452 |  4361 | `	break;` |
|        - |  4362 | `				 }` |
|        - |  4363 | `/* OP_TEQ P1 P2 *` |
|        - |  4364 | ` *` |
|        - |  4365 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4366 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4367 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4368 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4369 | ` */` |
|   128663 |  4370 | `case PH7_OP_TEQ: {` |
|   257328 |  4371 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4372 | `	/* Perform the comparison and act accordingly */` |
|        - |  4373 | `#ifdef UNTRUST` |
|        - |  4374 | `	if( pNos < pStack ){` |
|        - |  4375 | `		goto Abort;` |
|        - |  4376 | `	}` |
|        - |  4377 | `#endif` |
|   257328 |  4378 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   257328 |  4379 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4380 | `		rc = 0;` |
|        2 |  4381 | `	}else{` |
|   257326 |  4382 | `		rc = rc == 0;` |
|        - |  4383 | `	}` |
|   257328 |  4384 | `	VmPopOperand(&pTos,1);` |
|   257328 |  4385 | `	if( !pInstr->iP2 ){` |
|        - |  4386 | `		/* Push comparison result without taking the jump */` |
|   257328 |  4387 | `		PH7_MemObjRelease(pTos);` |
|   257328 |  4388 | `		pTos->x.iVal = rc;` |
|        - |  4389 | `		/* Invalidate any prior representation */` |
|   257328 |  4390 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   128665 |  4391 | `	}else{` |
|      ! 0 |  4392 | `		if( rc ){` |
|        - |  4393 | `			/* Jump to the desired location */` |
|      ! 0 |  4394 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4395 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4396 | `		}` |
|        - |  4397 | `	}` |
|   257328 |  4398 | `	break;` |
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
|   102119 |  4409 | `case PH7_OP_TNE: {` |
|   204240 |  4410 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4411 | `	/* Perform the comparison and act accordingly */` |
|        - |  4412 | `#ifdef UNTRUST` |
|        - |  4413 | `	if( pNos < pStack ){` |
|        - |  4414 | `		goto Abort;` |
|        - |  4415 | `	}` |
|        - |  4416 | `#endif` |
|   204240 |  4417 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   204240 |  4418 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4419 | `		rc = 1;` |
|        2 |  4420 | `	}else{` |
|   204238 |  4421 | `		rc = rc != 0;` |
|        - |  4422 | `	}` |
|   204240 |  4423 | `	VmPopOperand(&pTos,1);` |
|   204240 |  4424 | `	if( !pInstr->iP2 ){` |
|        - |  4425 | `		/* Push comparison result without taking the jump */` |
|   204240 |  4426 | `		PH7_MemObjRelease(pTos);` |
|   204240 |  4427 | `		pTos->x.iVal = rc;` |
|        - |  4428 | `		/* Invalidate any prior representation */` |
|   204240 |  4429 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   102121 |  4430 | `	}else{` |
|      ! 0 |  4431 | `		if( rc ){` |
|        - |  4432 | `			/* Jump to the desired location */` |
|      ! 0 |  4433 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4434 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4435 | `		}` |
|        - |  4436 | `	}` |
|   204240 |  4437 | `	break;` |
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
|   118421 |  4457 | `case PH7_OP_LT:` |
|        - |  4458 | `case PH7_OP_LE: {` |
|   236888 |  4459 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4460 | `	/* Perform the comparison and act accordingly */` |
|        - |  4461 | `#ifdef UNTRUST` |
|        - |  4462 | `	if( pNos < pStack ){` |
|        - |  4463 | `		goto Abort;` |
|        - |  4464 | `	}` |
|        - |  4465 | `#endif` |
|   236888 |  4466 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   236888 |  4467 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4468 | `		rc = 0;` |
|   236884 |  4469 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      395 |  4470 | `		rc = rc < 1;` |
|      198 |  4471 | `	}else{` |
|   236486 |  4472 | `		rc = rc < 0;` |
|        - |  4473 | `	}` |
|   236888 |  4474 | `	VmPopOperand(&pTos,1);` |
|   236888 |  4475 | `	if( !pInstr->iP2 ){` |
|        - |  4476 | `		/* Push comparison result without taking the jump */` |
|   236888 |  4477 | `		PH7_MemObjRelease(pTos);` |
|   236888 |  4478 | `		pTos->x.iVal = rc;` |
|        - |  4479 | `		/* Invalidate any prior representation */` |
|   236888 |  4480 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   118467 |  4481 | `	}else{` |
|      ! 0 |  4482 | `		if( rc ){` |
|        - |  4483 | `			/* Jump to the desired location */` |
|      ! 0 |  4484 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4485 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4486 | `		}` |
|        - |  4487 | `	}` |
|   236888 |  4488 | `	break;` |
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
|    52204 |  4508 | `case PH7_OP_GT:` |
|        - |  4509 | `case PH7_OP_GE: {` |
|   104410 |  4510 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4511 | `	/* Perform the comparison and act accordingly */` |
|        - |  4512 | `#ifdef UNTRUST` |
|        - |  4513 | `	if( pNos < pStack ){` |
|        - |  4514 | `		goto Abort;` |
|        - |  4515 | `	}` |
|        - |  4516 | `#endif` |
|   104410 |  4517 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   104410 |  4518 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4519 | `		rc = 0;` |
|   104406 |  4520 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   104254 |  4521 | `		rc = rc >= 0;` |
|    52128 |  4522 | `	}else{` |
|      150 |  4523 | `		rc = rc > 0;` |
|        - |  4524 | `	}` |
|   104410 |  4525 | `	VmPopOperand(&pTos,1);` |
|   104410 |  4526 | `	if( !pInstr->iP2 ){` |
|        - |  4527 | `		/* Push comparison result without taking the jump */` |
|   104410 |  4528 | `		PH7_MemObjRelease(pTos);` |
|   104410 |  4529 | `		pTos->x.iVal = rc;` |
|        - |  4530 | `		/* Invalidate any prior representation */` |
|   104410 |  4531 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    52206 |  4532 | `	}else{` |
|      ! 0 |  4533 | `		if( rc ){` |
|        - |  4534 | `			/* Jump to the desired location */` |
|      ! 0 |  4535 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4536 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4537 | `		}` |
|        - |  4538 | `	}` |
|   104410 |  4539 | `	break;` |
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
|       90 |  4681 | `			while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4682 | `				/* Safely ignore the exception frame */` |
|       61 |  4683 | `				pFrameLocal = pFrameLocal->pParent;` |
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
|       12 |  4733 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       26 |  4734 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4735 | `	VmFrame *pFrameLocal;` |
|       26 |  4736 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4737 | `	/* Create the exception frame */` |
|       26 |  4738 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       26 |  4739 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4740 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4741 | `		goto Abort;` |
|        - |  4742 | `	}` |
|        - |  4743 | `	/* Mark the special frame */` |
|       26 |  4744 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       26 |  4745 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4746 | `	/* Point to the frame that trigger the exception */` |
|       26 |  4747 | `	pFrameLocal = pFrameLocal->pParent;` |
|       76 |  4748 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|       51 |  4749 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4750 | `	}` |
|       26 |  4751 | `	pException->pFrame = pFrameLocal;` |
|       26 |  4752 | `	break;` |
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
|       34 |  4786 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4787 | `		/* Safely ignore the exception frame */` |
|       12 |  4788 | `		pFrameLocal = pFrameLocal->pParent;` |
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
|     4622 |  4831 | `case PH7_OP_FOREACH_INIT: {` |
|     9246 |  4832 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4833 | `	void *pName;` |
|        - |  4834 | `#ifdef UNTRUST` |
|        - |  4835 | `	if( pTos < pStack ){` |
|        - |  4836 | `		goto Abort;` |
|        - |  4837 | `	}` |
|        - |  4838 | `#endif` |
|     9246 |  4839 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
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
|     9246 |  4852 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
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
|     9246 |  4865 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4866 | `		/* Jump out of the loop */` |
|      ! 0 |  4867 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4868 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4869 | `		}` |
|      ! 0 |  4870 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4871 | `	}else{` |
|        - |  4872 | `		ph7_foreach_step *pStep;` |
|     9246 |  4873 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     9246 |  4874 | `		if( pStep == 0 ){` |
|      ! 0 |  4875 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4876 | `			/* Jump out of the loop */` |
|      ! 0 |  4877 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4878 | `		}else{` |
|        - |  4879 | `			/* Zero the structure */` |
|     9246 |  4880 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4881 | `			/* Prepare the step */` |
|     9246 |  4882 | `			pStep->iFlags = pInfo->iFlags;` |
|     9246 |  4883 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     9238 |  4884 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4885 | `				/* Reset the internal loop cursor */` |
|     9238 |  4886 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4887 | `				/* Mark the step */` |
|     9238 |  4888 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     9238 |  4889 | `				pStep->xIter.pMap = pMap;` |
|     9238 |  4890 | `				pMap->iRef++;` |
|     4620 |  4891 | `			}else{` |
|        9 |  4892 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4893 | `				/* Reset the loop cursor */` |
|        9 |  4894 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|        - |  4895 | `				/* Mark the step */` |
|        9 |  4896 | `				pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  4897 | `				pStep->xIter.pThis = pThis;` |
|        9 |  4898 | `				pThis->iRef++;` |
|        - |  4899 | `			}` |
|        - |  4900 | `		}` |
|     9246 |  4901 | `		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  4902 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  4903 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  4904 | `			/* Jump out of the loop */` |
|      ! 0 |  4905 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4906 | `		}` |
|        - |  4907 | `	}` |
|     9246 |  4908 | `	VmPopOperand(&pTos,1);` |
|     9246 |  4909 | `	break;` |
|        - |  4910 | `						  }` |
|        - |  4911 | `/*` |
|        - |  4912 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  4913 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  4914 | ` */` |
|    73965 |  4915 | `case PH7_OP_FOREACH_STEP: {` |
|   147932 |  4916 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4917 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  4918 | `	ph7_value *pValue;` |
|        - |  4919 | `	VmFrame *pFrameLocal;` |
|        - |  4920 | `	/* Peek the last step */` |
|   147932 |  4921 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   147932 |  4922 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   147932 |  4923 | `	pFrameLocal = pVm->pFrame;` |
|   196542 |  4924 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4925 | `		/* Safely ignore the exception frame */` |
|    48611 |  4926 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4927 | `	}` |
|   147932 |  4928 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   147908 |  4929 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  4930 | `		ph7_hashmap_node *pNode;` |
|        - |  4931 | `		/* Extract the current node value */` |
|   147908 |  4932 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   147908 |  4933 | `		if( pNode == 0 ){` |
|        - |  4934 | `			/* No more entry to process */` |
|     9238 |  4935 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     9238 |  4936 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4937 | `				/* Break the reference with the last element */` |
|        5 |  4938 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  4939 | `			}` |
|        - |  4940 | `			/* Automatically reset the loop cursor */` |
|     9238 |  4941 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4942 | `			/* Cleanup the mess left behind */` |
|     9238 |  4943 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     9238 |  4944 | `			SySetPop(&pInfo->aStep);` |
|     9238 |  4945 | `			PH7_HashmapUnref(pMap);` |
|     4620 |  4946 | `		}else{` |
|   138672 |  4947 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      408 |  4948 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      408 |  4949 | `				if( pKey ){` |
|      408 |  4950 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      203 |  4951 | `				}` |
|      203 |  4952 | `			}` |
|   138672 |  4953 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
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
|   138660 |  4965 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   138660 |  4966 | `				if( pValue ){` |
|   138660 |  4967 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    69329 |  4968 | `				}` |
|        - |  4969 | `			}` |
|        - |  4970 | `		}` |
|    73955 |  4971 | `	}else{` |
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
|   147932 |  5028 | `	break;` |
|        - |  5029 | `						  }` |
|        - |  5030 | `/*` |
|        - |  5031 | ` * OP_MEMBER P1 P2` |
|        - |  5032 | ` * Load class attribute/method on the stack.` |
|        - |  5033 | ` */` |
|     1796 |  5034 | `case PH7_OP_MEMBER: {` |
|        - |  5035 | `	ph7_class_instance *pThis;` |
|        - |  5036 | `	ph7_value *pNos;` |
|        - |  5037 | `	SyString sName;` |
|     3594 |  5038 | `	if( !pInstr->iP1 ){` |
|     3536 |  5039 | `		pNos = &pTos[-1];` |
|        - |  5040 | `#ifdef UNTRUST` |
|        - |  5041 | `		if( pNos < pStack ){` |
|        - |  5042 | `			goto Abort;` |
|        - |  5043 | `		}` |
|        - |  5044 | `#endif` |
|     3536 |  5045 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5046 | `			ph7_class *pClass;` |
|        - |  5047 | `			/* Class already instantiated */` |
|     3536 |  5048 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5049 | `			/* Point to the instantiated class */` |
|     3536 |  5050 | `			pClass = pThis->pClass;` |
|        - |  5051 | `			/* Extract attribute name first */` |
|     3536 |  5052 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     3536 |  5053 | `			if( pInstr->iP2 ){` |
|        - |  5054 | `				/* Method call */` |
|      124 |  5055 | `				ph7_class_method *pMeth = 0;` |
|      124 |  5056 | `				if( sName.nByte > 0 ){` |
|        - |  5057 | `					/* Extract the target method */` |
|      124 |  5058 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       61 |  5059 | `				}` |
|      124 |  5060 | `				if( pMeth == 0 ){` |
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
|      124 |  5071 | `					PH7_MemObjRelease(pTos);` |
|      124 |  5072 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      124 |  5073 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5074 | `				}` |
|      124 |  5075 | `				pTos->nIdx = SXU32_HIGH;` |
|       63 |  5076 | `			}else{` |
|        - |  5077 | `				/* Attribute access */` |
|     3414 |  5078 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5079 | `				SyHashEntry *pEntry;` |
|        - |  5080 | `				/* Extract the target attribute */` |
|     3414 |  5081 | `				if( sName.nByte > 0 ){` |
|     3414 |  5082 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3414 |  5083 | `					if( pEntry ){` |
|        - |  5084 | `						/* Point to the attribute value */` |
|     3412 |  5085 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1705 |  5086 | `					}` |
|     1706 |  5087 | `				}` |
|     3414 |  5088 | `				if( pObjAttr == 0 ){` |
|        - |  5089 | `					/* No such attribute,load null */` |
|        4 |  5090 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5091 | `						&pClass->sName,&sName);` |
|        - |  5092 | `					/* Call the __get magic method if available */` |
|        3 |  5093 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5094 | `				}` |
|     3414 |  5095 | `				VmPopOperand(&pTos,1);` |
|        - |  5096 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5097 | `				 * This is due to the following case:` |
|        - |  5098 | `				 *     (new TestClass())->foo;` |
|        - |  5099 | `				 */` |
|     3414 |  5100 | `				pThis->iRef++;` |
|     3414 |  5101 | `				PH7_MemObjRelease(pTos);` |
|     3414 |  5102 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3414 |  5103 | `				if( pObjAttr ){` |
|     3412 |  5104 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5105 | `					/* Check attribute access */` |
|     3412 |  5106 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5107 | `						/* Load attribute */` |
|     3412 |  5108 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3412 |  5109 | `						if( pValue ){` |
|     3412 |  5110 | `							if( pThis->iRef < 2 ){` |
|        - |  5111 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5112 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5113 | `								 */` |
|        3 |  5114 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5115 | `							}else{` |
|        - |  5116 | `								/* Simple load */` |
|     3410 |  5117 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5118 | `							}` |
|     3412 |  5119 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3410 |  5120 | `								if( pThis->iRef > 1 ){` |
|        - |  5121 | `									/* Load attribute index */` |
|     3408 |  5122 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1703 |  5123 | `								}` |
|     1704 |  5124 | `							}` |
|     1705 |  5125 | `						}` |
|     1705 |  5126 | `					}` |
|     1705 |  5127 | `				}` |
|        - |  5128 | `				/* Safely unreference the object */` |
|     3414 |  5129 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5130 | `			}` |
|     1769 |  5131 | `		}else{` |
|      ! 0 |  5132 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5133 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5134 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5135 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5136 | `		}` |
|     1769 |  5137 | `	}else{` |
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
|     3594 |  5281 | `	break;` |
|        - |  5282 | `					}` |
|        - |  5283 | `/*` |
|        - |  5284 | ` * OP_NEW P1 * * *` |
|        - |  5285 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5286 | ` */` |
|      256 |  5287 | `case PH7_OP_NEW: {` |
|      514 |  5288 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      514 |  5289 | `	ph7_class *pClass = 0;` |
|        - |  5290 | `	ph7_class_instance *pNew;` |
|      514 |  5291 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5292 | `		/* Try to extract the desired class */` |
|      770 |  5293 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      512 |  5294 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      256 |  5295 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5296 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5297 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5298 | `	}` |
|      514 |  5299 | `	if( pClass == 0 ){` |
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
|      514 |  5312 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      514 |  5313 | `		if( pNew == 0 ){` |
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
|      514 |  5326 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      514 |  5327 | `		if( pCons == 0 ){` |
|      456 |  5328 | `			SyString *pName = &pClass->sName;` |
|        - |  5329 | `			/* Check for a constructor with the same base class name */` |
|      456 |  5330 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      227 |  5331 | `		}` |
|      514 |  5332 | `		if( pCons ){` |
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
|      514 |  5361 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5362 | `			/* Pop given arguments */` |
|       44 |  5363 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       21 |  5364 | `		}` |
|      514 |  5365 | `		PH7_MemObjRelease(pTos);` |
|      514 |  5366 | `		pTos->x.pOther = pNew;` |
|      514 |  5367 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5368 | `	}` |
|      514 |  5369 | `	break;` |
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
|   283808 |  5456 | `case PH7_OP_CALL: {` |
|   567662 |  5457 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5458 | `	SyHashEntry *pEntry;` |
|        - |  5459 | `	SyString sName;` |
|        - |  5460 | `	/* Extract function name */` |
|   567662 |  5461 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
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
|   283579 |  5496 | `		break;` |
|        - |  5497 | `	}` |
|   567660 |  5498 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5499 | `	/* Check for a compiled function first */` |
|   567660 |  5500 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|   567660 |  5501 | `	if( pEntry ){` |
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
|    11556 |  5512 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    11556 |  5513 | `		pThis = 0;` |
|    11556 |  5514 | `		pSelf = 0;` |
|    11556 |  5515 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5516 | `			ph7_class_method *pMeth;` |
|        - |  5517 | `			/* Class method call */` |
|     1266 |  5518 | `			ph7_value *pTarget = &pTos[-1];` |
|     1266 |  5519 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5520 | `				/* Extract the 'this' pointer */` |
|     1266 |  5521 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5522 | `					/* Instance already loaded */` |
|     1236 |  5523 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1236 |  5524 | `					pThis->iRef++;` |
|     1236 |  5525 | `					pSelf = pThis->pClass;` |
|      617 |  5526 | `				}` |
|     1266 |  5527 | `				if( pSelf == 0 ){` |
|       31 |  5528 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5529 | `						/* "Late Static Binding" class name */` |
|       37 |  5530 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       12 |  5531 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       12 |  5532 | `					}` |
|       31 |  5533 | `					if( pSelf == 0 ){` |
|        7 |  5534 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 |  5535 | `					}` |
|       15 |  5536 | `				}` |
|     1266 |  5537 | `				if( pThis == 0  ){` |
|       31 |  5538 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       57 |  5539 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5540 | `						/* Safely ignore the exception frame */` |
|       27 |  5541 | `						pFrameLocal = pFrameLocal->pParent;` |
|        1 |  5542 | `					}` |
|       31 |  5543 | `					if( pFrameLocal->pParent ){` |
|        - |  5544 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5545 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5546 | `						if( pThis ){` |
|       13 |  5547 | `							pThis->iRef++;` |
|        6 |  5548 | `						}` |
|        9 |  5549 | `					}` |
|       15 |  5550 | `				}` |
|     1266 |  5551 | `				VmPopOperand(&pTos,1);` |
|     1266 |  5552 | `				PH7_MemObjRelease(pTos);` |
|        - |  5553 | `				/* Synchronize pointers */` |
|     1266 |  5554 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5555 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5556 | `				 * user have already computed the random generated unique class method name` |
|        - |  5557 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5558 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5559 | `				 */` |
|     1266 |  5560 | `				while( pArg < pStack ){` |
|      ! 0 |  5561 | `					pArg++;` |
|      ! 0 |  5562 | `				}` |
|     1266 |  5563 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5564 | `					/* Check if the call is allowed */` |
|     1266 |  5565 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1266 |  5566 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
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
|      632 |  5577 | `				}` |
|      632 |  5578 | `			}` |
|      632 |  5579 | `		}` |
|        - |  5580 | `		/* Check The recursion limit */` |
|    11556 |  5581 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
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
|    11554 |  5593 | `		if( pVmFunc->pNextName ){` |
|        - |  5594 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      123 |  5595 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       61 |  5596 | `		}` |
|        - |  5597 | `		/* Extract the formal argument set */` |
|    11554 |  5598 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5599 | `		/* Create a new VM frame  */` |
|    11554 |  5600 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    11554 |  5601 | `		if( rc != SXRET_OK ){` |
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
|    11554 |  5614 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5615 | `			/* Install the '$this' variable */` |
|        - |  5616 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1246 |  5617 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1246 |  5618 | `			if( pObj ){` |
|        - |  5619 | `				/* Reflect the change */` |
|     1246 |  5620 | `				pObj->x.pOther = pThis;` |
|     1246 |  5621 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      622 |  5622 | `			}` |
|      622 |  5623 | `		}` |
|    11554 |  5624 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
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
|    11554 |  5651 | `		n = 0;` |
|    32354 |  5652 | `		while( pArg < pTos ){` |
|    20802 |  5653 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    20652 |  5654 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5655 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5656 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5657 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5658 | `						goto Abort;` |
|        - |  5659 | `					}` |
|      ! 0 |  5660 | `				}` |
|        - |  5661 | `				/* Make sure the given arguments are of the correct type */` |
|    20652 |  5662 | `				if( aFormalArg[n].nType > 0 ){` |
|     1086 |  5663 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
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
|     1086 |  5689 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5690 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5691 | `						/* Cast to the desired type */` |
|      ! 0 |  5692 | `						xCast(pArg);` |
|      ! 0 |  5693 | `					}` |
|      542 |  5694 | `				}` |
|    20652 |  5695 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
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
|    20606 |  5721 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5722 | `				}` |
|    10327 |  5723 | `			}else{` |
|        - |  5724 | `				char zName[32];` |
|        - |  5725 | `				SyString sArgName;` |
|        - |  5726 | `				/* Set a dummy name */` |
|      152 |  5727 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      152 |  5728 | `				sArgName.zString = zName;` |
|        - |  5729 | `				/* Annonymous argument */` |
|      152 |  5730 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5731 | `			}` |
|    20802 |  5732 | `			if( pObj ){` |
|    20756 |  5733 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5734 | `				/* Insert argument index  */` |
|    20756 |  5735 | `				sArg.nIdx = pObj->nIdx;` |
|    20756 |  5736 | `				sArg.pUserData = 0;` |
|    20756 |  5737 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    10377 |  5738 | `			}` |
|    20802 |  5739 | `			PH7_MemObjRelease(pArg);` |
|    20802 |  5740 | `			pArg++;` |
|    20802 |  5741 | `			++n;` |
|        2 |  5742 | `		}` |
|        - |  5743 | `		/* Set up closure environment */` |
|    11554 |  5744 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
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
|    13362 |  5766 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1810 |  5767 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1804 |  5768 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1804 |  5769 | `				if( pObj ){` |
|        - |  5770 | `					/* Evaluate the default value and extract it's result */` |
|     1804 |  5771 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1804 |  5772 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5773 | `						goto Abort;` |
|        - |  5774 | `					}` |
|        - |  5775 | `					/* Insert argument index */` |
|     1804 |  5776 | `					sArg.nIdx = pObj->nIdx;` |
|     1804 |  5777 | `					sArg.pUserData = 0;` |
|     1804 |  5778 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  5779 | `					/* Make sure the default argument is of the correct type */` |
|     1804 |  5780 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5781 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5782 | `						/* Cast to the desired type */` |
|      ! 0 |  5783 | `						xCast(pObj);` |
|      ! 0 |  5784 | `					}` |
|      901 |  5785 | `				}` |
|      901 |  5786 | `			}` |
|     1810 |  5787 | `			++n;` |
|        2 |  5788 | `		}` |
|        - |  5789 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  5790 | `		 * does not return anything.` |
|        - |  5791 | `		 */` |
|    11554 |  5792 | `		PH7_MemObjRelease(pTos);` |
|    11554 |  5793 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  5794 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    11554 |  5795 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    11554 |  5796 | `		if( pFrameStack == 0 ){` |
|        - |  5797 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5798 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5799 | `				&pVmFunc->sName);` |
|      ! 0 |  5800 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5801 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5802 | `			}` |
|      ! 0 |  5803 | `			break;` |
|        - |  5804 | `		}` |
|    11554 |  5805 | `		if( pSelf ){` |
|        - |  5806 | `			/* Push class name */` |
|     1264 |  5807 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      631 |  5808 | `		}` |
|        - |  5809 | `		/* Increment nesting level */` |
|    11554 |  5810 | `		pVm->nRecursionDepth++;` |
|        - |  5811 | `		/* Execute function body */` |
|    11554 |  5812 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  5813 | `		/* Decrement nesting level */` |
|    11554 |  5814 | `		pVm->nRecursionDepth--;` |
|    11554 |  5815 | `		if( pSelf ){` |
|        - |  5816 | `			/* Pop class name */` |
|     1264 |  5817 | `			(void)SySetPop(&pVm->aSelf);` |
|      631 |  5818 | `		}` |
|        - |  5819 | `		/* Cleanup the mess left behind */` |
|    11554 |  5820 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
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
|    11554 |  5848 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
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
|    11554 |  5867 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  5868 | `		/* Leave the frame */` |
|    11554 |  5869 | `		VmLeaveFrame(&(*pVm));` |
|    11554 |  5870 | `		if( rc == PH7_ABORT ){` |
|        - |  5871 | `			/* Abort processing immeditaley */` |
|        7 |  5872 | `			goto Abort;` |
|    11548 |  5873 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5874 | `			goto Exception;` |
|        - |  5875 | `		}` |
|     5774 |  5876 | `	}else{` |
|        - |  5877 | `		ph7_user_func *pFunc;` |
|        - |  5878 | `		ph7_context sCtx;` |
|        - |  5879 | `		ph7_value sRet;` |
|        - |  5880 | `		/* Look for an installed foreign function */` |
|   556106 |  5881 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   556106 |  5882 | `		if( pEntry == 0 ){` |
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
|   556102 |  5893 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  5894 | `		/* Start collecting function arguments */` |
|   556102 |  5895 | `		SySetReset(&aArg);` |
|  1475286 |  5896 | `		while( pArg < pTos ){` |
|   919186 |  5897 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   919186 |  5898 | `			pArg++;` |
|        2 |  5899 | `		}` |
|        - |  5900 | `		/* Assume a null return value */` |
|   556102 |  5901 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  5902 | `		/* Init the call context */` |
|   556102 |  5903 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  5904 | `		/* Call the foreign function */` |
|   556102 |  5905 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5906 | `		/* Release the call context */` |
|   556102 |  5907 | `		VmReleaseCallContext(&sCtx);` |
|   556102 |  5908 | `		if( rc == PH7_ABORT ){` |
|      449 |  5909 | `			goto Abort;` |
|   555654 |  5910 | `		}else if( rc == PH7_EXCEPTION ){` |
|        7 |  5911 | `			goto Exception;` |
|        - |  5912 | `		}` |
|   555648 |  5913 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5914 | `			/* Pop function name and arguments */` |
|   538374 |  5915 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   269208 |  5916 | `		}` |
|        - |  5917 | `		/* Save foreign function return value */` |
|   555648 |  5918 | `		PH7_MemObjStore(&sRet,pTos);` |
|   555648 |  5919 | `		PH7_MemObjRelease(&sRet);` |
|        - |  5920 | `	}` |
|   567192 |  5921 | `	break;` |
|        - |  5922 | `				  }` |
|        - |  5923 | `/*` |
|        - |  5924 | ` * OP_CONSUME: P1 * *` |
|        - |  5925 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  5926 | ` */` |
|    10231 |  5927 | `case PH7_OP_CONSUME: {` |
|    20464 |  5928 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    20464 |  5929 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  5930 |  |
|    20464 |  5931 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    20464 |  5932 | `	pCur = pOut;` |
|        - |  5933 | `	/* Start the consume process  */` |
|    40926 |  5934 | `	while( pOut <= pTos ){` |
|        - |  5935 | `		/* Force a string cast */` |
|    20464 |  5936 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      172 |  5937 | `			PH7_MemObjToString(pOut);` |
|       85 |  5938 | `		}` |
|    20464 |  5939 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  5940 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  5941 | `			/* Invoke the output consumer callback */` |
|    10932 |  5942 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    10932 |  5943 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  5944 | `				/* Increment output length */` |
|     4520 |  5945 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     2259 |  5946 | `			}` |
|    10932 |  5947 | `			SyBlobRelease(&pOut->sBlob);` |
|    10932 |  5948 | `			if( rc == SXERR_ABORT ){` |
|        - |  5949 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  5950 | `				goto Abort;` |
|        - |  5951 | `			}` |
|     5465 |  5952 | `		}` |
|    20464 |  5953 | `		pOut++;` |
|        2 |  5954 | `	}` |
|    20464 |  5955 | `	pTos = &pCur[-1];` |
|    20462 |  5956 | `	break;` |
|        - |  5957 | `					 }` |
|        - |  5958 |  |
|        - |  5959 | `		} /* Switch() */` |
| 10064832 |  5960 | `		pc++; /* Next instruction in the stream */` |
|        2 |  5961 | `	} /* For(;;) */` |
|    14358 |  5962 | `Done:` |
|    28718 |  5963 | `	SySetRelease(&aArg);` |
|    28718 |  5964 | `	return SXRET_OK;` |
|      231 |  5965 | `Abort:` |
|      463 |  5966 | `	SySetRelease(&aArg);` |
|     1609 |  5967 | `	while( pTos >= pStack ){` |
|     1147 |  5968 | `		PH7_MemObjRelease(pTos);` |
|     1147 |  5969 | `		pTos--;` |
|        1 |  5970 | `	}` |
|      463 |  5971 | `	return PH7_ABORT;` |
|        4 |  5972 | `Exception:` |
|        9 |  5973 | `	SySetRelease(&aArg);` |
|       19 |  5974 | `	while( pTos >= pStack ){` |
|       11 |  5975 | `		PH7_MemObjRelease(pTos);` |
|       11 |  5976 | `		pTos--;` |
|        1 |  5977 | `	}` |
|        9 |  5978 | `	return PH7_EXCEPTION;` |
|    14595 |  5979 |  |
|        - |  5980 | `/*` |
|        - |  5981 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  5982 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  5983 | ` * See block-comment on that function for additional information.` |
|        - |  5984 | ` */` |
|    14088 |  5985 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  5986 |  |
|        - |  5987 | `	ph7_value *pStack;` |
|        - |  5988 | `	sxi32 rc;` |
|        - |  5989 | `	/* Allocate a new operand stack */` |
|    14090 |  5990 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    14090 |  5991 | `	if( pStack == 0 ){` |
|      ! 0 |  5992 | `		return SXERR_MEM;` |
|        - |  5993 | `	}` |
|        - |  5994 | `	/* Execute the program */` |
|    14090 |  5995 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  5996 | `	/* Free the operand stack */` |
|    14090 |  5997 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  5998 | `	/* Execution result */` |
|    14090 |  5999 | `	return rc;` |
|     7046 |  6000 |  |
|        - |  6001 | `/*` |
|        - |  6002 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6003 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6004 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6005 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6006 | ` * execution ends.` |
|        - |  6007 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6008 | ` * additional information.` |
|        - |  6009 | ` */` |
|     1982 |  6010 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6011 |  |
|        - |  6012 | `	VmShutdownCB *pEntry;` |
|        - |  6013 | `	ph7_value *apArg[10];` |
|        - |  6014 | `	sxu32 n,nEntry;` |
|        - |  6015 | `	int i;` |
|        - |  6016 | `	/* Point to the stack of registered callbacks */` |
|     1984 |  6017 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    21804 |  6018 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    19822 |  6019 | `		apArg[i] = 0;` |
|     9912 |  6020 | `	}` |
|     1986 |  6021 | `	for( n = 0 ; n < nEntry ; ++n ){` |
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
|     1984 |  6046 | `	SySetReset(&pVm->aShutdown);` |
|     1984 |  6047 |  |
|        - |  6048 | `/*` |
|        - |  6049 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6050 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6051 | ` * See block-comment on that function for additional information.` |
|        - |  6052 | ` */` |
|     1990 |  6053 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6054 |  |
|        - |  6055 | `	/* Make sure we are ready to execute this program */` |
|     1992 |  6056 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6057 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6058 | `	}` |
|        - |  6059 | `	/* Set the execution magic number  */` |
|     1992 |  6060 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6061 | `	/* Execute the program */` |
|     1992 |  6062 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6063 | `	/* Invoke any shutdown callbacks */` |
|     1988 |  6064 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6065 | `	/*` |
|        - |  6066 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6067 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6068 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6069 | `	 */` |
|     1988 |  6070 | `	return SXRET_OK;` |
|      997 |  6071 |  |
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
|      904 |  6271 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6272 |  |
|        - |  6273 | `	VmFrame *pFrame;` |
|        - |  6274 | `	ph7_vm *pVm;` |
|        - |  6275 | `	/* Point to the target VM */` |
|      906 |  6276 | `	pVm = pCtx->pVm;` |
|        - |  6277 | `	/* Current frame */` |
|      906 |  6278 | `	pFrame = pVm->pFrame;` |
|      906 |  6279 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6280 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6281 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6282 | `	}` |
|      906 |  6283 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6284 | `		SXUNUSED(nArg);` |
|      ! 0 |  6285 | `		SXUNUSED(apArg);` |
|        - |  6286 | `		/* Global frame,return -1 */` |
|      ! 0 |  6287 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6288 | `		return SXRET_OK;` |
|        - |  6289 | `	}` |
|        - |  6290 | `	/* Total number of arguments passed to the enclosing function */` |
|      906 |  6291 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      906 |  6292 | `	ph7_result_int(pCtx,nArg);` |
|      906 |  6293 | `	return SXRET_OK;` |
|      454 |  6294 |  |
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
|     1654 |  6449 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6450 |  |
|        - |  6451 | `	const char *zName;` |
|        - |  6452 | `	ph7_vm *pVm;` |
|        - |  6453 | `	int nLen;` |
|        - |  6454 | `	int res;` |
|     1656 |  6455 | `	if( nArg < 1 ){` |
|        - |  6456 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6457 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6458 | `		return SXRET_OK;` |
|        - |  6459 | `	}` |
|        - |  6460 | `	/* Point to the target VM */` |
|     1656 |  6461 | `	pVm = pCtx->pVm;` |
|        - |  6462 | `	/* Extract the function name */` |
|     1656 |  6463 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6464 | `	/* Assume the function is not defined */` |
|     1656 |  6465 | `	res = 0;` |
|        - |  6466 | `	/* Perform the lookup */` |
|     2481 |  6467 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1650 |  6468 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6469 | `			/* Function is defined */` |
|      202 |  6470 | `			res = 1;` |
|      100 |  6471 | `	}` |
|     1656 |  6472 | `	ph7_result_bool(pCtx,res);` |
|     1656 |  6473 | `	return SXRET_OK;` |
|      829 |  6474 |  |
|        - |  6475 | `/*` |
|        - |  6476 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6477 | ` * [i.e: Whether it is callable or not].` |
|        - |  6478 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6479 | ` */` |
|    15976 |  6480 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6481 |  |
|    15978 |  6482 | `	int res = 0;` |
|    15978 |  6483 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
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
|    15978 |  6499 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
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
|    15965 |  6523 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6524 | `		const char *zName;` |
|        - |  6525 | `		int nLen;` |
|        - |  6526 | `		/* Extract the name */` |
|     4704 |  6527 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6528 | `		/* Perform the lookup */` |
|     4719 |  6529 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  6530 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6531 | `				/* Function is callable */` |
|     4686 |  6532 | `				res = 1;` |
|     2342 |  6533 | `		}` |
|     2351 |  6534 | `	}` |
|    15978 |  6535 | `	return res;` |
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
|      502 |  6692 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6693 |  |
|      504 |  6694 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6695 | `	ph7_class **apClass;` |
|      504 |  6696 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6697 | `		/* Empty stack,return NULL */` |
|       15 |  6698 | `		return 0;` |
|        - |  6699 | `	}` |
|        - |  6700 | `	/* Peek the last entry */` |
|      490 |  6701 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      490 |  6702 | `	return apClass[pSet->nUsed - 1];` |
|      253 |  6703 |  |
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
|     1118 |  6752 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
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
|     1120 |  6766 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1120 |  6767 | `	if( aStack == 0 ){` |
|      ! 0 |  6768 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6769 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  6770 | `		return SXERR_MEM;` |
|        - |  6771 | `	}` |
|        - |  6772 | `	/* Fill the operand stack with the given arguments */` |
|     1652 |  6773 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      534 |  6774 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  6775 | `		/*` |
|        - |  6776 | `		 * Symisc eXtension:` |
|        - |  6777 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  6778 | `		 */` |
|      534 |  6779 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      268 |  6780 | `	}` |
|     1120 |  6781 | `	iCursor = nArg + 1;` |
|     1120 |  6782 | `	if( pThis ){` |
|        - |  6783 | `		/*` |
|        - |  6784 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  6785 | `		 */` |
|     1114 |  6786 | `		pThis->iRef++; /* Increment reference count */` |
|     1114 |  6787 | `		aStack[i].x.pOther = pThis;` |
|     1114 |  6788 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      556 |  6789 | `	}` |
|     1120 |  6790 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1120 |  6791 | `	i++;` |
|        - |  6792 | `	/* Push method name */` |
|     1120 |  6793 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1120 |  6794 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1120 |  6795 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1120 |  6796 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  6797 | `	/* Emit the CALL istruction */` |
|     1120 |  6798 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1120 |  6799 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1120 |  6800 | `	aInstr[0].iP2 = 0;` |
|     1120 |  6801 | `	aInstr[0].p3  = 0;` |
|        - |  6802 | `	/* Emit the DONE instruction */` |
|     1120 |  6803 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1120 |  6804 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1120 |  6805 | `	aInstr[1].iP2 = 0;` |
|     1120 |  6806 | `	aInstr[1].p3  = 0;` |
|        - |  6807 | `	/* Execute the method body (if available) */` |
|     1120 |  6808 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  6809 | `	/* Clean up the mess left behind */` |
|     1120 |  6810 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1120 |  6811 | `	return PH7_OK;` |
|      561 |  6812 |  |
|        - |  6813 | `/*` |
|        - |  6814 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  6815 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  6816 | ` * in the apArg[] array.` |
|        - |  6817 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  6818 | ` * return value indicates failure.` |
|        - |  6819 | ` */` |
|      912 |  6820 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
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
|      914 |  6831 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6832 | `		/* Don't bother processing,it's invalid anyway */` |
|      457 |  6833 | `		if( pResult ){` |
|        - |  6834 | `			/* Assume a null return value */` |
|      ! 0 |  6835 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  6836 | `		}` |
|      457 |  6837 | `		return SXERR_INVALID;` |
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
|      458 |  6928 |  |
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
|      416 |  7128 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7129 |  |
|      417 |  7130 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  7131 | `	ph7_value sName;` |
|        - |  7132 | `	sxi32 rc;` |
|        - |  7133 | `	/* Prepare the constant name for insertion */` |
|      417 |  7134 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      417 |  7135 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  7136 | `	/* Perform the insertion */` |
|      417 |  7137 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      417 |  7138 | `	PH7_MemObjRelease(&sName);` |
|      417 |  7139 | `	return rc;` |
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
|     2062 |  7180 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  7181 |  |
|        - |  7182 | `	sxu32 iNum;` |
|     2064 |  7183 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2064 |  7184 | `	return iNum;` |
|        2 |  7185 |  |
|        - |  7186 | `/*` |
|        - |  7187 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  7188 | ` * Note that the generated string is NOT null terminated.` |
|        - |  7189 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  7190 | ` * by te SQLite3 library.` |
|        - |  7191 | ` */` |
|    64966 |  7192 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  7193 |  |
|        - |  7194 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  7195 | `	int i;` |
|        - |  7196 | `	/* Generate a binary string first */` |
|    64968 |  7197 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  7198 | `	/* Turn the binary string into english based alphabet */` |
|   714796 |  7199 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   649830 |  7200 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   324916 |  7201 | `	 }` |
|    64968 |  7202 |  |
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
|    68574 |  7505 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7506 |  |
|        - |  7507 | `	ph7_value *pObj;` |
|    68576 |  7508 | `	int res = 0;` |
|        - |  7509 | `	int i;` |
|    68576 |  7510 | `	if( nArg < 1 ){` |
|        - |  7511 | `		/* Missing arguments,return false */` |
|      ! 0 |  7512 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  7513 | `		return SXRET_OK;` |
|        - |  7514 | `	}` |
|        - |  7515 | `	/* Iterate over available arguments */` |
|    90762 |  7516 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    68576 |  7517 | `		pObj = apArg[i];` |
|    68576 |  7518 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    45908 |  7519 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7520 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  7521 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  7522 | `			}` |
|    22953 |  7523 | `		}` |
|    68576 |  7524 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    68576 |  7525 | `		if( !res ){` |
|        - |  7526 | `			/* Variable not set,return FALSE */` |
|    46390 |  7527 | `			ph7_result_bool(pCtx,0);` |
|    46390 |  7528 | `			return SXRET_OK;` |
|        - |  7529 | `		}` |
|    11095 |  7530 | `	}` |
|        - |  7531 | `	/* All given variable are set,return TRUE */` |
|    22188 |  7532 | `	ph7_result_bool(pCtx,1);` |
|    22188 |  7533 | `	return SXRET_OK;` |
|    34289 |  7534 |  |
|        - |  7535 | `/*` |
|        - |  7536 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  7537 | ` * frame,the reference table and discard it's contents.` |
|        - |  7538 | ` * This function never fail and always return SXRET_OK.` |
|        - |  7539 | ` */` |
|  2949364 |  7540 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  7541 |  |
|        - |  7542 | `	ph7_value *pObj;` |
|        - |  7543 | `	VmRefObj *pRef;` |
|  2949366 |  7544 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2949366 |  7545 | `	if( pObj ){` |
|        - |  7546 | `		/* Release the object */` |
|  2949366 |  7547 | `		PH7_MemObjRelease(pObj);` |
|  1474682 |  7548 | `	}` |
|        - |  7549 | `	/* Remove old reference links */` |
|  2949366 |  7550 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2949366 |  7551 | `	if( pRef ){` |
|  2949346 |  7552 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  7553 | `		/* Unlink from the reference table */` |
|  2949346 |  7554 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2949346 |  7555 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  7556 | `			VmSlot sFree;` |
|        - |  7557 | `			/* Restore to the free list */` |
|  2949340 |  7558 | `			sFree.nIdx = nObjIdx;` |
|  2949340 |  7559 | `			sFree.pUserData = 0;` |
|  2949340 |  7560 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1474669 |  7561 | `		}` |
|  1474672 |  7562 | `	}` |
|  2949366 |  7563 | `	return SXRET_OK;` |
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
|     9652 |  7581 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6398 |  7582 | `		pObj = apArg[i];` |
|     6398 |  7583 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      868 |  7584 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7585 | `				/* Throw an error */` |
|      ! 0 |  7586 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  7587 | `			}` |
|      435 |  7588 | `		}else{` |
|     5531 |  7589 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  7590 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     5531 |  7591 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     5525 |  7592 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2762 |  7593 | `			}` |
|        - |  7594 | `		}` |
|     3200 |  7595 | `	}` |
|     3256 |  7596 | `	return SXRET_OK;` |
|        2 |  7597 |  |
|        - |  7598 | `/*` |
|        - |  7599 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  7600 | ` */` |
|       24 |  7601 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7602 |  |
|       25 |  7603 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|       25 |  7604 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  7605 | `	ph7_value *pObj;` |
|        - |  7606 | `	sxu32 nIdx;` |
|        - |  7607 | `	/* Extract the memory object */` |
|       25 |  7608 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       25 |  7609 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       25 |  7610 | `	if( pObj ){` |
|       25 |  7611 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|       23 |  7612 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  7613 | `				SyString sName;` |
|        - |  7614 | `				ph7_value sKey;` |
|        - |  7615 | `				/* Perform the insertion */` |
|       23 |  7616 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|       23 |  7617 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|       23 |  7618 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|       23 |  7619 | `				PH7_MemObjRelease(&sKey);` |
|       11 |  7620 | `			}` |
|       11 |  7621 | `		}` |
|       12 |  7622 | `	}` |
|       25 |  7623 | `	return SXRET_OK;` |
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
|        - |  7805 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  7806 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  7807 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  7808 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  7809 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  7810 | ` * $value` |
|        - |  7811 | ` *   An optional new value for the option.` |
|        - |  7812 | ` * Return` |
|        - |  7813 | ` *  Old setting on success or FALSE on failure.` |
|        - |  7814 | ` */` |
|       30 |  7815 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7816 |  |
|       32 |  7817 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7818 | `	int iOption;` |
|        - |  7819 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       32 |  7820 | `	if( nArg < 1 ){` |
|        3 |  7821 | `		return PH7_VmThrowException(pCtx,` |
|        - |  7822 | `			"ArgumentCountError",` |
|        - |  7823 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  7824 | `			);` |
|        - |  7825 | `	}` |
|        - |  7826 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       28 |  7827 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       30 |  7828 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  7829 | `		return PH7_VmThrowException(pCtx,` |
|        - |  7830 | `			"TypeError",` |
|        - |  7831 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  7832 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  7833 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  7834 | `			);` |
|        - |  7835 | `	}` |
|       30 |  7836 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  7837 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  7838 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  7839 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       30 |  7840 | `	switch( iOption ){` |
|        6 |  7841 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  7842 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       14 |  7843 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       14 |  7844 | `		if( nArg > 1 ){` |
|        5 |  7845 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  7846 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  7847 | `			}else{` |
|        3 |  7848 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  7849 | `			}` |
|        2 |  7850 | `		}` |
|       14 |  7851 | `		break;` |
|        1 |  7852 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  7853 | `		/* Return old callback or null */` |
|        3 |  7854 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  7855 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  7856 | `		}else{` |
|        3 |  7857 | `			ph7_result_null(pCtx);` |
|        - |  7858 | `		}` |
|        3 |  7859 | `		if( nArg > 1 ){` |
|      ! 0 |  7860 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  7861 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  7862 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  7863 | `			}else{` |
|      ! 0 |  7864 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  7865 | `			}` |
|      ! 0 |  7866 | `		}` |
|        3 |  7867 | `		break;` |
|        5 |  7868 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  7869 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  7870 | `		if( nArg > 1 ){` |
|        5 |  7871 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  7872 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  7873 | `			}else{` |
|        3 |  7874 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  7875 | `			}` |
|        2 |  7876 | `		}` |
|       11 |  7877 | `		break;` |
|      ! 0 |  7878 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  7879 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  7880 | `		break;` |
|        1 |  7881 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  7882 | `		ph7_result_int(pCtx, 1);` |
|        3 |  7883 | `		break;` |
|      ! 0 |  7884 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  7885 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  7886 | `		break;` |
|        1 |  7887 | `	default:` |
|        - |  7888 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  7889 | `		return PH7_VmThrowException(pCtx,` |
|        - |  7890 | `			"ValueError",` |
|        - |  7891 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  7892 | `			);` |
|        - |  7893 | `	}` |
|       28 |  7894 | `	return PH7_OK;` |
|       17 |  7895 |  |
|        - |  7896 | `/*` |
|        - |  7897 | ` * bool assert(mixed $assertion)` |
|        - |  7898 | ` *  Checks if assertion is FALSE.` |
|        - |  7899 | ` * Parameter` |
|        - |  7900 | ` *  $assertion` |
|        - |  7901 | ` *    The assertion to test.` |
|        - |  7902 | ` * Return` |
|        - |  7903 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  7904 | ` */` |
|       26 |  7905 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7906 |  |
|       28 |  7907 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7908 | `	int iFlags,iResult;` |
|        - |  7909 | `	const char *zDesc;` |
|        - |  7910 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       28 |  7911 | `	if( nArg < 1 ){` |
|        3 |  7912 | `		return PH7_VmThrowException(pCtx,` |
|        - |  7913 | `			"ArgumentCountError",` |
|        - |  7914 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  7915 | `			);` |
|        - |  7916 | `	}` |
|       26 |  7917 | `	iFlags = pVm->iAssertFlags;` |
|       26 |  7918 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  7919 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  7920 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  7921 | `		return PH7_OK;` |
|        - |  7922 | `	}` |
|        - |  7923 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       26 |  7924 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       26 |  7925 | `	if( !iResult ){` |
|        - |  7926 | `		/* Assertion failed */` |
|        - |  7927 | `		/* Extract optional description */` |
|       13 |  7928 | `		zDesc = 0;` |
|       13 |  7929 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  7930 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  7931 | `		}` |
|       13 |  7932 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  7933 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  7934 | `			ph7_value sFile,sLine;` |
|        - |  7935 | `			ph7_value *apCbArg[3];` |
|        - |  7936 | `			SyString *pFile;` |
|        - |  7937 | `			/* Extract the processed script */` |
|      ! 0 |  7938 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  7939 | `			if( pFile == 0 ){` |
|      ! 0 |  7940 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  7941 | `			}` |
|        - |  7942 | `			/* Invoke the callback */` |
|      ! 0 |  7943 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  7944 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  7945 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  7946 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  7947 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  7948 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  7949 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  7950 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  7951 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  7952 | `		}` |
|       13 |  7953 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  7954 | `			/* Abort VM execution immediately */` |
|      ! 0 |  7955 | `			return PH7_ABORT;` |
|        - |  7956 | `		}` |
|        - |  7957 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  7958 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  7959 | `			return PH7_VmThrowException(pCtx,` |
|        - |  7960 | `				"AssertionError",` |
|        - |  7961 | `				"%s",` |
|        1 |  7962 | `				zDesc` |
|        - |  7963 | `				);` |
|      ! 0 |  7964 | `		}else{` |
|       11 |  7965 | `			return PH7_VmThrowException(pCtx,` |
|        - |  7966 | `				"AssertionError",` |
|        - |  7967 | `				"assert(false)"` |
|        - |  7968 | `				);` |
|        - |  7969 | `		}` |
|        - |  7970 | `	}` |
|        - |  7971 | `	/* Assertion passed */` |
|       14 |  7972 | `	ph7_result_bool(pCtx,1);` |
|       14 |  7973 | `	return PH7_OK;` |
|       15 |  7974 |  |
|        - |  7975 | `/*` |
|        - |  7976 | ` * Section:` |
|        - |  7977 | ` *  Error reporting functions.` |
|        - |  7978 | ` * Status:` |
|        - |  7979 | ` *    Stable.` |
|        - |  7980 | ` */` |
|        - |  7981 | `/*` |
|        - |  7982 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  7983 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  7984 | ` * Parameters` |
|        - |  7985 | ` *  $error_msg` |
|        - |  7986 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  7987 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  7988 | ` * $error_type` |
|        - |  7989 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  7990 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  7991 | ` * Return` |
|        - |  7992 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  7993 | ` */` |
|       12 |  7994 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7995 |  |
|       14 |  7996 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  7997 | `	int rc = PH7_OK;` |
|       14 |  7998 | `	if( nArg > 0 ){` |
|        - |  7999 | `		const char *zErr;` |
|        - |  8000 | `		int nLen;` |
|        - |  8001 | `		/* Extract the error message */` |
|       12 |  8002 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8003 | `		if( nArg > 1 ){` |
|        - |  8004 | `			/* Extract the error type */` |
|       12 |  8005 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  8006 | `			switch( nErr ){` |
|        1 |  8007 | `			case 1:   /* E_ERROR */` |
|        - |  8008 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  8009 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  8010 | `			case 256: /* E_USER_ERROR */` |
|        3 |  8011 | `				nErr = PH7_CTX_ERR;` |
|        3 |  8012 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  8013 | `				break;` |
|        1 |  8014 | `			case 2:   /* E_WARNING */` |
|        - |  8015 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  8016 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  8017 | `			case 512: /* E_USER_WARNING */` |
|        3 |  8018 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  8019 | `				break;` |
|        3 |  8020 | `			default:` |
|        8 |  8021 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  8022 | `				break;` |
|        - |  8023 | `			}` |
|        5 |  8024 | `		}` |
|        - |  8025 | `		/* Report error */` |
|       12 |  8026 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  8027 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  8028 | `			return rc;` |
|        - |  8029 | `		}` |
|        - |  8030 | `		/* Return true */` |
|       12 |  8031 | `		ph7_result_bool(pCtx,1);` |
|        7 |  8032 | `	}else{` |
|        - |  8033 | `		/* Missing arguments,return FALSE */` |
|        3 |  8034 | `		ph7_result_bool(pCtx,0);` |
|        - |  8035 | `	}` |
|       14 |  8036 | `	return rc;` |
|        8 |  8037 |  |
|        - |  8038 | `/*` |
|        - |  8039 | ` * int error_reporting([int $level])` |
|        - |  8040 | ` *  Sets which PHP errors are reported.` |
|        - |  8041 | ` * Parameters` |
|        - |  8042 | ` *  $level` |
|        - |  8043 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  8044 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  8045 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  8046 | ` *   levels will not always behave as expected.` |
|        - |  8047 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  8048 | ` *   in the predefined constants.` |
|        - |  8049 | ` * Return` |
|        - |  8050 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  8051 | ` *   parameter is given.` |
|        - |  8052 | ` */` |
|       40 |  8053 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8054 |  |
|       42 |  8055 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8056 | `	int nOld;` |
|        - |  8057 | `	/* Extract the old reporting level */` |
|       42 |  8058 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       42 |  8059 | `	if( nArg > 0 ){` |
|        - |  8060 | `		int nNew;` |
|        - |  8061 | `		/* Extract the desired error reporting level */` |
|       34 |  8062 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       34 |  8063 | `		if( !nNew ){` |
|        - |  8064 | `			/* Do not report errors at all */` |
|        5 |  8065 | `			pVm->bErrReport = 0;` |
|        3 |  8066 | `		}else{` |
|        - |  8067 | `			/* Report all errors */` |
|       30 |  8068 | `			pVm->bErrReport = 1;` |
|        - |  8069 | `		}` |
|       16 |  8070 | `	}` |
|        - |  8071 | `	/* Return the old level */` |
|       42 |  8072 | `	ph7_result_int(pCtx,nOld);` |
|       42 |  8073 | `	return PH7_OK;` |
|        2 |  8074 |  |
|        - |  8075 | `/*` |
|        - |  8076 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  8077 | ` *  Send an error message somewhere.` |
|        - |  8078 | ` * Parameter` |
|        - |  8079 | ` *  $message` |
|        - |  8080 | ` *   The error message that should be logged.` |
|        - |  8081 | ` *  $message_type` |
|        - |  8082 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  8083 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  8084 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  8085 | ` *       This is the default option.` |
|        - |  8086 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  8087 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  8088 | ` *    2  No longer an option.` |
|        - |  8089 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  8090 | ` *       to the end of the message string.` |
|        - |  8091 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  8092 | ` *  $destination` |
|        - |  8093 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  8094 | ` *  $extra_headers` |
|        - |  8095 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  8096 | ` * Return` |
|        - |  8097 | ` *  TRUE on success or FALSE on failure.` |
|        - |  8098 | ` * NOTE:` |
|        - |  8099 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  8100 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  8101 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  8102 | ` *  Otherwise this function is no-op.` |
|        - |  8103 | ` */` |
|        4 |  8104 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8105 |  |
|        - |  8106 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  8107 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  8108 | `	int iType = 0;` |
|        5 |  8109 | `	if( nArg < 1 ){` |
|        - |  8110 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  8111 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8112 | `		return PH7_OK;` |
|        - |  8113 | `	}` |
|        5 |  8114 | `	if( pVm->xErrLog  ){` |
|        - |  8115 | `		/* Invoke the user callback */` |
|      ! 0 |  8116 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  8117 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  8118 | `		if( nArg > 1 ){` |
|      ! 0 |  8119 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  8120 | `			if( nArg > 2 ){` |
|      ! 0 |  8121 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  8122 | `				if( nArg > 3 ){` |
|      ! 0 |  8123 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  8124 | `				}` |
|      ! 0 |  8125 | `			}` |
|      ! 0 |  8126 | `		}` |
|      ! 0 |  8127 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  8128 | `	}` |
|        - |  8129 | `	/* Retun TRUE */` |
|        5 |  8130 | `	ph7_result_bool(pCtx,1);` |
|        5 |  8131 | `	return PH7_OK;` |
|        3 |  8132 |  |
|        - |  8133 | `/*` |
|        - |  8134 | ` * bool restore_exception_handler(void)` |
|        - |  8135 | ` *  Restores the previously defined exception handler function.` |
|        - |  8136 | ` * Parameter` |
|        - |  8137 | ` *  None` |
|        - |  8138 | ` * Return` |
|        - |  8139 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  8140 | ` */` |
|        4 |  8141 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8142 |  |
|        5 |  8143 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8144 | `	ph7_value *pOld,*pNew;` |
|        - |  8145 | `	/* Point to the old and the new handler */` |
|        5 |  8146 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  8147 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  8148 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8149 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8150 | `		SXUNUSED(apArg);` |
|        - |  8151 | `		/* No installed handler,return FALSE */` |
|        5 |  8152 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8153 | `		return PH7_OK;` |
|        - |  8154 | `	}` |
|        - |  8155 | `	/* Copy the old handler */` |
|      ! 0 |  8156 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8157 | `	PH7_MemObjRelease(pOld);` |
|        - |  8158 | `	/* Return TRUE */` |
|      ! 0 |  8159 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8160 | `	return PH7_OK;` |
|        3 |  8161 |  |
|        - |  8162 | `/*` |
|        - |  8163 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  8164 | ` *  Sets a user-defined exception handler function.` |
|        - |  8165 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  8166 | ` * NOTE` |
|        - |  8167 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  8168 | ` *  the satndard PHP engine.` |
|        - |  8169 | ` * Parameters` |
|        - |  8170 | ` *  $exception_handler` |
|        - |  8171 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  8172 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  8173 | ` *   that was thrown.` |
|        - |  8174 | ` *  Note:` |
|        - |  8175 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8176 | ` * Return` |
|        - |  8177 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  8178 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8179 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8180 | ` */` |
|        4 |  8181 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8182 |  |
|        6 |  8183 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8184 | `	ph7_value *pOld,*pNew;` |
|        - |  8185 | `	/* Point to the old and the new handler */` |
|        6 |  8186 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  8187 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  8188 | `	/* Return the old handler */` |
|        6 |  8189 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  8190 | `	if( nArg > 0 ){` |
|        6 |  8191 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8192 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  8193 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  8194 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  8195 | `		}else{` |
|        6 |  8196 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8197 | `			/* Install the new handler */` |
|        6 |  8198 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8199 | `		}` |
|        2 |  8200 | `	}` |
|        6 |  8201 | `	return PH7_OK;` |
|        2 |  8202 |  |
|        - |  8203 | `/*` |
|        - |  8204 | ` * bool restore_error_handler(void)` |
|        - |  8205 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8206 | ` * Parameters:` |
|        - |  8207 | ` *  None.` |
|        - |  8208 | ` * Return` |
|        - |  8209 | ` *  Always TRUE.` |
|        - |  8210 | ` */` |
|        4 |  8211 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8212 |  |
|        5 |  8213 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8214 | `	ph7_value *pOld,*pNew;` |
|        - |  8215 | `	/* Point to the old and the new handler */` |
|        5 |  8216 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  8217 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  8218 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8219 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8220 | `		SXUNUSED(apArg);` |
|        - |  8221 | `		/* No installed callback,return FALSE */` |
|        5 |  8222 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8223 | `		return PH7_OK;` |
|        - |  8224 | `	}` |
|        - |  8225 | `	/* Copy the old callback */` |
|      ! 0 |  8226 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8227 | `	PH7_MemObjRelease(pOld);` |
|        - |  8228 | `	/* Return TRUE */` |
|      ! 0 |  8229 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8230 | `	return PH7_OK;` |
|        3 |  8231 |  |
|        - |  8232 | `/*` |
|        - |  8233 | ` * value set_error_handler(callable $error_handler)` |
|        - |  8234 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8235 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8236 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8237 | ` *  Sets a user-defined error handler function.` |
|        - |  8238 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  8239 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  8240 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  8241 | ` *  conditions (using trigger_error()).` |
|        - |  8242 | ` * Parameters` |
|        - |  8243 | ` *  $error_handler` |
|        - |  8244 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  8245 | ` *   describing the error.` |
|        - |  8246 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  8247 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  8248 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  8249 | ` *   The function can be shown as:` |
|        - |  8250 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  8251 | ` *     errno` |
|        - |  8252 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  8253 | ` *   errstr` |
|        - |  8254 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  8255 | ` *   errfile` |
|        - |  8256 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  8257 | ` *     was raised in, as a string.` |
|        - |  8258 | ` *  Note:` |
|        - |  8259 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8260 | ` * Return` |
|        - |  8261 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  8262 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8263 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8264 | ` */` |
|     8730 |  8265 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8266 |  |
|     8732 |  8267 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8268 | `	ph7_value *pOld,*pNew;` |
|        - |  8269 | `	/* Point to the old and the new handler */` |
|     8732 |  8270 | `	pOld = &pVm->aErrCB[0];` |
|     8732 |  8271 | `	pNew = &pVm->aErrCB[1];` |
|        - |  8272 | `	/* Return the old handler */` |
|     8732 |  8273 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8732 |  8274 | `	if( nArg > 0 ){` |
|     8732 |  8275 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8276 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4365 |  8277 | `			PH7_MemObjRelease(pNew);` |
|     4365 |  8278 | `			ph7_result_bool(pCtx,1);` |
|     2183 |  8279 | `		}else{` |
|     4368 |  8280 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8281 | `			/* Install the new handler */` |
|     4368 |  8282 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8283 | `		}` |
|     4365 |  8284 | `	}` |
|     8732 |  8285 | `	return PH7_OK;` |
|        2 |  8286 |  |
|        - |  8287 | `/*` |
|        - |  8288 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  8289 | ` *  Generates a backtrace.` |
|        - |  8290 | ` * Paramaeter` |
|        - |  8291 | ` *  $options` |
|        - |  8292 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  8293 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  8294 | ` *   all the function/method arguments, to save memory.` |
|        - |  8295 | ` * $limit` |
|        - |  8296 | ` *   (Not Used)` |
|        - |  8297 | ` * Return` |
|        - |  8298 | ` *  An array.The possible returned elements are as follows:` |
|        - |  8299 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  8300 | ` *          Name        Type      Description` |
|        - |  8301 | ` *          ------      ------     -----------` |
|        - |  8302 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  8303 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  8304 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  8305 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  8306 | ` *          object      object    The current object.` |
|        - |  8307 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  8308 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  8309 | ` */` |
|      478 |  8310 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8311 |  |
|      480 |  8312 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8313 | `	ph7_value *pArray;` |
|        - |  8314 | `	ph7_class *pClass;` |
|        - |  8315 | `	ph7_value *pValue;` |
|        - |  8316 | `	SyString *pFile;` |
|        - |  8317 | `	/* Create a new array */` |
|      480 |  8318 | `	pArray = ph7_context_new_array(pCtx);` |
|      480 |  8319 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      480 |  8320 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8321 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8322 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  8323 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8324 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8325 | `		SXUNUSED(apArg);` |
|      ! 0 |  8326 | `		return PH7_OK;` |
|        - |  8327 | `	}` |
|        - |  8328 | `	/* Dump running function name and it's arguments  */` |
|      480 |  8329 | `	if( pVm->pFrame->pParent ){` |
|      480 |  8330 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8331 | `		ph7_vm_func *pFunc;` |
|        - |  8332 | `		ph7_value *pArg;` |
|      480 |  8333 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8334 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  8335 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  8336 | `		}` |
|      480 |  8337 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      480 |  8338 | `		if( pFrame->pParent && pFunc ){` |
|      480 |  8339 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      480 |  8340 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      480 |  8341 | `			ph7_value_reset_string_cursor(pValue);` |
|      239 |  8342 | `		}` |
|        - |  8343 | `		/* Function arguments */` |
|      480 |  8344 | `		pArg = ph7_context_new_array(pCtx);` |
|      480 |  8345 | `		if( pArg  ){` |
|        - |  8346 | `			ph7_value *pObj;` |
|        - |  8347 | `			VmSlot *aSlot;` |
|        - |  8348 | `			sxu32 n;` |
|        - |  8349 | `			/* Start filling the array with the given arguments */` |
|      480 |  8350 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     1906 |  8351 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1428 |  8352 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1428 |  8353 | `				if( pObj ){` |
|     1428 |  8354 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      713 |  8355 | `				}` |
|      715 |  8356 | `			}` |
|        - |  8357 | `			/* Save the array */` |
|      480 |  8358 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      239 |  8359 | `		}` |
|      239 |  8360 | `	}` |
|      480 |  8361 | `	ph7_value_int(pValue,1);` |
|        - |  8362 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  8363 | `	 * line numbers at run-time. )` |
|        - |  8364 | `	 */` |
|      480 |  8365 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  8366 | `	/* Current processed script */` |
|      480 |  8367 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      480 |  8368 | `	if( pFile ){` |
|      480 |  8369 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      480 |  8370 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      480 |  8371 | `		ph7_value_reset_string_cursor(pValue);` |
|      239 |  8372 | `	}` |
|        - |  8373 | `	/* Top class */` |
|      480 |  8374 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      480 |  8375 | `	if( pClass ){` |
|      476 |  8376 | `		ph7_value_reset_string_cursor(pValue);` |
|      476 |  8377 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      476 |  8378 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      237 |  8379 | `	}` |
|        - |  8380 | `	/* Return the freshly created array */` |
|      480 |  8381 | `	ph7_result_value(pCtx,pArray);` |
|        - |  8382 | `	/*` |
|        - |  8383 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  8384 | `	 * as soon we return from this function.` |
|        - |  8385 | `	 */` |
|      480 |  8386 | `	return PH7_OK;` |
|      241 |  8387 |  |
|        - |  8388 | `/*` |
|        - |  8389 | ` * Generate a small backtrace.` |
|        - |  8390 | ` * Store the generated dump in the given BLOB` |
|        - |  8391 | ` */` |
|        4 |  8392 | `static int VmMiniBacktrace(` |
|        - |  8393 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8394 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  8395 | `	)` |
|        1 |  8396 |  |
|        5 |  8397 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8398 | `	ph7_vm_func *pFunc;` |
|        - |  8399 | `	ph7_class *pClass;` |
|        - |  8400 | `	SyString *pFile;` |
|        - |  8401 | `	/* Called function */` |
|        5 |  8402 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8403 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  8404 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  8405 | `	}` |
|        5 |  8406 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  8407 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8408 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  8409 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  8410 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  8411 | `	}else{` |
|      ! 0 |  8412 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  8413 | `	}` |
|        5 |  8414 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  8415 | `	/* Current processed script */` |
|        5 |  8416 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  8417 | `	if( pFile ){` |
|        5 |  8418 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8419 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  8420 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  8421 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  8422 | `	}` |
|        - |  8423 | `	/* Top class */` |
|        5 |  8424 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  8425 | `	if( pClass ){` |
|      ! 0 |  8426 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  8427 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  8428 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  8429 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  8430 | `	}` |
|        5 |  8431 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  8432 | `	/* All done */` |
|        5 |  8433 | `	return SXRET_OK;` |
|        1 |  8434 |  |
|        - |  8435 | `/*` |
|        - |  8436 | ` * void debug_print_backtrace()` |
|        - |  8437 | ` *  Prints a backtrace` |
|        - |  8438 | ` * Parameters` |
|        - |  8439 | ` * None` |
|        - |  8440 | ` * Return` |
|        - |  8441 | ` * NULL` |
|        - |  8442 | ` */` |
|        2 |  8443 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8444 |  |
|        3 |  8445 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8446 | `	SyBlob sDump;` |
|        3 |  8447 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8448 | `	/* Generate the backtrace */` |
|        3 |  8449 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8450 | `	/* Output backtrace */` |
|        3 |  8451 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8452 | `	/* All done,cleanup */` |
|        3 |  8453 | `	SyBlobRelease(&sDump);` |
|        1 |  8454 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8455 | `	SXUNUSED(apArg);` |
|        3 |  8456 | `	return PH7_OK;` |
|        1 |  8457 |  |
|        - |  8458 | `/*` |
|        - |  8459 | ` * string debug_string_backtrace()` |
|        - |  8460 | ` *  Generate a backtrace` |
|        - |  8461 | ` * Parameters` |
|        - |  8462 | ` * None` |
|        - |  8463 | ` * Return` |
|        - |  8464 | ` *  A mini backtrace().` |
|        - |  8465 | ` * Note that this is a symisc extension.` |
|        - |  8466 | ` */` |
|        2 |  8467 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8468 |  |
|        3 |  8469 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8470 | `	SyBlob sDump;` |
|        3 |  8471 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8472 | `	/* Generate the backtrace */` |
|        3 |  8473 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8474 | `	/* Return the backtrace */` |
|        3 |  8475 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  8476 | `	/* All done,cleanup */` |
|        3 |  8477 | `	SyBlobRelease(&sDump);` |
|        1 |  8478 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8479 | `	SXUNUSED(apArg);` |
|        3 |  8480 | `	return PH7_OK;` |
|        1 |  8481 |  |
|        - |  8482 | `/*` |
|        - |  8483 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  8484 | ` * exception is triggered.` |
|        - |  8485 | ` */` |
|      458 |  8486 | `static sxi32 VmUncaughtException(` |
|        - |  8487 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8488 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8489 | `	)` |
|        1 |  8490 |  |
|        - |  8491 | `	ph7_value *apArg[2],sArg;` |
|      459 |  8492 | `	int nArg = 1;` |
|        - |  8493 | `	sxi32 rc;` |
|      459 |  8494 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  8495 | `		/* Nesting limit reached */` |
|      ! 0 |  8496 | `		return SXRET_OK;` |
|        - |  8497 | `	}` |
|        - |  8498 | `	/* Call any exception handler if available */` |
|      459 |  8499 | `	PH7_MemObjInit(pVm,&sArg);` |
|      459 |  8500 | `	if( pThis ){` |
|        - |  8501 | `		/* Load the exception instance */` |
|      459 |  8502 | `		sArg.x.pOther = pThis;` |
|      459 |  8503 | `		pThis->iRef++;` |
|      459 |  8504 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      230 |  8505 | `	}else{` |
|      ! 0 |  8506 | `		nArg = 0;` |
|        - |  8507 | `	}` |
|      459 |  8508 | `	apArg[0] = &sArg;` |
|        - |  8509 | `	/* Call the exception handler if available */` |
|      459 |  8510 | `	pVm->nExceptDepth++;` |
|      459 |  8511 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      459 |  8512 | `	pVm->nExceptDepth--;` |
|      459 |  8513 | `	if( rc != SXRET_OK ){` |
|        - |  8514 | `		SyBlob sMsgBuf;` |
|      457 |  8515 | `		const char *zClass = "Exception";` |
|      457 |  8516 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  8517 | `		const char *zMsg;` |
|        - |  8518 | `		sxu32 nMsg;` |
|        - |  8519 | `		const char *zFuncName;` |
|        - |  8520 | `		int nFuncLen;` |
|      457 |  8521 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      457 |  8522 | `		if( pThis ){` |
|        - |  8523 | `			ph7_class_method *pGetMessage;` |
|        - |  8524 | `			ph7_value sMsg;` |
|        - |  8525 | `			const char *zTmp;` |
|        - |  8526 | `			int nTmp;` |
|      457 |  8527 | `			zClass = pThis->pClass->sName.zString;` |
|      457 |  8528 | `			nClass = pThis->pClass->sName.nByte;` |
|      457 |  8529 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      457 |  8530 | `			if( pGetMessage ){` |
|      457 |  8531 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      457 |  8532 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      457 |  8533 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      457 |  8534 | `					if( zTmp && nTmp > 0 ){` |
|      457 |  8535 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      228 |  8536 | `					}` |
|      228 |  8537 | `				}` |
|      457 |  8538 | `				PH7_MemObjRelease(&sMsg);` |
|      228 |  8539 | `			}` |
|      228 |  8540 | `		}` |
|      457 |  8541 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  8542 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  8543 | `		}` |
|      457 |  8544 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      457 |  8545 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      457 |  8546 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      457 |  8547 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      457 |  8548 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  8549 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      457 |  8550 | `		rc = SXERR_ABORT;` |
|      228 |  8551 | `	}` |
|      459 |  8552 | `	PH7_MemObjRelease(&sArg);` |
|      459 |  8553 | `	return rc;` |
|      230 |  8554 |  |
|        - |  8555 | `/*` |
|        - |  8556 | ` * Throw an user exception.` |
|        - |  8557 | ` */` |
|      476 |  8558 | `static sxi32 VmThrowException(` |
|        - |  8559 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  8560 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8561 | `	)` |
|        2 |  8562 |  |
|        - |  8563 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  8564 | `	ph7_exception **apException;` |
|        - |  8565 | `	ph7_exception *pException;` |
|        - |  8566 | `	/* Point to the stack of loaded exceptions */` |
|      478 |  8567 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      478 |  8568 | `	pException = 0;` |
|      478 |  8569 | `	pCatch = 0;` |
|      478 |  8570 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8571 | `		ph7_exception_block *aCatch;` |
|        - |  8572 | `		ph7_class *pClass;` |
|        - |  8573 | `		sxu32 j;` |
|        - |  8574 | `		/* Locate the appropriate block to execute */` |
|       20 |  8575 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       20 |  8576 | `		(void)SySetPop(&pVm->aException);` |
|       20 |  8577 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       20 |  8578 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       20 |  8579 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  8580 | `			/* Extract the target class */` |
|       20 |  8581 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       20 |  8582 | `			if( pClass == 0 ){` |
|        - |  8583 | `				/* No such class */` |
|      ! 0 |  8584 | `				continue;` |
|        - |  8585 | `			}` |
|       20 |  8586 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  8587 | `				/* Catch block found,break immeditaley */` |
|       20 |  8588 | `				pCatch = &aCatch[j];` |
|       20 |  8589 | `				break;` |
|        - |  8590 | `			}` |
|      ! 0 |  8591 | `		}` |
|        9 |  8592 | `	}` |
|        - |  8593 | `	/* Execute the cached block if available */` |
|      478 |  8594 | `	if( pCatch == 0 ){` |
|        - |  8595 | `		sxi32 rc;` |
|      459 |  8596 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      459 |  8597 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  8598 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  8599 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8600 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  8601 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  8602 | `			}` |
|      ! 0 |  8603 | `			if( pException->pFrame == pFrame ){` |
|        - |  8604 | `				/* Tell the upper layer that the exception was caught */` |
|      ! 0 |  8605 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  8606 | `			}` |
|      ! 0 |  8607 | `		}` |
|      459 |  8608 | `		return rc;` |
|      ! 0 |  8609 | `	}else{` |
|       20 |  8610 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8611 | `		sxi32 rc;` |
|       42 |  8612 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8613 | `			/* Safely ignore the exception frame */` |
|       24 |  8614 | `			pFrame = pFrame->pParent;` |
|        2 |  8615 | `		}` |
|       20 |  8616 | `		if( pException->pFrame == pFrame ){` |
|        - |  8617 | `			/* Tell the upper layer that the exception was caught */` |
|       12 |  8618 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        5 |  8619 | `		}` |
|        - |  8620 | `		/* Create a private frame first */` |
|       20 |  8621 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       20 |  8622 | `		if( rc == SXRET_OK ){` |
|        - |  8623 | `			/* Mark as catch frame */` |
|       20 |  8624 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       20 |  8625 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       20 |  8626 | `			if( pObj ){` |
|        - |  8627 | `				/* Install the exception instance */` |
|       20 |  8628 | `				pThis->iRef++; /* Increment reference count */` |
|       20 |  8629 | `				pObj->x.pOther = pThis;` |
|       20 |  8630 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        9 |  8631 | `			}` |
|        - |  8632 | `			/* Exceute the block */` |
|       20 |  8633 | `			VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  8634 | `			/* Leave the frame */` |
|       20 |  8635 | `			VmLeaveFrame(&(*pVm));` |
|        9 |  8636 | `		}` |
|        - |  8637 | `	}` |
|        - |  8638 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  8639 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  8640 | `	 */` |
|       20 |  8641 | `	return SXRET_OK;` |
|      240 |  8642 |  |
|        - |  8643 | `/*` |
|        - |  8644 | ` * Section:` |
|        - |  8645 | ` *  Version,Credits and Copyright related functions.` |
|        - |  8646 | ` * Status:` |
|        - |  8647 | ` *    Stable.` |
|        - |  8648 | ` */` |
|        - |  8649 | `/*` |
|        - |  8650 | ` * string ph7version(void)` |
|        - |  8651 | ` *  Returns the running version of the PH7 version.` |
|        - |  8652 | ` * Parameters` |
|        - |  8653 | ` *  None` |
|        - |  8654 | ` * Return` |
|        - |  8655 | ` * Current PH7 version.` |
|        - |  8656 | ` */` |
|        2 |  8657 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8658 |  |
|        1 |  8659 | `	SXUNUSED(nArg);` |
|        1 |  8660 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  8661 | `	/* Current engine version */` |
|        3 |  8662 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  8663 | `	return PH7_OK;` |
|        1 |  8664 |  |
|        - |  8665 | `/*` |
|        - |  8666 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  8667 | ` */` |
|        - |  8668 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  8669 | ` "<html><head>"\` |
|        - |  8670 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  8671 | ` "<style type=\"text/css\">"\` |
|        - |  8672 | ` "div {"\` |
|        - |  8673 | `     "border: 1px solid #cccccc;"\` |
|        - |  8674 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  8675 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  8676 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  8677 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  8678 | `     "-webkit-border-radius: 10px;"\` |
|        - |  8679 | `     "-o-border-radius: 10px;"\` |
|        - |  8680 | `     "border-radius: 10px;"\` |
|        - |  8681 | `     "padding-left: 2em;"\` |
|        - |  8682 | `     "background-color: white;"\` |
|        - |  8683 | `     "margin-left: auto;"\` |
|        - |  8684 | `     "font-family: verdana;"\` |
|        - |  8685 | `     "padding-right: 2em;"\` |
|        - |  8686 | `     "margin-right: auto;"\` |
|        - |  8687 | `     "}"\` |
|        - |  8688 | `     "body {"\` |
|        - |  8689 | `     "padding: 0.2em;"\` |
|        - |  8690 | `     "font-style: normal;"\` |
|        - |  8691 | `     "font-size: medium;"\` |
|        - |  8692 | `     "background-color: #f2f2f2;"\` |
|        - |  8693 | `     "}"\` |
|        - |  8694 | `     "hr {"\` |
|        - |  8695 | `     "border-style: solid none none;"\` |
|        - |  8696 | `     "border-width: 1px medium medium;"\` |
|        - |  8697 | `     "border-top: 1px solid #cccccc;"\` |
|        - |  8698 | `     "height: 1px;"\` |
|        - |  8699 | `     "}"\` |
|        - |  8700 | `     "a {"\` |
|        - |  8701 | `     "color: #3366cc;"\` |
|        - |  8702 | `     "text-decoration: none;"\` |
|        - |  8703 | `     "}"\` |
|        - |  8704 | `     "a:hover {"\` |
|        - |  8705 | `     "color: #999999;"\` |
|        - |  8706 | `     "}"\` |
|        - |  8707 | `     "a:active {"\` |
|        - |  8708 | `     "color: #663399;"\` |
|        - |  8709 | `     "}"\` |
|        - |  8710 | `     "h1 {"\` |
|        - |  8711 | `     "margin: 0;"\` |
|        - |  8712 | `     "padding: 0;"\` |
|        - |  8713 | `     "font-family: Verdana;"\` |
|        - |  8714 | `     "font-weight: bold;"\` |
|        - |  8715 | `     "font-style: normal;"\` |
|        - |  8716 | `     "font-size: medium;"\` |
|        - |  8717 | `     "text-transform: capitalize;"\` |
|        - |  8718 | `     "color: #0a328c;"\` |
|        - |  8719 | `     "}"\` |
|        - |  8720 | `     "p {"\` |
|        - |  8721 | `     "margin: 0 auto;"\` |
|        - |  8722 | `     "font-size: medium;"\` |
|        - |  8723 | `     "font-style: normal;"\` |
|        - |  8724 | `     "font-family: verdana;"\` |
|        - |  8725 | `     "}"\` |
|        - |  8726 | `"</style></head><body>"\` |
|        - |  8727 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - |  8728 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - |  8729 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - |  8730 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - |  8731 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - |  8732 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - |  8733 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - |  8734 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - |  8735 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - |  8736 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - |  8737 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - |  8738 |  |
|        - |  8739 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8740 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - |  8741 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - |  8742 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - |  8743 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8744 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - |  8745 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  8746 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - |  8747 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  8748 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - |  8749 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8750 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - |  8751 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - |  8752 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - |  8753 |  |
|        - |  8754 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - |  8755 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - |  8756 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - |  8757 | `"&nbsp;*<br>"\` |
|        - |  8758 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - |  8759 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - |  8760 | `"&nbsp;* are met:<br>"\` |
|        - |  8761 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - |  8762 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - |  8763 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - |  8764 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - |  8765 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - |  8766 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - |  8767 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - |  8768 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - |  8769 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - |  8770 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - |  8771 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - |  8772 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - |  8773 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - |  8774 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - |  8775 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - |  8776 | `"&nbsp;*<br>"\` |
|        - |  8777 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - |  8778 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - |  8779 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - |  8780 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - |  8781 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - |  8782 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - |  8783 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - |  8784 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - |  8785 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - |  8786 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - |  8787 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - |  8788 | `"&nbsp;*/<br>"\` |
|        - |  8789 | `"</span></small></small></p>"\` |
|        - |  8790 | `"</div></body></html>"` |
|        - |  8791 | `/*` |
|        - |  8792 | ` * bool ph7credits(void)` |
|        - |  8793 | ` * bool ph7info(void)` |
|        - |  8794 | ` * bool ph7copyright(void)` |
|        - |  8795 | ` *  Prints out the credits for PH7 engine` |
|        - |  8796 | ` * Parameters` |
|        - |  8797 | ` *  None` |
|        - |  8798 | ` * Return` |
|        - |  8799 | ` *  Always TRUE` |
|        - |  8800 | ` */` |
|        2 |  8801 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8802 |  |
|        3 |  8803 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - |  8804 | `	/* Expand the HTML page above*/` |
|        3 |  8805 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 |  8806 | `	ph7_context_output_format(` |
|        1 |  8807 | `		pCtx,` |
|        - |  8808 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 |  8809 | `		ph7_lib_version(),   /* Engine version */` |
|        1 |  8810 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 |  8811 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 |  8812 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 |  8813 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 |  8814 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - |  8815 | `#ifdef __WINNT__` |
|        - |  8816 | `		"Windows NT"` |
|        - |  8817 | `#elif defined(__UNIXES__)` |
|        - |  8818 | `		"UNIX-Like"` |
|        - |  8819 | `#else` |
|        - |  8820 | `		"Other OS"` |
|        - |  8821 | `#endif` |
|        - |  8822 | `		);` |
|        3 |  8823 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 |  8824 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8825 | `	SXUNUSED(apArg);` |
|        - |  8826 | `	/* Return TRUE */` |
|        - |  8827 | `	//ph7_result_bool(pCtx,1);` |
|        3 |  8828 | `	return PH7_OK;` |
|        1 |  8829 |  |
|        - |  8830 | `/*` |
|        - |  8831 | ` * Section:` |
|        - |  8832 | ` *    URL related routines.` |
|        - |  8833 | ` * Status:` |
|        - |  8834 | ` *    Stable.` |
|        - |  8835 | ` */` |
|        - |  8836 | `/*` |
|        - |  8837 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - |  8838 | ` *  Parse a URL and return its fields.` |
|        - |  8839 | ` * Parameters` |
|        - |  8840 | ` *  $url` |
|        - |  8841 | ` *   The URL to parse.` |
|        - |  8842 | ` * $component` |
|        - |  8843 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - |  8844 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - |  8845 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - |  8846 | ` *  in which case the return value will be an integer).` |
|        - |  8847 | ` * Return` |
|        - |  8848 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - |  8849 | ` *  At least one element will be present within the array. Potential keys within` |
|        - |  8850 | ` *  this array are:` |
|        - |  8851 | ` *   scheme - e.g. http` |
|        - |  8852 | ` *   host` |
|        - |  8853 | ` *   port` |
|        - |  8854 | ` *   user` |
|        - |  8855 | ` *   pass` |
|        - |  8856 | ` *   path` |
|        - |  8857 | ` *   query - after the question mark ?` |
|        - |  8858 | ` *   fragment - after the hashmark #` |
|        - |  8859 | ` * Note:` |
|        - |  8860 | ` *  FALSE is returned on failure.` |
|        - |  8861 | ` *  This function work with relative URL unlike the one shipped` |
|        - |  8862 | ` *  with the standard PHP engine.` |
|        - |  8863 | ` */` |
|       28 |  8864 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8865 |  |
|        - |  8866 | `	const char *zStr; /* Input string */` |
|        - |  8867 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - |  8868 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - |  8869 | `	int nLen;` |
|        - |  8870 | `	sxi32 rc;` |
|       29 |  8871 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  8872 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  8873 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8874 | `		return PH7_OK;` |
|        - |  8875 | `	}` |
|        - |  8876 | `	/* Extract the given URI */` |
|       29 |  8877 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 |  8878 | `	if( nLen < 1 ){` |
|        - |  8879 | `		/* Nothing to process,return FALSE */` |
|        3 |  8880 | `		ph7_result_bool(pCtx,0);` |
|        3 |  8881 | `		return PH7_OK;` |
|        - |  8882 | `	}` |
|        - |  8883 | `	/* Get a parse */` |
|       27 |  8884 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 |  8885 | `	if( rc != SXRET_OK ){` |
|        - |  8886 | `		/* Malformed input,return FALSE */` |
|      ! 0 |  8887 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8888 | `		return PH7_OK;` |
|        - |  8889 | `	}` |
|       27 |  8890 | `	if( nArg > 1 ){` |
|      ! 0 |  8891 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - |  8892 | `		/* Refer to constant.c for constants values */` |
|      ! 0 |  8893 | `		switch(nComponent){` |
|      ! 0 |  8894 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 |  8895 | `			pComp = &sURI.sScheme;` |
|      ! 0 |  8896 | `			if( pComp->nByte < 1 ){` |
|        - |  8897 | `				/* No available value,return NULL */` |
|      ! 0 |  8898 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8899 | `			}else{` |
|      ! 0 |  8900 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8901 | `			}` |
|      ! 0 |  8902 | `			break;` |
|      ! 0 |  8903 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 |  8904 | `			pComp = &sURI.sHost;` |
|      ! 0 |  8905 | `			if( pComp->nByte < 1 ){` |
|        - |  8906 | `				/* No available value,return NULL */` |
|      ! 0 |  8907 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8908 | `			}else{` |
|      ! 0 |  8909 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8910 | `			}` |
|      ! 0 |  8911 | `			break;` |
|      ! 0 |  8912 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 |  8913 | `			pComp = &sURI.sPort;` |
|      ! 0 |  8914 | `			if( pComp->nByte < 1 ){` |
|        - |  8915 | `				/* No available value,return NULL */` |
|      ! 0 |  8916 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8917 | `			}else{` |
|      ! 0 |  8918 | `				int iPort = 0;` |
|        - |  8919 | `				/* Cast the value to integer */` |
|      ! 0 |  8920 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 |  8921 | `				ph7_result_int(pCtx,iPort);` |
|        - |  8922 | `			}` |
|      ! 0 |  8923 | `			break;` |
|      ! 0 |  8924 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 |  8925 | `			pComp = &sURI.sUser;` |
|      ! 0 |  8926 | `			if( pComp->nByte < 1 ){` |
|        - |  8927 | `				/* No available value,return NULL */` |
|      ! 0 |  8928 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8929 | `			}else{` |
|      ! 0 |  8930 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8931 | `			}` |
|      ! 0 |  8932 | `			break;` |
|      ! 0 |  8933 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 |  8934 | `			pComp = &sURI.sPass;` |
|      ! 0 |  8935 | `			if( pComp->nByte < 1 ){` |
|        - |  8936 | `				/* No available value,return NULL */` |
|      ! 0 |  8937 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8938 | `			}else{` |
|      ! 0 |  8939 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8940 | `			}` |
|      ! 0 |  8941 | `			break;` |
|      ! 0 |  8942 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 |  8943 | `			pComp = &sURI.sQuery;` |
|      ! 0 |  8944 | `			if( pComp->nByte < 1 ){` |
|        - |  8945 | `				/* No available value,return NULL */` |
|      ! 0 |  8946 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8947 | `			}else{` |
|      ! 0 |  8948 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8949 | `			}` |
|      ! 0 |  8950 | `			break;` |
|      ! 0 |  8951 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 |  8952 | `			pComp = &sURI.sFragment;` |
|      ! 0 |  8953 | `			if( pComp->nByte < 1 ){` |
|        - |  8954 | `				/* No available value,return NULL */` |
|      ! 0 |  8955 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8956 | `			}else{` |
|      ! 0 |  8957 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8958 | `			}` |
|      ! 0 |  8959 | `			break;` |
|      ! 0 |  8960 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 |  8961 | `			pComp = &sURI.sPath;` |
|      ! 0 |  8962 | `			if( pComp->nByte < 1 ){` |
|        - |  8963 | `				/* No available value,return NULL */` |
|      ! 0 |  8964 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8965 | `			}else{` |
|      ! 0 |  8966 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8967 | `			}` |
|      ! 0 |  8968 | `			break;` |
|      ! 0 |  8969 | `		default:` |
|        - |  8970 | `			/* No such entry,return NULL */` |
|      ! 0 |  8971 | `			ph7_result_null(pCtx);` |
|      ! 0 |  8972 | `			break;` |
|        - |  8973 | `		}` |
|      ! 0 |  8974 | `	}else{` |
|        - |  8975 | `		ph7_value *pArray,*pValue;` |
|        - |  8976 | `		/* Return an associative array */` |
|       27 |  8977 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 |  8978 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 |  8979 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8980 | `			/* Out of memory */` |
|      ! 0 |  8981 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  8982 | `			/* Return false */` |
|      ! 0 |  8983 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |  8984 | `			return PH7_OK;` |
|        - |  8985 | `		}` |
|        - |  8986 | `		/* Fill the array */` |
|       27 |  8987 | `		pComp = &sURI.sScheme;` |
|       27 |  8988 | `		if( pComp->nByte > 0 ){` |
|       19 |  8989 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 |  8990 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 |  8991 | `		}` |
|        - |  8992 | `		/* Reset the string cursor */` |
|       27 |  8993 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8994 | `		pComp = &sURI.sHost;` |
|       27 |  8995 | `		if( pComp->nByte > 0 ){` |
|       25 |  8996 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 |  8997 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 |  8998 | `		}` |
|        - |  8999 | `		/* Reset the string cursor */` |
|       27 |  9000 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9001 | `		pComp = &sURI.sPort;` |
|       27 |  9002 | `		if( pComp->nByte > 0 ){` |
|       11 |  9003 | `			int iPort = 0;/* cc warning */` |
|        - |  9004 | `			/* Convert to integer */` |
|       11 |  9005 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 |  9006 | `			ph7_value_int(pValue,iPort);` |
|       11 |  9007 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 |  9008 | `		}` |
|        - |  9009 | `		/* Reset the string cursor */` |
|       27 |  9010 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9011 | `		pComp = &sURI.sUser;` |
|       27 |  9012 | `		if( pComp->nByte > 0 ){` |
|        7 |  9013 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9014 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 |  9015 | `		}` |
|        - |  9016 | `		/* Reset the string cursor */` |
|       27 |  9017 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9018 | `		pComp = &sURI.sPass;` |
|       27 |  9019 | `		if( pComp->nByte > 0 ){` |
|        7 |  9020 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9021 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 |  9022 | `		}` |
|        - |  9023 | `		/* Reset the string cursor */` |
|       27 |  9024 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9025 | `		pComp = &sURI.sPath;` |
|       27 |  9026 | `		if( pComp->nByte > 0 ){` |
|       17 |  9027 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 |  9028 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 |  9029 | `		}` |
|        - |  9030 | `		/* Reset the string cursor */` |
|       27 |  9031 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9032 | `		pComp = &sURI.sQuery;` |
|       27 |  9033 | `		if( pComp->nByte > 0 ){` |
|        5 |  9034 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9035 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 |  9036 | `		}` |
|        - |  9037 | `		/* Reset the string cursor */` |
|       27 |  9038 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9039 | `		pComp = &sURI.sFragment;` |
|       27 |  9040 | `		if( pComp->nByte > 0 ){` |
|        5 |  9041 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9042 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 |  9043 | `		}` |
|        - |  9044 | `		/* Return the created array */` |
|       27 |  9045 | `		ph7_result_value(pCtx,pArray);` |
|        - |  9046 | `		/* NOTE:` |
|        - |  9047 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - |  9048 | `		 * automatically as soon we return from this function.` |
|        - |  9049 | `		 */` |
|        - |  9050 | `	}` |
|        - |  9051 | `	/* All done */` |
|       27 |  9052 | `	return PH7_OK;` |
|       15 |  9053 |  |
|        - |  9054 | `/*` |
|        - |  9055 | ` * Section:` |
|        - |  9056 | ` *   Array related routines.` |
|        - |  9057 | ` * Status:` |
|        - |  9058 | ` *    Stable.` |
|        - |  9059 | ` * Note 2012-5-21 01:04:15:` |
|        - |  9060 | ` *  Array related functions that need access to the underlying` |
|        - |  9061 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - |  9062 | ` */` |
|        - |  9063 | `/*` |
|        - |  9064 | ` * The [compact()] function store it's state information in an instance` |
|        - |  9065 | ` * of the following structure.` |
|        - |  9066 | ` */` |
|        - |  9067 | `struct compact_data` |
|        - |  9068 |  |
|        - |  9069 | `	ph7_value *pArray;  /* Target array */` |
|        - |  9070 | `	int nRecCount;      /* Recursion count */` |
|        - |  9071 | `};` |
|        - |  9072 | `/*` |
|        - |  9073 | ` * Walker callback for the [compact()] function defined below.` |
|        - |  9074 | ` */` |
|      ! 0 |  9075 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 |  9076 |  |
|      ! 0 |  9077 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 |  9078 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 |  9079 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9080 | `	/* Act according to the hashmap value */` |
|      ! 0 |  9081 | `	if( ph7_value_is_string(pValue) ){` |
|        - |  9082 | `		SyString sVar;` |
|      ! 0 |  9083 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 |  9084 | `		if( sVar.nByte > 0 ){` |
|        - |  9085 | `			/* Query the current frame */` |
|      ! 0 |  9086 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - |  9087 | `			/* ^` |
|        - |  9088 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - |  9089 | `			 */` |
|      ! 0 |  9090 | `			if( pKey ){` |
|        - |  9091 | `				/* Perform the insertion */` |
|      ! 0 |  9092 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 |  9093 | `			}` |
|      ! 0 |  9094 | `		}` |
|      ! 0 |  9095 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - |  9096 | `		int rc;` |
|        - |  9097 | `		/* Recursively traverse this array */` |
|      ! 0 |  9098 | `		pData->nRecCount++;` |
|      ! 0 |  9099 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 |  9100 | `		pData->nRecCount--;` |
|      ! 0 |  9101 | `		return rc;` |
|        - |  9102 | `	}` |
|      ! 0 |  9103 | `	return SXRET_OK;` |
|      ! 0 |  9104 |  |
|        - |  9105 | `/*` |
|        - |  9106 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - |  9107 | ` *  Create array containing variables and their values.` |
|        - |  9108 | ` *  For each of these, compact() looks for a variable with that name` |
|        - |  9109 | ` *  in the current symbol table and adds it to the output array such` |
|        - |  9110 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - |  9111 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - |  9112 | ` *  Any strings that are not set will simply be skipped.` |
|        - |  9113 | ` * Parameters` |
|        - |  9114 | ` *  $varname` |
|        - |  9115 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - |  9116 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - |  9117 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - |  9118 | ` *   it recursively.` |
|        - |  9119 | ` * Return` |
|        - |  9120 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - |  9121 | ` */` |
|        2 |  9122 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9123 |  |
|        - |  9124 | `	ph7_value *pArray,*pObj;` |
|        3 |  9125 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9126 | `	const char *zName;` |
|        - |  9127 | `	SyString sVar;` |
|        - |  9128 | `	int i,nLen;` |
|        3 |  9129 | `	if( nArg < 1 ){` |
|        - |  9130 | `		/* Missing arguments,return NULL */` |
|      ! 0 |  9131 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9132 | `		return PH7_OK;` |
|        - |  9133 | `	}` |
|        - |  9134 | `	/* Create the array */` |
|        3 |  9135 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9136 | `	if( pArray == 0 ){` |
|        - |  9137 | `		/* Out of memory */` |
|      ! 0 |  9138 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9139 | `		/* Return NULL */` |
|      ! 0 |  9140 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9141 | `		return PH7_OK;` |
|        - |  9142 | `	}` |
|        - |  9143 | `	/* Perform the requested operation */` |
|        7 |  9144 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 |  9145 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 |  9146 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - |  9147 | `				struct compact_data sData;` |
|      ! 0 |  9148 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - |  9149 | `				/* Recursively walk the array */` |
|      ! 0 |  9150 | `				sData.nRecCount = 0;` |
|      ! 0 |  9151 | `				sData.pArray = pArray;` |
|      ! 0 |  9152 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 |  9153 | `			}` |
|      ! 0 |  9154 | `		}else{` |
|        - |  9155 | `			/* Extract variable name */` |
|        5 |  9156 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 |  9157 | `			if( nLen > 0 ){` |
|        5 |  9158 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - |  9159 | `				/* Check if the variable is available in the current frame */` |
|        5 |  9160 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 |  9161 | `				if( pObj ){` |
|        5 |  9162 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 |  9163 | `				}` |
|        2 |  9164 | `			}` |
|        - |  9165 | `		}` |
|        3 |  9166 | `	}` |
|        - |  9167 | `	/* Return the array */` |
|        3 |  9168 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9169 | `	return PH7_OK;` |
|        2 |  9170 |  |
|        - |  9171 | `/*` |
|        - |  9172 | ` * The [extract()] function store it's state information in an instance` |
|        - |  9173 | ` * of the following structure.` |
|        - |  9174 | ` */` |
|        - |  9175 | `typedef struct extract_aux_data extract_aux_data;` |
|        - |  9176 | `struct extract_aux_data` |
|        - |  9177 |  |
|        - |  9178 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - |  9179 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - |  9180 | `	const char *zPrefix;  /* Prefix name */` |
|        - |  9181 | `	int Prefixlen;        /* Prefix  length */` |
|        - |  9182 | `	int iFlags;           /* Control flags */` |
|        - |  9183 | `	char zWorker[1024];   /* Working buffer */` |
|        - |  9184 | `};` |
|        - |  9185 | `/* Forward declaration */` |
|        - |  9186 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - |  9187 | `/*` |
|        - |  9188 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - |  9189 | ` *   Import variables into the current symbol table from an array.` |
|        - |  9190 | ` * Parameters` |
|        - |  9191 | ` * $var_array` |
|        - |  9192 | ` *  An associative array. This function treats keys as variable names and values` |
|        - |  9193 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - |  9194 | ` *  table, subject to extract_type and prefix parameters.` |
|        - |  9195 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - |  9196 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - |  9197 | ` * $extract_type` |
|        - |  9198 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - |  9199 | ` *  It can be one of the following values:` |
|        - |  9200 | ` *   EXTR_OVERWRITE` |
|        - |  9201 | ` *       If there is a collision, overwrite the existing variable.` |
|        - |  9202 | ` *   EXTR_SKIP` |
|        - |  9203 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - |  9204 | ` *   EXTR_PREFIX_SAME` |
|        - |  9205 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - |  9206 | ` *   EXTR_PREFIX_ALL` |
|        - |  9207 | ` *       Prefix all variable names with prefix.` |
|        - |  9208 | ` *   EXTR_PREFIX_INVALID` |
|        - |  9209 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - |  9210 | ` *   EXTR_IF_EXISTS` |
|        - |  9211 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - |  9212 | ` *       otherwise do nothing.` |
|        - |  9213 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - |  9214 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - |  9215 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - |  9216 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - |  9217 | ` *      the current symbol table.` |
|        - |  9218 | ` * $prefix` |
|        - |  9219 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - |  9220 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - |  9221 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - |  9222 | ` *  underscore character.` |
|        - |  9223 | ` * Return` |
|        - |  9224 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - |  9225 | ` */` |
|        4 |  9226 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9227 |  |
|        - |  9228 | `	extract_aux_data sAux;` |
|        - |  9229 | `	ph7_hashmap *pMap;` |
|        5 |  9230 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - |  9231 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 |  9232 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9233 | `		return PH7_OK;` |
|        - |  9234 | `	}` |
|        - |  9235 | `	/* Point to the target hashmap */` |
|        5 |  9236 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 |  9237 | `	if( pMap->nEntry < 1 ){` |
|        - |  9238 | `		/* Empty map,return  0 */` |
|      ! 0 |  9239 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9240 | `		return PH7_OK;` |
|        - |  9241 | `	}` |
|        - |  9242 | `	/* Prepare the aux data */` |
|        5 |  9243 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 |  9244 | `	if( nArg > 1 ){` |
|        3 |  9245 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 |  9246 | `		if( nArg > 2 ){` |
|      ! 0 |  9247 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 |  9248 | `		}` |
|        1 |  9249 | `	}` |
|        5 |  9250 | `	sAux.pVm = pCtx->pVm;` |
|        - |  9251 | `	/* Invoke the worker callback */` |
|        5 |  9252 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - |  9253 | `	/* Number of variables successfully imported */` |
|        5 |  9254 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 |  9255 | `	return PH7_OK;` |
|        3 |  9256 |  |
|        - |  9257 | `/*` |
|        - |  9258 | ` * Worker callback for the [extract()] function defined` |
|        - |  9259 | ` * below.` |
|        - |  9260 | ` */` |
|        8 |  9261 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9262 |  |
|        9 |  9263 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 |  9264 | `	int iFlags = pAux->iFlags;` |
|        9 |  9265 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9266 | `	ph7_value *pObj;` |
|        - |  9267 | `	SyString sVar;` |
|        9 |  9268 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 |  9269 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 |  9270 | `	}` |
|        - |  9271 | `	/* Perform a string cast */` |
|        9 |  9272 | `	PH7_MemObjToString(pKey);` |
|        9 |  9273 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9274 | `		/* Unavailable variable name */` |
|      ! 0 |  9275 | `		return SXRET_OK;` |
|        - |  9276 | `	}` |
|        9 |  9277 | `	sVar.nByte = 0; /* cc warning */` |
|        9 |  9278 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 |  9279 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9280 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9281 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9282 | `			);` |
|      ! 0 |  9283 | `	}else{` |
|       13 |  9284 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 |  9285 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9286 | `	}` |
|        9 |  9287 | `	sVar.zString = pAux->zWorker;` |
|        - |  9288 | `	/* Try to extract the variable */` |
|        9 |  9289 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 |  9290 | `	if( pObj ){` |
|        - |  9291 | `		/* Collision */` |
|        5 |  9292 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 |  9293 | `			return SXRET_OK;` |
|        - |  9294 | `		}` |
|        5 |  9295 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 |  9296 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - |  9297 | `				/* Already prefixed */` |
|      ! 0 |  9298 | `				return SXRET_OK;` |
|        - |  9299 | `			}` |
|      ! 0 |  9300 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9301 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9302 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9303 | `				);` |
|      ! 0 |  9304 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 |  9305 | `		}` |
|        3 |  9306 | `	}else{` |
|        - |  9307 | `		/* Create the variable */` |
|        5 |  9308 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - |  9309 | `	}` |
|        9 |  9310 | `	if( pObj ){` |
|        - |  9311 | `		/* Overwrite the old value */` |
|        9 |  9312 | `		PH7_MemObjStore(pValue,pObj);` |
|        - |  9313 | `		/* Increment counter */` |
|        9 |  9314 | `		pAux->iCount++;` |
|        4 |  9315 | `	}` |
|        9 |  9316 | `	return SXRET_OK;` |
|        5 |  9317 |  |
|        - |  9318 | `/*` |
|        - |  9319 | ` * Worker callback for the [import_request_variables()] function` |
|        - |  9320 | ` * defined below.` |
|        - |  9321 | ` */` |
|        2 |  9322 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9323 |  |
|        3 |  9324 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 |  9325 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9326 | `	ph7_value *pObj;` |
|        - |  9327 | `	SyString sVar;` |
|        - |  9328 | `	/* Perform a string cast */` |
|        3 |  9329 | `	PH7_MemObjToString(pKey);` |
|        3 |  9330 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9331 | `		/* Unavailable variable name */` |
|      ! 0 |  9332 | `		return SXRET_OK;` |
|        - |  9333 | `	}` |
|        3 |  9334 | `	sVar.nByte = 0; /* cc warning */` |
|        3 |  9335 | `	if( pAux->Prefixlen > 0 ){` |
|        4 |  9336 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 |  9337 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 |  9338 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9339 | `			);` |
|        2 |  9340 | `	}else{` |
|      ! 0 |  9341 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 |  9342 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9343 | `	}` |
|        3 |  9344 | `	sVar.zString = pAux->zWorker;` |
|        - |  9345 | `	/* Extract the variable */` |
|        3 |  9346 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 |  9347 | `	if( pObj ){` |
|        3 |  9348 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 |  9349 | `	}` |
|        3 |  9350 | `	return SXRET_OK;` |
|        2 |  9351 |  |
|        - |  9352 | `/*` |
|        - |  9353 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - |  9354 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - |  9355 | ` * Parameters` |
|        - |  9356 | ` * $types` |
|        - |  9357 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - |  9358 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - |  9359 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - |  9360 | ` *  POST includes the POST uploaded file information.` |
|        - |  9361 | ` *  Note:` |
|        - |  9362 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - |  9363 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - |  9364 | ` * $prefix` |
|        - |  9365 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - |  9366 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - |  9367 | ` *  variable named $pref_userid.` |
|        - |  9368 | ` * Return` |
|        - |  9369 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9370 | ` */` |
|        2 |  9371 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9372 |  |
|        - |  9373 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - |  9374 | `	extract_aux_data sAux;` |
|        - |  9375 | `	int nLen,nPrefixLen;` |
|        - |  9376 | `	ph7_value *pSuper;` |
|        - |  9377 | `	ph7_vm *pVm;` |
|        - |  9378 | `	/* By default import only $_GET variables  */` |
|        3 |  9379 | `	zImport = "G";` |
|        3 |  9380 | `	nLen = (int)sizeof(char);` |
|        3 |  9381 | `	zPrefix = 0;` |
|        3 |  9382 | `	nPrefixLen = 0;` |
|        3 |  9383 | `	if( nArg > 0 ){` |
|        3 |  9384 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 |  9385 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 |  9386 | `		}` |
|        3 |  9387 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9388 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 |  9389 | `		}` |
|        1 |  9390 | `	}` |
|        - |  9391 | `	/* Point to the underlying VM */` |
|        3 |  9392 | `	pVm = pCtx->pVm;` |
|        - |  9393 | `	/* Initialize the aux data */` |
|        3 |  9394 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 |  9395 | `	sAux.zPrefix = zPrefix;` |
|        3 |  9396 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 |  9397 | `	sAux.pVm = pVm;` |
|        - |  9398 | `	/* Extract */` |
|        3 |  9399 | `	zEnd = &zImport[nLen];` |
|        5 |  9400 | `	while( zImport < zEnd ){` |
|        3 |  9401 | `		int c = zImport[0];` |
|        3 |  9402 | `		pSuper = 0;` |
|        3 |  9403 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - |  9404 | `			/* Import $_GET variables */` |
|        3 |  9405 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 |  9406 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - |  9407 | `			/* Import $_POST variables */` |
|      ! 0 |  9408 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 |  9409 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - |  9410 | `			/* Import $_COOKIE variables */` |
|      ! 0 |  9411 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 |  9412 | `		}` |
|        3 |  9413 | `		if( pSuper ){` |
|        - |  9414 | `			/* Iterate throw array entries */` |
|        3 |  9415 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 |  9416 | `		}` |
|        - |  9417 | `		/* Advance the cursor */` |
|        3 |  9418 | `		zImport++;` |
|        1 |  9419 | `	}` |
|        - |  9420 | `	/* All done,return TRUE*/` |
|        3 |  9421 | `	ph7_result_bool(pCtx,0);` |
|        3 |  9422 | `	return PH7_OK;` |
|        1 |  9423 |  |
|        - |  9424 | `/*` |
|        - |  9425 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |  9426 | ` * Refer to the eval() language construct implementation for more` |
|        - |  9427 | ` * information.` |
|        - |  9428 | ` */` |
|     9778 |  9429 | `static sxi32 VmEvalChunk(` |
|        - |  9430 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |  9431 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |  9432 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |  9433 | `	int iFlags,         /* Compile flag */` |
|        - |  9434 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |  9435 | `	)` |
|        2 |  9436 |  |
|        - |  9437 | `	SySet *pByteCode,aByteCode;` |
|     9780 |  9438 | `	ProcConsumer xErr = 0;` |
|     9780 |  9439 | `	void *pErrData = 0;` |
|        - |  9440 | `	/* Initialize bytecode container */` |
|     9780 |  9441 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     9780 |  9442 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |  9443 | `	/* Reset the code generator */` |
|     9780 |  9444 | `	if( bTrueReturn ){` |
|        - |  9445 | `		/* Included file,log compile-time errors */` |
|     7537 |  9446 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7537 |  9447 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3768 |  9448 | `	}` |
|     9780 |  9449 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |  9450 | `	/* Swap bytecode container */` |
|     9780 |  9451 | `	pByteCode = pVm->pByteContainer;` |
|     9780 |  9452 | `	pVm->pByteContainer = &aByteCode;` |
|        - |  9453 | `	/* Compile the chunk */` |
|     9780 |  9454 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    14669 |  9455 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |  9456 | `		/* Compilation error,return false */` |
|        3 |  9457 | `		if( pCtx ){` |
|        3 |  9458 | `			ph7_result_bool(pCtx,0);` |
|        1 |  9459 | `		}` |
|        2 |  9460 | `	}else{` |
|        - |  9461 | `		/* Mount any newly defined classes */` |
|        - |  9462 | `		SyHashEntry *pEntry;` |
|        - |  9463 | `		ph7_class *pClass;` |
|        - |  9464 | `		ph7_value sResult; /* Return value */` |
|        - |  9465 | `		sxi32 rc;` |
|     9778 |  9466 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   271918 |  9467 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   257254 |  9468 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  9469 | `			/* Only mount classes that haven't been mounted yet */` |
|   257254 |  9470 | `			if( !pClass->bMounted ){` |
|    58826 |  9471 | `				rc = VmMountUserClass(pVm,pClass);` |
|    58826 |  9472 | `				if( rc != SXRET_OK ){` |
|        - |  9473 | `					/* Mount failure (likely memory error) */` |
|      ! 0 |  9474 | `					if( pCtx ){` |
|      ! 0 |  9475 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 |  9476 | `					}` |
|      ! 0 |  9477 | `					goto Cleanup;` |
|        - |  9478 | `				}` |
|    29412 |  9479 | `			}` |
|        2 |  9480 | `		}` |
|     9778 |  9481 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  9482 | `			/* Out of memory */` |
|      ! 0 |  9483 | `			if( pCtx ){` |
|      ! 0 |  9484 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  9485 | `			}` |
|      ! 0 |  9486 | `			goto Cleanup;` |
|        - |  9487 | `		}` |
|     9778 |  9488 | `		if( bTrueReturn ){` |
|        - |  9489 | `			/* Assume a boolean true return value */` |
|     7537 |  9490 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3769 |  9491 | `		}else{` |
|        - |  9492 | `			/* Assume a null return value */` |
|     2242 |  9493 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  9494 | `		}` |
|        - |  9495 | `		/* Execute the compiled chunk */` |
|     9778 |  9496 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|     9778 |  9497 | `		if( pCtx ){` |
|        - |  9498 | `			/* Set the execution result */` |
|     7550 |  9499 | `			ph7_result_value(pCtx,&sResult);` |
|     3774 |  9500 | `		}` |
|     9778 |  9501 | `		PH7_MemObjRelease(&sResult);` |
|        - |  9502 | `	}` |
|     4889 |  9503 | `Cleanup:` |
|        - |  9504 | `	/* Cleanup the mess left behind */` |
|     9780 |  9505 | `	pVm->pByteContainer = pByteCode;` |
|     9780 |  9506 | `	SySetRelease(&aByteCode);` |
|     9780 |  9507 | `	return SXRET_OK;` |
|        2 |  9508 |  |
|        - |  9509 | `/*` |
|        - |  9510 | ` * value eval(string $code)` |
|        - |  9511 | ` *   Evaluate a string as PHP code.` |
|        - |  9512 | ` * Parameter` |
|        - |  9513 | ` *  code: PHP code to evaluate.` |
|        - |  9514 | ` * Return` |
|        - |  9515 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  9516 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  9517 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  9518 | ` */` |
|       16 |  9519 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9520 |  |
|        - |  9521 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 |  9522 | `	if( nArg < 1 ){` |
|        - |  9523 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9524 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9525 | `		return SXRET_OK;` |
|        - |  9526 | `	}` |
|        - |  9527 | `	/* Chunk to evaluate */` |
|       18 |  9528 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 |  9529 | `	if( sChunk.nByte < 1 ){` |
|        - |  9530 | `		/* Empty string,return NULL */` |
|        3 |  9531 | `		ph7_result_null(pCtx);` |
|        3 |  9532 | `		return SXRET_OK;` |
|        - |  9533 | `	}` |
|        - |  9534 | `	/* Eval the chunk */` |
|       16 |  9535 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 |  9536 | `	return SXRET_OK;` |
|       10 |  9537 |  |
|        - |  9538 | `/*` |
|        - |  9539 | ` * Check if a file path is already included.` |
|        - |  9540 | ` */` |
|    15068 |  9541 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 |  9542 |  |
|        - |  9543 | `	SyString *aEntries;` |
|        - |  9544 | `	sxu32 n;` |
|    15069 |  9545 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  9546 | `	/* Perform a linear search */` |
| 56750809 |  9547 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 56735747 |  9548 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  9549 | `			/* Already included */` |
|        7 |  9550 | `			return TRUE;` |
|        - |  9551 | `		}` |
| 28367871 |  9552 | `	}` |
|    15063 |  9553 | `	return FALSE;` |
|     7535 |  9554 |  |
|        - |  9555 | `/*` |
|        - |  9556 | ` * Push a file path in the appropriate VM container.` |
|        - |  9557 | ` */` |
|    17288 |  9558 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 |  9559 |  |
|        - |  9560 | `	SyString sPath;` |
|        - |  9561 | `	char *zDup;` |
|        - |  9562 | `#ifdef __WINNT__` |
|        - |  9563 | `	char *zCur;` |
|        - |  9564 | `#endif` |
|        - |  9565 | `	sxi32 rc;` |
|    17290 |  9566 | `	if( nLen < 0 ){` |
|     2222 |  9567 | `		nLen = SyStrlen(zPath);` |
|     1110 |  9568 | `	}` |
|        - |  9569 | `	/* Duplicate the file path first */` |
|    17290 |  9570 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    17290 |  9571 | `	if( zDup == 0 ){` |
|      ! 0 |  9572 | `		return SXERR_MEM;` |
|        - |  9573 | `	}` |
|        - |  9574 | `#ifdef __WINNT__` |
|        - |  9575 | `	/* Normalize path on windows` |
|        - |  9576 | `	 * Example:` |
|        - |  9577 | `	 *    Path/To/File.php` |
|        - |  9578 | `	 * becomes` |
|        - |  9579 | `	 *   path\to\file.php` |
|        - |  9580 | `	 */` |
|        2 |  9581 | `	zCur = zDup;` |
|        2 |  9582 | `	while( zCur[0] != 0 ){` |
|        2 |  9583 | `		if( zCur[0] == '/' ){` |
|        2 |  9584 | `			zCur[0] = '\\';` |
|        2 |  9585 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 |  9586 | `			int c = SyToLower(zCur[0]);` |
|        1 |  9587 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - |  9588 | `		}` |
|        2 |  9589 | `		zCur++;` |
|        2 |  9590 | `	}` |
|        - |  9591 | `#endif` |
|        - |  9592 | `	/* Install the file path */` |
|    17290 |  9593 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    17290 |  9594 | `	if( !bMain ){` |
|    15069 |  9595 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  9596 | `			/* Already included */` |
|        7 |  9597 | `			*pNew = 0;` |
|        4 |  9598 | `		}else{` |
|        - |  9599 | `			/* Insert in the corresponding container */` |
|    15063 |  9600 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    15063 |  9601 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9602 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  9603 | `				return rc;` |
|        - |  9604 | `			}` |
|    15063 |  9605 | `			*pNew = 1;` |
|        - |  9606 | `		}` |
|     7534 |  9607 | `	}` |
|    17290 |  9608 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    17290 |  9609 | `	return SXRET_OK;` |
|     8646 |  9610 |  |
|        - |  9611 | `/*` |
|        - |  9612 | ` * Compile and Execute a PHP script at run-time.` |
|        - |  9613 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - |  9614 | ` * indicates failure.` |
|        - |  9615 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - |  9616 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - |  9617 | ` * operations.` |
|        - |  9618 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - |  9619 | ` * this function is a no-op.` |
|        - |  9620 | ` * Refer to the implementation of the include(),include_once() language` |
|        - |  9621 | ` * constructs for more information.` |
|        - |  9622 | ` */` |
|     7542 |  9623 | `static sxi32 VmExecIncludedFile(` |
|        - |  9624 | `	 ph7_context *pCtx, /* Call Context */` |
|        - |  9625 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - |  9626 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - |  9627 | `	 )` |
|        2 |  9628 |  |
|        - |  9629 | `	sxi32 rc;` |
|        - |  9630 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9631 | `	const ph7_io_stream *pStream;` |
|        - |  9632 | `	SyBlob sContents;` |
|        - |  9633 | `	void *pHandle;` |
|        - |  9634 | `	ph7_vm *pVm;` |
|        - |  9635 | `	int isNew;` |
|        - |  9636 | `	/* Initialize fields */` |
|     7544 |  9637 | `	pVm = pCtx->pVm;` |
|     7544 |  9638 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7544 |  9639 | `	isNew = 0;` |
|        - |  9640 | `	/* Extract the associated stream */` |
|     7544 |  9641 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - |  9642 | `	/*` |
|        - |  9643 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - |  9644 | `	 * in a read-only mode.` |
|        - |  9645 | `	 */` |
|     7544 |  9646 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7544 |  9647 | `	if( pHandle == 0 ){` |
|        3 |  9648 | `		return SXERR_IO;` |
|        - |  9649 | `	}` |
|     7541 |  9650 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7541 |  9651 | `	if( IncludeOnce && !isNew ){` |
|        - |  9652 | `		/* Already included */` |
|        5 |  9653 | `		rc = SXERR_EXISTS;` |
|        3 |  9654 | `	}else{` |
|        - |  9655 | `		/* Read the whole file contents */` |
|     7537 |  9656 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7537 |  9657 | `		if( rc == SXRET_OK ){` |
|        - |  9658 | `			SyString sScript;` |
|        - |  9659 | `			/* Compile and execute the script */` |
|     7537 |  9660 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7537 |  9661 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3768 |  9662 | `		}` |
|        - |  9663 | `	}` |
|        - |  9664 | `	/* Pop from the set of included file */` |
|     7541 |  9665 | `	(void)SySetPop(&pVm->aFiles);` |
|        - |  9666 | `	/* Close the handle */` |
|     7541 |  9667 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - |  9668 | `	/* Release the working buffer */` |
|     7541 |  9669 | `	SyBlobRelease(&sContents);` |
|        - |  9670 | `#else` |
|        - |  9671 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - |  9672 | `	SXUNUSED(pPath);` |
|        - |  9673 | `	SXUNUSED(IncludeOnce);` |
|        - |  9674 | `	rc = SXERR_IO;` |
|        - |  9675 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7541 |  9676 | `	return rc;` |
|     3773 |  9677 |  |
|        - |  9678 | `/*` |
|        - |  9679 | ` * string get_include_path(void)` |
|        - |  9680 | ` *  Gets the current include_path configuration option.` |
|        - |  9681 | ` * Parameter` |
|        - |  9682 | ` *  None` |
|        - |  9683 | ` * Return` |
|        - |  9684 | ` *  Included paths as a string` |
|        - |  9685 | ` */` |
|        2 |  9686 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9687 |  |
|        3 |  9688 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9689 | `	SyString *aEntry;` |
|        - |  9690 | `	int dir_sep;` |
|        - |  9691 | `	sxu32 n;` |
|        - |  9692 | `#ifdef __WINNT__` |
|        1 |  9693 | `	dir_sep = ';';` |
|        - |  9694 | `#else` |
|        - |  9695 | `	/* Assume UNIX path separator */` |
|        2 |  9696 | `	dir_sep = ':';` |
|        - |  9697 | `#endif` |
|        1 |  9698 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9699 | `	SXUNUSED(apArg);` |
|        - |  9700 | `	/* Point to the list of import paths */` |
|        3 |  9701 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 |  9702 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 |  9703 | `		SyString *pEntry = &aEntry[n];` |
|        3 |  9704 | `		if( n > 0 ){` |
|        - |  9705 | `			/* Append dir seprator */` |
|      ! 0 |  9706 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 |  9707 | `		}` |
|        - |  9708 | `		/* Append path */` |
|        3 |  9709 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 |  9710 | `	}` |
|        3 |  9711 | `	return PH7_OK;` |
|        1 |  9712 |  |
|        - |  9713 | `/*` |
|        - |  9714 | ` * string get_get_included_files(void)` |
|        - |  9715 | ` *  Gets the current include_path configuration option.` |
|        - |  9716 | ` * Parameter` |
|        - |  9717 | ` *  None` |
|        - |  9718 | ` * Return` |
|        - |  9719 | ` *  Included paths as a string` |
|        - |  9720 | ` */` |
|        2 |  9721 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9722 |  |
|        3 |  9723 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - |  9724 | `	ph7_value *pArray,*pWorker;` |
|        - |  9725 | `	SyString *pEntry;` |
|        - |  9726 | `	int c,d;` |
|        - |  9727 | `	/* Create an array and a working value */` |
|        3 |  9728 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 |  9729 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 |  9730 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - |  9731 | `		/* Out of memory,return null */` |
|      ! 0 |  9732 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9733 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9734 | `		SXUNUSED(apArg);` |
|      ! 0 |  9735 | `		return PH7_OK;` |
|        - |  9736 | `	}` |
|        3 |  9737 | `	c = d = '/';` |
|        - |  9738 | `#ifdef __WINNT__` |
|        1 |  9739 | `	d = '\\';` |
|        - |  9740 | `#endif` |
|        - |  9741 | `	/* Iterate throw entries */` |
|        3 |  9742 | `	SySetResetCursor(pFiles);` |
|     3697 |  9743 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - |  9744 | `		const char *zBase,*zEnd;` |
|        - |  9745 | `		int iLen;` |
|        - |  9746 | `		/* reset the string cursor */` |
|     3695 |  9747 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - |  9748 | `		/* Extract base name */` |
|     3695 |  9749 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - |  9750 | `		/* Ignore trailing '/' */` |
|     5542 |  9751 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 |  9752 | `			zEnd--;` |
|      ! 0 |  9753 | `		}` |
|     3695 |  9754 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   114014 |  9755 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   108473 |  9756 | `			zEnd--;` |
|        1 |  9757 | `		}` |
|     3695 |  9758 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3695 |  9759 | `		zEnd = &pEntry->zString[iLen];` |
|        - |  9760 | `		/* Copy entry name */` |
|     3695 |  9761 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - |  9762 | `		/* Perform the insertion */` |
|     3695 |  9763 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 |  9764 | `	}` |
|        - |  9765 | `	/* All done,return the created array */` |
|        3 |  9766 | `	ph7_result_value(pCtx,pArray);` |
|        - |  9767 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - |  9768 | `	 * by the engine as soon we return from this foreign` |
|        - |  9769 | `	 * function.` |
|        - |  9770 | `	 */` |
|        3 |  9771 | `	return PH7_OK;` |
|        2 |  9772 |  |
|        - |  9773 | `/*` |
|        - |  9774 | ` * include:` |
|        - |  9775 | ` * According to the PHP reference manual.` |
|        - |  9776 | ` *  The include() function includes and evaluates the specified file.` |
|        - |  9777 | ` *  Files are included based on the file path given or, if none is given` |
|        - |  9778 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - |  9779 | ` *  include() will finally check in the calling script's own directory` |
|        - |  9780 | ` *  and the current working directory before failing. The include()` |
|        - |  9781 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - |  9782 | ` *  behavior from require(), which will emit a fatal error.` |
|        - |  9783 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - |  9784 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - |  9785 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - |  9786 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - |  9787 | ` *  directory to find the requested file.` |
|        - |  9788 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - |  9789 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - |  9790 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - |  9791 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - |  9792 | ` */` |
|     7530 |  9793 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9794 |  |
|        - |  9795 | `	SyString sFile;` |
|        - |  9796 | `	sxi32 rc;` |
|     7532 |  9797 | `	if( nArg < 1 ){` |
|        - |  9798 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9799 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9800 | `		return SXRET_OK;` |
|        - |  9801 | `	}` |
|        - |  9802 | `	/* File to include */` |
|     7532 |  9803 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7532 |  9804 | `	if( sFile.nByte < 1 ){` |
|        - |  9805 | `		/* Empty string,return NULL */` |
|      ! 0 |  9806 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9807 | `		return SXRET_OK;` |
|        - |  9808 | `	}` |
|        - |  9809 | `	/* Open,compile and execute the desired script */` |
|     7532 |  9810 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7532 |  9811 | `	if( rc != SXRET_OK ){` |
|        - |  9812 | `		/* Emit a warning and return false */` |
|        3 |  9813 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 |  9814 | `		ph7_result_bool(pCtx,0);` |
|        1 |  9815 | `	}` |
|     7532 |  9816 | `	return SXRET_OK;` |
|     3767 |  9817 |  |
|        - |  9818 | `/*` |
|        - |  9819 | ` * include_once:` |
|        - |  9820 | ` *  According to the PHP reference manual.` |
|        - |  9821 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - |  9822 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - |  9823 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - |  9824 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - |  9825 | ` *   just once.` |
|        - |  9826 | ` */` |
|        4 |  9827 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9828 |  |
|        - |  9829 | `	SyString sFile;` |
|        - |  9830 | `	sxi32 rc;` |
|        5 |  9831 | `	if( nArg < 1 ){` |
|        - |  9832 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9833 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9834 | `		return SXRET_OK;` |
|        - |  9835 | `	}` |
|        - |  9836 | `	/* File to include */` |
|        5 |  9837 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 |  9838 | `	if( sFile.nByte < 1 ){` |
|        - |  9839 | `		/* Empty string,return NULL */` |
|      ! 0 |  9840 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9841 | `		return SXRET_OK;` |
|        - |  9842 | `	}` |
|        - |  9843 | `	/* Open,compile and execute the desired script */` |
|        5 |  9844 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 |  9845 | `	if( rc == SXERR_EXISTS ){` |
|        - |  9846 | `		/* File already included,return TRUE */` |
|        3 |  9847 | `		ph7_result_bool(pCtx,1);` |
|        3 |  9848 | `		return SXRET_OK;` |
|        - |  9849 | `	}` |
|        3 |  9850 | `	if( rc != SXRET_OK ){` |
|        - |  9851 | `		/* Emit a warning and return false */` |
|      ! 0 |  9852 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 |  9853 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9854 | ` 	}` |
|        3 |  9855 | `	return SXRET_OK;` |
|        3 |  9856 |  |
|        - |  9857 | `/*` |
|        - |  9858 | ` * require.` |
|        - |  9859 | ` *  According to the PHP reference manual.` |
|        - |  9860 | ` *   require() is identical to include() except upon failure it will` |
|        - |  9861 | ` *   also produce a fatal level error.` |
|        - |  9862 | ` *   In other words, it will halt the script whereas include() only` |
|        - |  9863 | ` *   emits a warning  which allows the script to continue.` |
|        - |  9864 | ` */` |
|        4 |  9865 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9866 |  |
|        - |  9867 | `	SyString sFile;` |
|        - |  9868 | `	sxi32 rc;` |
|        5 |  9869 | `	if( nArg < 1 ){` |
|        - |  9870 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9871 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9872 | `		return SXRET_OK;` |
|        - |  9873 | `	}` |
|        - |  9874 | `	/* File to include */` |
|        5 |  9875 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 |  9876 | `	if( sFile.nByte < 1 ){` |
|        - |  9877 | `		/* Empty string,return NULL */` |
|      ! 0 |  9878 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9879 | `		return SXRET_OK;` |
|        - |  9880 | `	}` |
|        - |  9881 | `	/* Open,compile and execute the desired script */` |
|        5 |  9882 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 |  9883 | `	if( rc != SXRET_OK ){` |
|        - |  9884 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  9885 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  9886 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9887 | `		return PH7_ABORT;` |
|        - |  9888 | `	}` |
|        5 |  9889 | `	return SXRET_OK;` |
|        3 |  9890 |  |
|        - |  9891 | `/*` |
|        - |  9892 | ` * require_once:` |
|        - |  9893 | ` *  According to the PHP reference manual.` |
|        - |  9894 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - |  9895 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - |  9896 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - |  9897 | ` *   and how it differs from its non _once siblings.` |
|        - |  9898 | ` */` |
|        4 |  9899 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9900 |  |
|        - |  9901 | `	SyString sFile;` |
|        - |  9902 | `	sxi32 rc;` |
|        5 |  9903 | `	if( nArg < 1 ){` |
|        - |  9904 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9905 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9906 | `		return SXRET_OK;` |
|        - |  9907 | `	}` |
|        - |  9908 | `	/* File to include */` |
|        5 |  9909 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 |  9910 | `	if( sFile.nByte < 1 ){` |
|        - |  9911 | `		/* Empty string,return NULL */` |
|      ! 0 |  9912 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9913 | `		return SXRET_OK;` |
|        - |  9914 | `	}` |
|        - |  9915 | `	/* Open,compile and execute the desired script */` |
|        5 |  9916 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 |  9917 | `	if( rc == SXERR_EXISTS ){` |
|        - |  9918 | `		/* File already included,return TRUE */` |
|        3 |  9919 | `		ph7_result_bool(pCtx,1);` |
|        3 |  9920 | `		return SXRET_OK;` |
|        - |  9921 | `	}` |
|        3 |  9922 | `	if( rc != SXRET_OK ){` |
|        - |  9923 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  9924 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  9925 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9926 | `		return PH7_ABORT;` |
|        - |  9927 | `	}` |
|        3 |  9928 | `	return SXRET_OK;` |
|        3 |  9929 |  |
|        - |  9930 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - |  9931 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - |  9932 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - |  9933 | `/* Table of built-in VM functions. */` |
|        - |  9934 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - |  9935 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - |  9936 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - |  9937 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - |  9938 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - |  9939 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - |  9940 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - |  9941 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - |  9942 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - |  9943 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - |  9944 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - |  9945 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - |  9946 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - |  9947 | `	    /* Constants management */` |
|        - |  9948 | `	{ "defined",  vm_builtin_defined              },` |
|        - |  9949 | `	{ "define",   vm_builtin_define               },` |
|        - |  9950 | `	{ "constant", vm_builtin_constant             },` |
|        - |  9951 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - |  9952 | `	   /* Class/Object functions */` |
|        - |  9953 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - |  9954 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - |  9955 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - |  9956 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - |  9957 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - |  9958 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - |  9959 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - |  9960 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - |  9961 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - |  9962 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - |  9963 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - |  9964 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - |  9965 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - |  9966 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - |  9967 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - |  9968 | `	{ "is_a", vm_builtin_is_a },` |
|        - |  9969 | `	   /* Random numbers/strings generators */` |
|        - |  9970 | `	{ "rand",          vm_builtin_rand            },` |
|        - |  9971 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - |  9972 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - |  9973 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - |  9974 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - |  9975 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9976 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  9977 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - |  9978 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  9979 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  9980 | `	   /* Language constructs functions */` |
|        - |  9981 | `	{ "echo",  vm_builtin_echo                    },` |
|        - |  9982 | `	{ "print", vm_builtin_print                   },` |
|        - |  9983 | `	{ "exit",  vm_builtin_exit                    },` |
|        - |  9984 | `	{ "die",   vm_builtin_exit                    },` |
|        - |  9985 | `	{ "eval",  vm_builtin_eval                    },` |
|        - |  9986 | `	  /* Variable handling functions */` |
|        - |  9987 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - |  9988 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - |  9989 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - |  9990 | `	{ "isset",     vm_builtin_isset                },` |
|        - |  9991 | `	{ "unset",     vm_builtin_unset                },` |
|        - |  9992 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - |  9993 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - |  9994 | `	{ "var_export",vm_builtin_var_export           },` |
|        - |  9995 | `	  /* Ouput control functions */` |
|        - |  9996 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - |  9997 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - |  9998 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - |  9999 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 10000 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 10001 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 10002 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 10003 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 10004 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 10005 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 10006 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 10007 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 10008 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 10009 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 10010 | `	  /* Assertion functions */` |
|        - | 10011 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 10012 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 10013 | `	  /* Error reporting functions */` |
|        - | 10014 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 10015 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 10016 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 10017 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 10018 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 10019 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 10020 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 10021 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 10022 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 10023 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 10024 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 10025 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 10026 | `	  /* Release info */` |
|        - | 10027 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 10028 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 10029 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 10030 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 10031 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 10032 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 10033 | `	  /* hashmap */` |
|        - | 10034 | `	{"compact",          vm_builtin_compact       },` |
|        - | 10035 | `	{"extract",          vm_builtin_extract       },` |
|        - | 10036 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 10037 | `	  /* URL related function */` |
|        - | 10038 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 10039 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 10040 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10041 | `	   /* XML processing functions */` |
|        - | 10042 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 10043 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 10044 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 10045 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 10046 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 10047 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 10048 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 10049 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 10050 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 10051 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 10052 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 10053 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 10054 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 10055 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 10056 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 10057 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 10058 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 10059 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 10060 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 10061 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 10062 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 10063 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10064 | `	   /* UTF-8 encoding/decoding */` |
|        - | 10065 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 10066 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 10067 | `	   /* Command line processing */` |
|        - | 10068 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 10069 | `	   /* JSON encoding/decoding */` |
|        - | 10070 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 10071 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 10072 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 10073 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 10074 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 10075 | `	   /* Files/URI inclusion facility */` |
|        - | 10076 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 10077 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 10078 | `	{ "include",      vm_builtin_include          },` |
|        - | 10079 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 10080 | `	{ "require",      vm_builtin_require          },` |
|        - | 10081 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 10082 | `};` |
|        - | 10083 | `/*` |
|        - | 10084 | ` * Register the built-in VM functions defined above.` |
|        - | 10085 | ` */` |
|     1990 | 10086 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 10087 |  |
|        - | 10088 | `	sxi32 rc;` |
|        - | 10089 | `	sxu32 n;` |
|   248752 | 10090 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 10091 | `		/* Note that these special functions have access` |
|        - | 10092 | `		 * to the underlying virtual machine as their` |
|        - | 10093 | `		 * private data.` |
|        - | 10094 | `		 */` |
|   246762 | 10095 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   246762 | 10096 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 10097 | `			return rc;` |
|        - | 10098 | `		}` |
|   123382 | 10099 | `	}` |
|     1992 | 10100 | `	return SXRET_OK;` |
|      997 | 10101 |  |
|        - | 10102 | `/*` |
|        - | 10103 | ` * Check if the given name refer to an installed class.` |
|        - | 10104 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 10105 | ` */` |
|    14536 | 10106 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 10107 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 10108 | `	const char *zName,  /* Name of the target class */` |
|        - | 10109 | `	sxu32 nByte,        /* zName length */` |
|        - | 10110 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 10111 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 10112 | `						 */` |
|        - | 10113 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 10114 | `	)` |
|        2 | 10115 |  |
|        - | 10116 | `	SyHashEntry *pEntry;` |
|        - | 10117 | `	ph7_class *pClass;` |
|     7268 | 10118 | `		SXUNUSED(iNest);` |
|        - | 10119 | `	/* Perform a hash lookup */` |
|    14538 | 10120 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|        - | 10121 |  |
|    14538 | 10122 | `	if( pEntry == 0 ){` |
|        - | 10123 | `		/* No such entry,return NULL */` |
|      ! 0 | 10124 | `		return 0;` |
|        - | 10125 | `	}` |
|    14538 | 10126 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    14538 | 10127 | `	if( !iLoadable ){` |
|        - | 10128 | `		/* Return the first class seen */` |
|    13532 | 10129 | `		return pClass;` |
|      ! 0 | 10130 | `	}else{` |
|        - | 10131 | `		/* Check the collision list */` |
|     1008 | 10132 | `		while(pClass){` |
|     1008 | 10133 | `			if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) == 0 ){` |
|        - | 10134 | `				/* Class is loadable */` |
|     1008 | 10135 | `				return pClass;` |
|        - | 10136 | `			}` |
|        - | 10137 | `			/* Point to the next entry */` |
|      ! 0 | 10138 | `			pClass = pClass->pNextName;` |
|      ! 0 | 10139 | `		}` |
|        - | 10140 | `	}` |
|        - | 10141 | `	/* No such loadable class */` |
|      ! 0 | 10142 | `	return 0;` |
|     7270 | 10143 |  |
|        - | 10144 | `/*` |
|        - | 10145 | ` * Reference Table Implementation` |
|        - | 10146 | ` * Status: stable <chm@symisc.net>` |
|        - | 10147 | ` * Intro` |
|        - | 10148 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 10149 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 10150 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 10151 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 10152 | ` *  Refer to the official for more information on this powerful` |
|        - | 10153 | ` *  extension.` |
|        - | 10154 | ` */` |
|        - | 10155 | `/*` |
|        - | 10156 | ` * Allocate a new reference entry.` |
|        - | 10157 | ` */` |
|  2977918 | 10158 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 10159 |  |
|        - | 10160 | `	VmRefObj *pRef;` |
|        - | 10161 | `	/* Allocate a new instance */` |
|  2977920 | 10162 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  2977920 | 10163 | `	if( pRef == 0 ){` |
|      ! 0 | 10164 | `		return 0;` |
|        - | 10165 | `	}` |
|        - | 10166 | `	/* Zero the structure */` |
|  2977920 | 10167 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 10168 | `	/* Initialize fields */` |
|  2977920 | 10169 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  2977920 | 10170 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  2977920 | 10171 | `	pRef->nIdx = nIdx;` |
|  2977920 | 10172 | `	return pRef;` |
|  1488961 | 10173 |  |
|        - | 10174 | `/*` |
|        - | 10175 | ` * Default hash function used by the reference table` |
|        - | 10176 | ` * for lookup/insertion operations.` |
|        - | 10177 | ` */` |
| 16537133 | 10178 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 10179 |  |
|        - | 10180 | `	/* Calculate the hash based on the memory object index */` |
| 16537135 | 10181 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 10182 |  |
|        - | 10183 | `/*` |
|        - | 10184 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 10185 | ` * in the reference table.` |
|        - | 10186 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 10187 | ` * otherwise.` |
|        - | 10188 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10189 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10190 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10191 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10192 | ` * Refer to the official for more information on this powerful` |
|        - | 10193 | ` * extension.` |
|        - | 10194 | ` */` |
|  8893692 | 10195 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 10196 |  |
|        - | 10197 | `	VmRefObj *pRef;` |
|        - | 10198 | `	sxu32 nBucket;` |
|        - | 10199 | `	/* Point to the appropriate bucket */` |
|  8893694 | 10200 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 10201 | `	/* Perform the lookup */` |
|  8893694 | 10202 | `	pRef = pVm->apRefObj[nBucket];` |
| 18782891 | 10203 | `	for(;;){` |
| 37560279 | 10204 | `		if( pRef == 0 ){` |
|  3050030 | 10205 | `			break;` |
|        - | 10206 | `		}` |
| 34510251 | 10207 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 10208 | `			/* Entry found */` |
|  5843666 | 10209 | `			return pRef;` |
|        - | 10210 | `		}` |
|        - | 10211 | `		/* Point to the next entry */` |
| 28666587 | 10212 | `		pRef = pRef->pNextCollide;` |
|        2 | 10213 | `	}` |
|        - | 10214 | `	/* No such entry,return NULL */` |
|  3050030 | 10215 | `	return 0;` |
|  4446848 | 10216 |  |
|        - | 10217 | `/*` |
|        - | 10218 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10219 | ` *` |
|        - | 10220 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10221 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10222 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10223 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10224 | ` * Refer to the official for more information on this powerful` |
|        - | 10225 | ` * extension.` |
|        - | 10226 | ` */` |
|  2977918 | 10227 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10228 |  |
|        - | 10229 | `	sxu32 nBucket;` |
|  2977920 | 10230 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 10231 | `		VmRefObj **apNew;` |
|        - | 10232 | `		sxu32 nNew;` |
|        - | 10233 | `		/* Allocate a larger table */` |
|     3126 | 10234 | `		nNew = pVm->nRefSize << 1;` |
|     3126 | 10235 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     3126 | 10236 | `		if( apNew ){` |
|     3126 | 10237 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 10238 | `			sxu32 n;` |
|        - | 10239 | `			/* Zero the structure */` |
|     3126 | 10240 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 10241 | `			/* Rehash all referenced entries */` |
|  2831480 | 10242 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 10243 | `				/* Remove old collision links */` |
|  2828356 | 10244 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 10245 | `				/* Point to the appropriate bucket */` |
|  2828356 | 10246 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 10247 | `				/* Insert the entry  */` |
|  2828356 | 10248 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2828356 | 10249 | `				if( apNew[nBucket] ){` |
|  2298896 | 10250 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 10251 | `				}` |
|  2828356 | 10252 | `				apNew[nBucket] = pEntry;` |
|        - | 10253 | `				/* Point to the next entry */` |
|  2828356 | 10254 | `				pEntry = pEntry->pNext;` |
|  1414179 | 10255 | `			}` |
|        - | 10256 | `			/* Release the old table */` |
|     3126 | 10257 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 10258 | `			/* Install the new one */` |
|     3126 | 10259 | `			pVm->apRefObj = apNew;` |
|     3126 | 10260 | `			pVm->nRefSize = nNew;` |
|     1562 | 10261 | `		}` |
|     1562 | 10262 | `	}` |
|        - | 10263 | `	/* Point to the appropriate bucket */` |
|  2977920 | 10264 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 10265 | `	/* Insert the entry */` |
|  2977920 | 10266 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  2977920 | 10267 | `	if( pVm->apRefObj[nBucket] ){` |
|  2471171 | 10268 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1235882 | 10269 | `	}` |
|  2977920 | 10270 | `	pVm->apRefObj[nBucket] = pRef;` |
|  2977920 | 10271 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  2977920 | 10272 | `	pVm->nRefUsed++;` |
|  2977920 | 10273 | `	return SXRET_OK;` |
|        2 | 10274 |  |
|        - | 10275 | `/*` |
|        - | 10276 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 10277 | ` * the reference table.` |
|        - | 10278 | ` * This function is invoked when the user perform an unset` |
|        - | 10279 | ` * call [i.e: unset($var); ].` |
|        - | 10280 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10281 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10282 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10283 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10284 | ` * Refer to the official for more information on this powerful` |
|        - | 10285 | ` * extension.` |
|        - | 10286 | ` */` |
|  2949344 | 10287 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10288 |  |
|        - | 10289 | `	ph7_hashmap_node **apNode;` |
|        - | 10290 | `	SyHashEntry **apEntry;` |
|        - | 10291 | `	sxu32 n;` |
|        - | 10292 | `	/* Point to the reference table */` |
|  2949346 | 10293 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2949346 | 10294 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 10295 | `	/* Unlink the entry from the reference table */` |
|  3026412 | 10296 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    77068 | 10297 | `		if( apEntry[n] ){` |
|    77018 | 10298 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    38508 | 10299 | `		}` |
|    38535 | 10300 | `	}` |
|  5823602 | 10301 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2874258 | 10302 | `		if( apNode[n] ){` |
|     5629 | 10303 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2814 | 10304 | `		}` |
|  1437130 | 10305 | `	}` |
|  2949346 | 10306 | `	if( pRef->pPrevCollide ){` |
|  1112177 | 10307 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   556156 | 10308 | `	}else{` |
|  1837171 | 10309 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 10310 | `	}` |
|  2949346 | 10311 | `	if( pRef->pNextCollide ){` |
|  1660184 | 10312 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   830333 | 10313 | `	}` |
|  2949346 | 10314 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 10315 | `	/* Release the node */` |
|  2949346 | 10316 | `	SySetRelease(&pRef->aReference);` |
|  2949346 | 10317 | `	SySetRelease(&pRef->aArrEntries);` |
|  2949346 | 10318 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2949346 | 10319 | `	pVm->nRefUsed--;` |
|  2949346 | 10320 | `	return SXRET_OK;` |
|        2 | 10321 |  |
|        - | 10322 | `/*` |
|        - | 10323 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10324 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10325 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10326 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10327 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10328 | ` * Refer to the official for more information on this powerful` |
|        - | 10329 | ` * extension.` |
|        - | 10330 | ` */` |
|  3003554 | 10331 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 10332 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10333 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10334 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10335 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 10336 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 10337 | `	)` |
|        2 | 10338 |  |
|  3003556 | 10339 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10340 | `	VmRefObj *pRef;` |
|        - | 10341 | `	/* Check if the referenced object already exists */` |
|  3003556 | 10342 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3003556 | 10343 | `	if( pRef == 0 ){` |
|        - | 10344 | `		/* Create a new entry */` |
|  2977920 | 10345 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  2977920 | 10346 | `		if( pRef == 0 ){` |
|      ! 0 | 10347 | `			return SXERR_MEM;` |
|        - | 10348 | `		}` |
|  2977920 | 10349 | `		pRef->iFlags = iFlags;` |
|        - | 10350 | `		/* Install the entry */` |
|  2977920 | 10351 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1488959 | 10352 | `	}` |
|  3079206 | 10353 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 10354 | `		/* Safely ignore the exception frame */` |
|    75652 | 10355 | `		pFrame = pFrame->pParent;` |
|        2 | 10356 | `	}` |
|  3003556 | 10357 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 10358 | `		VmSlot sRef;` |
|        - | 10359 | `		/* Local frame,record referenced entry so that it can` |
|        - | 10360 | `		 * be deleted when we leave this frame.` |
|        - | 10361 | `		 */` |
|    72142 | 10362 | `		sRef.nIdx = nIdx;` |
|    72142 | 10363 | `		sRef.pUserData = pEntry;` |
|    72142 | 10364 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 10365 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 10366 | `		}` |
|    36070 | 10367 | `	}` |
|  3003556 | 10368 | `	if( pEntry ){` |
|        - | 10369 | `		/* Address of the hash-entry */` |
|    97592 | 10370 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    48795 | 10371 | `	}` |
|  3003556 | 10372 | `	if( pMapEntry ){` |
|        - | 10373 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2901286 | 10374 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1450642 | 10375 | `	}` |
|  3003556 | 10376 | `	return SXRET_OK;` |
|  1501779 | 10377 |  |
|        - | 10378 | `/*` |
|        - | 10379 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 10380 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10381 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10382 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10383 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10384 | ` * Refer to the official for more information on this powerful` |
|        - | 10385 | ` * extension.` |
|        - | 10386 | ` */` |
|  2940774 | 10387 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 10388 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10389 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10390 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10391 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 10392 | `	)` |
|        2 | 10393 |  |
|        - | 10394 | `	VmRefObj *pRef;` |
|        - | 10395 | `	sxu32 n;` |
|        - | 10396 | `	/* Check if the referenced object already exists */` |
|  2940776 | 10397 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2940776 | 10398 | `	if( pRef == 0 ){` |
|        - | 10399 | `		/* Not such entry */` |
|    72092 | 10400 | `		return SXERR_NOTFOUND;` |
|        - | 10401 | `	}` |
|        - | 10402 | `	/* Remove the desired entry */` |
|  2868686 | 10403 | `	if( pEntry ){` |
|        - | 10404 | `		SyHashEntry **apEntry;` |
|       51 | 10405 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      195 | 10406 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      145 | 10407 | `			if( apEntry[n] == pEntry ){` |
|        - | 10408 | `				/* Nullify the entry */` |
|       51 | 10409 | `				apEntry[n] = 0;` |
|        - | 10410 | `				/*` |
|        - | 10411 | `				 * NOTE:` |
|        - | 10412 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 10413 | `				 * we avoid wasting spaces.` |
|        - | 10414 | `				 */` |
|       25 | 10415 | `			}` |
|       73 | 10416 | `		}` |
|       25 | 10417 | `	}` |
|  2868686 | 10418 | `	if( pMapEntry ){` |
|        - | 10419 | `		ph7_hashmap_node **apNode;` |
|  2868636 | 10420 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5737358 | 10421 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2868724 | 10422 | `			if( apNode[n] == pMapEntry ){` |
|        - | 10423 | `				/* nullify the entry */` |
|  2868636 | 10424 | `				apNode[n] = 0;` |
|  1434317 | 10425 | `			}` |
|  1434363 | 10426 | `		}` |
|  1434317 | 10427 | `	}` |
|  2868686 | 10428 | `	return SXRET_OK;` |
|  1470389 | 10429 |  |
|        - | 10430 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 10431 | `/*` |
|        - | 10432 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 10433 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 10434 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 10435 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 10436 | ` * For more information on how to register IO stream devices,please` |
|        - | 10437 | ` * refer to the official documentation.` |
|        - | 10438 | ` */` |
|    22598 | 10439 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 10440 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 10441 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 10442 | `	int nByte              /* *pzDevice length*/` |
|        - | 10443 | `	)` |
|        2 | 10444 |  |
|        - | 10445 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 10446 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 10447 | `	SyString sDev,sCur;` |
|        - | 10448 | `	sxu32 n,nEntry;` |
|        - | 10449 | `	int rc;` |
|        - | 10450 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    22600 | 10451 | `	zNext = zCur = zIn = *pzDevice;` |
|    22600 | 10452 | `	zEnd = &zIn[nByte];` |
|  1446565 | 10453 | `	while( zIn < zEnd ){` |
|  1423969 | 10454 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 10455 | `			/* Got one */` |
|        3 | 10456 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 10457 | `			break;` |
|        - | 10458 | `		}` |
|        - | 10459 | `		/* Advance the cursor */` |
|  1423967 | 10460 | `		zIn++;` |
|        2 | 10461 | `	}` |
|    22600 | 10462 | `	if( zIn >= zEnd ){` |
|        - | 10463 | `		/* No such scheme,return the default stream */` |
|    22598 | 10464 | `		return pVm->pDefStream;` |
|        - | 10465 | `	}` |
|        3 | 10466 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 10467 | `	/* Remove leading and trailing white spaces */` |
|        3 | 10468 | `	SyStringFullTrim(&sDev);` |
|        - | 10469 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 10470 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 10471 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 10472 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 10473 | `		pStream = apStream[n];` |
|        3 | 10474 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 10475 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 10476 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 10477 | `		if( rc == 0 ){` |
|        - | 10478 | `			/* Stream device found */` |
|        3 | 10479 | `			*pzDevice = zNext;` |
|        3 | 10480 | `			return pStream;` |
|        - | 10481 | `		}` |
|      ! 0 | 10482 | `	}` |
|        - | 10483 | `	/* No such stream,return NULL */` |
|      ! 0 | 10484 | `	return 0;` |
|    11301 | 10485 |  |
|        - | 10486 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 10487 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 10488 |  |
