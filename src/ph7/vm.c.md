# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3914/5172 lines (75.68%)

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
|   820626 |    86 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    87 |  |
|   820628 |    88 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       32 |    89 | `		return TRUE;` |
|        - |    90 | `	}` |
|   820598 |    91 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |    92 | `		return TRUE;` |
|        - |    93 | `	}` |
|   820590 |    94 | `	return FALSE;` |
|   410337 |    95 |  |
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
|   412920 |   110 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   412922 |   121 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   412922 |   122 | `	if( pEntry ){` |
|        - |   123 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   124 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   125 | `		pCons->xExpand = xExpand;` |
|        6 |   126 | `		pCons->pUserData = pUserData;` |
|        6 |   127 | `		return SXRET_OK;` |
|        - |   128 | `	}` |
|        - |   129 | `	/* Allocate a new constant instance */` |
|   412918 |   130 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   412918 |   131 | `	if( pCons == 0 ){` |
|      ! 0 |   132 | `		return 0;` |
|        - |   133 | `	}` |
|        - |   134 | `	/* Duplicate constant name */` |
|   412918 |   135 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   412918 |   136 | `	if( zDupName == 0 ){` |
|      ! 0 |   137 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   138 | `		return 0;` |
|        - |   139 | `	}` |
|        - |   140 | `	/* Install the constant */` |
|   412918 |   141 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   412918 |   142 | `	pCons->xExpand = xExpand;` |
|   412918 |   143 | `	pCons->pUserData = pUserData;` |
|   412918 |   144 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   412918 |   145 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   146 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return rc;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* All done,constant can be invoked from PHP code */` |
|   412918 |   151 | `	return SXRET_OK;` |
|   206462 |   152 |  |
|        - |   153 | `/*` |
|        - |   154 | ` * Allocate a new foreign function instance.` |
|        - |   155 | ` * This function return SXRET_OK on success. Any other` |
|        - |   156 | ` * return value indicates failure.` |
|        - |   157 | ` * Please refer to the official documentation for an introduction to` |
|        - |   158 | ` * the foreign function mechanism.` |
|        - |   159 | ` */` |
|   884790 |   160 | `static sxi32 PH7_NewForeignFunction(` |
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
|   884792 |   171 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   884792 |   172 | `	if( pFunc == 0 ){` |
|      ! 0 |   173 | `		return SXERR_MEM;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* Duplicate function name */` |
|   884792 |   176 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   884792 |   177 | `	if( zDup == 0 ){` |
|      ! 0 |   178 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   179 | `		return SXERR_MEM;` |
|        - |   180 | `	}` |
|        - |   181 | `	/* Zero the structure */` |
|   884792 |   182 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   183 | `	/* Initialize structure fields */` |
|   884792 |   184 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   884792 |   185 | `	pFunc->pVm   = pVm;` |
|   884792 |   186 | `	pFunc->xFunc = xFunc;` |
|   884792 |   187 | `	pFunc->pUserData = pUserData;` |
|   884792 |   188 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   189 | `	/* Write a pointer to the new function */` |
|   884792 |   190 | `	*ppOut = pFunc;` |
|   884792 |   191 | `	return SXRET_OK;` |
|   442397 |   192 |  |
|        - |   193 | `/*` |
|        - |   194 | ` * Install a foreign function and it's associated callback so that` |
|        - |   195 | ` * it can be invoked from the target PHP code.` |
|        - |   196 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   197 | ` * return value indicates failure.` |
|        - |   198 | ` * Please refer to the official documentation for an introduction to` |
|        - |   199 | ` * the foreign function mechanism.` |
|        - |   200 | ` */` |
|   886824 |   201 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|   886826 |   212 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   886826 |   213 | `	if( pEntry ){` |
|     2036 |   214 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2036 |   215 | `		pFunc->pUserData = pUserData;` |
|     2036 |   216 | `		pFunc->xFunc = xFunc;` |
|     2036 |   217 | `		SySetReset(&pFunc->aAux);` |
|     2036 |   218 | `		return SXRET_OK;` |
|        - |   219 | `	}` |
|        - |   220 | `	/* Create a new user function */` |
|   884792 |   221 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   884792 |   222 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   223 | `		return rc;` |
|        - |   224 | `	}` |
|        - |   225 | `	/* Install the function in the corresponding hashtable */` |
|   884792 |   226 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   884792 |   227 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   228 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   229 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   230 | `		return rc;` |
|        - |   231 | `	}` |
|        - |   232 | `	/* User function successfully installed */` |
|   884792 |   233 | `	return SXRET_OK;` |
|   443414 |   234 |  |
|        - |   235 | `/*` |
|        - |   236 | ` * Initialize a VM function.` |
|        - |   237 | ` */` |
|    96116 |   238 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   239 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   240 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   241 | `	const char *zName,  /* Function name */` |
|        - |   242 | `	sxu32 nByte,        /* zName length */` |
|        - |   243 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   244 | `	void *pUserData     /* Function private data */` |
|        - |   245 | `	)` |
|        2 |   246 |  |
|        - |   247 | `	/* Zero the structure */` |
|    96118 |   248 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   249 | `	/* Initialize structure fields */` |
|        - |   250 | `	/* Arguments container */` |
|    96118 |   251 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   252 | `	/* Static variable container */` |
|    96118 |   253 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   254 | `	/* Bytecode container */` |
|    96118 |   255 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   256 | `    /* Preallocate some instruction slots */` |
|    96118 |   257 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   258 | `	/* Closure environment */` |
|    96118 |   259 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|    96118 |   260 | `	pFunc->iFlags = iFlags;` |
|    96118 |   261 | `	pFunc->pUserData = pUserData;` |
|    96118 |   262 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|    96118 |   263 | `	return SXRET_OK;` |
|        2 |   264 |  |
|        - |   265 | `/*` |
|        - |   266 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   267 | ` */` |
|   348890 |   268 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   269 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   270 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   271 | `	SyString *pName     /* Function name */` |
|        - |   272 | `	)` |
|        2 |   273 |  |
|        - |   274 | `	SyHashEntry *pEntry;` |
|        - |   275 | `	sxi32 rc;` |
|   348892 |   276 | `	if( pName == 0 ){` |
|        - |   277 | `		/* Use the built-in name */` |
|    30014 |   278 | `		pName = &pFunc->sName;` |
|    15006 |   279 | `	}` |
|        - |   280 | `	/* Check for duplicates (functions with the same name) first */` |
|   348892 |   281 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   348892 |   282 | `	if( pEntry ){` |
|   271158 |   283 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   271158 |   284 | `		if( pLink != pFunc ){` |
|        - |   285 | `			/* Link */` |
|      179 |   286 | `			pFunc->pNextName = pLink;` |
|      179 |   287 | `			pEntry->pUserData = pFunc;` |
|       89 |   288 | `		}` |
|   271158 |   289 | `		return SXRET_OK;` |
|        - |   290 | `	}` |
|        - |   291 | `	/* First time seen */` |
|    77736 |   292 | `	pFunc->pNextName = 0;` |
|    77736 |   293 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    77736 |   294 | `	return rc;` |
|   174447 |   295 |  |
|        - |   296 | `/*` |
|        - |   297 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   298 | ` */` |
|    27530 |   299 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   300 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   301 | `	ph7_class *pClass /* Target Class */` |
|        - |   302 | `	)` |
|        2 |   303 |  |
|    27532 |   304 | `	SyString *pName = &pClass->sName;` |
|        - |   305 | `	SyHashEntry *pEntry;` |
|        - |   306 | `	sxi32 rc;` |
|        - |   307 | `	/* Check for duplicates */` |
|    27532 |   308 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    27532 |   309 | `	if( pEntry ){` |
|       31 |   310 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   311 | `		/* Link entry with the same name */` |
|       31 |   312 | `		pClass->pNextName = pLink;` |
|       31 |   313 | `		pEntry->pUserData = pClass;` |
|       31 |   314 | `		return SXRET_OK;` |
|        - |   315 | `	}` |
|    27502 |   316 | `	pClass->pNextName = 0;` |
|        - |   317 | `	/* Perform a simple hashtable insertion */` |
|    27502 |   318 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    27502 |   319 | `	return rc;` |
|    13767 |   320 |  |
|        - |   321 | `/*` |
|        - |   322 | ` * Instruction builder interface.` |
|        - |   323 | ` */` |
|  2554696 |   324 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  2554698 |   336 | `	sInstr.iOp = (sxu8)iOp;` |
|  2554698 |   337 | `	sInstr.iP1 = iP1;` |
|  2554698 |   338 | `	sInstr.iP2 = iP2;` |
|  2554698 |   339 | `	sInstr.p3  = p3;` |
|  2554698 |   340 | `	if( pIndex ){` |
|        - |   341 | `		/* Instruction index in the bytecode array */` |
|   162972 |   342 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    81485 |   343 | `	}` |
|        - |   344 | `	/* Finally,record the instruction */` |
|  2554698 |   345 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  2554698 |   346 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   347 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   348 | `		/* Fall throw */` |
|      ! 0 |   349 | `	}` |
|  2554698 |   350 | `	return rc;` |
|        2 |   351 |  |
|        - |   352 | `/*` |
|        - |   353 | ` * Swap the current bytecode container with the given one.` |
|        - |   354 | ` */` |
|   233632 |   355 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   356 |  |
|   233634 |   357 | `	if( pContainer == 0 ){` |
|        - |   358 | `		/* Point to the default container */` |
|      ! 0 |   359 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   360 | `	}else{` |
|        - |   361 | `		/* Change container */` |
|   233634 |   362 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   363 | `	}` |
|   233634 |   364 | `	return SXRET_OK;` |
|        2 |   365 |  |
|        - |   366 | `/*` |
|        - |   367 | ` * Return the current bytecode container.` |
|        - |   368 | ` */` |
|   116816 |   369 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   370 |  |
|   116818 |   371 | `	return pVm->pByteContainer;` |
|        2 |   372 |  |
|        - |   373 | `/*` |
|        - |   374 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   375 | ` */` |
|   160622 |   376 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   377 |  |
|        - |   378 | `	VmInstr *pInstr;` |
|   160624 |   379 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   160624 |   380 | `	return pInstr;` |
|        2 |   381 |  |
|        - |   382 | `/*` |
|        - |   383 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   384 | ` */` |
|   715632 |   385 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   386 |  |
|   715634 |   387 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   388 |  |
|        - |   389 | `/*` |
|        - |   390 | ` * Pop the last VM instruction.` |
|        - |   391 | ` */` |
|   152340 |   392 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   393 |  |
|   152342 |   394 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   395 |  |
|        - |   396 | `/*` |
|        - |   397 | ` * Peek the last VM instruction.` |
|        - |   398 | ` */` |
|   403490 |   399 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   400 |  |
|   403492 |   401 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   402 |  |
|    11590 |   403 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   404 |  |
|        - |   405 | `	VmInstr *aInstr;` |
|        - |   406 | `	sxu32 n;` |
|    11592 |   407 | `	n = SySetUsed(pVm->pByteContainer);` |
|    11592 |   408 | `	if( n < 2 ){` |
|      ! 0 |   409 | `		return 0;` |
|        - |   410 | `	}` |
|    11592 |   411 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    11592 |   412 | `	return &aInstr[n - 2];` |
|     5797 |   413 |  |
|        - |   414 | `/*` |
|        - |   415 | ` * Allocate a new virtual machine frame.` |
|        - |   416 | ` */` |
|    13992 |   417 | `static VmFrame * VmNewFrame(` |
|        - |   418 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   419 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   420 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   421 | `	)` |
|        2 |   422 |  |
|        - |   423 | `	VmFrame *pFrame;` |
|        - |   424 | `	/* Allocate a new vm frame */` |
|    13994 |   425 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    13994 |   426 | `	if( pFrame == 0 ){` |
|      ! 0 |   427 | `		return 0;` |
|        - |   428 | `	}` |
|        - |   429 | `	/* Zero the structure */` |
|    13994 |   430 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   431 | `	/* Initialize frame fields */` |
|    13994 |   432 | `	pFrame->pUserData = pUserData;` |
|    13994 |   433 | `	pFrame->pThis = pThis;` |
|    13994 |   434 | `	pFrame->pVm = pVm;` |
|    13994 |   435 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    13994 |   436 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    13994 |   437 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    13994 |   438 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    13994 |   439 | `	return pFrame;` |
|     6998 |   440 |  |
|        - |   441 | `/*` |
|        - |   442 | ` * Enter a VM frame.` |
|        - |   443 | ` */` |
|    13992 |   444 | `static sxi32 VmEnterFrame(` |
|        - |   445 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   446 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   447 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   448 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   449 | `	)` |
|        2 |   450 |  |
|        - |   451 | `	VmFrame *pFrame;` |
|        - |   452 | `	/* Allocate a new frame */` |
|    13994 |   453 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    13994 |   454 | `	if( pFrame == 0 ){` |
|      ! 0 |   455 | `		return SXERR_MEM;` |
|        - |   456 | `	}` |
|        - |   457 | `	/* Link to the list of active VM frame */` |
|    13994 |   458 | `	pFrame->pParent = pVm->pFrame;` |
|    13994 |   459 | `	pVm->pFrame = pFrame;` |
|    13994 |   460 | `	if( ppFrame ){` |
|        - |   461 | `		/* Write a pointer to the new VM frame */` |
|    11722 |   462 | `		*ppFrame = pFrame;` |
|     5860 |   463 | `	}` |
|    13994 |   464 | `	return SXRET_OK;` |
|     6998 |   465 |  |
|        - |   466 | `/*` |
|        - |   467 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   468 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   469 | ` * information.` |
|        - |   470 | ` */` |
|       52 |   471 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        2 |   472 |  |
|        - |   473 | `	VmFrame *pTarget,*pFrame;` |
|       54 |   474 | `	SyHashEntry *pEntry = 0;` |
|        - |   475 | `	sxi32 rc;` |
|        - |   476 | `	/* Point to the upper frame */` |
|       54 |   477 | `	pFrame = pVm->pFrame;` |
|       54 |   478 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |   479 | `		/* Safely ignore the exception frame */` |
|      ! 0 |   480 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   481 | `	}` |
|       54 |   482 | `	pTarget = pFrame;` |
|       54 |   483 | `	pFrame = pTarget->pParent;` |
|       54 |   484 | `	while( pFrame ){` |
|       54 |   485 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   486 | `			/* Query the current frame */` |
|       54 |   487 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       54 |   488 | `			if( pEntry ){` |
|        - |   489 | `				/* Variable found */` |
|       54 |   490 | `				break;` |
|        - |   491 | `			}` |
|      ! 0 |   492 | `		}` |
|        - |   493 | `		/* Point to the upper frame */` |
|      ! 0 |   494 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   495 | `	}` |
|       54 |   496 | `	if( pEntry == 0 ){` |
|        - |   497 | `		/* Inexistant variable */` |
|      ! 0 |   498 | `		return SXERR_NOTFOUND;` |
|        - |   499 | `	}` |
|        - |   500 | `	/* Link to the current frame */` |
|       54 |   501 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       54 |   502 | `	if( rc == SXRET_OK ){` |
|        - |   503 | `		sxu32 nIdx;` |
|       54 |   504 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       54 |   505 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       26 |   506 | `	}` |
|       54 |   507 | `	return rc;` |
|       28 |   508 |  |
|        - |   509 | `/*` |
|        - |   510 | ` * Leave the top-most active frame.` |
|        - |   511 | ` */` |
|    11720 |   512 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   513 |  |
|    11722 |   514 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    11722 |   515 | `	if( pCurFrame ){` |
|        - |   516 | `		/* Unlink from the list of active VM frame */` |
|    11722 |   517 | `		pVm->pFrame = pCurFrame->pParent;` |
|    11722 |   518 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   519 | `			VmSlot  *aSlot;` |
|        - |   520 | `			sxu32 n;` |
|        - |   521 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    11698 |   522 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    84460 |   523 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   524 | `				/* Unset the local variable */` |
|    72764 |   525 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    36383 |   526 | `			}` |
|        - |   527 | `			/* Remove local reference */` |
|    11698 |   528 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    84516 |   529 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    72820 |   530 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    36411 |   531 | `			}` |
|     5848 |   532 | `		}` |
|        - |   533 | `		/* Release internal containers */` |
|    11722 |   534 | `		SyHashRelease(&pCurFrame->hVar);` |
|    11722 |   535 | `		SySetRelease(&pCurFrame->sArg);` |
|    11722 |   536 | `		SySetRelease(&pCurFrame->sLocal);` |
|    11722 |   537 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   538 | `		/* Release the whole structure */` |
|    11722 |   539 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     5860 |   540 | `	}` |
|    11722 |   541 |  |
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
|    83800 |   658 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   659 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   660 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   661 | `	)` |
|        2 |   662 |  |
|        - |   663 | `	ph7_class_method *pMeth;` |
|        - |   664 | `	ph7_class_attr *pAttr;` |
|        - |   665 | `	SyHashEntry *pEntry;` |
|        - |   666 | `	sxi32 rc;` |
|        - |   667 | `	/* Reset the loop cursor */` |
|    83802 |   668 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   669 | `	/* Process only static and constant attribute */` |
|   325254 |   670 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   671 | `		/* Extract the current attribute */` |
|   199554 |   672 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   199554 |   673 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|    83802 |   695 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |   696 | `		/* Do not mount interface methods since they are signatures only.` |
|        - |   697 | `		 */` |
|    44806 |   698 | `		return SXRET_OK;` |
|        - |   699 | `	}` |
|        - |   700 | `	/* Create constructor alias if not yet done */` |
|    38998 |   701 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   702 | `		/* User constructor with the same base class name */` |
|      214 |   703 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      214 |   704 | `		if( pEntry ){` |
|      ! 0 |   705 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   706 | `			/* Create the alias */` |
|      ! 0 |   707 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   708 | `		}` |
|      106 |   709 | `	}` |
|        - |   710 | `	/* Install the methods now */` |
|    38998 |   711 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   377380 |   712 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   318886 |   713 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   318886 |   714 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   318880 |   715 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   318880 |   716 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   717 | `				return rc;` |
|        - |   718 | `			}` |
|   159439 |   719 | `		}` |
|        2 |   720 | `	}` |
|        - |   721 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    38998 |   722 | `	pClass->bMounted = TRUE;` |
|    38998 |   723 | `	return SXRET_OK;` |
|    41902 |   724 |  |
|        - |   725 | `/*` |
|        - |   726 | ` * Allocate a private frame for attributes of the given` |
|        - |   727 | ` * class instance (Object in the PHP jargon).` |
|        - |   728 | ` */` |
|     1040 |   729 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   730 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   731 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   732 | `	)` |
|        2 |   733 |  |
|     1042 |   734 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   735 | `	ph7_class_attr *pAttr;` |
|        - |   736 | `	SyHashEntry *pEntry;` |
|        - |   737 | `	sxi32 rc;` |
|        - |   738 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1042 |   739 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4524 |   740 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   741 | `		VmClassAttr *pVmAttr;` |
|        - |   742 | `		/* Extract the current attribute */` |
|     3484 |   743 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3484 |   744 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3484 |   745 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   746 | `			return SXERR_MEM;` |
|        - |   747 | `		}` |
|     3484 |   748 | `		pVmAttr->pAttr = pAttr;` |
|     3484 |   749 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   750 | `			ph7_value *pMemObj;` |
|        - |   751 | `			/* Reserve a memory object for this attribute */` |
|     3478 |   752 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3478 |   753 | `			if( pMemObj == 0 ){` |
|      ! 0 |   754 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   755 | `				return SXERR_MEM;` |
|        - |   756 | `			}` |
|     3478 |   757 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3478 |   758 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   759 | `				/* Initialize attribute default value (any complex expression) */` |
|     1136 |   760 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      567 |   761 | `			}` |
|     3478 |   762 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3478 |   763 | `			if( rc != SXRET_OK ){` |
|        - |   764 | `				VmSlot sSlot;` |
|        - |   765 | `				/* Restore memory object */` |
|      ! 0 |   766 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   767 | `				sSlot.pUserData = 0;` |
|      ! 0 |   768 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   769 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   770 | `				return SXERR_MEM;` |
|        - |   771 | `			}` |
|        - |   772 | `			/* Install attribute in the reference table */` |
|     3478 |   773 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1740 |   774 | `		}else{` |
|        - |   775 | `			/* Install static/constant attribute */` |
|        8 |   776 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   777 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   778 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   779 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   780 | `				return SXERR_MEM;` |
|        - |   781 | `			}` |
|        - |   782 | `		}` |
|        2 |   783 | `	}` |
|     1042 |   784 | `	return SXRET_OK;` |
|      522 |   785 |  |
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
|   279476 |   797 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   798 |  |
|        - |   799 | `	ph7_value *pObj;` |
|        - |   800 | `	sxi32 rc;` |
|   279478 |   801 | `	if( pIndex ){` |
|        - |   802 | `		/* Object index in the object table */` |
|   272662 |   803 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   136330 |   804 | `	}` |
|        - |   805 | `	/* Reserve a slot for the new object */` |
|   279478 |   806 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   279478 |   807 | `	if( rc != SXRET_OK ){` |
|        - |   808 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   809 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   810 | `		 */` |
|      ! 0 |   811 | `		return 0;` |
|        - |   812 | `	}` |
|   279478 |   813 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   279478 |   814 | `	return pObj;` |
|   139740 |   815 |  |
|        - |   816 | `/*` |
|        - |   817 | ` * Reserve a memory object.` |
|        - |   818 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   819 | ` */` |
|  2136734 |   820 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   821 |  |
|        - |   822 | `	ph7_value *pObj;` |
|        - |   823 | `	sxi32 rc;` |
|  2136736 |   824 | `	if( pIndex ){` |
|        - |   825 | `		/* Object index in the object table */` |
|  2136736 |   826 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1068367 |   827 | `	}` |
|        - |   828 | `	/* Reserve a slot for the new object */` |
|  2136736 |   829 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2136736 |   830 | `	if( rc != SXRET_OK ){` |
|        - |   831 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   832 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   833 | `		 */` |
|      ! 0 |   834 | `		return 0;` |
|        - |   835 | `	}` |
|  2136736 |   836 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2136736 |   837 | `	return pObj;` |
|  1068369 |   838 |  |
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
|     2272 |  1191 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1192 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1193 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1194 | `	 )` |
|        2 |  1195 |  |
|        - |  1196 | `	SyString sBuiltin;` |
|        - |  1197 | `	ph7_value *pObj;` |
|        - |  1198 | `	sxi32 rc;` |
|        - |  1199 | `	/* Zero the structure */` |
|     2274 |  1200 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1201 | `	/* Initialize VM fields */` |
|     2274 |  1202 | `	pVm->pEngine = &(*pEngine);` |
|     2274 |  1203 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1204 | `	/* Instructions containers */` |
|     2274 |  1205 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2274 |  1206 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2274 |  1207 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1208 | `	/* Object containers */` |
|     2274 |  1209 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2274 |  1210 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1211 | `	/* Virtual machine internal containers */` |
|     2274 |  1212 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2274 |  1213 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2274 |  1214 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2274 |  1215 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2274 |  1216 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2274 |  1217 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2274 |  1218 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2274 |  1219 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2274 |  1220 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2274 |  1221 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2274 |  1222 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2274 |  1223 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2274 |  1224 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2274 |  1225 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2274 |  1226 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|        - |  1227 | `	/* Configuration containers */` |
|     2274 |  1228 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2274 |  1229 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2274 |  1230 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2274 |  1231 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2274 |  1232 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1233 | `	/* Error callbacks containers */` |
|     2274 |  1234 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2274 |  1235 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2274 |  1236 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2274 |  1237 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2274 |  1238 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1239 | `	/* Set a default recursion limit */` |
|        - |  1240 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2274 |  1241 | `	pVm->nMaxDepth = 32;` |
|        - |  1242 | `#else` |
|        - |  1243 | `	pVm->nMaxDepth = 16;` |
|        - |  1244 | `#endif` |
|        - |  1245 | `	/* Default assertion flags */` |
|     2274 |  1246 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1247 | `	/* JSON return status */` |
|     2274 |  1248 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1249 | `	/* PRNG context */` |
|     2274 |  1250 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1251 | `	/* Install the null constant */` |
|     2274 |  1252 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2274 |  1253 | `	if( pObj == 0 ){` |
|      ! 0 |  1254 | `		rc = SXERR_MEM;` |
|      ! 0 |  1255 | `		goto Err;` |
|        - |  1256 | `	}` |
|     2274 |  1257 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1258 | `	/* Install the boolean TRUE constant */` |
|     2274 |  1259 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2274 |  1260 | `	if( pObj == 0 ){` |
|      ! 0 |  1261 | `		rc = SXERR_MEM;` |
|      ! 0 |  1262 | `		goto Err;` |
|        - |  1263 | `	}` |
|     2274 |  1264 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1265 | `	/* Install the boolean FALSE constant */` |
|     2274 |  1266 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2274 |  1267 | `	if( pObj == 0 ){` |
|      ! 0 |  1268 | `		rc = SXERR_MEM;` |
|      ! 0 |  1269 | `		goto Err;` |
|        - |  1270 | `	}` |
|     2274 |  1271 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1272 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1273 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1274 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2274 |  1275 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2274 |  1276 | `	if( pObj == 0 ){` |
|      ! 0 |  1277 | `		rc = SXERR_MEM;` |
|      ! 0 |  1278 | `		goto Err;` |
|        - |  1279 | `	}` |
|     2274 |  1280 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1281 | `	/* Create the global frame */` |
|     2274 |  1282 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2274 |  1283 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1284 | `		goto Err;` |
|        - |  1285 | `	}` |
|        - |  1286 | `	/* Initialize the code generator */` |
|     2274 |  1287 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2274 |  1288 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1289 | `		goto Err;` |
|        - |  1290 | `	}` |
|        - |  1291 | `	/* VM correctly initialized,set the magic number */` |
|     2274 |  1292 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2274 |  1293 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1294 | `	/* Compile the built-in library */` |
|     2274 |  1295 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1296 | `	/* Reset the code generator */` |
|     2274 |  1297 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2274 |  1298 | `	return SXRET_OK;` |
|      ! 0 |  1299 | `Err:` |
|      ! 0 |  1300 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1301 | `	return rc;` |
|     1138 |  1302 |  |
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
|    29492 |  1332 | `static ph7_value * VmNewOperandStack(` |
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
|    29494 |  1345 | `	nInstr += VM_STACK_GUARD;` |
|    29494 |  1346 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    29494 |  1347 | `	if( pStack == 0 ){` |
|      ! 0 |  1348 | `		return 0;` |
|        - |  1349 | `	}` |
|        - |  1350 | `	/* Initialize the operand stack */` |
|  1873302 |  1351 | `	while( nInstr > 0 ){` |
|  1843810 |  1352 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1843810 |  1353 | `		--nInstr;` |
|        2 |  1354 | `	}` |
|        - |  1355 | `	/* Ready for bytecode execution */` |
|    29494 |  1356 | `	return pStack;` |
|    14748 |  1357 |  |
|        - |  1358 | `/* Forward declaration */` |
|        - |  1359 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1360 | `/*` |
|        - |  1361 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1362 | ` * This routine gets called by the PH7 engine after` |
|        - |  1363 | ` * successful compilation of the target PHP program.` |
|        - |  1364 | ` */` |
|     2034 |  1365 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1366 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1367 | `	)` |
|        2 |  1368 |  |
|        - |  1369 | `	SyHashEntry *pEntry;` |
|        - |  1370 | `	sxi32 rc;` |
|     2036 |  1371 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1372 | `		/* Initialize your VM first */` |
|      ! 0 |  1373 | `		return SXERR_CORRUPT;` |
|        - |  1374 | `	}` |
|        - |  1375 | `	/* Mark the VM ready for byte-code execution */` |
|     2036 |  1376 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1377 | `	/* Release the code generator now we have compiled our program */` |
|     2036 |  1378 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1379 | `	/* Emit the DONE instruction */` |
|     2036 |  1380 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2036 |  1381 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1382 | `		return SXERR_MEM;` |
|        - |  1383 | `	}` |
|        - |  1384 | `	/* Script return value */` |
|     2036 |  1385 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1386 | `	/* Allocate a new operand stack */` |
|     2036 |  1387 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2036 |  1388 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1389 | `		return SXERR_MEM;` |
|        - |  1390 | `	}` |
|        - |  1391 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1392 | `	 * private data. */` |
|     2036 |  1393 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2036 |  1394 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1395 | `	/* Allocate the reference table */` |
|     2036 |  1396 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2036 |  1397 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2036 |  1398 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1399 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1400 | `		return SXERR_MEM;` |
|        - |  1401 | `	}` |
|        - |  1402 | `	/* Zero the reference table */` |
|     2036 |  1403 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1404 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2036 |  1405 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2036 |  1406 | `	if( rc != SXRET_OK ){` |
|        - |  1407 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1408 | `		return rc;` |
|        - |  1409 | `	}` |
|        - |  1410 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2036 |  1411 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2036 |  1412 | `	if( rc != SXRET_OK ){` |
|        - |  1413 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1414 | `		return rc;` |
|        - |  1415 | `	}` |
|        - |  1416 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2036 |  1417 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1418 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2036 |  1419 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1420 | `	/* Initialize and install static and constants class attributes */` |
|     2036 |  1421 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    26482 |  1422 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    24448 |  1423 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    24448 |  1424 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1425 | `			return rc;` |
|        - |  1426 | `		}` |
|        2 |  1427 | `	}` |
|        - |  1428 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2036 |  1429 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1430 | `	/* VM is ready for bytecode execution */` |
|     2036 |  1431 | `	return SXRET_OK;` |
|     1019 |  1432 |  |
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
|     2026 |  1452 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1453 |  |
|        - |  1454 | `	/* Set the stale magic number */` |
|     2028 |  1455 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1456 | `	/* Release the private memory subsystem */` |
|     2028 |  1457 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2028 |  1458 | `	return SXRET_OK;` |
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
|   560066 |  1470 | `static sxi32 VmInitCallContext(` |
|        - |  1471 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1472 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1473 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1474 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1475 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1476 | `	)` |
|        2 |  1477 |  |
|   560068 |  1478 | `	pOut->pFunc = pFunc;` |
|   560068 |  1479 | `	pOut->pVm   = pVm;` |
|   560068 |  1480 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   560068 |  1481 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1482 | `	/* Assume a null return value */` |
|   560068 |  1483 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   560068 |  1484 | `	pOut->pRet = pRet;` |
|   560068 |  1485 | `	pOut->iFlags = iFlags;` |
|   560068 |  1486 | `	return SXRET_OK;` |
|        2 |  1487 |  |
|        - |  1488 | `/*` |
|        - |  1489 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1490 | ` * left behind.` |
|        - |  1491 | ` */` |
|   560066 |  1492 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1493 |  |
|        - |  1494 | `	sxu32 n;` |
|   560068 |  1495 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6538 |  1496 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    18618 |  1497 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    12082 |  1498 | `			if( apObj[n] == 0 ){` |
|        - |  1499 | `				/* Already released */` |
|      250 |  1500 | `				continue;` |
|        - |  1501 | `			}` |
|    11834 |  1502 | `			PH7_MemObjRelease(apObj[n]);` |
|    11834 |  1503 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     5918 |  1504 | `		}` |
|     6538 |  1505 | `		SySetRelease(&pCtx->sVar);` |
|     3268 |  1506 | `	}` |
|   560068 |  1507 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
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
|   560068 |  1523 |  |
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
|  3399688 |  1554 | `static void VmPopOperand(` |
|        - |  1555 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1556 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1557 | `	)` |
|        2 |  1558 |  |
|  3399690 |  1559 | `	ph7_value *pTos = *ppTos;` |
|  7188082 |  1560 | `	while( nPop > 0 ){` |
|  3788394 |  1561 | `		PH7_MemObjRelease(pTos);` |
|  3788394 |  1562 | `		pTos--;` |
|  3788394 |  1563 | `		nPop--;` |
|        2 |  1564 | `	}` |
|        - |  1565 | `	/* Top of the stack */` |
|  3399690 |  1566 | `	*ppTos = pTos;` |
|  3399690 |  1567 |  |
|        - |  1568 | `/*` |
|        - |  1569 | ` * Reserve a memory object.` |
|        - |  1570 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1571 | ` */` |
|  2983984 |  1572 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1573 |  |
|  2983986 |  1574 | `	ph7_value *pObj = 0;` |
|        - |  1575 | `	VmSlot *pSlot;` |
|        - |  1576 | `	sxu32 nIdx;` |
|        - |  1577 | `	/* Check for a free slot */` |
|  2983986 |  1578 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  2983986 |  1579 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  2983986 |  1580 | `	if( pSlot ){` |
|   847252 |  1581 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   847252 |  1582 | `		nIdx = pSlot->nIdx;` |
|   423625 |  1583 | `	}` |
|  2983986 |  1584 | `	if( pObj == 0 ){` |
|        - |  1585 | `		/* Reserve a new memory object */` |
|  2136736 |  1586 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2136736 |  1587 | `		if( pObj == 0 ){` |
|      ! 0 |  1588 | `			return 0;` |
|        - |  1589 | `		}` |
|  1068367 |  1590 | `	}` |
|        - |  1591 | `	/* Set a null default value */` |
|  2983986 |  1592 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  2983986 |  1593 | `	pObj->nIdx = nIdx;` |
|  2983986 |  1594 | `	return pObj;` |
|  1491994 |  1595 |  |
|        - |  1596 | `/*` |
|        - |  1597 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1598 | ` */` |
|    25914 |  1599 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1600 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1601 | `	const char *zKey,  /* Entry key */` |
|        - |  1602 | `	sxu32 nByte,       /* Key length */` |
|        - |  1603 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1604 | `	)` |
|        2 |  1605 |  |
|        - |  1606 | `	ph7_value sKey;` |
|        - |  1607 | `	sxi32 rc;` |
|    25916 |  1608 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    25916 |  1609 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1610 | `	/* Perform the insertion */` |
|    25916 |  1611 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    25916 |  1612 | `	PH7_MemObjRelease(&sKey);` |
|    25916 |  1613 | `	return rc;` |
|        2 |  1614 |  |
|        - |  1615 | `/*` |
|        - |  1616 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1617 | ` * Return a pointer to the variable value on success.` |
|        - |  1618 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1619 | ` */` |
|  3225924 |  1620 | `static ph7_value * VmExtractMemObj(` |
|        - |  1621 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1622 | `	const SyString *pName, /* Variable name */` |
|        - |  1623 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1624 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1625 | `	)` |
|        2 |  1626 |  |
|  3225926 |  1627 | `	int bNullify = FALSE;` |
|        - |  1628 | `	SyHashEntry *pEntry;` |
|        - |  1629 | `	VmFrame *pFrame;` |
|        - |  1630 | `	ph7_value *pObj;` |
|        - |  1631 | `	sxu32 nIdx;` |
|        - |  1632 | `	sxi32 rc;` |
|        - |  1633 | `	/* Point to the top active frame */` |
|  3225926 |  1634 | `	pFrame = pVm->pFrame;` |
|  3225938 |  1635 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1636 | `		/* Safely ignore the exception frame */` |
|       13 |  1637 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        1 |  1638 | `	}` |
|        - |  1639 | `	/* Perform the lookup */` |
|  3225926 |  1640 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1641 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1642 | `		pName = &sAnnon;` |
|        - |  1643 | `		/* Always nullify the object */` |
|      ! 0 |  1644 | `		bNullify = TRUE;` |
|      ! 0 |  1645 | `		bDup = FALSE;` |
|      ! 0 |  1646 | `	}` |
|        - |  1647 | `	/* Check the superglobals table first */` |
|  3225926 |  1648 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3225926 |  1649 | `	if( pEntry == 0 ){` |
|        - |  1650 | `		/* Query the top active frame */` |
|  3225890 |  1651 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3225890 |  1652 | `		if( pEntry == 0 ){` |
|    78946 |  1653 | `			char *zName = (char *)pName->zString;` |
|        - |  1654 | `			VmSlot sLocal;` |
|    78946 |  1655 | `			if( !bCreate ){` |
|        - |  1656 | `				/* Do not create the variable,return NULL instead */` |
|      634 |  1657 | `				return 0;` |
|        - |  1658 | `			}` |
|        - |  1659 | `			/* No such variable,automatically create a new one and install` |
|        - |  1660 | `			 * it in the current frame.` |
|        - |  1661 | `			 */` |
|    78314 |  1662 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    78314 |  1663 | `			if( pObj == 0 ){` |
|      ! 0 |  1664 | `				return 0;` |
|        - |  1665 | `			}` |
|    78314 |  1666 | `			nIdx = pObj->nIdx;` |
|    78314 |  1667 | `			if( bDup ){` |
|        - |  1668 | `				/* Duplicate name */` |
|      164 |  1669 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      164 |  1670 | `				if( zName == 0 ){` |
|      ! 0 |  1671 | `					return 0;` |
|        - |  1672 | `				}` |
|       81 |  1673 | `			}` |
|        - |  1674 | `			/* Link to the top active VM frame */` |
|    78314 |  1675 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    78314 |  1676 | `			if( rc != SXRET_OK ){` |
|        - |  1677 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1678 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1679 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1680 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1681 | `				return 0;` |
|        - |  1682 | `			}` |
|    78314 |  1683 | `			if( pFrame->pParent != 0 ){` |
|        - |  1684 | `				/* Local variable */` |
|    72764 |  1685 | `				sLocal.nIdx = nIdx;` |
|    72764 |  1686 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    36383 |  1687 | `			}else{` |
|        - |  1688 | `				/* Register in the $GLOBALS array */` |
|     5552 |  1689 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1690 | `			}` |
|        - |  1691 | `			/* Install in the reference table */` |
|    78314 |  1692 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1693 | `			/* Save object index */` |
|    78314 |  1694 | `			pObj->nIdx = nIdx;` |
|    39158 |  1695 | `		}else{` |
|        - |  1696 | `			/* Extract variable contents */` |
|  3146946 |  1697 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3146946 |  1698 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3146946 |  1699 | `			if( bNullify && pObj ){` |
|      ! 0 |  1700 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1701 | `			}` |
|        - |  1702 | `		}` |
|  1612740 |  1703 | `	}else{` |
|        - |  1704 | `		/* Superglobal */` |
|       38 |  1705 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1706 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1707 | `	}` |
|  3225294 |  1708 | `	return pObj;` |
|  1613074 |  1709 |  |
|        - |  1710 | `/*` |
|        - |  1711 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1712 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1713 | ` */` |
|     2060 |  1714 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1715 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1716 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1717 | `	sxu32 nByte        /* zName length */` |
|        - |  1718 | `	)` |
|        2 |  1719 |  |
|        - |  1720 | `	SyHashEntry *pEntry;` |
|        - |  1721 | `	ph7_value *pValue;` |
|        - |  1722 | `	sxu32 nIdx;` |
|        - |  1723 | `	/* Query the superglobal table */` |
|     2062 |  1724 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2062 |  1725 | `	if( pEntry == 0 ){` |
|        - |  1726 | `		/* No such entry */` |
|      ! 0 |  1727 | `		return 0;` |
|        - |  1728 | `	}` |
|        - |  1729 | `	/* Extract the superglobal index in the global object pool */` |
|     2062 |  1730 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1731 | `	/* Extract the variable value  */` |
|     2062 |  1732 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2062 |  1733 | `	return pValue;` |
|     1032 |  1734 |  |
|        - |  1735 | `/*` |
|        - |  1736 | ` * Perform a raw hashmap insertion.` |
|        - |  1737 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1738 | ` */` |
|     2058 |  1739 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1740 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1741 | `	const char *zKey,   /* Entry key */` |
|        - |  1742 | `	int nKeylen,        /* zKey length*/` |
|        - |  1743 | `	const char *zData,  /* Entry data */` |
|        - |  1744 | `	int nLen            /* zData length */` |
|        - |  1745 | `	)` |
|        2 |  1746 |  |
|        - |  1747 | `	ph7_value sKey,sValue;` |
|        - |  1748 | `	sxi32 rc;` |
|     2060 |  1749 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2060 |  1750 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2060 |  1751 | `	if( zKey ){` |
|     2038 |  1752 | `		if( nKeylen < 0 ){` |
|     2038 |  1753 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1018 |  1754 | `		}` |
|     2038 |  1755 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1018 |  1756 | `	}` |
|     2060 |  1757 | `	if( zData ){` |
|     2060 |  1758 | `		if( nLen < 0 ){` |
|        - |  1759 | `			/* Compute length automatically */` |
|      ! 0 |  1760 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1761 | `		}` |
|     2060 |  1762 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1029 |  1763 | `	}` |
|        - |  1764 | `	/* Perform the insertion */` |
|     2060 |  1765 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2060 |  1766 | `	PH7_MemObjRelease(&sKey);` |
|     2060 |  1767 | `	PH7_MemObjRelease(&sValue);` |
|     2060 |  1768 | `	return rc;` |
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
|    32568 |  1783 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1784 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1785 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1786 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1787 | `	)` |
|        2 |  1788 |  |
|    32570 |  1789 | `	sxi32 rc = SXRET_OK;` |
|    32570 |  1790 | `	switch(nOp){` |
|     1017 |  1791 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2036 |  1792 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2036 |  1793 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1794 | `		/* VM output consumer callback */` |
|        - |  1795 | `#ifdef UNTRUST` |
|        - |  1796 | `		if( xConsumer == 0 ){` |
|        - |  1797 | `			rc = SXERR_CORRUPT;` |
|        - |  1798 | `			break;` |
|        - |  1799 | `		}` |
|        - |  1800 | `#endif` |
|        - |  1801 | `		/* Install the output consumer */` |
|     2036 |  1802 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2036 |  1803 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2036 |  1804 | `		break;` |
|        - |  1805 | `							   }` |
|     1017 |  1806 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1807 | `		/* Import path */` |
|        - |  1808 | `		  const char *zPath;` |
|        - |  1809 | `		  SyString sPath;` |
|     2036 |  1810 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1811 | `#if defined(UNTRUST)` |
|        - |  1812 | `		  if( zPath == 0 ){` |
|        - |  1813 | `			  rc = SXERR_EMPTY;` |
|        - |  1814 | `			  break;` |
|        - |  1815 | `		  }` |
|        - |  1816 | `#endif` |
|     2036 |  1817 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1818 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1819 | `#ifdef __WINNT__` |
|        2 |  1820 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1821 | `#endif` |
|     4070 |  1822 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1823 | `		  /* Remove leading and trailing white spaces */` |
|     2036 |  1824 | `		  SyStringFullTrim(&sPath);` |
|     2036 |  1825 | `		  if( sPath.nByte > 0 ){` |
|        - |  1826 | `			  /* Store the path in the corresponding conatiner */` |
|     2036 |  1827 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1017 |  1828 | `		  }` |
|     2036 |  1829 | `		  break;` |
|        - |  1830 | `									 }` |
|     1017 |  1831 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1832 | `		/* Run-Time Error report */` |
|     2036 |  1833 | `		pVm->bErrReport = 1;` |
|     2036 |  1834 | `		break;` |
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
|    10170 |  1856 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1857 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1858 | `		/* Create a new superglobal/global variable */` |
|    20342 |  1859 | `		const char *zName = va_arg(ap,const char *);` |
|    20342 |  1860 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
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
|    20342 |  1871 | `		nByte = SyStrlen(zName);` |
|    20342 |  1872 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1873 | `			/* Check if the superglobal is already installed */` |
|    20342 |  1874 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    10172 |  1875 | `		}else{` |
|        - |  1876 | `			/* Query the top active VM frame */` |
|      ! 0 |  1877 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1878 | `		}` |
|    20342 |  1879 | `		if( pEntry ){` |
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
|    20342 |  1890 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    20342 |  1891 | `			if( pObj == 0 ){` |
|      ! 0 |  1892 | `				rc = SXERR_MEM;` |
|      ! 0 |  1893 | `				break;` |
|        - |  1894 | `			}` |
|    20342 |  1895 | `			nIdx = pObj->nIdx;` |
|        - |  1896 | `			/* Copy value */` |
|    20342 |  1897 | `			PH7_MemObjStore(pValue,pObj);` |
|    20342 |  1898 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1899 | `				/* Install the superglobal */` |
|    20342 |  1900 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    10172 |  1901 | `			}else{` |
|        - |  1902 | `				/* Install in the current frame */` |
|      ! 0 |  1903 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1904 | `			}` |
|    20342 |  1905 | `			if( rc == SXRET_OK ){` |
|        - |  1906 | `				SyHashEntry *pRef;` |
|    20342 |  1907 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    20342 |  1908 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    10172 |  1909 | `				}else{` |
|      ! 0 |  1910 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1911 | `				}` |
|        - |  1912 | `				/* Install in the reference table */` |
|    20342 |  1913 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    20342 |  1914 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1915 | `					/* Register in the $GLOBALS array */` |
|    20342 |  1916 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    10170 |  1917 | `				}` |
|    10170 |  1918 | `			}` |
|        - |  1919 | `		}` |
|    20342 |  1920 | `		break;` |
|        - |  1921 | `									}` |
|     1018 |  1922 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1923 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1924 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1925 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1926 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1927 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1928 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2038 |  1929 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2038 |  1930 | `		const char *zValue = va_arg(ap,const char *);` |
|     2038 |  1931 | `		int nLen = va_arg(ap,int);` |
|        - |  1932 | `		ph7_hashmap *pMap;` |
|        - |  1933 | `		ph7_value *pValue;` |
|     2038 |  1934 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1935 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1936 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2037 |  1937 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  1938 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  1939 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2036 |  1940 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  1941 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  1942 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2036 |  1943 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  1944 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  1945 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2036 |  1946 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  1947 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  1948 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2036 |  1949 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  1950 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  1951 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  1952 | `		}else{` |
|        - |  1953 | `			/* Extract the $_SERVER superglobal */` |
|     2036 |  1954 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  1955 | `		}` |
|     2038 |  1956 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1957 | `			/* No such entry */` |
|      ! 0 |  1958 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1959 | `			break;` |
|        - |  1960 | `		}` |
|        - |  1961 | `		/* Point to the hashmap */` |
|     2038 |  1962 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1963 | `		/* Perform the insertion */` |
|     2038 |  1964 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2038 |  1965 | `		break;` |
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
|     2034 |  2016 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2017 | `		/* Register an IO stream device */` |
|     4070 |  2018 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2019 | `		/* Make sure we are dealing with a valid IO stream */` |
|     6102 |  2020 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4070 |  2021 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2022 | `				/* Invalid stream */` |
|      ! 0 |  2023 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2024 | `				break;` |
|        - |  2025 | `		}` |
|     4070 |  2026 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2027 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2036 |  2028 | `			pVm->pDefStream = pStream;` |
|     1017 |  2029 | `		}` |
|        - |  2030 | `		/* Insert in the appropriate container */` |
|     4070 |  2031 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4070 |  2032 | `		break;` |
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
|    32570 |  2069 | `	return rc;` |
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
|      540 |  2128 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2129 |  |
|      541 |  2130 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      541 |  2131 | `	sxi32 rc = SXRET_OK;` |
|        - |  2132 | `	/* Append a new line */` |
|        - |  2133 | `#ifdef __WINNT__` |
|        1 |  2134 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2135 | `#else` |
|      540 |  2136 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2137 | `#endif` |
|        - |  2138 | `	/* Invoke the output consumer callback */` |
|      541 |  2139 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      541 |  2140 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  2141 | `		/* Increment output length */` |
|      541 |  2142 | `		pVm->nOutputLen += SyBlobLength(pMsg);` |
|      270 |  2143 | `	}` |
|      541 |  2144 | `	return rc;` |
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
|      934 |  2346 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2347 |  |
|        - |  2348 | `	VmFrame *pFrame;` |
|        - |  2349 | `	ph7_vm_func *pFunc;` |
|      935 |  2350 | `	*pzFuncName = 0;` |
|      935 |  2351 | `	*pnFuncLen = 0;` |
|      935 |  2352 | `	pFrame = pVm->pFrame;` |
|      935 |  2353 | `	if( pFrame == 0 ){` |
|      ! 0 |  2354 | `		return;` |
|        - |  2355 | `	}` |
|      935 |  2356 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  2357 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  2358 | `	}` |
|      935 |  2359 | `	if( pFrame->pParent == 0 ){` |
|      929 |  2360 | `		return;` |
|        - |  2361 | `	}` |
|        7 |  2362 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        7 |  2363 | `	if( pFunc == 0 ){` |
|      ! 0 |  2364 | `		return;` |
|        - |  2365 | `	}` |
|        7 |  2366 | `	*pzFuncName = pFunc->sName.zString;` |
|        7 |  2367 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      468 |  2368 |  |
|        - |  2369 | `/*` |
|        - |  2370 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2371 | ` */` |
|      470 |  2372 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2373 |  |
|        - |  2374 | `	SyBlob sOut;` |
|        - |  2375 | `	SyString *pFile;` |
|      471 |  2376 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2377 | `		return PH7_OK;` |
|        - |  2378 | `	}` |
|      471 |  2379 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2380 | `		zClass = "Exception";` |
|      ! 0 |  2381 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2382 | `	}` |
|      471 |  2383 | `	if( zMsg == 0 ){` |
|      ! 0 |  2384 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2385 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2386 | `	}` |
|      471 |  2387 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      465 |  2388 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      232 |  2389 | `	}` |
|      471 |  2390 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      471 |  2391 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      471 |  2392 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      471 |  2393 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      471 |  2394 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      471 |  2395 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      471 |  2396 | `	if( pFile ){` |
|      471 |  2397 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      471 |  2398 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2399 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      235 |  2400 | `	}` |
|      471 |  2401 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      471 |  2402 | `	if( pFile ){` |
|      471 |  2403 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      471 |  2404 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2405 | `		if( zFuncName && nFuncLen > 0 ){` |
|        7 |  2406 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        4 |  2407 | `		}else{` |
|      465 |  2408 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2409 | `		}` |
|      235 |  2410 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2411 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2412 | `	}else{` |
|      ! 0 |  2413 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2414 | `	}` |
|      471 |  2415 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      471 |  2416 | `	if( pFile ){` |
|      471 |  2417 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      471 |  2418 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      471 |  2419 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2420 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      235 |  2421 | `	}` |
|      471 |  2422 | `	VmCallErrorHandler(pVm,&sOut);` |
|      471 |  2423 | `	SyBlobRelease(&sOut);` |
|      471 |  2424 | `	return PH7_ABORT;` |
|      236 |  2425 |  |
|        - |  2426 | `/*` |
|        - |  2427 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2428 | ` */` |
|      468 |  2429 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
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
|      470 |  2443 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2444 | `		return PH7_ABORT;` |
|        - |  2445 | `	}` |
|      470 |  2446 | `	pVm = pCtx->pVm;` |
|      470 |  2447 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2448 | `		zClass = "Error";` |
|      ! 0 |  2449 | `	}` |
|      470 |  2450 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      470 |  2451 | `	if( pClass == 0 ){` |
|      ! 0 |  2452 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2453 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2454 | `			zClass` |
|        - |  2455 | `			);` |
|        - |  2456 | `	}` |
|      470 |  2457 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      470 |  2458 | `	if( pThis == 0 ){` |
|      ! 0 |  2459 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2460 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2461 | `			);` |
|        - |  2462 | `	}` |
|        - |  2463 |  |
|      470 |  2464 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      470 |  2465 | `	va_start(ap,zFormat);` |
|      470 |  2466 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      470 |  2467 | `	va_end(ap);` |
|        - |  2468 |  |
|      470 |  2469 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      470 |  2470 | `	if( pCons ){` |
|      470 |  2471 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      470 |  2472 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      470 |  2473 | `		apArg[0] = &sArg;` |
|      470 |  2474 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      470 |  2475 | `		PH7_MemObjRelease(&sArg);` |
|      234 |  2476 | `	}` |
|      470 |  2477 | `	SyBlobRelease(&sMsg);` |
|        - |  2478 |  |
|      470 |  2479 | `	pFrame = pVm->pFrame;` |
|      470 |  2480 | `	if( pFrame ){` |
|      476 |  2481 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        7 |  2482 | `			pFrame = pFrame->pParent;` |
|        1 |  2483 | `		}` |
|      470 |  2484 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      234 |  2485 | `	}` |
|      470 |  2486 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      470 |  2487 | `	PH7_ClassInstanceUnref(pThis);` |
|      470 |  2488 | `	if( rc == SXERR_ABORT ){` |
|      463 |  2489 | `		return PH7_ABORT;` |
|        - |  2490 | `	}` |
|        7 |  2491 | `	return PH7_EXCEPTION;` |
|      236 |  2492 |  |
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
|    29492 |  2545 | `static sxi32 VmByteCodeExec(` |
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
|    29494 |  2561 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    29494 |  2562 | `	if( nTos < 0 ){` |
|    27902 |  2563 | `		pTos = &pStack[-1];` |
|    13952 |  2564 | `	}else{` |
|     1594 |  2565 | `		pTos = &pStack[nTos];` |
|        - |  2566 | `	}` |
|    29494 |  2567 | `	pc = 0;` |
|        - |  2568 | `	/* Execute as much as we can */` |
|  5100582 |  2569 | `	for(;;){` |
|        - |  2570 | `		/* Fetch the instruction to execute */` |
| 10200462 |  2571 | `		pInstr = &aInstr[pc];` |
| 10200462 |  2572 | `		rc = SXRET_OK;` |
|        - |  2573 | `/*` |
|        - |  2574 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2575 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2576 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2577 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2578 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2579 | ` */` |
| 10200462 |  2580 | `		switch(pInstr->iOp){` |
|        - |  2581 | `/*` |
|        - |  2582 | ` * DONE: P1 * *` |
|        - |  2583 | ` *` |
|        - |  2584 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2585 | ` * and return immediately.` |
|        - |  2586 | ` */` |
|    14503 |  2587 | `case PH7_OP_DONE:` |
|    29008 |  2588 | `	if( pInstr->iP1 ){` |
|        - |  2589 | `#ifdef UNTRUST` |
|        - |  2590 | `		if( pTos < pStack ){` |
|        - |  2591 | `			goto Abort;` |
|        - |  2592 | `		}` |
|        - |  2593 | `#endif` |
|    16746 |  2594 | `		if( pLastRef ){` |
|    10794 |  2595 | `			*pLastRef = pTos->nIdx;` |
|     5396 |  2596 | `		}` |
|    16746 |  2597 | `		if( pResult ){` |
|        - |  2598 | `			/* Execution result */` |
|    15954 |  2599 | `			PH7_MemObjStore(pTos,pResult);` |
|     7976 |  2600 | `		}` |
|    16746 |  2601 | `		VmPopOperand(&pTos,1);` |
|    20636 |  2602 | `	}else if( pLastRef ){` |
|        - |  2603 | `		/* Nothing referenced */` |
|      882 |  2604 | `		*pLastRef = SXU32_HIGH;` |
|      440 |  2605 | `	}` |
|    29008 |  2606 | `	goto Done;` |
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
|   223295 |  2654 | `case PH7_OP_JMP:` |
|   446636 |  2655 | `	pc = pInstr->iP2 - 1;` |
|   446636 |  2656 | `	break;` |
|        - |  2657 | `/*` |
|        - |  2658 | ` * JZ: P1 P2 *` |
|        - |  2659 | ` *` |
|        - |  2660 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2661 | ` * entry in the stack if P1 is zero.` |
|        - |  2662 | ` */` |
|   518993 |  2663 | `case PH7_OP_JZ:` |
|        - |  2664 | `#ifdef UNTRUST` |
|        - |  2665 | `	if( pTos < pStack ){` |
|        - |  2666 | `		goto Abort;` |
|        - |  2667 | `	}` |
|        - |  2668 | `#endif` |
|        - |  2669 | `	/* Get a boolean value */` |
|  1038076 |  2670 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      138 |  2671 | `		PH7_MemObjToBool(pTos);` |
|       68 |  2672 | `	}` |
|  1038076 |  2673 | `	if( !pTos->x.iVal ){` |
|        - |  2674 | `		/* Take the jump */` |
|   498644 |  2675 | `		pc = pInstr->iP2 - 1;` |
|   249321 |  2676 | `	}` |
|  1038076 |  2677 | `	if( !pInstr->iP1 ){` |
|   819122 |  2678 | `		VmPopOperand(&pTos,1);` |
|   409582 |  2679 | `	}` |
|  1038076 |  2680 | `	break;` |
|        - |  2681 | `/*` |
|        - |  2682 | ` * JNZ: P1 P2 *` |
|        - |  2683 | ` *` |
|        - |  2684 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2685 | ` * entry in the stack if P1 is zero.` |
|        - |  2686 | ` */` |
|    57524 |  2687 | `case PH7_OP_JNZ:` |
|        - |  2688 | `#ifdef UNTRUST` |
|        - |  2689 | `	if( pTos < pStack ){` |
|        - |  2690 | `		goto Abort;` |
|        - |  2691 | `	}` |
|        - |  2692 | `#endif` |
|        - |  2693 | `	/* Get a boolean value */` |
|   115050 |  2694 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2695 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2696 | `	}` |
|   115050 |  2697 | `	if( pTos->x.iVal ){` |
|        - |  2698 | `		/* Take the jump */` |
|     4168 |  2699 | `		pc = pInstr->iP2 - 1;` |
|     2083 |  2700 | `	}` |
|   115050 |  2701 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2702 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2703 | `	}` |
|   115050 |  2704 | `	break;` |
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
|   395972 |  2718 | `case PH7_OP_POP: {` |
|   791990 |  2719 | `	sxi32 n = pInstr->iP1;` |
|   791990 |  2720 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2721 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2722 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2723 | `	}` |
|   791990 |  2724 | `	VmPopOperand(&pTos,n);` |
|   791990 |  2725 | `	break;` |
|        - |  2726 | `				 }` |
|        - |  2727 | `/*` |
|        - |  2728 | ` * DUP: * * *` |
|        - |  2729 | ` *` |
|        - |  2730 | ` * Duplicate the top of the stack.` |
|        - |  2731 | ` */` |
|       33 |  2732 | `case PH7_OP_DUP:` |
|        - |  2733 | `#ifdef UNTRUST` |
|        - |  2734 | `	if( pTos < pStack ){` |
|        - |  2735 | `		goto Abort;` |
|        - |  2736 | `	}` |
|        - |  2737 | `#endif` |
|       68 |  2738 | `	pTos++;` |
|       68 |  2739 | `	PH7_MemObjInit(pVm,pTos);` |
|       68 |  2740 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       68 |  2741 | `	break;` |
|        - |  2742 | `/*` |
|        - |  2743 | ` * CVT_INT: * * *` |
|        - |  2744 | ` *` |
|        - |  2745 | ` * Force the top of the stack to be an integer.` |
|        - |  2746 | ` */` |
|       35 |  2747 | `case PH7_OP_CVT_INT:` |
|        - |  2748 | `#ifdef UNTRUST` |
|        - |  2749 | `	if( pTos < pStack ){` |
|        - |  2750 | `		goto Abort;` |
|        - |  2751 | `	}` |
|        - |  2752 | `#endif` |
|       72 |  2753 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2754 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2755 | `	}` |
|        - |  2756 | `	/* Invalidate any prior representation */` |
|       72 |  2757 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2758 | `	break;` |
|        - |  2759 | `/*` |
|        - |  2760 | ` * CVT_REAL: * * *` |
|        - |  2761 | ` *` |
|        - |  2762 | ` * Force the top of the stack to be a real.` |
|        - |  2763 | ` */` |
|        4 |  2764 | `case PH7_OP_CVT_REAL:` |
|        - |  2765 | `#ifdef UNTRUST` |
|        - |  2766 | `	if( pTos < pStack ){` |
|        - |  2767 | `		goto Abort;` |
|        - |  2768 | `	}` |
|        - |  2769 | `#endif` |
|        9 |  2770 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2771 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2772 | `	}` |
|        - |  2773 | `	/* Invalidate any prior representation */` |
|        9 |  2774 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2775 | `	break;` |
|        - |  2776 | `/*` |
|        - |  2777 | ` * CVT_STR: * * *` |
|        - |  2778 | ` *` |
|        - |  2779 | ` * Force the top of the stack to be a string.` |
|        - |  2780 | ` */` |
|      146 |  2781 | `case PH7_OP_CVT_STR:` |
|        - |  2782 | `#ifdef UNTRUST` |
|        - |  2783 | `	if( pTos < pStack ){` |
|        - |  2784 | `		goto Abort;` |
|        - |  2785 | `	}` |
|        - |  2786 | `#endif` |
|      294 |  2787 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  2788 | `		PH7_MemObjToString(pTos);` |
|      146 |  2789 | `	}` |
|      294 |  2790 | `	break;` |
|        - |  2791 | `/*` |
|        - |  2792 | ` * CVT_BOOL: * * *` |
|        - |  2793 | ` *` |
|        - |  2794 | ` * Force the top of the stack to be a boolean.` |
|        - |  2795 | ` */` |
|        5 |  2796 | `case PH7_OP_CVT_BOOL:` |
|        - |  2797 | `#ifdef UNTRUST` |
|        - |  2798 | `	if( pTos < pStack ){` |
|        - |  2799 | `		goto Abort;` |
|        - |  2800 | `	}` |
|        - |  2801 | `#endif` |
|       11 |  2802 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2803 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2804 | `	}` |
|       11 |  2805 | `	break;` |
|        - |  2806 | `/*` |
|        - |  2807 | ` * CVT_NULL: * * *` |
|        - |  2808 | ` *` |
|        - |  2809 | ` * Nullify the top of the stack.` |
|        - |  2810 | ` */` |
|        3 |  2811 | `case PH7_OP_CVT_NULL:` |
|        - |  2812 | `#ifdef UNTRUST` |
|        - |  2813 | `	if( pTos < pStack ){` |
|        - |  2814 | `		goto Abort;` |
|        - |  2815 | `	}` |
|        - |  2816 | `#endif` |
|        7 |  2817 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2818 | `	break;` |
|        - |  2819 | `/*` |
|        - |  2820 | ` * CVT_NUMC: * * *` |
|        - |  2821 | ` *` |
|        - |  2822 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2823 | ` */` |
|      ! 0 |  2824 | `case PH7_OP_CVT_NUMC:` |
|        - |  2825 | `#ifdef UNTRUST` |
|        - |  2826 | `	if( pTos < pStack ){` |
|        - |  2827 | `		goto Abort;` |
|        - |  2828 | `	}` |
|        - |  2829 | `#endif` |
|        - |  2830 | `	/* Force a numeric cast */` |
|      ! 0 |  2831 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2832 | `	break;` |
|        - |  2833 | `/*` |
|        - |  2834 | ` * CVT_ARRAY: * * *` |
|        - |  2835 | ` *` |
|        - |  2836 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2837 | ` */` |
|       10 |  2838 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2839 | `#ifdef UNTRUST` |
|        - |  2840 | `	if( pTos < pStack ){` |
|        - |  2841 | `		goto Abort;` |
|        - |  2842 | `	}` |
|        - |  2843 | `#endif` |
|        - |  2844 | `	/* Force a hashmap cast */` |
|       21 |  2845 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  2846 | `	if( rc != SXRET_OK ){` |
|        - |  2847 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  2848 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  2849 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  2850 | `	}` |
|       21 |  2851 | `	break;` |
|        - |  2852 | `/*` |
|        - |  2853 | ` * CVT_OBJ: * * *` |
|        - |  2854 | ` *` |
|        - |  2855 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  2856 | ` */` |
|        8 |  2857 | `case PH7_OP_CVT_OBJ:` |
|        - |  2858 | `#ifdef UNTRUST` |
|        - |  2859 | `	if( pTos < pStack ){` |
|        - |  2860 | `		goto Abort;` |
|        - |  2861 | `	}` |
|        - |  2862 | `#endif` |
|       17 |  2863 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  2864 | `		/* Force a 'stdClass()' cast */` |
|       17 |  2865 | `		PH7_MemObjToObject(pTos);` |
|        8 |  2866 | `	}` |
|       17 |  2867 | `	break;` |
|        - |  2868 | `/*` |
|        - |  2869 | ` * ERR_CTRL * * *` |
|        - |  2870 | ` *` |
|        - |  2871 | ` * Error control operator.` |
|        - |  2872 | ` */` |
|    11993 |  2873 | `case PH7_OP_ERR_CTRL:` |
|        - |  2874 | `	/*` |
|        - |  2875 | `	 * TICKET 1433-038:` |
|        - |  2876 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2877 | `	 * use the public API,to control error output.` |
|        - |  2878 | `	 */` |
|    23986 |  2879 | `	break;` |
|        - |  2880 | `/*` |
|        - |  2881 | ` * IS_A * * *` |
|        - |  2882 | ` *` |
|        - |  2883 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  2884 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  2885 | ` * holding a class name or an object).` |
|        - |  2886 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  2887 | ` */` |
|       11 |  2888 | `case PH7_OP_IS_A:{` |
|       23 |  2889 | `	ph7_value *pNos = &pTos[-1];` |
|       23 |  2890 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  2891 | `#ifdef UNTRUST` |
|        - |  2892 | `	if( pNos < pStack ){` |
|        - |  2893 | `		goto Abort;` |
|        - |  2894 | `	}` |
|        - |  2895 | `#endif` |
|       23 |  2896 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       21 |  2897 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       21 |  2898 | `		ph7_class *pClass = 0;` |
|        - |  2899 | `		/* Extract the target class */` |
|       21 |  2900 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  2901 | `			/* Instance already loaded */` |
|      ! 0 |  2902 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       21 |  2903 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2904 | `			/* Perform the query */` |
|       31 |  2905 | `			pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|       20 |  2906 | `				SyBlobLength(&pTos->sBlob),FALSE,0);` |
|       10 |  2907 | `		}` |
|       21 |  2908 | `		if( pClass ){` |
|        - |  2909 | `			/* Perform the query */` |
|       21 |  2910 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       10 |  2911 | `		}` |
|       10 |  2912 | `	}` |
|        - |  2913 | `	/* Push result */` |
|       23 |  2914 | `	VmPopOperand(&pTos,1);` |
|       23 |  2915 | `	PH7_MemObjRelease(pTos);` |
|       23 |  2916 | `	pTos->x.iVal = iRes;` |
|       23 |  2917 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       23 |  2918 | `	break;` |
|        - |  2919 | `				 }` |
|        - |  2920 |  |
|        - |  2921 | `/*` |
|        - |  2922 | ` * LOADC P1 P2 *` |
|        - |  2923 | ` *` |
|        - |  2924 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  2925 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  2926 | ` */` |
|   798469 |  2927 | `case PH7_OP_LOADC: {` |
|        - |  2928 | `	ph7_value *pObj;` |
|        - |  2929 | `	/* Reserve a room */` |
|  1596984 |  2930 | `	pTos++;` |
|  1596984 |  2931 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1596984 |  2932 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2933 | `			SyHashEntry *pEntry;` |
|        - |  2934 | `			/* Candidate for expansion via user defined callbacks */` |
|    19104 |  2935 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19104 |  2936 | `			if( pEntry ){` |
|    15314 |  2937 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2938 | `				/* Set a NULL default value */` |
|    15314 |  2939 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    15314 |  2940 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2941 | `				/* Invoke the callback and deal with the expanded value */` |
|    15314 |  2942 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  2943 | `				/* Mark as constant */` |
|    15314 |  2944 | `				pTos->nIdx = SXU32_HIGH;` |
|    15314 |  2945 | `				break;` |
|        - |  2946 | `			}` |
|     1895 |  2947 | `		}` |
|  1581672 |  2948 | `		PH7_MemObjLoad(pObj,pTos);` |
|   790859 |  2949 | `	}else{` |
|        - |  2950 | `		/* Set a NULL value */` |
|      ! 0 |  2951 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2952 | `	}` |
|        - |  2953 | `	/* Mark as constant */` |
|  1581672 |  2954 | `	pTos->nIdx = SXU32_HIGH;` |
|  1581672 |  2955 | `	break;` |
|        - |  2956 | `				  }` |
|        - |  2957 | `/*` |
|        - |  2958 | ` * LOAD: P1 * P3` |
|        - |  2959 | ` *` |
|        - |  2960 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  2961 | ` * from the P3 operand.` |
|        - |  2962 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  2963 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  2964 | ` */` |
|  1425797 |  2965 | `case PH7_OP_LOAD:{` |
|        - |  2966 | `	ph7_value *pObj;` |
|        - |  2967 | `	SyString sName;` |
|  2851816 |  2968 | `	if( pInstr->p3 == 0 ){` |
|        - |  2969 | `		/* Take the variable name from the top of the stack */` |
|        - |  2970 | `#ifdef UNTRUST` |
|        - |  2971 | `		if( pTos < pStack ){` |
|        - |  2972 | `			goto Abort;` |
|        - |  2973 | `		}` |
|        - |  2974 | `#endif` |
|        - |  2975 | `		/* Force a string cast */` |
|       19 |  2976 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2977 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  2978 | `		}` |
|       19 |  2979 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  2980 | `	}else{` |
|  2851798 |  2981 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  2982 | `		/* Reserve a room for the target object */` |
|  2851798 |  2983 | `		pTos++;` |
|        - |  2984 | `	}` |
|        - |  2985 | `	/* Extract the requested memory object */` |
|  2851816 |  2986 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2851816 |  2987 | `	if( pObj == 0 ){` |
|      626 |  2988 | `		if( pInstr->iP1 ){` |
|        - |  2989 | `			/* Variable not found,load NULL */` |
|      626 |  2990 | `			if( !pInstr->p3 ){` |
|      ! 0 |  2991 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  2992 | `			}else{` |
|      626 |  2993 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2994 | `			}` |
|      626 |  2995 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1426111 |  2996 | `			break;` |
|      ! 0 |  2997 | `		}else{` |
|        - |  2998 | `			/* Fatal error */` |
|      ! 0 |  2999 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3000 | `			goto Abort;` |
|        - |  3001 | `		}` |
|        - |  3002 | `	}` |
|        - |  3003 | `	/* Load variable contents */` |
|  2851192 |  3004 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2851192 |  3005 | `	pTos->nIdx = pObj->nIdx;` |
|  2851192 |  3006 | `	break;` |
|        - |  3007 | `				   }` |
|        - |  3008 | `/*` |
|        - |  3009 | ` * LOAD_MAP P1 * *` |
|        - |  3010 | ` *` |
|        - |  3011 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3012 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3013 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3014 | ` */` |
|    17313 |  3015 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3016 | `	ph7_hashmap *pMap;` |
|        - |  3017 | `	/* Allocate a new hashmap instance */` |
|    34628 |  3018 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    34628 |  3019 | `	if( pMap == 0 ){` |
|      ! 0 |  3020 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3021 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3022 | `		goto Abort;` |
|        - |  3023 | `	}` |
|    34628 |  3024 | `	if( pInstr->iP1 > 0 ){` |
|     2080 |  3025 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3026 | `		/* Perform the insertion */` |
|     6302 |  3027 | `		while( pEntry < pTos ){` |
|     4224 |  3028 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3029 | `				/* Insertion by reference */` |
|      142 |  3030 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3031 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3032 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3033 | `					);` |
|       48 |  3034 | `			}else{` |
|        - |  3035 | `				/* Standard insertion */` |
|     6194 |  3036 | `				PH7_HashmapInsert(pMap,` |
|     4128 |  3037 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2064 |  3038 | `					&pEntry[1]` |
|        - |  3039 | `				);` |
|        - |  3040 | `			}` |
|        - |  3041 | `			/* Next pair on the stack */` |
|     4224 |  3042 | `			pEntry += 2;` |
|        2 |  3043 | `		}` |
|        - |  3044 | `		/* Pop P1 elements */` |
|     2080 |  3045 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1039 |  3046 | `	}` |
|        - |  3047 | `	/* Push the hashmap */` |
|    34628 |  3048 | `	pTos++;` |
|    34628 |  3049 | `	pTos->nIdx = SXU32_HIGH;` |
|    34628 |  3050 | `	pTos->x.pOther = pMap;` |
|    34628 |  3051 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    34628 |  3052 | `	break;` |
|        - |  3053 | `					  }` |
|        - |  3054 | `/*` |
|        - |  3055 | ` * LOAD_LIST: P1 * *` |
|        - |  3056 | ` *` |
|        - |  3057 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3058 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3059 | ` * Caveats:` |
|        - |  3060 | ` *  This implementation support only a single nesting level.` |
|        - |  3061 | ` */` |
|       17 |  3062 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3063 | `	ph7_value *pEntry;` |
|       35 |  3064 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3065 | `		/* Empty list,break immediately */` |
|      ! 0 |  3066 | `		break;` |
|        - |  3067 | `	}` |
|       35 |  3068 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3069 | `#ifdef UNTRUST` |
|        - |  3070 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3071 | `		goto Abort;` |
|        - |  3072 | `	}` |
|        - |  3073 | `#endif` |
|       35 |  3074 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       31 |  3075 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3076 | `		ph7_hashmap_node *pNode;` |
|        - |  3077 | `		ph7_value sKey,*pObj;` |
|        - |  3078 | `		/* Start Copying */` |
|       31 |  3079 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|       99 |  3080 | `		while( pEntry <= pTos ){` |
|       69 |  3081 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       65 |  3082 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       65 |  3083 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       65 |  3084 | `					if( rc == SXRET_OK ){` |
|        - |  3085 | `						/* Store node value */` |
|       65 |  3086 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       33 |  3087 | `					}else{` |
|        - |  3088 | `						/* Nullify the variable */` |
|      ! 0 |  3089 | `						PH7_MemObjRelease(pObj);` |
|        - |  3090 | `					}` |
|       32 |  3091 | `				}` |
|       32 |  3092 | `			}` |
|       69 |  3093 | `			sKey.x.iVal++; /* Next numeric index */` |
|       69 |  3094 | `			pEntry++;` |
|        1 |  3095 | `		}` |
|       15 |  3096 | `	}` |
|       35 |  3097 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       35 |  3098 | `	break;` |
|        - |  3099 | `					   }` |
|        - |  3100 | `/*` |
|        - |  3101 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3102 | ` *` |
|        - |  3103 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3104 | ` * from the stack.` |
|        - |  3105 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3106 | ` * instead.` |
|        - |  3107 | ` */` |
|   239415 |  3108 | `case PH7_OP_LOAD_IDX: {` |
|   478876 |  3109 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   478876 |  3110 | `	ph7_hashmap *pMap = 0;` |
|        - |  3111 | `	ph7_value *pIdx;` |
|   478876 |  3112 | `	pIdx = 0;` |
|   478876 |  3113 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3114 | `		if( !pInstr->iP2){` |
|        - |  3115 | `			/* No available index,load NULL */` |
|      ! 0 |  3116 | `			if( pTos >= pStack ){` |
|      ! 0 |  3117 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3118 | `			}else{` |
|        - |  3119 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3120 | `				pTos++;` |
|      ! 0 |  3121 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3122 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3123 | `			}` |
|        - |  3124 | `			/* Emit a notice */` |
|      ! 0 |  3125 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3126 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3127 | `			break;` |
|        - |  3128 | `		}` |
|      ! 0 |  3129 | `	}else{` |
|   478876 |  3130 | `		pIdx = pTos;` |
|   478876 |  3131 | `		pTos--;` |
|        - |  3132 | `	}` |
|   478876 |  3133 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3134 | `		/* String access */` |
|   393100 |  3135 | `		if( pIdx ){` |
|        - |  3136 | `			sxu32 nOfft;` |
|   393100 |  3137 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3138 | `				/* Force an int cast */` |
|      ! 0 |  3139 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3140 | `			}` |
|   393100 |  3141 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   393100 |  3142 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3143 | `				/* Invalid offset,load null */` |
|      ! 0 |  3144 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3145 | `			}else{` |
|   393100 |  3146 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   393100 |  3147 | `				int c = zData[nOfft];` |
|   393100 |  3148 | `				PH7_MemObjRelease(pTos);` |
|   393100 |  3149 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   393100 |  3150 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3151 | `			}` |
|   196573 |  3152 | `		}else{` |
|        - |  3153 | `			/* No available index,load NULL */` |
|      ! 0 |  3154 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3155 | `		}` |
|   393100 |  3156 | `		break;` |
|        - |  3157 | `	}` |
|    85778 |  3158 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3159 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3160 | `			ph7_value *pObj;` |
|      ! 0 |  3161 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3162 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3163 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3164 | `			}` |
|      ! 0 |  3165 | `		}` |
|      ! 0 |  3166 | `	}` |
|    85778 |  3167 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    85778 |  3168 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3169 | `		/* Point to the hashmap */` |
|    85778 |  3170 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    85778 |  3171 | `		if( pIdx ){` |
|        - |  3172 | `			/* Load the desired entry */` |
|    85778 |  3173 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    42888 |  3174 | `		}` |
|    85778 |  3175 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3176 | `			/* Create a new empty entry */` |
|      ! 0 |  3177 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3178 | `			if( rc == SXRET_OK ){` |
|        - |  3179 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3180 | `				pNode = pMap->pLast;` |
|      ! 0 |  3181 | `			}` |
|      ! 0 |  3182 | `		}` |
|    42888 |  3183 | `	}` |
|    85778 |  3184 | `	if( pIdx ){` |
|    85778 |  3185 | `		PH7_MemObjRelease(pIdx);` |
|    42888 |  3186 | `	}` |
|    85778 |  3187 | `	if( rc == SXRET_OK ){` |
|        - |  3188 | `		/* Load entry contents */` |
|    39298 |  3189 | `		if( pMap->iRef < 2 ){` |
|        - |  3190 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3191 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3192 | `			 */` |
|        7 |  3193 | `			pTos->nIdx = SXU32_HIGH;` |
|        7 |  3194 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|        4 |  3195 | `		}else{` |
|    39292 |  3196 | `			pTos->nIdx = pNode->nValIdx;` |
|    39292 |  3197 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    39292 |  3198 | `			PH7_HashmapUnref(pMap);` |
|        - |  3199 | `		}` |
|    19650 |  3200 | `	}else{` |
|        - |  3201 | `		/* No such entry,load NULL */` |
|    46482 |  3202 | `		PH7_MemObjRelease(pTos);` |
|    46482 |  3203 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3204 | `	}` |
|    85778 |  3205 | `	break;` |
|        - |  3206 | `					  }` |
|        - |  3207 | `/*` |
|        - |  3208 | ` * LOAD_CLOSURE * * P3` |
|        - |  3209 | ` *` |
|        - |  3210 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3211 | ` * name in the stack.` |
|        - |  3212 | ` */` |
|        2 |  3213 | `case PH7_OP_LOAD_CLOSURE:{` |
|        5 |  3214 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        5 |  3215 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3216 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3217 | `		ph7_vm_func *pClosure;` |
|        - |  3218 | `		char *zName;` |
|        - |  3219 | `		sxu32 mLen;` |
|        - |  3220 | `		sxu32 n;` |
|        - |  3221 | `		/* Create a new VM function */` |
|        5 |  3222 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3223 | `		/* Generate an unique closure name */` |
|        5 |  3224 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        5 |  3225 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3226 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3227 | `			goto Abort;` |
|        - |  3228 | `		}` |
|        5 |  3229 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        5 |  3230 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3231 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3232 | `		}` |
|        - |  3233 | `		/* Zero the stucture */` |
|        5 |  3234 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3235 | `		/* Perform a structure assignment on read-only items */` |
|        5 |  3236 | `		pClosure->aArgs = pFunc->aArgs;` |
|        5 |  3237 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        5 |  3238 | `		pClosure->aStatic = pFunc->aStatic;` |
|        5 |  3239 | `		pClosure->iFlags = pFunc->iFlags;` |
|        5 |  3240 | `		pClosure->pUserData = pFunc->pUserData;` |
|        5 |  3241 | `		pClosure->sSignature = pFunc->sSignature;` |
|        5 |  3242 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3243 | `		/* Register the closure */` |
|        5 |  3244 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3245 | `		/* Set up closure environment */` |
|        5 |  3246 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        5 |  3247 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       13 |  3248 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3249 | `			ph7_value *pValue;` |
|        9 |  3250 | `			pEnv = &aEnv[n];` |
|        9 |  3251 | `			sEnv.sName  = pEnv->sName;` |
|        9 |  3252 | `			sEnv.iFlags = pEnv->iFlags;` |
|        9 |  3253 | `			sEnv.nIdx = SXU32_HIGH;` |
|        9 |  3254 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|        9 |  3255 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3256 | `				/* Pass by reference */` |
|      ! 0 |  3257 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3258 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3259 | `					);` |
|      ! 0 |  3260 | `			}` |
|        - |  3261 | `			/* Standard pass by value */` |
|        9 |  3262 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|        9 |  3263 | `			if( pValue ){` |
|        - |  3264 | `				/* Copy imported value */` |
|        5 |  3265 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        2 |  3266 | `			}` |
|        - |  3267 | `			/* Insert the imported variable */` |
|        9 |  3268 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        5 |  3269 | `		}` |
|        - |  3270 | `		/* Finally,load the closure name on the stack */` |
|        5 |  3271 | `		pTos++;` |
|        5 |  3272 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        2 |  3273 | `	}` |
|        5 |  3274 | `	break;` |
|        - |  3275 | `						 }` |
|        - |  3276 | `/*` |
|        - |  3277 | ` * STORE * P2 P3` |
|        - |  3278 | ` *` |
|        - |  3279 | ` * Perform a store (Assignment) operation.` |
|        - |  3280 | ` */` |
|   106438 |  3281 | `case PH7_OP_STORE: {` |
|        - |  3282 | `	ph7_value *pObj;` |
|        - |  3283 | `	SyString sName;` |
|        - |  3284 | `#ifdef UNTRUST` |
|        - |  3285 | `	if( pTos < pStack ){` |
|        - |  3286 | `		goto Abort;` |
|        - |  3287 | `	}` |
|        - |  3288 | `#endif` |
|   212878 |  3289 | `	if( pInstr->iP2 ){` |
|        - |  3290 | `		sxu32 nIdx;` |
|        - |  3291 | `		/* Member store operation */` |
|     2838 |  3292 | `		nIdx = pTos->nIdx;` |
|     2838 |  3293 | `		VmPopOperand(&pTos,1);` |
|     2838 |  3294 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3295 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3296 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3297 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3298 | `		}else{` |
|        - |  3299 | `			/* Point to the desired memory object */` |
|     2834 |  3300 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2834 |  3301 | `			if( pObj ){` |
|        - |  3302 | `				/* Perform the store operation */` |
|     2834 |  3303 | `				PH7_MemObjStore(pTos,pObj);` |
|     1416 |  3304 | `			}` |
|        - |  3305 | `		}` |
|   107858 |  3306 | `		break;` |
|   210042 |  3307 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3308 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3309 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3310 | `			/* Force a string cast */` |
|      ! 0 |  3311 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3312 | `		}` |
|        7 |  3313 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3314 | `		pTos--;` |
|        - |  3315 | `#ifdef UNTRUST` |
|        - |  3316 | `		if( pTos < pStack  ){` |
|        - |  3317 | `			goto Abort;` |
|        - |  3318 | `		}` |
|        - |  3319 | `#endif` |
|        4 |  3320 | `	}else{` |
|   210036 |  3321 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3322 | `	}` |
|        - |  3323 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   210042 |  3324 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   210042 |  3325 | `	if( pObj == 0 ){` |
|      ! 0 |  3326 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3327 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3328 | `		goto Abort;` |
|        - |  3329 | `	}` |
|   210042 |  3330 | `	if( !pInstr->p3 ){` |
|        7 |  3331 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3332 | `	}` |
|        - |  3333 | `	/* Perform the store operation */` |
|   210042 |  3334 | `	PH7_MemObjStore(pTos,pObj);` |
|   210042 |  3335 | `	break;` |
|        - |  3336 | `				   }` |
|        - |  3337 | `/*` |
|        - |  3338 | ` * STORE_IDX:   P1 * P3` |
|        - |  3339 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3340 | ` *` |
|        - |  3341 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3342 | ` */` |
|    78083 |  3343 | `case PH7_OP_STORE_IDX:` |
|        - |  3344 | `case PH7_OP_STORE_IDX_REF: {` |
|   156168 |  3345 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3346 | `	ph7_value *pKey;` |
|        - |  3347 | `	sxu32 nIdx;` |
|   156168 |  3348 | `	if( pInstr->iP1 ){` |
|        - |  3349 | `		/* Key is next on stack */` |
|    56050 |  3350 | `		pKey = pTos;` |
|    56050 |  3351 | `		pTos--;` |
|    28026 |  3352 | `	}else{` |
|   100120 |  3353 | `		pKey = 0;` |
|        - |  3354 | `	}` |
|   156168 |  3355 | `	nIdx = pTos->nIdx;` |
|   156168 |  3356 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3357 | `		/* Hashmap already loaded */` |
|   156116 |  3358 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   156116 |  3359 | `		if( pMap->iRef < 2 ){` |
|        - |  3360 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3361 | `			pMap->iRef = 2;` |
|      ! 0 |  3362 | `		}` |
|    78059 |  3363 | `	}else{` |
|        - |  3364 | `		ph7_value *pObj;` |
|       53 |  3365 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3366 | `		if( pObj == 0 ){` |
|      ! 0 |  3367 | `			if( pKey ){` |
|      ! 0 |  3368 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3369 | `			}` |
|      ! 0 |  3370 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3371 | `			break;` |
|        - |  3372 | `		}` |
|        - |  3373 | `		/* Phase#1: Load the array */` |
|       53 |  3374 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3375 | `			VmPopOperand(&pTos,1);` |
|       53 |  3376 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3377 | `				/* Force a string cast */` |
|      ! 0 |  3378 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3379 | `			}` |
|       53 |  3380 | `			if( pKey == 0 ){` |
|        - |  3381 | `				/* Append string */` |
|        3 |  3382 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3383 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3384 | `				}` |
|        2 |  3385 | `			}else{` |
|        - |  3386 | `				sxu32 nOfft;` |
|       51 |  3387 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3388 | `					/* Force an int cast */` |
|       51 |  3389 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3390 | `				}` |
|       51 |  3391 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3392 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3393 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3394 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3395 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3396 | `				}else{` |
|      ! 0 |  3397 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3398 | `						/* Perform an append operation */` |
|      ! 0 |  3399 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3400 | `					}` |
|        - |  3401 | `				}` |
|        - |  3402 | `			}` |
|       53 |  3403 | `			if( pKey ){` |
|       51 |  3404 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3405 | `			}` |
|       53 |  3406 | `			break;` |
|      ! 0 |  3407 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3408 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3409 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3410 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3411 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3412 | `				goto Abort;` |
|        - |  3413 | `			}` |
|      ! 0 |  3414 | `		}` |
|      ! 0 |  3415 | `		pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - |  3416 | `	}` |
|   156116 |  3417 | `	VmPopOperand(&pTos,1);` |
|        - |  3418 | `	/* Phase#2: Perform the insertion */` |
|   156116 |  3419 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3420 | `		/* Insertion by reference */` |
|       15 |  3421 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3422 | `	}else{` |
|   156102 |  3423 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3424 | `	}` |
|   156116 |  3425 | `	if( pKey ){` |
|    56000 |  3426 | `		PH7_MemObjRelease(pKey);` |
|    27999 |  3427 | `	}` |
|   156116 |  3428 | `	break;` |
|        - |  3429 | `					   }` |
|        - |  3430 | `/*` |
|        - |  3431 | ` * INCR: P1 * *` |
|        - |  3432 | ` *` |
|        - |  3433 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3434 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3435 | ` * the stack and increment after that.` |
|        - |  3436 | ` */` |
|   172952 |  3437 | `case PH7_OP_INCR:` |
|        - |  3438 | `#ifdef UNTRUST` |
|        - |  3439 | `	if( pTos < pStack ){` |
|        - |  3440 | `		goto Abort;` |
|        - |  3441 | `	}` |
|        - |  3442 | `#endif` |
|   345950 |  3443 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   345950 |  3444 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3445 | `			ph7_value *pObj;` |
|   345950 |  3446 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3447 | `				/* Force a numeric cast */` |
|   345950 |  3448 | `				PH7_MemObjToNumeric(pObj);` |
|   345950 |  3449 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3450 | `					pObj->rVal++;` |
|        - |  3451 | `					/* Try to get an integer representation */` |
|      ! 0 |  3452 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3453 | `				}else{` |
|   345950 |  3454 | `					pObj->x.iVal++;` |
|   345950 |  3455 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3456 | `				}` |
|   345950 |  3457 | `				if( pInstr->iP1 ){` |
|        - |  3458 | `					/* Pre-icrement */` |
|       71 |  3459 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3460 | `				}` |
|   172996 |  3461 | `			}` |
|   172998 |  3462 | `		}else{` |
|      ! 0 |  3463 | `			if( pInstr->iP1 ){` |
|        - |  3464 | `				/* Force a numeric cast */` |
|      ! 0 |  3465 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3466 | `				/* Pre-increment */` |
|      ! 0 |  3467 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3468 | `					pTos->rVal++;` |
|        - |  3469 | `					/* Try to get an integer representation */` |
|      ! 0 |  3470 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3471 | `				}else{` |
|      ! 0 |  3472 | `					pTos->x.iVal++;` |
|      ! 0 |  3473 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3474 | `				}` |
|      ! 0 |  3475 | `			}` |
|        - |  3476 | `		}` |
|   172996 |  3477 | `	}` |
|   345950 |  3478 | `	break;` |
|        - |  3479 | `/*` |
|        - |  3480 | ` * DECR: P1 * *` |
|        - |  3481 | ` *` |
|        - |  3482 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3483 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3484 | ` * and decrement after that.` |
|        - |  3485 | ` */` |
|        2 |  3486 | `case PH7_OP_DECR:` |
|        - |  3487 | `#ifdef UNTRUST` |
|        - |  3488 | `	if( pTos < pStack ){` |
|        - |  3489 | `		goto Abort;` |
|        - |  3490 | `	}` |
|        - |  3491 | `#endif` |
|        5 |  3492 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3493 | `		/* Force a numeric cast */` |
|        5 |  3494 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3495 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3496 | `			ph7_value *pObj;` |
|        5 |  3497 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3498 | `				/* Force a numeric cast */` |
|        5 |  3499 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3500 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3501 | `					pObj->rVal--;` |
|        - |  3502 | `					/* Try to get an integer representation */` |
|      ! 0 |  3503 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3504 | `				}else{` |
|        5 |  3505 | `					pObj->x.iVal--;` |
|        5 |  3506 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3507 | `				}` |
|        5 |  3508 | `				if( pInstr->iP1 ){` |
|        - |  3509 | `					/* Pre-icrement */` |
|      ! 0 |  3510 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3511 | `				}` |
|        2 |  3512 | `			}` |
|        3 |  3513 | `		}else{` |
|      ! 0 |  3514 | `			if( pInstr->iP1 ){` |
|        - |  3515 | `				/* Pre-increment */` |
|      ! 0 |  3516 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3517 | `					pTos->rVal--;` |
|        - |  3518 | `					/* Try to get an integer representation */` |
|      ! 0 |  3519 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3520 | `				}else{` |
|      ! 0 |  3521 | `					pTos->x.iVal--;` |
|      ! 0 |  3522 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3523 | `				}` |
|      ! 0 |  3524 | `			}` |
|        - |  3525 | `		}` |
|        2 |  3526 | `	}` |
|        5 |  3527 | `	break;` |
|        - |  3528 | `/*` |
|        - |  3529 | ` * UMINUS: * * *` |
|        - |  3530 | ` *` |
|        - |  3531 | ` * Perform a unary minus operation.` |
|        - |  3532 | ` */` |
|    22387 |  3533 | `case PH7_OP_UMINUS:` |
|        - |  3534 | `#ifdef UNTRUST` |
|        - |  3535 | `	if( pTos < pStack ){` |
|        - |  3536 | `		goto Abort;` |
|        - |  3537 | `	}` |
|        - |  3538 | `#endif` |
|        - |  3539 | `	/* Force a numeric (integer,real or both) cast */` |
|    44776 |  3540 | `	PH7_MemObjToNumeric(pTos);` |
|    44776 |  3541 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       32 |  3542 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3543 | `	}` |
|    44776 |  3544 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    44746 |  3545 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    22372 |  3546 | `	}` |
|    44776 |  3547 | `	break;` |
|        - |  3548 | `/*` |
|        - |  3549 | ` * UPLUS: * * *` |
|        - |  3550 | ` *` |
|        - |  3551 | ` * Perform a unary plus operation.` |
|        - |  3552 | ` */` |
|       16 |  3553 | `case PH7_OP_UPLUS:` |
|        - |  3554 | `#ifdef UNTRUST` |
|        - |  3555 | `	if( pTos < pStack ){` |
|        - |  3556 | `		goto Abort;` |
|        - |  3557 | `	}` |
|        - |  3558 | `#endif` |
|        - |  3559 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3560 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3561 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3562 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3563 | `	}` |
|       33 |  3564 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3565 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3566 | `	}` |
|       33 |  3567 | `	break;` |
|        - |  3568 | `/*` |
|        - |  3569 | ` * OP_LNOT: * * *` |
|        - |  3570 | ` *` |
|        - |  3571 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3572 | ` * with its complement.` |
|        - |  3573 | ` */` |
|    52017 |  3574 | `case PH7_OP_LNOT:` |
|        - |  3575 | `#ifdef UNTRUST` |
|        - |  3576 | `	if( pTos < pStack ){` |
|        - |  3577 | `		goto Abort;` |
|        - |  3578 | `	}` |
|        - |  3579 | `#endif` |
|        - |  3580 | `	/* Force a boolean cast */` |
|   104080 |  3581 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3582 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3583 | `	}` |
|   104080 |  3584 | `	pTos->x.iVal = !pTos->x.iVal;` |
|   104080 |  3585 | `	break;` |
|        - |  3586 | `/*` |
|        - |  3587 | ` * OP_BITNOT: * * *` |
|        - |  3588 | ` *` |
|        - |  3589 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3590 | ` * with its ones-complement.` |
|        - |  3591 | ` */` |
|       14 |  3592 | `case PH7_OP_BITNOT:` |
|        - |  3593 | `#ifdef UNTRUST` |
|        - |  3594 | `	if( pTos < pStack ){` |
|        - |  3595 | `		goto Abort;` |
|        - |  3596 | `	}` |
|        - |  3597 | `#endif` |
|        - |  3598 | `	/* Force an integer cast */` |
|       30 |  3599 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3600 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3601 | `	}` |
|       30 |  3602 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  3603 | `	break;` |
|        - |  3604 | `/* OP_MUL * * *` |
|        - |  3605 | ` * OP_MUL_STORE * * *` |
|        - |  3606 | ` *` |
|        - |  3607 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3608 | ` * and push the result back onto the stack.` |
|        - |  3609 | ` */` |
|     1234 |  3610 | `case PH7_OP_MUL:` |
|        - |  3611 | `case PH7_OP_MUL_STORE: {` |
|     2470 |  3612 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3613 | `	/* Force the operand to be numeric */` |
|        - |  3614 | `#ifdef UNTRUST` |
|        - |  3615 | `	if( pNos < pStack ){` |
|        - |  3616 | `		goto Abort;` |
|        - |  3617 | `	}` |
|        - |  3618 | `#endif` |
|     2470 |  3619 | `	PH7_MemObjToNumeric(pTos);` |
|     2470 |  3620 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3621 | `	/* Perform the requested operation */` |
|     2470 |  3622 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3623 | `		/* Floating point arithemic */` |
|        - |  3624 | `		ph7_real a,b,r;` |
|       17 |  3625 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3626 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3627 | `		}` |
|       17 |  3628 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3629 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3630 | `		}` |
|       17 |  3631 | `		a = pNos->rVal;` |
|       17 |  3632 | `		b = pTos->rVal;` |
|       17 |  3633 | `		r = a * b;` |
|        - |  3634 | `		/* Push the result */` |
|       17 |  3635 | `		pNos->rVal = r;` |
|       17 |  3636 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3637 | `		/* Try to get an integer representation */` |
|       17 |  3638 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3639 | `	}else{` |
|        - |  3640 | `		/* Integer arithmetic */` |
|        - |  3641 | `		sxi64 a,b,r;` |
|     2454 |  3642 | `		a = pNos->x.iVal;` |
|     2454 |  3643 | `		b = pTos->x.iVal;` |
|     2454 |  3644 | `		r = a * b;` |
|        - |  3645 | `		/* Push the result */` |
|     2454 |  3646 | `		pNos->x.iVal = r;` |
|     2454 |  3647 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3648 | `	}` |
|     2470 |  3649 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3650 | `		ph7_value *pObj;` |
|       19 |  3651 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3652 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  3653 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  3654 | `			PH7_MemObjStore(pNos,pObj);` |
|        9 |  3655 | `		}` |
|        9 |  3656 | `	}` |
|     2470 |  3657 | `	VmPopOperand(&pTos,1);` |
|     2470 |  3658 | `	break;` |
|        - |  3659 | `				 }` |
|        - |  3660 | `/* OP_ADD * * *` |
|        - |  3661 | ` *` |
|        - |  3662 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3663 | ` * and push the result back onto the stack.` |
|        - |  3664 | ` */` |
|      427 |  3665 | `case PH7_OP_ADD:{` |
|      856 |  3666 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3667 | `#ifdef UNTRUST` |
|        - |  3668 | `	if( pNos < pStack ){` |
|        - |  3669 | `		goto Abort;` |
|        - |  3670 | `	}` |
|        - |  3671 | `#endif` |
|        - |  3672 | `	/* Perform the addition */` |
|      856 |  3673 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      856 |  3674 | `	VmPopOperand(&pTos,1);` |
|      856 |  3675 | `	break;` |
|        - |  3676 | `				}` |
|        - |  3677 | `/*` |
|        - |  3678 | ` * OP_ADD_STORE * * *` |
|        - |  3679 | ` *` |
|        - |  3680 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3681 | ` * and push the result back onto the stack.` |
|        - |  3682 | ` */` |
|      481 |  3683 | `case PH7_OP_ADD_STORE:{` |
|      963 |  3684 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3685 | `	ph7_value *pObj;` |
|        - |  3686 | `	sxu32 nIdx;` |
|        - |  3687 | `#ifdef UNTRUST` |
|        - |  3688 | `	if( pNos < pStack ){` |
|        - |  3689 | `		goto Abort;` |
|        - |  3690 | `	}` |
|        - |  3691 | `#endif` |
|        - |  3692 | `	/* Perform the addition */` |
|      963 |  3693 | `	nIdx = pTos->nIdx;` |
|      963 |  3694 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3695 | `	/* Peform the store operation */` |
|      963 |  3696 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3697 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      963 |  3698 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      963 |  3699 | `		PH7_MemObjStore(pTos,pObj);` |
|      481 |  3700 | `	}` |
|        - |  3701 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      963 |  3702 | `	PH7_MemObjStore(pTos,pNos);` |
|      963 |  3703 | `	VmPopOperand(&pTos,1);` |
|      963 |  3704 | `	break;` |
|        - |  3705 | `				}` |
|        - |  3706 | `/* OP_SUB * * *` |
|        - |  3707 | ` *` |
|        - |  3708 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3709 | ` * first (what was next on the stack) from the second (the` |
|        - |  3710 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3711 | ` */` |
|      294 |  3712 | `case PH7_OP_SUB: {` |
|      589 |  3713 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3714 | `#ifdef UNTRUST` |
|        - |  3715 | `	if( pNos < pStack ){` |
|        - |  3716 | `		goto Abort;` |
|        - |  3717 | `	}` |
|        - |  3718 | `#endif` |
|      589 |  3719 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3720 | `		/* Floating point arithemic */` |
|        - |  3721 | `		ph7_real a,b,r;` |
|       95 |  3722 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3723 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3724 | `		}` |
|       95 |  3725 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3726 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3727 | `		}` |
|       95 |  3728 | `		a = pNos->rVal;` |
|       95 |  3729 | `		b = pTos->rVal;` |
|       95 |  3730 | `		r = a - b;` |
|        - |  3731 | `		/* Push the result */` |
|       95 |  3732 | `		pNos->rVal = r;` |
|       95 |  3733 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3734 | `		/* Try to get an integer representation */` |
|       95 |  3735 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  3736 | `	}else{` |
|        - |  3737 | `		/* Integer arithmetic */` |
|        - |  3738 | `		sxi64 a,b,r;` |
|      495 |  3739 | `		a = pNos->x.iVal;` |
|      495 |  3740 | `		b = pTos->x.iVal;` |
|      495 |  3741 | `		r = a - b;` |
|        - |  3742 | `		/* Push the result */` |
|      495 |  3743 | `		pNos->x.iVal = r;` |
|      495 |  3744 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3745 | `	}` |
|      589 |  3746 | `	VmPopOperand(&pTos,1);` |
|      589 |  3747 | `	break;` |
|        - |  3748 | `				 }` |
|        - |  3749 | `/* OP_SUB_STORE * * *` |
|        - |  3750 | ` *` |
|        - |  3751 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3752 | ` * first (what was next on the stack) from the second (the` |
|        - |  3753 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3754 | ` */` |
|        1 |  3755 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3756 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3757 | `	ph7_value *pObj;` |
|        - |  3758 | `#ifdef UNTRUST` |
|        - |  3759 | `	if( pNos < pStack ){` |
|        - |  3760 | `		goto Abort;` |
|        - |  3761 | `	}` |
|        - |  3762 | `#endif` |
|        3 |  3763 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3764 | `		/* Floating point arithemic */` |
|        - |  3765 | `		ph7_real a,b,r;` |
|      ! 0 |  3766 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3767 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3768 | `		}` |
|      ! 0 |  3769 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3770 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3771 | `		}` |
|      ! 0 |  3772 | `		a = pTos->rVal;` |
|      ! 0 |  3773 | `		b = pNos->rVal;` |
|      ! 0 |  3774 | `		r = a - b;` |
|        - |  3775 | `		/* Push the result */` |
|      ! 0 |  3776 | `		pNos->rVal = r;` |
|      ! 0 |  3777 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3778 | `		/* Try to get an integer representation */` |
|      ! 0 |  3779 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3780 | `	}else{` |
|        - |  3781 | `		/* Integer arithmetic */` |
|        - |  3782 | `		sxi64 a,b,r;` |
|        3 |  3783 | `		a = pTos->x.iVal;` |
|        3 |  3784 | `		b = pNos->x.iVal;` |
|        3 |  3785 | `		r = a - b;` |
|        - |  3786 | `		/* Push the result */` |
|        3 |  3787 | `		pNos->x.iVal = r;` |
|        3 |  3788 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3789 | `	}` |
|        3 |  3790 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3791 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3792 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3793 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3794 | `	}` |
|        3 |  3795 | `	VmPopOperand(&pTos,1);` |
|        3 |  3796 | `	break;` |
|        - |  3797 | `				 }` |
|        - |  3798 |  |
|        - |  3799 | `/*` |
|        - |  3800 | ` * OP_MOD * * *` |
|        - |  3801 | ` *` |
|        - |  3802 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3803 | ` * first (what was next on the stack) from the second (the` |
|        - |  3804 | ` * top of the stack) and push the remainder after division` |
|        - |  3805 | ` * onto the stack.` |
|        - |  3806 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3807 | ` */` |
|      296 |  3808 | `case PH7_OP_MOD:{` |
|      594 |  3809 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3810 | `	sxi64 a,b,r;` |
|        - |  3811 | `#ifdef UNTRUST` |
|        - |  3812 | `	if( pNos < pStack ){` |
|        - |  3813 | `		goto Abort;` |
|        - |  3814 | `	}` |
|        - |  3815 | `#endif` |
|        - |  3816 | `	/* Force the operands to be integer */` |
|      594 |  3817 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3818 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3819 | `	}` |
|      594 |  3820 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3821 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3822 | `	}` |
|        - |  3823 | `	/* Perform the requested operation */` |
|      594 |  3824 | `	a = pNos->x.iVal;` |
|      594 |  3825 | `	b = pTos->x.iVal;` |
|      594 |  3826 | `	if( b == 0 ){` |
|        3 |  3827 | `		r = 0;` |
|        3 |  3828 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3829 | `		/* goto Abort; */` |
|        2 |  3830 | `	}else{` |
|      591 |  3831 | `		r = a%b;` |
|        - |  3832 | `	}` |
|        - |  3833 | `	/* Push the result */` |
|      594 |  3834 | `	pNos->x.iVal = r;` |
|      594 |  3835 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      594 |  3836 | `	VmPopOperand(&pTos,1);` |
|      594 |  3837 | `	break;` |
|        - |  3838 | `				}` |
|        - |  3839 | `/*` |
|        - |  3840 | ` * OP_MOD_STORE * * *` |
|        - |  3841 | ` *` |
|        - |  3842 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3843 | ` * first (what was next on the stack) from the second (the` |
|        - |  3844 | ` * top of the stack) and push the remainder after division` |
|        - |  3845 | ` * onto the stack.` |
|        - |  3846 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3847 | ` */` |
|        1 |  3848 | `case PH7_OP_MOD_STORE: {` |
|        3 |  3849 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3850 | `	ph7_value *pObj;` |
|        - |  3851 | `	sxi64 a,b,r;` |
|        - |  3852 | `#ifdef UNTRUST` |
|        - |  3853 | `	if( pNos < pStack ){` |
|        - |  3854 | `		goto Abort;` |
|        - |  3855 | `	}` |
|        - |  3856 | `#endif` |
|        - |  3857 | `	/* Force the operands to be integer */` |
|        3 |  3858 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3859 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3860 | `	}` |
|        3 |  3861 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3862 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3863 | `	}` |
|        - |  3864 | `	/* Perform the requested operation */` |
|        3 |  3865 | `	a = pTos->x.iVal;` |
|        3 |  3866 | `	b = pNos->x.iVal;` |
|        3 |  3867 | `	if( b == 0 ){` |
|      ! 0 |  3868 | `		r = 0;` |
|      ! 0 |  3869 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3870 | `		/* goto Abort; */` |
|      ! 0 |  3871 | `	}else{` |
|        3 |  3872 | `		r = a%b;` |
|        - |  3873 | `	}` |
|        - |  3874 | `	/* Push the result */` |
|        3 |  3875 | `	pNos->x.iVal = r;` |
|        3 |  3876 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  3877 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3878 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3879 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3880 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3881 | `	}` |
|        3 |  3882 | `	VmPopOperand(&pTos,1);` |
|        3 |  3883 | `	break;` |
|        - |  3884 | `				}` |
|        - |  3885 | `/*` |
|        - |  3886 | ` * OP_DIV * * *` |
|        - |  3887 | ` *` |
|        - |  3888 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3889 | ` * first (what was next on the stack) from the second (the` |
|        - |  3890 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3891 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3892 | ` */` |
|       28 |  3893 | `case PH7_OP_DIV:{` |
|       58 |  3894 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3895 | `	ph7_real a,b,r;` |
|        - |  3896 | `#ifdef UNTRUST` |
|        - |  3897 | `	if( pNos < pStack ){` |
|        - |  3898 | `		goto Abort;` |
|        - |  3899 | `	}` |
|        - |  3900 | `#endif` |
|        - |  3901 | `	/* Force the operands to be real */` |
|       58 |  3902 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  3903 | `		PH7_MemObjToReal(pTos);` |
|       26 |  3904 | `	}` |
|       58 |  3905 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  3906 | `		PH7_MemObjToReal(pNos);` |
|        9 |  3907 | `	}` |
|        - |  3908 | `	/* Perform the requested operation */` |
|       58 |  3909 | `	a = pNos->rVal;` |
|       58 |  3910 | `	b = pTos->rVal;` |
|       58 |  3911 | `	if( b == 0 ){` |
|        - |  3912 | `		/* Division by zero */` |
|        3 |  3913 | `		pNos->rVal = 0;` |
|        3 |  3914 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  3915 | `		/* goto Abort; */` |
|        2 |  3916 | `	}else{` |
|       55 |  3917 | `		r = a/b;` |
|        - |  3918 | `		/* Push the result */` |
|       55 |  3919 | `		pNos->rVal = r;` |
|       55 |  3920 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3921 | `		/* Try to get an integer representation */` |
|       55 |  3922 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3923 | `	}` |
|       58 |  3924 | `	VmPopOperand(&pTos,1);` |
|       58 |  3925 | `	break;` |
|        - |  3926 | `				}` |
|        - |  3927 | `/*` |
|        - |  3928 | ` * OP_DIV_STORE * * *` |
|        - |  3929 | ` *` |
|        - |  3930 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3931 | ` * first (what was next on the stack) from the second (the` |
|        - |  3932 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3933 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3934 | ` */` |
|        1 |  3935 | `case PH7_OP_DIV_STORE:{` |
|        3 |  3936 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3937 | `	ph7_value *pObj;` |
|        - |  3938 | `	ph7_real a,b,r;` |
|        - |  3939 | `#ifdef UNTRUST` |
|        - |  3940 | `	if( pNos < pStack ){` |
|        - |  3941 | `		goto Abort;` |
|        - |  3942 | `	}` |
|        - |  3943 | `#endif` |
|        - |  3944 | `	/* Force the operands to be real */` |
|        3 |  3945 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3946 | `		PH7_MemObjToReal(pTos);` |
|        1 |  3947 | `	}` |
|        3 |  3948 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3949 | `		PH7_MemObjToReal(pNos);` |
|        1 |  3950 | `	}` |
|        - |  3951 | `	/* Perform the requested operation */` |
|        3 |  3952 | `	a = pTos->rVal;` |
|        3 |  3953 | `	b = pNos->rVal;` |
|        3 |  3954 | `	if( b == 0 ){` |
|        - |  3955 | `		/* Division by zero */` |
|      ! 0 |  3956 | `		r = 0;` |
|      ! 0 |  3957 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  3958 | `		/* goto Abort; */` |
|      ! 0 |  3959 | `	}else{` |
|        3 |  3960 | `		r = a/b;` |
|        - |  3961 | `		/* Push the result */` |
|        3 |  3962 | `		pNos->rVal = r;` |
|        3 |  3963 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3964 | `		/* Try to get an integer representation */` |
|        3 |  3965 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3966 | `	}` |
|        3 |  3967 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3968 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3969 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3970 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3971 | `	}` |
|        3 |  3972 | `	VmPopOperand(&pTos,1);` |
|        3 |  3973 | `	break;` |
|        - |  3974 | `				}` |
|        - |  3975 | `/* OP_BAND * * *` |
|        - |  3976 | ` *` |
|        - |  3977 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3978 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  3979 | ` * two elements.` |
|        - |  3980 | `*/` |
|        - |  3981 | `/* OP_BOR * * *` |
|        - |  3982 | ` *` |
|        - |  3983 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3984 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  3985 | ` * two elements.` |
|        - |  3986 | ` */` |
|        - |  3987 | `/* OP_BXOR * * *` |
|        - |  3988 | ` *` |
|        - |  3989 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3990 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  3991 | ` * two elements.` |
|        - |  3992 | ` */` |
|       30 |  3993 | `case PH7_OP_BAND:` |
|        - |  3994 | `case PH7_OP_BOR:` |
|        - |  3995 | `case PH7_OP_BXOR:{` |
|       62 |  3996 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3997 | `	sxi64 a,b,r;` |
|        - |  3998 | `#ifdef UNTRUST` |
|        - |  3999 | `	if( pNos < pStack ){` |
|        - |  4000 | `		goto Abort;` |
|        - |  4001 | `	}` |
|        - |  4002 | `#endif` |
|        - |  4003 | `	/* Force the operands to be integer */` |
|       62 |  4004 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4005 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4006 | `	}` |
|       62 |  4007 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4008 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4009 | `	}` |
|        - |  4010 | `	/* Perform the requested operation */` |
|       62 |  4011 | `	a = pNos->x.iVal;` |
|       62 |  4012 | `	b = pTos->x.iVal;` |
|       62 |  4013 | `	switch(pInstr->iOp){` |
|        6 |  4014 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4015 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4016 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4017 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       18 |  4018 | `	case PH7_OP_BAND_STORE:` |
|       18 |  4019 | `	case PH7_OP_BAND:` |
|       38 |  4020 | `	default:          r = a&b; break;` |
|        - |  4021 | `	}` |
|        - |  4022 | `	/* Push the result */` |
|       62 |  4023 | `	pNos->x.iVal = r;` |
|       62 |  4024 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       62 |  4025 | `	VmPopOperand(&pTos,1);` |
|       62 |  4026 | `	break;` |
|        - |  4027 | `				 }` |
|        - |  4028 | `/* OP_BAND_STORE * * *` |
|        - |  4029 | ` *` |
|        - |  4030 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4031 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4032 | ` * two elements.` |
|        - |  4033 | `*/` |
|        - |  4034 | `/* OP_BOR_STORE * * *` |
|        - |  4035 | ` *` |
|        - |  4036 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4037 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4038 | ` * two elements.` |
|        - |  4039 | ` */` |
|        - |  4040 | `/* OP_BXOR_STORE * * *` |
|        - |  4041 | ` *` |
|        - |  4042 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4043 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4044 | ` * two elements.` |
|        - |  4045 | ` */` |
|        7 |  4046 | `case PH7_OP_BAND_STORE:` |
|        - |  4047 | `case PH7_OP_BOR_STORE:` |
|        - |  4048 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4049 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4050 | `	ph7_value *pObj;` |
|        - |  4051 | `	sxi64 a,b,r;` |
|        - |  4052 | `#ifdef UNTRUST` |
|        - |  4053 | `	if( pNos < pStack ){` |
|        - |  4054 | `		goto Abort;` |
|        - |  4055 | `	}` |
|        - |  4056 | `#endif` |
|        - |  4057 | `	/* Force the operands to be integer */` |
|       15 |  4058 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4059 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4060 | `	}` |
|       15 |  4061 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4062 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4063 | `	}` |
|        - |  4064 | `	/* Perform the requested operation */` |
|       15 |  4065 | `	a = pTos->x.iVal;` |
|       15 |  4066 | `	b = pNos->x.iVal;` |
|       15 |  4067 | `	switch(pInstr->iOp){` |
|        2 |  4068 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4069 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4070 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4071 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4072 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4073 | `	case PH7_OP_BAND:` |
|        5 |  4074 | `	default:          r = a&b; break;` |
|        - |  4075 | `	}` |
|        - |  4076 | `	/* Push the result */` |
|       15 |  4077 | `	pNos->x.iVal = r;` |
|       15 |  4078 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4079 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4080 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4081 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4082 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4083 | `	}` |
|       15 |  4084 | `	VmPopOperand(&pTos,1);` |
|       15 |  4085 | `	break;` |
|        - |  4086 | `				 }` |
|        - |  4087 | `/* OP_SHL * * *` |
|        - |  4088 | ` *` |
|        - |  4089 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4090 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4091 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4092 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4093 | ` */` |
|        - |  4094 | `/* OP_SHR * * *` |
|        - |  4095 | ` *` |
|        - |  4096 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4097 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4098 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4099 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4100 | ` */` |
|        9 |  4101 | `case PH7_OP_SHL:` |
|        - |  4102 | `case PH7_OP_SHR: {` |
|       19 |  4103 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4104 | `	sxi64 a,r;` |
|        - |  4105 | `	sxi32 b;` |
|        - |  4106 | `#ifdef UNTRUST` |
|        - |  4107 | `	if( pNos < pStack ){` |
|        - |  4108 | `		goto Abort;` |
|        - |  4109 | `	}` |
|        - |  4110 | `#endif` |
|        - |  4111 | `	/* Force the operands to be integer */` |
|       19 |  4112 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4113 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4114 | `	}` |
|       19 |  4115 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4116 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4117 | `	}` |
|        - |  4118 | `	/* Perform the requested operation */` |
|       19 |  4119 | `	a = pNos->x.iVal;` |
|       19 |  4120 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4121 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4122 | `		r = a << b;` |
|        6 |  4123 | `	}else{` |
|        9 |  4124 | `		r = a >> b;` |
|        - |  4125 | `	}` |
|        - |  4126 | `	/* Push the result */` |
|       19 |  4127 | `	pNos->x.iVal = r;` |
|       19 |  4128 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4129 | `	VmPopOperand(&pTos,1);` |
|       19 |  4130 | `	break;` |
|        - |  4131 | `				 }` |
|        - |  4132 | `/*  OP_SHL_STORE * * *` |
|        - |  4133 | ` *` |
|        - |  4134 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4135 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4136 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4137 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4138 | ` */` |
|        - |  4139 | `/* OP_SHR_STORE * * *` |
|        - |  4140 | ` *` |
|        - |  4141 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4142 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4143 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4144 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4145 | ` */` |
|        7 |  4146 | `case PH7_OP_SHL_STORE:` |
|        - |  4147 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4148 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4149 | `	ph7_value *pObj;` |
|        - |  4150 | `	sxi64 a,r;` |
|        - |  4151 | `	sxi32 b;` |
|        - |  4152 | `#ifdef UNTRUST` |
|        - |  4153 | `	if( pNos < pStack ){` |
|        - |  4154 | `		goto Abort;` |
|        - |  4155 | `	}` |
|        - |  4156 | `#endif` |
|        - |  4157 | `	/* Force the operands to be integer */` |
|       15 |  4158 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4159 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4160 | `	}` |
|       15 |  4161 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4162 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4163 | `	}` |
|        - |  4164 | `	/* Perform the requested operation */` |
|       15 |  4165 | `	a = pTos->x.iVal;` |
|       15 |  4166 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4167 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4168 | `		r = a << b;` |
|        4 |  4169 | `	}else{` |
|        9 |  4170 | `		r = a >> b;` |
|        - |  4171 | `	}` |
|        - |  4172 | `	/* Push the result */` |
|       15 |  4173 | `	pNos->x.iVal = r;` |
|       15 |  4174 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4175 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4176 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4177 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4178 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4179 | `	}` |
|       15 |  4180 | `	VmPopOperand(&pTos,1);` |
|       15 |  4181 | `	break;` |
|        - |  4182 | `				 }` |
|        - |  4183 | `/* CAT:  P1 * *` |
|        - |  4184 | ` *` |
|        - |  4185 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4186 | ` * back.` |
|        - |  4187 | ` */` |
|    59861 |  4188 | `case PH7_OP_CAT:{` |
|        - |  4189 | `	ph7_value *pNos,*pCur;` |
|   119724 |  4190 | `	if( pInstr->iP1 < 1 ){` |
|    92840 |  4191 | `		pNos = &pTos[-1];` |
|    46421 |  4192 | `	}else{` |
|    26886 |  4193 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4194 | `	}` |
|        - |  4195 | `#ifdef UNTRUST` |
|        - |  4196 | `	if( pNos < pStack ){` |
|        - |  4197 | `		goto Abort;` |
|        - |  4198 | `	}` |
|        - |  4199 | `#endif` |
|        - |  4200 | `	/* Force a string cast */` |
|   119724 |  4201 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      980 |  4202 | `		PH7_MemObjToString(pNos);` |
|      489 |  4203 | `	}` |
|   119724 |  4204 | `	pCur = &pNos[1];` |
|   241286 |  4205 | `	while( pCur <= pTos ){` |
|   121564 |  4206 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50450 |  4207 | `			PH7_MemObjToString(pCur);` |
|    25224 |  4208 | `		}` |
|        - |  4209 | `		/* Perform the concatenation */` |
|   121564 |  4210 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   121526 |  4211 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    60762 |  4212 | `		}` |
|   121564 |  4213 | `		SyBlobRelease(&pCur->sBlob);` |
|   121564 |  4214 | `		pCur++;` |
|        2 |  4215 | `	}` |
|   119724 |  4216 | `	pTos = pNos;` |
|   119724 |  4217 | `	break;` |
|        - |  4218 | `				}` |
|        - |  4219 | `/*  CAT_STORE: * * *` |
|        - |  4220 | ` *` |
|        - |  4221 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4222 | ` * back.` |
|        - |  4223 | ` */` |
|     2989 |  4224 | `case PH7_OP_CAT_STORE:{` |
|     5980 |  4225 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4226 | `	ph7_value *pObj;` |
|        - |  4227 | `#ifdef UNTRUST` |
|        - |  4228 | `	if( pNos < pStack ){` |
|        - |  4229 | `		goto Abort;` |
|        - |  4230 | `	}` |
|        - |  4231 | `#endif` |
|     5980 |  4232 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4233 | `		/* Force a string cast */` |
|      ! 0 |  4234 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4235 | `	}` |
|     5980 |  4236 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4237 | `		/* Force a string cast */` |
|      ! 0 |  4238 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4239 | `	}` |
|        - |  4240 | `	/* Perform the concatenation (Reverse order) */` |
|     5980 |  4241 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     5980 |  4242 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     2989 |  4243 | `	}` |
|        - |  4244 | `	/* Perform the store operation */` |
|     5980 |  4245 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4246 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     5980 |  4247 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     5980 |  4248 | `		PH7_MemObjStore(pTos,pObj);` |
|     2989 |  4249 | `	}` |
|     5980 |  4250 | `	PH7_MemObjStore(pTos,pNos);` |
|     5980 |  4251 | `	VmPopOperand(&pTos,1);` |
|     5980 |  4252 | `	break;` |
|        - |  4253 | `				}` |
|        - |  4254 | `/* OP_AND: * * *` |
|        - |  4255 | ` *` |
|        - |  4256 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4257 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4258 | ` * stack.` |
|        - |  4259 | ` */` |
|        - |  4260 | `/* OP_OR: * * *` |
|        - |  4261 | ` *` |
|        - |  4262 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4263 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4264 | ` * stack.` |
|        - |  4265 | ` */` |
|   110892 |  4266 | `case PH7_OP_LAND:` |
|        - |  4267 | `case PH7_OP_LOR: {` |
|   221830 |  4268 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4269 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4270 | `#ifdef UNTRUST` |
|        - |  4271 | `	if( pNos < pStack ){` |
|        - |  4272 | `		goto Abort;` |
|        - |  4273 | `	}` |
|        - |  4274 | `#endif` |
|        - |  4275 | `	/* Force a boolean cast */` |
|   221830 |  4276 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4277 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4278 | `	}` |
|   221830 |  4279 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4280 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4281 | `	}` |
|   221830 |  4282 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   221830 |  4283 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   221830 |  4284 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4285 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|   110948 |  4286 | `		v1 = and_logic[v1*3+v2];` |
|    55497 |  4287 | `	}else{` |
|        - |  4288 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   110884 |  4289 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4290 | `	}` |
|   221830 |  4291 | `	if( v1 == 2 ){` |
|      ! 0 |  4292 | `		v1 = 1;` |
|      ! 0 |  4293 | `	}` |
|   221830 |  4294 | `	VmPopOperand(&pTos,1);` |
|   221830 |  4295 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   221830 |  4296 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   221830 |  4297 | `	break;` |
|        - |  4298 | `				 }` |
|        - |  4299 | `/* OP_LXOR: * * *` |
|        - |  4300 | ` *` |
|        - |  4301 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4302 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4303 | ` * stack.` |
|        - |  4304 | ` * According to the PHP language reference manual:` |
|        - |  4305 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4306 | ` *  TRUE,but not both.` |
|        - |  4307 | ` */` |
|        5 |  4308 | `case PH7_OP_LXOR:{` |
|       11 |  4309 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4310 | `	sxi32 v = 0;` |
|        - |  4311 | `#ifdef UNTRUST` |
|        - |  4312 | `	if( pNos < pStack ){` |
|        - |  4313 | `		goto Abort;` |
|        - |  4314 | `	}` |
|        - |  4315 | `#endif` |
|        - |  4316 | `	/* Force a boolean cast */` |
|       11 |  4317 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4318 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4319 | `	}` |
|       11 |  4320 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4321 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4322 | `	}` |
|       11 |  4323 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4324 | `		v = 1;` |
|        3 |  4325 | `	}` |
|       11 |  4326 | `	VmPopOperand(&pTos,1);` |
|       11 |  4327 | `	pTos->x.iVal = v;` |
|       11 |  4328 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4329 | `	break;` |
|        - |  4330 | `				 }` |
|        - |  4331 | `/* OP_EQ P1 P2 P3` |
|        - |  4332 | ` *` |
|        - |  4333 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4334 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4335 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4336 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4337 | ` */` |
|        - |  4338 | `/* OP_NEQ P1 P2 P3` |
|        - |  4339 | ` *` |
|        - |  4340 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4341 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4342 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4343 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4344 | ` */` |
|     3740 |  4345 | `case PH7_OP_EQ:` |
|        - |  4346 | `case PH7_OP_NEQ: {` |
|     7482 |  4347 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4348 | `	/* Perform the comparison and act accordingly */` |
|        - |  4349 | `#ifdef UNTRUST` |
|        - |  4350 | `	if( pNos < pStack ){` |
|        - |  4351 | `		goto Abort;` |
|        - |  4352 | `	}` |
|        - |  4353 | `#endif` |
|     7482 |  4354 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7482 |  4355 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       20 |  4356 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7473 |  4357 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7438 |  4358 | `		rc = rc == 0;` |
|     3720 |  4359 | `	}else{` |
|       28 |  4360 | `		rc = rc != 0;` |
|        - |  4361 | `	}` |
|     7482 |  4362 | `	VmPopOperand(&pTos,1);` |
|     7482 |  4363 | `	if( !pInstr->iP2 ){` |
|        - |  4364 | `		/* Push comparison result without taking the jump */` |
|     7482 |  4365 | `		PH7_MemObjRelease(pTos);` |
|     7482 |  4366 | `		pTos->x.iVal = rc;` |
|        - |  4367 | `		/* Invalidate any prior representation */` |
|     7482 |  4368 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3742 |  4369 | `	}else{` |
|      ! 0 |  4370 | `		if( rc ){` |
|        - |  4371 | `			/* Jump to the desired location */` |
|      ! 0 |  4372 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4373 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4374 | `		}` |
|        - |  4375 | `	}` |
|     7482 |  4376 | `	break;` |
|        - |  4377 | `				 }` |
|        - |  4378 | `/* OP_TEQ P1 P2 *` |
|        - |  4379 | ` *` |
|        - |  4380 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4381 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4382 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4383 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4384 | ` */` |
|   130183 |  4385 | `case PH7_OP_TEQ: {` |
|   260368 |  4386 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4387 | `	/* Perform the comparison and act accordingly */` |
|        - |  4388 | `#ifdef UNTRUST` |
|        - |  4389 | `	if( pNos < pStack ){` |
|        - |  4390 | `		goto Abort;` |
|        - |  4391 | `	}` |
|        - |  4392 | `#endif` |
|   260368 |  4393 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   260368 |  4394 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4395 | `		rc = 0;` |
|        2 |  4396 | `	}else{` |
|   260366 |  4397 | `		rc = rc == 0;` |
|        - |  4398 | `	}` |
|   260368 |  4399 | `	VmPopOperand(&pTos,1);` |
|   260368 |  4400 | `	if( !pInstr->iP2 ){` |
|        - |  4401 | `		/* Push comparison result without taking the jump */` |
|   260368 |  4402 | `		PH7_MemObjRelease(pTos);` |
|   260368 |  4403 | `		pTos->x.iVal = rc;` |
|        - |  4404 | `		/* Invalidate any prior representation */` |
|   260368 |  4405 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   130185 |  4406 | `	}else{` |
|      ! 0 |  4407 | `		if( rc ){` |
|        - |  4408 | `			/* Jump to the desired location */` |
|      ! 0 |  4409 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4410 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4411 | `		}` |
|        - |  4412 | `	}` |
|   260368 |  4413 | `	break;` |
|        - |  4414 | `				 }` |
|        - |  4415 | `/* OP_TNE P1 P2 *` |
|        - |  4416 | ` *` |
|        - |  4417 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4418 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4419 | ` * instruction.` |
|        - |  4420 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4421 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4422 | ` *` |
|        - |  4423 | ` */` |
|   103318 |  4424 | `case PH7_OP_TNE: {` |
|   206638 |  4425 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4426 | `	/* Perform the comparison and act accordingly */` |
|        - |  4427 | `#ifdef UNTRUST` |
|        - |  4428 | `	if( pNos < pStack ){` |
|        - |  4429 | `		goto Abort;` |
|        - |  4430 | `	}` |
|        - |  4431 | `#endif` |
|   206638 |  4432 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   206638 |  4433 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4434 | `		rc = 1;` |
|        2 |  4435 | `	}else{` |
|   206636 |  4436 | `		rc = rc != 0;` |
|        - |  4437 | `	}` |
|   206638 |  4438 | `	VmPopOperand(&pTos,1);` |
|   206638 |  4439 | `	if( !pInstr->iP2 ){` |
|        - |  4440 | `		/* Push comparison result without taking the jump */` |
|   206638 |  4441 | `		PH7_MemObjRelease(pTos);` |
|   206638 |  4442 | `		pTos->x.iVal = rc;` |
|        - |  4443 | `		/* Invalidate any prior representation */` |
|   206638 |  4444 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   103320 |  4445 | `	}else{` |
|      ! 0 |  4446 | `		if( rc ){` |
|        - |  4447 | `			/* Jump to the desired location */` |
|      ! 0 |  4448 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4449 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4450 | `		}` |
|        - |  4451 | `	}` |
|   206638 |  4452 | `	break;` |
|        - |  4453 | `				 }` |
|        - |  4454 | `/* OP_LT P1 P2 P3` |
|        - |  4455 | ` *` |
|        - |  4456 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4457 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4458 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4459 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4460 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4461 | ` *` |
|        - |  4462 | ` */` |
|        - |  4463 | `/* OP_LE P1 P2 P3` |
|        - |  4464 | ` *` |
|        - |  4465 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4466 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4467 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4468 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4469 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4470 | ` *` |
|        - |  4471 | ` */` |
|   119918 |  4472 | `case PH7_OP_LT:` |
|        - |  4473 | `case PH7_OP_LE: {` |
|   239882 |  4474 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4475 | `	/* Perform the comparison and act accordingly */` |
|        - |  4476 | `#ifdef UNTRUST` |
|        - |  4477 | `	if( pNos < pStack ){` |
|        - |  4478 | `		goto Abort;` |
|        - |  4479 | `	}` |
|        - |  4480 | `#endif` |
|   239882 |  4481 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   239882 |  4482 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4483 | `		rc = 0;` |
|   239878 |  4484 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      395 |  4485 | `		rc = rc < 1;` |
|      198 |  4486 | `	}else{` |
|   239480 |  4487 | `		rc = rc < 0;` |
|        - |  4488 | `	}` |
|   239882 |  4489 | `	VmPopOperand(&pTos,1);` |
|   239882 |  4490 | `	if( !pInstr->iP2 ){` |
|        - |  4491 | `		/* Push comparison result without taking the jump */` |
|   239882 |  4492 | `		PH7_MemObjRelease(pTos);` |
|   239882 |  4493 | `		pTos->x.iVal = rc;` |
|        - |  4494 | `		/* Invalidate any prior representation */` |
|   239882 |  4495 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   119964 |  4496 | `	}else{` |
|      ! 0 |  4497 | `		if( rc ){` |
|        - |  4498 | `			/* Jump to the desired location */` |
|      ! 0 |  4499 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4500 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4501 | `		}` |
|        - |  4502 | `	}` |
|   239882 |  4503 | `	break;` |
|        - |  4504 | `				}` |
|        - |  4505 | `/* OP_GT P1 P2 P3` |
|        - |  4506 | ` *` |
|        - |  4507 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4508 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4509 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4510 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4511 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4512 | ` *` |
|        - |  4513 | ` */` |
|        - |  4514 | `/* OP_GE P1 P2 P3` |
|        - |  4515 | ` *` |
|        - |  4516 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4517 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4518 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4519 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4520 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4521 | ` *` |
|        - |  4522 | ` */` |
|    53132 |  4523 | `case PH7_OP_GT:` |
|        - |  4524 | `case PH7_OP_GE: {` |
|   106266 |  4525 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4526 | `	/* Perform the comparison and act accordingly */` |
|        - |  4527 | `#ifdef UNTRUST` |
|        - |  4528 | `	if( pNos < pStack ){` |
|        - |  4529 | `		goto Abort;` |
|        - |  4530 | `	}` |
|        - |  4531 | `#endif` |
|   106266 |  4532 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   106266 |  4533 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4534 | `		rc = 0;` |
|   106262 |  4535 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   106110 |  4536 | `		rc = rc >= 0;` |
|    53056 |  4537 | `	}else{` |
|      150 |  4538 | `		rc = rc > 0;` |
|        - |  4539 | `	}` |
|   106266 |  4540 | `	VmPopOperand(&pTos,1);` |
|   106266 |  4541 | `	if( !pInstr->iP2 ){` |
|        - |  4542 | `		/* Push comparison result without taking the jump */` |
|   106266 |  4543 | `		PH7_MemObjRelease(pTos);` |
|   106266 |  4544 | `		pTos->x.iVal = rc;` |
|        - |  4545 | `		/* Invalidate any prior representation */` |
|   106266 |  4546 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    53134 |  4547 | `	}else{` |
|      ! 0 |  4548 | `		if( rc ){` |
|        - |  4549 | `			/* Jump to the desired location */` |
|      ! 0 |  4550 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4551 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4552 | `		}` |
|        - |  4553 | `	}` |
|   106266 |  4554 | `	break;` |
|        - |  4555 | `				}` |
|        - |  4556 | `/* OP_SEQ P1 P2 *` |
|        - |  4557 | ` * Strict string comparison.` |
|        - |  4558 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4559 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4560 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4561 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4562 | ` * use PH7_OP_EQ.` |
|        - |  4563 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4564 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4565 | ` */` |
|        - |  4566 | `/* OP_SNE P1 P2 *` |
|        - |  4567 | ` * Strict string comparison.` |
|        - |  4568 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4569 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4570 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4571 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4572 | ` * use PH7_OP_EQ.` |
|        - |  4573 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4574 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4575 | ` */` |
|       18 |  4576 | `case PH7_OP_SEQ:` |
|        - |  4577 | `case PH7_OP_SNE: {` |
|       38 |  4578 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4579 | `	SyString s1,s2;` |
|        - |  4580 | `	/* Perform the comparison and act accordingly */` |
|        - |  4581 | `#ifdef UNTRUST` |
|        - |  4582 | `	if( pNos < pStack ){` |
|        - |  4583 | `		goto Abort;` |
|        - |  4584 | `	}` |
|        - |  4585 | `#endif` |
|        - |  4586 | `	/* Force a string cast */` |
|       38 |  4587 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4588 | `		PH7_MemObjToString(pTos);` |
|        2 |  4589 | `	}` |
|       38 |  4590 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4591 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4592 | `	}` |
|       38 |  4593 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4594 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4595 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4596 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4597 | `		rc = rc != 0;` |
|      ! 0 |  4598 | `	}else{` |
|       38 |  4599 | `		rc = rc == 0;` |
|        - |  4600 | `	}` |
|       38 |  4601 | `	VmPopOperand(&pTos,1);` |
|       38 |  4602 | `	if( !pInstr->iP2 ){` |
|        - |  4603 | `		/* Push comparison result without taking the jump */` |
|       38 |  4604 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4605 | `		pTos->x.iVal = rc;` |
|        - |  4606 | `		/* Invalidate any prior representation */` |
|       38 |  4607 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4608 | `	}else{` |
|      ! 0 |  4609 | `		if( rc ){` |
|        - |  4610 | `			/* Jump to the desired location */` |
|      ! 0 |  4611 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4612 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4613 | `		}` |
|        - |  4614 | `	}` |
|       38 |  4615 | `	break;` |
|        - |  4616 | `				 }` |
|        - |  4617 | `/*` |
|        - |  4618 | ` * OP_LOAD_REF * * *` |
|        - |  4619 | ` * Push the index of a referenced object on the stack.` |
|        - |  4620 | ` */` |
|       57 |  4621 | `case PH7_OP_LOAD_REF: {` |
|        - |  4622 | `	sxu32 nIdx;` |
|        - |  4623 | `#ifdef UNTRUST` |
|        - |  4624 | `	if( pTos < pStack ){` |
|        - |  4625 | `		goto Abort;` |
|        - |  4626 | `	}` |
|        - |  4627 | `#endif` |
|        - |  4628 | `	/* Extract memory object index */` |
|      115 |  4629 | `	nIdx = pTos->nIdx;` |
|      115 |  4630 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4631 | `		/* Nullify the object */` |
|       95 |  4632 | `		PH7_MemObjRelease(pTos);` |
|        - |  4633 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4634 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4635 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4636 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4637 | `	}` |
|      115 |  4638 | `	break;` |
|        - |  4639 | `					  }` |
|        - |  4640 | `/*` |
|        - |  4641 | ` * OP_STORE_REF * * P3` |
|        - |  4642 | ` * Perform an assignment operation by reference.` |
|        - |  4643 | ` */` |
|       14 |  4644 | ` case PH7_OP_STORE_REF: {` |
|       30 |  4645 | `	 SyString sName = { 0 , 0 };` |
|        - |  4646 | `	 VmFrame *pFrameLocal;` |
|        - |  4647 | `	SyHashEntry *pEntry;` |
|        - |  4648 | `	sxu32 nIdx;` |
|        - |  4649 | `#ifdef UNTRUST` |
|        - |  4650 | `	if( pTos < pStack ){` |
|        - |  4651 | `		goto Abort;` |
|        - |  4652 | `	}` |
|        - |  4653 | `#endif` |
|       30 |  4654 | `	if( pInstr->p3 == 0 ){` |
|        - |  4655 | `		char *zName;` |
|        - |  4656 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4657 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4658 | `			/* Force a string cast */` |
|      ! 0 |  4659 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4660 | `		}` |
|      ! 0 |  4661 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4662 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4663 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4664 | `			if( zName ){` |
|      ! 0 |  4665 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4666 | `			}` |
|      ! 0 |  4667 | `		}` |
|      ! 0 |  4668 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4669 | `		pTos--;` |
|      ! 0 |  4670 | `	}else{` |
|       30 |  4671 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4672 | `	}` |
|       30 |  4673 | `	nIdx = pTos->nIdx;` |
|       30 |  4674 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4675 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4676 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4677 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4678 | `		}else{` |
|        - |  4679 | `			ph7_value *pObj;` |
|        - |  4680 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4681 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4682 | `			if( pObj == 0 ){` |
|      ! 0 |  4683 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4684 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4685 | `				goto Abort;` |
|        - |  4686 | `			}` |
|        - |  4687 | `			/* Perform the store operation */` |
|      ! 0 |  4688 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4689 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4690 | `		}` |
|       30 |  4691 | `	}else if( sName.nByte > 0){` |
|       30 |  4692 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4693 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4694 | `		}else{` |
|       30 |  4695 | `			pFrameLocal = pVm->pFrame;` |
|       30 |  4696 | `			while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4697 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  4698 | `				pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  4699 | `			}` |
|        - |  4700 | `			/* Query the local frame */` |
|       30 |  4701 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       30 |  4702 | `			if( pEntry ){` |
|      ! 0 |  4703 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4704 | `			}else{` |
|       30 |  4705 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       30 |  4706 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4707 | `					/* Insert in the $GLOBALS array */` |
|       26 |  4708 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       12 |  4709 | `				}` |
|       30 |  4710 | `				if( rc == SXRET_OK ){` |
|       30 |  4711 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       14 |  4712 | `				}` |
|        - |  4713 | `			}` |
|        - |  4714 | `		}` |
|       14 |  4715 | `	}` |
|       30 |  4716 | `	break;` |
|        - |  4717 | `				 }` |
|        - |  4718 | `/*` |
|        - |  4719 | ` * OP_UPLINK P1 * *` |
|        - |  4720 | ` * Link a variable to the top active VM frame.` |
|        - |  4721 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4722 | ` */` |
|       25 |  4723 | `case PH7_OP_UPLINK: {` |
|       52 |  4724 | `	if( pVm->pFrame->pParent ){` |
|       52 |  4725 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4726 | `		SyString sName;` |
|        - |  4727 | `		/* Perform the link */` |
|      104 |  4728 | `		while( pLink <= pTos ){` |
|       54 |  4729 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4730 | `				/* Force a string cast */` |
|      ! 0 |  4731 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4732 | `			}` |
|       54 |  4733 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  4734 | `			if( sName.nByte > 0 ){` |
|       54 |  4735 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  4736 | `			}` |
|       54 |  4737 | `			pLink++;` |
|        2 |  4738 | `		}` |
|       25 |  4739 | `	}` |
|       52 |  4740 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  4741 | `	break;` |
|        - |  4742 | `					}` |
|        - |  4743 | `/*` |
|        - |  4744 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4745 | ` * Push an exception in the corresponding container so that` |
|        - |  4746 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4747 | ` */` |
|       12 |  4748 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       26 |  4749 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4750 | `	VmFrame *pFrameLocal;` |
|       26 |  4751 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4752 | `	/* Create the exception frame */` |
|       26 |  4753 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       26 |  4754 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4755 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4756 | `		goto Abort;` |
|        - |  4757 | `	}` |
|        - |  4758 | `	/* Mark the special frame */` |
|       26 |  4759 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       26 |  4760 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4761 | `	/* Point to the frame that trigger the exception */` |
|       26 |  4762 | `	pFrameLocal = pFrameLocal->pParent;` |
|       28 |  4763 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        3 |  4764 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4765 | `	}` |
|       26 |  4766 | `	pException->pFrame = pFrameLocal;` |
|       26 |  4767 | `	break;` |
|        - |  4768 | `							}` |
|        - |  4769 | `/*` |
|        - |  4770 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4771 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4772 | ` */` |
|       12 |  4773 | `case PH7_OP_POP_EXCEPTION: {` |
|       26 |  4774 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       26 |  4775 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4776 | `		ph7_exception **apException;` |
|        - |  4777 | `		/* Pop the loaded exception */` |
|        7 |  4778 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        7 |  4779 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|        7 |  4780 | `			(void)SySetPop(&pVm->aException);` |
|        3 |  4781 | `		}` |
|        3 |  4782 | `	}` |
|       26 |  4783 | `	pException->pFrame = 0;` |
|        - |  4784 | `	/* Leave the exception frame */` |
|       26 |  4785 | `	VmLeaveFrame(&(*pVm));` |
|       26 |  4786 | `	break;` |
|        - |  4787 | `							}` |
|        - |  4788 |  |
|        - |  4789 | `/*` |
|        - |  4790 | ` * OP_THROW * P2 *` |
|        - |  4791 | ` * Throw an user exception.` |
|        - |  4792 | ` */` |
|       11 |  4793 | `case PH7_OP_THROW: {` |
|       24 |  4794 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       24 |  4795 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4796 | `#ifdef UNTRUST` |
|        - |  4797 | `	if( pTos < pStack ){` |
|        - |  4798 | `		goto Abort;` |
|        - |  4799 | `	}` |
|        - |  4800 | `#endif` |
|       28 |  4801 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4802 | `		/* Safely ignore the exception frame */` |
|        6 |  4803 | `		pFrameLocal = pFrameLocal->pParent;` |
|        2 |  4804 | `	}` |
|        - |  4805 | `	/* Tell the upper layer that an exception was thrown */` |
|       24 |  4806 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       24 |  4807 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       24 |  4808 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4809 | `		ph7_class *pException;` |
|        - |  4810 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4811 | `		 */` |
|       24 |  4812 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       24 |  4813 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4814 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4815 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4816 | `			if( rc == SXERR_ABORT ){` |
|        - |  4817 | `				/* Abort processing immediately */` |
|      ! 0 |  4818 | `				goto Abort;` |
|        - |  4819 | `			}` |
|      ! 0 |  4820 | `		}else{` |
|        - |  4821 | `			/* Throw the exception */` |
|       24 |  4822 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       24 |  4823 | `			if( rc == SXERR_ABORT ){` |
|        - |  4824 | `				/* Abort processing immediately */` |
|        9 |  4825 | `				goto Abort;` |
|        - |  4826 | `			}` |
|        - |  4827 | `		}` |
|        9 |  4828 | `	}else{` |
|        - |  4829 | `		/* Expecting a class instance */` |
|      ! 0 |  4830 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  4831 | `		if( rc == SXERR_ABORT ){` |
|        - |  4832 | `			/* Abort processing immediately */` |
|      ! 0 |  4833 | `			goto Abort;` |
|        - |  4834 | `		}` |
|        - |  4835 | `	}` |
|        - |  4836 | `	/* Pop the top entry */` |
|       16 |  4837 | `	VmPopOperand(&pTos,1);` |
|        - |  4838 | `	/* Perform an unconditional jump */` |
|       16 |  4839 | `	pc = nJump - 1;` |
|       16 |  4840 | `	break;` |
|        - |  4841 | `				   }` |
|        - |  4842 | `/*` |
|        - |  4843 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  4844 | ` * Prepare a foreach step.` |
|        - |  4845 | ` */` |
|     4647 |  4846 | `case PH7_OP_FOREACH_INIT: {` |
|     9296 |  4847 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4848 | `	void *pName;` |
|        - |  4849 | `#ifdef UNTRUST` |
|        - |  4850 | `	if( pTos < pStack ){` |
|        - |  4851 | `		goto Abort;` |
|        - |  4852 | `	}` |
|        - |  4853 | `#endif` |
|     9296 |  4854 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4855 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  4856 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4857 | `			/* Force a string cast */` |
|      ! 0 |  4858 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4859 | `		}` |
|        - |  4860 | `		/* Duplicate name */` |
|      ! 0 |  4861 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4862 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4863 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4864 | `		}` |
|      ! 0 |  4865 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4866 | `	}` |
|     9296 |  4867 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  4868 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4869 | `			/* Force a string cast */` |
|      ! 0 |  4870 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4871 | `		}` |
|        - |  4872 | `		/* Duplicate name */` |
|      ! 0 |  4873 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4874 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4875 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4876 | `		}` |
|      ! 0 |  4877 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4878 | `	}` |
|        - |  4879 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     9296 |  4880 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4881 | `		/* Jump out of the loop */` |
|      ! 0 |  4882 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4883 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4884 | `		}` |
|      ! 0 |  4885 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4886 | `	}else{` |
|        - |  4887 | `		ph7_foreach_step *pStep;` |
|     9296 |  4888 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     9296 |  4889 | `		if( pStep == 0 ){` |
|      ! 0 |  4890 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4891 | `			/* Jump out of the loop */` |
|      ! 0 |  4892 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4893 | `		}else{` |
|        - |  4894 | `			/* Zero the structure */` |
|     9296 |  4895 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4896 | `			/* Prepare the step */` |
|     9296 |  4897 | `			pStep->iFlags = pInfo->iFlags;` |
|     9296 |  4898 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     9288 |  4899 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4900 | `				/* Reset the internal loop cursor */` |
|     9288 |  4901 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4902 | `				/* Mark the step */` |
|     9288 |  4903 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     9288 |  4904 | `				pStep->xIter.pMap = pMap;` |
|     9288 |  4905 | `				pMap->iRef++;` |
|     4645 |  4906 | `			}else{` |
|        9 |  4907 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4908 | `				/* Reset the loop cursor */` |
|        9 |  4909 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|        - |  4910 | `				/* Mark the step */` |
|        9 |  4911 | `				pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  4912 | `				pStep->xIter.pThis = pThis;` |
|        9 |  4913 | `				pThis->iRef++;` |
|        - |  4914 | `			}` |
|        - |  4915 | `		}` |
|     9296 |  4916 | `		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  4917 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  4918 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  4919 | `			/* Jump out of the loop */` |
|      ! 0 |  4920 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4921 | `		}` |
|        - |  4922 | `	}` |
|     9296 |  4923 | `	VmPopOperand(&pTos,1);` |
|     9296 |  4924 | `	break;` |
|        - |  4925 | `						  }` |
|        - |  4926 | `/*` |
|        - |  4927 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  4928 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  4929 | ` */` |
|    74422 |  4930 | `case PH7_OP_FOREACH_STEP: {` |
|   148846 |  4931 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4932 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  4933 | `	ph7_value *pValue;` |
|        - |  4934 | `	VmFrame *pFrameLocal;` |
|        - |  4935 | `	/* Peek the last step */` |
|   148846 |  4936 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   148846 |  4937 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   148846 |  4938 | `	pFrameLocal = pVm->pFrame;` |
|   148846 |  4939 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4940 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  4941 | `		pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  4942 | `	}` |
|   148846 |  4943 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   148822 |  4944 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  4945 | `		ph7_hashmap_node *pNode;` |
|        - |  4946 | `		/* Extract the current node value */` |
|   148822 |  4947 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   148822 |  4948 | `		if( pNode == 0 ){` |
|        - |  4949 | `			/* No more entry to process */` |
|     9288 |  4950 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     9288 |  4951 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4952 | `				/* Break the reference with the last element */` |
|        5 |  4953 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  4954 | `			}` |
|        - |  4955 | `			/* Automatically reset the loop cursor */` |
|     9288 |  4956 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4957 | `			/* Cleanup the mess left behind */` |
|     9288 |  4958 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     9288 |  4959 | `			SySetPop(&pInfo->aStep);` |
|     9288 |  4960 | `			PH7_HashmapUnref(pMap);` |
|     4645 |  4961 | `		}else{` |
|   139536 |  4962 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      408 |  4963 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      408 |  4964 | `				if( pKey ){` |
|      408 |  4965 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      203 |  4966 | `				}` |
|      203 |  4967 | `			}` |
|   139536 |  4968 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4969 | `				SyHashEntry *pEntry;` |
|        - |  4970 | `				/* Pass by reference */` |
|       13 |  4971 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       13 |  4972 | `				if( pEntry ){` |
|       13 |  4973 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        7 |  4974 | `				}else{` |
|      ! 0 |  4975 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  4976 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  4977 | `				}` |
|        7 |  4978 | `			}else{` |
|        - |  4979 | `				/* Make a copy of the entry value */` |
|   139524 |  4980 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   139524 |  4981 | `				if( pValue ){` |
|   139524 |  4982 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    69761 |  4983 | `				}` |
|        - |  4984 | `			}` |
|        - |  4985 | `		}` |
|    74412 |  4986 | `	}else{` |
|       25 |  4987 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  4988 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  4989 | `		SyHashEntry *pEntry;` |
|        - |  4990 | `		/* Point to the next attribute */` |
|       29 |  4991 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  4992 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  4993 | `			/* Check access permission */` |
|       31 |  4994 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  4995 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  4996 | `					break; /* Access is granted */` |
|        - |  4997 | `			}` |
|        1 |  4998 | `		}` |
|       25 |  4999 | `		if( pEntry == 0 ){` |
|        - |  5000 | `			/* Clean up the mess left behind */` |
|        9 |  5001 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5002 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5003 | `				/* Break the reference with the last element */` |
|        3 |  5004 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5005 | `			}` |
|        9 |  5006 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5007 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5008 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5009 | `		}else{` |
|       17 |  5010 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5011 | `			ph7_value *pAttrValue;` |
|       17 |  5012 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5013 | `				/* Fill with the current attribute name */` |
|       17 |  5014 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5015 | `				if( pKey ){` |
|       17 |  5016 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5017 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5018 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5019 | `				}` |
|        8 |  5020 | `			}` |
|        - |  5021 | `			/* Extract attribute value */` |
|       17 |  5022 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5023 | `			if( pAttrValue ){` |
|       17 |  5024 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5025 | `					/* Pass by reference */` |
|        3 |  5026 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5027 | `					if( pEntry ){` |
|        3 |  5028 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5029 | `					}else{` |
|      ! 0 |  5030 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5031 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5032 | `					}` |
|        2 |  5033 | `				}else{` |
|        - |  5034 | `					/* Make a copy of the attribute value */` |
|       15 |  5035 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5036 | `					if( pValue ){` |
|       15 |  5037 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5038 | `					}` |
|        - |  5039 | `				}` |
|        8 |  5040 | `			}` |
|        - |  5041 | `		}` |
|        - |  5042 | `	}` |
|   148846 |  5043 | `	break;` |
|        - |  5044 | `						  }` |
|        - |  5045 | `/*` |
|        - |  5046 | ` * OP_MEMBER P1 P2` |
|        - |  5047 | ` * Load class attribute/method on the stack.` |
|        - |  5048 | ` */` |
|     1838 |  5049 | `case PH7_OP_MEMBER: {` |
|        - |  5050 | `	ph7_class_instance *pThis;` |
|        - |  5051 | `	ph7_value *pNos;` |
|        - |  5052 | `	SyString sName;` |
|     3678 |  5053 | `	if( !pInstr->iP1 ){` |
|     3620 |  5054 | `		pNos = &pTos[-1];` |
|        - |  5055 | `#ifdef UNTRUST` |
|        - |  5056 | `		if( pNos < pStack ){` |
|        - |  5057 | `			goto Abort;` |
|        - |  5058 | `		}` |
|        - |  5059 | `#endif` |
|     3620 |  5060 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5061 | `			ph7_class *pClass;` |
|        - |  5062 | `			/* Class already instantiated */` |
|     3620 |  5063 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5064 | `			/* Point to the instantiated class */` |
|     3620 |  5065 | `			pClass = pThis->pClass;` |
|        - |  5066 | `			/* Extract attribute name first */` |
|     3620 |  5067 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     3620 |  5068 | `			if( pInstr->iP2 ){` |
|        - |  5069 | `				/* Method call */` |
|      124 |  5070 | `				ph7_class_method *pMeth = 0;` |
|      124 |  5071 | `				if( sName.nByte > 0 ){` |
|        - |  5072 | `					/* Extract the target method */` |
|      124 |  5073 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       61 |  5074 | `				}` |
|      124 |  5075 | `				if( pMeth == 0 ){` |
|      ! 0 |  5076 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5077 | `						&pClass->sName,&sName` |
|        - |  5078 | `						);` |
|        - |  5079 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5080 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5081 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5082 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5083 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5084 | `				}else{` |
|        - |  5085 | `					/* Push method name on the stack */` |
|      124 |  5086 | `					PH7_MemObjRelease(pTos);` |
|      124 |  5087 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      124 |  5088 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5089 | `				}` |
|      124 |  5090 | `				pTos->nIdx = SXU32_HIGH;` |
|       63 |  5091 | `			}else{` |
|        - |  5092 | `				/* Attribute access */` |
|     3498 |  5093 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5094 | `				SyHashEntry *pEntry;` |
|        - |  5095 | `				/* Extract the target attribute */` |
|     3498 |  5096 | `				if( sName.nByte > 0 ){` |
|     3498 |  5097 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3498 |  5098 | `					if( pEntry ){` |
|        - |  5099 | `						/* Point to the attribute value */` |
|     3496 |  5100 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1747 |  5101 | `					}` |
|     1748 |  5102 | `				}` |
|     3498 |  5103 | `				if( pObjAttr == 0 ){` |
|        - |  5104 | `					/* No such attribute,load null */` |
|        4 |  5105 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5106 | `						&pClass->sName,&sName);` |
|        - |  5107 | `					/* Call the __get magic method if available */` |
|        3 |  5108 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5109 | `				}` |
|     3498 |  5110 | `				VmPopOperand(&pTos,1);` |
|        - |  5111 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5112 | `				 * This is due to the following case:` |
|        - |  5113 | `				 *     (new TestClass())->foo;` |
|        - |  5114 | `				 */` |
|     3498 |  5115 | `				pThis->iRef++;` |
|     3498 |  5116 | `				PH7_MemObjRelease(pTos);` |
|     3498 |  5117 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3498 |  5118 | `				if( pObjAttr ){` |
|     3496 |  5119 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5120 | `					/* Check attribute access */` |
|     3496 |  5121 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5122 | `						/* Load attribute */` |
|     3496 |  5123 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3496 |  5124 | `						if( pValue ){` |
|     3496 |  5125 | `							if( pThis->iRef < 2 ){` |
|        - |  5126 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5127 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5128 | `								 */` |
|        3 |  5129 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5130 | `							}else{` |
|        - |  5131 | `								/* Simple load */` |
|     3494 |  5132 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5133 | `							}` |
|     3496 |  5134 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3494 |  5135 | `								if( pThis->iRef > 1 ){` |
|        - |  5136 | `									/* Load attribute index */` |
|     3492 |  5137 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1745 |  5138 | `								}` |
|     1746 |  5139 | `							}` |
|     1747 |  5140 | `						}` |
|     1747 |  5141 | `					}` |
|     1747 |  5142 | `				}` |
|        - |  5143 | `				/* Safely unreference the object */` |
|     3498 |  5144 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5145 | `			}` |
|     1811 |  5146 | `		}else{` |
|      ! 0 |  5147 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5148 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5149 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5150 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5151 | `		}` |
|     1811 |  5152 | `	}else{` |
|        - |  5153 | `		/* Static member access using class name */` |
|       59 |  5154 | `		pNos = pTos;` |
|       59 |  5155 | `		pThis = 0;` |
|       59 |  5156 | `		if( !pInstr->p3 ){` |
|       57 |  5157 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       57 |  5158 | `			pNos--;` |
|        - |  5159 | `#ifdef UNTRUST` |
|        - |  5160 | `			if( pNos < pStack ){` |
|        - |  5161 | `				goto Abort;` |
|        - |  5162 | `			}` |
|        - |  5163 | `#endif` |
|       29 |  5164 | `		}else{` |
|        - |  5165 | `			/* Attribute name already computed */` |
|        3 |  5166 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5167 | `		}` |
|       59 |  5168 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|       59 |  5169 | `			ph7_class *pClass = 0;` |
|       59 |  5170 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5171 | `				/* Class already instantiated */` |
|      ! 0 |  5172 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5173 | `				pClass = pThis->pClass;` |
|      ! 0 |  5174 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5175 | `			}else{` |
|        - |  5176 | `				/* Try to extract the target class */` |
|       59 |  5177 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       88 |  5178 | `					pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pNos->sBlob),` |
|       29 |  5179 | `						SyBlobLength(&pNos->sBlob),FALSE,0);` |
|       29 |  5180 | `				}` |
|        - |  5181 | `			}` |
|       59 |  5182 | `			if( pClass == 0 ){` |
|        - |  5183 | `				/* Undefined class */` |
|      ! 0 |  5184 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5185 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5186 | `					);` |
|      ! 0 |  5187 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5188 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5189 | `				}` |
|      ! 0 |  5190 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5191 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5192 | `			}else{` |
|       59 |  5193 | `				if( pInstr->iP2 ){` |
|        - |  5194 | `					/* Method call */` |
|       25 |  5195 | `					ph7_class_method *pMeth = 0;` |
|       25 |  5196 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5197 | `						/* Extract the target method */` |
|       25 |  5198 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       12 |  5199 | `					}` |
|       25 |  5200 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5201 | `						if( pMeth ){` |
|      ! 0 |  5202 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5203 | `								&pClass->sName,&sName` |
|        - |  5204 | `								);` |
|      ! 0 |  5205 | `						}else{` |
|      ! 0 |  5206 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5207 | `								&pClass->sName,&sName` |
|        - |  5208 | `								);` |
|        - |  5209 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5210 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5211 | `						}` |
|        - |  5212 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5213 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5214 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5215 | `						}` |
|      ! 0 |  5216 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5217 | `					}else{` |
|        - |  5218 | `						/* Push method name on the stack */` |
|       25 |  5219 | `						PH7_MemObjRelease(pTos);` |
|       25 |  5220 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       25 |  5221 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5222 | `					}` |
|       25 |  5223 | `					pTos->nIdx = SXU32_HIGH;` |
|       13 |  5224 | `				}else{` |
|        - |  5225 | `					/* Attribute access */` |
|       35 |  5226 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5227 | `					/* Check for special ::class pseudo-constant */` |
|       49 |  5228 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       28 |  5229 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5230 | `						/* ::class returns the fully qualified class name */` |
|        - |  5231 | `						/* Pop the attribute name from the stack */` |
|       27 |  5232 | `						if( !pInstr->p3 ){` |
|       27 |  5233 | `							VmPopOperand(&pTos,1);` |
|       13 |  5234 | `						}` |
|       27 |  5235 | `						PH7_MemObjRelease(pTos);` |
|        - |  5236 | `						/* Load the class name */` |
|       27 |  5237 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       27 |  5238 | `						pTos->nIdx = SXU32_HIGH;` |
|       14 |  5239 | `					}else{` |
|        - |  5240 | `						/* Extract the target attribute */` |
|        9 |  5241 | `						if( sName.nByte > 0 ){` |
|        9 |  5242 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        4 |  5243 | `						}` |
|        9 |  5244 | `						if( pAttr == 0 ){` |
|        - |  5245 | `							/* No such attribute,load null */` |
|      ! 0 |  5246 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5247 | `								&pClass->sName,&sName);` |
|        - |  5248 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5249 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5250 | `						}` |
|        - |  5251 | `						/* Pop the attribute name from the stack */` |
|        9 |  5252 | `						if( !pInstr->p3 ){` |
|        7 |  5253 | `							VmPopOperand(&pTos,1);` |
|        3 |  5254 | `						}` |
|        9 |  5255 | `						PH7_MemObjRelease(pTos);` |
|        9 |  5256 | `						pTos->nIdx = SXU32_HIGH;` |
|        9 |  5257 | `						if( pAttr ){` |
|        9 |  5258 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5259 | `								/* Access to a non static attribute */` |
|      ! 0 |  5260 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5261 | `									&pClass->sName,&pAttr->sName` |
|        - |  5262 | `									);` |
|      ! 0 |  5263 | `							}else{` |
|        - |  5264 | `								ph7_value *pValue;` |
|        - |  5265 | `								/* Check if the access to the attribute is allowed */` |
|        9 |  5266 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5267 | `									/* Load the desired attribute */` |
|        9 |  5268 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|        9 |  5269 | `									if( pValue ){` |
|        9 |  5270 | `										PH7_MemObjLoad(pValue,pTos);` |
|        9 |  5271 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5272 | `											/* Load index number */` |
|        3 |  5273 | `											pTos->nIdx = pAttr->nIdx;` |
|        1 |  5274 | `										}` |
|        4 |  5275 | `									}` |
|        4 |  5276 | `								}` |
|        - |  5277 | `							}` |
|        4 |  5278 | `						}` |
|        - |  5279 | `					}` |
|        - |  5280 | `				}` |
|       59 |  5281 | `				if( pThis ){` |
|        - |  5282 | `					/* Safely unreference the object */` |
|      ! 0 |  5283 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5284 | `				}` |
|        - |  5285 | `			}` |
|       30 |  5286 | `		}else{` |
|        - |  5287 | `			/* Pop operands */` |
|      ! 0 |  5288 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5289 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5290 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5291 | `			}` |
|      ! 0 |  5292 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5293 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5294 | `		}` |
|        - |  5295 | `	}` |
|     3678 |  5296 | `	break;` |
|        - |  5297 | `					}` |
|        - |  5298 | `/*` |
|        - |  5299 | ` * OP_NEW P1 * * *` |
|        - |  5300 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5301 | ` */` |
|      257 |  5302 | `case PH7_OP_NEW: {` |
|      516 |  5303 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      516 |  5304 | `	ph7_class *pClass = 0;` |
|        - |  5305 | `	ph7_class_instance *pNew;` |
|      516 |  5306 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5307 | `		/* Try to extract the desired class */` |
|      773 |  5308 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      514 |  5309 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      257 |  5310 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5311 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5312 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5313 | `	}` |
|      516 |  5314 | `	if( pClass == 0 ){` |
|        - |  5315 | `		/* No such class */` |
|      ! 0 |  5316 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined,PH7 is loading NULL",` |
|      ! 0 |  5317 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5318 | `			);` |
|      ! 0 |  5319 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5320 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5321 | `			/* Pop given arguments */` |
|      ! 0 |  5322 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5323 | `		}` |
|      ! 0 |  5324 | `	}else{` |
|        - |  5325 | `		ph7_class_method *pCons;` |
|        - |  5326 | `		/* Create a new class instance */` |
|      516 |  5327 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      516 |  5328 | `		if( pNew == 0 ){` |
|      ! 0 |  5329 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5330 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5331 | `				&pClass->sName` |
|        - |  5332 | `			);` |
|      ! 0 |  5333 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5334 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5335 | `				/* Pop given arguments */` |
|      ! 0 |  5336 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5337 | `			}` |
|      ! 0 |  5338 | `			break;` |
|        - |  5339 | `		}` |
|        - |  5340 | `		/* Check if a constructor is available */` |
|      516 |  5341 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      516 |  5342 | `		if( pCons == 0 ){` |
|      458 |  5343 | `			SyString *pName = &pClass->sName;` |
|        - |  5344 | `			/* Check for a constructor with the same base class name */` |
|      458 |  5345 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      228 |  5346 | `		}` |
|      516 |  5347 | `		if( pCons ){` |
|        - |  5348 | `			/* Call the class constructor */` |
|       60 |  5349 | `			SySetReset(&aArg);` |
|      108 |  5350 | `			while( pArg < pTos ){` |
|       50 |  5351 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       50 |  5352 | `				pArg++;` |
|        2 |  5353 | `			}` |
|       60 |  5354 | `			if( pVm->bErrReport ){` |
|        - |  5355 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5356 | `				sxu32 n;` |
|       17 |  5357 | `				n = SySetUsed(&aArg);` |
|        - |  5358 | `				/* Emit a notice for missing arguments */` |
|       45 |  5359 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       29 |  5360 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       29 |  5361 | `					if( pFuncArg ){` |
|       29 |  5362 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5363 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5364 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5365 | `						}` |
|       14 |  5366 | `					}` |
|       29 |  5367 | `					n++;` |
|        1 |  5368 | `				}` |
|        8 |  5369 | `			}` |
|       60 |  5370 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5371 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       60 |  5372 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5373 | `				pNew->iRef = 1;` |
|      ! 0 |  5374 | `			}` |
|       29 |  5375 | `		}` |
|      516 |  5376 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5377 | `			/* Pop given arguments */` |
|       44 |  5378 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       21 |  5379 | `		}` |
|      516 |  5380 | `		PH7_MemObjRelease(pTos);` |
|      516 |  5381 | `		pTos->x.pOther = pNew;` |
|      516 |  5382 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5383 | `	}` |
|      516 |  5384 | `	break;` |
|        - |  5385 | `				 }` |
|        - |  5386 | `/*` |
|        - |  5387 | ` * OP_CLONE * * *` |
|        - |  5388 | ` * Perfome a clone operation.` |
|        - |  5389 | ` */` |
|       23 |  5390 | `case PH7_OP_CLONE: {` |
|        - |  5391 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5392 | `#ifdef UNTRUST` |
|        - |  5393 | `	if( pTos < pStack ){` |
|        - |  5394 | `		goto Abort;` |
|        - |  5395 | `	}` |
|        - |  5396 | `#endif` |
|        - |  5397 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5398 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5399 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5400 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5401 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5402 | `		break;` |
|        - |  5403 | `	}` |
|        - |  5404 | `	/* Point to the source */` |
|       44 |  5405 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5406 | `	/* Perform the clone operation */` |
|       44 |  5407 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5408 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5409 | `	if( pClone == 0 ){` |
|      ! 0 |  5410 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5411 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5412 | `	}else{` |
|        - |  5413 | `		/* Load the cloned object */` |
|       44 |  5414 | `		pTos->x.pOther = pClone;` |
|       44 |  5415 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5416 | `	}` |
|       44 |  5417 | `	break;` |
|        - |  5418 | `				   }` |
|        - |  5419 | `/*` |
|        - |  5420 | ` * OP_SWITCH * * P3` |
|        - |  5421 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5422 | ` */` |
|       18 |  5423 | `case PH7_OP_SWITCH: {` |
|       38 |  5424 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5425 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5426 | `	ph7_value sValue,sCaseValue;` |
|        - |  5427 | `	sxu32 n,nEntry;` |
|        - |  5428 | `#ifdef UNTRUST` |
|        - |  5429 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5430 | `		goto Abort;` |
|        - |  5431 | `	}` |
|        - |  5432 | `#endif` |
|        - |  5433 | `	/* Point to the case table  */` |
|       38 |  5434 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5435 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5436 | `	/* Select the appropriate case block to execute */` |
|       38 |  5437 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5438 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5439 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5440 | `		pCase = &aCase[n];` |
|       92 |  5441 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5442 | `		/* Execute the case expression first */` |
|       92 |  5443 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5444 | `		/* Compare the two expression */` |
|       92 |  5445 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5446 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5447 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5448 | `		if( rc == 0 ){` |
|        - |  5449 | `			/* Value match,jump to this block */` |
|       38 |  5450 | `			pc = pCase->nStart - 1;` |
|       38 |  5451 | `			break;` |
|        - |  5452 | `		}` |
|       29 |  5453 | `	}` |
|       38 |  5454 | `	VmPopOperand(&pTos,1);` |
|       38 |  5455 | `	if( n >= nEntry ){` |
|        - |  5456 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5457 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5458 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5459 | `		}else{` |
|        - |  5460 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5461 | `			pc = pSwitch->nOut - 1;` |
|        - |  5462 | `		}` |
|      ! 0 |  5463 | `	}` |
|       38 |  5464 | `	break;` |
|        - |  5465 | `					}` |
|        - |  5466 | `/*` |
|        - |  5467 | ` * OP_CALL P1 * *` |
|        - |  5468 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5469 | ` *  function on the stack.` |
|        - |  5470 | ` */` |
|   285854 |  5471 | `case PH7_OP_CALL: {` |
|   571754 |  5472 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5473 | `	SyHashEntry *pEntry;` |
|        - |  5474 | `	SyString sName;` |
|        - |  5475 | `	/* Extract function name */` |
|   571754 |  5476 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5477 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5478 | `			ph7_value sResult;` |
|      ! 0 |  5479 | `			SySetReset(&aArg);` |
|      ! 0 |  5480 | `			while( pArg < pTos ){` |
|      ! 0 |  5481 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5482 | `				pArg++;` |
|      ! 0 |  5483 | `			}` |
|      ! 0 |  5484 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5485 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5486 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5487 | `			SySetReset(&aArg);` |
|        - |  5488 | `			/* Pop given arguments */` |
|      ! 0 |  5489 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5490 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5491 | `			}` |
|        - |  5492 | `			/* Copy result */` |
|      ! 0 |  5493 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5494 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5495 | `		}else{` |
|        3 |  5496 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5497 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5498 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5499 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5500 | `			}else{` |
|        - |  5501 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5502 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5503 | `			}` |
|        - |  5504 | `			/* Pop given arguments */` |
|        3 |  5505 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5506 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5507 | `			}` |
|        - |  5508 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5509 | `			PH7_MemObjRelease(pTos);` |
|        - |  5510 | `		}` |
|   285621 |  5511 | `		break;` |
|        - |  5512 | `	}` |
|   571752 |  5513 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5514 | `	/* Check for a compiled function first */` |
|   571752 |  5515 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|   571752 |  5516 | `	if( pEntry ){` |
|        - |  5517 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5518 | `		ph7_class_instance *pThis;` |
|        - |  5519 | `		ph7_value *pFrameStack;` |
|        - |  5520 | `		ph7_vm_func *pVmFunc;` |
|        - |  5521 | `		ph7_class *pSelf;` |
|        - |  5522 | `		VmFrame *pFrame;` |
|        - |  5523 | `		ph7_value *pObj;` |
|        - |  5524 | `		VmSlot sArg;` |
|        - |  5525 | `		sxu32 n;` |
|        - |  5526 | `		/* initialize fields */` |
|    11682 |  5527 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    11682 |  5528 | `		pThis = 0;` |
|    11682 |  5529 | `		pSelf = 0;` |
|    11682 |  5530 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5531 | `			ph7_class_method *pMeth;` |
|        - |  5532 | `			/* Class method call */` |
|     1294 |  5533 | `			ph7_value *pTarget = &pTos[-1];` |
|     1294 |  5534 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5535 | `				/* Extract the 'this' pointer */` |
|     1294 |  5536 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5537 | `					/* Instance already loaded */` |
|     1264 |  5538 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1264 |  5539 | `					pThis->iRef++;` |
|     1264 |  5540 | `					pSelf = pThis->pClass;` |
|      631 |  5541 | `				}` |
|     1294 |  5542 | `				if( pSelf == 0 ){` |
|       31 |  5543 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5544 | `						/* "Late Static Binding" class name */` |
|       37 |  5545 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       12 |  5546 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       12 |  5547 | `					}` |
|       31 |  5548 | `					if( pSelf == 0 ){` |
|        7 |  5549 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 |  5550 | `					}` |
|       15 |  5551 | `				}` |
|     1294 |  5552 | `				if( pThis == 0  ){` |
|       31 |  5553 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       31 |  5554 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5555 | `						/* Safely ignore the exception frame */` |
|      ! 0 |  5556 | `						pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  5557 | `					}` |
|       31 |  5558 | `					if( pFrameLocal->pParent ){` |
|        - |  5559 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5560 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5561 | `						if( pThis ){` |
|       13 |  5562 | `							pThis->iRef++;` |
|        6 |  5563 | `						}` |
|        9 |  5564 | `					}` |
|       15 |  5565 | `				}` |
|     1294 |  5566 | `				VmPopOperand(&pTos,1);` |
|     1294 |  5567 | `				PH7_MemObjRelease(pTos);` |
|        - |  5568 | `				/* Synchronize pointers */` |
|     1294 |  5569 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5570 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5571 | `				 * user have already computed the random generated unique class method name` |
|        - |  5572 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5573 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5574 | `				 */` |
|     1294 |  5575 | `				while( pArg < pStack ){` |
|      ! 0 |  5576 | `					pArg++;` |
|      ! 0 |  5577 | `				}` |
|     1294 |  5578 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5579 | `					/* Check if the call is allowed */` |
|     1294 |  5580 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1294 |  5581 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        5 |  5582 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5583 | `							/* Pop given arguments */` |
|      ! 0 |  5584 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5585 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5586 | `							}` |
|        - |  5587 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5588 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5589 | `							break;` |
|        - |  5590 | `						}` |
|        2 |  5591 | `					}` |
|      646 |  5592 | `				}` |
|      646 |  5593 | `			}` |
|      646 |  5594 | `		}` |
|        - |  5595 | `		/* Check The recursion limit */` |
|    11682 |  5596 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5597 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5598 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5599 | `				&pVmFunc->sName);` |
|        - |  5600 | `			/* Pop given arguments */` |
|        3 |  5601 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5602 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5603 | `			}` |
|        - |  5604 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5605 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5606 | `			break;` |
|        - |  5607 | `		}` |
|    11680 |  5608 | `		if( pVmFunc->pNextName ){` |
|        - |  5609 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      123 |  5610 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       61 |  5611 | `		}` |
|        - |  5612 | `		/* Extract the formal argument set */` |
|    11680 |  5613 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5614 | `		/* Create a new VM frame  */` |
|    11680 |  5615 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    11680 |  5616 | `		if( rc != SXRET_OK ){` |
|        - |  5617 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5618 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5619 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5620 | `				&pVmFunc->sName);` |
|        - |  5621 | `			/* Pop given arguments */` |
|      ! 0 |  5622 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5623 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5624 | `			}` |
|        - |  5625 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5626 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5627 | `			break;` |
|        - |  5628 | `		}` |
|    11680 |  5629 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5630 | `			/* Install the '$this' variable */` |
|        - |  5631 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1274 |  5632 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1274 |  5633 | `			if( pObj ){` |
|        - |  5634 | `				/* Reflect the change */` |
|     1274 |  5635 | `				pObj->x.pOther = pThis;` |
|     1274 |  5636 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      636 |  5637 | `			}` |
|      636 |  5638 | `		}` |
|    11680 |  5639 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5640 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5641 | `			/* Install static variables */` |
|      ! 0 |  5642 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5643 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5644 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5645 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5646 | `					/* Initialize the static variables */` |
|      ! 0 |  5647 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5648 | `					if( pObj ){` |
|        - |  5649 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5650 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5651 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5652 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5653 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5654 | `						}` |
|      ! 0 |  5655 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5656 | `					}else{` |
|      ! 0 |  5657 | `						continue;` |
|        - |  5658 | `					}` |
|      ! 0 |  5659 | `				}` |
|        - |  5660 | `				/* Install in the current frame */` |
|      ! 0 |  5661 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5662 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5663 | `			}` |
|      ! 0 |  5664 | `		}` |
|        - |  5665 | `		/* Push arguments in the local frame */` |
|    11680 |  5666 | `		n = 0;` |
|    32682 |  5667 | `		while( pArg < pTos ){` |
|    21004 |  5668 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    20854 |  5669 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5670 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5671 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5672 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5673 | `						goto Abort;` |
|        - |  5674 | `					}` |
|      ! 0 |  5675 | `				}` |
|        - |  5676 | `				/* Make sure the given arguments are of the correct type */` |
|    20854 |  5677 | `				if( aFormalArg[n].nType > 0 ){` |
|     1088 |  5678 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5679 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5680 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5681 | `						ph7_class *pClass;` |
|        - |  5682 | `						/* Try to extract the desired class */` |
|      ! 0 |  5683 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5684 | `						if( pClass ){` |
|      ! 0 |  5685 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5686 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5687 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5688 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5689 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5690 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5691 | `								}` |
|      ! 0 |  5692 | `							}else{` |
|        - |  5693 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5694 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5695 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5696 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5697 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5698 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5699 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5700 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5701 | `								}` |
|        - |  5702 | `							}` |
|      ! 0 |  5703 | `						}` |
|     1088 |  5704 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5705 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5706 | `						/* Cast to the desired type */` |
|      ! 0 |  5707 | `						xCast(pArg);` |
|      ! 0 |  5708 | `					}` |
|      543 |  5709 | `				}` |
|    20854 |  5710 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5711 | `					/* Pass by reference */` |
|       48 |  5712 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5713 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5714 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5715 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5716 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5717 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5718 | `						}` |
|        - |  5719 | `						/* Switch to pass by value */` |
|      ! 0 |  5720 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5721 | `					}else{` |
|        - |  5722 | `						SyHashEntry *pRefEntry;` |
|        - |  5723 | `						/* Install the referenced variable in the private function frame */` |
|       48 |  5724 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       48 |  5725 | `						if( pRefEntry == 0 ){` |
|       71 |  5726 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       46 |  5727 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       48 |  5728 | `							sArg.nIdx = pArg->nIdx;` |
|       48 |  5729 | `							sArg.pUserData = 0;` |
|       48 |  5730 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  5731 | `						}` |
|       48 |  5732 | `						pObj = 0;` |
|        - |  5733 | `					}` |
|       25 |  5734 | `				}else{` |
|        - |  5735 | `					/* Pass by value,make a copy of the given argument */` |
|    20808 |  5736 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5737 | `				}` |
|    10428 |  5738 | `			}else{` |
|        - |  5739 | `				char zName[32];` |
|        - |  5740 | `				SyString sArgName;` |
|        - |  5741 | `				/* Set a dummy name */` |
|      152 |  5742 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      152 |  5743 | `				sArgName.zString = zName;` |
|        - |  5744 | `				/* Annonymous argument */` |
|      152 |  5745 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5746 | `			}` |
|    21004 |  5747 | `			if( pObj ){` |
|    20958 |  5748 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5749 | `				/* Insert argument index  */` |
|    20958 |  5750 | `				sArg.nIdx = pObj->nIdx;` |
|    20958 |  5751 | `				sArg.pUserData = 0;` |
|    20958 |  5752 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    10478 |  5753 | `			}` |
|    21004 |  5754 | `			PH7_MemObjRelease(pArg);` |
|    21004 |  5755 | `			pArg++;` |
|    21004 |  5756 | `			++n;` |
|        2 |  5757 | `		}` |
|        - |  5758 | `		/* Set up closure environment */` |
|    11680 |  5759 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5760 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  5761 | `			ph7_value *pValue;` |
|        - |  5762 | `			sxu32 iEnv;` |
|        9 |  5763 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  5764 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  5765 | `				pEnv = &aEnv[iEnv];` |
|       17 |  5766 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  5767 | `					/* Do not install null value */` |
|        9 |  5768 | `					continue;` |
|        - |  5769 | `				}` |
|        9 |  5770 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  5771 | `				if( pValue == 0 ){` |
|      ! 0 |  5772 | `					continue;` |
|        - |  5773 | `				}` |
|        - |  5774 | `				/* Invalidate any prior representation */` |
|        9 |  5775 | `				PH7_MemObjRelease(pValue);` |
|        - |  5776 | `				/* Duplicate bound variable value */` |
|        9 |  5777 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  5778 | `			}` |
|        4 |  5779 | `		}` |
|        - |  5780 | `		/* Process default values */` |
|    13518 |  5781 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1840 |  5782 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1834 |  5783 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1834 |  5784 | `				if( pObj ){` |
|        - |  5785 | `					/* Evaluate the default value and extract it's result */` |
|     1834 |  5786 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1834 |  5787 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5788 | `						goto Abort;` |
|        - |  5789 | `					}` |
|        - |  5790 | `					/* Insert argument index */` |
|     1834 |  5791 | `					sArg.nIdx = pObj->nIdx;` |
|     1834 |  5792 | `					sArg.pUserData = 0;` |
|     1834 |  5793 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  5794 | `					/* Make sure the default argument is of the correct type */` |
|     1834 |  5795 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5796 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5797 | `						/* Cast to the desired type */` |
|      ! 0 |  5798 | `						xCast(pObj);` |
|      ! 0 |  5799 | `					}` |
|      916 |  5800 | `				}` |
|      916 |  5801 | `			}` |
|     1840 |  5802 | `			++n;` |
|        2 |  5803 | `		}` |
|        - |  5804 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  5805 | `		 * does not return anything.` |
|        - |  5806 | `		 */` |
|    11680 |  5807 | `		PH7_MemObjRelease(pTos);` |
|    11680 |  5808 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  5809 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    11680 |  5810 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    11680 |  5811 | `		if( pFrameStack == 0 ){` |
|        - |  5812 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5813 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5814 | `				&pVmFunc->sName);` |
|      ! 0 |  5815 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5816 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5817 | `			}` |
|      ! 0 |  5818 | `			break;` |
|        - |  5819 | `		}` |
|    11680 |  5820 | `		if( pSelf ){` |
|        - |  5821 | `			/* Push class name */` |
|     1292 |  5822 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      645 |  5823 | `		}` |
|        - |  5824 | `		/* Increment nesting level */` |
|    11680 |  5825 | `		pVm->nRecursionDepth++;` |
|        - |  5826 | `		/* Execute function body */` |
|    11680 |  5827 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  5828 | `		/* Decrement nesting level */` |
|    11680 |  5829 | `		pVm->nRecursionDepth--;` |
|    11680 |  5830 | `		if( pSelf ){` |
|        - |  5831 | `			/* Pop class name */` |
|     1292 |  5832 | `			(void)SySetPop(&pVm->aSelf);` |
|      645 |  5833 | `		}` |
|        - |  5834 | `		/* Cleanup the mess left behind */` |
|    11680 |  5835 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  5836 | `			/* Return by reference,reflect that */` |
|        9 |  5837 | `			if( n != SXU32_HIGH ){` |
|        9 |  5838 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  5839 | `				sxu32 i;` |
|        - |  5840 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  5841 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  5842 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  5843 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  5844 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5845 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5846 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  5847 | `								&pVmFunc->sName);` |
|      ! 0 |  5848 | `						}` |
|      ! 0 |  5849 | `						n = SXU32_HIGH;` |
|      ! 0 |  5850 | `						break;` |
|        - |  5851 | `					}` |
|        3 |  5852 | `				}` |
|        5 |  5853 | `			}else{` |
|      ! 0 |  5854 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5855 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5856 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  5857 | `						&pVmFunc->sName);` |
|      ! 0 |  5858 | `				}` |
|        - |  5859 | `			}` |
|        9 |  5860 | `			pTos->nIdx = n;` |
|        4 |  5861 | `		}` |
|        - |  5862 | `		/* Cleanup the mess left behind */` |
|    11680 |  5863 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  5864 | `			/* An exception was throw in this frame */` |
|        7 |  5865 | `			pFrame = pFrame->pParent;` |
|        7 |  5866 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  5867 | `				/* Pop the resutlt */` |
|        5 |  5868 | `				VmPopOperand(&pTos,1);` |
|        - |  5869 | `				/* Jump to this destination */` |
|        5 |  5870 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  5871 | `				rc = PH7_OK;` |
|        3 |  5872 | `			}else{` |
|        3 |  5873 | `				if( pFrame->pParent ){` |
|        3 |  5874 | `					rc = PH7_EXCEPTION;` |
|        2 |  5875 | `				}else{` |
|        - |  5876 | `					/* Continue normal execution */` |
|      ! 0 |  5877 | `					rc = PH7_OK;` |
|        - |  5878 | `				}` |
|        - |  5879 | `			}` |
|        3 |  5880 | `		}` |
|        - |  5881 | `		/* Free the operand stack */` |
|    11680 |  5882 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  5883 | `		/* Leave the frame */` |
|    11680 |  5884 | `		VmLeaveFrame(&(*pVm));` |
|    11680 |  5885 | `		if( rc == PH7_ABORT ){` |
|        - |  5886 | `			/* Abort processing immeditaley */` |
|        7 |  5887 | `			goto Abort;` |
|    11674 |  5888 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5889 | `			goto Exception;` |
|        - |  5890 | `		}` |
|     5837 |  5891 | `	}else{` |
|        - |  5892 | `		ph7_user_func *pFunc;` |
|        - |  5893 | `		ph7_context sCtx;` |
|        - |  5894 | `		ph7_value sRet;` |
|        - |  5895 | `		/* Look for an installed foreign function */` |
|   560072 |  5896 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   560072 |  5897 | `		if( pEntry == 0 ){` |
|        - |  5898 | `			/* Call to undefined function */` |
|        5 |  5899 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  5900 | `			/* Pop given arguments */` |
|        5 |  5901 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5902 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5903 | `			}` |
|        - |  5904 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  5905 | `			PH7_MemObjRelease(pTos);` |
|        8 |  5906 | `			break;` |
|        - |  5907 | `		}` |
|   560068 |  5908 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  5909 | `		/* Start collecting function arguments */` |
|   560068 |  5910 | `		SySetReset(&aArg);` |
|  1485380 |  5911 | `		while( pArg < pTos ){` |
|   925314 |  5912 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   925314 |  5913 | `			pArg++;` |
|        2 |  5914 | `		}` |
|        - |  5915 | `		/* Assume a null return value */` |
|   560068 |  5916 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  5917 | `		/* Init the call context */` |
|   560068 |  5918 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  5919 | `		/* Call the foreign function */` |
|   560068 |  5920 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5921 | `		/* Release the call context */` |
|   560068 |  5922 | `		VmReleaseCallContext(&sCtx);` |
|   560068 |  5923 | `		if( rc == PH7_ABORT ){` |
|      463 |  5924 | `			goto Abort;` |
|   559606 |  5925 | `		}else if( rc == PH7_EXCEPTION ){` |
|        7 |  5926 | `			VmFrame *pFrm = pVm->pFrame;` |
|       13 |  5927 | `			while( pFrm->pParent && (pFrm->iFlags & VM_FRAME_EXCEPTION) ){` |
|        7 |  5928 | `				pFrm = pFrm->pParent;` |
|        1 |  5929 | `			}` |
|        7 |  5930 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  5931 | `				/* Exception was NOT caught, propagate */` |
|      ! 0 |  5932 | `				goto Exception;` |
|        - |  5933 | `			}` |
|        - |  5934 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  5935 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  5936 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  5937 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  5938 | `			}` |
|        - |  5939 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  5940 | `			VmPopOperand(&pTos,1);` |
|        - |  5941 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  5942 | `			pFrm = pVm->pFrame;` |
|        7 |  5943 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  5944 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  5945 | `			}` |
|        7 |  5946 | `			break;` |
|        - |  5947 | `		}` |
|   559600 |  5948 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5949 | `			/* Pop function name and arguments */` |
|   542316 |  5950 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   271179 |  5951 | `		}` |
|        - |  5952 | `		/* Save foreign function return value */` |
|   559600 |  5953 | `		PH7_MemObjStore(&sRet,pTos);` |
|   559600 |  5954 | `		PH7_MemObjRelease(&sRet);` |
|        - |  5955 | `	}` |
|   571270 |  5956 | `	break;` |
|        - |  5957 | `				  }` |
|        - |  5958 | `/*` |
|        - |  5959 | ` * OP_CONSUME: P1 * *` |
|        - |  5960 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  5961 | ` */` |
|    10355 |  5962 | `case PH7_OP_CONSUME: {` |
|    20712 |  5963 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    20712 |  5964 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  5965 |  |
|    20712 |  5966 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    20712 |  5967 | `	pCur = pOut;` |
|        - |  5968 | `	/* Start the consume process  */` |
|    41422 |  5969 | `	while( pOut <= pTos ){` |
|        - |  5970 | `		/* Force a string cast */` |
|    20712 |  5971 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      198 |  5972 | `			PH7_MemObjToString(pOut);` |
|       98 |  5973 | `		}` |
|    20712 |  5974 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  5975 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  5976 | `			/* Invoke the output consumer callback */` |
|    11138 |  5977 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    11138 |  5978 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  5979 | `				/* Increment output length */` |
|     4692 |  5980 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     2345 |  5981 | `			}` |
|    11138 |  5982 | `			SyBlobRelease(&pOut->sBlob);` |
|    11138 |  5983 | `			if( rc == SXERR_ABORT ){` |
|        - |  5984 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  5985 | `				goto Abort;` |
|        - |  5986 | `			}` |
|     5568 |  5987 | `		}` |
|    20712 |  5988 | `		pOut++;` |
|        2 |  5989 | `	}` |
|    20712 |  5990 | `	pTos = &pCur[-1];` |
|    20710 |  5991 | `	break;` |
|        - |  5992 | `					 }` |
|        - |  5993 |  |
|        - |  5994 | `		} /* Switch() */` |
| 10170970 |  5995 | `		pc++; /* Next instruction in the stream */` |
|        2 |  5996 | `	} /* For(;;) */` |
|    14503 |  5997 | `Done:` |
|    29008 |  5998 | `	SySetRelease(&aArg);` |
|    29008 |  5999 | `	return SXRET_OK;` |
|      238 |  6000 | `Abort:` |
|      477 |  6001 | `	SySetRelease(&aArg);` |
|     1661 |  6002 | `	while( pTos >= pStack ){` |
|     1185 |  6003 | `		PH7_MemObjRelease(pTos);` |
|     1185 |  6004 | `		pTos--;` |
|        1 |  6005 | `	}` |
|      477 |  6006 | `	return PH7_ABORT;` |
|        1 |  6007 | `Exception:` |
|        3 |  6008 | `	SySetRelease(&aArg);` |
|        5 |  6009 | `	while( pTos >= pStack ){` |
|        3 |  6010 | `		PH7_MemObjRelease(pTos);` |
|        3 |  6011 | `		pTos--;` |
|        1 |  6012 | `	}` |
|        3 |  6013 | `	return PH7_EXCEPTION;` |
|    14744 |  6014 |  |
|        - |  6015 | `/*` |
|        - |  6016 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6017 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6018 | ` * See block-comment on that function for additional information.` |
|        - |  6019 | ` */` |
|    14188 |  6020 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6021 |  |
|        - |  6022 | `	ph7_value *pStack;` |
|        - |  6023 | `	sxi32 rc;` |
|        - |  6024 | `	/* Allocate a new operand stack */` |
|    14190 |  6025 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    14190 |  6026 | `	if( pStack == 0 ){` |
|      ! 0 |  6027 | `		return SXERR_MEM;` |
|        - |  6028 | `	}` |
|        - |  6029 | `	/* Execute the program */` |
|    14190 |  6030 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  6031 | `	/* Free the operand stack */` |
|    14190 |  6032 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6033 | `	/* Execution result */` |
|    14190 |  6034 | `	return rc;` |
|     7096 |  6035 |  |
|        - |  6036 | `/*` |
|        - |  6037 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6038 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6039 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6040 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6041 | ` * execution ends.` |
|        - |  6042 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6043 | ` * additional information.` |
|        - |  6044 | ` */` |
|     2026 |  6045 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6046 |  |
|        - |  6047 | `	VmShutdownCB *pEntry;` |
|        - |  6048 | `	ph7_value *apArg[10];` |
|        - |  6049 | `	sxu32 n,nEntry;` |
|        - |  6050 | `	int i;` |
|        - |  6051 | `	/* Point to the stack of registered callbacks */` |
|     2028 |  6052 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    22288 |  6053 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    20262 |  6054 | `		apArg[i] = 0;` |
|    10132 |  6055 | `	}` |
|     2030 |  6056 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6057 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6058 | `		if( pEntry ){` |
|        - |  6059 | `			/* Prepare callback arguments if any */` |
|        3 |  6060 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6061 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6062 | `					break;` |
|        - |  6063 | `				}` |
|      ! 0 |  6064 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6065 | `			}` |
|        - |  6066 | `			/* Invoke the callback */` |
|        3 |  6067 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6068 | `			/*` |
|        - |  6069 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6070 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6071 | `			 */` |
|        3 |  6072 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6073 | `			if( pEntry ){` |
|        3 |  6074 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6075 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6076 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6077 | `				}` |
|        1 |  6078 | `			}` |
|        1 |  6079 | `		}` |
|        2 |  6080 | `	}` |
|     2028 |  6081 | `	SySetReset(&pVm->aShutdown);` |
|     2028 |  6082 |  |
|        - |  6083 | `/*` |
|        - |  6084 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6085 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6086 | ` * See block-comment on that function for additional information.` |
|        - |  6087 | ` */` |
|     2034 |  6088 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6089 |  |
|        - |  6090 | `	/* Make sure we are ready to execute this program */` |
|     2036 |  6091 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6092 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6093 | `	}` |
|        - |  6094 | `	/* Set the execution magic number  */` |
|     2036 |  6095 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6096 | `	/* Execute the program */` |
|     2036 |  6097 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6098 | `	/* Invoke any shutdown callbacks */` |
|     2032 |  6099 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6100 | `	/*` |
|        - |  6101 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6102 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6103 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6104 | `	 */` |
|     2032 |  6105 | `	return SXRET_OK;` |
|     1019 |  6106 |  |
|        - |  6107 | `/*` |
|        - |  6108 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6109 | ` * the desired message.` |
|        - |  6110 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6111 | ` * in 'api.c' for additional information.` |
|        - |  6112 | ` */` |
|      350 |  6113 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6114 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6115 | `	SyString *pString /* Message to output */` |
|        - |  6116 | `	)` |
|        2 |  6117 |  |
|      352 |  6118 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      352 |  6119 | `	sxi32 rc = SXRET_OK;` |
|        - |  6120 | `	/* Call the output consumer */` |
|      352 |  6121 | `	if( pString->nByte > 0 ){` |
|      352 |  6122 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      352 |  6123 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6124 | `			/* Increment output length */` |
|       17 |  6125 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6126 | `		}` |
|      175 |  6127 | `	}` |
|      352 |  6128 | `	return rc;` |
|        2 |  6129 |  |
|        - |  6130 | `/*` |
|        - |  6131 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6132 | ` * callback to consume the formatted message.` |
|        - |  6133 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6134 | ` * in 'api.c' for additional information.` |
|        - |  6135 | ` */` |
|        2 |  6136 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6137 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6138 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6139 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6140 | `	)` |
|        1 |  6141 |  |
|        3 |  6142 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6143 | `	sxi32 rc = SXRET_OK;` |
|        - |  6144 | `	SyBlob sWorker;` |
|        - |  6145 | `	/* Format the message and call the output consumer */` |
|        3 |  6146 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6147 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6148 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6149 | `		/* Consume the formatted message */` |
|        3 |  6150 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6151 | `	}` |
|        3 |  6152 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6153 | `		/* Increment output length */` |
|      ! 0 |  6154 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6155 | `	}` |
|        - |  6156 | `	/* Release the working buffer */` |
|        3 |  6157 | `	SyBlobRelease(&sWorker);` |
|        3 |  6158 | `	return rc;` |
|        1 |  6159 |  |
|        - |  6160 | `/*` |
|        - |  6161 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6162 | ` * This function never fail and always return a pointer` |
|        - |  6163 | ` * to a null terminated string.` |
|        - |  6164 | ` */` |
|       10 |  6165 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6166 |  |
|       11 |  6167 | `	const char *zOp = "Unknown     ";` |
|       11 |  6168 | `	switch(nOp){` |
|        3 |  6169 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6170 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6171 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6172 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6173 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6174 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6175 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6176 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6177 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6178 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6179 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6180 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6181 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6182 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6183 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6184 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6185 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6186 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6187 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6188 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6189 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6190 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6191 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6192 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6193 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6194 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6195 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6196 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6197 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6198 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6199 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6200 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6201 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6202 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6203 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6204 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6205 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6206 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6207 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6208 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6209 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6210 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6211 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6212 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6213 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6214 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6215 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6216 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6217 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6218 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|      ! 0 |  6219 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6220 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6221 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6222 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6223 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6224 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6225 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6226 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6227 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6228 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6229 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6230 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6231 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6232 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6233 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6234 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6235 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6236 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6237 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6238 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6239 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6240 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6241 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6242 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6243 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6244 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6245 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6246 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6247 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6248 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6249 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6250 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6251 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6252 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6253 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6254 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6255 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6256 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6257 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6258 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6259 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6260 | `	default:` |
|      ! 0 |  6261 | `		break;` |
|        - |  6262 | `	}` |
|       11 |  6263 | `	return zOp;` |
|        1 |  6264 |  |
|        - |  6265 | `/*` |
|        - |  6266 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6267 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6268 | ` * is responsible of consuming the generated dump.` |
|        - |  6269 | ` */` |
|        2 |  6270 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6271 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6272 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6273 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6274 | `	)` |
|        1 |  6275 |  |
|        - |  6276 | `	sxi32 rc;` |
|        3 |  6277 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6278 | `	return rc;` |
|        1 |  6279 |  |
|        - |  6280 | `/*` |
|        - |  6281 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6282 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6283 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6284 | ` * in 'compile.c' for additional information.` |
|        - |  6285 | ` */` |
|        8 |  6286 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6287 |  |
|        9 |  6288 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6289 | `	/* Evaluate and expand constant value */` |
|        9 |  6290 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6291 |  |
|        - |  6292 | `/*` |
|        - |  6293 | ` * Section:` |
|        - |  6294 | ` *  Function handling functions.` |
|        - |  6295 | ` * Status:` |
|        - |  6296 | ` *    Stable.` |
|        - |  6297 | ` */` |
|        - |  6298 | `/*` |
|        - |  6299 | ` * int func_num_args(void)` |
|        - |  6300 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6301 | ` * Parameters` |
|        - |  6302 | ` *   None.` |
|        - |  6303 | ` * Return` |
|        - |  6304 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6305 | ` *  or -1 if called from the globe scope.` |
|        - |  6306 | ` */` |
|      906 |  6307 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6308 |  |
|        - |  6309 | `	VmFrame *pFrame;` |
|        - |  6310 | `	ph7_vm *pVm;` |
|        - |  6311 | `	/* Point to the target VM */` |
|      908 |  6312 | `	pVm = pCtx->pVm;` |
|        - |  6313 | `	/* Current frame */` |
|      908 |  6314 | `	pFrame = pVm->pFrame;` |
|      908 |  6315 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6316 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6317 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6318 | `	}` |
|      908 |  6319 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6320 | `		SXUNUSED(nArg);` |
|      ! 0 |  6321 | `		SXUNUSED(apArg);` |
|        - |  6322 | `		/* Global frame,return -1 */` |
|      ! 0 |  6323 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6324 | `		return SXRET_OK;` |
|        - |  6325 | `	}` |
|        - |  6326 | `	/* Total number of arguments passed to the enclosing function */` |
|      908 |  6327 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      908 |  6328 | `	ph7_result_int(pCtx,nArg);` |
|      908 |  6329 | `	return SXRET_OK;` |
|      455 |  6330 |  |
|        - |  6331 | `/*` |
|        - |  6332 | ` * value func_get_arg(int $arg_num)` |
|        - |  6333 | ` *   Return an item from the argument list.` |
|        - |  6334 | ` * Parameters` |
|        - |  6335 | ` *  Argument number(index start from zero).` |
|        - |  6336 | ` * Return` |
|        - |  6337 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6338 | ` */` |
|       22 |  6339 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6340 |  |
|       24 |  6341 | `	ph7_value *pObj = 0;` |
|       24 |  6342 | `	VmSlot *pSlot = 0;` |
|        - |  6343 | `	VmFrame *pFrame;` |
|        - |  6344 | `	ph7_vm *pVm;` |
|        - |  6345 | `	/* Point to the target VM */` |
|       24 |  6346 | `	pVm = pCtx->pVm;` |
|        - |  6347 | `	/* Current frame */` |
|       24 |  6348 | `	pFrame = pVm->pFrame;` |
|       24 |  6349 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6350 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6351 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6352 | `	}` |
|       24 |  6353 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6354 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6355 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6356 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6357 | `		return SXRET_OK;` |
|        - |  6358 | `	}` |
|        - |  6359 | `	/* Extract the desired index */` |
|       21 |  6360 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  6361 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6362 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6363 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6364 | `		return SXRET_OK;` |
|        - |  6365 | `	}` |
|        - |  6366 | `	/* Extract the desired argument */` |
|       21 |  6367 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  6368 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6369 | `			/* Return the desired argument */` |
|       21 |  6370 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  6371 | `		}else{` |
|        - |  6372 | `			/* No such argument,return false */` |
|      ! 0 |  6373 | `			ph7_result_bool(pCtx,0);` |
|        - |  6374 | `		}` |
|       11 |  6375 | `	}else{` |
|        - |  6376 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6377 | `		ph7_result_bool(pCtx,0);` |
|        - |  6378 | `	}` |
|       21 |  6379 | `	return SXRET_OK;` |
|       13 |  6380 |  |
|        - |  6381 | `/*` |
|        - |  6382 | ` * array func_get_args_byref(void)` |
|        - |  6383 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6384 | ` * Parameters` |
|        - |  6385 | ` *  None.` |
|        - |  6386 | ` * Return` |
|        - |  6387 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6388 | ` *  member of the current user-defined function's argument list.` |
|        - |  6389 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6390 | ` * NOTE:` |
|        - |  6391 | ` *  Arguments are returned to the array by reference.` |
|        - |  6392 | ` */` |
|        2 |  6393 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6394 |  |
|        - |  6395 | `	ph7_value *pArray;` |
|        - |  6396 | `	VmFrame *pFrame;` |
|        - |  6397 | `	VmSlot *aSlot;` |
|        - |  6398 | `	sxu32 n;` |
|        - |  6399 | `	/* Point to the current frame */` |
|        3 |  6400 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6401 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6402 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6403 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6404 | `	}` |
|        3 |  6405 | `	if( pFrame->pParent == 0 ){` |
|        - |  6406 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6407 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6408 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6409 | `		return SXRET_OK;` |
|        - |  6410 | `	}` |
|        - |  6411 | `	/* Create a new array */` |
|        3 |  6412 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6413 | `	if( pArray == 0 ){` |
|      ! 0 |  6414 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6415 | `		SXUNUSED(apArg);` |
|      ! 0 |  6416 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6417 | `		return SXRET_OK;` |
|        - |  6418 | `	}` |
|        - |  6419 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6420 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6421 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6422 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6423 | `	}` |
|        - |  6424 | `	/* Return the freshly created array */` |
|        3 |  6425 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6426 | `	return SXRET_OK;` |
|        2 |  6427 |  |
|        - |  6428 | `/*` |
|        - |  6429 | ` * array func_get_args(void)` |
|        - |  6430 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6431 | ` * Parameters` |
|        - |  6432 | ` *  None.` |
|        - |  6433 | ` * Return` |
|        - |  6434 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6435 | ` *  member of the current user-defined function's argument list.` |
|        - |  6436 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6437 | ` */` |
|       62 |  6438 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6439 |  |
|       64 |  6440 | `	ph7_value *pObj = 0;` |
|        - |  6441 | `	ph7_value *pArray;` |
|        - |  6442 | `	VmFrame *pFrame;` |
|        - |  6443 | `	VmSlot *aSlot;` |
|        - |  6444 | `	sxu32 n;` |
|        - |  6445 | `	/* Point to the current frame */` |
|       64 |  6446 | `	pFrame = pCtx->pVm->pFrame;` |
|       64 |  6447 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6448 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6449 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6450 | `	}` |
|       64 |  6451 | `	if( pFrame->pParent == 0 ){` |
|        - |  6452 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6453 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6454 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6455 | `		return SXRET_OK;` |
|        - |  6456 | `	}` |
|        - |  6457 | `	/* Create a new array */` |
|       64 |  6458 | `	pArray = ph7_context_new_array(pCtx);` |
|       64 |  6459 | `	if( pArray == 0 ){` |
|      ! 0 |  6460 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6461 | `		SXUNUSED(apArg);` |
|      ! 0 |  6462 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6463 | `		return SXRET_OK;` |
|        - |  6464 | `	}` |
|        - |  6465 | `	/* Start filling the array with the given arguments */` |
|       64 |  6466 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      192 |  6467 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      130 |  6468 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      130 |  6469 | `		if( pObj ){` |
|      130 |  6470 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       64 |  6471 | `		}` |
|       66 |  6472 | `	}` |
|        - |  6473 | `	/* Return the freshly created array */` |
|       64 |  6474 | `	ph7_result_value(pCtx,pArray);` |
|       64 |  6475 | `	return SXRET_OK;` |
|       33 |  6476 |  |
|        - |  6477 | `/*` |
|        - |  6478 | ` * bool function_exists(string $name)` |
|        - |  6479 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6480 | ` * Parameters` |
|        - |  6481 | ` *  The name of the desired function.` |
|        - |  6482 | ` * Return` |
|        - |  6483 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6484 | ` */` |
|     1648 |  6485 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6486 |  |
|        - |  6487 | `	const char *zName;` |
|        - |  6488 | `	ph7_vm *pVm;` |
|        - |  6489 | `	int nLen;` |
|        - |  6490 | `	int res;` |
|     1650 |  6491 | `	if( nArg < 1 ){` |
|        - |  6492 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6493 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6494 | `		return SXRET_OK;` |
|        - |  6495 | `	}` |
|        - |  6496 | `	/* Point to the target VM */` |
|     1650 |  6497 | `	pVm = pCtx->pVm;` |
|        - |  6498 | `	/* Extract the function name */` |
|     1650 |  6499 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6500 | `	/* Assume the function is not defined */` |
|     1650 |  6501 | `	res = 0;` |
|        - |  6502 | `	/* Perform the lookup */` |
|     2472 |  6503 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1644 |  6504 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6505 | `			/* Function is defined */` |
|      202 |  6506 | `			res = 1;` |
|      100 |  6507 | `	}` |
|     1650 |  6508 | `	ph7_result_bool(pCtx,res);` |
|     1650 |  6509 | `	return SXRET_OK;` |
|      826 |  6510 |  |
|        - |  6511 | `/*` |
|        - |  6512 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6513 | ` * [i.e: Whether it is callable or not].` |
|        - |  6514 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6515 | ` */` |
|    16002 |  6516 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6517 |  |
|    16004 |  6518 | `	int res = 0;` |
|    16004 |  6519 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6520 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6521 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6522 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6523 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6524 | `		if( pMethod && CallInvoke ){` |
|        - |  6525 | `			ph7_value sResult;` |
|        - |  6526 | `			sxi32 rc;` |
|        - |  6527 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6528 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6529 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6530 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6531 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6532 | `			}` |
|      ! 0 |  6533 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6534 | `		}` |
|    16004 |  6535 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  6536 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  6537 | `		if( pMap->nEntry == 2 ){` |
|        - |  6538 | `			ph7_class *pClass;` |
|        - |  6539 | `			ph7_value *pV;` |
|        - |  6540 | `			/* Extract the target class */` |
|       12 |  6541 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  6542 | `			if( pV ){` |
|       12 |  6543 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  6544 | `				if( pClass ){` |
|        - |  6545 | `					ph7_class_method *pMethod;` |
|        - |  6546 | `					/* Extract the target method */` |
|       10 |  6547 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  6548 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6549 | `						/* Perform the lookup */` |
|       10 |  6550 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  6551 | `						if( pMethod ){` |
|        - |  6552 | `							/* Method is callable */` |
|        5 |  6553 | `							res = 1;` |
|        2 |  6554 | `						}` |
|        4 |  6555 | `					}` |
|        4 |  6556 | `				}` |
|        5 |  6557 | `			}` |
|        7 |  6558 | `		}` |
|    15991 |  6559 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6560 | `		const char *zName;` |
|        - |  6561 | `		int nLen;` |
|        - |  6562 | `		/* Extract the name */` |
|     4700 |  6563 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6564 | `		/* Perform the lookup */` |
|     4715 |  6565 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  6566 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6567 | `				/* Function is callable */` |
|     4682 |  6568 | `				res = 1;` |
|     2340 |  6569 | `		}` |
|     2349 |  6570 | `	}` |
|    16004 |  6571 | `	return res;` |
|        2 |  6572 |  |
|        - |  6573 | `/*` |
|        - |  6574 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6575 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6576 | ` * Parameters` |
|        - |  6577 | ` * $name` |
|        - |  6578 | ` *    The callback function to check` |
|        - |  6579 | ` * $syntax_only` |
|        - |  6580 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6581 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6582 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6583 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6584 | ` *    a string.` |
|        - |  6585 | ` * Return` |
|        - |  6586 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6587 | ` */` |
|       14 |  6588 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6589 |  |
|        - |  6590 | `	ph7_vm *pVm;` |
|        - |  6591 | `	int res;` |
|       15 |  6592 | `	if( nArg < 1 ){` |
|        - |  6593 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6594 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6595 | `		return SXRET_OK;` |
|        - |  6596 | `	}` |
|        - |  6597 | `	/* Point to the target VM */` |
|       15 |  6598 | `	pVm = pCtx->pVm;` |
|        - |  6599 | `	/* Perform the requested operation */` |
|       15 |  6600 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6601 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6602 | `	return SXRET_OK;` |
|        8 |  6603 |  |
|        - |  6604 | `/*` |
|        - |  6605 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6606 | ` * defined below.` |
|        - |  6607 | ` */` |
|     1082 |  6608 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6609 |  |
|     1083 |  6610 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6611 | `	ph7_value sName;` |
|        - |  6612 | `	sxi32 rc;` |
|        - |  6613 | `	/* Prepare the function name for insertion */` |
|     1083 |  6614 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1083 |  6615 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6616 | `	/* Perform the insertion */` |
|     1083 |  6617 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1083 |  6618 | `	PH7_MemObjRelease(&sName);` |
|     1083 |  6619 | `	return rc;` |
|        1 |  6620 |  |
|        - |  6621 | `/*` |
|        - |  6622 | ` * array get_defined_functions(void)` |
|        - |  6623 | ` *  Returns an array of all defined functions.` |
|        - |  6624 | ` * Parameter` |
|        - |  6625 | ` *  None.` |
|        - |  6626 | ` * Return` |
|        - |  6627 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6628 | ` *  both built-in (internal) and user-defined.` |
|        - |  6629 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6630 | ` *  defined ones using $arr["user"].` |
|        - |  6631 | ` * Note:` |
|        - |  6632 | ` *  NULL is returned on failure.` |
|        - |  6633 | ` */` |
|        2 |  6634 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6635 |  |
|        - |  6636 | `	ph7_value *pArray,*pEntry;` |
|        - |  6637 | `	/* NOTE:` |
|        - |  6638 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6639 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6640 | `	 */` |
|        3 |  6641 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6642 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6643 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6644 | `		SXUNUSED(apArg);` |
|        - |  6645 | `		/* Return NULL */` |
|      ! 0 |  6646 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6647 | `		return SXRET_OK;` |
|        - |  6648 | `	}` |
|        3 |  6649 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6650 | `	if( pEntry == 0 ){` |
|        - |  6651 | `		/* Return NULL */` |
|      ! 0 |  6652 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6653 | `		return SXRET_OK;` |
|        - |  6654 | `	}` |
|        - |  6655 | `	/* Fill with the appropriate information */` |
|        3 |  6656 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6657 | `	/* Create the 'internal' index */` |
|        3 |  6658 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6659 | `	/* Create the user-func array */` |
|        3 |  6660 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6661 | `	if( pEntry == 0 ){` |
|        - |  6662 | `		/* Return NULL */` |
|      ! 0 |  6663 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6664 | `		return SXRET_OK;` |
|        - |  6665 | `	}` |
|        - |  6666 | `	/* Fill with the appropriate information */` |
|        3 |  6667 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6668 | `	/* Create the 'user' index */` |
|        3 |  6669 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6670 | `	/* Return the multi-dimensional array */` |
|        3 |  6671 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6672 | `	return SXRET_OK;` |
|        2 |  6673 |  |
|        - |  6674 | `/*` |
|        - |  6675 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6676 | ` *  Register a function for execution on shutdown.` |
|        - |  6677 | ` * Note` |
|        - |  6678 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6679 | ` *  be called in the same order as they were registered.` |
|        - |  6680 | ` * Parameters` |
|        - |  6681 | ` *  $callback` |
|        - |  6682 | ` *   The shutdown callback to register.` |
|        - |  6683 | ` * $param` |
|        - |  6684 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6685 | ` * Return` |
|        - |  6686 | ` *  Nothing.` |
|        - |  6687 | ` */` |
|        2 |  6688 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6689 |  |
|        - |  6690 | `	VmShutdownCB sEntry;` |
|        - |  6691 | `	int i,j;` |
|        3 |  6692 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6693 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6694 | `		return PH7_OK;` |
|        - |  6695 | `	}` |
|        - |  6696 | `	/* Zero the Entry */` |
|        3 |  6697 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6698 | `	/* Initialize fields */` |
|        3 |  6699 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6700 | `	/* Save the callback name for later invocation name */` |
|        3 |  6701 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6702 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6703 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6704 | `	}` |
|        - |  6705 | `	/* Copy arguments */` |
|        3 |  6706 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6707 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6708 | `			/* Limit reached */` |
|      ! 0 |  6709 | `			break;` |
|        - |  6710 | `		}` |
|      ! 0 |  6711 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6712 | `	}` |
|        3 |  6713 | `	sEntry.nArg = j;` |
|        - |  6714 | `	/* Install the callback */` |
|        3 |  6715 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6716 | `	return PH7_OK;` |
|        2 |  6717 |  |
|        - |  6718 | `/*` |
|        - |  6719 | ` * Section:` |
|        - |  6720 | ` *  Class handling functions.` |
|        - |  6721 | ` * Status:` |
|        - |  6722 | ` *    Stable.` |
|        - |  6723 | ` */` |
|        - |  6724 | `/*` |
|        - |  6725 | ` * Extract the top active class. NULL is returned` |
|        - |  6726 | ` * if the class stack is empty.` |
|        - |  6727 | ` */` |
|      516 |  6728 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6729 |  |
|      518 |  6730 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6731 | `	ph7_class **apClass;` |
|      518 |  6732 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6733 | `		/* Empty stack,return NULL */` |
|       15 |  6734 | `		return 0;` |
|        - |  6735 | `	}` |
|        - |  6736 | `	/* Peek the last entry */` |
|      504 |  6737 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      504 |  6738 | `	return apClass[pSet->nUsed - 1];` |
|      260 |  6739 |  |
|        - |  6740 | `/*` |
|        - |  6741 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  6742 | ` *   Get the class that declared the currently executing method.` |
|        - |  6743 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  6744 | ` *` |
|        - |  6745 | ` * Parameters` |
|        - |  6746 | ` *   pVm: Target VM` |
|        - |  6747 | ` *` |
|        - |  6748 | ` * Return` |
|        - |  6749 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  6750 | ` *   - Not executing within a class method` |
|        - |  6751 | ` *` |
|        - |  6752 | ` * Note` |
|        - |  6753 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  6754 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  6755 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  6756 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  6757 | ` *   declaring class.` |
|        - |  6758 | ` */` |
|       18 |  6759 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        1 |  6760 |  |
|       19 |  6761 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  6762 | `	ph7_vm_func *pVmFunc;` |
|        - |  6763 |  |
|        - |  6764 | `	/* Skip exception frames to find the actual method frame */` |
|       19 |  6765 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  6766 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6767 | `	}` |
|        - |  6768 |  |
|        - |  6769 | `	/* Check if we're in a method context */` |
|       19 |  6770 | `	if( pFrame->pParent ){` |
|       15 |  6771 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       15 |  6772 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  6773 | `			/* Return the declaring class */` |
|       15 |  6774 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  6775 | `		}` |
|      ! 0 |  6776 | `	}` |
|        - |  6777 |  |
|        5 |  6778 | `	return 0;` |
|       10 |  6779 |  |
|        - |  6780 |  |
|        - |  6781 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  6782 | `/*` |
|        - |  6783 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  6784 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  6785 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  6786 | ` * return value indicates failure.` |
|        - |  6787 | ` */` |
|     1146 |  6788 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  6789 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  6790 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  6791 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  6792 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  6793 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  6794 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  6795 | `	)` |
|        2 |  6796 |  |
|        - |  6797 | `	ph7_value *aStack;` |
|        - |  6798 | `	VmInstr aInstr[2];` |
|        - |  6799 | `	int iCursor;` |
|        - |  6800 | `	int i;` |
|        - |  6801 | `	/* Create a new operand stack */` |
|     1148 |  6802 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1148 |  6803 | `	if( aStack == 0 ){` |
|      ! 0 |  6804 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6805 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  6806 | `		return SXERR_MEM;` |
|        - |  6807 | `	}` |
|        - |  6808 | `	/* Fill the operand stack with the given arguments */` |
|     1694 |  6809 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      548 |  6810 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  6811 | `		/*` |
|        - |  6812 | `		 * Symisc eXtension:` |
|        - |  6813 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  6814 | `		 */` |
|      548 |  6815 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      275 |  6816 | `	}` |
|     1148 |  6817 | `	iCursor = nArg + 1;` |
|     1148 |  6818 | `	if( pThis ){` |
|        - |  6819 | `		/*` |
|        - |  6820 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  6821 | `		 */` |
|     1142 |  6822 | `		pThis->iRef++; /* Increment reference count */` |
|     1142 |  6823 | `		aStack[i].x.pOther = pThis;` |
|     1142 |  6824 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      570 |  6825 | `	}` |
|     1148 |  6826 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1148 |  6827 | `	i++;` |
|        - |  6828 | `	/* Push method name */` |
|     1148 |  6829 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1148 |  6830 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1148 |  6831 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1148 |  6832 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  6833 | `	/* Emit the CALL istruction */` |
|     1148 |  6834 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1148 |  6835 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1148 |  6836 | `	aInstr[0].iP2 = 0;` |
|     1148 |  6837 | `	aInstr[0].p3  = 0;` |
|        - |  6838 | `	/* Emit the DONE instruction */` |
|     1148 |  6839 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1148 |  6840 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1148 |  6841 | `	aInstr[1].iP2 = 0;` |
|     1148 |  6842 | `	aInstr[1].p3  = 0;` |
|        - |  6843 | `	/* Execute the method body (if available) */` |
|     1148 |  6844 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  6845 | `	/* Clean up the mess left behind */` |
|     1148 |  6846 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1148 |  6847 | `	return PH7_OK;` |
|      575 |  6848 |  |
|        - |  6849 | `/*` |
|        - |  6850 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  6851 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  6852 | ` * in the apArg[] array.` |
|        - |  6853 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  6854 | ` * return value indicates failure.` |
|        - |  6855 | ` */` |
|      926 |  6856 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  6857 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  6858 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  6859 | `	int nArg,          /* Total number of given arguments */` |
|        - |  6860 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  6861 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  6862 | `	)` |
|        2 |  6863 |  |
|        - |  6864 | `	ph7_value *aStack;` |
|        - |  6865 | `	VmInstr aInstr[2];` |
|        - |  6866 | `	int i;` |
|      928 |  6867 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6868 | `		/* Don't bother processing,it's invalid anyway */` |
|      471 |  6869 | `		if( pResult ){` |
|        - |  6870 | `			/* Assume a null return value */` |
|      ! 0 |  6871 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  6872 | `		}` |
|      471 |  6873 | `		return SXERR_INVALID;` |
|        - |  6874 | `	}` |
|      458 |  6875 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6876 | `		/* Class method */` |
|       11 |  6877 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  6878 | `		ph7_class_method *pMethod = 0;` |
|       11 |  6879 | `		ph7_class_instance *pThis = 0;` |
|       11 |  6880 | `		ph7_class *pClass = 0;` |
|        - |  6881 | `		ph7_value *pValue;` |
|        - |  6882 | `		sxi32 rc;` |
|       11 |  6883 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  6884 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  6885 | `			if( pResult ){` |
|        - |  6886 | `				/* Assume a null return value */` |
|      ! 0 |  6887 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  6888 | `			}` |
|      ! 0 |  6889 | `			return SXRET_OK;` |
|        - |  6890 | `		}` |
|        - |  6891 | `		/* Extract the class name or an instance of it */` |
|       11 |  6892 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  6893 | `		if( pValue ){` |
|       11 |  6894 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  6895 | `		}` |
|       11 |  6896 | `		if( pClass == 0 ){` |
|        - |  6897 | `			/* No such class,return NULL */` |
|      ! 0 |  6898 | `			if( pResult ){` |
|      ! 0 |  6899 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  6900 | `			}` |
|      ! 0 |  6901 | `			return SXRET_OK;` |
|        - |  6902 | `		}` |
|       11 |  6903 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6904 | `			/* Point to the class instance */` |
|        5 |  6905 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  6906 | `		}` |
|        - |  6907 | `		/* Try to extract the method */` |
|       11 |  6908 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  6909 | `		if( pValue ){` |
|       11 |  6910 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  6911 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  6912 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  6913 | `			}` |
|        5 |  6914 | `		}` |
|       11 |  6915 | `		if( pMethod == 0 ){` |
|        - |  6916 | `			/* No such method,return NULL */` |
|      ! 0 |  6917 | `			if( pResult ){` |
|      ! 0 |  6918 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  6919 | `			}` |
|      ! 0 |  6920 | `			return SXRET_OK;` |
|        - |  6921 | `		}` |
|        - |  6922 | `		/* Call the class method */` |
|       11 |  6923 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  6924 | `		return rc;` |
|        - |  6925 | `	}` |
|        - |  6926 | `	/* Create a new operand stack */` |
|      448 |  6927 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      448 |  6928 | `	if( aStack == 0 ){` |
|      ! 0 |  6929 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6930 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  6931 | `		if( pResult ){` |
|        - |  6932 | `			/* Assume a null return value */` |
|      ! 0 |  6933 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  6934 | `		}` |
|      ! 0 |  6935 | `		return SXERR_MEM;` |
|        - |  6936 | `	}` |
|        - |  6937 | `	/* Fill the operand stack with the given arguments */` |
|     1470 |  6938 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1024 |  6939 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  6940 | `		/*` |
|        - |  6941 | `		 * Symisc eXtension:` |
|        - |  6942 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  6943 | `		 */` |
|     1024 |  6944 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      513 |  6945 | `	}` |
|        - |  6946 | `	/* Push the function name */` |
|      448 |  6947 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      448 |  6948 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  6949 | `	/* Emit the CALL istruction */` |
|      448 |  6950 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      448 |  6951 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      448 |  6952 | `	aInstr[0].iP2 = 0;` |
|      448 |  6953 | `	aInstr[0].p3  = 0;` |
|        - |  6954 | `	/* Emit the DONE instruction */` |
|      448 |  6955 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      448 |  6956 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      448 |  6957 | `	aInstr[1].iP2 = 0;` |
|      448 |  6958 | `	aInstr[1].p3  = 0;` |
|        - |  6959 | `	/* Execute the function body (if available) */` |
|      448 |  6960 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  6961 | `	/* Clean up the mess left behind */` |
|      448 |  6962 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      448 |  6963 | `	return PH7_OK;` |
|      465 |  6964 |  |
|        - |  6965 | `/*` |
|        - |  6966 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  6967 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  6968 | ` * parameter.` |
|        - |  6969 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  6970 | ` * return value indicates failure.` |
|        - |  6971 | ` */` |
|      236 |  6972 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  6973 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  6974 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  6975 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  6976 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  6977 | `	)` |
|        1 |  6978 |  |
|        - |  6979 | `	ph7_value *pArg;` |
|        - |  6980 | `	SySet aArg;` |
|        - |  6981 | `	va_list ap;` |
|        - |  6982 | `	sxi32 rc;` |
|      237 |  6983 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  6984 | `	/* Copy arguments one after one */` |
|      237 |  6985 | `	va_start(ap,pResult);` |
|      393 |  6986 | `	for(;;){` |
|      787 |  6987 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  6988 | `		if( pArg == 0 ){` |
|      237 |  6989 | `			break;` |
|        - |  6990 | `		}` |
|      551 |  6991 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  6992 | `	}` |
|        - |  6993 | `	/* Call the core routine */` |
|      237 |  6994 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  6995 | `	/* Cleanup */` |
|      237 |  6996 | `	SySetRelease(&aArg);` |
|      237 |  6997 | `	return rc;` |
|        1 |  6998 |  |
|        - |  6999 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  7000 | `/*` |
|        - |  7001 | ` * bool defined(string $name)` |
|        - |  7002 | ` *  Checks whether a given named constant exists.` |
|        - |  7003 | ` * Parameter:` |
|        - |  7004 | ` *  Name of the desired constant.` |
|        - |  7005 | ` * Return` |
|        - |  7006 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  7007 | ` */` |
|       14 |  7008 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7009 |  |
|        - |  7010 | `	const char *zName;` |
|       16 |  7011 | `	int nLen = 0;` |
|       16 |  7012 | `	int res = 0;` |
|       16 |  7013 | `	if( nArg < 1 ){` |
|        - |  7014 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  7015 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  7016 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7017 | `		return SXRET_OK;` |
|        - |  7018 | `	}` |
|        - |  7019 | `	/* Extract constant name */` |
|       16 |  7020 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7021 | `	/* Perform the lookup */` |
|       16 |  7022 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7023 | `		/* Already defined */` |
|       10 |  7024 | `		res = 1;` |
|        4 |  7025 | `	}` |
|       16 |  7026 | `	ph7_result_bool(pCtx,res);` |
|       16 |  7027 | `	return SXRET_OK;` |
|        9 |  7028 |  |
|        - |  7029 | `/*` |
|        - |  7030 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  7031 | ` * below.` |
|        - |  7032 | ` */` |
|        8 |  7033 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  7034 |  |
|       10 |  7035 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7036 | `	/* Expand constant value */` |
|       10 |  7037 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7038 |  |
|        - |  7039 | `/*` |
|        - |  7040 | ` * bool define(string $constant_name,expression value)` |
|        - |  7041 | ` *  Defines a named constant at runtime.` |
|        - |  7042 | ` * Parameter:` |
|        - |  7043 | ` *  $constant_name` |
|        - |  7044 | ` *   The name of the constant` |
|        - |  7045 | ` *  $value` |
|        - |  7046 | ` *   Constant value` |
|        - |  7047 | ` * Return:` |
|        - |  7048 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7049 | ` */` |
|       10 |  7050 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7051 |  |
|        - |  7052 | `	const char *zName;  /* Constant name */` |
|        - |  7053 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7054 | `	int nLen = 0;       /* Name length */` |
|        - |  7055 | `	sxi32 rc;` |
|       12 |  7056 | `	if( nArg < 2 ){` |
|        - |  7057 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7058 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7059 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7060 | `		return SXRET_OK;` |
|        - |  7061 | `	}` |
|       12 |  7062 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7063 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7064 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7065 | `		return SXRET_OK;` |
|        - |  7066 | `	}` |
|        - |  7067 | `	/* Extract constant name */` |
|       12 |  7068 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7069 | `	if( nLen < 1 ){` |
|      ! 0 |  7070 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7071 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7072 | `		return SXRET_OK;` |
|        - |  7073 | `	}` |
|        - |  7074 | `	/* Duplicate constant value */` |
|       12 |  7075 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7076 | `	if( pValue == 0 ){` |
|      ! 0 |  7077 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7078 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7079 | `		return SXRET_OK;` |
|        - |  7080 | `	}` |
|        - |  7081 | `	/* Initialize the memory object */` |
|       12 |  7082 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7083 | `	/* Register the constant */` |
|       12 |  7084 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7085 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7086 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7087 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7088 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7089 | `		return SXRET_OK;` |
|        - |  7090 | `	}` |
|        - |  7091 | `	/* Duplicate constant value */` |
|       12 |  7092 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7093 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7094 | `		/* Lower case the constant name */` |
|      ! 0 |  7095 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7096 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7097 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7098 | `				/* UTF-8 stream */` |
|      ! 0 |  7099 | `				zCur++;` |
|      ! 0 |  7100 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  7101 | `					zCur++;` |
|      ! 0 |  7102 | `				}` |
|      ! 0 |  7103 | `				continue;` |
|        - |  7104 | `			}` |
|      ! 0 |  7105 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  7106 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  7107 | `				zCur[0] = (char)c;` |
|      ! 0 |  7108 | `			}` |
|      ! 0 |  7109 | `			zCur++;` |
|      ! 0 |  7110 | `		}` |
|        - |  7111 | `		/* Finally,register the constant */` |
|      ! 0 |  7112 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  7113 | `	}` |
|        - |  7114 | `	/* All done,return TRUE */` |
|       12 |  7115 | `	ph7_result_bool(pCtx,1);` |
|       12 |  7116 | `	return SXRET_OK;` |
|        7 |  7117 |  |
|        - |  7118 | `/*` |
|        - |  7119 | ` * value constant(string $name)` |
|        - |  7120 | ` *  Returns the value of a constant` |
|        - |  7121 | ` * Parameter` |
|        - |  7122 | ` *  $name` |
|        - |  7123 | ` *    Name of the constant.` |
|        - |  7124 | ` * Return` |
|        - |  7125 | ` *  Constant value or NULL if not defined.` |
|        - |  7126 | ` */` |
|        8 |  7127 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7128 |  |
|        - |  7129 | `	SyHashEntry *pEntry;` |
|        - |  7130 | `	ph7_constant *pCons;` |
|        - |  7131 | `	const char *zName; /* Constant name */` |
|        - |  7132 | `	ph7_value sVal;    /* Constant value */` |
|        - |  7133 | `	int nLen;` |
|       10 |  7134 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  7135 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  7136 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  7137 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7138 | `		return SXRET_OK;` |
|        - |  7139 | `	}` |
|        - |  7140 | `	/* Extract the constant name */` |
|       10 |  7141 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7142 | `	/* Perform the query */` |
|       10 |  7143 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  7144 | `	if( pEntry == 0 ){` |
|        3 |  7145 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  7146 | `		ph7_result_null(pCtx);` |
|        3 |  7147 | `		return SXRET_OK;` |
|        - |  7148 | `	}` |
|        8 |  7149 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  7150 | `	/* Point to the structure that describe the constant */` |
|        8 |  7151 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  7152 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  7153 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  7154 | `	/* Return that value */` |
|        8 |  7155 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  7156 | `	/* Cleanup */` |
|        8 |  7157 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  7158 | `	return SXRET_OK;` |
|        6 |  7159 |  |
|        - |  7160 | `/*` |
|        - |  7161 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  7162 | ` * defined below.` |
|        - |  7163 | ` */` |
|      416 |  7164 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7165 |  |
|      417 |  7166 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  7167 | `	ph7_value sName;` |
|        - |  7168 | `	sxi32 rc;` |
|        - |  7169 | `	/* Prepare the constant name for insertion */` |
|      417 |  7170 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      417 |  7171 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  7172 | `	/* Perform the insertion */` |
|      417 |  7173 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      417 |  7174 | `	PH7_MemObjRelease(&sName);` |
|      417 |  7175 | `	return rc;` |
|        1 |  7176 |  |
|        - |  7177 | `/*` |
|        - |  7178 | ` * array get_defined_constants(void)` |
|        - |  7179 | ` *  Returns an associative array with the names of all defined` |
|        - |  7180 | ` *  constants.` |
|        - |  7181 | ` * Parameters` |
|        - |  7182 | ` *  NONE.` |
|        - |  7183 | ` * Returns` |
|        - |  7184 | ` *  Returns the names of all the constants currently defined.` |
|        - |  7185 | ` */` |
|        2 |  7186 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7187 |  |
|        - |  7188 | `	ph7_value *pArray;` |
|        - |  7189 | `	/* Create the array first*/` |
|        3 |  7190 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7191 | `	if( pArray == 0 ){` |
|      ! 0 |  7192 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7193 | `		SXUNUSED(apArg);` |
|        - |  7194 | `		/* Return NULL */` |
|      ! 0 |  7195 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7196 | `		return SXRET_OK;` |
|        - |  7197 | `	}` |
|        - |  7198 | `	/* Fill the array with the defined constants */` |
|        3 |  7199 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  7200 | `	/* Return the created array */` |
|        3 |  7201 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7202 | `	return SXRET_OK;` |
|        2 |  7203 |  |
|        - |  7204 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  7205 | `/*` |
|        - |  7206 | ` * Section:` |
|        - |  7207 | ` *  Random numbers/string generators.` |
|        - |  7208 | ` * Status:` |
|        - |  7209 | ` *    Stable.` |
|        - |  7210 | ` */` |
|        - |  7211 | `/*` |
|        - |  7212 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  7213 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  7214 | ` * used by te SQLite3 library.` |
|        - |  7215 | ` */` |
|     2107 |  7216 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  7217 |  |
|        - |  7218 | `	sxu32 iNum;` |
|     2109 |  7219 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2109 |  7220 | `	return iNum;` |
|        2 |  7221 |  |
|        - |  7222 | `/*` |
|        - |  7223 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  7224 | ` * Note that the generated string is NOT null terminated.` |
|        - |  7225 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  7226 | ` * by te SQLite3 library.` |
|        - |  7227 | ` */` |
|    66242 |  7228 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  7229 |  |
|        - |  7230 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  7231 | `	int i;` |
|        - |  7232 | `	/* Generate a binary string first */` |
|    66244 |  7233 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  7234 | `	/* Turn the binary string into english based alphabet */` |
|   728832 |  7235 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   662590 |  7236 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   331296 |  7237 | `	 }` |
|    66244 |  7238 |  |
|        - |  7239 | `/*` |
|        - |  7240 | ` * int rand()` |
|        - |  7241 | ` * int mt_rand()` |
|        - |  7242 | ` * int rand(int $min,int $max)` |
|        - |  7243 | ` * int mt_rand(int $min,int $max)` |
|        - |  7244 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  7245 | ` * Parameter` |
|        - |  7246 | ` *  $min` |
|        - |  7247 | ` *    The lowest value to return (default: 0)` |
|        - |  7248 | ` *  $max` |
|        - |  7249 | ` *   The highest value to return (default: getrandmax())` |
|        - |  7250 | ` * Return` |
|        - |  7251 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  7252 | ` * Note:` |
|        - |  7253 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7254 | ` *  by te SQLite3 library.` |
|        - |  7255 | ` */` |
|       20 |  7256 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7257 |  |
|        - |  7258 | `	sxu32 iNum;` |
|        - |  7259 | `	/* Generate the random number */` |
|       21 |  7260 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  7261 | `	if( nArg > 1 ){` |
|        - |  7262 | `		sxu32 iMin,iMax;` |
|        3 |  7263 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  7264 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  7265 | `		if( iMin < iMax ){` |
|        3 |  7266 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  7267 | `			if( iDiv > 0 ){` |
|        3 |  7268 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  7269 | `			}` |
|        1 |  7270 | `		}else if(iMax > 0 ){` |
|      ! 0 |  7271 | `			iNum %= iMax;` |
|      ! 0 |  7272 | `		}` |
|        1 |  7273 | `	}` |
|        - |  7274 | `	/* Return the number */` |
|       21 |  7275 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  7276 | `	return SXRET_OK;` |
|        1 |  7277 |  |
|        - |  7278 | `/*` |
|        - |  7279 | ` * int getrandmax(void)` |
|        - |  7280 | ` * int mt_getrandmax(void)` |
|        - |  7281 | ` * int rc4_getrandmax(void)` |
|        - |  7282 | ` *   Show largest possible random value` |
|        - |  7283 | ` * Return` |
|        - |  7284 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  7285 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  7286 | ` * Note:` |
|        - |  7287 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7288 | ` *  by te SQLite3 library.` |
|        - |  7289 | ` */` |
|        4 |  7290 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7291 |  |
|        2 |  7292 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  7293 | `	SXUNUSED(apArg);` |
|        5 |  7294 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  7295 | `	return SXRET_OK;` |
|        1 |  7296 |  |
|        - |  7297 | `/*` |
|        - |  7298 | ` * string rand_str()` |
|        - |  7299 | ` * string rand_str(int $len)` |
|        - |  7300 | ` *  Generate a random string (English alphabet).` |
|        - |  7301 | ` * Parameter` |
|        - |  7302 | ` *  $len` |
|        - |  7303 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  7304 | ` * Return` |
|        - |  7305 | ` *   A pseudo random string.` |
|        - |  7306 | ` * Note:` |
|        - |  7307 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7308 | ` *  by te SQLite3 library.` |
|        - |  7309 | ` *  This function is a symisc extension.` |
|        - |  7310 | ` */` |
|      120 |  7311 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7312 |  |
|        - |  7313 | `	char zString[1024];` |
|      122 |  7314 | `	int iLen = 0x10;` |
|      122 |  7315 | `	if( nArg > 0 ){` |
|        - |  7316 | `		/* Get the desired length */` |
|      122 |  7317 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  7318 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  7319 | `			/* Default length */` |
|        3 |  7320 | `			iLen = 0x10;` |
|        1 |  7321 | `		}` |
|       60 |  7322 | `	}` |
|        - |  7323 | `	/* Generate the random string */` |
|      122 |  7324 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  7325 | `	/* Return the generated string */` |
|      122 |  7326 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  7327 | `	return SXRET_OK;` |
|        2 |  7328 |  |
|        - |  7329 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  7330 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  7331 | `/* Unique ID private data */` |
|        - |  7332 | `struct unique_id_data` |
|        - |  7333 |  |
|        - |  7334 | `	ph7_context *pCtx; /* Call context */` |
|        - |  7335 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  7336 | `};` |
|        - |  7337 | `/*` |
|        - |  7338 | ` * Binary to hex consumer callback.` |
|        - |  7339 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  7340 | ` * defined below.` |
|        - |  7341 | ` */` |
|      192 |  7342 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  7343 |  |
|      193 |  7344 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  7345 | `	sxu32 nBuflen;` |
|        - |  7346 | `	/* Extract result buffer length */` |
|      193 |  7347 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  7348 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  7349 | `			/*` |
|        - |  7350 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  7351 | `			 * string will be 13 characters long` |
|        - |  7352 | `			 */` |
|       25 |  7353 | `		return SXERR_ABORT;` |
|        - |  7354 | `	}` |
|      169 |  7355 | `	if( nBuflen > 22 ){` |
|      ! 0 |  7356 | `		return SXERR_ABORT;` |
|        - |  7357 | `	}` |
|        - |  7358 | `	/* Safely Consume the hex stream */` |
|      169 |  7359 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  7360 | `	return SXRET_OK;` |
|       97 |  7361 |  |
|        - |  7362 | `/*` |
|        - |  7363 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  7364 | ` *  Generate a unique ID` |
|        - |  7365 | ` * Parameter` |
|        - |  7366 | ` * $prefix` |
|        - |  7367 | ` *  Append this prefix to the generated unique ID.` |
|        - |  7368 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  7369 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  7370 | ` * $more_entropy` |
|        - |  7371 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  7372 | ` *  that the result will be unique.` |
|        - |  7373 | ` * Return` |
|        - |  7374 | ` *  Returns the unique identifier, as a string.` |
|        - |  7375 | ` */` |
|       24 |  7376 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7377 |  |
|        - |  7378 | `	struct unique_id_data sUniq;` |
|        - |  7379 | `	unsigned char zDigest[20];` |
|       25 |  7380 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7381 | `	const char *zPrefix;` |
|        - |  7382 | `	SHA1Context sCtx;` |
|        - |  7383 | `	char zRandom[7];` |
|        - |  7384 | `	int nPrefix;` |
|        - |  7385 | `	int entropy;` |
|        - |  7386 | `	/* Generate a random string first */` |
|       25 |  7387 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  7388 | `	/* Initialize fields */` |
|       25 |  7389 | `	zPrefix = 0;` |
|       25 |  7390 | `	nPrefix = 0;` |
|       25 |  7391 | `	entropy = 0;` |
|       25 |  7392 | `	if( nArg > 0 ){` |
|        - |  7393 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  7394 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  7395 | `		if( nArg > 1 ){` |
|      ! 0 |  7396 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  7397 | `		}` |
|      ! 0 |  7398 | `	}` |
|       25 |  7399 | `	SHA1Init(&sCtx);` |
|        - |  7400 | `	/* Generate the random ID */` |
|       25 |  7401 | `	if( nPrefix > 0 ){` |
|      ! 0 |  7402 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  7403 | `	}` |
|        - |  7404 | `	/* Append the random ID */` |
|       25 |  7405 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  7406 | `	/* Append the random string */` |
|       25 |  7407 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  7408 | `	/* Increment the number */` |
|       25 |  7409 | `	pVm->unique_id++;` |
|       25 |  7410 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  7411 | `	/* Hexify the digest */` |
|       25 |  7412 | `	sUniq.pCtx = pCtx;` |
|       25 |  7413 | `	sUniq.entropy = entropy;` |
|       25 |  7414 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  7415 | `	/* All done */` |
|       25 |  7416 | `	return PH7_OK;` |
|        1 |  7417 |  |
|        - |  7418 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  7419 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  7420 | `/*` |
|        - |  7421 | ` * Section:` |
|        - |  7422 | ` *  Language construct implementation as foreign functions.` |
|        - |  7423 | ` * Status:` |
|        - |  7424 | ` *    Stable.` |
|        - |  7425 | ` */` |
|        - |  7426 | `/*` |
|        - |  7427 | ` * void echo($string...)` |
|        - |  7428 | ` *  Output one or more messages.` |
|        - |  7429 | ` * Parameters` |
|        - |  7430 | ` *  $string` |
|        - |  7431 | ` *   Message to output.` |
|        - |  7432 | ` * Return` |
|        - |  7433 | ` *  NULL.` |
|        - |  7434 | ` */` |
|      ! 0 |  7435 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7436 |  |
|        - |  7437 | `	const char *zData;` |
|      ! 0 |  7438 | `	int nDataLen = 0;` |
|        - |  7439 | `	ph7_vm *pVm;` |
|        - |  7440 | `	int i,rc;` |
|        - |  7441 | `	/* Point to the target VM */` |
|      ! 0 |  7442 | `	pVm = pCtx->pVm;` |
|        - |  7443 | `	/* Output */` |
|      ! 0 |  7444 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  7445 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  7446 | `		if( nDataLen > 0 ){` |
|      ! 0 |  7447 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  7448 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7449 | `				/* Increment output length */` |
|      ! 0 |  7450 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  7451 | `			}` |
|      ! 0 |  7452 | `			if( rc == SXERR_ABORT ){` |
|        - |  7453 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7454 | `				return PH7_ABORT;` |
|        - |  7455 | `			}` |
|      ! 0 |  7456 | `		}` |
|      ! 0 |  7457 | `	}` |
|      ! 0 |  7458 | `	return SXRET_OK;` |
|      ! 0 |  7459 |  |
|        - |  7460 | `/*` |
|        - |  7461 | ` * int print($string...)` |
|        - |  7462 | ` *  Output one or more messages.` |
|        - |  7463 | ` * Parameters` |
|        - |  7464 | ` *  $string` |
|        - |  7465 | ` *   Message to output.` |
|        - |  7466 | ` * Return` |
|        - |  7467 | ` *  1 always.` |
|        - |  7468 | ` */` |
|        2 |  7469 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7470 |  |
|        - |  7471 | `	const char *zData;` |
|        3 |  7472 | `	int nDataLen = 0;` |
|        - |  7473 | `	ph7_vm *pVm;` |
|        - |  7474 | `	int i,rc;` |
|        - |  7475 | `	/* Point to the target VM */` |
|        3 |  7476 | `	pVm = pCtx->pVm;` |
|        - |  7477 | `	/* Output */` |
|        5 |  7478 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  7479 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  7480 | `		if( nDataLen > 0 ){` |
|        3 |  7481 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  7482 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7483 | `				/* Increment output length */` |
|        3 |  7484 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  7485 | `			}` |
|        3 |  7486 | `			if( rc == SXERR_ABORT ){` |
|        - |  7487 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7488 | `				return PH7_ABORT;` |
|        - |  7489 | `			}` |
|        1 |  7490 | `		}` |
|        2 |  7491 | `	}` |
|        - |  7492 | `	/* Return 1 */` |
|        3 |  7493 | `	ph7_result_int(pCtx,1);` |
|        3 |  7494 | `	return SXRET_OK;` |
|        2 |  7495 |  |
|        - |  7496 | `/*` |
|        - |  7497 | ` * void exit(string $msg)` |
|        - |  7498 | ` * void exit(int $status)` |
|        - |  7499 | ` * void die(string $ms)` |
|        - |  7500 | ` * void die(int $status)` |
|        - |  7501 | ` *   Output a message and terminate program execution.` |
|        - |  7502 | ` * Parameter` |
|        - |  7503 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  7504 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  7505 | ` *  and not printed` |
|        - |  7506 | ` * Return` |
|        - |  7507 | ` *  NULL` |
|        - |  7508 | ` */` |
|      ! 0 |  7509 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7510 |  |
|      ! 0 |  7511 | `	if( nArg > 0 ){` |
|      ! 0 |  7512 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  7513 | `			const char *zData;` |
|      ! 0 |  7514 | `			int iLen = 0;` |
|        - |  7515 | `			/* Print exit message */` |
|      ! 0 |  7516 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  7517 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  7518 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  7519 | `			sxi32 iExitStatus;` |
|        - |  7520 | `			/* Record exit status code */` |
|      ! 0 |  7521 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  7522 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  7523 | `		}` |
|      ! 0 |  7524 | `	}` |
|        - |  7525 | `	/* Check if we are in an included file */` |
|      ! 0 |  7526 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  7527 | `		/* Exit the entire process */` |
|      ! 0 |  7528 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  7529 | `	}` |
|        - |  7530 | `	/* Abort processing immediately */` |
|      ! 0 |  7531 | `	return PH7_ABORT;` |
|      ! 0 |  7532 |  |
|        - |  7533 | `/*` |
|        - |  7534 | ` * bool isset($var,...)` |
|        - |  7535 | ` *  Finds out whether a variable is set.` |
|        - |  7536 | ` * Parameters` |
|        - |  7537 | ` *  One or more variable to check.` |
|        - |  7538 | ` * Return` |
|        - |  7539 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  7540 | ` */` |
|    68986 |  7541 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7542 |  |
|        - |  7543 | `	ph7_value *pObj;` |
|    68988 |  7544 | `	int res = 0;` |
|        - |  7545 | `	int i;` |
|    68988 |  7546 | `	if( nArg < 1 ){` |
|        - |  7547 | `		/* Missing arguments,return false */` |
|      ! 0 |  7548 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  7549 | `		return SXRET_OK;` |
|        - |  7550 | `	}` |
|        - |  7551 | `	/* Iterate over available arguments */` |
|    91272 |  7552 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    68988 |  7553 | `		pObj = apArg[i];` |
|    68988 |  7554 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    46208 |  7555 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7556 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  7557 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  7558 | `			}` |
|    23103 |  7559 | `		}` |
|    68988 |  7560 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    68988 |  7561 | `		if( !res ){` |
|        - |  7562 | `			/* Variable not set,return FALSE */` |
|    46704 |  7563 | `			ph7_result_bool(pCtx,0);` |
|    46704 |  7564 | `			return SXRET_OK;` |
|        - |  7565 | `		}` |
|    11144 |  7566 | `	}` |
|        - |  7567 | `	/* All given variable are set,return TRUE */` |
|    22286 |  7568 | `	ph7_result_bool(pCtx,1);` |
|    22286 |  7569 | `	return SXRET_OK;` |
|    34495 |  7570 |  |
|        - |  7571 | `/*` |
|        - |  7572 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  7573 | ` * frame,the reference table and discard it's contents.` |
|        - |  7574 | ` * This function never fail and always return SXRET_OK.` |
|        - |  7575 | ` */` |
|  2952870 |  7576 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  7577 |  |
|        - |  7578 | `	ph7_value *pObj;` |
|        - |  7579 | `	VmRefObj *pRef;` |
|  2952872 |  7580 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2952872 |  7581 | `	if( pObj ){` |
|        - |  7582 | `		/* Release the object */` |
|  2952872 |  7583 | `		PH7_MemObjRelease(pObj);` |
|  1476435 |  7584 | `	}` |
|        - |  7585 | `	/* Remove old reference links */` |
|  2952872 |  7586 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2952872 |  7587 | `	if( pRef ){` |
|  2952852 |  7588 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  7589 | `		/* Unlink from the reference table */` |
|  2952852 |  7590 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2952852 |  7591 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  7592 | `			VmSlot sFree;` |
|        - |  7593 | `			/* Restore to the free list */` |
|  2952846 |  7594 | `			sFree.nIdx = nObjIdx;` |
|  2952846 |  7595 | `			sFree.pUserData = 0;` |
|  2952846 |  7596 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1476422 |  7597 | `		}` |
|  1476425 |  7598 | `	}` |
|  2952872 |  7599 | `	return SXRET_OK;` |
|        2 |  7600 |  |
|        - |  7601 | `/*` |
|        - |  7602 | ` * void unset($var,...)` |
|        - |  7603 | ` *   Unset one or more given variable.` |
|        - |  7604 | ` * Parameters` |
|        - |  7605 | ` *  One or more variable to unset.` |
|        - |  7606 | ` * Return` |
|        - |  7607 | ` *  Nothing.` |
|        - |  7608 | ` */` |
|     3260 |  7609 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7610 |  |
|        - |  7611 | `	ph7_value *pObj;` |
|        - |  7612 | `	ph7_vm *pVm;` |
|        - |  7613 | `	int i;` |
|        - |  7614 | `	/* Point to the target VM */` |
|     3262 |  7615 | `	pVm = pCtx->pVm;` |
|        - |  7616 | `	/* Iterate and unset */` |
|     9666 |  7617 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6406 |  7618 | `		pObj = apArg[i];` |
|     6406 |  7619 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      870 |  7620 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7621 | `				/* Throw an error */` |
|      ! 0 |  7622 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  7623 | `			}` |
|      436 |  7624 | `		}else{` |
|     5537 |  7625 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  7626 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     5537 |  7627 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     5531 |  7628 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2765 |  7629 | `			}` |
|        - |  7630 | `		}` |
|     3204 |  7631 | `	}` |
|     3262 |  7632 | `	return SXRET_OK;` |
|        2 |  7633 |  |
|        - |  7634 | `/*` |
|        - |  7635 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  7636 | ` */` |
|      110 |  7637 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7638 |  |
|      111 |  7639 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  7640 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  7641 | `	ph7_value *pObj;` |
|        - |  7642 | `	sxu32 nIdx;` |
|        - |  7643 | `	/* Extract the memory object */` |
|      111 |  7644 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  7645 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  7646 | `	if( pObj ){` |
|      111 |  7647 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  7648 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  7649 | `				SyString sName;` |
|        - |  7650 | `				ph7_value sKey;` |
|        - |  7651 | `				/* Perform the insertion */` |
|      109 |  7652 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  7653 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  7654 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  7655 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  7656 | `			}` |
|       54 |  7657 | `		}` |
|       55 |  7658 | `	}` |
|      111 |  7659 | `	return SXRET_OK;` |
|        1 |  7660 |  |
|        - |  7661 | `/*` |
|        - |  7662 | ` * array get_defined_vars(void)` |
|        - |  7663 | ` *  Returns an array of all defined variables.` |
|        - |  7664 | ` * Parameter` |
|        - |  7665 | ` *  None` |
|        - |  7666 | ` * Return` |
|        - |  7667 | ` *  An array with all the variables defined in the current scope.` |
|        - |  7668 | ` */` |
|        2 |  7669 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7670 |  |
|        3 |  7671 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7672 | `	ph7_value *pArray;` |
|        - |  7673 | `	/* Create a new array */` |
|        3 |  7674 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7675 | ` 	if( pArray == 0 ){` |
|      ! 0 |  7676 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7677 | `		SXUNUSED(apArg);` |
|        - |  7678 | `		/* Return NULL */` |
|      ! 0 |  7679 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7680 | `		return SXRET_OK;` |
|        - |  7681 | `	}` |
|        - |  7682 | `	/* Superglobals first */` |
|        3 |  7683 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  7684 | `	/* Then variable defined in the current frame */` |
|        3 |  7685 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  7686 | `	/* Finally,return the created array */` |
|        3 |  7687 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7688 | `	return SXRET_OK;` |
|        2 |  7689 |  |
|        - |  7690 | `/*` |
|        - |  7691 | ` * bool gettype($var)` |
|        - |  7692 | ` *  Get the type of a variable` |
|        - |  7693 | ` * Parameters` |
|        - |  7694 | ` *   $var` |
|        - |  7695 | ` *    The variable being type checked.` |
|        - |  7696 | ` * Return` |
|        - |  7697 | ` *   String representation of the given variable type.` |
|        - |  7698 | ` */` |
|       32 |  7699 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7700 |  |
|       34 |  7701 | `	const char *zType = "Empty";` |
|       34 |  7702 | `	if( nArg > 0 ){` |
|       34 |  7703 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  7704 | `	}` |
|        - |  7705 | `	/* Return the variable type */` |
|       34 |  7706 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  7707 | `	return SXRET_OK;` |
|        2 |  7708 |  |
|        - |  7709 | `/*` |
|        - |  7710 | ` * string get_resource_type(resource $handle)` |
|        - |  7711 | ` *  This function gets the type of the given resource.` |
|        - |  7712 | ` * Parameters` |
|        - |  7713 | ` *  $handle` |
|        - |  7714 | ` *  The evaluated resource handle.` |
|        - |  7715 | ` * Return` |
|        - |  7716 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  7717 | ` *  representing its type. If the type is not identified by this function` |
|        - |  7718 | ` *  the return value will be the string Unknown.` |
|        - |  7719 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  7720 | ` *  is not a resource.` |
|        - |  7721 | ` */` |
|        2 |  7722 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7723 |  |
|        3 |  7724 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  7725 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  7726 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7727 | `		return PH7_OK;` |
|        - |  7728 | `	}` |
|        3 |  7729 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  7730 | `	return SXRET_OK;` |
|        2 |  7731 |  |
|        - |  7732 | `/*` |
|        - |  7733 | ` * void var_dump(expression,....)` |
|        - |  7734 | ` *   var_dump � Dumps information about a variable` |
|        - |  7735 | ` * Parameters` |
|        - |  7736 | ` *   One or more expression to dump.` |
|        - |  7737 | ` * Returns` |
|        - |  7738 | ` *  Nothing.` |
|        - |  7739 | ` */` |
|      218 |  7740 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7741 |  |
|        - |  7742 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  7743 | `	int i;` |
|      220 |  7744 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  7745 | `	/* Dump one or more expressions */` |
|      444 |  7746 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  7747 | `		ph7_value *pObj = apArg[i];` |
|        - |  7748 | `		/* Reset the working buffer */` |
|      226 |  7749 | `		SyBlobReset(&sDump);` |
|        - |  7750 | `		/* Dump the given expression */` |
|      226 |  7751 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  7752 | `		/* Output */` |
|      226 |  7753 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  7754 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  7755 | `		}` |
|      114 |  7756 | `	}` |
|        - |  7757 | `	/* Release the working buffer */` |
|      220 |  7758 | `	SyBlobRelease(&sDump);` |
|      220 |  7759 | `	return SXRET_OK;` |
|        2 |  7760 |  |
|        - |  7761 | `/*` |
|        - |  7762 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  7763 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  7764 | ` * Parameters` |
|        - |  7765 | ` *   expression: Expression to dump` |
|        - |  7766 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  7767 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  7768 | ` *            print_r() will return the information rather than print it.` |
|        - |  7769 | ` * Return` |
|        - |  7770 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  7771 | ` *  Otherwise, the return value is TRUE.` |
|        - |  7772 | ` */` |
|       16 |  7773 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7774 |  |
|       17 |  7775 | `	int ret_string = 0;` |
|        - |  7776 | `	SyBlob sDump;` |
|       17 |  7777 | `	if( nArg < 1 ){` |
|        - |  7778 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  7779 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7780 | `		return SXRET_OK;` |
|        - |  7781 | `	}` |
|       17 |  7782 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  7783 | `	if ( nArg > 1 ){` |
|        - |  7784 | `		/* Where to redirect output */` |
|       11 |  7785 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  7786 | `	}` |
|        - |  7787 | `	/* Generate dump */` |
|       17 |  7788 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  7789 | `	if( !ret_string ){` |
|        - |  7790 | `		/* Output dump */` |
|        7 |  7791 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7792 | `		/* Return true */` |
|        7 |  7793 | `		ph7_result_bool(pCtx,1);` |
|        4 |  7794 | `	}else{` |
|        - |  7795 | `		/* Generated dump as return value */` |
|       11 |  7796 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7797 | `	}` |
|        - |  7798 | `	/* Release the working buffer */` |
|       17 |  7799 | `	SyBlobRelease(&sDump);` |
|       17 |  7800 | `	return SXRET_OK;` |
|        9 |  7801 |  |
|        - |  7802 | `/*` |
|        - |  7803 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  7804 | ` * Same job as print_r. (see coment above)` |
|        - |  7805 | ` */` |
|        2 |  7806 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7807 |  |
|        3 |  7808 | `	int ret_string = 0;` |
|        - |  7809 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  7810 | `	if( nArg < 1 ){` |
|        - |  7811 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  7812 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7813 | `		return SXRET_OK;` |
|        - |  7814 | `	}` |
|        3 |  7815 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  7816 | `	if ( nArg > 1 ){` |
|        - |  7817 | `		/* Where to redirect output */` |
|        3 |  7818 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  7819 | `	}` |
|        - |  7820 | `	/* Generate dump */` |
|        3 |  7821 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  7822 | `	if( !ret_string ){` |
|        - |  7823 | `		/* Output dump */` |
|      ! 0 |  7824 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7825 | `		/* Return NULL */` |
|      ! 0 |  7826 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7827 | `	}else{` |
|        - |  7828 | `		/* Generated dump as return value */` |
|        3 |  7829 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7830 | `	}` |
|        - |  7831 | `	/* Release the working buffer */` |
|        3 |  7832 | `	SyBlobRelease(&sDump);` |
|        3 |  7833 | `	return SXRET_OK;` |
|        2 |  7834 |  |
|        - |  7835 | `/*` |
|        - |  7836 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  7837 | ` *  Set/get the various assert flags.` |
|        - |  7838 | ` * Parameter` |
|        - |  7839 | ` * $what` |
|        - |  7840 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  7841 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  7842 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  7843 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  7844 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  7845 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  7846 | ` * $value` |
|        - |  7847 | ` *   An optional new value for the option.` |
|        - |  7848 | ` * Return` |
|        - |  7849 | ` *  Old setting on success or FALSE on failure.` |
|        - |  7850 | ` */` |
|       30 |  7851 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7852 |  |
|       32 |  7853 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7854 | `	int iOption;` |
|        - |  7855 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       32 |  7856 | `	if( nArg < 1 ){` |
|        3 |  7857 | `		return PH7_VmThrowException(pCtx,` |
|        - |  7858 | `			"ArgumentCountError",` |
|        - |  7859 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  7860 | `			);` |
|        - |  7861 | `	}` |
|        - |  7862 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       28 |  7863 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       30 |  7864 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  7865 | `		return PH7_VmThrowException(pCtx,` |
|        - |  7866 | `			"TypeError",` |
|        - |  7867 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  7868 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  7869 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  7870 | `			);` |
|        - |  7871 | `	}` |
|       30 |  7872 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  7873 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  7874 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  7875 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       30 |  7876 | `	switch( iOption ){` |
|        6 |  7877 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  7878 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       14 |  7879 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       14 |  7880 | `		if( nArg > 1 ){` |
|        5 |  7881 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  7882 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  7883 | `			}else{` |
|        3 |  7884 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  7885 | `			}` |
|        2 |  7886 | `		}` |
|       14 |  7887 | `		break;` |
|        1 |  7888 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  7889 | `		/* Return old callback or null */` |
|        3 |  7890 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  7891 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  7892 | `		}else{` |
|        3 |  7893 | `			ph7_result_null(pCtx);` |
|        - |  7894 | `		}` |
|        3 |  7895 | `		if( nArg > 1 ){` |
|      ! 0 |  7896 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  7897 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  7898 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  7899 | `			}else{` |
|      ! 0 |  7900 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  7901 | `			}` |
|      ! 0 |  7902 | `		}` |
|        3 |  7903 | `		break;` |
|        5 |  7904 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  7905 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  7906 | `		if( nArg > 1 ){` |
|        5 |  7907 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  7908 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  7909 | `			}else{` |
|        3 |  7910 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  7911 | `			}` |
|        2 |  7912 | `		}` |
|       11 |  7913 | `		break;` |
|      ! 0 |  7914 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  7915 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  7916 | `		break;` |
|        1 |  7917 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  7918 | `		ph7_result_int(pCtx, 1);` |
|        3 |  7919 | `		break;` |
|      ! 0 |  7920 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  7921 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  7922 | `		break;` |
|        1 |  7923 | `	default:` |
|        - |  7924 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  7925 | `		return PH7_VmThrowException(pCtx,` |
|        - |  7926 | `			"ValueError",` |
|        - |  7927 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  7928 | `			);` |
|        - |  7929 | `	}` |
|       28 |  7930 | `	return PH7_OK;` |
|       17 |  7931 |  |
|        - |  7932 | `/*` |
|        - |  7933 | ` * bool assert(mixed $assertion)` |
|        - |  7934 | ` *  Checks if assertion is FALSE.` |
|        - |  7935 | ` * Parameter` |
|        - |  7936 | ` *  $assertion` |
|        - |  7937 | ` *    The assertion to test.` |
|        - |  7938 | ` * Return` |
|        - |  7939 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  7940 | ` */` |
|       26 |  7941 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7942 |  |
|       28 |  7943 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7944 | `	int iFlags,iResult;` |
|        - |  7945 | `	const char *zDesc;` |
|        - |  7946 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       28 |  7947 | `	if( nArg < 1 ){` |
|        3 |  7948 | `		return PH7_VmThrowException(pCtx,` |
|        - |  7949 | `			"ArgumentCountError",` |
|        - |  7950 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  7951 | `			);` |
|        - |  7952 | `	}` |
|       26 |  7953 | `	iFlags = pVm->iAssertFlags;` |
|       26 |  7954 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  7955 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  7956 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  7957 | `		return PH7_OK;` |
|        - |  7958 | `	}` |
|        - |  7959 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       26 |  7960 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       26 |  7961 | `	if( !iResult ){` |
|        - |  7962 | `		/* Assertion failed */` |
|        - |  7963 | `		/* Extract optional description */` |
|       13 |  7964 | `		zDesc = 0;` |
|       13 |  7965 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  7966 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  7967 | `		}` |
|       13 |  7968 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  7969 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  7970 | `			ph7_value sFile,sLine;` |
|        - |  7971 | `			ph7_value *apCbArg[3];` |
|        - |  7972 | `			SyString *pFile;` |
|        - |  7973 | `			/* Extract the processed script */` |
|      ! 0 |  7974 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  7975 | `			if( pFile == 0 ){` |
|      ! 0 |  7976 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  7977 | `			}` |
|        - |  7978 | `			/* Invoke the callback */` |
|      ! 0 |  7979 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  7980 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  7981 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  7982 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  7983 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  7984 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  7985 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  7986 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  7987 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  7988 | `		}` |
|       13 |  7989 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  7990 | `			/* Abort VM execution immediately */` |
|      ! 0 |  7991 | `			return PH7_ABORT;` |
|        - |  7992 | `		}` |
|        - |  7993 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  7994 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  7995 | `			return PH7_VmThrowException(pCtx,` |
|        - |  7996 | `				"AssertionError",` |
|        - |  7997 | `				"%s",` |
|        1 |  7998 | `				zDesc` |
|        - |  7999 | `				);` |
|      ! 0 |  8000 | `		}else{` |
|       11 |  8001 | `			return PH7_VmThrowException(pCtx,` |
|        - |  8002 | `				"AssertionError",` |
|        - |  8003 | `				"assert(false)"` |
|        - |  8004 | `				);` |
|        - |  8005 | `		}` |
|        - |  8006 | `	}` |
|        - |  8007 | `	/* Assertion passed */` |
|       14 |  8008 | `	ph7_result_bool(pCtx,1);` |
|       14 |  8009 | `	return PH7_OK;` |
|       15 |  8010 |  |
|        - |  8011 | `/*` |
|        - |  8012 | ` * Section:` |
|        - |  8013 | ` *  Error reporting functions.` |
|        - |  8014 | ` * Status:` |
|        - |  8015 | ` *    Stable.` |
|        - |  8016 | ` */` |
|        - |  8017 | `/*` |
|        - |  8018 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  8019 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  8020 | ` * Parameters` |
|        - |  8021 | ` *  $error_msg` |
|        - |  8022 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  8023 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  8024 | ` * $error_type` |
|        - |  8025 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  8026 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  8027 | ` * Return` |
|        - |  8028 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  8029 | ` */` |
|       12 |  8030 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8031 |  |
|       14 |  8032 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  8033 | `	int rc = PH7_OK;` |
|       14 |  8034 | `	if( nArg > 0 ){` |
|        - |  8035 | `		const char *zErr;` |
|        - |  8036 | `		int nLen;` |
|        - |  8037 | `		/* Extract the error message */` |
|       12 |  8038 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8039 | `		if( nArg > 1 ){` |
|        - |  8040 | `			/* Extract the error type */` |
|       12 |  8041 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  8042 | `			switch( nErr ){` |
|        1 |  8043 | `			case 1:   /* E_ERROR */` |
|        - |  8044 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  8045 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  8046 | `			case 256: /* E_USER_ERROR */` |
|        3 |  8047 | `				nErr = PH7_CTX_ERR;` |
|        3 |  8048 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  8049 | `				break;` |
|        1 |  8050 | `			case 2:   /* E_WARNING */` |
|        - |  8051 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  8052 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  8053 | `			case 512: /* E_USER_WARNING */` |
|        3 |  8054 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  8055 | `				break;` |
|        3 |  8056 | `			default:` |
|        8 |  8057 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  8058 | `				break;` |
|        - |  8059 | `			}` |
|        5 |  8060 | `		}` |
|        - |  8061 | `		/* Report error */` |
|       12 |  8062 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  8063 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  8064 | `			return rc;` |
|        - |  8065 | `		}` |
|        - |  8066 | `		/* Return true */` |
|       12 |  8067 | `		ph7_result_bool(pCtx,1);` |
|        7 |  8068 | `	}else{` |
|        - |  8069 | `		/* Missing arguments,return FALSE */` |
|        3 |  8070 | `		ph7_result_bool(pCtx,0);` |
|        - |  8071 | `	}` |
|       14 |  8072 | `	return rc;` |
|        8 |  8073 |  |
|        - |  8074 | `/*` |
|        - |  8075 | ` * int error_reporting([int $level])` |
|        - |  8076 | ` *  Sets which PHP errors are reported.` |
|        - |  8077 | ` * Parameters` |
|        - |  8078 | ` *  $level` |
|        - |  8079 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  8080 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  8081 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  8082 | ` *   levels will not always behave as expected.` |
|        - |  8083 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  8084 | ` *   in the predefined constants.` |
|        - |  8085 | ` * Return` |
|        - |  8086 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  8087 | ` *   parameter is given.` |
|        - |  8088 | ` */` |
|       40 |  8089 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8090 |  |
|       42 |  8091 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8092 | `	int nOld;` |
|        - |  8093 | `	/* Extract the old reporting level */` |
|       42 |  8094 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       42 |  8095 | `	if( nArg > 0 ){` |
|        - |  8096 | `		int nNew;` |
|        - |  8097 | `		/* Extract the desired error reporting level */` |
|       34 |  8098 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       34 |  8099 | `		if( !nNew ){` |
|        - |  8100 | `			/* Do not report errors at all */` |
|        5 |  8101 | `			pVm->bErrReport = 0;` |
|        3 |  8102 | `		}else{` |
|        - |  8103 | `			/* Report all errors */` |
|       30 |  8104 | `			pVm->bErrReport = 1;` |
|        - |  8105 | `		}` |
|       16 |  8106 | `	}` |
|        - |  8107 | `	/* Return the old level */` |
|       42 |  8108 | `	ph7_result_int(pCtx,nOld);` |
|       42 |  8109 | `	return PH7_OK;` |
|        2 |  8110 |  |
|        - |  8111 | `/*` |
|        - |  8112 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  8113 | ` *  Send an error message somewhere.` |
|        - |  8114 | ` * Parameter` |
|        - |  8115 | ` *  $message` |
|        - |  8116 | ` *   The error message that should be logged.` |
|        - |  8117 | ` *  $message_type` |
|        - |  8118 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  8119 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  8120 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  8121 | ` *       This is the default option.` |
|        - |  8122 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  8123 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  8124 | ` *    2  No longer an option.` |
|        - |  8125 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  8126 | ` *       to the end of the message string.` |
|        - |  8127 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  8128 | ` *  $destination` |
|        - |  8129 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  8130 | ` *  $extra_headers` |
|        - |  8131 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  8132 | ` * Return` |
|        - |  8133 | ` *  TRUE on success or FALSE on failure.` |
|        - |  8134 | ` * NOTE:` |
|        - |  8135 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  8136 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  8137 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  8138 | ` *  Otherwise this function is no-op.` |
|        - |  8139 | ` */` |
|        4 |  8140 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8141 |  |
|        - |  8142 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  8143 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  8144 | `	int iType = 0;` |
|        5 |  8145 | `	if( nArg < 1 ){` |
|        - |  8146 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  8147 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8148 | `		return PH7_OK;` |
|        - |  8149 | `	}` |
|        5 |  8150 | `	if( pVm->xErrLog  ){` |
|        - |  8151 | `		/* Invoke the user callback */` |
|      ! 0 |  8152 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  8153 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  8154 | `		if( nArg > 1 ){` |
|      ! 0 |  8155 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  8156 | `			if( nArg > 2 ){` |
|      ! 0 |  8157 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  8158 | `				if( nArg > 3 ){` |
|      ! 0 |  8159 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  8160 | `				}` |
|      ! 0 |  8161 | `			}` |
|      ! 0 |  8162 | `		}` |
|      ! 0 |  8163 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  8164 | `	}` |
|        - |  8165 | `	/* Retun TRUE */` |
|        5 |  8166 | `	ph7_result_bool(pCtx,1);` |
|        5 |  8167 | `	return PH7_OK;` |
|        3 |  8168 |  |
|        - |  8169 | `/*` |
|        - |  8170 | ` * bool restore_exception_handler(void)` |
|        - |  8171 | ` *  Restores the previously defined exception handler function.` |
|        - |  8172 | ` * Parameter` |
|        - |  8173 | ` *  None` |
|        - |  8174 | ` * Return` |
|        - |  8175 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  8176 | ` */` |
|        4 |  8177 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8178 |  |
|        5 |  8179 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8180 | `	ph7_value *pOld,*pNew;` |
|        - |  8181 | `	/* Point to the old and the new handler */` |
|        5 |  8182 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  8183 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  8184 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8185 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8186 | `		SXUNUSED(apArg);` |
|        - |  8187 | `		/* No installed handler,return FALSE */` |
|        5 |  8188 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8189 | `		return PH7_OK;` |
|        - |  8190 | `	}` |
|        - |  8191 | `	/* Copy the old handler */` |
|      ! 0 |  8192 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8193 | `	PH7_MemObjRelease(pOld);` |
|        - |  8194 | `	/* Return TRUE */` |
|      ! 0 |  8195 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8196 | `	return PH7_OK;` |
|        3 |  8197 |  |
|        - |  8198 | `/*` |
|        - |  8199 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  8200 | ` *  Sets a user-defined exception handler function.` |
|        - |  8201 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  8202 | ` * NOTE` |
|        - |  8203 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  8204 | ` *  the satndard PHP engine.` |
|        - |  8205 | ` * Parameters` |
|        - |  8206 | ` *  $exception_handler` |
|        - |  8207 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  8208 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  8209 | ` *   that was thrown.` |
|        - |  8210 | ` *  Note:` |
|        - |  8211 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8212 | ` * Return` |
|        - |  8213 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  8214 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8215 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8216 | ` */` |
|        4 |  8217 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8218 |  |
|        6 |  8219 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8220 | `	ph7_value *pOld,*pNew;` |
|        - |  8221 | `	/* Point to the old and the new handler */` |
|        6 |  8222 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  8223 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  8224 | `	/* Return the old handler */` |
|        6 |  8225 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  8226 | `	if( nArg > 0 ){` |
|        6 |  8227 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8228 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  8229 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  8230 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  8231 | `		}else{` |
|        6 |  8232 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8233 | `			/* Install the new handler */` |
|        6 |  8234 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8235 | `		}` |
|        2 |  8236 | `	}` |
|        6 |  8237 | `	return PH7_OK;` |
|        2 |  8238 |  |
|        - |  8239 | `/*` |
|        - |  8240 | ` * bool restore_error_handler(void)` |
|        - |  8241 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8242 | ` * Parameters:` |
|        - |  8243 | ` *  None.` |
|        - |  8244 | ` * Return` |
|        - |  8245 | ` *  Always TRUE.` |
|        - |  8246 | ` */` |
|        4 |  8247 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8248 |  |
|        5 |  8249 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8250 | `	ph7_value *pOld,*pNew;` |
|        - |  8251 | `	/* Point to the old and the new handler */` |
|        5 |  8252 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  8253 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  8254 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8255 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8256 | `		SXUNUSED(apArg);` |
|        - |  8257 | `		/* No installed callback,return FALSE */` |
|        5 |  8258 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8259 | `		return PH7_OK;` |
|        - |  8260 | `	}` |
|        - |  8261 | `	/* Copy the old callback */` |
|      ! 0 |  8262 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8263 | `	PH7_MemObjRelease(pOld);` |
|        - |  8264 | `	/* Return TRUE */` |
|      ! 0 |  8265 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8266 | `	return PH7_OK;` |
|        3 |  8267 |  |
|        - |  8268 | `/*` |
|        - |  8269 | ` * value set_error_handler(callable $error_handler)` |
|        - |  8270 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8271 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8272 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8273 | ` *  Sets a user-defined error handler function.` |
|        - |  8274 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  8275 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  8276 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  8277 | ` *  conditions (using trigger_error()).` |
|        - |  8278 | ` * Parameters` |
|        - |  8279 | ` *  $error_handler` |
|        - |  8280 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  8281 | ` *   describing the error.` |
|        - |  8282 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  8283 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  8284 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  8285 | ` *   The function can be shown as:` |
|        - |  8286 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  8287 | ` *     errno` |
|        - |  8288 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  8289 | ` *   errstr` |
|        - |  8290 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  8291 | ` *   errfile` |
|        - |  8292 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  8293 | ` *     was raised in, as a string.` |
|        - |  8294 | ` *  Note:` |
|        - |  8295 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8296 | ` * Return` |
|        - |  8297 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  8298 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8299 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8300 | ` */` |
|     8722 |  8301 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8302 |  |
|     8724 |  8303 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8304 | `	ph7_value *pOld,*pNew;` |
|        - |  8305 | `	/* Point to the old and the new handler */` |
|     8724 |  8306 | `	pOld = &pVm->aErrCB[0];` |
|     8724 |  8307 | `	pNew = &pVm->aErrCB[1];` |
|        - |  8308 | `	/* Return the old handler */` |
|     8724 |  8309 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8724 |  8310 | `	if( nArg > 0 ){` |
|     8724 |  8311 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8312 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4361 |  8313 | `			PH7_MemObjRelease(pNew);` |
|     4361 |  8314 | `			ph7_result_bool(pCtx,1);` |
|     2181 |  8315 | `		}else{` |
|     4364 |  8316 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8317 | `			/* Install the new handler */` |
|     4364 |  8318 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8319 | `		}` |
|     4361 |  8320 | `	}` |
|     8724 |  8321 | `	return PH7_OK;` |
|        2 |  8322 |  |
|        - |  8323 | `/*` |
|        - |  8324 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  8325 | ` *  Generates a backtrace.` |
|        - |  8326 | ` * Paramaeter` |
|        - |  8327 | ` *  $options` |
|        - |  8328 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  8329 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  8330 | ` *   all the function/method arguments, to save memory.` |
|        - |  8331 | ` * $limit` |
|        - |  8332 | ` *   (Not Used)` |
|        - |  8333 | ` * Return` |
|        - |  8334 | ` *  An array.The possible returned elements are as follows:` |
|        - |  8335 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  8336 | ` *          Name        Type      Description` |
|        - |  8337 | ` *          ------      ------     -----------` |
|        - |  8338 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  8339 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  8340 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  8341 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  8342 | ` *          object      object    The current object.` |
|        - |  8343 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  8344 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  8345 | ` */` |
|      492 |  8346 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8347 |  |
|      494 |  8348 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8349 | `	ph7_value *pArray;` |
|        - |  8350 | `	ph7_class *pClass;` |
|        - |  8351 | `	ph7_value *pValue;` |
|        - |  8352 | `	SyString *pFile;` |
|        - |  8353 | `	/* Create a new array */` |
|      494 |  8354 | `	pArray = ph7_context_new_array(pCtx);` |
|      494 |  8355 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      494 |  8356 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8357 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8358 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  8359 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8360 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8361 | `		SXUNUSED(apArg);` |
|      ! 0 |  8362 | `		return PH7_OK;` |
|        - |  8363 | `	}` |
|        - |  8364 | `	/* Dump running function name and it's arguments  */` |
|      494 |  8365 | `	if( pVm->pFrame->pParent ){` |
|      494 |  8366 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8367 | `		ph7_vm_func *pFunc;` |
|        - |  8368 | `		ph7_value *pArg;` |
|      494 |  8369 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8370 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  8371 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  8372 | `		}` |
|      494 |  8373 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      494 |  8374 | `		if( pFrame->pParent && pFunc ){` |
|      494 |  8375 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      494 |  8376 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      494 |  8377 | `			ph7_value_reset_string_cursor(pValue);` |
|      246 |  8378 | `		}` |
|        - |  8379 | `		/* Function arguments */` |
|      494 |  8380 | `		pArg = ph7_context_new_array(pCtx);` |
|      494 |  8381 | `		if( pArg  ){` |
|        - |  8382 | `			ph7_value *pObj;` |
|        - |  8383 | `			VmSlot *aSlot;` |
|        - |  8384 | `			sxu32 n;` |
|        - |  8385 | `			/* Start filling the array with the given arguments */` |
|      494 |  8386 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     1962 |  8387 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1470 |  8388 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1470 |  8389 | `				if( pObj ){` |
|     1470 |  8390 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      734 |  8391 | `				}` |
|      736 |  8392 | `			}` |
|        - |  8393 | `			/* Save the array */` |
|      494 |  8394 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      246 |  8395 | `		}` |
|      246 |  8396 | `	}` |
|      494 |  8397 | `	ph7_value_int(pValue,1);` |
|        - |  8398 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  8399 | `	 * line numbers at run-time. )` |
|        - |  8400 | `	 */` |
|      494 |  8401 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  8402 | `	/* Current processed script */` |
|      494 |  8403 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      494 |  8404 | `	if( pFile ){` |
|      494 |  8405 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      494 |  8406 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      494 |  8407 | `		ph7_value_reset_string_cursor(pValue);` |
|      246 |  8408 | `	}` |
|        - |  8409 | `	/* Top class */` |
|      494 |  8410 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      494 |  8411 | `	if( pClass ){` |
|      490 |  8412 | `		ph7_value_reset_string_cursor(pValue);` |
|      490 |  8413 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      490 |  8414 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      244 |  8415 | `	}` |
|        - |  8416 | `	/* Return the freshly created array */` |
|      494 |  8417 | `	ph7_result_value(pCtx,pArray);` |
|        - |  8418 | `	/*` |
|        - |  8419 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  8420 | `	 * as soon we return from this function.` |
|        - |  8421 | `	 */` |
|      494 |  8422 | `	return PH7_OK;` |
|      248 |  8423 |  |
|        - |  8424 | `/*` |
|        - |  8425 | ` * Generate a small backtrace.` |
|        - |  8426 | ` * Store the generated dump in the given BLOB` |
|        - |  8427 | ` */` |
|        4 |  8428 | `static int VmMiniBacktrace(` |
|        - |  8429 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8430 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  8431 | `	)` |
|        1 |  8432 |  |
|        5 |  8433 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8434 | `	ph7_vm_func *pFunc;` |
|        - |  8435 | `	ph7_class *pClass;` |
|        - |  8436 | `	SyString *pFile;` |
|        - |  8437 | `	/* Called function */` |
|        5 |  8438 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8439 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  8440 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  8441 | `	}` |
|        5 |  8442 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  8443 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8444 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  8445 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  8446 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  8447 | `	}else{` |
|      ! 0 |  8448 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  8449 | `	}` |
|        5 |  8450 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  8451 | `	/* Current processed script */` |
|        5 |  8452 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  8453 | `	if( pFile ){` |
|        5 |  8454 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8455 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  8456 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  8457 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  8458 | `	}` |
|        - |  8459 | `	/* Top class */` |
|        5 |  8460 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  8461 | `	if( pClass ){` |
|      ! 0 |  8462 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  8463 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  8464 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  8465 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  8466 | `	}` |
|        5 |  8467 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  8468 | `	/* All done */` |
|        5 |  8469 | `	return SXRET_OK;` |
|        1 |  8470 |  |
|        - |  8471 | `/*` |
|        - |  8472 | ` * void debug_print_backtrace()` |
|        - |  8473 | ` *  Prints a backtrace` |
|        - |  8474 | ` * Parameters` |
|        - |  8475 | ` * None` |
|        - |  8476 | ` * Return` |
|        - |  8477 | ` * NULL` |
|        - |  8478 | ` */` |
|        2 |  8479 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8480 |  |
|        3 |  8481 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8482 | `	SyBlob sDump;` |
|        3 |  8483 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8484 | `	/* Generate the backtrace */` |
|        3 |  8485 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8486 | `	/* Output backtrace */` |
|        3 |  8487 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8488 | `	/* All done,cleanup */` |
|        3 |  8489 | `	SyBlobRelease(&sDump);` |
|        1 |  8490 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8491 | `	SXUNUSED(apArg);` |
|        3 |  8492 | `	return PH7_OK;` |
|        1 |  8493 |  |
|        - |  8494 | `/*` |
|        - |  8495 | ` * string debug_string_backtrace()` |
|        - |  8496 | ` *  Generate a backtrace` |
|        - |  8497 | ` * Parameters` |
|        - |  8498 | ` * None` |
|        - |  8499 | ` * Return` |
|        - |  8500 | ` *  A mini backtrace().` |
|        - |  8501 | ` * Note that this is a symisc extension.` |
|        - |  8502 | ` */` |
|        2 |  8503 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8504 |  |
|        3 |  8505 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8506 | `	SyBlob sDump;` |
|        3 |  8507 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8508 | `	/* Generate the backtrace */` |
|        3 |  8509 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8510 | `	/* Return the backtrace */` |
|        3 |  8511 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  8512 | `	/* All done,cleanup */` |
|        3 |  8513 | `	SyBlobRelease(&sDump);` |
|        1 |  8514 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8515 | `	SXUNUSED(apArg);` |
|        3 |  8516 | `	return PH7_OK;` |
|        1 |  8517 |  |
|        - |  8518 | `/*` |
|        - |  8519 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  8520 | ` * exception is triggered.` |
|        - |  8521 | ` */` |
|      472 |  8522 | `static sxi32 VmUncaughtException(` |
|        - |  8523 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8524 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8525 | `	)` |
|        1 |  8526 |  |
|        - |  8527 | `	ph7_value *apArg[2],sArg;` |
|      473 |  8528 | `	int nArg = 1;` |
|        - |  8529 | `	sxi32 rc;` |
|      473 |  8530 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  8531 | `		/* Nesting limit reached */` |
|      ! 0 |  8532 | `		return SXRET_OK;` |
|        - |  8533 | `	}` |
|        - |  8534 | `	/* Call any exception handler if available */` |
|      473 |  8535 | `	PH7_MemObjInit(pVm,&sArg);` |
|      473 |  8536 | `	if( pThis ){` |
|        - |  8537 | `		/* Load the exception instance */` |
|      473 |  8538 | `		sArg.x.pOther = pThis;` |
|      473 |  8539 | `		pThis->iRef++;` |
|      473 |  8540 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      237 |  8541 | `	}else{` |
|      ! 0 |  8542 | `		nArg = 0;` |
|        - |  8543 | `	}` |
|      473 |  8544 | `	apArg[0] = &sArg;` |
|        - |  8545 | `	/* Call the exception handler if available */` |
|      473 |  8546 | `	pVm->nExceptDepth++;` |
|      473 |  8547 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      473 |  8548 | `	pVm->nExceptDepth--;` |
|      473 |  8549 | `	if( rc != SXRET_OK ){` |
|        - |  8550 | `		SyBlob sMsgBuf;` |
|      471 |  8551 | `		const char *zClass = "Exception";` |
|      471 |  8552 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  8553 | `		const char *zMsg;` |
|        - |  8554 | `		sxu32 nMsg;` |
|        - |  8555 | `		const char *zFuncName;` |
|        - |  8556 | `		int nFuncLen;` |
|      471 |  8557 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      471 |  8558 | `		if( pThis ){` |
|        - |  8559 | `			ph7_class_method *pGetMessage;` |
|        - |  8560 | `			ph7_value sMsg;` |
|        - |  8561 | `			const char *zTmp;` |
|        - |  8562 | `			int nTmp;` |
|      471 |  8563 | `			zClass = pThis->pClass->sName.zString;` |
|      471 |  8564 | `			nClass = pThis->pClass->sName.nByte;` |
|      471 |  8565 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      471 |  8566 | `			if( pGetMessage ){` |
|      471 |  8567 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      471 |  8568 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      471 |  8569 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      471 |  8570 | `					if( zTmp && nTmp > 0 ){` |
|      471 |  8571 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      235 |  8572 | `					}` |
|      235 |  8573 | `				}` |
|      471 |  8574 | `				PH7_MemObjRelease(&sMsg);` |
|      235 |  8575 | `			}` |
|      235 |  8576 | `		}` |
|      471 |  8577 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  8578 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  8579 | `		}` |
|      471 |  8580 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      471 |  8581 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      471 |  8582 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      471 |  8583 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      471 |  8584 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  8585 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      471 |  8586 | `		rc = SXERR_ABORT;` |
|      235 |  8587 | `	}` |
|      473 |  8588 | `	PH7_MemObjRelease(&sArg);` |
|      473 |  8589 | `	return rc;` |
|      237 |  8590 |  |
|        - |  8591 | `/*` |
|        - |  8592 | ` * Throw an user exception.` |
|        - |  8593 | ` */` |
|      490 |  8594 | `static sxi32 VmThrowException(` |
|        - |  8595 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  8596 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8597 | `	)` |
|        2 |  8598 |  |
|        - |  8599 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  8600 | `	ph7_exception **apException;` |
|        - |  8601 | `	ph7_exception *pException;` |
|        - |  8602 | `	/* Point to the stack of loaded exceptions */` |
|      492 |  8603 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      492 |  8604 | `	pException = 0;` |
|      492 |  8605 | `	pCatch = 0;` |
|      492 |  8606 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8607 | `		ph7_exception_block *aCatch;` |
|        - |  8608 | `		ph7_class *pClass;` |
|        - |  8609 | `		sxu32 j;` |
|        - |  8610 | `		/* Locate the appropriate block to execute */` |
|       20 |  8611 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       20 |  8612 | `		(void)SySetPop(&pVm->aException);` |
|       20 |  8613 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       20 |  8614 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       20 |  8615 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  8616 | `			/* Extract the target class */` |
|       20 |  8617 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       20 |  8618 | `			if( pClass == 0 ){` |
|        - |  8619 | `				/* No such class */` |
|      ! 0 |  8620 | `				continue;` |
|        - |  8621 | `			}` |
|       20 |  8622 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  8623 | `				/* Catch block found,break immeditaley */` |
|       20 |  8624 | `				pCatch = &aCatch[j];` |
|       20 |  8625 | `				break;` |
|        - |  8626 | `			}` |
|      ! 0 |  8627 | `		}` |
|        9 |  8628 | `	}` |
|        - |  8629 | `	/* Execute the cached block if available */` |
|      492 |  8630 | `	if( pCatch == 0 ){` |
|        - |  8631 | `		sxi32 rc;` |
|      473 |  8632 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      473 |  8633 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  8634 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  8635 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8636 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  8637 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  8638 | `			}` |
|      ! 0 |  8639 | `			if( pException->pFrame == pFrame ){` |
|        - |  8640 | `				/* Tell the upper layer that the exception was caught */` |
|      ! 0 |  8641 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  8642 | `			}` |
|      ! 0 |  8643 | `		}` |
|      473 |  8644 | `		return rc;` |
|      ! 0 |  8645 | `	}else{` |
|       20 |  8646 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8647 | `		sxi32 rc;` |
|       30 |  8648 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8649 | `			/* Safely ignore the exception frame */` |
|       12 |  8650 | `			pFrame = pFrame->pParent;` |
|        2 |  8651 | `		}` |
|       20 |  8652 | `		if( pException->pFrame == pFrame ){` |
|        - |  8653 | `			/* Tell the upper layer that the exception was caught */` |
|       12 |  8654 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        5 |  8655 | `		}` |
|        - |  8656 | `		/* Create a private frame first */` |
|       20 |  8657 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       20 |  8658 | `		if( rc == SXRET_OK ){` |
|        - |  8659 | `			/* Mark as catch frame */` |
|       20 |  8660 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       20 |  8661 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       20 |  8662 | `			if( pObj ){` |
|        - |  8663 | `				/* Install the exception instance */` |
|       20 |  8664 | `				pThis->iRef++; /* Increment reference count */` |
|       20 |  8665 | `				pObj->x.pOther = pThis;` |
|       20 |  8666 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        9 |  8667 | `			}` |
|        - |  8668 | `			/* Exceute the block */` |
|       20 |  8669 | `			VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  8670 | `			/* Leave the frame */` |
|       20 |  8671 | `			VmLeaveFrame(&(*pVm));` |
|        9 |  8672 | `		}` |
|        - |  8673 | `	}` |
|        - |  8674 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  8675 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  8676 | `	 */` |
|       20 |  8677 | `	return SXRET_OK;` |
|      247 |  8678 |  |
|        - |  8679 | `/*` |
|        - |  8680 | ` * Section:` |
|        - |  8681 | ` *  Version,Credits and Copyright related functions.` |
|        - |  8682 | ` * Status:` |
|        - |  8683 | ` *    Stable.` |
|        - |  8684 | ` */` |
|        - |  8685 | `/*` |
|        - |  8686 | ` * string ph7version(void)` |
|        - |  8687 | ` *  Returns the running version of the PH7 version.` |
|        - |  8688 | ` * Parameters` |
|        - |  8689 | ` *  None` |
|        - |  8690 | ` * Return` |
|        - |  8691 | ` * Current PH7 version.` |
|        - |  8692 | ` */` |
|        2 |  8693 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8694 |  |
|        1 |  8695 | `	SXUNUSED(nArg);` |
|        1 |  8696 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  8697 | `	/* Current engine version */` |
|        3 |  8698 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  8699 | `	return PH7_OK;` |
|        1 |  8700 |  |
|        - |  8701 | `/*` |
|        - |  8702 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  8703 | ` */` |
|        - |  8704 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  8705 | ` "<html><head>"\` |
|        - |  8706 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  8707 | ` "<style type=\"text/css\">"\` |
|        - |  8708 | ` "div {"\` |
|        - |  8709 | `     "border: 1px solid #cccccc;"\` |
|        - |  8710 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  8711 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  8712 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  8713 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  8714 | `     "-webkit-border-radius: 10px;"\` |
|        - |  8715 | `     "-o-border-radius: 10px;"\` |
|        - |  8716 | `     "border-radius: 10px;"\` |
|        - |  8717 | `     "padding-left: 2em;"\` |
|        - |  8718 | `     "background-color: white;"\` |
|        - |  8719 | `     "margin-left: auto;"\` |
|        - |  8720 | `     "font-family: verdana;"\` |
|        - |  8721 | `     "padding-right: 2em;"\` |
|        - |  8722 | `     "margin-right: auto;"\` |
|        - |  8723 | `     "}"\` |
|        - |  8724 | `     "body {"\` |
|        - |  8725 | `     "padding: 0.2em;"\` |
|        - |  8726 | `     "font-style: normal;"\` |
|        - |  8727 | `     "font-size: medium;"\` |
|        - |  8728 | `     "background-color: #f2f2f2;"\` |
|        - |  8729 | `     "}"\` |
|        - |  8730 | `     "hr {"\` |
|        - |  8731 | `     "border-style: solid none none;"\` |
|        - |  8732 | `     "border-width: 1px medium medium;"\` |
|        - |  8733 | `     "border-top: 1px solid #cccccc;"\` |
|        - |  8734 | `     "height: 1px;"\` |
|        - |  8735 | `     "}"\` |
|        - |  8736 | `     "a {"\` |
|        - |  8737 | `     "color: #3366cc;"\` |
|        - |  8738 | `     "text-decoration: none;"\` |
|        - |  8739 | `     "}"\` |
|        - |  8740 | `     "a:hover {"\` |
|        - |  8741 | `     "color: #999999;"\` |
|        - |  8742 | `     "}"\` |
|        - |  8743 | `     "a:active {"\` |
|        - |  8744 | `     "color: #663399;"\` |
|        - |  8745 | `     "}"\` |
|        - |  8746 | `     "h1 {"\` |
|        - |  8747 | `     "margin: 0;"\` |
|        - |  8748 | `     "padding: 0;"\` |
|        - |  8749 | `     "font-family: Verdana;"\` |
|        - |  8750 | `     "font-weight: bold;"\` |
|        - |  8751 | `     "font-style: normal;"\` |
|        - |  8752 | `     "font-size: medium;"\` |
|        - |  8753 | `     "text-transform: capitalize;"\` |
|        - |  8754 | `     "color: #0a328c;"\` |
|        - |  8755 | `     "}"\` |
|        - |  8756 | `     "p {"\` |
|        - |  8757 | `     "margin: 0 auto;"\` |
|        - |  8758 | `     "font-size: medium;"\` |
|        - |  8759 | `     "font-style: normal;"\` |
|        - |  8760 | `     "font-family: verdana;"\` |
|        - |  8761 | `     "}"\` |
|        - |  8762 | `"</style></head><body>"\` |
|        - |  8763 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - |  8764 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - |  8765 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - |  8766 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - |  8767 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - |  8768 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - |  8769 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - |  8770 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - |  8771 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - |  8772 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - |  8773 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - |  8774 |  |
|        - |  8775 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8776 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - |  8777 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - |  8778 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - |  8779 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8780 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - |  8781 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  8782 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - |  8783 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  8784 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - |  8785 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8786 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - |  8787 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - |  8788 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - |  8789 |  |
|        - |  8790 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - |  8791 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - |  8792 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - |  8793 | `"&nbsp;*<br>"\` |
|        - |  8794 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - |  8795 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - |  8796 | `"&nbsp;* are met:<br>"\` |
|        - |  8797 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - |  8798 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - |  8799 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - |  8800 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - |  8801 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - |  8802 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - |  8803 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - |  8804 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - |  8805 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - |  8806 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - |  8807 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - |  8808 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - |  8809 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - |  8810 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - |  8811 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - |  8812 | `"&nbsp;*<br>"\` |
|        - |  8813 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - |  8814 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - |  8815 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - |  8816 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - |  8817 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - |  8818 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - |  8819 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - |  8820 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - |  8821 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - |  8822 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - |  8823 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - |  8824 | `"&nbsp;*/<br>"\` |
|        - |  8825 | `"</span></small></small></p>"\` |
|        - |  8826 | `"</div></body></html>"` |
|        - |  8827 | `/*` |
|        - |  8828 | ` * bool ph7credits(void)` |
|        - |  8829 | ` * bool ph7info(void)` |
|        - |  8830 | ` * bool ph7copyright(void)` |
|        - |  8831 | ` *  Prints out the credits for PH7 engine` |
|        - |  8832 | ` * Parameters` |
|        - |  8833 | ` *  None` |
|        - |  8834 | ` * Return` |
|        - |  8835 | ` *  Always TRUE` |
|        - |  8836 | ` */` |
|        2 |  8837 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8838 |  |
|        3 |  8839 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - |  8840 | `	/* Expand the HTML page above*/` |
|        3 |  8841 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 |  8842 | `	ph7_context_output_format(` |
|        1 |  8843 | `		pCtx,` |
|        - |  8844 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 |  8845 | `		ph7_lib_version(),   /* Engine version */` |
|        1 |  8846 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 |  8847 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 |  8848 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 |  8849 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 |  8850 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - |  8851 | `#ifdef __WINNT__` |
|        - |  8852 | `		"Windows NT"` |
|        - |  8853 | `#elif defined(__UNIXES__)` |
|        - |  8854 | `		"UNIX-Like"` |
|        - |  8855 | `#else` |
|        - |  8856 | `		"Other OS"` |
|        - |  8857 | `#endif` |
|        - |  8858 | `		);` |
|        3 |  8859 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 |  8860 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8861 | `	SXUNUSED(apArg);` |
|        - |  8862 | `	/* Return TRUE */` |
|        - |  8863 | `	//ph7_result_bool(pCtx,1);` |
|        3 |  8864 | `	return PH7_OK;` |
|        1 |  8865 |  |
|        - |  8866 | `/*` |
|        - |  8867 | ` * Section:` |
|        - |  8868 | ` *    URL related routines.` |
|        - |  8869 | ` * Status:` |
|        - |  8870 | ` *    Stable.` |
|        - |  8871 | ` */` |
|        - |  8872 | `/*` |
|        - |  8873 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - |  8874 | ` *  Parse a URL and return its fields.` |
|        - |  8875 | ` * Parameters` |
|        - |  8876 | ` *  $url` |
|        - |  8877 | ` *   The URL to parse.` |
|        - |  8878 | ` * $component` |
|        - |  8879 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - |  8880 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - |  8881 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - |  8882 | ` *  in which case the return value will be an integer).` |
|        - |  8883 | ` * Return` |
|        - |  8884 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - |  8885 | ` *  At least one element will be present within the array. Potential keys within` |
|        - |  8886 | ` *  this array are:` |
|        - |  8887 | ` *   scheme - e.g. http` |
|        - |  8888 | ` *   host` |
|        - |  8889 | ` *   port` |
|        - |  8890 | ` *   user` |
|        - |  8891 | ` *   pass` |
|        - |  8892 | ` *   path` |
|        - |  8893 | ` *   query - after the question mark ?` |
|        - |  8894 | ` *   fragment - after the hashmark #` |
|        - |  8895 | ` * Note:` |
|        - |  8896 | ` *  FALSE is returned on failure.` |
|        - |  8897 | ` *  This function work with relative URL unlike the one shipped` |
|        - |  8898 | ` *  with the standard PHP engine.` |
|        - |  8899 | ` */` |
|       28 |  8900 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8901 |  |
|        - |  8902 | `	const char *zStr; /* Input string */` |
|        - |  8903 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - |  8904 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - |  8905 | `	int nLen;` |
|        - |  8906 | `	sxi32 rc;` |
|       29 |  8907 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  8908 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  8909 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8910 | `		return PH7_OK;` |
|        - |  8911 | `	}` |
|        - |  8912 | `	/* Extract the given URI */` |
|       29 |  8913 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 |  8914 | `	if( nLen < 1 ){` |
|        - |  8915 | `		/* Nothing to process,return FALSE */` |
|        3 |  8916 | `		ph7_result_bool(pCtx,0);` |
|        3 |  8917 | `		return PH7_OK;` |
|        - |  8918 | `	}` |
|        - |  8919 | `	/* Get a parse */` |
|       27 |  8920 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 |  8921 | `	if( rc != SXRET_OK ){` |
|        - |  8922 | `		/* Malformed input,return FALSE */` |
|      ! 0 |  8923 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8924 | `		return PH7_OK;` |
|        - |  8925 | `	}` |
|       27 |  8926 | `	if( nArg > 1 ){` |
|      ! 0 |  8927 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - |  8928 | `		/* Refer to constant.c for constants values */` |
|      ! 0 |  8929 | `		switch(nComponent){` |
|      ! 0 |  8930 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 |  8931 | `			pComp = &sURI.sScheme;` |
|      ! 0 |  8932 | `			if( pComp->nByte < 1 ){` |
|        - |  8933 | `				/* No available value,return NULL */` |
|      ! 0 |  8934 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8935 | `			}else{` |
|      ! 0 |  8936 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8937 | `			}` |
|      ! 0 |  8938 | `			break;` |
|      ! 0 |  8939 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 |  8940 | `			pComp = &sURI.sHost;` |
|      ! 0 |  8941 | `			if( pComp->nByte < 1 ){` |
|        - |  8942 | `				/* No available value,return NULL */` |
|      ! 0 |  8943 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8944 | `			}else{` |
|      ! 0 |  8945 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8946 | `			}` |
|      ! 0 |  8947 | `			break;` |
|      ! 0 |  8948 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 |  8949 | `			pComp = &sURI.sPort;` |
|      ! 0 |  8950 | `			if( pComp->nByte < 1 ){` |
|        - |  8951 | `				/* No available value,return NULL */` |
|      ! 0 |  8952 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8953 | `			}else{` |
|      ! 0 |  8954 | `				int iPort = 0;` |
|        - |  8955 | `				/* Cast the value to integer */` |
|      ! 0 |  8956 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 |  8957 | `				ph7_result_int(pCtx,iPort);` |
|        - |  8958 | `			}` |
|      ! 0 |  8959 | `			break;` |
|      ! 0 |  8960 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 |  8961 | `			pComp = &sURI.sUser;` |
|      ! 0 |  8962 | `			if( pComp->nByte < 1 ){` |
|        - |  8963 | `				/* No available value,return NULL */` |
|      ! 0 |  8964 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8965 | `			}else{` |
|      ! 0 |  8966 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8967 | `			}` |
|      ! 0 |  8968 | `			break;` |
|      ! 0 |  8969 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 |  8970 | `			pComp = &sURI.sPass;` |
|      ! 0 |  8971 | `			if( pComp->nByte < 1 ){` |
|        - |  8972 | `				/* No available value,return NULL */` |
|      ! 0 |  8973 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8974 | `			}else{` |
|      ! 0 |  8975 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8976 | `			}` |
|      ! 0 |  8977 | `			break;` |
|      ! 0 |  8978 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 |  8979 | `			pComp = &sURI.sQuery;` |
|      ! 0 |  8980 | `			if( pComp->nByte < 1 ){` |
|        - |  8981 | `				/* No available value,return NULL */` |
|      ! 0 |  8982 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8983 | `			}else{` |
|      ! 0 |  8984 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8985 | `			}` |
|      ! 0 |  8986 | `			break;` |
|      ! 0 |  8987 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 |  8988 | `			pComp = &sURI.sFragment;` |
|      ! 0 |  8989 | `			if( pComp->nByte < 1 ){` |
|        - |  8990 | `				/* No available value,return NULL */` |
|      ! 0 |  8991 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8992 | `			}else{` |
|      ! 0 |  8993 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8994 | `			}` |
|      ! 0 |  8995 | `			break;` |
|      ! 0 |  8996 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 |  8997 | `			pComp = &sURI.sPath;` |
|      ! 0 |  8998 | `			if( pComp->nByte < 1 ){` |
|        - |  8999 | `				/* No available value,return NULL */` |
|      ! 0 |  9000 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9001 | `			}else{` |
|      ! 0 |  9002 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9003 | `			}` |
|      ! 0 |  9004 | `			break;` |
|      ! 0 |  9005 | `		default:` |
|        - |  9006 | `			/* No such entry,return NULL */` |
|      ! 0 |  9007 | `			ph7_result_null(pCtx);` |
|      ! 0 |  9008 | `			break;` |
|        - |  9009 | `		}` |
|      ! 0 |  9010 | `	}else{` |
|        - |  9011 | `		ph7_value *pArray,*pValue;` |
|        - |  9012 | `		/* Return an associative array */` |
|       27 |  9013 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 |  9014 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 |  9015 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9016 | `			/* Out of memory */` |
|      ! 0 |  9017 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9018 | `			/* Return false */` |
|      ! 0 |  9019 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |  9020 | `			return PH7_OK;` |
|        - |  9021 | `		}` |
|        - |  9022 | `		/* Fill the array */` |
|       27 |  9023 | `		pComp = &sURI.sScheme;` |
|       27 |  9024 | `		if( pComp->nByte > 0 ){` |
|       19 |  9025 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 |  9026 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 |  9027 | `		}` |
|        - |  9028 | `		/* Reset the string cursor */` |
|       27 |  9029 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9030 | `		pComp = &sURI.sHost;` |
|       27 |  9031 | `		if( pComp->nByte > 0 ){` |
|       25 |  9032 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 |  9033 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 |  9034 | `		}` |
|        - |  9035 | `		/* Reset the string cursor */` |
|       27 |  9036 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9037 | `		pComp = &sURI.sPort;` |
|       27 |  9038 | `		if( pComp->nByte > 0 ){` |
|       11 |  9039 | `			int iPort = 0;/* cc warning */` |
|        - |  9040 | `			/* Convert to integer */` |
|       11 |  9041 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 |  9042 | `			ph7_value_int(pValue,iPort);` |
|       11 |  9043 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 |  9044 | `		}` |
|        - |  9045 | `		/* Reset the string cursor */` |
|       27 |  9046 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9047 | `		pComp = &sURI.sUser;` |
|       27 |  9048 | `		if( pComp->nByte > 0 ){` |
|        7 |  9049 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9050 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 |  9051 | `		}` |
|        - |  9052 | `		/* Reset the string cursor */` |
|       27 |  9053 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9054 | `		pComp = &sURI.sPass;` |
|       27 |  9055 | `		if( pComp->nByte > 0 ){` |
|        7 |  9056 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9057 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 |  9058 | `		}` |
|        - |  9059 | `		/* Reset the string cursor */` |
|       27 |  9060 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9061 | `		pComp = &sURI.sPath;` |
|       27 |  9062 | `		if( pComp->nByte > 0 ){` |
|       17 |  9063 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 |  9064 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 |  9065 | `		}` |
|        - |  9066 | `		/* Reset the string cursor */` |
|       27 |  9067 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9068 | `		pComp = &sURI.sQuery;` |
|       27 |  9069 | `		if( pComp->nByte > 0 ){` |
|        5 |  9070 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9071 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 |  9072 | `		}` |
|        - |  9073 | `		/* Reset the string cursor */` |
|       27 |  9074 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9075 | `		pComp = &sURI.sFragment;` |
|       27 |  9076 | `		if( pComp->nByte > 0 ){` |
|        5 |  9077 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9078 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 |  9079 | `		}` |
|        - |  9080 | `		/* Return the created array */` |
|       27 |  9081 | `		ph7_result_value(pCtx,pArray);` |
|        - |  9082 | `		/* NOTE:` |
|        - |  9083 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - |  9084 | `		 * automatically as soon we return from this function.` |
|        - |  9085 | `		 */` |
|        - |  9086 | `	}` |
|        - |  9087 | `	/* All done */` |
|       27 |  9088 | `	return PH7_OK;` |
|       15 |  9089 |  |
|        - |  9090 | `/*` |
|        - |  9091 | ` * Section:` |
|        - |  9092 | ` *   Array related routines.` |
|        - |  9093 | ` * Status:` |
|        - |  9094 | ` *    Stable.` |
|        - |  9095 | ` * Note 2012-5-21 01:04:15:` |
|        - |  9096 | ` *  Array related functions that need access to the underlying` |
|        - |  9097 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - |  9098 | ` */` |
|        - |  9099 | `/*` |
|        - |  9100 | ` * The [compact()] function store it's state information in an instance` |
|        - |  9101 | ` * of the following structure.` |
|        - |  9102 | ` */` |
|        - |  9103 | `struct compact_data` |
|        - |  9104 |  |
|        - |  9105 | `	ph7_value *pArray;  /* Target array */` |
|        - |  9106 | `	int nRecCount;      /* Recursion count */` |
|        - |  9107 | `};` |
|        - |  9108 | `/*` |
|        - |  9109 | ` * Walker callback for the [compact()] function defined below.` |
|        - |  9110 | ` */` |
|      ! 0 |  9111 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 |  9112 |  |
|      ! 0 |  9113 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 |  9114 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 |  9115 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9116 | `	/* Act according to the hashmap value */` |
|      ! 0 |  9117 | `	if( ph7_value_is_string(pValue) ){` |
|        - |  9118 | `		SyString sVar;` |
|      ! 0 |  9119 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 |  9120 | `		if( sVar.nByte > 0 ){` |
|        - |  9121 | `			/* Query the current frame */` |
|      ! 0 |  9122 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - |  9123 | `			/* ^` |
|        - |  9124 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - |  9125 | `			 */` |
|      ! 0 |  9126 | `			if( pKey ){` |
|        - |  9127 | `				/* Perform the insertion */` |
|      ! 0 |  9128 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 |  9129 | `			}` |
|      ! 0 |  9130 | `		}` |
|      ! 0 |  9131 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - |  9132 | `		int rc;` |
|        - |  9133 | `		/* Recursively traverse this array */` |
|      ! 0 |  9134 | `		pData->nRecCount++;` |
|      ! 0 |  9135 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 |  9136 | `		pData->nRecCount--;` |
|      ! 0 |  9137 | `		return rc;` |
|        - |  9138 | `	}` |
|      ! 0 |  9139 | `	return SXRET_OK;` |
|      ! 0 |  9140 |  |
|        - |  9141 | `/*` |
|        - |  9142 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - |  9143 | ` *  Create array containing variables and their values.` |
|        - |  9144 | ` *  For each of these, compact() looks for a variable with that name` |
|        - |  9145 | ` *  in the current symbol table and adds it to the output array such` |
|        - |  9146 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - |  9147 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - |  9148 | ` *  Any strings that are not set will simply be skipped.` |
|        - |  9149 | ` * Parameters` |
|        - |  9150 | ` *  $varname` |
|        - |  9151 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - |  9152 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - |  9153 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - |  9154 | ` *   it recursively.` |
|        - |  9155 | ` * Return` |
|        - |  9156 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - |  9157 | ` */` |
|        2 |  9158 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9159 |  |
|        - |  9160 | `	ph7_value *pArray,*pObj;` |
|        3 |  9161 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9162 | `	const char *zName;` |
|        - |  9163 | `	SyString sVar;` |
|        - |  9164 | `	int i,nLen;` |
|        3 |  9165 | `	if( nArg < 1 ){` |
|        - |  9166 | `		/* Missing arguments,return NULL */` |
|      ! 0 |  9167 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9168 | `		return PH7_OK;` |
|        - |  9169 | `	}` |
|        - |  9170 | `	/* Create the array */` |
|        3 |  9171 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9172 | `	if( pArray == 0 ){` |
|        - |  9173 | `		/* Out of memory */` |
|      ! 0 |  9174 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9175 | `		/* Return NULL */` |
|      ! 0 |  9176 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9177 | `		return PH7_OK;` |
|        - |  9178 | `	}` |
|        - |  9179 | `	/* Perform the requested operation */` |
|        7 |  9180 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 |  9181 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 |  9182 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - |  9183 | `				struct compact_data sData;` |
|      ! 0 |  9184 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - |  9185 | `				/* Recursively walk the array */` |
|      ! 0 |  9186 | `				sData.nRecCount = 0;` |
|      ! 0 |  9187 | `				sData.pArray = pArray;` |
|      ! 0 |  9188 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 |  9189 | `			}` |
|      ! 0 |  9190 | `		}else{` |
|        - |  9191 | `			/* Extract variable name */` |
|        5 |  9192 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 |  9193 | `			if( nLen > 0 ){` |
|        5 |  9194 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - |  9195 | `				/* Check if the variable is available in the current frame */` |
|        5 |  9196 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 |  9197 | `				if( pObj ){` |
|        5 |  9198 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 |  9199 | `				}` |
|        2 |  9200 | `			}` |
|        - |  9201 | `		}` |
|        3 |  9202 | `	}` |
|        - |  9203 | `	/* Return the array */` |
|        3 |  9204 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9205 | `	return PH7_OK;` |
|        2 |  9206 |  |
|        - |  9207 | `/*` |
|        - |  9208 | ` * The [extract()] function store it's state information in an instance` |
|        - |  9209 | ` * of the following structure.` |
|        - |  9210 | ` */` |
|        - |  9211 | `typedef struct extract_aux_data extract_aux_data;` |
|        - |  9212 | `struct extract_aux_data` |
|        - |  9213 |  |
|        - |  9214 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - |  9215 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - |  9216 | `	const char *zPrefix;  /* Prefix name */` |
|        - |  9217 | `	int Prefixlen;        /* Prefix  length */` |
|        - |  9218 | `	int iFlags;           /* Control flags */` |
|        - |  9219 | `	char zWorker[1024];   /* Working buffer */` |
|        - |  9220 | `};` |
|        - |  9221 | `/* Forward declaration */` |
|        - |  9222 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - |  9223 | `/*` |
|        - |  9224 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - |  9225 | ` *   Import variables into the current symbol table from an array.` |
|        - |  9226 | ` * Parameters` |
|        - |  9227 | ` * $var_array` |
|        - |  9228 | ` *  An associative array. This function treats keys as variable names and values` |
|        - |  9229 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - |  9230 | ` *  table, subject to extract_type and prefix parameters.` |
|        - |  9231 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - |  9232 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - |  9233 | ` * $extract_type` |
|        - |  9234 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - |  9235 | ` *  It can be one of the following values:` |
|        - |  9236 | ` *   EXTR_OVERWRITE` |
|        - |  9237 | ` *       If there is a collision, overwrite the existing variable.` |
|        - |  9238 | ` *   EXTR_SKIP` |
|        - |  9239 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - |  9240 | ` *   EXTR_PREFIX_SAME` |
|        - |  9241 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - |  9242 | ` *   EXTR_PREFIX_ALL` |
|        - |  9243 | ` *       Prefix all variable names with prefix.` |
|        - |  9244 | ` *   EXTR_PREFIX_INVALID` |
|        - |  9245 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - |  9246 | ` *   EXTR_IF_EXISTS` |
|        - |  9247 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - |  9248 | ` *       otherwise do nothing.` |
|        - |  9249 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - |  9250 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - |  9251 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - |  9252 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - |  9253 | ` *      the current symbol table.` |
|        - |  9254 | ` * $prefix` |
|        - |  9255 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - |  9256 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - |  9257 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - |  9258 | ` *  underscore character.` |
|        - |  9259 | ` * Return` |
|        - |  9260 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - |  9261 | ` */` |
|        4 |  9262 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9263 |  |
|        - |  9264 | `	extract_aux_data sAux;` |
|        - |  9265 | `	ph7_hashmap *pMap;` |
|        5 |  9266 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - |  9267 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 |  9268 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9269 | `		return PH7_OK;` |
|        - |  9270 | `	}` |
|        - |  9271 | `	/* Point to the target hashmap */` |
|        5 |  9272 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 |  9273 | `	if( pMap->nEntry < 1 ){` |
|        - |  9274 | `		/* Empty map,return  0 */` |
|      ! 0 |  9275 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9276 | `		return PH7_OK;` |
|        - |  9277 | `	}` |
|        - |  9278 | `	/* Prepare the aux data */` |
|        5 |  9279 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 |  9280 | `	if( nArg > 1 ){` |
|        3 |  9281 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 |  9282 | `		if( nArg > 2 ){` |
|      ! 0 |  9283 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 |  9284 | `		}` |
|        1 |  9285 | `	}` |
|        5 |  9286 | `	sAux.pVm = pCtx->pVm;` |
|        - |  9287 | `	/* Invoke the worker callback */` |
|        5 |  9288 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - |  9289 | `	/* Number of variables successfully imported */` |
|        5 |  9290 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 |  9291 | `	return PH7_OK;` |
|        3 |  9292 |  |
|        - |  9293 | `/*` |
|        - |  9294 | ` * Worker callback for the [extract()] function defined` |
|        - |  9295 | ` * below.` |
|        - |  9296 | ` */` |
|        8 |  9297 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9298 |  |
|        9 |  9299 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 |  9300 | `	int iFlags = pAux->iFlags;` |
|        9 |  9301 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9302 | `	ph7_value *pObj;` |
|        - |  9303 | `	SyString sVar;` |
|        9 |  9304 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 |  9305 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 |  9306 | `	}` |
|        - |  9307 | `	/* Perform a string cast */` |
|        9 |  9308 | `	PH7_MemObjToString(pKey);` |
|        9 |  9309 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9310 | `		/* Unavailable variable name */` |
|      ! 0 |  9311 | `		return SXRET_OK;` |
|        - |  9312 | `	}` |
|        9 |  9313 | `	sVar.nByte = 0; /* cc warning */` |
|        9 |  9314 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 |  9315 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9316 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9317 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9318 | `			);` |
|      ! 0 |  9319 | `	}else{` |
|       13 |  9320 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 |  9321 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9322 | `	}` |
|        9 |  9323 | `	sVar.zString = pAux->zWorker;` |
|        - |  9324 | `	/* Try to extract the variable */` |
|        9 |  9325 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 |  9326 | `	if( pObj ){` |
|        - |  9327 | `		/* Collision */` |
|        5 |  9328 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 |  9329 | `			return SXRET_OK;` |
|        - |  9330 | `		}` |
|        5 |  9331 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 |  9332 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - |  9333 | `				/* Already prefixed */` |
|      ! 0 |  9334 | `				return SXRET_OK;` |
|        - |  9335 | `			}` |
|      ! 0 |  9336 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9337 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9338 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9339 | `				);` |
|      ! 0 |  9340 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 |  9341 | `		}` |
|        3 |  9342 | `	}else{` |
|        - |  9343 | `		/* Create the variable */` |
|        5 |  9344 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - |  9345 | `	}` |
|        9 |  9346 | `	if( pObj ){` |
|        - |  9347 | `		/* Overwrite the old value */` |
|        9 |  9348 | `		PH7_MemObjStore(pValue,pObj);` |
|        - |  9349 | `		/* Increment counter */` |
|        9 |  9350 | `		pAux->iCount++;` |
|        4 |  9351 | `	}` |
|        9 |  9352 | `	return SXRET_OK;` |
|        5 |  9353 |  |
|        - |  9354 | `/*` |
|        - |  9355 | ` * Worker callback for the [import_request_variables()] function` |
|        - |  9356 | ` * defined below.` |
|        - |  9357 | ` */` |
|        2 |  9358 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9359 |  |
|        3 |  9360 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 |  9361 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9362 | `	ph7_value *pObj;` |
|        - |  9363 | `	SyString sVar;` |
|        - |  9364 | `	/* Perform a string cast */` |
|        3 |  9365 | `	PH7_MemObjToString(pKey);` |
|        3 |  9366 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9367 | `		/* Unavailable variable name */` |
|      ! 0 |  9368 | `		return SXRET_OK;` |
|        - |  9369 | `	}` |
|        3 |  9370 | `	sVar.nByte = 0; /* cc warning */` |
|        3 |  9371 | `	if( pAux->Prefixlen > 0 ){` |
|        4 |  9372 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 |  9373 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 |  9374 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9375 | `			);` |
|        2 |  9376 | `	}else{` |
|      ! 0 |  9377 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 |  9378 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9379 | `	}` |
|        3 |  9380 | `	sVar.zString = pAux->zWorker;` |
|        - |  9381 | `	/* Extract the variable */` |
|        3 |  9382 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 |  9383 | `	if( pObj ){` |
|        3 |  9384 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 |  9385 | `	}` |
|        3 |  9386 | `	return SXRET_OK;` |
|        2 |  9387 |  |
|        - |  9388 | `/*` |
|        - |  9389 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - |  9390 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - |  9391 | ` * Parameters` |
|        - |  9392 | ` * $types` |
|        - |  9393 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - |  9394 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - |  9395 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - |  9396 | ` *  POST includes the POST uploaded file information.` |
|        - |  9397 | ` *  Note:` |
|        - |  9398 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - |  9399 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - |  9400 | ` * $prefix` |
|        - |  9401 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - |  9402 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - |  9403 | ` *  variable named $pref_userid.` |
|        - |  9404 | ` * Return` |
|        - |  9405 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9406 | ` */` |
|        2 |  9407 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9408 |  |
|        - |  9409 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - |  9410 | `	extract_aux_data sAux;` |
|        - |  9411 | `	int nLen,nPrefixLen;` |
|        - |  9412 | `	ph7_value *pSuper;` |
|        - |  9413 | `	ph7_vm *pVm;` |
|        - |  9414 | `	/* By default import only $_GET variables  */` |
|        3 |  9415 | `	zImport = "G";` |
|        3 |  9416 | `	nLen = (int)sizeof(char);` |
|        3 |  9417 | `	zPrefix = 0;` |
|        3 |  9418 | `	nPrefixLen = 0;` |
|        3 |  9419 | `	if( nArg > 0 ){` |
|        3 |  9420 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 |  9421 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 |  9422 | `		}` |
|        3 |  9423 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9424 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 |  9425 | `		}` |
|        1 |  9426 | `	}` |
|        - |  9427 | `	/* Point to the underlying VM */` |
|        3 |  9428 | `	pVm = pCtx->pVm;` |
|        - |  9429 | `	/* Initialize the aux data */` |
|        3 |  9430 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 |  9431 | `	sAux.zPrefix = zPrefix;` |
|        3 |  9432 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 |  9433 | `	sAux.pVm = pVm;` |
|        - |  9434 | `	/* Extract */` |
|        3 |  9435 | `	zEnd = &zImport[nLen];` |
|        5 |  9436 | `	while( zImport < zEnd ){` |
|        3 |  9437 | `		int c = zImport[0];` |
|        3 |  9438 | `		pSuper = 0;` |
|        3 |  9439 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - |  9440 | `			/* Import $_GET variables */` |
|        3 |  9441 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 |  9442 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - |  9443 | `			/* Import $_POST variables */` |
|      ! 0 |  9444 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 |  9445 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - |  9446 | `			/* Import $_COOKIE variables */` |
|      ! 0 |  9447 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 |  9448 | `		}` |
|        3 |  9449 | `		if( pSuper ){` |
|        - |  9450 | `			/* Iterate throw array entries */` |
|        3 |  9451 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 |  9452 | `		}` |
|        - |  9453 | `		/* Advance the cursor */` |
|        3 |  9454 | `		zImport++;` |
|        1 |  9455 | `	}` |
|        - |  9456 | `	/* All done,return TRUE*/` |
|        3 |  9457 | `	ph7_result_bool(pCtx,0);` |
|        3 |  9458 | `	return PH7_OK;` |
|        1 |  9459 |  |
|        - |  9460 | `/*` |
|        - |  9461 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |  9462 | ` * Refer to the eval() language construct implementation for more` |
|        - |  9463 | ` * information.` |
|        - |  9464 | ` */` |
|     9820 |  9465 | `static sxi32 VmEvalChunk(` |
|        - |  9466 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |  9467 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |  9468 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |  9469 | `	int iFlags,         /* Compile flag */` |
|        - |  9470 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |  9471 | `	)` |
|        2 |  9472 |  |
|        - |  9473 | `	SySet *pByteCode,aByteCode;` |
|     9822 |  9474 | `	ProcConsumer xErr = 0;` |
|     9822 |  9475 | `	void *pErrData = 0;` |
|        - |  9476 | `	/* Initialize bytecode container */` |
|     9822 |  9477 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     9822 |  9478 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |  9479 | `	/* Reset the code generator */` |
|     9822 |  9480 | `	if( bTrueReturn ){` |
|        - |  9481 | `		/* Included file,log compile-time errors */` |
|     7535 |  9482 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7535 |  9483 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3767 |  9484 | `	}` |
|     9822 |  9485 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |  9486 | `	/* Swap bytecode container */` |
|     9822 |  9487 | `	pByteCode = pVm->pByteContainer;` |
|     9822 |  9488 | `	pVm->pByteContainer = &aByteCode;` |
|        - |  9489 | `	/* Compile the chunk */` |
|     9822 |  9490 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    14732 |  9491 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |  9492 | `		/* Compilation error,return false */` |
|        3 |  9493 | `		if( pCtx ){` |
|        3 |  9494 | `			ph7_result_bool(pCtx,0);` |
|        1 |  9495 | `		}` |
|        2 |  9496 | `	}else{` |
|        - |  9497 | `		/* Mount any newly defined classes */` |
|        - |  9498 | `		SyHashEntry *pEntry;` |
|        - |  9499 | `		ph7_class *pClass;` |
|        - |  9500 | `		ph7_value sResult; /* Return value */` |
|        - |  9501 | `		sxi32 rc;` |
|     9820 |  9502 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   272565 |  9503 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   257838 |  9504 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  9505 | `			/* Only mount classes that haven't been mounted yet */` |
|   257838 |  9506 | `			if( !pClass->bMounted ){` |
|    59356 |  9507 | `				rc = VmMountUserClass(pVm,pClass);` |
|    59356 |  9508 | `				if( rc != SXRET_OK ){` |
|        - |  9509 | `					/* Mount failure (likely memory error) */` |
|      ! 0 |  9510 | `					if( pCtx ){` |
|      ! 0 |  9511 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 |  9512 | `					}` |
|      ! 0 |  9513 | `					goto Cleanup;` |
|        - |  9514 | `				}` |
|    29677 |  9515 | `			}` |
|        2 |  9516 | `		}` |
|     9820 |  9517 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  9518 | `			/* Out of memory */` |
|      ! 0 |  9519 | `			if( pCtx ){` |
|      ! 0 |  9520 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  9521 | `			}` |
|      ! 0 |  9522 | `			goto Cleanup;` |
|        - |  9523 | `		}` |
|     9820 |  9524 | `		if( bTrueReturn ){` |
|        - |  9525 | `			/* Assume a boolean true return value */` |
|     7535 |  9526 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3768 |  9527 | `		}else{` |
|        - |  9528 | `			/* Assume a null return value */` |
|     2286 |  9529 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  9530 | `		}` |
|        - |  9531 | `		/* Execute the compiled chunk */` |
|     9820 |  9532 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|     9820 |  9533 | `		if( pCtx ){` |
|        - |  9534 | `			/* Set the execution result */` |
|     7548 |  9535 | `			ph7_result_value(pCtx,&sResult);` |
|     3773 |  9536 | `		}` |
|     9820 |  9537 | `		PH7_MemObjRelease(&sResult);` |
|        - |  9538 | `	}` |
|     4910 |  9539 | `Cleanup:` |
|        - |  9540 | `	/* Cleanup the mess left behind */` |
|     9822 |  9541 | `	pVm->pByteContainer = pByteCode;` |
|     9822 |  9542 | `	SySetRelease(&aByteCode);` |
|     9822 |  9543 | `	return SXRET_OK;` |
|        2 |  9544 |  |
|        - |  9545 | `/*` |
|        - |  9546 | ` * value eval(string $code)` |
|        - |  9547 | ` *   Evaluate a string as PHP code.` |
|        - |  9548 | ` * Parameter` |
|        - |  9549 | ` *  code: PHP code to evaluate.` |
|        - |  9550 | ` * Return` |
|        - |  9551 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  9552 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  9553 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  9554 | ` */` |
|       16 |  9555 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9556 |  |
|        - |  9557 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 |  9558 | `	if( nArg < 1 ){` |
|        - |  9559 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9560 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9561 | `		return SXRET_OK;` |
|        - |  9562 | `	}` |
|        - |  9563 | `	/* Chunk to evaluate */` |
|       18 |  9564 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 |  9565 | `	if( sChunk.nByte < 1 ){` |
|        - |  9566 | `		/* Empty string,return NULL */` |
|        3 |  9567 | `		ph7_result_null(pCtx);` |
|        3 |  9568 | `		return SXRET_OK;` |
|        - |  9569 | `	}` |
|        - |  9570 | `	/* Eval the chunk */` |
|       16 |  9571 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 |  9572 | `	return SXRET_OK;` |
|       10 |  9573 |  |
|        - |  9574 | `/*` |
|        - |  9575 | ` * Check if a file path is already included.` |
|        - |  9576 | ` */` |
|    15064 |  9577 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 |  9578 |  |
|        - |  9579 | `	SyString *aEntries;` |
|        - |  9580 | `	sxu32 n;` |
|    15065 |  9581 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  9582 | `	/* Perform a linear search */` |
| 56720651 |  9583 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 56705593 |  9584 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  9585 | `			/* Already included */` |
|        7 |  9586 | `			return TRUE;` |
|        - |  9587 | `		}` |
| 28352794 |  9588 | `	}` |
|    15059 |  9589 | `	return FALSE;` |
|     7533 |  9590 |  |
|        - |  9591 | `/*` |
|        - |  9592 | ` * Push a file path in the appropriate VM container.` |
|        - |  9593 | ` */` |
|    17328 |  9594 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 |  9595 |  |
|        - |  9596 | `	SyString sPath;` |
|        - |  9597 | `	char *zDup;` |
|        - |  9598 | `#ifdef __WINNT__` |
|        - |  9599 | `	char *zCur;` |
|        - |  9600 | `#endif` |
|        - |  9601 | `	sxi32 rc;` |
|    17330 |  9602 | `	if( nLen < 0 ){` |
|     2266 |  9603 | `		nLen = SyStrlen(zPath);` |
|     1132 |  9604 | `	}` |
|        - |  9605 | `	/* Duplicate the file path first */` |
|    17330 |  9606 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    17330 |  9607 | `	if( zDup == 0 ){` |
|      ! 0 |  9608 | `		return SXERR_MEM;` |
|        - |  9609 | `	}` |
|        - |  9610 | `#ifdef __WINNT__` |
|        - |  9611 | `	/* Normalize path on windows` |
|        - |  9612 | `	 * Example:` |
|        - |  9613 | `	 *    Path/To/File.php` |
|        - |  9614 | `	 * becomes` |
|        - |  9615 | `	 *   path\to\file.php` |
|        - |  9616 | `	 */` |
|        2 |  9617 | `	zCur = zDup;` |
|        2 |  9618 | `	while( zCur[0] != 0 ){` |
|        2 |  9619 | `		if( zCur[0] == '/' ){` |
|        2 |  9620 | `			zCur[0] = '\\';` |
|        2 |  9621 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 |  9622 | `			int c = SyToLower(zCur[0]);` |
|        1 |  9623 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - |  9624 | `		}` |
|        2 |  9625 | `		zCur++;` |
|        2 |  9626 | `	}` |
|        - |  9627 | `#endif` |
|        - |  9628 | `	/* Install the file path */` |
|    17330 |  9629 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    17330 |  9630 | `	if( !bMain ){` |
|    15065 |  9631 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  9632 | `			/* Already included */` |
|        7 |  9633 | `			*pNew = 0;` |
|        4 |  9634 | `		}else{` |
|        - |  9635 | `			/* Insert in the corresponding container */` |
|    15059 |  9636 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    15059 |  9637 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9638 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  9639 | `				return rc;` |
|        - |  9640 | `			}` |
|    15059 |  9641 | `			*pNew = 1;` |
|        - |  9642 | `		}` |
|     7532 |  9643 | `	}` |
|    17330 |  9644 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    17330 |  9645 | `	return SXRET_OK;` |
|     8666 |  9646 |  |
|        - |  9647 | `/*` |
|        - |  9648 | ` * Compile and Execute a PHP script at run-time.` |
|        - |  9649 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - |  9650 | ` * indicates failure.` |
|        - |  9651 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - |  9652 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - |  9653 | ` * operations.` |
|        - |  9654 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - |  9655 | ` * this function is a no-op.` |
|        - |  9656 | ` * Refer to the implementation of the include(),include_once() language` |
|        - |  9657 | ` * constructs for more information.` |
|        - |  9658 | ` */` |
|     7540 |  9659 | `static sxi32 VmExecIncludedFile(` |
|        - |  9660 | `	 ph7_context *pCtx, /* Call Context */` |
|        - |  9661 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - |  9662 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - |  9663 | `	 )` |
|        2 |  9664 |  |
|        - |  9665 | `	sxi32 rc;` |
|        - |  9666 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9667 | `	const ph7_io_stream *pStream;` |
|        - |  9668 | `	SyBlob sContents;` |
|        - |  9669 | `	void *pHandle;` |
|        - |  9670 | `	ph7_vm *pVm;` |
|        - |  9671 | `	int isNew;` |
|        - |  9672 | `	/* Initialize fields */` |
|     7542 |  9673 | `	pVm = pCtx->pVm;` |
|     7542 |  9674 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7542 |  9675 | `	isNew = 0;` |
|        - |  9676 | `	/* Extract the associated stream */` |
|     7542 |  9677 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - |  9678 | `	/*` |
|        - |  9679 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - |  9680 | `	 * in a read-only mode.` |
|        - |  9681 | `	 */` |
|     7542 |  9682 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7542 |  9683 | `	if( pHandle == 0 ){` |
|        3 |  9684 | `		return SXERR_IO;` |
|        - |  9685 | `	}` |
|     7539 |  9686 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7539 |  9687 | `	if( IncludeOnce && !isNew ){` |
|        - |  9688 | `		/* Already included */` |
|        5 |  9689 | `		rc = SXERR_EXISTS;` |
|        3 |  9690 | `	}else{` |
|        - |  9691 | `		/* Read the whole file contents */` |
|     7535 |  9692 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7535 |  9693 | `		if( rc == SXRET_OK ){` |
|        - |  9694 | `			SyString sScript;` |
|        - |  9695 | `			/* Compile and execute the script */` |
|     7535 |  9696 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7535 |  9697 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3767 |  9698 | `		}` |
|        - |  9699 | `	}` |
|        - |  9700 | `	/* Pop from the set of included file */` |
|     7539 |  9701 | `	(void)SySetPop(&pVm->aFiles);` |
|        - |  9702 | `	/* Close the handle */` |
|     7539 |  9703 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - |  9704 | `	/* Release the working buffer */` |
|     7539 |  9705 | `	SyBlobRelease(&sContents);` |
|        - |  9706 | `#else` |
|        - |  9707 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - |  9708 | `	SXUNUSED(pPath);` |
|        - |  9709 | `	SXUNUSED(IncludeOnce);` |
|        - |  9710 | `	rc = SXERR_IO;` |
|        - |  9711 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7539 |  9712 | `	return rc;` |
|     3772 |  9713 |  |
|        - |  9714 | `/*` |
|        - |  9715 | ` * string get_include_path(void)` |
|        - |  9716 | ` *  Gets the current include_path configuration option.` |
|        - |  9717 | ` * Parameter` |
|        - |  9718 | ` *  None` |
|        - |  9719 | ` * Return` |
|        - |  9720 | ` *  Included paths as a string` |
|        - |  9721 | ` */` |
|        2 |  9722 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9723 |  |
|        3 |  9724 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9725 | `	SyString *aEntry;` |
|        - |  9726 | `	int dir_sep;` |
|        - |  9727 | `	sxu32 n;` |
|        - |  9728 | `#ifdef __WINNT__` |
|        1 |  9729 | `	dir_sep = ';';` |
|        - |  9730 | `#else` |
|        - |  9731 | `	/* Assume UNIX path separator */` |
|        2 |  9732 | `	dir_sep = ':';` |
|        - |  9733 | `#endif` |
|        1 |  9734 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9735 | `	SXUNUSED(apArg);` |
|        - |  9736 | `	/* Point to the list of import paths */` |
|        3 |  9737 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 |  9738 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 |  9739 | `		SyString *pEntry = &aEntry[n];` |
|        3 |  9740 | `		if( n > 0 ){` |
|        - |  9741 | `			/* Append dir seprator */` |
|      ! 0 |  9742 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 |  9743 | `		}` |
|        - |  9744 | `		/* Append path */` |
|        3 |  9745 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 |  9746 | `	}` |
|        3 |  9747 | `	return PH7_OK;` |
|        1 |  9748 |  |
|        - |  9749 | `/*` |
|        - |  9750 | ` * string get_get_included_files(void)` |
|        - |  9751 | ` *  Gets the current include_path configuration option.` |
|        - |  9752 | ` * Parameter` |
|        - |  9753 | ` *  None` |
|        - |  9754 | ` * Return` |
|        - |  9755 | ` *  Included paths as a string` |
|        - |  9756 | ` */` |
|        2 |  9757 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9758 |  |
|        3 |  9759 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - |  9760 | `	ph7_value *pArray,*pWorker;` |
|        - |  9761 | `	SyString *pEntry;` |
|        - |  9762 | `	int c,d;` |
|        - |  9763 | `	/* Create an array and a working value */` |
|        3 |  9764 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 |  9765 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 |  9766 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - |  9767 | `		/* Out of memory,return null */` |
|      ! 0 |  9768 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9769 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9770 | `		SXUNUSED(apArg);` |
|      ! 0 |  9771 | `		return PH7_OK;` |
|        - |  9772 | `	}` |
|        3 |  9773 | `	c = d = '/';` |
|        - |  9774 | `#ifdef __WINNT__` |
|        1 |  9775 | `	d = '\\';` |
|        - |  9776 | `#endif` |
|        - |  9777 | `	/* Iterate throw entries */` |
|        3 |  9778 | `	SySetResetCursor(pFiles);` |
|     3691 |  9779 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - |  9780 | `		const char *zBase,*zEnd;` |
|        - |  9781 | `		int iLen;` |
|        - |  9782 | `		/* reset the string cursor */` |
|     3689 |  9783 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - |  9784 | `		/* Extract base name */` |
|     3689 |  9785 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - |  9786 | `		/* Ignore trailing '/' */` |
|     5533 |  9787 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 |  9788 | `			zEnd--;` |
|      ! 0 |  9789 | `		}` |
|     3689 |  9790 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   113825 |  9791 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   108293 |  9792 | `			zEnd--;` |
|        1 |  9793 | `		}` |
|     3689 |  9794 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3689 |  9795 | `		zEnd = &pEntry->zString[iLen];` |
|        - |  9796 | `		/* Copy entry name */` |
|     3689 |  9797 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - |  9798 | `		/* Perform the insertion */` |
|     3689 |  9799 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 |  9800 | `	}` |
|        - |  9801 | `	/* All done,return the created array */` |
|        3 |  9802 | `	ph7_result_value(pCtx,pArray);` |
|        - |  9803 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - |  9804 | `	 * by the engine as soon we return from this foreign` |
|        - |  9805 | `	 * function.` |
|        - |  9806 | `	 */` |
|        3 |  9807 | `	return PH7_OK;` |
|        2 |  9808 |  |
|        - |  9809 | `/*` |
|        - |  9810 | ` * include:` |
|        - |  9811 | ` * According to the PHP reference manual.` |
|        - |  9812 | ` *  The include() function includes and evaluates the specified file.` |
|        - |  9813 | ` *  Files are included based on the file path given or, if none is given` |
|        - |  9814 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - |  9815 | ` *  include() will finally check in the calling script's own directory` |
|        - |  9816 | ` *  and the current working directory before failing. The include()` |
|        - |  9817 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - |  9818 | ` *  behavior from require(), which will emit a fatal error.` |
|        - |  9819 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - |  9820 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - |  9821 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - |  9822 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - |  9823 | ` *  directory to find the requested file.` |
|        - |  9824 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - |  9825 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - |  9826 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - |  9827 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - |  9828 | ` */` |
|     7528 |  9829 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9830 |  |
|        - |  9831 | `	SyString sFile;` |
|        - |  9832 | `	sxi32 rc;` |
|     7530 |  9833 | `	if( nArg < 1 ){` |
|        - |  9834 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9835 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9836 | `		return SXRET_OK;` |
|        - |  9837 | `	}` |
|        - |  9838 | `	/* File to include */` |
|     7530 |  9839 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7530 |  9840 | `	if( sFile.nByte < 1 ){` |
|        - |  9841 | `		/* Empty string,return NULL */` |
|      ! 0 |  9842 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9843 | `		return SXRET_OK;` |
|        - |  9844 | `	}` |
|        - |  9845 | `	/* Open,compile and execute the desired script */` |
|     7530 |  9846 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7530 |  9847 | `	if( rc != SXRET_OK ){` |
|        - |  9848 | `		/* Emit a warning and return false */` |
|        3 |  9849 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 |  9850 | `		ph7_result_bool(pCtx,0);` |
|        1 |  9851 | `	}` |
|     7530 |  9852 | `	return SXRET_OK;` |
|     3766 |  9853 |  |
|        - |  9854 | `/*` |
|        - |  9855 | ` * include_once:` |
|        - |  9856 | ` *  According to the PHP reference manual.` |
|        - |  9857 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - |  9858 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - |  9859 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - |  9860 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - |  9861 | ` *   just once.` |
|        - |  9862 | ` */` |
|        4 |  9863 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
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
|        - |  9887 | `		/* Emit a warning and return false */` |
|      ! 0 |  9888 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 |  9889 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9890 | ` 	}` |
|        3 |  9891 | `	return SXRET_OK;` |
|        3 |  9892 |  |
|        - |  9893 | `/*` |
|        - |  9894 | ` * require.` |
|        - |  9895 | ` *  According to the PHP reference manual.` |
|        - |  9896 | ` *   require() is identical to include() except upon failure it will` |
|        - |  9897 | ` *   also produce a fatal level error.` |
|        - |  9898 | ` *   In other words, it will halt the script whereas include() only` |
|        - |  9899 | ` *   emits a warning  which allows the script to continue.` |
|        - |  9900 | ` */` |
|        4 |  9901 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9902 |  |
|        - |  9903 | `	SyString sFile;` |
|        - |  9904 | `	sxi32 rc;` |
|        5 |  9905 | `	if( nArg < 1 ){` |
|        - |  9906 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9907 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9908 | `		return SXRET_OK;` |
|        - |  9909 | `	}` |
|        - |  9910 | `	/* File to include */` |
|        5 |  9911 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 |  9912 | `	if( sFile.nByte < 1 ){` |
|        - |  9913 | `		/* Empty string,return NULL */` |
|      ! 0 |  9914 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9915 | `		return SXRET_OK;` |
|        - |  9916 | `	}` |
|        - |  9917 | `	/* Open,compile and execute the desired script */` |
|        5 |  9918 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 |  9919 | `	if( rc != SXRET_OK ){` |
|        - |  9920 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  9921 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  9922 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9923 | `		return PH7_ABORT;` |
|        - |  9924 | `	}` |
|        5 |  9925 | `	return SXRET_OK;` |
|        3 |  9926 |  |
|        - |  9927 | `/*` |
|        - |  9928 | ` * require_once:` |
|        - |  9929 | ` *  According to the PHP reference manual.` |
|        - |  9930 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - |  9931 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - |  9932 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - |  9933 | ` *   and how it differs from its non _once siblings.` |
|        - |  9934 | ` */` |
|        4 |  9935 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9936 |  |
|        - |  9937 | `	SyString sFile;` |
|        - |  9938 | `	sxi32 rc;` |
|        5 |  9939 | `	if( nArg < 1 ){` |
|        - |  9940 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9941 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9942 | `		return SXRET_OK;` |
|        - |  9943 | `	}` |
|        - |  9944 | `	/* File to include */` |
|        5 |  9945 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 |  9946 | `	if( sFile.nByte < 1 ){` |
|        - |  9947 | `		/* Empty string,return NULL */` |
|      ! 0 |  9948 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9949 | `		return SXRET_OK;` |
|        - |  9950 | `	}` |
|        - |  9951 | `	/* Open,compile and execute the desired script */` |
|        5 |  9952 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 |  9953 | `	if( rc == SXERR_EXISTS ){` |
|        - |  9954 | `		/* File already included,return TRUE */` |
|        3 |  9955 | `		ph7_result_bool(pCtx,1);` |
|        3 |  9956 | `		return SXRET_OK;` |
|        - |  9957 | `	}` |
|        3 |  9958 | `	if( rc != SXRET_OK ){` |
|        - |  9959 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  9960 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  9961 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9962 | `		return PH7_ABORT;` |
|        - |  9963 | `	}` |
|        3 |  9964 | `	return SXRET_OK;` |
|        3 |  9965 |  |
|        - |  9966 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - |  9967 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - |  9968 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - |  9969 | `/* Table of built-in VM functions. */` |
|        - |  9970 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - |  9971 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - |  9972 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - |  9973 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - |  9974 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - |  9975 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - |  9976 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - |  9977 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - |  9978 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - |  9979 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - |  9980 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - |  9981 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - |  9982 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - |  9983 | `	    /* Constants management */` |
|        - |  9984 | `	{ "defined",  vm_builtin_defined              },` |
|        - |  9985 | `	{ "define",   vm_builtin_define               },` |
|        - |  9986 | `	{ "constant", vm_builtin_constant             },` |
|        - |  9987 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - |  9988 | `	   /* Class/Object functions */` |
|        - |  9989 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - |  9990 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - |  9991 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - |  9992 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - |  9993 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - |  9994 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - |  9995 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - |  9996 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - |  9997 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - |  9998 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - |  9999 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 10000 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 10001 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 10002 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 10003 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 10004 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 10005 | `	   /* Random numbers/strings generators */` |
|        - | 10006 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 10007 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 10008 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 10009 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 10010 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 10011 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10012 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 10013 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 10014 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 10015 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10016 | `	   /* Language constructs functions */` |
|        - | 10017 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 10018 | `	{ "print", vm_builtin_print                   },` |
|        - | 10019 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 10020 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 10021 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 10022 | `	  /* Variable handling functions */` |
|        - | 10023 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 10024 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 10025 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 10026 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 10027 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 10028 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 10029 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 10030 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 10031 | `	  /* Ouput control functions */` |
|        - | 10032 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 10033 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 10034 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 10035 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 10036 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 10037 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 10038 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 10039 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 10040 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 10041 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 10042 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 10043 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 10044 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 10045 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 10046 | `	  /* Assertion functions */` |
|        - | 10047 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 10048 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 10049 | `	  /* Error reporting functions */` |
|        - | 10050 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 10051 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 10052 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 10053 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 10054 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 10055 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 10056 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 10057 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 10058 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 10059 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 10060 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 10061 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 10062 | `	  /* Release info */` |
|        - | 10063 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 10064 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 10065 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 10066 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 10067 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 10068 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 10069 | `	  /* hashmap */` |
|        - | 10070 | `	{"compact",          vm_builtin_compact       },` |
|        - | 10071 | `	{"extract",          vm_builtin_extract       },` |
|        - | 10072 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 10073 | `	  /* URL related function */` |
|        - | 10074 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 10075 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 10076 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10077 | `	   /* XML processing functions */` |
|        - | 10078 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 10079 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 10080 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 10081 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 10082 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 10083 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 10084 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 10085 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 10086 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 10087 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 10088 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 10089 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 10090 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 10091 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 10092 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 10093 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 10094 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 10095 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 10096 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 10097 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 10098 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 10099 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10100 | `	   /* UTF-8 encoding/decoding */` |
|        - | 10101 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 10102 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 10103 | `	   /* Command line processing */` |
|        - | 10104 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 10105 | `	   /* JSON encoding/decoding */` |
|        - | 10106 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 10107 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 10108 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 10109 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 10110 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 10111 | `	   /* Files/URI inclusion facility */` |
|        - | 10112 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 10113 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 10114 | `	{ "include",      vm_builtin_include          },` |
|        - | 10115 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 10116 | `	{ "require",      vm_builtin_require          },` |
|        - | 10117 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 10118 | `};` |
|        - | 10119 | `/*` |
|        - | 10120 | ` * Register the built-in VM functions defined above.` |
|        - | 10121 | ` */` |
|     2034 | 10122 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 10123 |  |
|        - | 10124 | `	sxi32 rc;` |
|        - | 10125 | `	sxu32 n;` |
|   254252 | 10126 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 10127 | `		/* Note that these special functions have access` |
|        - | 10128 | `		 * to the underlying virtual machine as their` |
|        - | 10129 | `		 * private data.` |
|        - | 10130 | `		 */` |
|   252218 | 10131 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   252218 | 10132 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 10133 | `			return rc;` |
|        - | 10134 | `		}` |
|   126110 | 10135 | `	}` |
|     2036 | 10136 | `	return SXRET_OK;` |
|     1019 | 10137 |  |
|        - | 10138 | `/*` |
|        - | 10139 | ` * Check if the given name refer to an installed class.` |
|        - | 10140 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 10141 | ` */` |
|    14816 | 10142 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 10143 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 10144 | `	const char *zName,  /* Name of the target class */` |
|        - | 10145 | `	sxu32 nByte,        /* zName length */` |
|        - | 10146 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 10147 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 10148 | `						 */` |
|        - | 10149 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 10150 | `	)` |
|        2 | 10151 |  |
|        - | 10152 | `	SyHashEntry *pEntry;` |
|        - | 10153 | `	ph7_class *pClass;` |
|     7408 | 10154 | `		SXUNUSED(iNest);` |
|        - | 10155 | `	/* Perform a hash lookup */` |
|    14818 | 10156 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|        - | 10157 |  |
|    14818 | 10158 | `	if( pEntry == 0 ){` |
|        - | 10159 | `		/* No such entry,return NULL */` |
|      ! 0 | 10160 | `		return 0;` |
|        - | 10161 | `	}` |
|    14818 | 10162 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    14818 | 10163 | `	if( !iLoadable ){` |
|        - | 10164 | `		/* Return the first class seen */` |
|    13796 | 10165 | `		return pClass;` |
|      ! 0 | 10166 | `	}else{` |
|        - | 10167 | `		/* Check the collision list */` |
|     1024 | 10168 | `		while(pClass){` |
|     1024 | 10169 | `			if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) == 0 ){` |
|        - | 10170 | `				/* Class is loadable */` |
|     1024 | 10171 | `				return pClass;` |
|        - | 10172 | `			}` |
|        - | 10173 | `			/* Point to the next entry */` |
|      ! 0 | 10174 | `			pClass = pClass->pNextName;` |
|      ! 0 | 10175 | `		}` |
|        - | 10176 | `	}` |
|        - | 10177 | `	/* No such loadable class */` |
|      ! 0 | 10178 | `	return 0;` |
|     7410 | 10179 |  |
|        - | 10180 | `/*` |
|        - | 10181 | ` * Reference Table Implementation` |
|        - | 10182 | ` * Status: stable <chm@symisc.net>` |
|        - | 10183 | ` * Intro` |
|        - | 10184 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 10185 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 10186 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 10187 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 10188 | ` *  Refer to the official for more information on this powerful` |
|        - | 10189 | ` *  extension.` |
|        - | 10190 | ` */` |
|        - | 10191 | `/*` |
|        - | 10192 | ` * Allocate a new reference entry.` |
|        - | 10193 | ` */` |
|  2981950 | 10194 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 10195 |  |
|        - | 10196 | `	VmRefObj *pRef;` |
|        - | 10197 | `	/* Allocate a new instance */` |
|  2981952 | 10198 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  2981952 | 10199 | `	if( pRef == 0 ){` |
|      ! 0 | 10200 | `		return 0;` |
|        - | 10201 | `	}` |
|        - | 10202 | `	/* Zero the structure */` |
|  2981952 | 10203 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 10204 | `	/* Initialize fields */` |
|  2981952 | 10205 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  2981952 | 10206 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  2981952 | 10207 | `	pRef->nIdx = nIdx;` |
|  2981952 | 10208 | `	return pRef;` |
|  1490977 | 10209 |  |
|        - | 10210 | `/*` |
|        - | 10211 | ` * Default hash function used by the reference table` |
|        - | 10212 | ` * for lookup/insertion operations.` |
|        - | 10213 | ` */` |
| 16556136 | 10214 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 10215 |  |
|        - | 10216 | `	/* Calculate the hash based on the memory object index */` |
| 16556138 | 10217 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 10218 |  |
|        - | 10219 | `/*` |
|        - | 10220 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 10221 | ` * in the reference table.` |
|        - | 10222 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 10223 | ` * otherwise.` |
|        - | 10224 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10225 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10226 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10227 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10228 | ` * Refer to the official for more information on this powerful` |
|        - | 10229 | ` * extension.` |
|        - | 10230 | ` */` |
|  8905118 | 10231 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 10232 |  |
|        - | 10233 | `	VmRefObj *pRef;` |
|        - | 10234 | `	sxu32 nBucket;` |
|        - | 10235 | `	/* Point to the appropriate bucket */` |
|  8905120 | 10236 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 10237 | `	/* Perform the lookup */` |
|  8905120 | 10238 | `	pRef = pVm->apRefObj[nBucket];` |
| 18793040 | 10239 | `	for(;;){` |
| 37586102 | 10240 | `		if( pRef == 0 ){` |
|  3054736 | 10241 | `			break;` |
|        - | 10242 | `		}` |
| 34531368 | 10243 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 10244 | `			/* Entry found */` |
|  5850386 | 10245 | `			return pRef;` |
|        - | 10246 | `		}` |
|        - | 10247 | `		/* Point to the next entry */` |
| 28680984 | 10248 | `		pRef = pRef->pNextCollide;` |
|        2 | 10249 | `	}` |
|        - | 10250 | `	/* No such entry,return NULL */` |
|  3054736 | 10251 | `	return 0;` |
|  4452561 | 10252 |  |
|        - | 10253 | `/*` |
|        - | 10254 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10255 | ` *` |
|        - | 10256 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10257 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10258 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10259 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10260 | ` * Refer to the official for more information on this powerful` |
|        - | 10261 | ` * extension.` |
|        - | 10262 | ` */` |
|  2981950 | 10263 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10264 |  |
|        - | 10265 | `	sxu32 nBucket;` |
|  2981952 | 10266 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 10267 | `		VmRefObj **apNew;` |
|        - | 10268 | `		sxu32 nNew;` |
|        - | 10269 | `		/* Allocate a larger table */` |
|     3202 | 10270 | `		nNew = pVm->nRefSize << 1;` |
|     3202 | 10271 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     3202 | 10272 | `		if( apNew ){` |
|     3202 | 10273 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 10274 | `			sxu32 n;` |
|        - | 10275 | `			/* Zero the structure */` |
|     3202 | 10276 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 10277 | `			/* Rehash all referenced entries */` |
|  2832326 | 10278 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 10279 | `				/* Remove old collision links */` |
|  2829126 | 10280 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 10281 | `				/* Point to the appropriate bucket */` |
|  2829126 | 10282 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 10283 | `				/* Insert the entry  */` |
|  2829126 | 10284 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2829126 | 10285 | `				if( apNew[nBucket] ){` |
|  2298896 | 10286 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 10287 | `				}` |
|  2829126 | 10288 | `				apNew[nBucket] = pEntry;` |
|        - | 10289 | `				/* Point to the next entry */` |
|  2829126 | 10290 | `				pEntry = pEntry->pNext;` |
|  1414564 | 10291 | `			}` |
|        - | 10292 | `			/* Release the old table */` |
|     3202 | 10293 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 10294 | `			/* Install the new one */` |
|     3202 | 10295 | `			pVm->apRefObj = apNew;` |
|     3202 | 10296 | `			pVm->nRefSize = nNew;` |
|     1600 | 10297 | `		}` |
|     1600 | 10298 | `	}` |
|        - | 10299 | `	/* Point to the appropriate bucket */` |
|  2981952 | 10300 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 10301 | `	/* Insert the entry */` |
|  2981952 | 10302 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  2981952 | 10303 | `	if( pVm->apRefObj[nBucket] ){` |
|  2471792 | 10304 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1235640 | 10305 | `	}` |
|  2981952 | 10306 | `	pVm->apRefObj[nBucket] = pRef;` |
|  2981952 | 10307 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  2981952 | 10308 | `	pVm->nRefUsed++;` |
|  2981952 | 10309 | `	return SXRET_OK;` |
|        2 | 10310 |  |
|        - | 10311 | `/*` |
|        - | 10312 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 10313 | ` * the reference table.` |
|        - | 10314 | ` * This function is invoked when the user perform an unset` |
|        - | 10315 | ` * call [i.e: unset($var); ].` |
|        - | 10316 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10317 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10318 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10319 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10320 | ` * Refer to the official for more information on this powerful` |
|        - | 10321 | ` * extension.` |
|        - | 10322 | ` */` |
|  2952850 | 10323 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10324 |  |
|        - | 10325 | `	ph7_hashmap_node **apNode;` |
|        - | 10326 | `	SyHashEntry **apEntry;` |
|        - | 10327 | `	sxu32 n;` |
|        - | 10328 | `	/* Point to the reference table */` |
|  2952852 | 10329 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2952852 | 10330 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 10331 | `	/* Unlink the entry from the reference table */` |
|  3030598 | 10332 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    77748 | 10333 | `		if( apEntry[n] ){` |
|    77698 | 10334 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    38848 | 10335 | `		}` |
|    38875 | 10336 | `	}` |
|  5829856 | 10337 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2877006 | 10338 | `		if( apNode[n] ){` |
|     5635 | 10339 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2817 | 10340 | `		}` |
|  1438504 | 10341 | `	}` |
|  2952852 | 10342 | `	if( pRef->pPrevCollide ){` |
|  1112908 | 10343 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   556666 | 10344 | `	}else{` |
|  1839946 | 10345 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 10346 | `	}` |
|  2952852 | 10347 | `	if( pRef->pNextCollide ){` |
|  1660157 | 10348 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   829881 | 10349 | `	}` |
|  2952852 | 10350 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 10351 | `	/* Release the node */` |
|  2952852 | 10352 | `	SySetRelease(&pRef->aReference);` |
|  2952852 | 10353 | `	SySetRelease(&pRef->aArrEntries);` |
|  2952852 | 10354 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2952852 | 10355 | `	pVm->nRefUsed--;` |
|  2952852 | 10356 | `	return SXRET_OK;` |
|        2 | 10357 |  |
|        - | 10358 | `/*` |
|        - | 10359 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10360 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10361 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10362 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10363 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10364 | ` * Refer to the official for more information on this powerful` |
|        - | 10365 | ` * extension.` |
|        - | 10366 | ` */` |
|  3008054 | 10367 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 10368 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10369 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10370 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10371 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 10372 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 10373 | `	)` |
|        2 | 10374 |  |
|  3008056 | 10375 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10376 | `	VmRefObj *pRef;` |
|        - | 10377 | `	/* Check if the referenced object already exists */` |
|  3008056 | 10378 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3008056 | 10379 | `	if( pRef == 0 ){` |
|        - | 10380 | `		/* Create a new entry */` |
|  2981952 | 10381 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  2981952 | 10382 | `		if( pRef == 0 ){` |
|      ! 0 | 10383 | `			return SXERR_MEM;` |
|        - | 10384 | `		}` |
|  2981952 | 10385 | `		pRef->iFlags = iFlags;` |
|        - | 10386 | `		/* Install the entry */` |
|  2981952 | 10387 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1490975 | 10388 | `	}` |
|  3008132 | 10389 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 10390 | `		/* Safely ignore the exception frame */` |
|       78 | 10391 | `		pFrame = pFrame->pParent;` |
|        2 | 10392 | `	}` |
|  3008056 | 10393 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 10394 | `		VmSlot sRef;` |
|        - | 10395 | `		/* Local frame,record referenced entry so that it can` |
|        - | 10396 | `		 * be deleted when we leave this frame.` |
|        - | 10397 | `		 */` |
|    72820 | 10398 | `		sRef.nIdx = nIdx;` |
|    72820 | 10399 | `		sRef.pUserData = pEntry;` |
|    72820 | 10400 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 10401 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 10402 | `		}` |
|    36409 | 10403 | `	}` |
|  3008056 | 10404 | `	if( pEntry ){` |
|        - | 10405 | `		/* Address of the hash-entry */` |
|    98734 | 10406 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    49366 | 10407 | `	}` |
|  3008056 | 10408 | `	if( pMapEntry ){` |
|        - | 10409 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2904560 | 10410 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1452279 | 10411 | `	}` |
|  3008056 | 10412 | `	return SXRET_OK;` |
|  1504029 | 10413 |  |
|        - | 10414 | `/*` |
|        - | 10415 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 10416 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10417 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10418 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10419 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10420 | ` * Refer to the official for more information on this powerful` |
|        - | 10421 | ` * extension.` |
|        - | 10422 | ` */` |
|  2944194 | 10423 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 10424 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10425 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10426 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10427 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 10428 | `	)` |
|        2 | 10429 |  |
|        - | 10430 | `	VmRefObj *pRef;` |
|        - | 10431 | `	sxu32 n;` |
|        - | 10432 | `	/* Check if the referenced object already exists */` |
|  2944196 | 10433 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2944196 | 10434 | `	if( pRef == 0 ){` |
|        - | 10435 | `		/* Not such entry */` |
|    72766 | 10436 | `		return SXERR_NOTFOUND;` |
|        - | 10437 | `	}` |
|        - | 10438 | `	/* Remove the desired entry */` |
|  2871432 | 10439 | `	if( pEntry ){` |
|        - | 10440 | `		SyHashEntry **apEntry;` |
|       56 | 10441 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 10442 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 10443 | `			if( apEntry[n] == pEntry ){` |
|        - | 10444 | `				/* Nullify the entry */` |
|       56 | 10445 | `				apEntry[n] = 0;` |
|        - | 10446 | `				/*` |
|        - | 10447 | `				 * NOTE:` |
|        - | 10448 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 10449 | `				 * we avoid wasting spaces.` |
|        - | 10450 | `				 */` |
|       27 | 10451 | `			}` |
|       79 | 10452 | `		}` |
|       27 | 10453 | `	}` |
|  2871432 | 10454 | `	if( pMapEntry ){` |
|        - | 10455 | `		ph7_hashmap_node **apNode;` |
|  2871378 | 10456 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5742842 | 10457 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2871466 | 10458 | `			if( apNode[n] == pMapEntry ){` |
|        - | 10459 | `				/* nullify the entry */` |
|  2871378 | 10460 | `				apNode[n] = 0;` |
|  1435688 | 10461 | `			}` |
|  1435734 | 10462 | `		}` |
|  1435688 | 10463 | `	}` |
|  2871432 | 10464 | `	return SXRET_OK;` |
|  1472099 | 10465 |  |
|        - | 10466 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 10467 | `/*` |
|        - | 10468 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 10469 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 10470 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 10471 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 10472 | ` * For more information on how to register IO stream devices,please` |
|        - | 10473 | ` * refer to the official documentation.` |
|        - | 10474 | ` */` |
|    22664 | 10475 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 10476 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 10477 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 10478 | `	int nByte              /* *pzDevice length*/` |
|        - | 10479 | `	)` |
|        2 | 10480 |  |
|        - | 10481 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 10482 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 10483 | `	SyString sDev,sCur;` |
|        - | 10484 | `	sxu32 n,nEntry;` |
|        - | 10485 | `	int rc;` |
|        - | 10486 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    22666 | 10487 | `	zNext = zCur = zIn = *pzDevice;` |
|    22666 | 10488 | `	zEnd = &zIn[nByte];` |
|  1450899 | 10489 | `	while( zIn < zEnd ){` |
|  1428237 | 10490 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 10491 | `			/* Got one */` |
|        3 | 10492 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 10493 | `			break;` |
|        - | 10494 | `		}` |
|        - | 10495 | `		/* Advance the cursor */` |
|  1428235 | 10496 | `		zIn++;` |
|        2 | 10497 | `	}` |
|    22666 | 10498 | `	if( zIn >= zEnd ){` |
|        - | 10499 | `		/* No such scheme,return the default stream */` |
|    22664 | 10500 | `		return pVm->pDefStream;` |
|        - | 10501 | `	}` |
|        3 | 10502 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 10503 | `	/* Remove leading and trailing white spaces */` |
|        3 | 10504 | `	SyStringFullTrim(&sDev);` |
|        - | 10505 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 10506 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 10507 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 10508 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 10509 | `		pStream = apStream[n];` |
|        3 | 10510 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 10511 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 10512 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 10513 | `		if( rc == 0 ){` |
|        - | 10514 | `			/* Stream device found */` |
|        3 | 10515 | `			*pzDevice = zNext;` |
|        3 | 10516 | `			return pStream;` |
|        - | 10517 | `		}` |
|      ! 0 | 10518 | `	}` |
|        - | 10519 | `	/* No such stream,return NULL */` |
|      ! 0 | 10520 | `	return 0;` |
|    11334 | 10521 |  |
|        - | 10522 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 10523 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 10524 |  |
