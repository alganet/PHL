# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3999/5256 lines (76.08%)

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
|   754350 |    86 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    87 |  |
|   754352 |    88 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       32 |    89 | `		return TRUE;` |
|        - |    90 | `	}` |
|   754322 |    91 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |    92 | `		return TRUE;` |
|        - |    93 | `	}` |
|   754314 |    94 | `	return FALSE;` |
|   377199 |    95 |  |
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
|   442152 |   110 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   442154 |   121 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   442154 |   122 | `	if( pEntry ){` |
|        - |   123 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   124 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   125 | `		pCons->xExpand = xExpand;` |
|        6 |   126 | `		pCons->pUserData = pUserData;` |
|        6 |   127 | `		return SXRET_OK;` |
|        - |   128 | `	}` |
|        - |   129 | `	/* Allocate a new constant instance */` |
|   442150 |   130 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   442150 |   131 | `	if( pCons == 0 ){` |
|      ! 0 |   132 | `		return 0;` |
|        - |   133 | `	}` |
|        - |   134 | `	/* Duplicate constant name */` |
|   442150 |   135 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   442150 |   136 | `	if( zDupName == 0 ){` |
|      ! 0 |   137 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   138 | `		return 0;` |
|        - |   139 | `	}` |
|        - |   140 | `	/* Install the constant */` |
|   442150 |   141 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   442150 |   142 | `	pCons->xExpand = xExpand;` |
|   442150 |   143 | `	pCons->pUserData = pUserData;` |
|   442150 |   144 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   442150 |   145 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   146 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return rc;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* All done,constant can be invoked from PHP code */` |
|   442150 |   151 | `	return SXRET_OK;` |
|   221078 |   152 |  |
|        - |   153 | `/*` |
|        - |   154 | ` * Allocate a new foreign function instance.` |
|        - |   155 | ` * This function return SXRET_OK on success. Any other` |
|        - |   156 | ` * return value indicates failure.` |
|        - |   157 | ` * Please refer to the official documentation for an introduction to` |
|        - |   158 | ` * the foreign function mechanism.` |
|        - |   159 | ` */` |
|   947430 |   160 | `static sxi32 PH7_NewForeignFunction(` |
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
|   947432 |   171 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   947432 |   172 | `	if( pFunc == 0 ){` |
|      ! 0 |   173 | `		return SXERR_MEM;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* Duplicate function name */` |
|   947432 |   176 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   947432 |   177 | `	if( zDup == 0 ){` |
|      ! 0 |   178 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   179 | `		return SXERR_MEM;` |
|        - |   180 | `	}` |
|        - |   181 | `	/* Zero the structure */` |
|   947432 |   182 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   183 | `	/* Initialize structure fields */` |
|   947432 |   184 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   947432 |   185 | `	pFunc->pVm   = pVm;` |
|   947432 |   186 | `	pFunc->xFunc = xFunc;` |
|   947432 |   187 | `	pFunc->pUserData = pUserData;` |
|   947432 |   188 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   189 | `	/* Write a pointer to the new function */` |
|   947432 |   190 | `	*ppOut = pFunc;` |
|   947432 |   191 | `	return SXRET_OK;` |
|   473717 |   192 |  |
|        - |   193 | `/*` |
|        - |   194 | ` * Install a foreign function and it's associated callback so that` |
|        - |   195 | ` * it can be invoked from the target PHP code.` |
|        - |   196 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   197 | ` * return value indicates failure.` |
|        - |   198 | ` * Please refer to the official documentation for an introduction to` |
|        - |   199 | ` * the foreign function mechanism.` |
|        - |   200 | ` */` |
|   949608 |   201 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|   949610 |   212 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   949610 |   213 | `	if( pEntry ){` |
|     2180 |   214 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2180 |   215 | `		pFunc->pUserData = pUserData;` |
|     2180 |   216 | `		pFunc->xFunc = xFunc;` |
|     2180 |   217 | `		SySetReset(&pFunc->aAux);` |
|     2180 |   218 | `		return SXRET_OK;` |
|        - |   219 | `	}` |
|        - |   220 | `	/* Create a new user function */` |
|   947432 |   221 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   947432 |   222 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   223 | `		return rc;` |
|        - |   224 | `	}` |
|        - |   225 | `	/* Install the function in the corresponding hashtable */` |
|   947432 |   226 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   947432 |   227 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   228 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   229 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   230 | `		return rc;` |
|        - |   231 | `	}` |
|        - |   232 | `	/* User function successfully installed */` |
|   947432 |   233 | `	return SXRET_OK;` |
|   474806 |   234 |  |
|        - |   235 | `/*` |
|        - |   236 | ` * Initialize a VM function.` |
|        - |   237 | ` */` |
|   102128 |   238 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   239 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   240 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   241 | `	const char *zName,  /* Function name */` |
|        - |   242 | `	sxu32 nByte,        /* zName length */` |
|        - |   243 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   244 | `	void *pUserData     /* Function private data */` |
|        - |   245 | `	)` |
|        2 |   246 |  |
|        - |   247 | `	/* Zero the structure */` |
|   102130 |   248 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   249 | `	/* Initialize structure fields */` |
|        - |   250 | `	/* Arguments container */` |
|   102130 |   251 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   252 | `	/* Static variable container */` |
|   102130 |   253 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   254 | `	/* Bytecode container */` |
|   102130 |   255 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   256 | `    /* Preallocate some instruction slots */` |
|   102130 |   257 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   258 | `	/* Closure environment */` |
|   102130 |   259 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   102130 |   260 | `	pFunc->iFlags = iFlags;` |
|   102130 |   261 | `	pFunc->pUserData = pUserData;` |
|   102130 |   262 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   102130 |   263 | `	return SXRET_OK;` |
|        2 |   264 |  |
|        - |   265 | `/*` |
|        - |   266 | ` * Namespace-aware function lookup.` |
|        - |   267 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   268 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   269 | ` */` |
|        - |   270 | `/*` |
|        - |   271 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   272 | ` */` |
|   371864 |   273 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   274 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   275 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   276 | `	SyString *pName     /* Function name */` |
|        - |   277 | `	)` |
|        2 |   278 |  |
|        - |   279 | `	SyHashEntry *pEntry;` |
|        - |   280 | `	sxi32 rc;` |
|   371866 |   281 | `	if( pName == 0 ){` |
|        - |   282 | `		/* Use the built-in name */` |
|    31848 |   283 | `		pName = &pFunc->sName;` |
|    15923 |   284 | `	}` |
|        - |   285 | `	/* Check for duplicates (functions with the same name) first */` |
|   371866 |   286 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   371866 |   287 | `	if( pEntry ){` |
|   289254 |   288 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   289254 |   289 | `		if( pLink != pFunc ){` |
|        - |   290 | `			/* Link */` |
|      184 |   291 | `			pFunc->pNextName = pLink;` |
|      184 |   292 | `			pEntry->pUserData = pFunc;` |
|       91 |   293 | `		}` |
|   289254 |   294 | `		return SXRET_OK;` |
|        - |   295 | `	}` |
|        - |   296 | `	/* First time seen */` |
|    82614 |   297 | `	pFunc->pNextName = 0;` |
|    82614 |   298 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    82614 |   299 | `	return rc;` |
|   185934 |   300 |  |
|        - |   301 | `/*` |
|        - |   302 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   303 | ` */` |
|    29322 |   304 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   305 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   306 | `	ph7_class *pClass /* Target Class */` |
|        - |   307 | `	)` |
|        2 |   308 |  |
|    29324 |   309 | `	SyString *pName = &pClass->sName;` |
|        - |   310 | `	SyHashEntry *pEntry;` |
|        - |   311 | `	sxi32 rc;` |
|        - |   312 | `	/* Check for duplicates */` |
|    29324 |   313 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    29324 |   314 | `	if( pEntry ){` |
|       31 |   315 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   316 | `		/* Link entry with the same name */` |
|       31 |   317 | `		pClass->pNextName = pLink;` |
|       31 |   318 | `		pEntry->pUserData = pClass;` |
|       31 |   319 | `		return SXRET_OK;` |
|        - |   320 | `	}` |
|    29294 |   321 | `	pClass->pNextName = 0;` |
|        - |   322 | `	/* Perform a simple hashtable insertion */` |
|    29294 |   323 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    29294 |   324 | `	return rc;` |
|    14663 |   325 |  |
|        - |   326 | `/*` |
|        - |   327 | ` * Instruction builder interface.` |
|        - |   328 | ` */` |
|  2718002 |   329 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  2718004 |   341 | `	sInstr.iOp = (sxu8)iOp;` |
|  2718004 |   342 | `	sInstr.iP1 = iP1;` |
|  2718004 |   343 | `	sInstr.iP2 = iP2;` |
|  2718004 |   344 | `	sInstr.p3  = p3;` |
|  2718004 |   345 | `	if( pIndex ){` |
|        - |   346 | `		/* Instruction index in the bytecode array */` |
|   172568 |   347 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    86283 |   348 | `	}` |
|        - |   349 | `	/* Finally,record the instruction */` |
|  2718004 |   350 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  2718004 |   351 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   352 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   353 | `		/* Fall throw */` |
|      ! 0 |   354 | `	}` |
|  2718004 |   355 | `	return rc;` |
|        2 |   356 |  |
|        - |   357 | `/*` |
|        - |   358 | ` * Swap the current bytecode container with the given one.` |
|        - |   359 | ` */` |
|   248200 |   360 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   361 |  |
|   248202 |   362 | `	if( pContainer == 0 ){` |
|        - |   363 | `		/* Point to the default container */` |
|      ! 0 |   364 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   365 | `	}else{` |
|        - |   366 | `		/* Change container */` |
|   248202 |   367 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   368 | `	}` |
|   248202 |   369 | `	return SXRET_OK;` |
|        2 |   370 |  |
|        - |   371 | `/*` |
|        - |   372 | ` * Return the current bytecode container.` |
|        - |   373 | ` */` |
|   124100 |   374 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   375 |  |
|   124102 |   376 | `	return pVm->pByteContainer;` |
|        2 |   377 |  |
|        - |   378 | `/*` |
|        - |   379 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   380 | ` */` |
|   170078 |   381 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   382 |  |
|        - |   383 | `	VmInstr *pInstr;` |
|   170080 |   384 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   170080 |   385 | `	return pInstr;` |
|        2 |   386 |  |
|        - |   387 | `/*` |
|        - |   388 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   389 | ` */` |
|   758548 |   390 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   391 |  |
|   758550 |   392 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Pop the last VM instruction.` |
|        - |   396 | ` */` |
|   161466 |   397 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   398 |  |
|   161468 |   399 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   400 |  |
|        - |   401 | `/*` |
|        - |   402 | ` * Peek the last VM instruction.` |
|        - |   403 | ` */` |
|   532342 |   404 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   405 |  |
|   532344 |   406 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   407 |  |
|    24704 |   408 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   409 |  |
|        - |   410 | `	VmInstr *aInstr;` |
|        - |   411 | `	sxu32 n;` |
|    24706 |   412 | `	n = SySetUsed(pVm->pByteContainer);` |
|    24706 |   413 | `	if( n < 2 ){` |
|      ! 0 |   414 | `		return 0;` |
|        - |   415 | `	}` |
|    24706 |   416 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    24706 |   417 | `	return &aInstr[n - 2];` |
|    12354 |   418 |  |
|        - |   419 | `/*` |
|        - |   420 | ` * Allocate a new virtual machine frame.` |
|        - |   421 | ` */` |
|    14510 |   422 | `static VmFrame * VmNewFrame(` |
|        - |   423 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   424 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   425 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   426 | `	)` |
|        2 |   427 |  |
|        - |   428 | `	VmFrame *pFrame;` |
|        - |   429 | `	/* Allocate a new vm frame */` |
|    14512 |   430 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    14512 |   431 | `	if( pFrame == 0 ){` |
|      ! 0 |   432 | `		return 0;` |
|        - |   433 | `	}` |
|        - |   434 | `	/* Zero the structure */` |
|    14512 |   435 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   436 | `	/* Initialize frame fields */` |
|    14512 |   437 | `	pFrame->pUserData = pUserData;` |
|    14512 |   438 | `	pFrame->pThis = pThis;` |
|    14512 |   439 | `	pFrame->pVm = pVm;` |
|    14512 |   440 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    14512 |   441 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    14512 |   442 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    14512 |   443 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    14512 |   444 | `	return pFrame;` |
|     7257 |   445 |  |
|        - |   446 | `/*` |
|        - |   447 | ` * Enter a VM frame.` |
|        - |   448 | ` */` |
|    14510 |   449 | `static sxi32 VmEnterFrame(` |
|        - |   450 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   451 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   452 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   453 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   454 | `	)` |
|        2 |   455 |  |
|        - |   456 | `	VmFrame *pFrame;` |
|        - |   457 | `	/* Allocate a new frame */` |
|    14512 |   458 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    14512 |   459 | `	if( pFrame == 0 ){` |
|      ! 0 |   460 | `		return SXERR_MEM;` |
|        - |   461 | `	}` |
|        - |   462 | `	/* Link to the list of active VM frame */` |
|    14512 |   463 | `	pFrame->pParent = pVm->pFrame;` |
|    14512 |   464 | `	pVm->pFrame = pFrame;` |
|    14512 |   465 | `	if( ppFrame ){` |
|        - |   466 | `		/* Write a pointer to the new VM frame */` |
|    12100 |   467 | `		*ppFrame = pFrame;` |
|     6049 |   468 | `	}` |
|    14512 |   469 | `	return SXRET_OK;` |
|     7257 |   470 |  |
|        - |   471 | `/*` |
|        - |   472 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   473 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   474 | ` * information.` |
|        - |   475 | ` */` |
|       52 |   476 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        2 |   477 |  |
|        - |   478 | `	VmFrame *pTarget,*pFrame;` |
|       54 |   479 | `	SyHashEntry *pEntry = 0;` |
|        - |   480 | `	sxi32 rc;` |
|        - |   481 | `	/* Point to the upper frame */` |
|       54 |   482 | `	pFrame = pVm->pFrame;` |
|       54 |   483 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |   484 | `		/* Safely ignore the exception frame */` |
|      ! 0 |   485 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   486 | `	}` |
|       54 |   487 | `	pTarget = pFrame;` |
|       54 |   488 | `	pFrame = pTarget->pParent;` |
|       54 |   489 | `	while( pFrame ){` |
|       54 |   490 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   491 | `			/* Query the current frame */` |
|       54 |   492 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       54 |   493 | `			if( pEntry ){` |
|        - |   494 | `				/* Variable found */` |
|       54 |   495 | `				break;` |
|        - |   496 | `			}` |
|      ! 0 |   497 | `		}` |
|        - |   498 | `		/* Point to the upper frame */` |
|      ! 0 |   499 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   500 | `	}` |
|       54 |   501 | `	if( pEntry == 0 ){` |
|        - |   502 | `		/* Inexistant variable */` |
|      ! 0 |   503 | `		return SXERR_NOTFOUND;` |
|        - |   504 | `	}` |
|        - |   505 | `	/* Link to the current frame */` |
|       54 |   506 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       54 |   507 | `	if( rc == SXRET_OK ){` |
|        - |   508 | `		sxu32 nIdx;` |
|       54 |   509 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       54 |   510 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       26 |   511 | `	}` |
|       54 |   512 | `	return rc;` |
|       28 |   513 |  |
|        - |   514 | `/*` |
|        - |   515 | ` * Leave the top-most active frame.` |
|        - |   516 | ` */` |
|    12098 |   517 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   518 |  |
|    12100 |   519 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    12100 |   520 | `	if( pCurFrame ){` |
|        - |   521 | `		/* Unlink from the list of active VM frame */` |
|    12100 |   522 | `		pVm->pFrame = pCurFrame->pParent;` |
|    12100 |   523 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   524 | `			VmSlot  *aSlot;` |
|        - |   525 | `			sxu32 n;` |
|        - |   526 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    12076 |   527 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    86442 |   528 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   529 | `				/* Unset the local variable */` |
|    74368 |   530 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    37185 |   531 | `			}` |
|        - |   532 | `			/* Remove local reference */` |
|    12076 |   533 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    86498 |   534 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    74424 |   535 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    37213 |   536 | `			}` |
|     6037 |   537 | `		}` |
|        - |   538 | `		/* Release internal containers */` |
|    12100 |   539 | `		SyHashRelease(&pCurFrame->hVar);` |
|    12100 |   540 | `		SySetRelease(&pCurFrame->sArg);` |
|    12100 |   541 | `		SySetRelease(&pCurFrame->sLocal);` |
|    12100 |   542 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   543 | `		/* Release the whole structure */` |
|    12100 |   544 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6049 |   545 | `	}` |
|    12100 |   546 |  |
|        - |   547 | `/*` |
|        - |   548 | ` * Compare two functions signature and return the comparison result.` |
|        - |   549 | ` */` |
|      818 |   550 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   551 |  |
|      819 |   552 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      819 |   553 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      819 |   554 | `	const char *zSin = pSecond->zString;` |
|      819 |   555 | `	const char *zFin = pFirst->zString;` |
|      819 |   556 | `	const char *zPtr = zFin;` |
|      409 |   557 | `	for(;;){` |
|      819 |   558 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      410 |   559 | `			break;` |
|        - |   560 | `		}` |
|      ! 0 |   561 | `		if( zFin[0] != zSin[0] ){` |
|        - |   562 | `			/* mismatch */` |
|      ! 0 |   563 | `			break;` |
|        - |   564 | `		}` |
|      ! 0 |   565 | `		zFin++;` |
|      ! 0 |   566 | `		zSin++;` |
|      ! 0 |   567 | `	}` |
|      819 |   568 | `	return (int)(zFin-zPtr);` |
|        1 |   569 |  |
|        - |   570 | `/*` |
|        - |   571 | ` * Select the appropriate VM function for the current call context.` |
|        - |   572 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   573 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   574 | ` * Refer to the official documentation for more information.` |
|        - |   575 | ` */` |
|      132 |   576 | `static ph7_vm_func * VmOverload(` |
|        - |   577 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   578 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   579 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   580 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   581 | `	)` |
|        2 |   582 |  |
|        - |   583 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   584 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   585 | `	ph7_vm_func *pLink;` |
|        - |   586 | `	SyString sArgSig;` |
|        - |   587 | `	SyBlob sSig;` |
|        - |   588 |  |
|      134 |   589 | `	pLink = pList;` |
|      134 |   590 | `	i = 0;` |
|        - |   591 | `	/* Put functions expecting the same number of passed arguments */` |
|     1062 |   592 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1000 |   593 | `		if( pLink == 0 ){` |
|       72 |   594 | `			break;` |
|        - |   595 | `		}` |
|      930 |   596 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   597 | `			/* Candidate for overloading */` |
|      884 |   598 | `			apSet[i++] = pLink;` |
|      441 |   599 | `		}` |
|        - |   600 | `		/* Point to the next entry */` |
|      930 |   601 | `		pLink = pLink->pNextName;` |
|        2 |   602 | `	}` |
|      134 |   603 | `	if( i < 1 ){` |
|        - |   604 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   605 | `		return pList;` |
|        - |   606 | `	}` |
|      134 |   607 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   608 | `		/* Return the only candidate */` |
|       32 |   609 | `		return apSet[0];` |
|        - |   610 | `	}` |
|        - |   611 | `	/* Calculate function signature */` |
|      103 |   612 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      355 |   613 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      253 |   614 | `		int c = 'n'; /* null */` |
|      253 |   615 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   616 | `			/* Hashmap */` |
|       45 |   617 | `			c = 'h';` |
|      231 |   618 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   619 | `			/* bool */` |
|      ! 0 |   620 | `			c = 'b';` |
|      209 |   621 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   622 | `			/* int */` |
|        5 |   623 | `			c = 'i';` |
|      207 |   624 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   625 | `			/* String */` |
|      105 |   626 | `			c = 's';` |
|      153 |   627 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   628 | `			/* Float */` |
|      ! 0 |   629 | `			c = 'f';` |
|      101 |   630 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   631 | `			/* Class instance */` |
|      ! 0 |   632 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|      ! 0 |   633 | `			SyString *pName = &pClass->sName;` |
|      ! 0 |   634 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|      ! 0 |   635 | `			c = -1;` |
|      ! 0 |   636 | `		}` |
|      253 |   637 | `		if( c > 0 ){` |
|      253 |   638 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      126 |   639 | `		}` |
|      127 |   640 | `	}` |
|      103 |   641 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      103 |   642 | `	iTarget = 0;` |
|      103 |   643 | `	iMax = -1;` |
|        - |   644 | `	/* Select the appropriate function */` |
|      921 |   645 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   646 | `		/* Compare the two signatures */` |
|      819 |   647 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      819 |   648 | `		if( iCur > iMax ){` |
|      103 |   649 | `			iMax = iCur;` |
|      103 |   650 | `			iTarget = j;` |
|       51 |   651 | `		}` |
|      410 |   652 | `	}` |
|      103 |   653 | `	SyBlobRelease(&sSig);` |
|        - |   654 | `	/* Appropriate function for the current call context */` |
|      103 |   655 | `	return apSet[iTarget];` |
|       68 |   656 |  |
|        - |   657 | `/* Forward declaration */` |
|        - |   658 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   659 | `/*` |
|        - |   660 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   661 | ` * it can be instanciated from the executed PHP script.` |
|        - |   662 | ` */` |
|    87308 |   663 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   664 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   665 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   666 | `	)` |
|        2 |   667 |  |
|        - |   668 | `	ph7_class_method *pMeth;` |
|        - |   669 | `	ph7_class_attr *pAttr;` |
|        - |   670 | `	SyHashEntry *pEntry;` |
|        - |   671 | `	sxi32 rc;` |
|        - |   672 | `	/* Reset the loop cursor */` |
|    87310 |   673 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   674 | `	/* Process only static and constant attribute */` |
|   343600 |   675 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   676 | `		/* Extract the current attribute */` |
|   212638 |   677 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   212638 |   678 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   679 | `			ph7_value *pMemObj;` |
|        - |   680 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1294 |   681 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1294 |   682 | `			if( pMemObj == 0 ){` |
|      ! 0 |   683 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   684 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   685 | `					&pClass->sName,&pAttr->sName` |
|        - |   686 | `					);` |
|      ! 0 |   687 | `				return SXERR_MEM;` |
|        - |   688 | `			}` |
|     1294 |   689 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   690 | `				/* Initialize attribute default value (any complex expression) */` |
|     1294 |   691 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      646 |   692 | `			}` |
|        - |   693 | `			/* Record attribute index */` |
|     1294 |   694 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   695 | `			/* Install static attribute in the reference table */` |
|     1294 |   696 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      646 |   697 | `		}` |
|        2 |   698 | `	}` |
|        - |   699 | `	/* Install class methods */` |
|    87310 |   700 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   701 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   702 | `		 */` |
|    45694 |   703 | `		return SXRET_OK;` |
|        - |   704 | `	}` |
|        - |   705 | `	/* Create constructor alias if not yet done */` |
|    41618 |   706 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   707 | `		/* User constructor with the same base class name */` |
|      276 |   708 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      276 |   709 | `		if( pEntry ){` |
|      ! 0 |   710 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   711 | `			/* Create the alias */` |
|      ! 0 |   712 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   713 | `		}` |
|      137 |   714 | `	}` |
|        - |   715 | `	/* Install the methods now */` |
|    41618 |   716 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   402450 |   717 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   340026 |   718 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   340026 |   719 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   340020 |   720 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   340020 |   721 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   722 | `				return rc;` |
|        - |   723 | `			}` |
|   170009 |   724 | `		}` |
|        2 |   725 | `	}` |
|        - |   726 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    41618 |   727 | `	pClass->bMounted = TRUE;` |
|    41618 |   728 | `	return SXRET_OK;` |
|    43656 |   729 |  |
|        - |   730 | `/*` |
|        - |   731 | ` * Allocate a private frame for attributes of the given` |
|        - |   732 | ` * class instance (Object in the PHP jargon).` |
|        - |   733 | ` */` |
|     1104 |   734 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   735 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   736 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   737 | `	)` |
|        2 |   738 |  |
|     1106 |   739 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   740 | `	ph7_class_attr *pAttr;` |
|        - |   741 | `	SyHashEntry *pEntry;` |
|        - |   742 | `	sxi32 rc;` |
|        - |   743 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1106 |   744 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4600 |   745 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   746 | `		VmClassAttr *pVmAttr;` |
|        - |   747 | `		/* Extract the current attribute */` |
|     3496 |   748 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3496 |   749 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3496 |   750 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   751 | `			return SXERR_MEM;` |
|        - |   752 | `		}` |
|     3496 |   753 | `		pVmAttr->pAttr = pAttr;` |
|     3496 |   754 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   755 | `			ph7_value *pMemObj;` |
|        - |   756 | `			/* Reserve a memory object for this attribute */` |
|     3490 |   757 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3490 |   758 | `			if( pMemObj == 0 ){` |
|      ! 0 |   759 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   760 | `				return SXERR_MEM;` |
|        - |   761 | `			}` |
|     3490 |   762 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3490 |   763 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   764 | `				/* Initialize attribute default value (any complex expression) */` |
|     1144 |   765 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      571 |   766 | `			}` |
|     3490 |   767 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3490 |   768 | `			if( rc != SXRET_OK ){` |
|        - |   769 | `				VmSlot sSlot;` |
|        - |   770 | `				/* Restore memory object */` |
|      ! 0 |   771 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   772 | `				sSlot.pUserData = 0;` |
|      ! 0 |   773 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   774 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   775 | `				return SXERR_MEM;` |
|        - |   776 | `			}` |
|        - |   777 | `			/* Install attribute in the reference table */` |
|     3490 |   778 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1746 |   779 | `		}else{` |
|        - |   780 | `			/* Install static/constant attribute */` |
|        8 |   781 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   782 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   783 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   784 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   785 | `				return SXERR_MEM;` |
|        - |   786 | `			}` |
|        - |   787 | `		}` |
|        2 |   788 | `	}` |
|     1106 |   789 | `	return SXRET_OK;` |
|      554 |   790 |  |
|        - |   791 | `/* Forward declaration */` |
|        - |   792 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   793 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   794 | `/*` |
|        - |   795 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   796 | ` */` |
|        - |   797 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   798 | `/*` |
|        - |   799 | ` * Reserve a constant memory object.` |
|        - |   800 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   801 | ` */` |
|   295542 |   802 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   803 |  |
|        - |   804 | `	ph7_value *pObj;` |
|        - |   805 | `	sxi32 rc;` |
|   295544 |   806 | `	if( pIndex ){` |
|        - |   807 | `		/* Object index in the object table */` |
|   288308 |   808 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   144153 |   809 | `	}` |
|        - |   810 | `	/* Reserve a slot for the new object */` |
|   295544 |   811 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   295544 |   812 | `	if( rc != SXRET_OK ){` |
|        - |   813 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   814 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   815 | `		 */` |
|      ! 0 |   816 | `		return 0;` |
|        - |   817 | `	}` |
|   295544 |   818 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   295544 |   819 | `	return pObj;` |
|   147773 |   820 |  |
|        - |   821 | `/*` |
|        - |   822 | ` * Reserve a memory object.` |
|        - |   823 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   824 | ` */` |
|  2138920 |   825 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   826 |  |
|        - |   827 | `	ph7_value *pObj;` |
|        - |   828 | `	sxi32 rc;` |
|  2138922 |   829 | `	if( pIndex ){` |
|        - |   830 | `		/* Object index in the object table */` |
|  2138922 |   831 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1069460 |   832 | `	}` |
|        - |   833 | `	/* Reserve a slot for the new object */` |
|  2138922 |   834 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2138922 |   835 | `	if( rc != SXRET_OK ){` |
|        - |   836 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   837 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   838 | `		 */` |
|      ! 0 |   839 | `		return 0;` |
|        - |   840 | `	}` |
|  2138922 |   841 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2138922 |   842 | `	return pObj;` |
|  1069462 |   843 |  |
|        - |   844 | `/* Forward declaration */` |
|        - |   845 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   846 | `/*` |
|        - |   847 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   848 | ` * directly as foreign functions.` |
|        - |   849 | ` */` |
|        - |   850 | `#define PH7_BUILTIN_LIB \` |
|        - |   851 | `	"class Exception { "\` |
|        - |   852 | `    "protected $message = 'Unknown exception';"\` |
|        - |   853 | `    "protected $code = 0;"\` |
|        - |   854 | `    "protected $file;"\` |
|        - |   855 | `    "protected $line;"\` |
|        - |   856 | `    "protected $trace;"\` |
|        - |   857 | `    "protected $previous;"\` |
|        - |   858 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   859 | `	"   if( isset($message) ){"\` |
|        - |   860 | `	"	  $this->message = $message;"\` |
|        - |   861 | `	"   }"\` |
|        - |   862 | `	"   $this->code = $code;"\` |
|        - |   863 | `	"   $this->file = __FILE__;"\` |
|        - |   864 | `	"   $this->line = __LINE__;"\` |
|        - |   865 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   866 | `	"   if( isset($previous) ){"\` |
|        - |   867 | `	"     $this->previous = $previous;"\` |
|        - |   868 | `	"   }"\` |
|        - |   869 | `	"}"\` |
|        - |   870 | `	"public function getMessage(){"\` |
|        - |   871 | `	"   return $this->message;"\` |
|        - |   872 | `	"}"\` |
|        - |   873 | `	" public function getCode(){"\` |
|        - |   874 | `	"  return $this->code;"\` |
|        - |   875 | `	"}"\` |
|        - |   876 | `	"public function getFile(){"\` |
|        - |   877 | `	"  return $this->file;"\` |
|        - |   878 | `	"}"\` |
|        - |   879 | `	"public function getLine(){"\` |
|        - |   880 | `	"  return $this->line;"\` |
|        - |   881 | `	"}"\` |
|        - |   882 | `	"public function getTrace(){"\` |
|        - |   883 | `	"   return $this->trace;"\` |
|        - |   884 | `	"}"\` |
|        - |   885 | `	"public function getTraceAsString(){"\` |
|        - |   886 | `	"  return debug_string_backtrace();"\` |
|        - |   887 | `	"}"\` |
|        - |   888 | `	"public function getPrevious(){"\` |
|        - |   889 | `	"    return $this->previous;"\` |
|        - |   890 | `	"}"\` |
|        - |   891 | `	"public function __toString(){"\` |
|        - |   892 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   893 | `    "}"\` |
|        - |   894 | `	"}"\` |
|        - |   895 | `	"class Error extends Exception { }"\` |
|        - |   896 | `	"class TypeError extends Error { }"\` |
|        - |   897 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   898 | `	"class ValueError extends Error { }"\` |
|        - |   899 | `	"class AssertionError extends Error { }"\` |
|        - |   900 | `	"class ErrorException extends Exception { "\` |
|        - |   901 | `	"protected $severity;"\` |
|        - |   902 | `	"public function __construct(string $message = null,"\` |
|        - |   903 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   904 | `	"   if( isset($message) ){"\` |
|        - |   905 | `	"	  $this->message = $message;"\` |
|        - |   906 | `	"   }"\` |
|        - |   907 | `	"   $this->severity = $severity;"\` |
|        - |   908 | `	"   $this->code = $code;"\` |
|        - |   909 | `	"   $this->file = $filename;"\` |
|        - |   910 | `	"   $this->line = $lineno;"\` |
|        - |   911 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   912 | `	"   if( isset($previous) ){"\` |
|        - |   913 | `	"     $this->previous = $previous;"\` |
|        - |   914 | `	"   }"\` |
|        - |   915 | `	"}"\` |
|        - |   916 | `	"public function getSeverity(){"\` |
|        - |   917 | `	"   return $this->severity;"\` |
|        - |   918 | `    "}"\` |
|        - |   919 | `	"}"\` |
|        - |   920 | `	"interface Iterator {"\` |
|        - |   921 | `	"public function current();"\` |
|        - |   922 | `	"public function key();"\` |
|        - |   923 | `	"public function next();"\` |
|        - |   924 | `	"public function rewind();"\` |
|        - |   925 | `	"public function valid();"\` |
|        - |   926 | `	"}"\` |
|        - |   927 | `	"interface IteratorAggregate {"\` |
|        - |   928 | `	"public function getIterator();"\` |
|        - |   929 | `	"}"\` |
|        - |   930 | `	"interface Serializable {"\` |
|        - |   931 | `	"public function serialize();"\` |
|        - |   932 | `	"public function unserialize(string $serialized);"\` |
|        - |   933 | `	"}"\` |
|        - |   934 | `	"/* Directory releated IO */"\` |
|        - |   935 | `	"class Directory {"\` |
|        - |   936 | `	"public $handle = null;"\` |
|        - |   937 | `	"public $path  = null;"\` |
|        - |   938 | `	"public function __construct(string $path)"\` |
|        - |   939 | `	"{"\` |
|        - |   940 | `	"   $this->handle = opendir($path);"\` |
|        - |   941 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   942 | `	"      $this->path = $path;"\` |
|        - |   943 | `	"   }"\` |
|        - |   944 | `	"}"\` |
|        - |   945 | `	"public function __destruct()"\` |
|        - |   946 | `	"{"\` |
|        - |   947 | `	"  if( $this->handle != null ){"\` |
|        - |   948 | `	"       closedir($this->handle);"\` |
|        - |   949 | `	"  }"\` |
|        - |   950 | `	"}"\` |
|        - |   951 | `	"public function read()"\` |
|        - |   952 | `	"{"\` |
|        - |   953 | `	"    return readdir($this->handle);"\` |
|        - |   954 | `	"}"\` |
|        - |   955 | `	"public function rewind()"\` |
|        - |   956 | `	"{"\` |
|        - |   957 | `	"    rewinddir($this->handle);"\` |
|        - |   958 | `	"}"\` |
|        - |   959 | `	"public function close()"\` |
|        - |   960 | `	"{"\` |
|        - |   961 | `	"    closedir($this->handle);"\` |
|        - |   962 | `	"    $this->handle = null;"\` |
|        - |   963 | `	"}"\` |
|        - |   964 | `	"}"\` |
|        - |   965 | `	"class stdClass{"\` |
|        - |   966 | `	"  public $value;"\` |
|        - |   967 | `	" /* Magic methods */"\` |
|        - |   968 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |   969 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |   970 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |   971 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |   972 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |   973 | `	"}"\` |
|        - |   974 | `	"function dir(string $path){"\` |
|        - |   975 | `	"   return new Directory($path);"\` |
|        - |   976 | `	"}"\` |
|        - |   977 | `	"function Dir(string $path){"\` |
|        - |   978 | `	"   return new Directory($path);"\` |
|        - |   979 | `	"}"\` |
|        - |   980 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |   981 | `    "{"\` |
|        - |   982 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |   983 | `	"  $aDir = array();"\` |
|        - |   984 | `	"  $pHandle = opendir($directory);"\` |
|        - |   985 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |   986 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |   987 | `	"      $aDir[] = $pEntry;"\` |
|        - |   988 | `	"   }"\` |
|        - |   989 | `	"  closedir($pHandle);"\` |
|        - |   990 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |   991 | `	"      rsort($aDir);"\` |
|        - |   992 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |   993 | `	"      sort($aDir);"\` |
|        - |   994 | `	"  }"\` |
|        - |   995 | `	"  return $aDir;"\` |
|        - |   996 | `	"}"\` |
|        - |   997 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |   998 | `	"/* Open the target directory */"\` |
|        - |   999 | `	"$zDir = dirname($pattern);"\` |
|        - |  1000 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1001 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1002 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1003 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1004 | `	"	return FALSE;"\` |
|        - |  1005 | `	"}"\` |
|        - |  1006 | `	"$pattern = basename($pattern);"\` |
|        - |  1007 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1008 | `	"/* Loop throw available entries */"\` |
|        - |  1009 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1010 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1011 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1012 | `	"	if( $rc ){"\` |
|        - |  1013 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1014 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1015 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1016 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1017 | `	"		  }"\` |
|        - |  1018 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1019 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1020 | `	"		 continue;"\` |
|        - |  1021 | `	"	   }"\` |
|        - |  1022 | `	"	   /* Add the entry */"\` |
|        - |  1023 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1024 | `	"	}"\` |
|        - |  1025 | `	" }"\` |
|        - |  1026 | `	"/* Close the handle */"\` |
|        - |  1027 | `	"closedir($pHandle);"\` |
|        - |  1028 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1029 | `	"  /* Sort the array */"\` |
|        - |  1030 | `	"  sort($pArray);"\` |
|        - |  1031 | `	"}"\` |
|        - |  1032 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1033 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1034 | `	"  $pArray[] = $pattern;"\` |
|        - |  1035 | `	"}"\` |
|        - |  1036 | `	"/* Return the created array */"\` |
|        - |  1037 | `	"return $pArray;"\` |
|        - |  1038 | `   "}"\` |
|        - |  1039 | `   "/* Creates a temporary file */"\` |
|        - |  1040 | `   "function tmpfile(){"\` |
|        - |  1041 | `   "  /* Extract the temp directory */"\` |
|        - |  1042 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1043 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1044 | `   "    /* Use the current dir */"\` |
|        - |  1045 | `   "    $zTempDir = '.';"\` |
|        - |  1046 | `   "  }"\` |
|        - |  1047 | `   "  /* Create the file */"\` |
|        - |  1048 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1049 | `   "  return $pHandle;"\` |
|        - |  1050 | `   "}"\` |
|        - |  1051 | `   "/* Creates a temporary filename */"\` |
|        - |  1052 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1053 | `   "{"\` |
|        - |  1054 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1055 | `   "}"\` |
|        - |  1056 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1057 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1058 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1059 | `   "/* Copy arguments */"\` |
|        - |  1060 | `   "$nArgs = func_num_args();"\` |
|        - |  1061 | `   "$pNew = array();"\` |
|        - |  1062 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1063 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1064 | `    "}"\` |
|        - |  1065 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1066 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1067 | `	"/* Erase */"\` |
|        - |  1068 | `	"array_erase($pArray);"\` |
|        - |  1069 | `	"/* Unshift */"\` |
|        - |  1070 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1071 | `	"return sizeof($pArray);"\` |
|        - |  1072 | `    "}"\` |
|        - |  1073 | `	"function array_merge_recursive(){"\` |
|        - |  1074 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1075 | `    "$arrays = func_get_args();"\` |
|        - |  1076 | `    "$narrays = count($arrays);"\` |
|        - |  1077 | `    "$ret = array();"\` |
|        - |  1078 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1079 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1080 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1081 | `	 " }"\` |
|        - |  1082 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1083 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1084 | `     "  if( $keyIsInt ) {"\` |
|        - |  1085 | `     "   $ret[] = $value;"\` |
|        - |  1086 | `     "  } else {"\` |
|        - |  1087 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1088 | `     "    $cur = $ret[$key];"\` |
|        - |  1089 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1090 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1091 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1092 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1093 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1094 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1095 | `     "    } else {"\` |
|        - |  1096 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1097 | `     "    }"\` |
|        - |  1098 | `     "   } else {"\` |
|        - |  1099 | `     "    $ret[$key] = $value;"\` |
|        - |  1100 | `     "   }"\` |
|        - |  1101 | `     "  }"\` |
|        - |  1102 | `     " }"\` |
|        - |  1103 | `	 " }"\` |
|        - |  1104 | `	 " return $ret;"\` |
|        - |  1105 | `    "}"\` |
|        - |  1106 | `	"function max(){"\` |
|        - |  1107 | `    "  $pArgs = func_get_args();"\` |
|        - |  1108 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1109 | `	"  return null;"\` |
|        - |  1110 | `    " }"\` |
|        - |  1111 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1112 | `    " $pArg = $pArgs[0];"\` |
|        - |  1113 | `	" if( !is_array($pArg) ){"\` |
|        - |  1114 | `	"   return $pArg; "\` |
|        - |  1115 | `	" }"\` |
|        - |  1116 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1117 | `	"   return null;"\` |
|        - |  1118 | `	" }"\` |
|        - |  1119 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1120 | `	" reset($pArg);"\` |
|        - |  1121 | `	" $max = current($pArg);"\` |
|        - |  1122 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1123 | `	"   if( $val > $max ){"\` |
|        - |  1124 | `	"     $max = $val;"\` |
|        - |  1125 | `    " }"\` |
|        - |  1126 | `	" }"\` |
|        - |  1127 | `	" return $max;"\` |
|        - |  1128 | `    " }"\` |
|        - |  1129 | `    " $max = $pArgs[0];"\` |
|        - |  1130 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1131 | `    " $val = $pArgs[$i];"\` |
|        - |  1132 | `	"if( $val > $max ){"\` |
|        - |  1133 | `	" $max = $val;"\` |
|        - |  1134 | `	"}"\` |
|        - |  1135 | `    " }"\` |
|        - |  1136 | `	" return $max;"\` |
|        - |  1137 | `    "}"\` |
|        - |  1138 | `	"function min(){"\` |
|        - |  1139 | `    "  $pArgs = func_get_args();"\` |
|        - |  1140 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1141 | `	"  return null;"\` |
|        - |  1142 | `    " }"\` |
|        - |  1143 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1144 | `    " $pArg = $pArgs[0];"\` |
|        - |  1145 | `	" if( !is_array($pArg) ){"\` |
|        - |  1146 | `	"   return $pArg; "\` |
|        - |  1147 | `	" }"\` |
|        - |  1148 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1149 | `	"   return null;"\` |
|        - |  1150 | `	" }"\` |
|        - |  1151 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1152 | `	" reset($pArg);"\` |
|        - |  1153 | `	" $min = current($pArg);"\` |
|        - |  1154 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1155 | `	"   if( $val < $min ){"\` |
|        - |  1156 | `	"     $min = $val;"\` |
|        - |  1157 | `    " }"\` |
|        - |  1158 | `	" }"\` |
|        - |  1159 | `	" return $min;"\` |
|        - |  1160 | `    " }"\` |
|        - |  1161 | `    " $min = $pArgs[0];"\` |
|        - |  1162 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1163 | `    " $val = $pArgs[$i];"\` |
|        - |  1164 | `	"if( $val < $min ){"\` |
|        - |  1165 | `	" $min = $val;"\` |
|        - |  1166 | `	" }"\` |
|        - |  1167 | `    " }"\` |
|        - |  1168 | `	" return $min;"\` |
|        - |  1169 | `	"}"\` |
|        - |  1170 | `	"function fileowner(string $file){"\` |
|        - |  1171 | `    " $a = stat($file);"\` |
|        - |  1172 | `	" if( !is_array($a) ){"\` |
|        - |  1173 | `	"	return false;"\` |
|        - |  1174 | `	" }"\` |
|        - |  1175 | `	" return $a['uid'];"\` |
|        - |  1176 | `    "}"\` |
|        - |  1177 | `    "function filegroup(string $file){"\` |
|        - |  1178 | `	" $a = stat($file);"\` |
|        - |  1179 | `	" if( !is_array($a) ){"\` |
|        - |  1180 | `	"	return false;"\` |
|        - |  1181 | `	" }"\` |
|        - |  1182 | `	" return $a['gid'];"\` |
|        - |  1183 | `    "}"\` |
|        - |  1184 | `	 "function fileinode(string $file){"\` |
|        - |  1185 | `	" $a = stat($file);"\` |
|        - |  1186 | `	" if( !is_array($a) ){"\` |
|        - |  1187 | `	"	return false;"\` |
|        - |  1188 | `	" }"\` |
|        - |  1189 | `	" return $a['ino'];"\` |
|        - |  1190 | `    "}"` |
|        - |  1191 |  |
|        - |  1192 | `/*` |
|        - |  1193 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1194 | ` * start compiling the target PHP program.` |
|        - |  1195 | ` */` |
|     2412 |  1196 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1197 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1198 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1199 | `	 )` |
|        2 |  1200 |  |
|        - |  1201 | `	SyString sBuiltin;` |
|        - |  1202 | `	ph7_value *pObj;` |
|        - |  1203 | `	sxi32 rc;` |
|        - |  1204 | `	/* Zero the structure */` |
|     2414 |  1205 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1206 | `	/* Initialize VM fields */` |
|     2414 |  1207 | `	pVm->pEngine = &(*pEngine);` |
|     2414 |  1208 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1209 | `	/* Instructions containers */` |
|     2414 |  1210 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2414 |  1211 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2414 |  1212 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1213 | `	/* Object containers */` |
|     2414 |  1214 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2414 |  1215 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1216 | `	/* Virtual machine internal containers */` |
|     2414 |  1217 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2414 |  1218 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2414 |  1219 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2414 |  1220 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2414 |  1221 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2414 |  1222 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2414 |  1223 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2414 |  1224 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2414 |  1225 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2414 |  1226 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2414 |  1227 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2414 |  1228 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2414 |  1229 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2414 |  1230 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2414 |  1231 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2414 |  1232 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2414 |  1233 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|        - |  1234 | `	/* Configuration containers */` |
|     2414 |  1235 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2414 |  1236 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2414 |  1237 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2414 |  1238 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2414 |  1239 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1240 | `	/* Error callbacks containers */` |
|     2414 |  1241 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2414 |  1242 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2414 |  1243 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2414 |  1244 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2414 |  1245 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1246 | `	/* Set a default recursion limit */` |
|        - |  1247 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2414 |  1248 | `	pVm->nMaxDepth = 32;` |
|        - |  1249 | `#else` |
|        - |  1250 | `	pVm->nMaxDepth = 16;` |
|        - |  1251 | `#endif` |
|        - |  1252 | `	/* Default assertion flags */` |
|     2414 |  1253 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1254 | `	/* JSON return status */` |
|     2414 |  1255 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1256 | `	/* PRNG context */` |
|     2414 |  1257 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1258 | `	/* Install the null constant */` |
|     2414 |  1259 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2414 |  1260 | `	if( pObj == 0 ){` |
|      ! 0 |  1261 | `		rc = SXERR_MEM;` |
|      ! 0 |  1262 | `		goto Err;` |
|        - |  1263 | `	}` |
|     2414 |  1264 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1265 | `	/* Install the boolean TRUE constant */` |
|     2414 |  1266 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2414 |  1267 | `	if( pObj == 0 ){` |
|      ! 0 |  1268 | `		rc = SXERR_MEM;` |
|      ! 0 |  1269 | `		goto Err;` |
|        - |  1270 | `	}` |
|     2414 |  1271 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1272 | `	/* Install the boolean FALSE constant */` |
|     2414 |  1273 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2414 |  1274 | `	if( pObj == 0 ){` |
|      ! 0 |  1275 | `		rc = SXERR_MEM;` |
|      ! 0 |  1276 | `		goto Err;` |
|        - |  1277 | `	}` |
|     2414 |  1278 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1279 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1280 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1281 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2414 |  1282 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2414 |  1283 | `	if( pObj == 0 ){` |
|      ! 0 |  1284 | `		rc = SXERR_MEM;` |
|      ! 0 |  1285 | `		goto Err;` |
|        - |  1286 | `	}` |
|     2414 |  1287 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1288 | `	/* Create the global frame */` |
|     2414 |  1289 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2414 |  1290 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1291 | `		goto Err;` |
|        - |  1292 | `	}` |
|        - |  1293 | `	/* Initialize the code generator */` |
|     2414 |  1294 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2414 |  1295 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1296 | `		goto Err;` |
|        - |  1297 | `	}` |
|        - |  1298 | `	/* VM correctly initialized,set the magic number */` |
|     2414 |  1299 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2414 |  1300 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1301 | `	/* Compile the built-in library */` |
|     2414 |  1302 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1303 | `	/* Reset the code generator */` |
|     2414 |  1304 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2414 |  1305 | `	return SXRET_OK;` |
|      ! 0 |  1306 | `Err:` |
|      ! 0 |  1307 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1308 | `	return rc;` |
|     1208 |  1309 |  |
|        - |  1310 | `/*` |
|        - |  1311 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1312 | ` * routine which store the output in an internal blob.` |
|        - |  1313 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1314 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1315 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1316 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1317 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1318 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1319 | ` * to finish executing and extracting the output.` |
|        - |  1320 | ` */` |
|      ! 0 |  1321 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1322 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1323 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1324 | `	void *pUserData     /* User private data */` |
|        - |  1325 | `	)` |
|      ! 0 |  1326 |  |
|        - |  1327 | `	 sxi32 rc;` |
|        - |  1328 | `	 /* Store the output in an internal BLOB */` |
|      ! 0 |  1329 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|      ! 0 |  1330 | `	 return rc;` |
|      ! 0 |  1331 |  |
|        - |  1332 | `#define VM_STACK_GUARD 16` |
|        - |  1333 | `/*` |
|        - |  1334 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1335 | ` * our compiled PHP program.` |
|        - |  1336 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1337 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1338 | ` */` |
|    30170 |  1339 | `static ph7_value * VmNewOperandStack(` |
|        - |  1340 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1341 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1342 | `	)` |
|        2 |  1343 |  |
|        - |  1344 | `	ph7_value *pStack;` |
|        - |  1345 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1346 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1347 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1348 | `  ** on the maximum stack depth required.` |
|        - |  1349 | `  **` |
|        - |  1350 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1351 | `  */` |
|    30172 |  1352 | `	nInstr += VM_STACK_GUARD;` |
|    30172 |  1353 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    30172 |  1354 | `	if( pStack == 0 ){` |
|      ! 0 |  1355 | `		return 0;` |
|        - |  1356 | `	}` |
|        - |  1357 | `	/* Initialize the operand stack */` |
|  1918942 |  1358 | `	while( nInstr > 0 ){` |
|  1888772 |  1359 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1888772 |  1360 | `		--nInstr;` |
|        2 |  1361 | `	}` |
|        - |  1362 | `	/* Ready for bytecode execution */` |
|    30172 |  1363 | `	return pStack;` |
|    15087 |  1364 |  |
|        - |  1365 | `/* Forward declaration */` |
|        - |  1366 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1367 | `/*` |
|        - |  1368 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1369 | ` * This routine gets called by the PH7 engine after` |
|        - |  1370 | ` * successful compilation of the target PHP program.` |
|        - |  1371 | ` */` |
|     2178 |  1372 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1373 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1374 | `	)` |
|        2 |  1375 |  |
|        - |  1376 | `	SyHashEntry *pEntry;` |
|        - |  1377 | `	sxi32 rc;` |
|     2180 |  1378 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1379 | `		/* Initialize your VM first */` |
|      ! 0 |  1380 | `		return SXERR_CORRUPT;` |
|        - |  1381 | `	}` |
|        - |  1382 | `	/* Mark the VM ready for byte-code execution */` |
|     2180 |  1383 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1384 | `	/* Release the code generator now we have compiled our program */` |
|     2180 |  1385 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1386 | `	/* Emit the DONE instruction */` |
|     2180 |  1387 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2180 |  1388 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1389 | `		return SXERR_MEM;` |
|        - |  1390 | `	}` |
|        - |  1391 | `	/* Script return value */` |
|     2180 |  1392 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1393 | `	/* Allocate a new operand stack */` |
|     2180 |  1394 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2180 |  1395 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1396 | `		return SXERR_MEM;` |
|        - |  1397 | `	}` |
|        - |  1398 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1399 | `	 * private data. */` |
|     2180 |  1400 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2180 |  1401 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1402 | `	/* Allocate the reference table */` |
|     2180 |  1403 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2180 |  1404 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2180 |  1405 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1406 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1407 | `		return SXERR_MEM;` |
|        - |  1408 | `	}` |
|        - |  1409 | `	/* Zero the reference table */` |
|     2180 |  1410 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1411 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2180 |  1412 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2180 |  1413 | `	if( rc != SXRET_OK ){` |
|        - |  1414 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1415 | `		return rc;` |
|        - |  1416 | `	}` |
|        - |  1417 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2180 |  1418 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2180 |  1419 | `	if( rc != SXRET_OK ){` |
|        - |  1420 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1421 | `		return rc;` |
|        - |  1422 | `	}` |
|        - |  1423 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2180 |  1424 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1425 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2180 |  1426 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1427 | `	/* Initialize and install static and constants class attributes */` |
|     2180 |  1428 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    28454 |  1429 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    26276 |  1430 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    26276 |  1431 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1432 | `			return rc;` |
|        - |  1433 | `		}` |
|        2 |  1434 | `	}` |
|        - |  1435 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2180 |  1436 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1437 | `	/* VM is ready for bytecode execution */` |
|     2180 |  1438 | `	return SXRET_OK;` |
|     1091 |  1439 |  |
|        - |  1440 | `/*` |
|        - |  1441 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1442 | ` */` |
|      ! 0 |  1443 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1444 |  |
|      ! 0 |  1445 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1446 | `		return SXERR_CORRUPT;` |
|        - |  1447 | `	}` |
|        - |  1448 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1449 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1450 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1451 | `	/* Set the ready flag */` |
|      ! 0 |  1452 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1453 | `	return SXRET_OK;` |
|      ! 0 |  1454 |  |
|        - |  1455 | `/*` |
|        - |  1456 | ` * Release a Virtual Machine.` |
|        - |  1457 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1458 | ` */` |
|     2170 |  1459 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1460 |  |
|        - |  1461 | `	/* Set the stale magic number */` |
|     2172 |  1462 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1463 | `	/* Release the private memory subsystem */` |
|     2172 |  1464 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2172 |  1465 | `	return SXRET_OK;` |
|        2 |  1466 |  |
|        - |  1467 | `/*` |
|        - |  1468 | ` * Initialize a foreign function call context.` |
|        - |  1469 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1470 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1471 | ` * functions.` |
|        - |  1472 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1473 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1474 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1475 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1476 | ` */` |
|   541014 |  1477 | `static sxi32 VmInitCallContext(` |
|        - |  1478 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1479 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1480 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1481 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1482 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1483 | `	)` |
|        2 |  1484 |  |
|   541016 |  1485 | `	pOut->pFunc = pFunc;` |
|   541016 |  1486 | `	pOut->pVm   = pVm;` |
|   541016 |  1487 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   541016 |  1488 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1489 | `	/* Assume a null return value */` |
|   541016 |  1490 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   541016 |  1491 | `	pOut->pRet = pRet;` |
|   541016 |  1492 | `	pOut->iFlags = iFlags;` |
|   541016 |  1493 | `	return SXRET_OK;` |
|        2 |  1494 |  |
|        - |  1495 | `/*` |
|        - |  1496 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1497 | ` * left behind.` |
|        - |  1498 | ` */` |
|   541014 |  1499 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1500 |  |
|        - |  1501 | `	sxu32 n;` |
|   541016 |  1502 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6616 |  1503 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    18852 |  1504 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    12238 |  1505 | `			if( apObj[n] == 0 ){` |
|        - |  1506 | `				/* Already released */` |
|      250 |  1507 | `				continue;` |
|        - |  1508 | `			}` |
|    11990 |  1509 | `			PH7_MemObjRelease(apObj[n]);` |
|    11990 |  1510 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     5996 |  1511 | `		}` |
|     6616 |  1512 | `		SySetRelease(&pCtx->sVar);` |
|     3307 |  1513 | `	}` |
|   541016 |  1514 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1515 | `		ph7_aux_data *aAux;` |
|        - |  1516 | `		void *pChunk;` |
|        - |  1517 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1518 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1519 | `		 */` |
|        9 |  1520 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1521 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1522 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1523 | `			/* Release the chunk */` |
|       25 |  1524 | `			if( pChunk ){` |
|       25 |  1525 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1526 | `			}` |
|       13 |  1527 | `		}` |
|        9 |  1528 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1529 | `	}` |
|   541016 |  1530 |  |
|        - |  1531 | `/*` |
|        - |  1532 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1533 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1534 | ` */` |
|      248 |  1535 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1536 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1537 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1538 | `	)` |
|        2 |  1539 |  |
|      250 |  1540 | `	if( pValue == 0 ){` |
|        - |  1541 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1542 | `		return;` |
|        - |  1543 | `	}` |
|      250 |  1544 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      250 |  1545 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1546 | `		sxu32 n;` |
|      936 |  1547 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      936 |  1548 | `			if( apObj[n] == pValue ){` |
|      250 |  1549 | `				PH7_MemObjRelease(pValue);` |
|      250 |  1550 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1551 | `				/* Mark as released */` |
|      250 |  1552 | `				apObj[n] = 0;` |
|      250 |  1553 | `				break;` |
|        - |  1554 | `			}` |
|      345 |  1555 | `		}` |
|      124 |  1556 | `	}` |
|      126 |  1557 |  |
|        - |  1558 | `/*` |
|        - |  1559 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1560 | ` */` |
|  3185030 |  1561 | `static void VmPopOperand(` |
|        - |  1562 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1563 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1564 | `	)` |
|        2 |  1565 |  |
|  3185032 |  1566 | `	ph7_value *pTos = *ppTos;` |
|  6765672 |  1567 | `	while( nPop > 0 ){` |
|  3580642 |  1568 | `		PH7_MemObjRelease(pTos);` |
|  3580642 |  1569 | `		pTos--;` |
|  3580642 |  1570 | `		nPop--;` |
|        2 |  1571 | `	}` |
|        - |  1572 | `	/* Top of the stack */` |
|  3185032 |  1573 | `	*ppTos = pTos;` |
|  3185032 |  1574 |  |
|        - |  1575 | `/*` |
|        - |  1576 | ` * Reserve a memory object.` |
|        - |  1577 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1578 | ` */` |
|  2991670 |  1579 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1580 |  |
|  2991672 |  1581 | `	ph7_value *pObj = 0;` |
|        - |  1582 | `	VmSlot *pSlot;` |
|        - |  1583 | `	sxu32 nIdx;` |
|        - |  1584 | `	/* Check for a free slot */` |
|  2991672 |  1585 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  2991672 |  1586 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  2991672 |  1587 | `	if( pSlot ){` |
|   852752 |  1588 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   852752 |  1589 | `		nIdx = pSlot->nIdx;` |
|   426375 |  1590 | `	}` |
|  2991672 |  1591 | `	if( pObj == 0 ){` |
|        - |  1592 | `		/* Reserve a new memory object */` |
|  2138922 |  1593 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2138922 |  1594 | `		if( pObj == 0 ){` |
|      ! 0 |  1595 | `			return 0;` |
|        - |  1596 | `		}` |
|  1069460 |  1597 | `	}` |
|        - |  1598 | `	/* Set a null default value */` |
|  2991672 |  1599 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  2991672 |  1600 | `	pObj->nIdx = nIdx;` |
|  2991672 |  1601 | `	return pObj;` |
|  1495837 |  1602 |  |
|        - |  1603 | `/*` |
|        - |  1604 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1605 | ` */` |
|    27448 |  1606 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1607 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1608 | `	const char *zKey,  /* Entry key */` |
|        - |  1609 | `	sxu32 nByte,       /* Key length */` |
|        - |  1610 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1611 | `	)` |
|        2 |  1612 |  |
|        - |  1613 | `	ph7_value sKey;` |
|        - |  1614 | `	sxi32 rc;` |
|    27450 |  1615 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    27450 |  1616 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1617 | `	/* Perform the insertion */` |
|    27450 |  1618 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    27450 |  1619 | `	PH7_MemObjRelease(&sKey);` |
|    27450 |  1620 | `	return rc;` |
|        2 |  1621 |  |
|        - |  1622 | `/*` |
|        - |  1623 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1624 | ` * Return a pointer to the variable value on success.` |
|        - |  1625 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1626 | ` */` |
|  2983444 |  1627 | `static ph7_value * VmExtractMemObj(` |
|        - |  1628 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1629 | `	const SyString *pName, /* Variable name */` |
|        - |  1630 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1631 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1632 | `	)` |
|        2 |  1633 |  |
|  2983446 |  1634 | `	int bNullify = FALSE;` |
|        - |  1635 | `	SyHashEntry *pEntry;` |
|        - |  1636 | `	VmFrame *pFrame;` |
|        - |  1637 | `	ph7_value *pObj;` |
|        - |  1638 | `	sxu32 nIdx;` |
|        - |  1639 | `	sxi32 rc;` |
|        - |  1640 | `	/* Point to the top active frame */` |
|  2983446 |  1641 | `	pFrame = pVm->pFrame;` |
|  2983458 |  1642 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1643 | `		/* Safely ignore the exception frame */` |
|       13 |  1644 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        1 |  1645 | `	}` |
|        - |  1646 | `	/* Perform the lookup */` |
|  2983446 |  1647 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1648 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1649 | `		pName = &sAnnon;` |
|        - |  1650 | `		/* Always nullify the object */` |
|      ! 0 |  1651 | `		bNullify = TRUE;` |
|      ! 0 |  1652 | `		bDup = FALSE;` |
|      ! 0 |  1653 | `	}` |
|        - |  1654 | `	/* Check the superglobals table first */` |
|  2983446 |  1655 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  2983446 |  1656 | `	if( pEntry == 0 ){` |
|        - |  1657 | `		/* Query the top active frame */` |
|  2983410 |  1658 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  2983410 |  1659 | `		if( pEntry == 0 ){` |
|    80642 |  1660 | `			char *zName = (char *)pName->zString;` |
|        - |  1661 | `			VmSlot sLocal;` |
|    80642 |  1662 | `			if( !bCreate ){` |
|        - |  1663 | `				/* Do not create the variable,return NULL instead */` |
|      632 |  1664 | `				return 0;` |
|        - |  1665 | `			}` |
|        - |  1666 | `			/* No such variable,automatically create a new one and install` |
|        - |  1667 | `			 * it in the current frame.` |
|        - |  1668 | `			 */` |
|    80012 |  1669 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    80012 |  1670 | `			if( pObj == 0 ){` |
|      ! 0 |  1671 | `				return 0;` |
|        - |  1672 | `			}` |
|    80012 |  1673 | `			nIdx = pObj->nIdx;` |
|    80012 |  1674 | `			if( bDup ){` |
|        - |  1675 | `				/* Duplicate name */` |
|      164 |  1676 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      164 |  1677 | `				if( zName == 0 ){` |
|      ! 0 |  1678 | `					return 0;` |
|        - |  1679 | `				}` |
|       81 |  1680 | `			}` |
|        - |  1681 | `			/* Link to the top active VM frame */` |
|    80012 |  1682 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    80012 |  1683 | `			if( rc != SXRET_OK ){` |
|        - |  1684 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1685 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1686 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1687 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1688 | `				return 0;` |
|        - |  1689 | `			}` |
|    80012 |  1690 | `			if( pFrame->pParent != 0 ){` |
|        - |  1691 | `				/* Local variable */` |
|    74368 |  1692 | `				sLocal.nIdx = nIdx;` |
|    74368 |  1693 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    37185 |  1694 | `			}else{` |
|        - |  1695 | `				/* Register in the $GLOBALS array */` |
|     5646 |  1696 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1697 | `			}` |
|        - |  1698 | `			/* Install in the reference table */` |
|    80012 |  1699 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1700 | `			/* Save object index */` |
|    80012 |  1701 | `			pObj->nIdx = nIdx;` |
|    40007 |  1702 | `		}else{` |
|        - |  1703 | `			/* Extract variable contents */` |
|  2902770 |  1704 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  2902770 |  1705 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2902770 |  1706 | `			if( bNullify && pObj ){` |
|      ! 0 |  1707 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1708 | `			}` |
|        - |  1709 | `		}` |
|  1491501 |  1710 | `	}else{` |
|        - |  1711 | `		/* Superglobal */` |
|       38 |  1712 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1713 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1714 | `	}` |
|  2982816 |  1715 | `	return pObj;` |
|  1491834 |  1716 |  |
|        - |  1717 | `/*` |
|        - |  1718 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1719 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1720 | ` */` |
|     2204 |  1721 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1722 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1723 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1724 | `	sxu32 nByte        /* zName length */` |
|        - |  1725 | `	)` |
|        2 |  1726 |  |
|        - |  1727 | `	SyHashEntry *pEntry;` |
|        - |  1728 | `	ph7_value *pValue;` |
|        - |  1729 | `	sxu32 nIdx;` |
|        - |  1730 | `	/* Query the superglobal table */` |
|     2206 |  1731 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2206 |  1732 | `	if( pEntry == 0 ){` |
|        - |  1733 | `		/* No such entry */` |
|      ! 0 |  1734 | `		return 0;` |
|        - |  1735 | `	}` |
|        - |  1736 | `	/* Extract the superglobal index in the global object pool */` |
|     2206 |  1737 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1738 | `	/* Extract the variable value  */` |
|     2206 |  1739 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2206 |  1740 | `	return pValue;` |
|     1104 |  1741 |  |
|        - |  1742 | `/*` |
|        - |  1743 | ` * Perform a raw hashmap insertion.` |
|        - |  1744 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1745 | ` */` |
|     2202 |  1746 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1747 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1748 | `	const char *zKey,   /* Entry key */` |
|        - |  1749 | `	int nKeylen,        /* zKey length*/` |
|        - |  1750 | `	const char *zData,  /* Entry data */` |
|        - |  1751 | `	int nLen            /* zData length */` |
|        - |  1752 | `	)` |
|        2 |  1753 |  |
|        - |  1754 | `	ph7_value sKey,sValue;` |
|        - |  1755 | `	sxi32 rc;` |
|     2204 |  1756 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2204 |  1757 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2204 |  1758 | `	if( zKey ){` |
|     2182 |  1759 | `		if( nKeylen < 0 ){` |
|     2182 |  1760 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1090 |  1761 | `		}` |
|     2182 |  1762 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1090 |  1763 | `	}` |
|     2204 |  1764 | `	if( zData ){` |
|     2204 |  1765 | `		if( nLen < 0 ){` |
|        - |  1766 | `			/* Compute length automatically */` |
|      ! 0 |  1767 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1768 | `		}` |
|     2204 |  1769 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1101 |  1770 | `	}` |
|        - |  1771 | `	/* Perform the insertion */` |
|     2204 |  1772 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2204 |  1773 | `	PH7_MemObjRelease(&sKey);` |
|     2204 |  1774 | `	PH7_MemObjRelease(&sValue);` |
|     2204 |  1775 | `	return rc;` |
|        2 |  1776 |  |
|        - |  1777 | `/*` |
|        - |  1778 | ` * Configure a working virtual machine instance.` |
|        - |  1779 | ` *` |
|        - |  1780 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1781 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1782 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1783 | ` * The second argument to this function is an integer configuration option` |
|        - |  1784 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1785 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1786 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1787 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1788 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1789 | ` */` |
|    34872 |  1790 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1791 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1792 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1793 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1794 | `	)` |
|        2 |  1795 |  |
|    34874 |  1796 | `	sxi32 rc = SXRET_OK;` |
|    34874 |  1797 | `	switch(nOp){` |
|     1089 |  1798 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2180 |  1799 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2180 |  1800 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1801 | `		/* VM output consumer callback */` |
|        - |  1802 | `#ifdef UNTRUST` |
|        - |  1803 | `		if( xConsumer == 0 ){` |
|        - |  1804 | `			rc = SXERR_CORRUPT;` |
|        - |  1805 | `			break;` |
|        - |  1806 | `		}` |
|        - |  1807 | `#endif` |
|        - |  1808 | `		/* Install the output consumer */` |
|     2180 |  1809 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2180 |  1810 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2180 |  1811 | `		break;` |
|        - |  1812 | `							   }` |
|     1089 |  1813 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1814 | `		/* Import path */` |
|        - |  1815 | `		  const char *zPath;` |
|        - |  1816 | `		  SyString sPath;` |
|     2180 |  1817 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1818 | `#if defined(UNTRUST)` |
|        - |  1819 | `		  if( zPath == 0 ){` |
|        - |  1820 | `			  rc = SXERR_EMPTY;` |
|        - |  1821 | `			  break;` |
|        - |  1822 | `		  }` |
|        - |  1823 | `#endif` |
|     2180 |  1824 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1825 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1826 | `#ifdef __WINNT__` |
|        2 |  1827 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1828 | `#endif` |
|     4358 |  1829 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1830 | `		  /* Remove leading and trailing white spaces */` |
|     2180 |  1831 | `		  SyStringFullTrim(&sPath);` |
|     2180 |  1832 | `		  if( sPath.nByte > 0 ){` |
|        - |  1833 | `			  /* Store the path in the corresponding conatiner */` |
|     2180 |  1834 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1089 |  1835 | `		  }` |
|     2180 |  1836 | `		  break;` |
|        - |  1837 | `									 }` |
|     1089 |  1838 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1839 | `		/* Run-Time Error report */` |
|     2180 |  1840 | `		pVm->bErrReport = 1;` |
|     2180 |  1841 | `		break;` |
|      ! 0 |  1842 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1843 | `		/* Recursion depth */` |
|      ! 0 |  1844 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1845 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1846 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1847 | `		}` |
|      ! 0 |  1848 | `		break;` |
|        - |  1849 | `									   }` |
|      ! 0 |  1850 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1851 | `		/* VM output length in bytes */` |
|      ! 0 |  1852 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1853 | `#ifdef UNTRUST` |
|        - |  1854 | `		if( pOut == 0 ){` |
|        - |  1855 | `			rc = SXERR_CORRUPT;` |
|        - |  1856 | `			break;` |
|        - |  1857 | `		}` |
|        - |  1858 | `#endif` |
|      ! 0 |  1859 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1860 | `		break;` |
|        - |  1861 | `							   }` |
|        - |  1862 |  |
|    10890 |  1863 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1864 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1865 | `		/* Create a new superglobal/global variable */` |
|    21782 |  1866 | `		const char *zName = va_arg(ap,const char *);` |
|    21782 |  1867 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1868 | `		SyHashEntry *pEntry;` |
|        - |  1869 | `		ph7_value *pObj;` |
|        - |  1870 | `		sxu32 nByte;` |
|        - |  1871 | `		sxu32 nIdx;` |
|        - |  1872 | `#ifdef UNTRUST` |
|        - |  1873 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1874 | `			rc = SXERR_CORRUPT;` |
|        - |  1875 | `			break;` |
|        - |  1876 | `		}` |
|        - |  1877 | `#endif` |
|    21782 |  1878 | `		nByte = SyStrlen(zName);` |
|    21782 |  1879 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1880 | `			/* Check if the superglobal is already installed */` |
|    21782 |  1881 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    10892 |  1882 | `		}else{` |
|        - |  1883 | `			/* Query the top active VM frame */` |
|      ! 0 |  1884 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1885 | `		}` |
|    21782 |  1886 | `		if( pEntry ){` |
|        - |  1887 | `			/* Variable already installed */` |
|      ! 0 |  1888 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1889 | `			/* Extract contents */` |
|      ! 0 |  1890 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  1891 | `			if( pObj ){` |
|        - |  1892 | `				/* Overwrite old contents */` |
|      ! 0 |  1893 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  1894 | `			}` |
|      ! 0 |  1895 | `		}else{` |
|        - |  1896 | `			/* Install a new variable */` |
|    21782 |  1897 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    21782 |  1898 | `			if( pObj == 0 ){` |
|      ! 0 |  1899 | `				rc = SXERR_MEM;` |
|      ! 0 |  1900 | `				break;` |
|        - |  1901 | `			}` |
|    21782 |  1902 | `			nIdx = pObj->nIdx;` |
|        - |  1903 | `			/* Copy value */` |
|    21782 |  1904 | `			PH7_MemObjStore(pValue,pObj);` |
|    21782 |  1905 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1906 | `				/* Install the superglobal */` |
|    21782 |  1907 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    10892 |  1908 | `			}else{` |
|        - |  1909 | `				/* Install in the current frame */` |
|      ! 0 |  1910 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1911 | `			}` |
|    21782 |  1912 | `			if( rc == SXRET_OK ){` |
|        - |  1913 | `				SyHashEntry *pRef;` |
|    21782 |  1914 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    21782 |  1915 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    10892 |  1916 | `				}else{` |
|      ! 0 |  1917 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1918 | `				}` |
|        - |  1919 | `				/* Install in the reference table */` |
|    21782 |  1920 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    21782 |  1921 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1922 | `					/* Register in the $GLOBALS array */` |
|    21782 |  1923 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    10890 |  1924 | `				}` |
|    10890 |  1925 | `			}` |
|        - |  1926 | `		}` |
|    21782 |  1927 | `		break;` |
|        - |  1928 | `									}` |
|     1090 |  1929 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1930 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1931 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1932 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1933 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1934 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1935 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2182 |  1936 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2182 |  1937 | `		const char *zValue = va_arg(ap,const char *);` |
|     2182 |  1938 | `		int nLen = va_arg(ap,int);` |
|        - |  1939 | `		ph7_hashmap *pMap;` |
|        - |  1940 | `		ph7_value *pValue;` |
|     2182 |  1941 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1942 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1943 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2181 |  1944 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  1945 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  1946 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2180 |  1947 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  1948 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  1949 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2180 |  1950 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  1951 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  1952 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2180 |  1953 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  1954 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  1955 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2180 |  1956 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  1957 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  1958 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  1959 | `		}else{` |
|        - |  1960 | `			/* Extract the $_SERVER superglobal */` |
|     2180 |  1961 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  1962 | `		}` |
|     2182 |  1963 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1964 | `			/* No such entry */` |
|      ! 0 |  1965 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1966 | `			break;` |
|        - |  1967 | `		}` |
|        - |  1968 | `		/* Point to the hashmap */` |
|     2182 |  1969 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1970 | `		/* Perform the insertion */` |
|     2182 |  1971 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2182 |  1972 | `		break;` |
|        - |  1973 | `								   }` |
|       11 |  1974 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  1975 | `		/* Script arguments */` |
|       24 |  1976 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  1977 | `		ph7_hashmap *pMap;` |
|        - |  1978 | `		ph7_value *pValue;` |
|        - |  1979 | `		sxu32 n;` |
|       24 |  1980 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  1981 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  1982 | `			break;` |
|        - |  1983 | `		}` |
|        - |  1984 | `		/* Extract the $argv array */` |
|       24 |  1985 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  1986 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1987 | `			/* No such entry */` |
|      ! 0 |  1988 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1989 | `			break;` |
|        - |  1990 | `		}` |
|        - |  1991 | `		/* Point to the hashmap */` |
|       24 |  1992 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1993 | `		/* Perform the insertion */` |
|       24 |  1994 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  1995 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  1996 | `		if( rc == SXRET_OK ){` |
|       24 |  1997 | `			if( pMap->nEntry > 1 ){` |
|        - |  1998 | `				/* Append space separator first */` |
|       18 |  1999 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2000 | `			}` |
|       24 |  2001 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2002 | `		}` |
|       24 |  2003 | `		break;` |
|        - |  2004 | `								  }` |
|      ! 0 |  2005 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2006 | `		/* error_log() consumer */` |
|      ! 0 |  2007 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2008 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2009 | `		break;` |
|        - |  2010 | `										}` |
|      ! 0 |  2011 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2012 | `		/* Script return value */` |
|      ! 0 |  2013 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2014 | `#ifdef UNTRUST` |
|        - |  2015 | `		if( ppValue == 0 ){` |
|        - |  2016 | `			rc = SXERR_CORRUPT;` |
|        - |  2017 | `			break;` |
|        - |  2018 | `		}` |
|        - |  2019 | `#endif` |
|      ! 0 |  2020 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2021 | `		break;` |
|        - |  2022 | `								   }` |
|     2178 |  2023 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2024 | `		/* Register an IO stream device */` |
|     4358 |  2025 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2026 | `		/* Make sure we are dealing with a valid IO stream */` |
|     6534 |  2027 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4358 |  2028 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2029 | `				/* Invalid stream */` |
|      ! 0 |  2030 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2031 | `				break;` |
|        - |  2032 | `		}` |
|     4358 |  2033 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2034 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2180 |  2035 | `			pVm->pDefStream = pStream;` |
|     1089 |  2036 | `		}` |
|        - |  2037 | `		/* Insert in the appropriate container */` |
|     4358 |  2038 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4358 |  2039 | `		break;` |
|        - |  2040 | `								  }` |
|      ! 0 |  2041 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2042 | `		/* Point to the VM internal output consumer buffer */` |
|      ! 0 |  2043 | `		const void **ppOut = va_arg(ap,const void **);` |
|      ! 0 |  2044 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2045 | `#ifdef UNTRUST` |
|        - |  2046 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2047 | `			rc = SXERR_CORRUPT;` |
|        - |  2048 | `			break;` |
|        - |  2049 | `		}` |
|        - |  2050 | `#endif` |
|      ! 0 |  2051 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|      ! 0 |  2052 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|      ! 0 |  2053 | `		break;` |
|        - |  2054 | `									   }` |
|      ! 0 |  2055 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2056 | `		/* Raw HTTP request*/` |
|      ! 0 |  2057 | `		const char *zRequest = va_arg(ap,const char *);` |
|      ! 0 |  2058 | `		int nByte = va_arg(ap,int);` |
|      ! 0 |  2059 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2060 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2061 | `			break;` |
|        - |  2062 | `		}` |
|      ! 0 |  2063 | `		if( nByte < 0 ){` |
|        - |  2064 | `			/* Compute length automatically */` |
|      ! 0 |  2065 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2066 | `		}` |
|        - |  2067 | `		/* Process the request */` |
|      ! 0 |  2068 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|      ! 0 |  2069 | `		break;` |
|        - |  2070 | `									}` |
|      ! 0 |  2071 | `	default:` |
|        - |  2072 | `		/* Unknown configuration option */` |
|      ! 0 |  2073 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2074 | `		break;` |
|        - |  2075 | `	}` |
|    34874 |  2076 | `	return rc;` |
|        2 |  2077 |  |
|        - |  2078 | `/* Forward declaration */` |
|        - |  2079 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2080 | `/*` |
|        - |  2081 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2082 | ` * format.` |
|        - |  2083 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2084 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2085 | ` * (STDOUT).` |
|        - |  2086 | ` */` |
|        2 |  2087 | `static sxi32 VmByteCodeDump(` |
|        - |  2088 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2089 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2090 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2091 | `	)` |
|        1 |  2092 |  |
|        - |  2093 | `	static const char zDump[] = {` |
|        - |  2094 | `		"====================================================\n"` |
|        - |  2095 | `		"PH7 VM Dump\n"` |
|        - |  2096 | `		"====================================================\n"` |
|        - |  2097 | `	};` |
|        - |  2098 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2099 | `	sxi32 rc = SXRET_OK;` |
|        - |  2100 | `	sxu32 n;` |
|        - |  2101 | `	/* Point to the PH7 instructions */` |
|        3 |  2102 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2103 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2104 | `	n = 0;` |
|        3 |  2105 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2106 | `	/* Dump instructions */` |
|        7 |  2107 | `	for(;;){` |
|       15 |  2108 | `		if( pInstr >= pEnd ){` |
|        - |  2109 | `			/* No more instructions */` |
|        3 |  2110 | `			break;` |
|        - |  2111 | `		}` |
|        - |  2112 | `		/* Format and call the consumer callback */` |
|       19 |  2113 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2114 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2115 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2116 | `		if( rc != SXRET_OK ){` |
|        - |  2117 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2118 | `			return rc;` |
|        - |  2119 | `		}` |
|       13 |  2120 | `		++n;` |
|       13 |  2121 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2122 | `	}` |
|        3 |  2123 | `	return rc;` |
|        2 |  2124 |  |
|        - |  2125 | `/* Forward declaration */` |
|        - |  2126 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2127 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2128 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2129 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2130 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2131 | `/*` |
|        - |  2132 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2133 | ` * consumer callback.` |
|        - |  2134 | ` */` |
|      542 |  2135 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2136 |  |
|      543 |  2137 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      543 |  2138 | `	sxi32 rc = SXRET_OK;` |
|        - |  2139 | `	/* Append a new line */` |
|        - |  2140 | `#ifdef __WINNT__` |
|        1 |  2141 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2142 | `#else` |
|      542 |  2143 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2144 | `#endif` |
|        - |  2145 | `	/* Invoke the output consumer callback */` |
|      543 |  2146 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      543 |  2147 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  2148 | `		/* Increment output length */` |
|      543 |  2149 | `		pVm->nOutputLen += SyBlobLength(pMsg);` |
|      271 |  2150 | `	}` |
|      543 |  2151 | `	return rc;` |
|        1 |  2152 |  |
|        - |  2153 | `/*` |
|        - |  2154 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2155 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2156 | ` * information.` |
|        - |  2157 | ` */` |
|      130 |  2158 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2159 |  |
|      132 |  2160 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2161 | `		ph7_value apArg[4];` |
|        - |  2162 | `		ph7_value *apArgPtr[4];` |
|        - |  2163 | `		ph7_value sResult;` |
|        - |  2164 | `		SyString sErr;` |
|        - |  2165 | `		/* Prepare arguments */` |
|       61 |  2166 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2167 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2168 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2169 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2170 | `		if( pFile ){` |
|       61 |  2171 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2172 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2173 | `		}else{` |
|      ! 0 |  2174 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2175 | `		}` |
|       61 |  2176 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2177 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2178 | `		/* Set up pointer array */` |
|       61 |  2179 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2180 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2181 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2182 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2183 | `		/* Call the handler */` |
|       61 |  2184 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2185 | `		/* Check return value */` |
|       61 |  2186 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2187 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2188 | `		}` |
|        - |  2189 | `		/* Release */` |
|       61 |  2190 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2191 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2192 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2193 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2194 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2195 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2196 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2197 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2198 | `	}` |
|        - |  2199 | `	/* No handler, always call error handler */` |
|       71 |  2200 | `	return TRUE;` |
|       67 |  2201 |  |
|       94 |  2202 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2203 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2204 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2205 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2206 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2207 | `	)` |
|        2 |  2208 |  |
|       96 |  2209 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2210 | `	SyString *pFile;` |
|        - |  2211 | `	char *zErr;` |
|       96 |  2212 | `	sxi32 rc = SXRET_OK;` |
|       96 |  2213 | `	if( !pVm->bErrReport ){` |
|        - |  2214 | `		/* Don't bother reporting errors */` |
|        3 |  2215 | `		return SXRET_OK;` |
|        - |  2216 | `	}` |
|        - |  2217 | `	/* Reset the working buffer */` |
|       94 |  2218 | `	SyBlobReset(pWorker);` |
|        - |  2219 | `	/* Peek the processed file if available */` |
|       94 |  2220 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       94 |  2221 | `	if( pFile ){` |
|        - |  2222 | `		/* Append file name */` |
|       94 |  2223 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       94 |  2224 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       46 |  2225 | `	}` |
|        - |  2226 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2227 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2228 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2229 | `	 * E_DEPRECATED). */` |
|       94 |  2230 | `	zErr = "Error:  ";` |
|       94 |  2231 | `	switch(iErr){` |
|       17 |  2232 | `	case PH7_CTX_WARNING:` |
|       36 |  2233 | `		zErr = "Warning:  ";` |
|       36 |  2234 | `		break;` |
|        6 |  2235 | `	case PH7_CTX_NOTICE:` |
|       14 |  2236 | `		zErr = "Notice:  ";` |
|       12 |  2237 | `		break;` |
|       23 |  2238 | `	default:` |
|        - |  2239 | `		/* keep iErr unchanged */` |
|       46 |  2240 | `		break;` |
|        - |  2241 | `	}` |
|       94 |  2242 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       94 |  2243 | `	if( pFuncName ){` |
|        - |  2244 | `		/* Append function name first */` |
|       21 |  2245 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       21 |  2246 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       10 |  2247 | `	}` |
|       94 |  2248 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2249 | `	/* Check for user error handler.  compute length of C string */` |
|       94 |  2250 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       45 |  2251 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       22 |  2252 | `	}` |
|       94 |  2253 | `	return rc;` |
|       49 |  2254 |  |
|        - |  2255 | `/*` |
|        - |  2256 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2257 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2258 | ` * information.` |
|        - |  2259 | ` */` |
|       38 |  2260 | `static sxi32 VmThrowErrorAp(` |
|        - |  2261 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2262 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2263 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2264 | `	const char *zFormat, /* Format message */` |
|        - |  2265 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2266 | `	)` |
|        2 |  2267 |  |
|       40 |  2268 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2269 | `	SyBlob sMsg;` |
|        - |  2270 | `	SyString *pFile;` |
|        - |  2271 | `	char *zErr;` |
|       40 |  2272 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2273 | `	if( !pVm->bErrReport ){` |
|        - |  2274 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2275 | `		return SXRET_OK;` |
|        - |  2276 | `	}` |
|        - |  2277 | `	/* Reset the working buffer */` |
|       40 |  2278 | `	SyBlobReset(pWorker);` |
|        - |  2279 | `	/* Peek the processed file if available */` |
|       40 |  2280 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2281 | `	if( pFile ){` |
|        - |  2282 | `		/* Append file name */` |
|       40 |  2283 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2284 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2285 | `	}` |
|        - |  2286 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2287 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2288 | `	 * the correct errno value. */` |
|       40 |  2289 | `	zErr = "Error:  ";` |
|       40 |  2290 | `	switch(iErr){` |
|        4 |  2291 | `	case PH7_CTX_WARNING:` |
|        9 |  2292 | `		zErr = "Warning:  ";` |
|        9 |  2293 | `		break;` |
|        3 |  2294 | `	case PH7_CTX_NOTICE:` |
|        7 |  2295 | `		zErr = "Notice:  ";` |
|        6 |  2296 | `		break;` |
|       12 |  2297 | `	default:` |
|        - |  2298 | `		/* do not change iErr */` |
|       24 |  2299 | `		break;` |
|        - |  2300 | `	}` |
|       40 |  2301 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2302 | `	if( pFuncName ){` |
|        - |  2303 | `		/* Append function name first */` |
|       26 |  2304 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2305 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2306 | `	}` |
|        - |  2307 | `	/* Format the raw message */` |
|       40 |  2308 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2309 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2310 | `	/* Check if a user error handler is installed */` |
|       40 |  2311 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2312 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2313 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2314 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2315 | `	}` |
|       40 |  2316 | `	SyBlobRelease(&sMsg);` |
|       40 |  2317 | `	return rc;` |
|       21 |  2318 |  |
|        - |  2319 | `/*` |
|        - |  2320 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2321 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2322 | ` * information.` |
|        - |  2323 | ` * ------------------------------------` |
|        - |  2324 | ` * Simple boring wrapper function.` |
|        - |  2325 | ` * ------------------------------------` |
|        - |  2326 | ` */` |
|       14 |  2327 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2328 |  |
|        - |  2329 | `	va_list ap;` |
|        - |  2330 | `	sxi32 rc;` |
|       15 |  2331 | `	va_start(ap,zFormat);` |
|       15 |  2332 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2333 | `	va_end(ap);` |
|       15 |  2334 | `	return rc;` |
|        1 |  2335 |  |
|        - |  2336 | `/*` |
|        - |  2337 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2338 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2339 | ` * information.` |
|        - |  2340 | ` * ------------------------------------` |
|        - |  2341 | ` * Simple boring wrapper function.` |
|        - |  2342 | ` * ------------------------------------` |
|        - |  2343 | ` */` |
|       24 |  2344 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2345 |  |
|        - |  2346 | `	sxi32 rc;` |
|       26 |  2347 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2348 | `	return rc;` |
|        2 |  2349 |  |
|        - |  2350 | `/*` |
|        - |  2351 | ` * Resolve function context from the current frame.` |
|        - |  2352 | ` */` |
|      934 |  2353 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2354 |  |
|        - |  2355 | `	VmFrame *pFrame;` |
|        - |  2356 | `	ph7_vm_func *pFunc;` |
|      935 |  2357 | `	*pzFuncName = 0;` |
|      935 |  2358 | `	*pnFuncLen = 0;` |
|      935 |  2359 | `	pFrame = pVm->pFrame;` |
|      935 |  2360 | `	if( pFrame == 0 ){` |
|      ! 0 |  2361 | `		return;` |
|        - |  2362 | `	}` |
|      935 |  2363 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  2364 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  2365 | `	}` |
|      935 |  2366 | `	if( pFrame->pParent == 0 ){` |
|      929 |  2367 | `		return;` |
|        - |  2368 | `	}` |
|        7 |  2369 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        7 |  2370 | `	if( pFunc == 0 ){` |
|      ! 0 |  2371 | `		return;` |
|        - |  2372 | `	}` |
|        7 |  2373 | `	*pzFuncName = pFunc->sName.zString;` |
|        7 |  2374 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      468 |  2375 |  |
|        - |  2376 | `/*` |
|        - |  2377 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2378 | ` */` |
|      470 |  2379 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2380 |  |
|        - |  2381 | `	SyBlob sOut;` |
|        - |  2382 | `	SyString *pFile;` |
|      471 |  2383 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2384 | `		return PH7_OK;` |
|        - |  2385 | `	}` |
|      471 |  2386 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2387 | `		zClass = "Exception";` |
|      ! 0 |  2388 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2389 | `	}` |
|      471 |  2390 | `	if( zMsg == 0 ){` |
|      ! 0 |  2391 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2392 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2393 | `	}` |
|      471 |  2394 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      465 |  2395 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      232 |  2396 | `	}` |
|      471 |  2397 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      471 |  2398 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      471 |  2399 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      471 |  2400 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      471 |  2401 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      471 |  2402 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      471 |  2403 | `	if( pFile ){` |
|      471 |  2404 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      471 |  2405 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2406 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      235 |  2407 | `	}` |
|      471 |  2408 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      471 |  2409 | `	if( pFile ){` |
|      471 |  2410 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      471 |  2411 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2412 | `		if( zFuncName && nFuncLen > 0 ){` |
|        7 |  2413 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        4 |  2414 | `		}else{` |
|      465 |  2415 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2416 | `		}` |
|      235 |  2417 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2418 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2419 | `	}else{` |
|      ! 0 |  2420 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2421 | `	}` |
|      471 |  2422 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      471 |  2423 | `	if( pFile ){` |
|      471 |  2424 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      471 |  2425 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      471 |  2426 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2427 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      235 |  2428 | `	}` |
|      471 |  2429 | `	VmCallErrorHandler(pVm,&sOut);` |
|      471 |  2430 | `	SyBlobRelease(&sOut);` |
|      471 |  2431 | `	return PH7_ABORT;` |
|      236 |  2432 |  |
|        - |  2433 | `/*` |
|        - |  2434 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2435 | ` */` |
|      468 |  2436 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2437 |  |
|        - |  2438 | `	ph7_vm *pVm;` |
|        - |  2439 | `	ph7_class *pClass;` |
|        - |  2440 | `	ph7_class_instance *pThis;` |
|        - |  2441 | `	ph7_class_method *pCons;` |
|        - |  2442 | `	ph7_value sArg;` |
|        - |  2443 | `	ph7_value *apArg[1];` |
|        - |  2444 | `	SyBlob sMsg;` |
|        - |  2445 | `	SyString sMsgStr;` |
|        - |  2446 | `	VmFrame *pFrame;` |
|        - |  2447 | `	va_list ap;` |
|        - |  2448 | `	sxi32 rc;` |
|        - |  2449 |  |
|      470 |  2450 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2451 | `		return PH7_ABORT;` |
|        - |  2452 | `	}` |
|      470 |  2453 | `	pVm = pCtx->pVm;` |
|      470 |  2454 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2455 | `		zClass = "Error";` |
|      ! 0 |  2456 | `	}` |
|      470 |  2457 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      470 |  2458 | `	if( pClass == 0 ){` |
|      ! 0 |  2459 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2460 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2461 | `			zClass` |
|        - |  2462 | `			);` |
|        - |  2463 | `	}` |
|      470 |  2464 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      470 |  2465 | `	if( pThis == 0 ){` |
|      ! 0 |  2466 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2467 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2468 | `			);` |
|        - |  2469 | `	}` |
|        - |  2470 |  |
|      470 |  2471 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      470 |  2472 | `	va_start(ap,zFormat);` |
|      470 |  2473 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      470 |  2474 | `	va_end(ap);` |
|        - |  2475 |  |
|      470 |  2476 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      470 |  2477 | `	if( pCons ){` |
|      470 |  2478 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      470 |  2479 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      470 |  2480 | `		apArg[0] = &sArg;` |
|      470 |  2481 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      470 |  2482 | `		PH7_MemObjRelease(&sArg);` |
|      234 |  2483 | `	}` |
|      470 |  2484 | `	SyBlobRelease(&sMsg);` |
|        - |  2485 |  |
|      470 |  2486 | `	pFrame = pVm->pFrame;` |
|      470 |  2487 | `	if( pFrame ){` |
|      476 |  2488 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        7 |  2489 | `			pFrame = pFrame->pParent;` |
|        1 |  2490 | `		}` |
|      470 |  2491 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      234 |  2492 | `	}` |
|      470 |  2493 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      470 |  2494 | `	PH7_ClassInstanceUnref(pThis);` |
|      470 |  2495 | `	if( rc == SXERR_ABORT ){` |
|      463 |  2496 | `		return PH7_ABORT;` |
|        - |  2497 | `	}` |
|        7 |  2498 | `	return PH7_EXCEPTION;` |
|      236 |  2499 |  |
|        - |  2500 | `/*` |
|        - |  2501 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2502 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2503 | ` */` |
|      ! 0 |  2504 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2505 |  |
|        - |  2506 | `	ph7_vm *pVm;` |
|        - |  2507 | `	SyBlob sMsg;` |
|      ! 0 |  2508 | `	const char *zFuncName = 0;` |
|      ! 0 |  2509 | `	int nFuncLen = 0;` |
|        - |  2510 | `	va_list ap;` |
|        - |  2511 | `	sxi32 rc;` |
|        - |  2512 |  |
|      ! 0 |  2513 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2514 | `		return PH7_OK;` |
|        - |  2515 | `	}` |
|      ! 0 |  2516 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2517 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2518 | `		zClass = "Error";` |
|      ! 0 |  2519 | `	}` |
|        - |  2520 |  |
|      ! 0 |  2521 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2522 |  |
|      ! 0 |  2523 | `	va_start(ap,zFormat);` |
|      ! 0 |  2524 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2525 | `	va_end(ap);` |
|        - |  2526 |  |
|      ! 0 |  2527 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2528 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2529 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2530 | `	}` |
|      ! 0 |  2531 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2532 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2533 | `	}` |
|      ! 0 |  2534 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2535 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2536 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2537 | `	return rc;` |
|      ! 0 |  2538 |  |
|        - |  2539 | `/*` |
|        - |  2540 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2541 | ` *` |
|        - |  2542 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2543 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2544 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2545 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2546 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2547 | ` * then the program execution is halted.` |
|        - |  2548 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2549 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2550 | ` * or to reset the VM to it's initial state.` |
|        - |  2551 | ` */` |
|    30170 |  2552 | `static sxi32 VmByteCodeExec(` |
|        - |  2553 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2554 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2555 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2556 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2557 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2558 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2559 | `	int is_callback      /* TRUE if we are executing a callback */` |
|        - |  2560 | `	)` |
|        2 |  2561 |  |
|        - |  2562 | `	VmInstr *pInstr;` |
|        - |  2563 | `	ph7_value *pTos;` |
|        - |  2564 | `	SySet aArg;` |
|        - |  2565 | `	sxi32 pc;` |
|        - |  2566 | `	sxi32 rc;` |
|        - |  2567 | `	/* Argument container */` |
|    30172 |  2568 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    30172 |  2569 | `	if( nTos < 0 ){` |
|    28576 |  2570 | `		pTos = &pStack[-1];` |
|    14289 |  2571 | `	}else{` |
|     1598 |  2572 | `		pTos = &pStack[nTos];` |
|        - |  2573 | `	}` |
|    30172 |  2574 | `	pc = 0;` |
|        - |  2575 | `	/* Execute as much as we can */` |
|  4771594 |  2576 | `	for(;;){` |
|        - |  2577 | `		/* Fetch the instruction to execute */` |
|  9542486 |  2578 | `		pInstr = &aInstr[pc];` |
|  9542486 |  2579 | `		rc = SXRET_OK;` |
|        - |  2580 | `/*` |
|        - |  2581 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2582 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2583 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2584 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2585 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2586 | ` */` |
|  9542486 |  2587 | `		switch(pInstr->iOp){` |
|        - |  2588 | `/*` |
|        - |  2589 | ` * DONE: P1 * *` |
|        - |  2590 | ` *` |
|        - |  2591 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2592 | ` * and return immediately.` |
|        - |  2593 | ` */` |
|    14842 |  2594 | `case PH7_OP_DONE:` |
|    29686 |  2595 | `	if( pInstr->iP1 ){` |
|        - |  2596 | `#ifdef UNTRUST` |
|        - |  2597 | `		if( pTos < pStack ){` |
|        - |  2598 | `			goto Abort;` |
|        - |  2599 | `		}` |
|        - |  2600 | `#endif` |
|    17128 |  2601 | `		if( pLastRef ){` |
|    11160 |  2602 | `			*pLastRef = pTos->nIdx;` |
|     5579 |  2603 | `		}` |
|    17128 |  2604 | `		if( pResult ){` |
|        - |  2605 | `			/* Execution result */` |
|    16332 |  2606 | `			PH7_MemObjStore(pTos,pResult);` |
|     8165 |  2607 | `		}` |
|    17128 |  2608 | `		VmPopOperand(&pTos,1);` |
|    21123 |  2609 | `	}else if( pLastRef ){` |
|        - |  2610 | `		/* Nothing referenced */` |
|      894 |  2611 | `		*pLastRef = SXU32_HIGH;` |
|      446 |  2612 | `	}` |
|    29686 |  2613 | `	goto Done;` |
|        - |  2614 | `/*` |
|        - |  2615 | ` * HALT: P1 * *` |
|        - |  2616 | ` *` |
|        - |  2617 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2618 | ` * and abort immediately.` |
|        - |  2619 | ` */` |
|        4 |  2620 | `case PH7_OP_HALT:` |
|        9 |  2621 | `	if( pInstr->iP1 ){` |
|        - |  2622 | `#ifdef UNTRUST` |
|        - |  2623 | `		if( pTos < pStack ){` |
|        - |  2624 | `			goto Abort;` |
|        - |  2625 | `		}` |
|        - |  2626 | `#endif` |
|        9 |  2627 | `		if( pLastRef ){` |
|      ! 0 |  2628 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2629 | `		}` |
|        9 |  2630 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2631 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2632 | `				/* Output the exit message */` |
|        7 |  2633 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2634 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2635 | `				if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  2636 | `					/* Increment output length */` |
|        5 |  2637 | `					pVm->nOutputLen += SyBlobLength(&pTos->sBlob);` |
|        2 |  2638 | `				}` |
|        3 |  2639 | `			}` |
|        7 |  2640 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2641 | `			/* Record exit status */` |
|        5 |  2642 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2643 | `		}` |
|        9 |  2644 | `		VmPopOperand(&pTos,1);` |
|        4 |  2645 | `	}else if( pLastRef ){` |
|        - |  2646 | `		/* Nothing referenced */` |
|      ! 0 |  2647 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2648 | `	}` |
|        - |  2649 | `	/* Check if we're in an included file context */` |
|        9 |  2650 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2651 | `		/* Terminate the entire process */` |
|        9 |  2652 | `		exit(pVm->iExitStatus);` |
|        - |  2653 | `	}` |
|      ! 0 |  2654 | `	goto Abort;` |
|        - |  2655 | `/*` |
|        - |  2656 | ` * JMP: * P2 *` |
|        - |  2657 | ` *` |
|        - |  2658 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2659 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2660 | ` */` |
|   206286 |  2661 | `case PH7_OP_JMP:` |
|   412618 |  2662 | `	pc = pInstr->iP2 - 1;` |
|   412618 |  2663 | `	break;` |
|        - |  2664 | `/*` |
|        - |  2665 | ` * JZ: P1 P2 *` |
|        - |  2666 | ` *` |
|        - |  2667 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2668 | ` * entry in the stack if P1 is zero.` |
|        - |  2669 | ` */` |
|   480365 |  2670 | `case PH7_OP_JZ:` |
|        - |  2671 | `#ifdef UNTRUST` |
|        - |  2672 | `	if( pTos < pStack ){` |
|        - |  2673 | `		goto Abort;` |
|        - |  2674 | `	}` |
|        - |  2675 | `#endif` |
|        - |  2676 | `	/* Get a boolean value */` |
|   960820 |  2677 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      138 |  2678 | `		PH7_MemObjToBool(pTos);` |
|       68 |  2679 | `	}` |
|   960820 |  2680 | `	if( !pTos->x.iVal ){` |
|        - |  2681 | `		/* Take the jump */` |
|   482156 |  2682 | `		pc = pInstr->iP2 - 1;` |
|   241077 |  2683 | `	}` |
|   960820 |  2684 | `	if( !pInstr->iP1 ){` |
|   766050 |  2685 | `		VmPopOperand(&pTos,1);` |
|   383046 |  2686 | `	}` |
|   960820 |  2687 | `	break;` |
|        - |  2688 | `/*` |
|        - |  2689 | ` * JNZ: P1 P2 *` |
|        - |  2690 | ` *` |
|        - |  2691 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2692 | ` * entry in the stack if P1 is zero.` |
|        - |  2693 | ` */` |
|    51995 |  2694 | `case PH7_OP_JNZ:` |
|        - |  2695 | `#ifdef UNTRUST` |
|        - |  2696 | `	if( pTos < pStack ){` |
|        - |  2697 | `		goto Abort;` |
|        - |  2698 | `	}` |
|        - |  2699 | `#endif` |
|        - |  2700 | `	/* Get a boolean value */` |
|   103992 |  2701 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2702 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2703 | `	}` |
|   103992 |  2704 | `	if( pTos->x.iVal ){` |
|        - |  2705 | `		/* Take the jump */` |
|     4246 |  2706 | `		pc = pInstr->iP2 - 1;` |
|     2122 |  2707 | `	}` |
|   103992 |  2708 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2709 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2710 | `	}` |
|   103992 |  2711 | `	break;` |
|        - |  2712 | `/*` |
|        - |  2713 | ` * NOOP: * * *` |
|        - |  2714 | ` *` |
|        - |  2715 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2716 | ` * destination.` |
|        - |  2717 | ` */` |
|      ! 0 |  2718 | `case PH7_OP_NOOP:` |
|      ! 0 |  2719 | `	break;` |
|        - |  2720 | `/*` |
|        - |  2721 | ` * POP: P1 * *` |
|        - |  2722 | ` *` |
|        - |  2723 | ` * Pop P1 elements from the operand stack.` |
|        - |  2724 | ` */` |
|   374964 |  2725 | `case PH7_OP_POP: {` |
|   749974 |  2726 | `	sxi32 n = pInstr->iP1;` |
|   749974 |  2727 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2728 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2729 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2730 | `	}` |
|   749974 |  2731 | `	VmPopOperand(&pTos,n);` |
|   749974 |  2732 | `	break;` |
|        - |  2733 | `				 }` |
|        - |  2734 | `/*` |
|        - |  2735 | ` * DUP: * * *` |
|        - |  2736 | ` *` |
|        - |  2737 | ` * Duplicate the top of the stack.` |
|        - |  2738 | ` */` |
|       33 |  2739 | `case PH7_OP_DUP:` |
|        - |  2740 | `#ifdef UNTRUST` |
|        - |  2741 | `	if( pTos < pStack ){` |
|        - |  2742 | `		goto Abort;` |
|        - |  2743 | `	}` |
|        - |  2744 | `#endif` |
|       68 |  2745 | `	pTos++;` |
|       68 |  2746 | `	PH7_MemObjInit(pVm,pTos);` |
|       68 |  2747 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       68 |  2748 | `	break;` |
|        - |  2749 | `/*` |
|        - |  2750 | ` * NSSWITCH: * * P3` |
|        - |  2751 | ` *` |
|        - |  2752 | ` * Switch the active namespace at runtime.` |
|        - |  2753 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  2754 | ` */` |
|     6093 |  2755 | `case PH7_OP_NSSWITCH:` |
|    12188 |  2756 | `	SyBlobReset(&pVm->sNamespace);` |
|    12188 |  2757 | `	if( pInstr->p3 ){` |
|       49 |  2758 | `		const char *zNs = (const char *)pInstr->p3;` |
|       49 |  2759 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       24 |  2760 | `	}` |
|    12188 |  2761 | `	break;` |
|        - |  2762 | `/*` |
|        - |  2763 | ` * CVT_INT: * * *` |
|        - |  2764 | ` *` |
|        - |  2765 | ` * Force the top of the stack to be an integer.` |
|        - |  2766 | ` */` |
|       35 |  2767 | `case PH7_OP_CVT_INT:` |
|        - |  2768 | `#ifdef UNTRUST` |
|        - |  2769 | `	if( pTos < pStack ){` |
|        - |  2770 | `		goto Abort;` |
|        - |  2771 | `	}` |
|        - |  2772 | `#endif` |
|       72 |  2773 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2774 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2775 | `	}` |
|        - |  2776 | `	/* Invalidate any prior representation */` |
|       72 |  2777 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2778 | `	break;` |
|        - |  2779 | `/*` |
|        - |  2780 | ` * CVT_REAL: * * *` |
|        - |  2781 | ` *` |
|        - |  2782 | ` * Force the top of the stack to be a real.` |
|        - |  2783 | ` */` |
|        4 |  2784 | `case PH7_OP_CVT_REAL:` |
|        - |  2785 | `#ifdef UNTRUST` |
|        - |  2786 | `	if( pTos < pStack ){` |
|        - |  2787 | `		goto Abort;` |
|        - |  2788 | `	}` |
|        - |  2789 | `#endif` |
|        9 |  2790 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2791 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2792 | `	}` |
|        - |  2793 | `	/* Invalidate any prior representation */` |
|        9 |  2794 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2795 | `	break;` |
|        - |  2796 | `/*` |
|        - |  2797 | ` * CVT_STR: * * *` |
|        - |  2798 | ` *` |
|        - |  2799 | ` * Force the top of the stack to be a string.` |
|        - |  2800 | ` */` |
|      146 |  2801 | `case PH7_OP_CVT_STR:` |
|        - |  2802 | `#ifdef UNTRUST` |
|        - |  2803 | `	if( pTos < pStack ){` |
|        - |  2804 | `		goto Abort;` |
|        - |  2805 | `	}` |
|        - |  2806 | `#endif` |
|      294 |  2807 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  2808 | `		PH7_MemObjToString(pTos);` |
|      146 |  2809 | `	}` |
|      294 |  2810 | `	break;` |
|        - |  2811 | `/*` |
|        - |  2812 | ` * CVT_BOOL: * * *` |
|        - |  2813 | ` *` |
|        - |  2814 | ` * Force the top of the stack to be a boolean.` |
|        - |  2815 | ` */` |
|        5 |  2816 | `case PH7_OP_CVT_BOOL:` |
|        - |  2817 | `#ifdef UNTRUST` |
|        - |  2818 | `	if( pTos < pStack ){` |
|        - |  2819 | `		goto Abort;` |
|        - |  2820 | `	}` |
|        - |  2821 | `#endif` |
|       11 |  2822 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2823 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2824 | `	}` |
|       11 |  2825 | `	break;` |
|        - |  2826 | `/*` |
|        - |  2827 | ` * CVT_NULL: * * *` |
|        - |  2828 | ` *` |
|        - |  2829 | ` * Nullify the top of the stack.` |
|        - |  2830 | ` */` |
|        3 |  2831 | `case PH7_OP_CVT_NULL:` |
|        - |  2832 | `#ifdef UNTRUST` |
|        - |  2833 | `	if( pTos < pStack ){` |
|        - |  2834 | `		goto Abort;` |
|        - |  2835 | `	}` |
|        - |  2836 | `#endif` |
|        7 |  2837 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2838 | `	break;` |
|        - |  2839 | `/*` |
|        - |  2840 | ` * CVT_NUMC: * * *` |
|        - |  2841 | ` *` |
|        - |  2842 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2843 | ` */` |
|      ! 0 |  2844 | `case PH7_OP_CVT_NUMC:` |
|        - |  2845 | `#ifdef UNTRUST` |
|        - |  2846 | `	if( pTos < pStack ){` |
|        - |  2847 | `		goto Abort;` |
|        - |  2848 | `	}` |
|        - |  2849 | `#endif` |
|        - |  2850 | `	/* Force a numeric cast */` |
|      ! 0 |  2851 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2852 | `	break;` |
|        - |  2853 | `/*` |
|        - |  2854 | ` * CVT_ARRAY: * * *` |
|        - |  2855 | ` *` |
|        - |  2856 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2857 | ` */` |
|       10 |  2858 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2859 | `#ifdef UNTRUST` |
|        - |  2860 | `	if( pTos < pStack ){` |
|        - |  2861 | `		goto Abort;` |
|        - |  2862 | `	}` |
|        - |  2863 | `#endif` |
|        - |  2864 | `	/* Force a hashmap cast */` |
|       21 |  2865 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  2866 | `	if( rc != SXRET_OK ){` |
|        - |  2867 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  2868 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  2869 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  2870 | `	}` |
|       21 |  2871 | `	break;` |
|        - |  2872 | `/*` |
|        - |  2873 | ` * CVT_OBJ: * * *` |
|        - |  2874 | ` *` |
|        - |  2875 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  2876 | ` */` |
|        8 |  2877 | `case PH7_OP_CVT_OBJ:` |
|        - |  2878 | `#ifdef UNTRUST` |
|        - |  2879 | `	if( pTos < pStack ){` |
|        - |  2880 | `		goto Abort;` |
|        - |  2881 | `	}` |
|        - |  2882 | `#endif` |
|       17 |  2883 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  2884 | `		/* Force a 'stdClass()' cast */` |
|       17 |  2885 | `		PH7_MemObjToObject(pTos);` |
|        8 |  2886 | `	}` |
|       17 |  2887 | `	break;` |
|        - |  2888 | `/*` |
|        - |  2889 | ` * ERR_CTRL * * *` |
|        - |  2890 | ` *` |
|        - |  2891 | ` * Error control operator.` |
|        - |  2892 | ` */` |
|    12188 |  2893 | `case PH7_OP_ERR_CTRL:` |
|        - |  2894 | `	/*` |
|        - |  2895 | `	 * TICKET 1433-038:` |
|        - |  2896 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2897 | `	 * use the public API,to control error output.` |
|        - |  2898 | `	 */` |
|    24376 |  2899 | `	break;` |
|        - |  2900 | `/*` |
|        - |  2901 | ` * IS_A * * *` |
|        - |  2902 | ` *` |
|        - |  2903 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  2904 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  2905 | ` * holding a class name or an object).` |
|        - |  2906 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  2907 | ` */` |
|       23 |  2908 | `case PH7_OP_IS_A:{` |
|       48 |  2909 | `	ph7_value *pNos = &pTos[-1];` |
|       48 |  2910 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  2911 | `#ifdef UNTRUST` |
|        - |  2912 | `	if( pNos < pStack ){` |
|        - |  2913 | `		goto Abort;` |
|        - |  2914 | `	}` |
|        - |  2915 | `#endif` |
|       48 |  2916 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       46 |  2917 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       46 |  2918 | `		ph7_class *pClass = 0;` |
|        - |  2919 | `		/* Extract the target class */` |
|       46 |  2920 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  2921 | `			/* Instance already loaded */` |
|      ! 0 |  2922 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       46 |  2923 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       46 |  2924 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       46 |  2925 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  2926 | `			/* Handle self/static/parent keywords */` |
|       46 |  2927 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  2928 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       44 |  2929 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  2930 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       43 |  2931 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  2932 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  2933 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  2934 | `					pClass = pSelf->pBase;` |
|        2 |  2935 | `				}` |
|        3 |  2936 | `			}else{` |
|       36 |  2937 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  2938 | `			}` |
|       22 |  2939 | `		}` |
|       46 |  2940 | `		if( pClass ){` |
|        - |  2941 | `			/* Perform the query */` |
|       46 |  2942 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       22 |  2943 | `		}` |
|       22 |  2944 | `	}` |
|        - |  2945 | `	/* Push result */` |
|       48 |  2946 | `	VmPopOperand(&pTos,1);` |
|       48 |  2947 | `	PH7_MemObjRelease(pTos);` |
|       48 |  2948 | `	pTos->x.iVal = iRes;` |
|       48 |  2949 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       48 |  2950 | `	break;` |
|        - |  2951 | `				 }` |
|        - |  2952 |  |
|        - |  2953 | `/*` |
|        - |  2954 | ` * LOADC P1 P2 *` |
|        - |  2955 | ` *` |
|        - |  2956 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  2957 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  2958 | ` */` |
|   791336 |  2959 | `case PH7_OP_LOADC: {` |
|        - |  2960 | `	ph7_value *pObj;` |
|        - |  2961 | `	/* Reserve a room */` |
|  1582718 |  2962 | `	pTos++;` |
|  2366303 |  2963 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1582718 |  2964 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2965 | `			SyHashEntry *pEntry;` |
|        - |  2966 | `			/* Candidate for expansion via user defined callbacks */` |
|    15594 |  2967 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    15594 |  2968 | `			if( pEntry ){` |
|    15590 |  2969 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2970 | `				/* Set a NULL default value */` |
|    15590 |  2971 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    15590 |  2972 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2973 | `				/* Invoke the callback and deal with the expanded value */` |
|    15590 |  2974 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  2975 | `				/* Mark as constant */` |
|    15590 |  2976 | `				pTos->nIdx = SXU32_HIGH;` |
|    15590 |  2977 | `				break;` |
|        - |  2978 | `			}` |
|        - |  2979 | `			/* Constant not found.  For qualified names (containing '\')` |
|        - |  2980 | `			 * this is always an error — bare unqualified names still fall` |
|        - |  2981 | `			 * through to string value for backward compatibility. */` |
|        - |  2982 | `			{` |
|        6 |  2983 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  2984 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  2985 | `				sxu32 j;` |
|       32 |  2986 | `				for( j = 0; j < nLit; j++ ){` |
|       30 |  2987 | `					if( zLit[j] == '\\' ){` |
|        - |  2988 | `						/* Qualified name: must be a real constant.` |
|        - |  2989 | `						 * Format as PHP Fatal error to match PHP behavior. */` |
|        - |  2990 | `						{` |
|        3 |  2991 | `							SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  2992 | `							SyBlob sErr;` |
|        3 |  2993 | `							SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  2994 | `							SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  2995 | `							if( pErrFile ){` |
|        3 |  2996 | `								SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  2997 | `							}` |
|        3 |  2998 | `							SyBlobAppend(&sErr,"\n",1);` |
|        3 |  2999 | `							VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3000 | `							SyBlobRelease(&sErr);` |
|        - |  3001 | `						}` |
|        3 |  3002 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3003 | `						pTos->nIdx = SXU32_HIGH;` |
|        3 |  3004 | `						goto LoadC_Done;` |
|        - |  3005 | `					}` |
|       15 |  3006 | `				}` |
|        - |  3007 | `			}` |
|        1 |  3008 | `		}` |
|  1567128 |  3009 | `		PH7_MemObjLoad(pObj,pTos);` |
|   783587 |  3010 | `	}else{` |
|        - |  3011 | `		/* Set a NULL value */` |
|      ! 0 |  3012 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3013 | `	}` |
|   783542 |  3014 | `LoadC_Done:` |
|        - |  3015 | `	/* Mark as constant */` |
|  1567130 |  3016 | `	pTos->nIdx = SXU32_HIGH;` |
|  1567130 |  3017 | `	break;` |
|        - |  3018 | `				  }` |
|        - |  3019 | `/*` |
|        - |  3020 | ` * LOAD: P1 * P3` |
|        - |  3021 | ` *` |
|        - |  3022 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3023 | ` * from the P3 operand.` |
|        - |  3024 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3025 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3026 | ` */` |
|  1300966 |  3027 | `case PH7_OP_LOAD:{` |
|        - |  3028 | `	ph7_value *pObj;` |
|        - |  3029 | `	SyString sName;` |
|  2602154 |  3030 | `	if( pInstr->p3 == 0 ){` |
|        - |  3031 | `		/* Take the variable name from the top of the stack */` |
|        - |  3032 | `#ifdef UNTRUST` |
|        - |  3033 | `		if( pTos < pStack ){` |
|        - |  3034 | `			goto Abort;` |
|        - |  3035 | `		}` |
|        - |  3036 | `#endif` |
|        - |  3037 | `		/* Force a string cast */` |
|       19 |  3038 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3039 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3040 | `		}` |
|       19 |  3041 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3042 | `	}else{` |
|  2602136 |  3043 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3044 | `		/* Reserve a room for the target object */` |
|  2602136 |  3045 | `		pTos++;` |
|        - |  3046 | `	}` |
|        - |  3047 | `	/* Extract the requested memory object */` |
|  2602154 |  3048 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2602154 |  3049 | `	if( pObj == 0 ){` |
|      624 |  3050 | `		if( pInstr->iP1 ){` |
|        - |  3051 | `			/* Variable not found,load NULL */` |
|      624 |  3052 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3053 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3054 | `			}else{` |
|      624 |  3055 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3056 | `			}` |
|      624 |  3057 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1301279 |  3058 | `			break;` |
|      ! 0 |  3059 | `		}else{` |
|        - |  3060 | `			/* Fatal error */` |
|      ! 0 |  3061 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3062 | `			goto Abort;` |
|        - |  3063 | `		}` |
|        - |  3064 | `	}` |
|        - |  3065 | `	/* Load variable contents */` |
|  2601532 |  3066 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2601532 |  3067 | `	pTos->nIdx = pObj->nIdx;` |
|  2601532 |  3068 | `	break;` |
|        - |  3069 | `				   }` |
|        - |  3070 | `/*` |
|        - |  3071 | ` * LOAD_MAP P1 * *` |
|        - |  3072 | ` *` |
|        - |  3073 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3074 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3075 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3076 | ` */` |
|    17616 |  3077 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3078 | `	ph7_hashmap *pMap;` |
|        - |  3079 | `	/* Allocate a new hashmap instance */` |
|    35234 |  3080 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    35234 |  3081 | `	if( pMap == 0 ){` |
|      ! 0 |  3082 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3083 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3084 | `		goto Abort;` |
|        - |  3085 | `	}` |
|    35234 |  3086 | `	if( pInstr->iP1 > 0 ){` |
|     2152 |  3087 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3088 | `		/* Perform the insertion */` |
|     6548 |  3089 | `		while( pEntry < pTos ){` |
|     4398 |  3090 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3091 | `				/* Insertion by reference */` |
|      142 |  3092 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3093 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3094 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3095 | `					);` |
|       48 |  3096 | `			}else{` |
|        - |  3097 | `				/* Standard insertion */` |
|     6455 |  3098 | `				PH7_HashmapInsert(pMap,` |
|     4302 |  3099 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2151 |  3100 | `					&pEntry[1]` |
|        - |  3101 | `				);` |
|        - |  3102 | `			}` |
|        - |  3103 | `			/* Next pair on the stack */` |
|     4398 |  3104 | `			pEntry += 2;` |
|        2 |  3105 | `		}` |
|        - |  3106 | `		/* Pop P1 elements */` |
|     2152 |  3107 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1075 |  3108 | `	}` |
|        - |  3109 | `	/* Push the hashmap */` |
|    35234 |  3110 | `	pTos++;` |
|    35234 |  3111 | `	pTos->nIdx = SXU32_HIGH;` |
|    35234 |  3112 | `	pTos->x.pOther = pMap;` |
|    35234 |  3113 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    35234 |  3114 | `	break;` |
|        - |  3115 | `					  }` |
|        - |  3116 | `/*` |
|        - |  3117 | ` * LOAD_LIST: P1 * *` |
|        - |  3118 | ` *` |
|        - |  3119 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3120 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3121 | ` * Caveats:` |
|        - |  3122 | ` *  This implementation support only a single nesting level.` |
|        - |  3123 | ` */` |
|       17 |  3124 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3125 | `	ph7_value *pEntry;` |
|       35 |  3126 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3127 | `		/* Empty list,break immediately */` |
|      ! 0 |  3128 | `		break;` |
|        - |  3129 | `	}` |
|       35 |  3130 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3131 | `#ifdef UNTRUST` |
|        - |  3132 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3133 | `		goto Abort;` |
|        - |  3134 | `	}` |
|        - |  3135 | `#endif` |
|       35 |  3136 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       31 |  3137 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3138 | `		ph7_hashmap_node *pNode;` |
|        - |  3139 | `		ph7_value sKey,*pObj;` |
|        - |  3140 | `		/* Start Copying */` |
|       31 |  3141 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|       99 |  3142 | `		while( pEntry <= pTos ){` |
|       69 |  3143 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       65 |  3144 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       65 |  3145 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       65 |  3146 | `					if( rc == SXRET_OK ){` |
|        - |  3147 | `						/* Store node value */` |
|       65 |  3148 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       33 |  3149 | `					}else{` |
|        - |  3150 | `						/* Nullify the variable */` |
|      ! 0 |  3151 | `						PH7_MemObjRelease(pObj);` |
|        - |  3152 | `					}` |
|       32 |  3153 | `				}` |
|       32 |  3154 | `			}` |
|       69 |  3155 | `			sKey.x.iVal++; /* Next numeric index */` |
|       69 |  3156 | `			pEntry++;` |
|        1 |  3157 | `		}` |
|       15 |  3158 | `	}` |
|       35 |  3159 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       35 |  3160 | `	break;` |
|        - |  3161 | `					   }` |
|        - |  3162 | `/*` |
|        - |  3163 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3164 | ` *` |
|        - |  3165 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3166 | ` * from the stack.` |
|        - |  3167 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3168 | ` * instead.` |
|        - |  3169 | ` */` |
|   209997 |  3170 | `case PH7_OP_LOAD_IDX: {` |
|   420040 |  3171 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   420040 |  3172 | `	ph7_hashmap *pMap = 0;` |
|        - |  3173 | `	ph7_value *pIdx;` |
|   420040 |  3174 | `	pIdx = 0;` |
|   420040 |  3175 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3176 | `		if( !pInstr->iP2){` |
|        - |  3177 | `			/* No available index,load NULL */` |
|      ! 0 |  3178 | `			if( pTos >= pStack ){` |
|      ! 0 |  3179 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3180 | `			}else{` |
|        - |  3181 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3182 | `				pTos++;` |
|      ! 0 |  3183 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3184 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3185 | `			}` |
|        - |  3186 | `			/* Emit a notice */` |
|      ! 0 |  3187 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3188 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3189 | `			break;` |
|        - |  3190 | `		}` |
|      ! 0 |  3191 | `	}else{` |
|   420040 |  3192 | `		pIdx = pTos;` |
|   420040 |  3193 | `		pTos--;` |
|        - |  3194 | `	}` |
|   420040 |  3195 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3196 | `		/* String access */` |
|   332712 |  3197 | `		if( pIdx ){` |
|        - |  3198 | `			sxu32 nOfft;` |
|   332712 |  3199 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3200 | `				/* Force an int cast */` |
|      ! 0 |  3201 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3202 | `			}` |
|   332712 |  3203 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   332712 |  3204 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3205 | `				/* Invalid offset,load null */` |
|      ! 0 |  3206 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3207 | `			}else{` |
|   332712 |  3208 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   332712 |  3209 | `				int c = zData[nOfft];` |
|   332712 |  3210 | `				PH7_MemObjRelease(pTos);` |
|   332712 |  3211 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   332712 |  3212 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3213 | `			}` |
|   166379 |  3214 | `		}else{` |
|        - |  3215 | `			/* No available index,load NULL */` |
|      ! 0 |  3216 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3217 | `		}` |
|   332712 |  3218 | `		break;` |
|        - |  3219 | `	}` |
|    87330 |  3220 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3221 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3222 | `			ph7_value *pObj;` |
|      ! 0 |  3223 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3224 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3225 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3226 | `			}` |
|      ! 0 |  3227 | `		}` |
|      ! 0 |  3228 | `	}` |
|    87330 |  3229 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    87330 |  3230 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3231 | `		/* Point to the hashmap */` |
|    87330 |  3232 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    87330 |  3233 | `		if( pIdx ){` |
|        - |  3234 | `			/* Load the desired entry */` |
|    87330 |  3235 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    43664 |  3236 | `		}` |
|    87330 |  3237 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3238 | `			/* Create a new empty entry */` |
|      ! 0 |  3239 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3240 | `			if( rc == SXRET_OK ){` |
|        - |  3241 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3242 | `				pNode = pMap->pLast;` |
|      ! 0 |  3243 | `			}` |
|      ! 0 |  3244 | `		}` |
|    43664 |  3245 | `	}` |
|    87330 |  3246 | `	if( pIdx ){` |
|    87330 |  3247 | `		PH7_MemObjRelease(pIdx);` |
|    43664 |  3248 | `	}` |
|    87330 |  3249 | `	if( rc == SXRET_OK ){` |
|        - |  3250 | `		/* Load entry contents */` |
|    39882 |  3251 | `		if( pMap->iRef < 2 ){` |
|        - |  3252 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3253 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3254 | `			 */` |
|       24 |  3255 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3256 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3257 | `		}else{` |
|    39860 |  3258 | `			pTos->nIdx = pNode->nValIdx;` |
|    39860 |  3259 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    39860 |  3260 | `			PH7_HashmapUnref(pMap);` |
|        - |  3261 | `		}` |
|    19942 |  3262 | `	}else{` |
|        - |  3263 | `		/* No such entry,load NULL */` |
|    47450 |  3264 | `		PH7_MemObjRelease(pTos);` |
|    47450 |  3265 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3266 | `	}` |
|    87330 |  3267 | `	break;` |
|        - |  3268 | `					  }` |
|        - |  3269 | `/*` |
|        - |  3270 | ` * LOAD_CLOSURE * * P3` |
|        - |  3271 | ` *` |
|        - |  3272 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3273 | ` * name in the stack.` |
|        - |  3274 | ` */` |
|        2 |  3275 | `case PH7_OP_LOAD_CLOSURE:{` |
|        5 |  3276 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        5 |  3277 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3278 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3279 | `		ph7_vm_func *pClosure;` |
|        - |  3280 | `		char *zName;` |
|        - |  3281 | `		sxu32 mLen;` |
|        - |  3282 | `		sxu32 n;` |
|        - |  3283 | `		/* Create a new VM function */` |
|        5 |  3284 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3285 | `		/* Generate an unique closure name */` |
|        5 |  3286 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        5 |  3287 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3288 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3289 | `			goto Abort;` |
|        - |  3290 | `		}` |
|        5 |  3291 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        5 |  3292 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3293 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3294 | `		}` |
|        - |  3295 | `		/* Zero the stucture */` |
|        5 |  3296 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3297 | `		/* Perform a structure assignment on read-only items */` |
|        5 |  3298 | `		pClosure->aArgs = pFunc->aArgs;` |
|        5 |  3299 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        5 |  3300 | `		pClosure->aStatic = pFunc->aStatic;` |
|        5 |  3301 | `		pClosure->iFlags = pFunc->iFlags;` |
|        5 |  3302 | `		pClosure->pUserData = pFunc->pUserData;` |
|        5 |  3303 | `		pClosure->sSignature = pFunc->sSignature;` |
|        5 |  3304 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3305 | `		/* Register the closure */` |
|        5 |  3306 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3307 | `		/* Set up closure environment */` |
|        5 |  3308 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        5 |  3309 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       13 |  3310 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3311 | `			ph7_value *pValue;` |
|        9 |  3312 | `			pEnv = &aEnv[n];` |
|        9 |  3313 | `			sEnv.sName  = pEnv->sName;` |
|        9 |  3314 | `			sEnv.iFlags = pEnv->iFlags;` |
|        9 |  3315 | `			sEnv.nIdx = SXU32_HIGH;` |
|        9 |  3316 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|        9 |  3317 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3318 | `				/* Pass by reference */` |
|      ! 0 |  3319 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3320 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3321 | `					);` |
|      ! 0 |  3322 | `			}` |
|        - |  3323 | `			/* Standard pass by value */` |
|        9 |  3324 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|        9 |  3325 | `			if( pValue ){` |
|        - |  3326 | `				/* Copy imported value */` |
|        5 |  3327 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        2 |  3328 | `			}` |
|        - |  3329 | `			/* Insert the imported variable */` |
|        9 |  3330 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        5 |  3331 | `		}` |
|        - |  3332 | `		/* Finally,load the closure name on the stack */` |
|        5 |  3333 | `		pTos++;` |
|        5 |  3334 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        2 |  3335 | `	}` |
|        5 |  3336 | `	break;` |
|        - |  3337 | `						 }` |
|        - |  3338 | `/*` |
|        - |  3339 | ` * STORE * P2 P3` |
|        - |  3340 | ` *` |
|        - |  3341 | ` * Perform a store (Assignment) operation.` |
|        - |  3342 | ` */` |
|   108403 |  3343 | `case PH7_OP_STORE: {` |
|        - |  3344 | `	ph7_value *pObj;` |
|        - |  3345 | `	SyString sName;` |
|        - |  3346 | `#ifdef UNTRUST` |
|        - |  3347 | `	if( pTos < pStack ){` |
|        - |  3348 | `		goto Abort;` |
|        - |  3349 | `	}` |
|        - |  3350 | `#endif` |
|   216808 |  3351 | `	if( pInstr->iP2 ){` |
|        - |  3352 | `		sxu32 nIdx;` |
|        - |  3353 | `		/* Member store operation */` |
|     2844 |  3354 | `		nIdx = pTos->nIdx;` |
|     2844 |  3355 | `		VmPopOperand(&pTos,1);` |
|     2844 |  3356 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3357 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3358 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3359 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3360 | `		}else{` |
|        - |  3361 | `			/* Point to the desired memory object */` |
|     2840 |  3362 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2840 |  3363 | `			if( pObj ){` |
|        - |  3364 | `				/* Perform the store operation */` |
|     2840 |  3365 | `				PH7_MemObjStore(pTos,pObj);` |
|     1419 |  3366 | `			}` |
|        - |  3367 | `		}` |
|   109826 |  3368 | `		break;` |
|   213966 |  3369 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3370 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3371 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3372 | `			/* Force a string cast */` |
|      ! 0 |  3373 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3374 | `		}` |
|        7 |  3375 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3376 | `		pTos--;` |
|        - |  3377 | `#ifdef UNTRUST` |
|        - |  3378 | `		if( pTos < pStack  ){` |
|        - |  3379 | `			goto Abort;` |
|        - |  3380 | `		}` |
|        - |  3381 | `#endif` |
|        4 |  3382 | `	}else{` |
|   213960 |  3383 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3384 | `	}` |
|        - |  3385 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   213966 |  3386 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   213966 |  3387 | `	if( pObj == 0 ){` |
|      ! 0 |  3388 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3389 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3390 | `		goto Abort;` |
|        - |  3391 | `	}` |
|   213966 |  3392 | `	if( !pInstr->p3 ){` |
|        7 |  3393 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3394 | `	}` |
|        - |  3395 | `	/* Perform the store operation */` |
|   213966 |  3396 | `	PH7_MemObjStore(pTos,pObj);` |
|   213966 |  3397 | `	break;` |
|        - |  3398 | `				   }` |
|        - |  3399 | `/*` |
|        - |  3400 | ` * STORE_IDX:   P1 * P3` |
|        - |  3401 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3402 | ` *` |
|        - |  3403 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3404 | ` */` |
|    79066 |  3405 | `case PH7_OP_STORE_IDX:` |
|        - |  3406 | `case PH7_OP_STORE_IDX_REF: {` |
|   158134 |  3407 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3408 | `	ph7_value *pKey;` |
|        - |  3409 | `	sxu32 nIdx;` |
|   158134 |  3410 | `	if( pInstr->iP1 ){` |
|        - |  3411 | `		/* Key is next on stack */` |
|    56424 |  3412 | `		pKey = pTos;` |
|    56424 |  3413 | `		pTos--;` |
|    28213 |  3414 | `	}else{` |
|   101712 |  3415 | `		pKey = 0;` |
|        - |  3416 | `	}` |
|   158134 |  3417 | `	nIdx = pTos->nIdx;` |
|   158134 |  3418 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3419 | `		/* Hashmap already loaded */` |
|   158082 |  3420 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   158082 |  3421 | `		if( pMap->iRef < 2 ){` |
|        - |  3422 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3423 | `			pMap->iRef = 2;` |
|      ! 0 |  3424 | `		}` |
|    79042 |  3425 | `	}else{` |
|        - |  3426 | `		ph7_value *pObj;` |
|       53 |  3427 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3428 | `		if( pObj == 0 ){` |
|      ! 0 |  3429 | `			if( pKey ){` |
|      ! 0 |  3430 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3431 | `			}` |
|      ! 0 |  3432 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3433 | `			break;` |
|        - |  3434 | `		}` |
|        - |  3435 | `		/* Phase#1: Load the array */` |
|       53 |  3436 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3437 | `			VmPopOperand(&pTos,1);` |
|       53 |  3438 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3439 | `				/* Force a string cast */` |
|      ! 0 |  3440 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3441 | `			}` |
|       53 |  3442 | `			if( pKey == 0 ){` |
|        - |  3443 | `				/* Append string */` |
|        3 |  3444 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3445 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3446 | `				}` |
|        2 |  3447 | `			}else{` |
|        - |  3448 | `				sxu32 nOfft;` |
|       51 |  3449 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3450 | `					/* Force an int cast */` |
|       51 |  3451 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3452 | `				}` |
|       51 |  3453 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3454 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3455 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3456 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3457 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3458 | `				}else{` |
|      ! 0 |  3459 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3460 | `						/* Perform an append operation */` |
|      ! 0 |  3461 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3462 | `					}` |
|        - |  3463 | `				}` |
|        - |  3464 | `			}` |
|       53 |  3465 | `			if( pKey ){` |
|       51 |  3466 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3467 | `			}` |
|       53 |  3468 | `			break;` |
|      ! 0 |  3469 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3470 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3471 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3472 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3473 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3474 | `				goto Abort;` |
|        - |  3475 | `			}` |
|      ! 0 |  3476 | `		}` |
|      ! 0 |  3477 | `		pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - |  3478 | `	}` |
|   158082 |  3479 | `	VmPopOperand(&pTos,1);` |
|        - |  3480 | `	/* Phase#2: Perform the insertion */` |
|   158082 |  3481 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3482 | `		/* Insertion by reference */` |
|       15 |  3483 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3484 | `	}else{` |
|   158068 |  3485 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3486 | `	}` |
|   158082 |  3487 | `	if( pKey ){` |
|    56374 |  3488 | `		PH7_MemObjRelease(pKey);` |
|    28186 |  3489 | `	}` |
|   158082 |  3490 | `	break;` |
|        - |  3491 | `					   }` |
|        - |  3492 | `/*` |
|        - |  3493 | ` * INCR: P1 * *` |
|        - |  3494 | ` *` |
|        - |  3495 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3496 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3497 | ` * the stack and increment after that.` |
|        - |  3498 | ` */` |
|   148419 |  3499 | `case PH7_OP_INCR:` |
|        - |  3500 | `#ifdef UNTRUST` |
|        - |  3501 | `	if( pTos < pStack ){` |
|        - |  3502 | `		goto Abort;` |
|        - |  3503 | `	}` |
|        - |  3504 | `#endif` |
|   296884 |  3505 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   296884 |  3506 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3507 | `			ph7_value *pObj;` |
|   296884 |  3508 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3509 | `				/* Force a numeric cast */` |
|   296884 |  3510 | `				PH7_MemObjToNumeric(pObj);` |
|   296884 |  3511 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3512 | `					pObj->rVal++;` |
|        - |  3513 | `					/* Try to get an integer representation */` |
|      ! 0 |  3514 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3515 | `				}else{` |
|   296884 |  3516 | `					pObj->x.iVal++;` |
|   296884 |  3517 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3518 | `				}` |
|   296884 |  3519 | `				if( pInstr->iP1 ){` |
|        - |  3520 | `					/* Pre-icrement */` |
|       71 |  3521 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3522 | `				}` |
|   148463 |  3523 | `			}` |
|   148465 |  3524 | `		}else{` |
|      ! 0 |  3525 | `			if( pInstr->iP1 ){` |
|        - |  3526 | `				/* Force a numeric cast */` |
|      ! 0 |  3527 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3528 | `				/* Pre-increment */` |
|      ! 0 |  3529 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3530 | `					pTos->rVal++;` |
|        - |  3531 | `					/* Try to get an integer representation */` |
|      ! 0 |  3532 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3533 | `				}else{` |
|      ! 0 |  3534 | `					pTos->x.iVal++;` |
|      ! 0 |  3535 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3536 | `				}` |
|      ! 0 |  3537 | `			}` |
|        - |  3538 | `		}` |
|   148463 |  3539 | `	}` |
|   296884 |  3540 | `	break;` |
|        - |  3541 | `/*` |
|        - |  3542 | ` * DECR: P1 * *` |
|        - |  3543 | ` *` |
|        - |  3544 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3545 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3546 | ` * and decrement after that.` |
|        - |  3547 | ` */` |
|        2 |  3548 | `case PH7_OP_DECR:` |
|        - |  3549 | `#ifdef UNTRUST` |
|        - |  3550 | `	if( pTos < pStack ){` |
|        - |  3551 | `		goto Abort;` |
|        - |  3552 | `	}` |
|        - |  3553 | `#endif` |
|        5 |  3554 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3555 | `		/* Force a numeric cast */` |
|        5 |  3556 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3557 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3558 | `			ph7_value *pObj;` |
|        5 |  3559 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3560 | `				/* Force a numeric cast */` |
|        5 |  3561 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3562 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3563 | `					pObj->rVal--;` |
|        - |  3564 | `					/* Try to get an integer representation */` |
|      ! 0 |  3565 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3566 | `				}else{` |
|        5 |  3567 | `					pObj->x.iVal--;` |
|        5 |  3568 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3569 | `				}` |
|        5 |  3570 | `				if( pInstr->iP1 ){` |
|        - |  3571 | `					/* Pre-icrement */` |
|      ! 0 |  3572 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3573 | `				}` |
|        2 |  3574 | `			}` |
|        3 |  3575 | `		}else{` |
|      ! 0 |  3576 | `			if( pInstr->iP1 ){` |
|        - |  3577 | `				/* Pre-increment */` |
|      ! 0 |  3578 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3579 | `					pTos->rVal--;` |
|        - |  3580 | `					/* Try to get an integer representation */` |
|      ! 0 |  3581 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3582 | `				}else{` |
|      ! 0 |  3583 | `					pTos->x.iVal--;` |
|      ! 0 |  3584 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3585 | `				}` |
|      ! 0 |  3586 | `			}` |
|        - |  3587 | `		}` |
|        2 |  3588 | `	}` |
|        5 |  3589 | `	break;` |
|        - |  3590 | `/*` |
|        - |  3591 | ` * UMINUS: * * *` |
|        - |  3592 | ` *` |
|        - |  3593 | ` * Perform a unary minus operation.` |
|        - |  3594 | ` */` |
|    22763 |  3595 | `case PH7_OP_UMINUS:` |
|        - |  3596 | `#ifdef UNTRUST` |
|        - |  3597 | `	if( pTos < pStack ){` |
|        - |  3598 | `		goto Abort;` |
|        - |  3599 | `	}` |
|        - |  3600 | `#endif` |
|        - |  3601 | `	/* Force a numeric (integer,real or both) cast */` |
|    45528 |  3602 | `	PH7_MemObjToNumeric(pTos);` |
|    45528 |  3603 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       32 |  3604 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3605 | `	}` |
|    45528 |  3606 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    45498 |  3607 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    22748 |  3608 | `	}` |
|    45528 |  3609 | `	break;` |
|        - |  3610 | `/*` |
|        - |  3611 | ` * UPLUS: * * *` |
|        - |  3612 | ` *` |
|        - |  3613 | ` * Perform a unary plus operation.` |
|        - |  3614 | ` */` |
|       16 |  3615 | `case PH7_OP_UPLUS:` |
|        - |  3616 | `#ifdef UNTRUST` |
|        - |  3617 | `	if( pTos < pStack ){` |
|        - |  3618 | `		goto Abort;` |
|        - |  3619 | `	}` |
|        - |  3620 | `#endif` |
|        - |  3621 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3622 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3623 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3624 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3625 | `	}` |
|       33 |  3626 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3627 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3628 | `	}` |
|       33 |  3629 | `	break;` |
|        - |  3630 | `/*` |
|        - |  3631 | ` * OP_LNOT: * * *` |
|        - |  3632 | ` *` |
|        - |  3633 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3634 | ` * with its complement.` |
|        - |  3635 | ` */` |
|    38994 |  3636 | `case PH7_OP_LNOT:` |
|        - |  3637 | `#ifdef UNTRUST` |
|        - |  3638 | `	if( pTos < pStack ){` |
|        - |  3639 | `		goto Abort;` |
|        - |  3640 | `	}` |
|        - |  3641 | `#endif` |
|        - |  3642 | `	/* Force a boolean cast */` |
|    78034 |  3643 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3644 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3645 | `	}` |
|    78034 |  3646 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    78034 |  3647 | `	break;` |
|        - |  3648 | `/*` |
|        - |  3649 | ` * OP_BITNOT: * * *` |
|        - |  3650 | ` *` |
|        - |  3651 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3652 | ` * with its ones-complement.` |
|        - |  3653 | ` */` |
|       14 |  3654 | `case PH7_OP_BITNOT:` |
|        - |  3655 | `#ifdef UNTRUST` |
|        - |  3656 | `	if( pTos < pStack ){` |
|        - |  3657 | `		goto Abort;` |
|        - |  3658 | `	}` |
|        - |  3659 | `#endif` |
|        - |  3660 | `	/* Force an integer cast */` |
|       30 |  3661 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3662 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3663 | `	}` |
|       30 |  3664 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  3665 | `	break;` |
|        - |  3666 | `/* OP_MUL * * *` |
|        - |  3667 | ` * OP_MUL_STORE * * *` |
|        - |  3668 | ` *` |
|        - |  3669 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3670 | ` * and push the result back onto the stack.` |
|        - |  3671 | ` */` |
|     1234 |  3672 | `case PH7_OP_MUL:` |
|        - |  3673 | `case PH7_OP_MUL_STORE: {` |
|     2470 |  3674 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3675 | `	/* Force the operand to be numeric */` |
|        - |  3676 | `#ifdef UNTRUST` |
|        - |  3677 | `	if( pNos < pStack ){` |
|        - |  3678 | `		goto Abort;` |
|        - |  3679 | `	}` |
|        - |  3680 | `#endif` |
|     2470 |  3681 | `	PH7_MemObjToNumeric(pTos);` |
|     2470 |  3682 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3683 | `	/* Perform the requested operation */` |
|     2470 |  3684 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3685 | `		/* Floating point arithemic */` |
|        - |  3686 | `		ph7_real a,b,r;` |
|       17 |  3687 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3688 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3689 | `		}` |
|       17 |  3690 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3691 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3692 | `		}` |
|       17 |  3693 | `		a = pNos->rVal;` |
|       17 |  3694 | `		b = pTos->rVal;` |
|       17 |  3695 | `		r = a * b;` |
|        - |  3696 | `		/* Push the result */` |
|       17 |  3697 | `		pNos->rVal = r;` |
|       17 |  3698 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3699 | `		/* Try to get an integer representation */` |
|       17 |  3700 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3701 | `	}else{` |
|        - |  3702 | `		/* Integer arithmetic */` |
|        - |  3703 | `		sxi64 a,b,r;` |
|     2454 |  3704 | `		a = pNos->x.iVal;` |
|     2454 |  3705 | `		b = pTos->x.iVal;` |
|     2454 |  3706 | `		r = a * b;` |
|        - |  3707 | `		/* Push the result */` |
|     2454 |  3708 | `		pNos->x.iVal = r;` |
|     2454 |  3709 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3710 | `	}` |
|     2470 |  3711 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3712 | `		ph7_value *pObj;` |
|       19 |  3713 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3714 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  3715 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  3716 | `			PH7_MemObjStore(pNos,pObj);` |
|        9 |  3717 | `		}` |
|        9 |  3718 | `	}` |
|     2470 |  3719 | `	VmPopOperand(&pTos,1);` |
|     2470 |  3720 | `	break;` |
|        - |  3721 | `				 }` |
|        - |  3722 | `/* OP_ADD * * *` |
|        - |  3723 | ` *` |
|        - |  3724 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3725 | ` * and push the result back onto the stack.` |
|        - |  3726 | ` */` |
|      429 |  3727 | `case PH7_OP_ADD:{` |
|      860 |  3728 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3729 | `#ifdef UNTRUST` |
|        - |  3730 | `	if( pNos < pStack ){` |
|        - |  3731 | `		goto Abort;` |
|        - |  3732 | `	}` |
|        - |  3733 | `#endif` |
|        - |  3734 | `	/* Perform the addition */` |
|      860 |  3735 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      860 |  3736 | `	VmPopOperand(&pTos,1);` |
|      860 |  3737 | `	break;` |
|        - |  3738 | `				}` |
|        - |  3739 | `/*` |
|        - |  3740 | ` * OP_ADD_STORE * * *` |
|        - |  3741 | ` *` |
|        - |  3742 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3743 | ` * and push the result back onto the stack.` |
|        - |  3744 | ` */` |
|      482 |  3745 | `case PH7_OP_ADD_STORE:{` |
|      966 |  3746 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3747 | `	ph7_value *pObj;` |
|        - |  3748 | `	sxu32 nIdx;` |
|        - |  3749 | `#ifdef UNTRUST` |
|        - |  3750 | `	if( pNos < pStack ){` |
|        - |  3751 | `		goto Abort;` |
|        - |  3752 | `	}` |
|        - |  3753 | `#endif` |
|        - |  3754 | `	/* Perform the addition */` |
|      966 |  3755 | `	nIdx = pTos->nIdx;` |
|      966 |  3756 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3757 | `	/* Peform the store operation */` |
|      966 |  3758 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3759 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      966 |  3760 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      966 |  3761 | `		PH7_MemObjStore(pTos,pObj);` |
|      482 |  3762 | `	}` |
|        - |  3763 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      966 |  3764 | `	PH7_MemObjStore(pTos,pNos);` |
|      966 |  3765 | `	VmPopOperand(&pTos,1);` |
|      966 |  3766 | `	break;` |
|        - |  3767 | `				}` |
|        - |  3768 | `/* OP_SUB * * *` |
|        - |  3769 | ` *` |
|        - |  3770 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3771 | ` * first (what was next on the stack) from the second (the` |
|        - |  3772 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3773 | ` */` |
|      294 |  3774 | `case PH7_OP_SUB: {` |
|      589 |  3775 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3776 | `#ifdef UNTRUST` |
|        - |  3777 | `	if( pNos < pStack ){` |
|        - |  3778 | `		goto Abort;` |
|        - |  3779 | `	}` |
|        - |  3780 | `#endif` |
|      589 |  3781 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3782 | `		/* Floating point arithemic */` |
|        - |  3783 | `		ph7_real a,b,r;` |
|       95 |  3784 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3785 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3786 | `		}` |
|       95 |  3787 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3788 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3789 | `		}` |
|       95 |  3790 | `		a = pNos->rVal;` |
|       95 |  3791 | `		b = pTos->rVal;` |
|       95 |  3792 | `		r = a - b;` |
|        - |  3793 | `		/* Push the result */` |
|       95 |  3794 | `		pNos->rVal = r;` |
|       95 |  3795 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3796 | `		/* Try to get an integer representation */` |
|       95 |  3797 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  3798 | `	}else{` |
|        - |  3799 | `		/* Integer arithmetic */` |
|        - |  3800 | `		sxi64 a,b,r;` |
|      495 |  3801 | `		a = pNos->x.iVal;` |
|      495 |  3802 | `		b = pTos->x.iVal;` |
|      495 |  3803 | `		r = a - b;` |
|        - |  3804 | `		/* Push the result */` |
|      495 |  3805 | `		pNos->x.iVal = r;` |
|      495 |  3806 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3807 | `	}` |
|      589 |  3808 | `	VmPopOperand(&pTos,1);` |
|      589 |  3809 | `	break;` |
|        - |  3810 | `				 }` |
|        - |  3811 | `/* OP_SUB_STORE * * *` |
|        - |  3812 | ` *` |
|        - |  3813 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3814 | ` * first (what was next on the stack) from the second (the` |
|        - |  3815 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3816 | ` */` |
|        1 |  3817 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3818 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3819 | `	ph7_value *pObj;` |
|        - |  3820 | `#ifdef UNTRUST` |
|        - |  3821 | `	if( pNos < pStack ){` |
|        - |  3822 | `		goto Abort;` |
|        - |  3823 | `	}` |
|        - |  3824 | `#endif` |
|        3 |  3825 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3826 | `		/* Floating point arithemic */` |
|        - |  3827 | `		ph7_real a,b,r;` |
|      ! 0 |  3828 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3829 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3830 | `		}` |
|      ! 0 |  3831 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3832 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3833 | `		}` |
|      ! 0 |  3834 | `		a = pTos->rVal;` |
|      ! 0 |  3835 | `		b = pNos->rVal;` |
|      ! 0 |  3836 | `		r = a - b;` |
|        - |  3837 | `		/* Push the result */` |
|      ! 0 |  3838 | `		pNos->rVal = r;` |
|      ! 0 |  3839 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3840 | `		/* Try to get an integer representation */` |
|      ! 0 |  3841 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3842 | `	}else{` |
|        - |  3843 | `		/* Integer arithmetic */` |
|        - |  3844 | `		sxi64 a,b,r;` |
|        3 |  3845 | `		a = pTos->x.iVal;` |
|        3 |  3846 | `		b = pNos->x.iVal;` |
|        3 |  3847 | `		r = a - b;` |
|        - |  3848 | `		/* Push the result */` |
|        3 |  3849 | `		pNos->x.iVal = r;` |
|        3 |  3850 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3851 | `	}` |
|        3 |  3852 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3853 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3854 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3855 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3856 | `	}` |
|        3 |  3857 | `	VmPopOperand(&pTos,1);` |
|        3 |  3858 | `	break;` |
|        - |  3859 | `				 }` |
|        - |  3860 |  |
|        - |  3861 | `/*` |
|        - |  3862 | ` * OP_MOD * * *` |
|        - |  3863 | ` *` |
|        - |  3864 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3865 | ` * first (what was next on the stack) from the second (the` |
|        - |  3866 | ` * top of the stack) and push the remainder after division` |
|        - |  3867 | ` * onto the stack.` |
|        - |  3868 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3869 | ` */` |
|      296 |  3870 | `case PH7_OP_MOD:{` |
|      594 |  3871 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3872 | `	sxi64 a,b,r;` |
|        - |  3873 | `#ifdef UNTRUST` |
|        - |  3874 | `	if( pNos < pStack ){` |
|        - |  3875 | `		goto Abort;` |
|        - |  3876 | `	}` |
|        - |  3877 | `#endif` |
|        - |  3878 | `	/* Force the operands to be integer */` |
|      594 |  3879 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3880 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3881 | `	}` |
|      594 |  3882 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3883 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3884 | `	}` |
|        - |  3885 | `	/* Perform the requested operation */` |
|      594 |  3886 | `	a = pNos->x.iVal;` |
|      594 |  3887 | `	b = pTos->x.iVal;` |
|      594 |  3888 | `	if( b == 0 ){` |
|        3 |  3889 | `		r = 0;` |
|        3 |  3890 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3891 | `		/* goto Abort; */` |
|        2 |  3892 | `	}else{` |
|      591 |  3893 | `		r = a%b;` |
|        - |  3894 | `	}` |
|        - |  3895 | `	/* Push the result */` |
|      594 |  3896 | `	pNos->x.iVal = r;` |
|      594 |  3897 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      594 |  3898 | `	VmPopOperand(&pTos,1);` |
|      594 |  3899 | `	break;` |
|        - |  3900 | `				}` |
|        - |  3901 | `/*` |
|        - |  3902 | ` * OP_MOD_STORE * * *` |
|        - |  3903 | ` *` |
|        - |  3904 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3905 | ` * first (what was next on the stack) from the second (the` |
|        - |  3906 | ` * top of the stack) and push the remainder after division` |
|        - |  3907 | ` * onto the stack.` |
|        - |  3908 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3909 | ` */` |
|        1 |  3910 | `case PH7_OP_MOD_STORE: {` |
|        3 |  3911 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3912 | `	ph7_value *pObj;` |
|        - |  3913 | `	sxi64 a,b,r;` |
|        - |  3914 | `#ifdef UNTRUST` |
|        - |  3915 | `	if( pNos < pStack ){` |
|        - |  3916 | `		goto Abort;` |
|        - |  3917 | `	}` |
|        - |  3918 | `#endif` |
|        - |  3919 | `	/* Force the operands to be integer */` |
|        3 |  3920 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3921 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3922 | `	}` |
|        3 |  3923 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3924 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3925 | `	}` |
|        - |  3926 | `	/* Perform the requested operation */` |
|        3 |  3927 | `	a = pTos->x.iVal;` |
|        3 |  3928 | `	b = pNos->x.iVal;` |
|        3 |  3929 | `	if( b == 0 ){` |
|      ! 0 |  3930 | `		r = 0;` |
|      ! 0 |  3931 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3932 | `		/* goto Abort; */` |
|      ! 0 |  3933 | `	}else{` |
|        3 |  3934 | `		r = a%b;` |
|        - |  3935 | `	}` |
|        - |  3936 | `	/* Push the result */` |
|        3 |  3937 | `	pNos->x.iVal = r;` |
|        3 |  3938 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  3939 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3940 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3941 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3942 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3943 | `	}` |
|        3 |  3944 | `	VmPopOperand(&pTos,1);` |
|        3 |  3945 | `	break;` |
|        - |  3946 | `				}` |
|        - |  3947 | `/*` |
|        - |  3948 | ` * OP_DIV * * *` |
|        - |  3949 | ` *` |
|        - |  3950 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3951 | ` * first (what was next on the stack) from the second (the` |
|        - |  3952 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3953 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3954 | ` */` |
|       28 |  3955 | `case PH7_OP_DIV:{` |
|       58 |  3956 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3957 | `	ph7_real a,b,r;` |
|        - |  3958 | `#ifdef UNTRUST` |
|        - |  3959 | `	if( pNos < pStack ){` |
|        - |  3960 | `		goto Abort;` |
|        - |  3961 | `	}` |
|        - |  3962 | `#endif` |
|        - |  3963 | `	/* Force the operands to be real */` |
|       58 |  3964 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  3965 | `		PH7_MemObjToReal(pTos);` |
|       26 |  3966 | `	}` |
|       58 |  3967 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  3968 | `		PH7_MemObjToReal(pNos);` |
|        9 |  3969 | `	}` |
|        - |  3970 | `	/* Perform the requested operation */` |
|       58 |  3971 | `	a = pNos->rVal;` |
|       58 |  3972 | `	b = pTos->rVal;` |
|       58 |  3973 | `	if( b == 0 ){` |
|        - |  3974 | `		/* Division by zero */` |
|        3 |  3975 | `		pNos->rVal = 0;` |
|        3 |  3976 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  3977 | `		/* goto Abort; */` |
|        2 |  3978 | `	}else{` |
|       55 |  3979 | `		r = a/b;` |
|        - |  3980 | `		/* Push the result */` |
|       55 |  3981 | `		pNos->rVal = r;` |
|       55 |  3982 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3983 | `		/* Try to get an integer representation */` |
|       55 |  3984 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3985 | `	}` |
|       58 |  3986 | `	VmPopOperand(&pTos,1);` |
|       58 |  3987 | `	break;` |
|        - |  3988 | `				}` |
|        - |  3989 | `/*` |
|        - |  3990 | ` * OP_DIV_STORE * * *` |
|        - |  3991 | ` *` |
|        - |  3992 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3993 | ` * first (what was next on the stack) from the second (the` |
|        - |  3994 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3995 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3996 | ` */` |
|        1 |  3997 | `case PH7_OP_DIV_STORE:{` |
|        3 |  3998 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3999 | `	ph7_value *pObj;` |
|        - |  4000 | `	ph7_real a,b,r;` |
|        - |  4001 | `#ifdef UNTRUST` |
|        - |  4002 | `	if( pNos < pStack ){` |
|        - |  4003 | `		goto Abort;` |
|        - |  4004 | `	}` |
|        - |  4005 | `#endif` |
|        - |  4006 | `	/* Force the operands to be real */` |
|        3 |  4007 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4008 | `		PH7_MemObjToReal(pTos);` |
|        1 |  4009 | `	}` |
|        3 |  4010 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4011 | `		PH7_MemObjToReal(pNos);` |
|        1 |  4012 | `	}` |
|        - |  4013 | `	/* Perform the requested operation */` |
|        3 |  4014 | `	a = pTos->rVal;` |
|        3 |  4015 | `	b = pNos->rVal;` |
|        3 |  4016 | `	if( b == 0 ){` |
|        - |  4017 | `		/* Division by zero */` |
|      ! 0 |  4018 | `		r = 0;` |
|      ! 0 |  4019 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4020 | `		/* goto Abort; */` |
|      ! 0 |  4021 | `	}else{` |
|        3 |  4022 | `		r = a/b;` |
|        - |  4023 | `		/* Push the result */` |
|        3 |  4024 | `		pNos->rVal = r;` |
|        3 |  4025 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4026 | `		/* Try to get an integer representation */` |
|        3 |  4027 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4028 | `	}` |
|        3 |  4029 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4030 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4031 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4032 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4033 | `	}` |
|        3 |  4034 | `	VmPopOperand(&pTos,1);` |
|        3 |  4035 | `	break;` |
|        - |  4036 | `				}` |
|        - |  4037 | `/* OP_BAND * * *` |
|        - |  4038 | ` *` |
|        - |  4039 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4040 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4041 | ` * two elements.` |
|        - |  4042 | `*/` |
|        - |  4043 | `/* OP_BOR * * *` |
|        - |  4044 | ` *` |
|        - |  4045 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4046 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4047 | ` * two elements.` |
|        - |  4048 | ` */` |
|        - |  4049 | `/* OP_BXOR * * *` |
|        - |  4050 | ` *` |
|        - |  4051 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4052 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4053 | ` * two elements.` |
|        - |  4054 | ` */` |
|       30 |  4055 | `case PH7_OP_BAND:` |
|        - |  4056 | `case PH7_OP_BOR:` |
|        - |  4057 | `case PH7_OP_BXOR:{` |
|       62 |  4058 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4059 | `	sxi64 a,b,r;` |
|        - |  4060 | `#ifdef UNTRUST` |
|        - |  4061 | `	if( pNos < pStack ){` |
|        - |  4062 | `		goto Abort;` |
|        - |  4063 | `	}` |
|        - |  4064 | `#endif` |
|        - |  4065 | `	/* Force the operands to be integer */` |
|       62 |  4066 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4067 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4068 | `	}` |
|       62 |  4069 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4070 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4071 | `	}` |
|        - |  4072 | `	/* Perform the requested operation */` |
|       62 |  4073 | `	a = pNos->x.iVal;` |
|       62 |  4074 | `	b = pTos->x.iVal;` |
|       62 |  4075 | `	switch(pInstr->iOp){` |
|        6 |  4076 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4077 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4078 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4079 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       18 |  4080 | `	case PH7_OP_BAND_STORE:` |
|       18 |  4081 | `	case PH7_OP_BAND:` |
|       38 |  4082 | `	default:          r = a&b; break;` |
|        - |  4083 | `	}` |
|        - |  4084 | `	/* Push the result */` |
|       62 |  4085 | `	pNos->x.iVal = r;` |
|       62 |  4086 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       62 |  4087 | `	VmPopOperand(&pTos,1);` |
|       62 |  4088 | `	break;` |
|        - |  4089 | `				 }` |
|        - |  4090 | `/* OP_BAND_STORE * * *` |
|        - |  4091 | ` *` |
|        - |  4092 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4093 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4094 | ` * two elements.` |
|        - |  4095 | `*/` |
|        - |  4096 | `/* OP_BOR_STORE * * *` |
|        - |  4097 | ` *` |
|        - |  4098 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4099 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4100 | ` * two elements.` |
|        - |  4101 | ` */` |
|        - |  4102 | `/* OP_BXOR_STORE * * *` |
|        - |  4103 | ` *` |
|        - |  4104 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4105 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4106 | ` * two elements.` |
|        - |  4107 | ` */` |
|        7 |  4108 | `case PH7_OP_BAND_STORE:` |
|        - |  4109 | `case PH7_OP_BOR_STORE:` |
|        - |  4110 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4111 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4112 | `	ph7_value *pObj;` |
|        - |  4113 | `	sxi64 a,b,r;` |
|        - |  4114 | `#ifdef UNTRUST` |
|        - |  4115 | `	if( pNos < pStack ){` |
|        - |  4116 | `		goto Abort;` |
|        - |  4117 | `	}` |
|        - |  4118 | `#endif` |
|        - |  4119 | `	/* Force the operands to be integer */` |
|       15 |  4120 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4121 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4122 | `	}` |
|       15 |  4123 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4124 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4125 | `	}` |
|        - |  4126 | `	/* Perform the requested operation */` |
|       15 |  4127 | `	a = pTos->x.iVal;` |
|       15 |  4128 | `	b = pNos->x.iVal;` |
|       15 |  4129 | `	switch(pInstr->iOp){` |
|        2 |  4130 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4131 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4132 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4133 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4134 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4135 | `	case PH7_OP_BAND:` |
|        5 |  4136 | `	default:          r = a&b; break;` |
|        - |  4137 | `	}` |
|        - |  4138 | `	/* Push the result */` |
|       15 |  4139 | `	pNos->x.iVal = r;` |
|       15 |  4140 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4141 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4142 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4143 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4144 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4145 | `	}` |
|       15 |  4146 | `	VmPopOperand(&pTos,1);` |
|       15 |  4147 | `	break;` |
|        - |  4148 | `				 }` |
|        - |  4149 | `/* OP_SHL * * *` |
|        - |  4150 | ` *` |
|        - |  4151 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4152 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4153 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4154 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4155 | ` */` |
|        - |  4156 | `/* OP_SHR * * *` |
|        - |  4157 | ` *` |
|        - |  4158 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4159 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4160 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4161 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4162 | ` */` |
|        9 |  4163 | `case PH7_OP_SHL:` |
|        - |  4164 | `case PH7_OP_SHR: {` |
|       19 |  4165 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4166 | `	sxi64 a,r;` |
|        - |  4167 | `	sxi32 b;` |
|        - |  4168 | `#ifdef UNTRUST` |
|        - |  4169 | `	if( pNos < pStack ){` |
|        - |  4170 | `		goto Abort;` |
|        - |  4171 | `	}` |
|        - |  4172 | `#endif` |
|        - |  4173 | `	/* Force the operands to be integer */` |
|       19 |  4174 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4175 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4176 | `	}` |
|       19 |  4177 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4178 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4179 | `	}` |
|        - |  4180 | `	/* Perform the requested operation */` |
|       19 |  4181 | `	a = pNos->x.iVal;` |
|       19 |  4182 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4183 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4184 | `		r = a << b;` |
|        6 |  4185 | `	}else{` |
|        9 |  4186 | `		r = a >> b;` |
|        - |  4187 | `	}` |
|        - |  4188 | `	/* Push the result */` |
|       19 |  4189 | `	pNos->x.iVal = r;` |
|       19 |  4190 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4191 | `	VmPopOperand(&pTos,1);` |
|       19 |  4192 | `	break;` |
|        - |  4193 | `				 }` |
|        - |  4194 | `/*  OP_SHL_STORE * * *` |
|        - |  4195 | ` *` |
|        - |  4196 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4197 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4198 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4199 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4200 | ` */` |
|        - |  4201 | `/* OP_SHR_STORE * * *` |
|        - |  4202 | ` *` |
|        - |  4203 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4204 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4205 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4206 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4207 | ` */` |
|        7 |  4208 | `case PH7_OP_SHL_STORE:` |
|        - |  4209 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4210 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4211 | `	ph7_value *pObj;` |
|        - |  4212 | `	sxi64 a,r;` |
|        - |  4213 | `	sxi32 b;` |
|        - |  4214 | `#ifdef UNTRUST` |
|        - |  4215 | `	if( pNos < pStack ){` |
|        - |  4216 | `		goto Abort;` |
|        - |  4217 | `	}` |
|        - |  4218 | `#endif` |
|        - |  4219 | `	/* Force the operands to be integer */` |
|       15 |  4220 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4221 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4222 | `	}` |
|       15 |  4223 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4224 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4225 | `	}` |
|        - |  4226 | `	/* Perform the requested operation */` |
|       15 |  4227 | `	a = pTos->x.iVal;` |
|       15 |  4228 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4229 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4230 | `		r = a << b;` |
|        4 |  4231 | `	}else{` |
|        9 |  4232 | `		r = a >> b;` |
|        - |  4233 | `	}` |
|        - |  4234 | `	/* Push the result */` |
|       15 |  4235 | `	pNos->x.iVal = r;` |
|       15 |  4236 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4237 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4238 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4239 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4240 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4241 | `	}` |
|       15 |  4242 | `	VmPopOperand(&pTos,1);` |
|       15 |  4243 | `	break;` |
|        - |  4244 | `				 }` |
|        - |  4245 | `/* CAT:  P1 * *` |
|        - |  4246 | ` *` |
|        - |  4247 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4248 | ` * back.` |
|        - |  4249 | ` */` |
|    60701 |  4250 | `case PH7_OP_CAT:{` |
|        - |  4251 | `	ph7_value *pNos,*pCur;` |
|   121404 |  4252 | `	if( pInstr->iP1 < 1 ){` |
|    94516 |  4253 | `		pNos = &pTos[-1];` |
|    47259 |  4254 | `	}else{` |
|    26890 |  4255 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4256 | `	}` |
|        - |  4257 | `#ifdef UNTRUST` |
|        - |  4258 | `	if( pNos < pStack ){` |
|        - |  4259 | `		goto Abort;` |
|        - |  4260 | `	}` |
|        - |  4261 | `#endif` |
|        - |  4262 | `	/* Force a string cast */` |
|   121404 |  4263 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      980 |  4264 | `		PH7_MemObjToString(pNos);` |
|      489 |  4265 | `	}` |
|   121404 |  4266 | `	pCur = &pNos[1];` |
|   244654 |  4267 | `	while( pCur <= pTos ){` |
|   123252 |  4268 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50456 |  4269 | `			PH7_MemObjToString(pCur);` |
|    25227 |  4270 | `		}` |
|        - |  4271 | `		/* Perform the concatenation */` |
|   123252 |  4272 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   123214 |  4273 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    61606 |  4274 | `		}` |
|   123252 |  4275 | `		SyBlobRelease(&pCur->sBlob);` |
|   123252 |  4276 | `		pCur++;` |
|        2 |  4277 | `	}` |
|   121404 |  4278 | `	pTos = pNos;` |
|   121404 |  4279 | `	break;` |
|        - |  4280 | `				}` |
|        - |  4281 | `/*  CAT_STORE: * * *` |
|        - |  4282 | ` *` |
|        - |  4283 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4284 | ` * back.` |
|        - |  4285 | ` */` |
|     3193 |  4286 | `case PH7_OP_CAT_STORE:{` |
|     6388 |  4287 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4288 | `	ph7_value *pObj;` |
|        - |  4289 | `#ifdef UNTRUST` |
|        - |  4290 | `	if( pNos < pStack ){` |
|        - |  4291 | `		goto Abort;` |
|        - |  4292 | `	}` |
|        - |  4293 | `#endif` |
|     6388 |  4294 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4295 | `		/* Force a string cast */` |
|      ! 0 |  4296 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4297 | `	}` |
|     6388 |  4298 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4299 | `		/* Force a string cast */` |
|      ! 0 |  4300 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4301 | `	}` |
|        - |  4302 | `	/* Perform the concatenation (Reverse order) */` |
|     6388 |  4303 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     6388 |  4304 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3193 |  4305 | `	}` |
|        - |  4306 | `	/* Perform the store operation */` |
|     6388 |  4307 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4308 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     6388 |  4309 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     6388 |  4310 | `		PH7_MemObjStore(pTos,pObj);` |
|     3193 |  4311 | `	}` |
|     6388 |  4312 | `	PH7_MemObjStore(pTos,pNos);` |
|     6388 |  4313 | `	VmPopOperand(&pTos,1);` |
|     6388 |  4314 | `	break;` |
|        - |  4315 | `				}` |
|        - |  4316 | `/* OP_AND: * * *` |
|        - |  4317 | ` *` |
|        - |  4318 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4319 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4320 | ` * stack.` |
|        - |  4321 | ` */` |
|        - |  4322 | `/* OP_OR: * * *` |
|        - |  4323 | ` *` |
|        - |  4324 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4325 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4326 | ` * stack.` |
|        - |  4327 | ` */` |
|    92163 |  4328 | `case PH7_OP_LAND:` |
|        - |  4329 | `case PH7_OP_LOR: {` |
|   184372 |  4330 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4331 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4332 | `#ifdef UNTRUST` |
|        - |  4333 | `	if( pNos < pStack ){` |
|        - |  4334 | `		goto Abort;` |
|        - |  4335 | `	}` |
|        - |  4336 | `#endif` |
|        - |  4337 | `	/* Force a boolean cast */` |
|   184372 |  4338 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4339 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4340 | `	}` |
|   184372 |  4341 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4342 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4343 | `	}` |
|   184372 |  4344 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   184372 |  4345 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   184372 |  4346 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4347 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    84626 |  4348 | `		v1 = and_logic[v1*3+v2];` |
|    42336 |  4349 | `	}else{` |
|        - |  4350 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|    99748 |  4351 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4352 | `	}` |
|   184372 |  4353 | `	if( v1 == 2 ){` |
|      ! 0 |  4354 | `		v1 = 1;` |
|      ! 0 |  4355 | `	}` |
|   184372 |  4356 | `	VmPopOperand(&pTos,1);` |
|   184372 |  4357 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   184372 |  4358 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   184372 |  4359 | `	break;` |
|        - |  4360 | `				 }` |
|        - |  4361 | `/* OP_LXOR: * * *` |
|        - |  4362 | ` *` |
|        - |  4363 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4364 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4365 | ` * stack.` |
|        - |  4366 | ` * According to the PHP language reference manual:` |
|        - |  4367 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4368 | ` *  TRUE,but not both.` |
|        - |  4369 | ` */` |
|        5 |  4370 | `case PH7_OP_LXOR:{` |
|       11 |  4371 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4372 | `	sxi32 v = 0;` |
|        - |  4373 | `#ifdef UNTRUST` |
|        - |  4374 | `	if( pNos < pStack ){` |
|        - |  4375 | `		goto Abort;` |
|        - |  4376 | `	}` |
|        - |  4377 | `#endif` |
|        - |  4378 | `	/* Force a boolean cast */` |
|       11 |  4379 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4380 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4381 | `	}` |
|       11 |  4382 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4383 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4384 | `	}` |
|       11 |  4385 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4386 | `		v = 1;` |
|        3 |  4387 | `	}` |
|       11 |  4388 | `	VmPopOperand(&pTos,1);` |
|       11 |  4389 | `	pTos->x.iVal = v;` |
|       11 |  4390 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4391 | `	break;` |
|        - |  4392 | `				 }` |
|        - |  4393 | `/* OP_EQ P1 P2 P3` |
|        - |  4394 | ` *` |
|        - |  4395 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4396 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4397 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4398 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4399 | ` */` |
|        - |  4400 | `/* OP_NEQ P1 P2 P3` |
|        - |  4401 | ` *` |
|        - |  4402 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4403 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4404 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4405 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4406 | ` */` |
|     3779 |  4407 | `case PH7_OP_EQ:` |
|        - |  4408 | `case PH7_OP_NEQ: {` |
|     7560 |  4409 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4410 | `	/* Perform the comparison and act accordingly */` |
|        - |  4411 | `#ifdef UNTRUST` |
|        - |  4412 | `	if( pNos < pStack ){` |
|        - |  4413 | `		goto Abort;` |
|        - |  4414 | `	}` |
|        - |  4415 | `#endif` |
|     7560 |  4416 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7560 |  4417 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       20 |  4418 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7551 |  4419 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7516 |  4420 | `		rc = rc == 0;` |
|     3759 |  4421 | `	}else{` |
|       28 |  4422 | `		rc = rc != 0;` |
|        - |  4423 | `	}` |
|     7560 |  4424 | `	VmPopOperand(&pTos,1);` |
|     7560 |  4425 | `	if( !pInstr->iP2 ){` |
|        - |  4426 | `		/* Push comparison result without taking the jump */` |
|     7560 |  4427 | `		PH7_MemObjRelease(pTos);` |
|     7560 |  4428 | `		pTos->x.iVal = rc;` |
|        - |  4429 | `		/* Invalidate any prior representation */` |
|     7560 |  4430 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3781 |  4431 | `	}else{` |
|      ! 0 |  4432 | `		if( rc ){` |
|        - |  4433 | `			/* Jump to the desired location */` |
|      ! 0 |  4434 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4435 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4436 | `		}` |
|        - |  4437 | `	}` |
|     7560 |  4438 | `	break;` |
|        - |  4439 | `				 }` |
|        - |  4440 | `/* OP_TEQ P1 P2 *` |
|        - |  4441 | ` *` |
|        - |  4442 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4443 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4444 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4445 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4446 | ` */` |
|   126272 |  4447 | `case PH7_OP_TEQ: {` |
|   252546 |  4448 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4449 | `	/* Perform the comparison and act accordingly */` |
|        - |  4450 | `#ifdef UNTRUST` |
|        - |  4451 | `	if( pNos < pStack ){` |
|        - |  4452 | `		goto Abort;` |
|        - |  4453 | `	}` |
|        - |  4454 | `#endif` |
|   252546 |  4455 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   252546 |  4456 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4457 | `		rc = 0;` |
|        2 |  4458 | `	}else{` |
|   252544 |  4459 | `		rc = rc == 0;` |
|        - |  4460 | `	}` |
|   252546 |  4461 | `	VmPopOperand(&pTos,1);` |
|   252546 |  4462 | `	if( !pInstr->iP2 ){` |
|        - |  4463 | `		/* Push comparison result without taking the jump */` |
|   252546 |  4464 | `		PH7_MemObjRelease(pTos);` |
|   252546 |  4465 | `		pTos->x.iVal = rc;` |
|        - |  4466 | `		/* Invalidate any prior representation */` |
|   252546 |  4467 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   126274 |  4468 | `	}else{` |
|      ! 0 |  4469 | `		if( rc ){` |
|        - |  4470 | `			/* Jump to the desired location */` |
|      ! 0 |  4471 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4472 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4473 | `		}` |
|        - |  4474 | `	}` |
|   252546 |  4475 | `	break;` |
|        - |  4476 | `				 }` |
|        - |  4477 | `/* OP_TNE P1 P2 *` |
|        - |  4478 | ` *` |
|        - |  4479 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4480 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4481 | ` * instruction.` |
|        - |  4482 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4483 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4484 | ` *` |
|        - |  4485 | ` */` |
|    98719 |  4486 | `case PH7_OP_TNE: {` |
|   197440 |  4487 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4488 | `	/* Perform the comparison and act accordingly */` |
|        - |  4489 | `#ifdef UNTRUST` |
|        - |  4490 | `	if( pNos < pStack ){` |
|        - |  4491 | `		goto Abort;` |
|        - |  4492 | `	}` |
|        - |  4493 | `#endif` |
|   197440 |  4494 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   197440 |  4495 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4496 | `		rc = 1;` |
|        2 |  4497 | `	}else{` |
|   197438 |  4498 | `		rc = rc != 0;` |
|        - |  4499 | `	}` |
|   197440 |  4500 | `	VmPopOperand(&pTos,1);` |
|   197440 |  4501 | `	if( !pInstr->iP2 ){` |
|        - |  4502 | `		/* Push comparison result without taking the jump */` |
|   197440 |  4503 | `		PH7_MemObjRelease(pTos);` |
|   197440 |  4504 | `		pTos->x.iVal = rc;` |
|        - |  4505 | `		/* Invalidate any prior representation */` |
|   197440 |  4506 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    98721 |  4507 | `	}else{` |
|      ! 0 |  4508 | `		if( rc ){` |
|        - |  4509 | `			/* Jump to the desired location */` |
|      ! 0 |  4510 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4511 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4512 | `		}` |
|        - |  4513 | `	}` |
|   197440 |  4514 | `	break;` |
|        - |  4515 | `				 }` |
|        - |  4516 | `/* OP_LT P1 P2 P3` |
|        - |  4517 | ` *` |
|        - |  4518 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4519 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4520 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4521 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4522 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4523 | ` *` |
|        - |  4524 | ` */` |
|        - |  4525 | `/* OP_LE P1 P2 P3` |
|        - |  4526 | ` *` |
|        - |  4527 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4528 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4529 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4530 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4531 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4532 | ` *` |
|        - |  4533 | ` */` |
|   100883 |  4534 | `case PH7_OP_LT:` |
|        - |  4535 | `case PH7_OP_LE: {` |
|   201812 |  4536 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4537 | `	/* Perform the comparison and act accordingly */` |
|        - |  4538 | `#ifdef UNTRUST` |
|        - |  4539 | `	if( pNos < pStack ){` |
|        - |  4540 | `		goto Abort;` |
|        - |  4541 | `	}` |
|        - |  4542 | `#endif` |
|   201812 |  4543 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   201812 |  4544 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4545 | `		rc = 0;` |
|   201808 |  4546 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      395 |  4547 | `		rc = rc < 1;` |
|      198 |  4548 | `	}else{` |
|   201410 |  4549 | `		rc = rc < 0;` |
|        - |  4550 | `	}` |
|   201812 |  4551 | `	VmPopOperand(&pTos,1);` |
|   201812 |  4552 | `	if( !pInstr->iP2 ){` |
|        - |  4553 | `		/* Push comparison result without taking the jump */` |
|   201812 |  4554 | `		PH7_MemObjRelease(pTos);` |
|   201812 |  4555 | `		pTos->x.iVal = rc;` |
|        - |  4556 | `		/* Invalidate any prior representation */` |
|   201812 |  4557 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   100929 |  4558 | `	}else{` |
|      ! 0 |  4559 | `		if( rc ){` |
|        - |  4560 | `			/* Jump to the desired location */` |
|      ! 0 |  4561 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4562 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4563 | `		}` |
|        - |  4564 | `	}` |
|   201812 |  4565 | `	break;` |
|        - |  4566 | `				}` |
|        - |  4567 | `/* OP_GT P1 P2 P3` |
|        - |  4568 | ` *` |
|        - |  4569 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4570 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4571 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4572 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4573 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4574 | ` *` |
|        - |  4575 | ` */` |
|        - |  4576 | `/* OP_GE P1 P2 P3` |
|        - |  4577 | ` *` |
|        - |  4578 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4579 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4580 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4581 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4582 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4583 | ` *` |
|        - |  4584 | ` */` |
|    47500 |  4585 | `case PH7_OP_GT:` |
|        - |  4586 | `case PH7_OP_GE: {` |
|    95002 |  4587 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4588 | `	/* Perform the comparison and act accordingly */` |
|        - |  4589 | `#ifdef UNTRUST` |
|        - |  4590 | `	if( pNos < pStack ){` |
|        - |  4591 | `		goto Abort;` |
|        - |  4592 | `	}` |
|        - |  4593 | `#endif` |
|    95002 |  4594 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    95002 |  4595 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4596 | `		rc = 0;` |
|    94998 |  4597 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    94846 |  4598 | `		rc = rc >= 0;` |
|    47424 |  4599 | `	}else{` |
|      150 |  4600 | `		rc = rc > 0;` |
|        - |  4601 | `	}` |
|    95002 |  4602 | `	VmPopOperand(&pTos,1);` |
|    95002 |  4603 | `	if( !pInstr->iP2 ){` |
|        - |  4604 | `		/* Push comparison result without taking the jump */` |
|    95002 |  4605 | `		PH7_MemObjRelease(pTos);` |
|    95002 |  4606 | `		pTos->x.iVal = rc;` |
|        - |  4607 | `		/* Invalidate any prior representation */` |
|    95002 |  4608 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    47502 |  4609 | `	}else{` |
|      ! 0 |  4610 | `		if( rc ){` |
|        - |  4611 | `			/* Jump to the desired location */` |
|      ! 0 |  4612 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4613 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4614 | `		}` |
|        - |  4615 | `	}` |
|    95002 |  4616 | `	break;` |
|        - |  4617 | `				}` |
|        - |  4618 | `/* OP_SEQ P1 P2 *` |
|        - |  4619 | ` * Strict string comparison.` |
|        - |  4620 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4621 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4622 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4623 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4624 | ` * use PH7_OP_EQ.` |
|        - |  4625 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4626 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4627 | ` */` |
|        - |  4628 | `/* OP_SNE P1 P2 *` |
|        - |  4629 | ` * Strict string comparison.` |
|        - |  4630 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4631 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4632 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4633 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4634 | ` * use PH7_OP_EQ.` |
|        - |  4635 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4636 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4637 | ` */` |
|       18 |  4638 | `case PH7_OP_SEQ:` |
|        - |  4639 | `case PH7_OP_SNE: {` |
|       38 |  4640 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4641 | `	SyString s1,s2;` |
|        - |  4642 | `	/* Perform the comparison and act accordingly */` |
|        - |  4643 | `#ifdef UNTRUST` |
|        - |  4644 | `	if( pNos < pStack ){` |
|        - |  4645 | `		goto Abort;` |
|        - |  4646 | `	}` |
|        - |  4647 | `#endif` |
|        - |  4648 | `	/* Force a string cast */` |
|       38 |  4649 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4650 | `		PH7_MemObjToString(pTos);` |
|        2 |  4651 | `	}` |
|       38 |  4652 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4653 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4654 | `	}` |
|       38 |  4655 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4656 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4657 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4658 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4659 | `		rc = rc != 0;` |
|      ! 0 |  4660 | `	}else{` |
|       38 |  4661 | `		rc = rc == 0;` |
|        - |  4662 | `	}` |
|       38 |  4663 | `	VmPopOperand(&pTos,1);` |
|       38 |  4664 | `	if( !pInstr->iP2 ){` |
|        - |  4665 | `		/* Push comparison result without taking the jump */` |
|       38 |  4666 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4667 | `		pTos->x.iVal = rc;` |
|        - |  4668 | `		/* Invalidate any prior representation */` |
|       38 |  4669 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4670 | `	}else{` |
|      ! 0 |  4671 | `		if( rc ){` |
|        - |  4672 | `			/* Jump to the desired location */` |
|      ! 0 |  4673 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4674 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4675 | `		}` |
|        - |  4676 | `	}` |
|       38 |  4677 | `	break;` |
|        - |  4678 | `				 }` |
|        - |  4679 | `/*` |
|        - |  4680 | ` * OP_LOAD_REF * * *` |
|        - |  4681 | ` * Push the index of a referenced object on the stack.` |
|        - |  4682 | ` */` |
|       57 |  4683 | `case PH7_OP_LOAD_REF: {` |
|        - |  4684 | `	sxu32 nIdx;` |
|        - |  4685 | `#ifdef UNTRUST` |
|        - |  4686 | `	if( pTos < pStack ){` |
|        - |  4687 | `		goto Abort;` |
|        - |  4688 | `	}` |
|        - |  4689 | `#endif` |
|        - |  4690 | `	/* Extract memory object index */` |
|      115 |  4691 | `	nIdx = pTos->nIdx;` |
|      115 |  4692 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4693 | `		/* Nullify the object */` |
|       95 |  4694 | `		PH7_MemObjRelease(pTos);` |
|        - |  4695 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4696 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4697 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4698 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4699 | `	}` |
|      115 |  4700 | `	break;` |
|        - |  4701 | `					  }` |
|        - |  4702 | `/*` |
|        - |  4703 | ` * OP_STORE_REF * * P3` |
|        - |  4704 | ` * Perform an assignment operation by reference.` |
|        - |  4705 | ` */` |
|       14 |  4706 | ` case PH7_OP_STORE_REF: {` |
|       30 |  4707 | `	 SyString sName = { 0 , 0 };` |
|        - |  4708 | `	 VmFrame *pFrameLocal;` |
|        - |  4709 | `	SyHashEntry *pEntry;` |
|        - |  4710 | `	sxu32 nIdx;` |
|        - |  4711 | `#ifdef UNTRUST` |
|        - |  4712 | `	if( pTos < pStack ){` |
|        - |  4713 | `		goto Abort;` |
|        - |  4714 | `	}` |
|        - |  4715 | `#endif` |
|       30 |  4716 | `	if( pInstr->p3 == 0 ){` |
|        - |  4717 | `		char *zName;` |
|        - |  4718 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4719 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4720 | `			/* Force a string cast */` |
|      ! 0 |  4721 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4722 | `		}` |
|      ! 0 |  4723 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4724 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4725 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4726 | `			if( zName ){` |
|      ! 0 |  4727 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4728 | `			}` |
|      ! 0 |  4729 | `		}` |
|      ! 0 |  4730 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4731 | `		pTos--;` |
|      ! 0 |  4732 | `	}else{` |
|       30 |  4733 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4734 | `	}` |
|       30 |  4735 | `	nIdx = pTos->nIdx;` |
|       30 |  4736 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4737 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4738 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4739 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4740 | `		}else{` |
|        - |  4741 | `			ph7_value *pObj;` |
|        - |  4742 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4743 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4744 | `			if( pObj == 0 ){` |
|      ! 0 |  4745 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4746 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4747 | `				goto Abort;` |
|        - |  4748 | `			}` |
|        - |  4749 | `			/* Perform the store operation */` |
|      ! 0 |  4750 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4751 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4752 | `		}` |
|       30 |  4753 | `	}else if( sName.nByte > 0){` |
|       30 |  4754 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4755 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4756 | `		}else{` |
|       30 |  4757 | `			pFrameLocal = pVm->pFrame;` |
|       30 |  4758 | `			while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4759 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  4760 | `				pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  4761 | `			}` |
|        - |  4762 | `			/* Query the local frame */` |
|       30 |  4763 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       30 |  4764 | `			if( pEntry ){` |
|      ! 0 |  4765 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4766 | `			}else{` |
|       30 |  4767 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       30 |  4768 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4769 | `					/* Insert in the $GLOBALS array */` |
|       26 |  4770 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       12 |  4771 | `				}` |
|       30 |  4772 | `				if( rc == SXRET_OK ){` |
|       30 |  4773 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       14 |  4774 | `				}` |
|        - |  4775 | `			}` |
|        - |  4776 | `		}` |
|       14 |  4777 | `	}` |
|       30 |  4778 | `	break;` |
|        - |  4779 | `				 }` |
|        - |  4780 | `/*` |
|        - |  4781 | ` * OP_UPLINK P1 * *` |
|        - |  4782 | ` * Link a variable to the top active VM frame.` |
|        - |  4783 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4784 | ` */` |
|       25 |  4785 | `case PH7_OP_UPLINK: {` |
|       52 |  4786 | `	if( pVm->pFrame->pParent ){` |
|       52 |  4787 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4788 | `		SyString sName;` |
|        - |  4789 | `		/* Perform the link */` |
|      104 |  4790 | `		while( pLink <= pTos ){` |
|       54 |  4791 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4792 | `				/* Force a string cast */` |
|      ! 0 |  4793 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4794 | `			}` |
|       54 |  4795 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  4796 | `			if( sName.nByte > 0 ){` |
|       54 |  4797 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  4798 | `			}` |
|       54 |  4799 | `			pLink++;` |
|        2 |  4800 | `		}` |
|       25 |  4801 | `	}` |
|       52 |  4802 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  4803 | `	break;` |
|        - |  4804 | `					}` |
|        - |  4805 | `/*` |
|        - |  4806 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4807 | ` * Push an exception in the corresponding container so that` |
|        - |  4808 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4809 | ` */` |
|       12 |  4810 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       26 |  4811 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4812 | `	VmFrame *pFrameLocal;` |
|       26 |  4813 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4814 | `	/* Create the exception frame */` |
|       26 |  4815 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       26 |  4816 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4817 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4818 | `		goto Abort;` |
|        - |  4819 | `	}` |
|        - |  4820 | `	/* Mark the special frame */` |
|       26 |  4821 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       26 |  4822 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4823 | `	/* Point to the frame that trigger the exception */` |
|       26 |  4824 | `	pFrameLocal = pFrameLocal->pParent;` |
|       28 |  4825 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        3 |  4826 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4827 | `	}` |
|       26 |  4828 | `	pException->pFrame = pFrameLocal;` |
|       26 |  4829 | `	break;` |
|        - |  4830 | `							}` |
|        - |  4831 | `/*` |
|        - |  4832 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4833 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4834 | ` */` |
|       12 |  4835 | `case PH7_OP_POP_EXCEPTION: {` |
|       26 |  4836 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       26 |  4837 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4838 | `		ph7_exception **apException;` |
|        - |  4839 | `		/* Pop the loaded exception */` |
|        7 |  4840 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        7 |  4841 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|        7 |  4842 | `			(void)SySetPop(&pVm->aException);` |
|        3 |  4843 | `		}` |
|        3 |  4844 | `	}` |
|       26 |  4845 | `	pException->pFrame = 0;` |
|        - |  4846 | `	/* Leave the exception frame */` |
|       26 |  4847 | `	VmLeaveFrame(&(*pVm));` |
|       26 |  4848 | `	break;` |
|        - |  4849 | `							}` |
|        - |  4850 |  |
|        - |  4851 | `/*` |
|        - |  4852 | ` * OP_THROW * P2 *` |
|        - |  4853 | ` * Throw an user exception.` |
|        - |  4854 | ` */` |
|       11 |  4855 | `case PH7_OP_THROW: {` |
|       24 |  4856 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       24 |  4857 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4858 | `#ifdef UNTRUST` |
|        - |  4859 | `	if( pTos < pStack ){` |
|        - |  4860 | `		goto Abort;` |
|        - |  4861 | `	}` |
|        - |  4862 | `#endif` |
|       28 |  4863 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4864 | `		/* Safely ignore the exception frame */` |
|        6 |  4865 | `		pFrameLocal = pFrameLocal->pParent;` |
|        2 |  4866 | `	}` |
|        - |  4867 | `	/* Tell the upper layer that an exception was thrown */` |
|       24 |  4868 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       24 |  4869 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       24 |  4870 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4871 | `		ph7_class *pException;` |
|        - |  4872 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4873 | `		 */` |
|       24 |  4874 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       24 |  4875 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4876 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4877 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4878 | `			if( rc == SXERR_ABORT ){` |
|        - |  4879 | `				/* Abort processing immediately */` |
|      ! 0 |  4880 | `				goto Abort;` |
|        - |  4881 | `			}` |
|      ! 0 |  4882 | `		}else{` |
|        - |  4883 | `			/* Throw the exception */` |
|       24 |  4884 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       24 |  4885 | `			if( rc == SXERR_ABORT ){` |
|        - |  4886 | `				/* Abort processing immediately */` |
|        9 |  4887 | `				goto Abort;` |
|        - |  4888 | `			}` |
|        - |  4889 | `		}` |
|        9 |  4890 | `	}else{` |
|        - |  4891 | `		/* Expecting a class instance */` |
|      ! 0 |  4892 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  4893 | `		if( rc == SXERR_ABORT ){` |
|        - |  4894 | `			/* Abort processing immediately */` |
|      ! 0 |  4895 | `			goto Abort;` |
|        - |  4896 | `		}` |
|        - |  4897 | `	}` |
|        - |  4898 | `	/* Pop the top entry */` |
|       16 |  4899 | `	VmPopOperand(&pTos,1);` |
|        - |  4900 | `	/* Perform an unconditional jump */` |
|       16 |  4901 | `	pc = nJump - 1;` |
|       16 |  4902 | `	break;` |
|        - |  4903 | `				   }` |
|        - |  4904 | `/*` |
|        - |  4905 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  4906 | ` * Prepare a foreach step.` |
|        - |  4907 | ` */` |
|     4727 |  4908 | `case PH7_OP_FOREACH_INIT: {` |
|     9456 |  4909 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4910 | `	void *pName;` |
|        - |  4911 | `#ifdef UNTRUST` |
|        - |  4912 | `	if( pTos < pStack ){` |
|        - |  4913 | `		goto Abort;` |
|        - |  4914 | `	}` |
|        - |  4915 | `#endif` |
|     9456 |  4916 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4917 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  4918 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4919 | `			/* Force a string cast */` |
|      ! 0 |  4920 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4921 | `		}` |
|        - |  4922 | `		/* Duplicate name */` |
|      ! 0 |  4923 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4924 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4925 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4926 | `		}` |
|      ! 0 |  4927 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4928 | `	}` |
|     9456 |  4929 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  4930 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4931 | `			/* Force a string cast */` |
|      ! 0 |  4932 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4933 | `		}` |
|        - |  4934 | `		/* Duplicate name */` |
|      ! 0 |  4935 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4936 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4937 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4938 | `		}` |
|      ! 0 |  4939 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4940 | `	}` |
|        - |  4941 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     9456 |  4942 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4943 | `		/* Jump out of the loop */` |
|      ! 0 |  4944 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4945 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4946 | `		}` |
|      ! 0 |  4947 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4948 | `	}else{` |
|        - |  4949 | `		ph7_foreach_step *pStep;` |
|     9456 |  4950 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     9456 |  4951 | `		if( pStep == 0 ){` |
|      ! 0 |  4952 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4953 | `			/* Jump out of the loop */` |
|      ! 0 |  4954 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4955 | `		}else{` |
|        - |  4956 | `			/* Zero the structure */` |
|     9456 |  4957 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4958 | `			/* Prepare the step */` |
|     9456 |  4959 | `			pStep->iFlags = pInfo->iFlags;` |
|     9456 |  4960 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     9448 |  4961 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4962 | `				/* Reset the internal loop cursor */` |
|     9448 |  4963 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4964 | `				/* Mark the step */` |
|     9448 |  4965 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     9448 |  4966 | `				pStep->xIter.pMap = pMap;` |
|     9448 |  4967 | `				pMap->iRef++;` |
|     4725 |  4968 | `			}else{` |
|        9 |  4969 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4970 | `				/* Reset the loop cursor */` |
|        9 |  4971 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|        - |  4972 | `				/* Mark the step */` |
|        9 |  4973 | `				pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  4974 | `				pStep->xIter.pThis = pThis;` |
|        9 |  4975 | `				pThis->iRef++;` |
|        - |  4976 | `			}` |
|        - |  4977 | `		}` |
|     9456 |  4978 | `		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  4979 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  4980 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  4981 | `			/* Jump out of the loop */` |
|      ! 0 |  4982 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4983 | `		}` |
|        - |  4984 | `	}` |
|     9456 |  4985 | `	VmPopOperand(&pTos,1);` |
|     9456 |  4986 | `	break;` |
|        - |  4987 | `						  }` |
|        - |  4988 | `/*` |
|        - |  4989 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  4990 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  4991 | ` */` |
|    75841 |  4992 | `case PH7_OP_FOREACH_STEP: {` |
|   151684 |  4993 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4994 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  4995 | `	ph7_value *pValue;` |
|        - |  4996 | `	VmFrame *pFrameLocal;` |
|        - |  4997 | `	/* Peek the last step */` |
|   151684 |  4998 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   151684 |  4999 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   151684 |  5000 | `	pFrameLocal = pVm->pFrame;` |
|   151684 |  5001 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5002 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  5003 | `		pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  5004 | `	}` |
|   151684 |  5005 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   151660 |  5006 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5007 | `		ph7_hashmap_node *pNode;` |
|        - |  5008 | `		/* Extract the current node value */` |
|   151660 |  5009 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   151660 |  5010 | `		if( pNode == 0 ){` |
|        - |  5011 | `			/* No more entry to process */` |
|     9448 |  5012 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     9448 |  5013 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5014 | `				/* Break the reference with the last element */` |
|        5 |  5015 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  5016 | `			}` |
|        - |  5017 | `			/* Automatically reset the loop cursor */` |
|     9448 |  5018 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5019 | `			/* Cleanup the mess left behind */` |
|     9448 |  5020 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     9448 |  5021 | `			SySetPop(&pInfo->aStep);` |
|     9448 |  5022 | `			PH7_HashmapUnref(pMap);` |
|     4725 |  5023 | `		}else{` |
|   142214 |  5024 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      412 |  5025 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      412 |  5026 | `				if( pKey ){` |
|      412 |  5027 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      205 |  5028 | `				}` |
|      205 |  5029 | `			}` |
|   142214 |  5030 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5031 | `				SyHashEntry *pEntry;` |
|        - |  5032 | `				/* Pass by reference */` |
|       13 |  5033 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       13 |  5034 | `				if( pEntry ){` |
|       13 |  5035 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        7 |  5036 | `				}else{` |
|      ! 0 |  5037 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5038 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5039 | `				}` |
|        7 |  5040 | `			}else{` |
|        - |  5041 | `				/* Make a copy of the entry value */` |
|   142202 |  5042 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   142202 |  5043 | `				if( pValue ){` |
|   142202 |  5044 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    71100 |  5045 | `				}` |
|        - |  5046 | `			}` |
|        - |  5047 | `		}` |
|    75831 |  5048 | `	}else{` |
|       25 |  5049 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5050 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5051 | `		SyHashEntry *pEntry;` |
|        - |  5052 | `		/* Point to the next attribute */` |
|       29 |  5053 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5054 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5055 | `			/* Check access permission */` |
|       31 |  5056 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5057 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5058 | `					break; /* Access is granted */` |
|        - |  5059 | `			}` |
|        1 |  5060 | `		}` |
|       25 |  5061 | `		if( pEntry == 0 ){` |
|        - |  5062 | `			/* Clean up the mess left behind */` |
|        9 |  5063 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5064 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5065 | `				/* Break the reference with the last element */` |
|        3 |  5066 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5067 | `			}` |
|        9 |  5068 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5069 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5070 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5071 | `		}else{` |
|       17 |  5072 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5073 | `			ph7_value *pAttrValue;` |
|       17 |  5074 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5075 | `				/* Fill with the current attribute name */` |
|       17 |  5076 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5077 | `				if( pKey ){` |
|       17 |  5078 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5079 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5080 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5081 | `				}` |
|        8 |  5082 | `			}` |
|        - |  5083 | `			/* Extract attribute value */` |
|       17 |  5084 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5085 | `			if( pAttrValue ){` |
|       17 |  5086 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5087 | `					/* Pass by reference */` |
|        3 |  5088 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5089 | `					if( pEntry ){` |
|        3 |  5090 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5091 | `					}else{` |
|      ! 0 |  5092 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5093 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5094 | `					}` |
|        2 |  5095 | `				}else{` |
|        - |  5096 | `					/* Make a copy of the attribute value */` |
|       15 |  5097 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5098 | `					if( pValue ){` |
|       15 |  5099 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5100 | `					}` |
|        - |  5101 | `				}` |
|        8 |  5102 | `			}` |
|        - |  5103 | `		}` |
|        - |  5104 | `	}` |
|   151684 |  5105 | `	break;` |
|        - |  5106 | `						  }` |
|        - |  5107 | `/*` |
|        - |  5108 | ` * OP_MEMBER P1 P2` |
|        - |  5109 | ` * Load class attribute/method on the stack.` |
|        - |  5110 | ` */` |
|     1935 |  5111 | `case PH7_OP_MEMBER: {` |
|        - |  5112 | `	ph7_class_instance *pThis;` |
|        - |  5113 | `	ph7_value *pNos;` |
|        - |  5114 | `	SyString sName;` |
|     3872 |  5115 | `	if( !pInstr->iP1 ){` |
|     3778 |  5116 | `		pNos = &pTos[-1];` |
|        - |  5117 | `#ifdef UNTRUST` |
|        - |  5118 | `		if( pNos < pStack ){` |
|        - |  5119 | `			goto Abort;` |
|        - |  5120 | `		}` |
|        - |  5121 | `#endif` |
|     3778 |  5122 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5123 | `			ph7_class *pClass;` |
|        - |  5124 | `			/* Class already instantiated */` |
|     3778 |  5125 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5126 | `			/* Point to the instantiated class */` |
|     3778 |  5127 | `			pClass = pThis->pClass;` |
|        - |  5128 | `			/* Extract attribute name first */` |
|     3778 |  5129 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     3778 |  5130 | `			if( pInstr->iP2 ){` |
|        - |  5131 | `				/* Method call */` |
|      254 |  5132 | `				ph7_class_method *pMeth = 0;` |
|      254 |  5133 | `				if( sName.nByte > 0 ){` |
|        - |  5134 | `					/* Extract the target method */` |
|      254 |  5135 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      126 |  5136 | `				}` |
|      254 |  5137 | `				if( pMeth == 0 ){` |
|      ! 0 |  5138 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5139 | `						&pClass->sName,&sName` |
|        - |  5140 | `						);` |
|        - |  5141 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5142 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5143 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5144 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5145 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5146 | `				}else{` |
|        - |  5147 | `					/* Push method name on the stack */` |
|      254 |  5148 | `					PH7_MemObjRelease(pTos);` |
|      254 |  5149 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      254 |  5150 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5151 | `				}` |
|      254 |  5152 | `				pTos->nIdx = SXU32_HIGH;` |
|      128 |  5153 | `			}else{` |
|        - |  5154 | `				/* Attribute access */` |
|     3526 |  5155 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5156 | `				SyHashEntry *pEntry;` |
|        - |  5157 | `				/* Extract the target attribute */` |
|     3526 |  5158 | `				if( sName.nByte > 0 ){` |
|     3526 |  5159 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3526 |  5160 | `					if( pEntry ){` |
|        - |  5161 | `						/* Point to the attribute value */` |
|     3524 |  5162 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1761 |  5163 | `					}` |
|     1762 |  5164 | `				}` |
|     3526 |  5165 | `				if( pObjAttr == 0 ){` |
|        - |  5166 | `					/* No such attribute,load null */` |
|        4 |  5167 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5168 | `						&pClass->sName,&sName);` |
|        - |  5169 | `					/* Call the __get magic method if available */` |
|        3 |  5170 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5171 | `				}` |
|     3526 |  5172 | `				VmPopOperand(&pTos,1);` |
|        - |  5173 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5174 | `				 * This is due to the following case:` |
|        - |  5175 | `				 *     (new TestClass())->foo;` |
|        - |  5176 | `				 */` |
|     3526 |  5177 | `				pThis->iRef++;` |
|     3526 |  5178 | `				PH7_MemObjRelease(pTos);` |
|     3526 |  5179 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3526 |  5180 | `				if( pObjAttr ){` |
|     3524 |  5181 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5182 | `					/* Check attribute access */` |
|     3524 |  5183 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5184 | `						/* Load attribute */` |
|     3524 |  5185 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3524 |  5186 | `						if( pValue ){` |
|     3524 |  5187 | `							if( pThis->iRef < 2 ){` |
|        - |  5188 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5189 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5190 | `								 */` |
|        3 |  5191 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5192 | `							}else{` |
|        - |  5193 | `								/* Simple load */` |
|     3522 |  5194 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5195 | `							}` |
|     3524 |  5196 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3522 |  5197 | `								if( pThis->iRef > 1 ){` |
|        - |  5198 | `									/* Load attribute index */` |
|     3520 |  5199 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1759 |  5200 | `								}` |
|     1760 |  5201 | `							}` |
|     1761 |  5202 | `						}` |
|     1761 |  5203 | `					}` |
|     1761 |  5204 | `				}` |
|        - |  5205 | `				/* Safely unreference the object */` |
|     3526 |  5206 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5207 | `			}` |
|     1890 |  5208 | `		}else{` |
|      ! 0 |  5209 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5210 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5211 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5212 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5213 | `		}` |
|     1890 |  5214 | `	}else{` |
|        - |  5215 | `		/* Static member access using class name */` |
|       96 |  5216 | `		pNos = pTos;` |
|       96 |  5217 | `		pThis = 0;` |
|       96 |  5218 | `		if( !pInstr->p3 ){` |
|       84 |  5219 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       84 |  5220 | `			pNos--;` |
|        - |  5221 | `#ifdef UNTRUST` |
|        - |  5222 | `			if( pNos < pStack ){` |
|        - |  5223 | `				goto Abort;` |
|        - |  5224 | `			}` |
|        - |  5225 | `#endif` |
|       43 |  5226 | `		}else{` |
|        - |  5227 | `			/* Attribute name already computed */` |
|       14 |  5228 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5229 | `		}` |
|       96 |  5230 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|       96 |  5231 | `			ph7_class *pClass = 0;` |
|       96 |  5232 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5233 | `				/* Class already instantiated */` |
|      ! 0 |  5234 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5235 | `				pClass = pThis->pClass;` |
|      ! 0 |  5236 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5237 | `			}else{` |
|        - |  5238 | `				/* Try to extract the target class */` |
|       96 |  5239 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       96 |  5240 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|       96 |  5241 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5242 | `					/* Handle self/static/parent keywords */` |
|       96 |  5243 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       26 |  5244 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       84 |  5245 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       16 |  5246 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       71 |  5247 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       14 |  5248 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       14 |  5249 | `						if( pSelf && pSelf->pBase ){` |
|       14 |  5250 | `							pClass = pSelf->pBase;` |
|        6 |  5251 | `						}` |
|        8 |  5252 | `					}else{` |
|       46 |  5253 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5254 | `					}` |
|       47 |  5255 | `				}` |
|        - |  5256 | `			}` |
|       96 |  5257 | `			if( pClass == 0 ){` |
|        - |  5258 | `				/* Undefined class */` |
|      ! 0 |  5259 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5260 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5261 | `					);` |
|      ! 0 |  5262 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5263 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5264 | `				}` |
|      ! 0 |  5265 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5266 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5267 | `			}else{` |
|       96 |  5268 | `				if( pInstr->iP2 ){` |
|        - |  5269 | `					/* Method call */` |
|       30 |  5270 | `					ph7_class_method *pMeth = 0;` |
|       30 |  5271 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5272 | `						/* Extract the target method */` |
|       30 |  5273 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       14 |  5274 | `					}` |
|       30 |  5275 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5276 | `						if( pMeth ){` |
|      ! 0 |  5277 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5278 | `								&pClass->sName,&sName` |
|        - |  5279 | `								);` |
|      ! 0 |  5280 | `						}else{` |
|      ! 0 |  5281 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5282 | `								&pClass->sName,&sName` |
|        - |  5283 | `								);` |
|        - |  5284 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5285 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5286 | `						}` |
|        - |  5287 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5288 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5289 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5290 | `						}` |
|      ! 0 |  5291 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5292 | `					}else{` |
|        - |  5293 | `						/* Push method name on the stack */` |
|       30 |  5294 | `						PH7_MemObjRelease(pTos);` |
|       30 |  5295 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       30 |  5296 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5297 | `					}` |
|       30 |  5298 | `					pTos->nIdx = SXU32_HIGH;` |
|       16 |  5299 | `				}else{` |
|        - |  5300 | `					/* Attribute access */` |
|       68 |  5301 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5302 | `					/* Check for special ::class pseudo-constant */` |
|       98 |  5303 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       60 |  5304 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5305 | `						/* ::class returns the fully qualified class name */` |
|        - |  5306 | `						/* Pop the attribute name from the stack */` |
|       50 |  5307 | `						if( !pInstr->p3 ){` |
|       50 |  5308 | `							VmPopOperand(&pTos,1);` |
|       24 |  5309 | `						}` |
|       50 |  5310 | `						PH7_MemObjRelease(pTos);` |
|        - |  5311 | `						/* Load the class name */` |
|       50 |  5312 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       50 |  5313 | `						pTos->nIdx = SXU32_HIGH;` |
|       26 |  5314 | `					}else{` |
|        - |  5315 | `						/* Extract the target attribute */` |
|       20 |  5316 | `						if( sName.nByte > 0 ){` |
|       20 |  5317 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        9 |  5318 | `						}` |
|       20 |  5319 | `						if( pAttr == 0 ){` |
|        - |  5320 | `							/* No such attribute,load null */` |
|      ! 0 |  5321 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5322 | `								&pClass->sName,&sName);` |
|        - |  5323 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5324 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5325 | `						}` |
|        - |  5326 | `						/* Pop the attribute name from the stack */` |
|       20 |  5327 | `						if( !pInstr->p3 ){` |
|        7 |  5328 | `							VmPopOperand(&pTos,1);` |
|        3 |  5329 | `						}` |
|       20 |  5330 | `						PH7_MemObjRelease(pTos);` |
|       20 |  5331 | `						pTos->nIdx = SXU32_HIGH;` |
|       20 |  5332 | `						if( pAttr ){` |
|       20 |  5333 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5334 | `								/* Access to a non static attribute */` |
|      ! 0 |  5335 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5336 | `									&pClass->sName,&pAttr->sName` |
|        - |  5337 | `									);` |
|      ! 0 |  5338 | `							}else{` |
|        - |  5339 | `								ph7_value *pValue;` |
|        - |  5340 | `								/* Check if the access to the attribute is allowed */` |
|       20 |  5341 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5342 | `									/* Load the desired attribute */` |
|       20 |  5343 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       20 |  5344 | `									if( pValue ){` |
|       20 |  5345 | `										PH7_MemObjLoad(pValue,pTos);` |
|       20 |  5346 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5347 | `											/* Load index number */` |
|       14 |  5348 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5349 | `										}` |
|        9 |  5350 | `									}` |
|        9 |  5351 | `								}` |
|        - |  5352 | `							}` |
|        9 |  5353 | `						}` |
|        - |  5354 | `					}` |
|        - |  5355 | `				}` |
|       96 |  5356 | `				if( pThis ){` |
|        - |  5357 | `					/* Safely unreference the object */` |
|      ! 0 |  5358 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5359 | `				}` |
|        - |  5360 | `			}` |
|       49 |  5361 | `		}else{` |
|        - |  5362 | `			/* Pop operands */` |
|      ! 0 |  5363 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5364 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5365 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5366 | `			}` |
|      ! 0 |  5367 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5368 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5369 | `		}` |
|        - |  5370 | `	}` |
|     3872 |  5371 | `	break;` |
|        - |  5372 | `					}` |
|        - |  5373 | `/*` |
|        - |  5374 | ` * OP_NEW P1 * * *` |
|        - |  5375 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5376 | ` */` |
|      289 |  5377 | `case PH7_OP_NEW: {` |
|      580 |  5378 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      580 |  5379 | `	ph7_class *pClass = 0;` |
|        - |  5380 | `	ph7_class_instance *pNew;` |
|      580 |  5381 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5382 | `		/* Try to extract the desired class */` |
|      869 |  5383 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      578 |  5384 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      289 |  5385 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5386 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5387 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5388 | `	}` |
|      580 |  5389 | `	if( pClass == 0 ){` |
|        - |  5390 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  5391 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  5392 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5393 | `			);` |
|        - |  5394 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  5395 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5396 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5397 | `			/* Pop given arguments */` |
|      ! 0 |  5398 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5399 | `		}` |
|      ! 0 |  5400 | `		goto Abort;` |
|      ! 0 |  5401 | `	}else{` |
|        - |  5402 | `		ph7_class_method *pCons;` |
|        - |  5403 | `		/* Create a new class instance */` |
|      580 |  5404 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      580 |  5405 | `		if( pNew == 0 ){` |
|      ! 0 |  5406 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5407 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5408 | `				&pClass->sName` |
|        - |  5409 | `			);` |
|      ! 0 |  5410 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5411 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5412 | `				/* Pop given arguments */` |
|      ! 0 |  5413 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5414 | `			}` |
|      ! 0 |  5415 | `			break;` |
|        - |  5416 | `		}` |
|        - |  5417 | `		/* Check if a constructor is available */` |
|      580 |  5418 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      580 |  5419 | `		if( pCons == 0 ){` |
|      518 |  5420 | `			SyString *pName = &pClass->sName;` |
|        - |  5421 | `			/* Check for a constructor with the same base class name */` |
|      518 |  5422 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      258 |  5423 | `		}` |
|      580 |  5424 | `		if( pCons ){` |
|        - |  5425 | `			/* Call the class constructor */` |
|       64 |  5426 | `			SySetReset(&aArg);` |
|      116 |  5427 | `			while( pArg < pTos ){` |
|       54 |  5428 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       54 |  5429 | `				pArg++;` |
|        2 |  5430 | `			}` |
|       64 |  5431 | `			if( pVm->bErrReport ){` |
|        - |  5432 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5433 | `				sxu32 n;` |
|       21 |  5434 | `				n = SySetUsed(&aArg);` |
|        - |  5435 | `				/* Emit a notice for missing arguments */` |
|       49 |  5436 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       29 |  5437 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       29 |  5438 | `					if( pFuncArg ){` |
|       29 |  5439 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5440 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5441 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5442 | `						}` |
|       14 |  5443 | `					}` |
|       29 |  5444 | `					n++;` |
|        1 |  5445 | `				}` |
|       10 |  5446 | `			}` |
|       64 |  5447 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5448 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       64 |  5449 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5450 | `				pNew->iRef = 1;` |
|      ! 0 |  5451 | `			}` |
|       31 |  5452 | `		}` |
|      580 |  5453 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5454 | `			/* Pop given arguments */` |
|       48 |  5455 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       23 |  5456 | `		}` |
|      580 |  5457 | `		PH7_MemObjRelease(pTos);` |
|      580 |  5458 | `		pTos->x.pOther = pNew;` |
|      580 |  5459 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5460 | `	}` |
|      580 |  5461 | `	break;` |
|        - |  5462 | `				 }` |
|        - |  5463 | `/*` |
|        - |  5464 | ` * OP_CLONE * * *` |
|        - |  5465 | ` * Perfome a clone operation.` |
|        - |  5466 | ` */` |
|       23 |  5467 | `case PH7_OP_CLONE: {` |
|        - |  5468 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5469 | `#ifdef UNTRUST` |
|        - |  5470 | `	if( pTos < pStack ){` |
|        - |  5471 | `		goto Abort;` |
|        - |  5472 | `	}` |
|        - |  5473 | `#endif` |
|        - |  5474 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5475 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5476 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5477 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5478 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5479 | `		break;` |
|        - |  5480 | `	}` |
|        - |  5481 | `	/* Point to the source */` |
|       44 |  5482 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5483 | `	/* Perform the clone operation */` |
|       44 |  5484 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5485 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5486 | `	if( pClone == 0 ){` |
|      ! 0 |  5487 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5488 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5489 | `	}else{` |
|        - |  5490 | `		/* Load the cloned object */` |
|       44 |  5491 | `		pTos->x.pOther = pClone;` |
|       44 |  5492 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5493 | `	}` |
|       44 |  5494 | `	break;` |
|        - |  5495 | `				   }` |
|        - |  5496 | `/*` |
|        - |  5497 | ` * OP_SWITCH * * P3` |
|        - |  5498 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5499 | ` */` |
|       18 |  5500 | `case PH7_OP_SWITCH: {` |
|       38 |  5501 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5502 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5503 | `	ph7_value sValue,sCaseValue;` |
|        - |  5504 | `	sxu32 n,nEntry;` |
|        - |  5505 | `#ifdef UNTRUST` |
|        - |  5506 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5507 | `		goto Abort;` |
|        - |  5508 | `	}` |
|        - |  5509 | `#endif` |
|        - |  5510 | `	/* Point to the case table  */` |
|       38 |  5511 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5512 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5513 | `	/* Select the appropriate case block to execute */` |
|       38 |  5514 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5515 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5516 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5517 | `		pCase = &aCase[n];` |
|       92 |  5518 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5519 | `		/* Execute the case expression first */` |
|       92 |  5520 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5521 | `		/* Compare the two expression */` |
|       92 |  5522 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5523 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5524 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5525 | `		if( rc == 0 ){` |
|        - |  5526 | `			/* Value match,jump to this block */` |
|       38 |  5527 | `			pc = pCase->nStart - 1;` |
|       38 |  5528 | `			break;` |
|        - |  5529 | `		}` |
|       29 |  5530 | `	}` |
|       38 |  5531 | `	VmPopOperand(&pTos,1);` |
|       38 |  5532 | `	if( n >= nEntry ){` |
|        - |  5533 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5534 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5535 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5536 | `		}else{` |
|        - |  5537 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5538 | `			pc = pSwitch->nOut - 1;` |
|        - |  5539 | `		}` |
|      ! 0 |  5540 | `	}` |
|       38 |  5541 | `	break;` |
|        - |  5542 | `					}` |
|        - |  5543 | `/*` |
|        - |  5544 | ` * OP_CALL P1 * *` |
|        - |  5545 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5546 | ` *  function on the stack.` |
|        - |  5547 | ` */` |
|   276517 |  5548 | `case PH7_OP_CALL: {` |
|   553080 |  5549 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5550 | `	SyHashEntry *pEntry;` |
|        - |  5551 | `	SyString sName;` |
|        - |  5552 | `	/* Extract function name */` |
|   553080 |  5553 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5554 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5555 | `			ph7_value sResult;` |
|      ! 0 |  5556 | `			SySetReset(&aArg);` |
|      ! 0 |  5557 | `			while( pArg < pTos ){` |
|      ! 0 |  5558 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5559 | `				pArg++;` |
|      ! 0 |  5560 | `			}` |
|      ! 0 |  5561 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5562 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5563 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5564 | `			SySetReset(&aArg);` |
|        - |  5565 | `			/* Pop given arguments */` |
|      ! 0 |  5566 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5567 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5568 | `			}` |
|        - |  5569 | `			/* Copy result */` |
|      ! 0 |  5570 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5571 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5572 | `		}else{` |
|        3 |  5573 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5574 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5575 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5576 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5577 | `			}else{` |
|        - |  5578 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5579 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5580 | `			}` |
|        - |  5581 | `			/* Pop given arguments */` |
|        3 |  5582 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5583 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5584 | `			}` |
|        - |  5585 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5586 | `			PH7_MemObjRelease(pTos);` |
|        - |  5587 | `		}` |
|   276284 |  5588 | `		break;` |
|        - |  5589 | `	}` |
|   553078 |  5590 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5591 | `	/* Check for a compiled function first.` |
|        - |  5592 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  5593 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   553078 |  5594 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  5595 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  5596 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  5597 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  5598 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  5599 | `	 * function calls inside namespaces. */` |
|   553078 |  5600 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  5601 | `		const char *zFunc;` |
|        - |  5602 | `		const char *zEnd;` |
|        - |  5603 | `		const char *z;` |
|        - |  5604 | `		SyString sGlobal;` |
|       15 |  5605 | `		zFunc = sName.zString;` |
|       15 |  5606 | `		zEnd  = zFunc + sName.nByte;` |
|       15 |  5607 | `		z = zEnd;` |
|        - |  5608 | `		/* Find last namespace separator */` |
|      133 |  5609 | `		while( z > zFunc ){` |
|      133 |  5610 | `			if( z[-1] == '\\' ){` |
|       15 |  5611 | `				break;` |
|        - |  5612 | `			}` |
|      119 |  5613 | `			z--;` |
|        1 |  5614 | `		}` |
|       15 |  5615 | `		if( z > zFunc && z < zEnd ){` |
|        - |  5616 | `			/* Retry lookup using the unqualified/global function name */` |
|       15 |  5617 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       15 |  5618 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        7 |  5619 | `		}` |
|        7 |  5620 | `	}` |
|   553078 |  5621 | `	if( pEntry ){` |
|        - |  5622 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5623 | `		ph7_class_instance *pThis;` |
|        - |  5624 | `		ph7_value *pFrameStack;` |
|        - |  5625 | `		ph7_vm_func *pVmFunc;` |
|        - |  5626 | `		ph7_class *pSelf;` |
|        - |  5627 | `		VmFrame *pFrame;` |
|        - |  5628 | `		ph7_value *pObj;` |
|        - |  5629 | `		VmSlot sArg;` |
|        - |  5630 | `		sxu32 n;` |
|        - |  5631 | `		/* initialize fields */` |
|    12060 |  5632 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    12060 |  5633 | `		pThis = 0;` |
|    12060 |  5634 | `		pSelf = 0;` |
|    12060 |  5635 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5636 | `			ph7_class_method *pMeth;` |
|        - |  5637 | `			/* Class method call */` |
|     1432 |  5638 | `			ph7_value *pTarget = &pTos[-1];` |
|     1432 |  5639 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5640 | `				/* Extract the 'this' pointer */` |
|     1432 |  5641 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5642 | `					/* Instance already loaded */` |
|     1398 |  5643 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1398 |  5644 | `					pThis->iRef++;` |
|     1398 |  5645 | `					pSelf = pThis->pClass;` |
|      698 |  5646 | `				}` |
|     1432 |  5647 | `				if( pSelf == 0 ){` |
|       36 |  5648 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5649 | `						/* "Late Static Binding" class name */` |
|       44 |  5650 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       14 |  5651 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       14 |  5652 | `					}` |
|       36 |  5653 | `					if( pSelf == 0 ){` |
|       13 |  5654 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        6 |  5655 | `					}` |
|       17 |  5656 | `				}` |
|     1432 |  5657 | `				if( pThis == 0  ){` |
|       36 |  5658 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       36 |  5659 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5660 | `						/* Safely ignore the exception frame */` |
|      ! 0 |  5661 | `						pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  5662 | `					}` |
|       36 |  5663 | `					if( pFrameLocal->pParent ){` |
|        - |  5664 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5665 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5666 | `						if( pThis ){` |
|       13 |  5667 | `							pThis->iRef++;` |
|        6 |  5668 | `						}` |
|        9 |  5669 | `					}` |
|       17 |  5670 | `				}` |
|     1432 |  5671 | `				VmPopOperand(&pTos,1);` |
|     1432 |  5672 | `				PH7_MemObjRelease(pTos);` |
|        - |  5673 | `				/* Synchronize pointers */` |
|     1432 |  5674 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5675 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5676 | `				 * user have already computed the random generated unique class method name` |
|        - |  5677 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5678 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5679 | `				 */` |
|     1432 |  5680 | `				while( pArg < pStack ){` |
|      ! 0 |  5681 | `					pArg++;` |
|      ! 0 |  5682 | `				}` |
|     1432 |  5683 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5684 | `					/* Check if the call is allowed */` |
|     1432 |  5685 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1432 |  5686 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        8 |  5687 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5688 | `							/* Pop given arguments */` |
|      ! 0 |  5689 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5690 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5691 | `							}` |
|        - |  5692 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5693 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5694 | `							break;` |
|        - |  5695 | `						}` |
|        3 |  5696 | `					}` |
|      715 |  5697 | `				}` |
|      715 |  5698 | `			}` |
|      715 |  5699 | `		}` |
|        - |  5700 | `		/* Check The recursion limit */` |
|    12060 |  5701 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5702 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5703 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5704 | `				&pVmFunc->sName);` |
|        - |  5705 | `			/* Pop given arguments */` |
|        3 |  5706 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5707 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5708 | `			}` |
|        - |  5709 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5710 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5711 | `			break;` |
|        - |  5712 | `		}` |
|    12058 |  5713 | `		if( pVmFunc->pNextName ){` |
|        - |  5714 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      134 |  5715 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       66 |  5716 | `		}` |
|        - |  5717 | `		/* Extract the formal argument set */` |
|    12058 |  5718 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5719 | `		/* Create a new VM frame  */` |
|    12058 |  5720 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    12058 |  5721 | `		if( rc != SXRET_OK ){` |
|        - |  5722 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5723 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5724 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5725 | `				&pVmFunc->sName);` |
|        - |  5726 | `			/* Pop given arguments */` |
|      ! 0 |  5727 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5728 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5729 | `			}` |
|        - |  5730 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5731 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5732 | `			break;` |
|        - |  5733 | `		}` |
|    12058 |  5734 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5735 | `			/* Install the '$this' variable */` |
|        - |  5736 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1408 |  5737 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1408 |  5738 | `			if( pObj ){` |
|        - |  5739 | `				/* Reflect the change */` |
|     1408 |  5740 | `				pObj->x.pOther = pThis;` |
|     1408 |  5741 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      703 |  5742 | `			}` |
|      703 |  5743 | `		}` |
|    12058 |  5744 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5745 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5746 | `			/* Install static variables */` |
|      ! 0 |  5747 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5748 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5749 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5750 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5751 | `					/* Initialize the static variables */` |
|      ! 0 |  5752 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5753 | `					if( pObj ){` |
|        - |  5754 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5755 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5756 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5757 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5758 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5759 | `						}` |
|      ! 0 |  5760 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5761 | `					}else{` |
|      ! 0 |  5762 | `						continue;` |
|        - |  5763 | `					}` |
|      ! 0 |  5764 | `				}` |
|        - |  5765 | `				/* Install in the current frame */` |
|      ! 0 |  5766 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5767 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5768 | `			}` |
|      ! 0 |  5769 | `		}` |
|        - |  5770 | `		/* Push arguments in the local frame */` |
|    12058 |  5771 | `		n = 0;` |
|    33502 |  5772 | `		while( pArg < pTos ){` |
|    21446 |  5773 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    21296 |  5774 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5775 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5776 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5777 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5778 | `						goto Abort;` |
|        - |  5779 | `					}` |
|      ! 0 |  5780 | `				}` |
|        - |  5781 | `				/* Make sure the given arguments are of the correct type */` |
|    21296 |  5782 | `				if( aFormalArg[n].nType > 0 ){` |
|     1088 |  5783 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5784 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5785 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5786 | `						ph7_class *pClass;` |
|        - |  5787 | `						/* Try to extract the desired class */` |
|      ! 0 |  5788 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5789 | `						if( pClass ){` |
|      ! 0 |  5790 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5791 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5792 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5793 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5794 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5795 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5796 | `								}` |
|      ! 0 |  5797 | `							}else{` |
|        - |  5798 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5799 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5800 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5801 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5802 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5803 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5804 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5805 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5806 | `								}` |
|        - |  5807 | `							}` |
|      ! 0 |  5808 | `						}` |
|     1088 |  5809 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5810 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5811 | `						/* Cast to the desired type */` |
|      ! 0 |  5812 | `						xCast(pArg);` |
|      ! 0 |  5813 | `					}` |
|      543 |  5814 | `				}` |
|    21296 |  5815 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5816 | `					/* Pass by reference */` |
|       48 |  5817 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5818 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5819 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5820 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5821 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5822 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5823 | `						}` |
|        - |  5824 | `						/* Switch to pass by value */` |
|      ! 0 |  5825 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5826 | `					}else{` |
|        - |  5827 | `						SyHashEntry *pRefEntry;` |
|        - |  5828 | `						/* Install the referenced variable in the private function frame */` |
|       48 |  5829 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       48 |  5830 | `						if( pRefEntry == 0 ){` |
|       71 |  5831 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       46 |  5832 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       48 |  5833 | `							sArg.nIdx = pArg->nIdx;` |
|       48 |  5834 | `							sArg.pUserData = 0;` |
|       48 |  5835 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  5836 | `						}` |
|       48 |  5837 | `						pObj = 0;` |
|        - |  5838 | `					}` |
|       25 |  5839 | `				}else{` |
|        - |  5840 | `					/* Pass by value,make a copy of the given argument */` |
|    21250 |  5841 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5842 | `				}` |
|    10649 |  5843 | `			}else{` |
|        - |  5844 | `				char zName[32];` |
|        - |  5845 | `				SyString sArgName;` |
|        - |  5846 | `				/* Set a dummy name */` |
|      152 |  5847 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      152 |  5848 | `				sArgName.zString = zName;` |
|        - |  5849 | `				/* Annonymous argument */` |
|      152 |  5850 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5851 | `			}` |
|    21446 |  5852 | `			if( pObj ){` |
|    21400 |  5853 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5854 | `				/* Insert argument index  */` |
|    21400 |  5855 | `				sArg.nIdx = pObj->nIdx;` |
|    21400 |  5856 | `				sArg.pUserData = 0;` |
|    21400 |  5857 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    10699 |  5858 | `			}` |
|    21446 |  5859 | `			PH7_MemObjRelease(pArg);` |
|    21446 |  5860 | `			pArg++;` |
|    21446 |  5861 | `			++n;` |
|        2 |  5862 | `		}` |
|        - |  5863 | `		/* Set up closure environment */` |
|    12058 |  5864 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5865 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  5866 | `			ph7_value *pValue;` |
|        - |  5867 | `			sxu32 iEnv;` |
|        9 |  5868 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  5869 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  5870 | `				pEnv = &aEnv[iEnv];` |
|       17 |  5871 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  5872 | `					/* Do not install null value */` |
|        9 |  5873 | `					continue;` |
|        - |  5874 | `				}` |
|        9 |  5875 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  5876 | `				if( pValue == 0 ){` |
|      ! 0 |  5877 | `					continue;` |
|        - |  5878 | `				}` |
|        - |  5879 | `				/* Invalidate any prior representation */` |
|        9 |  5880 | `				PH7_MemObjRelease(pValue);` |
|        - |  5881 | `				/* Duplicate bound variable value */` |
|        9 |  5882 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  5883 | `			}` |
|        4 |  5884 | `		}` |
|        - |  5885 | `		/* Process default values */` |
|    13896 |  5886 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1840 |  5887 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1834 |  5888 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1834 |  5889 | `				if( pObj ){` |
|        - |  5890 | `					/* Evaluate the default value and extract it's result */` |
|     1834 |  5891 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1834 |  5892 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5893 | `						goto Abort;` |
|        - |  5894 | `					}` |
|        - |  5895 | `					/* Insert argument index */` |
|     1834 |  5896 | `					sArg.nIdx = pObj->nIdx;` |
|     1834 |  5897 | `					sArg.pUserData = 0;` |
|     1834 |  5898 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  5899 | `					/* Make sure the default argument is of the correct type */` |
|     1834 |  5900 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5901 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5902 | `						/* Cast to the desired type */` |
|      ! 0 |  5903 | `						xCast(pObj);` |
|      ! 0 |  5904 | `					}` |
|      916 |  5905 | `				}` |
|      916 |  5906 | `			}` |
|     1840 |  5907 | `			++n;` |
|        2 |  5908 | `		}` |
|        - |  5909 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  5910 | `		 * does not return anything.` |
|        - |  5911 | `		 */` |
|    12058 |  5912 | `		PH7_MemObjRelease(pTos);` |
|    12058 |  5913 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  5914 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    12058 |  5915 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    12058 |  5916 | `		if( pFrameStack == 0 ){` |
|        - |  5917 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5918 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5919 | `				&pVmFunc->sName);` |
|      ! 0 |  5920 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5921 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5922 | `			}` |
|      ! 0 |  5923 | `			break;` |
|        - |  5924 | `		}` |
|    12058 |  5925 | `		if( pSelf ){` |
|        - |  5926 | `			/* Push class name */` |
|     1430 |  5927 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      714 |  5928 | `		}` |
|        - |  5929 | `		/* Increment nesting level */` |
|    12058 |  5930 | `		pVm->nRecursionDepth++;` |
|        - |  5931 | `		/* Execute function body */` |
|    12058 |  5932 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  5933 | `		/* Decrement nesting level */` |
|    12058 |  5934 | `		pVm->nRecursionDepth--;` |
|    12058 |  5935 | `		if( pSelf ){` |
|        - |  5936 | `			/* Pop class name */` |
|     1430 |  5937 | `			(void)SySetPop(&pVm->aSelf);` |
|      714 |  5938 | `		}` |
|        - |  5939 | `		/* Cleanup the mess left behind */` |
|    12058 |  5940 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  5941 | `			/* Return by reference,reflect that */` |
|        9 |  5942 | `			if( n != SXU32_HIGH ){` |
|        9 |  5943 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  5944 | `				sxu32 i;` |
|        - |  5945 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  5946 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  5947 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  5948 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  5949 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5950 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5951 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  5952 | `								&pVmFunc->sName);` |
|      ! 0 |  5953 | `						}` |
|      ! 0 |  5954 | `						n = SXU32_HIGH;` |
|      ! 0 |  5955 | `						break;` |
|        - |  5956 | `					}` |
|        3 |  5957 | `				}` |
|        5 |  5958 | `			}else{` |
|      ! 0 |  5959 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5960 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5961 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  5962 | `						&pVmFunc->sName);` |
|      ! 0 |  5963 | `				}` |
|        - |  5964 | `			}` |
|        9 |  5965 | `			pTos->nIdx = n;` |
|        4 |  5966 | `		}` |
|        - |  5967 | `		/* Cleanup the mess left behind */` |
|    12058 |  5968 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  5969 | `			/* An exception was throw in this frame */` |
|        7 |  5970 | `			pFrame = pFrame->pParent;` |
|        7 |  5971 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  5972 | `				/* Pop the resutlt */` |
|        5 |  5973 | `				VmPopOperand(&pTos,1);` |
|        - |  5974 | `				/* Jump to this destination */` |
|        5 |  5975 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  5976 | `				rc = PH7_OK;` |
|        3 |  5977 | `			}else{` |
|        3 |  5978 | `				if( pFrame->pParent ){` |
|        3 |  5979 | `					rc = PH7_EXCEPTION;` |
|        2 |  5980 | `				}else{` |
|        - |  5981 | `					/* Continue normal execution */` |
|      ! 0 |  5982 | `					rc = PH7_OK;` |
|        - |  5983 | `				}` |
|        - |  5984 | `			}` |
|        3 |  5985 | `		}` |
|        - |  5986 | `		/* Free the operand stack */` |
|    12058 |  5987 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  5988 | `		/* Leave the frame */` |
|    12058 |  5989 | `		VmLeaveFrame(&(*pVm));` |
|    12058 |  5990 | `		if( rc == PH7_ABORT ){` |
|        - |  5991 | `			/* Abort processing immeditaley */` |
|        7 |  5992 | `			goto Abort;` |
|    12052 |  5993 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5994 | `			goto Exception;` |
|        - |  5995 | `		}` |
|     6026 |  5996 | `	}else{` |
|        - |  5997 | `		ph7_user_func *pFunc;` |
|        - |  5998 | `		ph7_context sCtx;` |
|        - |  5999 | `		ph7_value sRet;` |
|        - |  6000 | `		/* Look for an installed foreign function.` |
|        - |  6001 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6002 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6003 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6004 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6005 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6006 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   541020 |  6007 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   541020 |  6008 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6009 | `			/* Compiler-qualified: try short name as global fallback */` |
|       15 |  6010 | `			const char *zShort = sName.zString;` |
|        - |  6011 | `			sxu32 i;` |
|      217 |  6012 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      203 |  6013 | `				if( sName.zString[i] == '\\' ){` |
|       19 |  6014 | `					zShort = &sName.zString[i + 1];` |
|        9 |  6015 | `				}` |
|      102 |  6016 | `			}` |
|       15 |  6017 | `			if( zShort != sName.zString ){` |
|       15 |  6018 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       15 |  6019 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        7 |  6020 | `			}` |
|        7 |  6021 | `		}` |
|   541020 |  6022 | `		if( pEntry == 0 ){` |
|        - |  6023 | `			/* Call to undefined function */` |
|        5 |  6024 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6025 | `			/* Pop given arguments */` |
|        5 |  6026 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6027 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6028 | `			}` |
|        - |  6029 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6030 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6031 | `			break;` |
|        - |  6032 | `		}` |
|   541016 |  6033 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6034 | `		/* Start collecting function arguments */` |
|   541016 |  6035 | `		SySetReset(&aArg);` |
|  1453906 |  6036 | `		while( pArg < pTos ){` |
|   912892 |  6037 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   912892 |  6038 | `			pArg++;` |
|        2 |  6039 | `		}` |
|        - |  6040 | `		/* Assume a null return value */` |
|   541016 |  6041 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6042 | `		/* Init the call context */` |
|   541016 |  6043 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6044 | `		/* Call the foreign function */` |
|   541016 |  6045 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6046 | `		/* Release the call context */` |
|   541016 |  6047 | `		VmReleaseCallContext(&sCtx);` |
|   541016 |  6048 | `		if( rc == PH7_ABORT ){` |
|      463 |  6049 | `			goto Abort;` |
|   540554 |  6050 | `		}else if( rc == PH7_EXCEPTION ){` |
|        7 |  6051 | `			VmFrame *pFrm = pVm->pFrame;` |
|       13 |  6052 | `			while( pFrm->pParent && (pFrm->iFlags & VM_FRAME_EXCEPTION) ){` |
|        7 |  6053 | `				pFrm = pFrm->pParent;` |
|        1 |  6054 | `			}` |
|        7 |  6055 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6056 | `				/* Exception was NOT caught, propagate */` |
|      ! 0 |  6057 | `				goto Exception;` |
|        - |  6058 | `			}` |
|        - |  6059 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6060 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6061 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6062 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6063 | `			}` |
|        - |  6064 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6065 | `			VmPopOperand(&pTos,1);` |
|        - |  6066 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6067 | `			pFrm = pVm->pFrame;` |
|        7 |  6068 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6069 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6070 | `			}` |
|        7 |  6071 | `			break;` |
|        - |  6072 | `		}` |
|   540548 |  6073 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6074 | `			/* Pop function name and arguments */` |
|   523264 |  6075 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   261653 |  6076 | `		}` |
|        - |  6077 | `		/* Save foreign function return value */` |
|   540548 |  6078 | `		PH7_MemObjStore(&sRet,pTos);` |
|   540548 |  6079 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6080 | `	}` |
|   552596 |  6081 | `	break;` |
|        - |  6082 | `				  }` |
|        - |  6083 | `/*` |
|        - |  6084 | ` * OP_CONSUME: P1 * *` |
|        - |  6085 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6086 | ` */` |
|    10742 |  6087 | `case PH7_OP_CONSUME: {` |
|    21486 |  6088 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    21486 |  6089 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6090 |  |
|    21486 |  6091 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    21486 |  6092 | `	pCur = pOut;` |
|        - |  6093 | `	/* Start the consume process  */` |
|    42970 |  6094 | `	while( pOut <= pTos ){` |
|        - |  6095 | `		/* Force a string cast */` |
|    21486 |  6096 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      300 |  6097 | `			PH7_MemObjToString(pOut);` |
|      149 |  6098 | `		}` |
|    21486 |  6099 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6100 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6101 | `			/* Invoke the output consumer callback */` |
|    11768 |  6102 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    11768 |  6103 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6104 | `				/* Increment output length */` |
|     5322 |  6105 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     2660 |  6106 | `			}` |
|    11768 |  6107 | `			SyBlobRelease(&pOut->sBlob);` |
|    11768 |  6108 | `			if( rc == SXERR_ABORT ){` |
|        - |  6109 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6110 | `				goto Abort;` |
|        - |  6111 | `			}` |
|     5883 |  6112 | `		}` |
|    21486 |  6113 | `		pOut++;` |
|        2 |  6114 | `	}` |
|    21486 |  6115 | `	pTos = &pCur[-1];` |
|    21484 |  6116 | `	break;` |
|        - |  6117 | `					 }` |
|        - |  6118 |  |
|        - |  6119 | `		} /* Switch() */` |
|  9512316 |  6120 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6121 | `	} /* For(;;) */` |
|    14842 |  6122 | `Done:` |
|    29686 |  6123 | `	SySetRelease(&aArg);` |
|    29686 |  6124 | `	return SXRET_OK;` |
|      238 |  6125 | `Abort:` |
|      477 |  6126 | `	SySetRelease(&aArg);` |
|     1661 |  6127 | `	while( pTos >= pStack ){` |
|     1185 |  6128 | `		PH7_MemObjRelease(pTos);` |
|     1185 |  6129 | `		pTos--;` |
|        1 |  6130 | `	}` |
|      477 |  6131 | `	return PH7_ABORT;` |
|        1 |  6132 | `Exception:` |
|        3 |  6133 | `	SySetRelease(&aArg);` |
|        5 |  6134 | `	while( pTos >= pStack ){` |
|        3 |  6135 | `		PH7_MemObjRelease(pTos);` |
|        3 |  6136 | `		pTos--;` |
|        1 |  6137 | `	}` |
|        3 |  6138 | `	return PH7_EXCEPTION;` |
|    15083 |  6139 |  |
|        - |  6140 | `/*` |
|        - |  6141 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6142 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6143 | ` * See block-comment on that function for additional information.` |
|        - |  6144 | ` */` |
|    14340 |  6145 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6146 |  |
|        - |  6147 | `	ph7_value *pStack;` |
|        - |  6148 | `	sxi32 rc;` |
|        - |  6149 | `	/* Allocate a new operand stack */` |
|    14342 |  6150 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    14342 |  6151 | `	if( pStack == 0 ){` |
|      ! 0 |  6152 | `		return SXERR_MEM;` |
|        - |  6153 | `	}` |
|        - |  6154 | `	/* Execute the program */` |
|    14342 |  6155 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  6156 | `	/* Free the operand stack */` |
|    14342 |  6157 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6158 | `	/* Execution result */` |
|    14342 |  6159 | `	return rc;` |
|     7172 |  6160 |  |
|        - |  6161 | `/*` |
|        - |  6162 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6163 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6164 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6165 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6166 | ` * execution ends.` |
|        - |  6167 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6168 | ` * additional information.` |
|        - |  6169 | ` */` |
|     2170 |  6170 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6171 |  |
|        - |  6172 | `	VmShutdownCB *pEntry;` |
|        - |  6173 | `	ph7_value *apArg[10];` |
|        - |  6174 | `	sxu32 n,nEntry;` |
|        - |  6175 | `	int i;` |
|        - |  6176 | `	/* Point to the stack of registered callbacks */` |
|     2172 |  6177 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    23872 |  6178 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    21702 |  6179 | `		apArg[i] = 0;` |
|    10852 |  6180 | `	}` |
|     2174 |  6181 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6182 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6183 | `		if( pEntry ){` |
|        - |  6184 | `			/* Prepare callback arguments if any */` |
|        3 |  6185 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6186 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6187 | `					break;` |
|        - |  6188 | `				}` |
|      ! 0 |  6189 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6190 | `			}` |
|        - |  6191 | `			/* Invoke the callback */` |
|        3 |  6192 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6193 | `			/*` |
|        - |  6194 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6195 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6196 | `			 */` |
|        3 |  6197 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6198 | `			if( pEntry ){` |
|        3 |  6199 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6200 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6201 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6202 | `				}` |
|        1 |  6203 | `			}` |
|        1 |  6204 | `		}` |
|        2 |  6205 | `	}` |
|     2172 |  6206 | `	SySetReset(&pVm->aShutdown);` |
|     2172 |  6207 |  |
|        - |  6208 | `/*` |
|        - |  6209 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6210 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6211 | ` * See block-comment on that function for additional information.` |
|        - |  6212 | ` */` |
|     2178 |  6213 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6214 |  |
|        - |  6215 | `	/* Make sure we are ready to execute this program */` |
|     2180 |  6216 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6217 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6218 | `	}` |
|        - |  6219 | `	/* Set the execution magic number  */` |
|     2180 |  6220 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6221 | `	/* Execute the program */` |
|     2180 |  6222 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6223 | `	/* Invoke any shutdown callbacks */` |
|     2176 |  6224 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6225 | `	/*` |
|        - |  6226 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6227 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6228 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6229 | `	 */` |
|     2176 |  6230 | `	return SXRET_OK;` |
|     1091 |  6231 |  |
|        - |  6232 | `/*` |
|        - |  6233 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6234 | ` * the desired message.` |
|        - |  6235 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6236 | ` * in 'api.c' for additional information.` |
|        - |  6237 | ` */` |
|      350 |  6238 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6239 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6240 | `	SyString *pString /* Message to output */` |
|        - |  6241 | `	)` |
|        2 |  6242 |  |
|      352 |  6243 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      352 |  6244 | `	sxi32 rc = SXRET_OK;` |
|        - |  6245 | `	/* Call the output consumer */` |
|      352 |  6246 | `	if( pString->nByte > 0 ){` |
|      352 |  6247 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      352 |  6248 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6249 | `			/* Increment output length */` |
|       17 |  6250 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6251 | `		}` |
|      175 |  6252 | `	}` |
|      352 |  6253 | `	return rc;` |
|        2 |  6254 |  |
|        - |  6255 | `/*` |
|        - |  6256 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6257 | ` * callback to consume the formatted message.` |
|        - |  6258 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6259 | ` * in 'api.c' for additional information.` |
|        - |  6260 | ` */` |
|        2 |  6261 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6262 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6263 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6264 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6265 | `	)` |
|        1 |  6266 |  |
|        3 |  6267 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6268 | `	sxi32 rc = SXRET_OK;` |
|        - |  6269 | `	SyBlob sWorker;` |
|        - |  6270 | `	/* Format the message and call the output consumer */` |
|        3 |  6271 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6272 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6273 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6274 | `		/* Consume the formatted message */` |
|        3 |  6275 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6276 | `	}` |
|        3 |  6277 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6278 | `		/* Increment output length */` |
|      ! 0 |  6279 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6280 | `	}` |
|        - |  6281 | `	/* Release the working buffer */` |
|        3 |  6282 | `	SyBlobRelease(&sWorker);` |
|        3 |  6283 | `	return rc;` |
|        1 |  6284 |  |
|        - |  6285 | `/*` |
|        - |  6286 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6287 | ` * This function never fail and always return a pointer` |
|        - |  6288 | ` * to a null terminated string.` |
|        - |  6289 | ` */` |
|       12 |  6290 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6291 |  |
|       13 |  6292 | `	const char *zOp = "Unknown     ";` |
|       13 |  6293 | `	switch(nOp){` |
|        3 |  6294 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6295 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6296 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6297 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6298 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6299 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6300 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6301 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6302 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6303 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6304 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6305 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6306 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6307 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6308 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6309 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6310 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6311 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6312 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6313 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6314 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6315 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6316 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6317 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6318 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6319 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6320 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6321 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6322 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6323 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6324 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6325 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6326 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6327 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6328 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6329 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6330 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6331 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6332 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6333 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6334 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6335 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6336 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6337 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6338 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6339 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6340 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6341 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6342 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6343 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  6344 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  6345 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6346 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6347 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6348 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6349 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6350 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6351 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6352 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6353 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6354 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6355 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6356 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6357 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6358 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6359 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6360 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6361 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6362 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6363 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6364 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6365 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6366 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6367 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6368 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6369 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6370 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6371 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6372 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6373 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6374 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6375 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6376 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6377 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6378 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6379 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6380 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6381 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6382 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6383 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6384 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6385 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6386 | `	default:` |
|      ! 0 |  6387 | `		break;` |
|        - |  6388 | `	}` |
|       13 |  6389 | `	return zOp;` |
|        1 |  6390 |  |
|        - |  6391 | `/*` |
|        - |  6392 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6393 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6394 | ` * is responsible of consuming the generated dump.` |
|        - |  6395 | ` */` |
|        2 |  6396 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6397 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6398 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6399 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6400 | `	)` |
|        1 |  6401 |  |
|        - |  6402 | `	sxi32 rc;` |
|        3 |  6403 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6404 | `	return rc;` |
|        1 |  6405 |  |
|        - |  6406 | `/*` |
|        - |  6407 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6408 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6409 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6410 | ` * in 'compile.c' for additional information.` |
|        - |  6411 | ` */` |
|        8 |  6412 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6413 |  |
|        9 |  6414 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6415 | `	/* Evaluate and expand constant value */` |
|        9 |  6416 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6417 |  |
|        - |  6418 | `/*` |
|        - |  6419 | ` * Section:` |
|        - |  6420 | ` *  Function handling functions.` |
|        - |  6421 | ` * Status:` |
|        - |  6422 | ` *    Stable.` |
|        - |  6423 | ` */` |
|        - |  6424 | `/*` |
|        - |  6425 | ` * int func_num_args(void)` |
|        - |  6426 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6427 | ` * Parameters` |
|        - |  6428 | ` *   None.` |
|        - |  6429 | ` * Return` |
|        - |  6430 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6431 | ` *  or -1 if called from the globe scope.` |
|        - |  6432 | ` */` |
|      906 |  6433 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6434 |  |
|        - |  6435 | `	VmFrame *pFrame;` |
|        - |  6436 | `	ph7_vm *pVm;` |
|        - |  6437 | `	/* Point to the target VM */` |
|      908 |  6438 | `	pVm = pCtx->pVm;` |
|        - |  6439 | `	/* Current frame */` |
|      908 |  6440 | `	pFrame = pVm->pFrame;` |
|      908 |  6441 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6442 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6443 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6444 | `	}` |
|      908 |  6445 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6446 | `		SXUNUSED(nArg);` |
|      ! 0 |  6447 | `		SXUNUSED(apArg);` |
|        - |  6448 | `		/* Global frame,return -1 */` |
|      ! 0 |  6449 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6450 | `		return SXRET_OK;` |
|        - |  6451 | `	}` |
|        - |  6452 | `	/* Total number of arguments passed to the enclosing function */` |
|      908 |  6453 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      908 |  6454 | `	ph7_result_int(pCtx,nArg);` |
|      908 |  6455 | `	return SXRET_OK;` |
|      455 |  6456 |  |
|        - |  6457 | `/*` |
|        - |  6458 | ` * value func_get_arg(int $arg_num)` |
|        - |  6459 | ` *   Return an item from the argument list.` |
|        - |  6460 | ` * Parameters` |
|        - |  6461 | ` *  Argument number(index start from zero).` |
|        - |  6462 | ` * Return` |
|        - |  6463 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6464 | ` */` |
|       22 |  6465 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6466 |  |
|       24 |  6467 | `	ph7_value *pObj = 0;` |
|       24 |  6468 | `	VmSlot *pSlot = 0;` |
|        - |  6469 | `	VmFrame *pFrame;` |
|        - |  6470 | `	ph7_vm *pVm;` |
|        - |  6471 | `	/* Point to the target VM */` |
|       24 |  6472 | `	pVm = pCtx->pVm;` |
|        - |  6473 | `	/* Current frame */` |
|       24 |  6474 | `	pFrame = pVm->pFrame;` |
|       24 |  6475 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6476 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6477 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6478 | `	}` |
|       24 |  6479 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6480 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6481 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6482 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6483 | `		return SXRET_OK;` |
|        - |  6484 | `	}` |
|        - |  6485 | `	/* Extract the desired index */` |
|       21 |  6486 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  6487 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6488 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6489 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6490 | `		return SXRET_OK;` |
|        - |  6491 | `	}` |
|        - |  6492 | `	/* Extract the desired argument */` |
|       21 |  6493 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  6494 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6495 | `			/* Return the desired argument */` |
|       21 |  6496 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  6497 | `		}else{` |
|        - |  6498 | `			/* No such argument,return false */` |
|      ! 0 |  6499 | `			ph7_result_bool(pCtx,0);` |
|        - |  6500 | `		}` |
|       11 |  6501 | `	}else{` |
|        - |  6502 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6503 | `		ph7_result_bool(pCtx,0);` |
|        - |  6504 | `	}` |
|       21 |  6505 | `	return SXRET_OK;` |
|       13 |  6506 |  |
|        - |  6507 | `/*` |
|        - |  6508 | ` * array func_get_args_byref(void)` |
|        - |  6509 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6510 | ` * Parameters` |
|        - |  6511 | ` *  None.` |
|        - |  6512 | ` * Return` |
|        - |  6513 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6514 | ` *  member of the current user-defined function's argument list.` |
|        - |  6515 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6516 | ` * NOTE:` |
|        - |  6517 | ` *  Arguments are returned to the array by reference.` |
|        - |  6518 | ` */` |
|        2 |  6519 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6520 |  |
|        - |  6521 | `	ph7_value *pArray;` |
|        - |  6522 | `	VmFrame *pFrame;` |
|        - |  6523 | `	VmSlot *aSlot;` |
|        - |  6524 | `	sxu32 n;` |
|        - |  6525 | `	/* Point to the current frame */` |
|        3 |  6526 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6527 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6528 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6529 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6530 | `	}` |
|        3 |  6531 | `	if( pFrame->pParent == 0 ){` |
|        - |  6532 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6533 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6534 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6535 | `		return SXRET_OK;` |
|        - |  6536 | `	}` |
|        - |  6537 | `	/* Create a new array */` |
|        3 |  6538 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6539 | `	if( pArray == 0 ){` |
|      ! 0 |  6540 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6541 | `		SXUNUSED(apArg);` |
|      ! 0 |  6542 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6543 | `		return SXRET_OK;` |
|        - |  6544 | `	}` |
|        - |  6545 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6546 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6547 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6548 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6549 | `	}` |
|        - |  6550 | `	/* Return the freshly created array */` |
|        3 |  6551 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6552 | `	return SXRET_OK;` |
|        2 |  6553 |  |
|        - |  6554 | `/*` |
|        - |  6555 | ` * array func_get_args(void)` |
|        - |  6556 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6557 | ` * Parameters` |
|        - |  6558 | ` *  None.` |
|        - |  6559 | ` * Return` |
|        - |  6560 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6561 | ` *  member of the current user-defined function's argument list.` |
|        - |  6562 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6563 | ` */` |
|       62 |  6564 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6565 |  |
|       64 |  6566 | `	ph7_value *pObj = 0;` |
|        - |  6567 | `	ph7_value *pArray;` |
|        - |  6568 | `	VmFrame *pFrame;` |
|        - |  6569 | `	VmSlot *aSlot;` |
|        - |  6570 | `	sxu32 n;` |
|        - |  6571 | `	/* Point to the current frame */` |
|       64 |  6572 | `	pFrame = pCtx->pVm->pFrame;` |
|       64 |  6573 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6574 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6575 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6576 | `	}` |
|       64 |  6577 | `	if( pFrame->pParent == 0 ){` |
|        - |  6578 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6579 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6580 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6581 | `		return SXRET_OK;` |
|        - |  6582 | `	}` |
|        - |  6583 | `	/* Create a new array */` |
|       64 |  6584 | `	pArray = ph7_context_new_array(pCtx);` |
|       64 |  6585 | `	if( pArray == 0 ){` |
|      ! 0 |  6586 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6587 | `		SXUNUSED(apArg);` |
|      ! 0 |  6588 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6589 | `		return SXRET_OK;` |
|        - |  6590 | `	}` |
|        - |  6591 | `	/* Start filling the array with the given arguments */` |
|       64 |  6592 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      192 |  6593 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      130 |  6594 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      130 |  6595 | `		if( pObj ){` |
|      130 |  6596 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       64 |  6597 | `		}` |
|       66 |  6598 | `	}` |
|        - |  6599 | `	/* Return the freshly created array */` |
|       64 |  6600 | `	ph7_result_value(pCtx,pArray);` |
|       64 |  6601 | `	return SXRET_OK;` |
|       33 |  6602 |  |
|        - |  6603 | `/*` |
|        - |  6604 | ` * bool function_exists(string $name)` |
|        - |  6605 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6606 | ` * Parameters` |
|        - |  6607 | ` *  The name of the desired function.` |
|        - |  6608 | ` * Return` |
|        - |  6609 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6610 | ` */` |
|     1640 |  6611 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6612 |  |
|        - |  6613 | `	const char *zName;` |
|        - |  6614 | `	ph7_vm *pVm;` |
|        - |  6615 | `	int nLen;` |
|        - |  6616 | `	int res;` |
|     1642 |  6617 | `	if( nArg < 1 ){` |
|        - |  6618 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6619 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6620 | `		return SXRET_OK;` |
|        - |  6621 | `	}` |
|        - |  6622 | `	/* Point to the target VM */` |
|     1642 |  6623 | `	pVm = pCtx->pVm;` |
|        - |  6624 | `	/* Extract the function name */` |
|     1642 |  6625 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6626 | `	/* Assume the function is not defined */` |
|     1642 |  6627 | `	res = 0;` |
|        - |  6628 | `	/* Perform the lookup */` |
|     2460 |  6629 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1636 |  6630 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6631 | `			/* Function is defined */` |
|      206 |  6632 | `			res = 1;` |
|      102 |  6633 | `	}` |
|     1642 |  6634 | `	ph7_result_bool(pCtx,res);` |
|     1642 |  6635 | `	return SXRET_OK;` |
|      822 |  6636 |  |
|        - |  6637 | `/*` |
|        - |  6638 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6639 | ` * [i.e: Whether it is callable or not].` |
|        - |  6640 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6641 | ` */` |
|    16002 |  6642 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6643 |  |
|    16004 |  6644 | `	int res = 0;` |
|    16004 |  6645 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6646 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6647 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6648 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6649 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6650 | `		if( pMethod && CallInvoke ){` |
|        - |  6651 | `			ph7_value sResult;` |
|        - |  6652 | `			sxi32 rc;` |
|        - |  6653 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6654 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6655 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6656 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6657 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6658 | `			}` |
|      ! 0 |  6659 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6660 | `		}` |
|    16004 |  6661 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  6662 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  6663 | `		if( pMap->nEntry == 2 ){` |
|        - |  6664 | `			ph7_class *pClass;` |
|        - |  6665 | `			ph7_value *pV;` |
|        - |  6666 | `			/* Extract the target class */` |
|       12 |  6667 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  6668 | `			if( pV ){` |
|       12 |  6669 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  6670 | `				if( pClass ){` |
|        - |  6671 | `					ph7_class_method *pMethod;` |
|        - |  6672 | `					/* Extract the target method */` |
|       10 |  6673 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  6674 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6675 | `						/* Perform the lookup */` |
|       10 |  6676 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  6677 | `						if( pMethod ){` |
|        - |  6678 | `							/* Method is callable */` |
|        5 |  6679 | `							res = 1;` |
|        2 |  6680 | `						}` |
|        4 |  6681 | `					}` |
|        4 |  6682 | `				}` |
|        5 |  6683 | `			}` |
|        7 |  6684 | `		}` |
|    15991 |  6685 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6686 | `		const char *zName;` |
|        - |  6687 | `		int nLen;` |
|        - |  6688 | `		/* Extract the name */` |
|     4700 |  6689 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6690 | `		/* Perform the lookup */` |
|     4715 |  6691 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  6692 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6693 | `				/* Function is callable */` |
|     4682 |  6694 | `				res = 1;` |
|     2340 |  6695 | `		}` |
|     2349 |  6696 | `	}` |
|    16004 |  6697 | `	return res;` |
|        2 |  6698 |  |
|        - |  6699 | `/*` |
|        - |  6700 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6701 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6702 | ` * Parameters` |
|        - |  6703 | ` * $name` |
|        - |  6704 | ` *    The callback function to check` |
|        - |  6705 | ` * $syntax_only` |
|        - |  6706 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6707 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6708 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6709 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6710 | ` *    a string.` |
|        - |  6711 | ` * Return` |
|        - |  6712 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6713 | ` */` |
|       14 |  6714 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6715 |  |
|        - |  6716 | `	ph7_vm *pVm;` |
|        - |  6717 | `	int res;` |
|       15 |  6718 | `	if( nArg < 1 ){` |
|        - |  6719 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6720 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6721 | `		return SXRET_OK;` |
|        - |  6722 | `	}` |
|        - |  6723 | `	/* Point to the target VM */` |
|       15 |  6724 | `	pVm = pCtx->pVm;` |
|        - |  6725 | `	/* Perform the requested operation */` |
|       15 |  6726 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6727 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6728 | `	return SXRET_OK;` |
|        8 |  6729 |  |
|        - |  6730 | `/*` |
|        - |  6731 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6732 | ` * defined below.` |
|        - |  6733 | ` */` |
|     1082 |  6734 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6735 |  |
|     1083 |  6736 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6737 | `	ph7_value sName;` |
|        - |  6738 | `	sxi32 rc;` |
|        - |  6739 | `	/* Prepare the function name for insertion */` |
|     1083 |  6740 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1083 |  6741 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6742 | `	/* Perform the insertion */` |
|     1083 |  6743 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1083 |  6744 | `	PH7_MemObjRelease(&sName);` |
|     1083 |  6745 | `	return rc;` |
|        1 |  6746 |  |
|        - |  6747 | `/*` |
|        - |  6748 | ` * array get_defined_functions(void)` |
|        - |  6749 | ` *  Returns an array of all defined functions.` |
|        - |  6750 | ` * Parameter` |
|        - |  6751 | ` *  None.` |
|        - |  6752 | ` * Return` |
|        - |  6753 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6754 | ` *  both built-in (internal) and user-defined.` |
|        - |  6755 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6756 | ` *  defined ones using $arr["user"].` |
|        - |  6757 | ` * Note:` |
|        - |  6758 | ` *  NULL is returned on failure.` |
|        - |  6759 | ` */` |
|        2 |  6760 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6761 |  |
|        - |  6762 | `	ph7_value *pArray,*pEntry;` |
|        - |  6763 | `	/* NOTE:` |
|        - |  6764 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6765 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6766 | `	 */` |
|        3 |  6767 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6768 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6769 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6770 | `		SXUNUSED(apArg);` |
|        - |  6771 | `		/* Return NULL */` |
|      ! 0 |  6772 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6773 | `		return SXRET_OK;` |
|        - |  6774 | `	}` |
|        3 |  6775 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6776 | `	if( pEntry == 0 ){` |
|        - |  6777 | `		/* Return NULL */` |
|      ! 0 |  6778 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6779 | `		return SXRET_OK;` |
|        - |  6780 | `	}` |
|        - |  6781 | `	/* Fill with the appropriate information */` |
|        3 |  6782 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6783 | `	/* Create the 'internal' index */` |
|        3 |  6784 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6785 | `	/* Create the user-func array */` |
|        3 |  6786 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6787 | `	if( pEntry == 0 ){` |
|        - |  6788 | `		/* Return NULL */` |
|      ! 0 |  6789 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6790 | `		return SXRET_OK;` |
|        - |  6791 | `	}` |
|        - |  6792 | `	/* Fill with the appropriate information */` |
|        3 |  6793 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6794 | `	/* Create the 'user' index */` |
|        3 |  6795 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6796 | `	/* Return the multi-dimensional array */` |
|        3 |  6797 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6798 | `	return SXRET_OK;` |
|        2 |  6799 |  |
|        - |  6800 | `/*` |
|        - |  6801 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6802 | ` *  Register a function for execution on shutdown.` |
|        - |  6803 | ` * Note` |
|        - |  6804 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6805 | ` *  be called in the same order as they were registered.` |
|        - |  6806 | ` * Parameters` |
|        - |  6807 | ` *  $callback` |
|        - |  6808 | ` *   The shutdown callback to register.` |
|        - |  6809 | ` * $param` |
|        - |  6810 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6811 | ` * Return` |
|        - |  6812 | ` *  Nothing.` |
|        - |  6813 | ` */` |
|        2 |  6814 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6815 |  |
|        - |  6816 | `	VmShutdownCB sEntry;` |
|        - |  6817 | `	int i,j;` |
|        3 |  6818 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6819 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6820 | `		return PH7_OK;` |
|        - |  6821 | `	}` |
|        - |  6822 | `	/* Zero the Entry */` |
|        3 |  6823 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6824 | `	/* Initialize fields */` |
|        3 |  6825 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6826 | `	/* Save the callback name for later invocation name */` |
|        3 |  6827 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6828 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6829 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6830 | `	}` |
|        - |  6831 | `	/* Copy arguments */` |
|        3 |  6832 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6833 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6834 | `			/* Limit reached */` |
|      ! 0 |  6835 | `			break;` |
|        - |  6836 | `		}` |
|      ! 0 |  6837 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6838 | `	}` |
|        3 |  6839 | `	sEntry.nArg = j;` |
|        - |  6840 | `	/* Install the callback */` |
|        3 |  6841 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6842 | `	return PH7_OK;` |
|        2 |  6843 |  |
|        - |  6844 | `/*` |
|        - |  6845 | ` * Section:` |
|        - |  6846 | ` *  Class handling functions.` |
|        - |  6847 | ` * Status:` |
|        - |  6848 | ` *    Stable.` |
|        - |  6849 | ` */` |
|        - |  6850 | `/*` |
|        - |  6851 | ` * Extract the top active class. NULL is returned` |
|        - |  6852 | ` * if the class stack is empty.` |
|        - |  6853 | ` */` |
|      526 |  6854 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6855 |  |
|      528 |  6856 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6857 | `	ph7_class **apClass;` |
|      528 |  6858 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6859 | `		/* Empty stack,return NULL */` |
|       15 |  6860 | `		return 0;` |
|        - |  6861 | `	}` |
|        - |  6862 | `	/* Peek the last entry */` |
|      514 |  6863 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      514 |  6864 | `	return apClass[pSet->nUsed - 1];` |
|      265 |  6865 |  |
|        - |  6866 | `/*` |
|        - |  6867 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  6868 | ` *   Get the class that declared the currently executing method.` |
|        - |  6869 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  6870 | ` *` |
|        - |  6871 | ` * Parameters` |
|        - |  6872 | ` *   pVm: Target VM` |
|        - |  6873 | ` *` |
|        - |  6874 | ` * Return` |
|        - |  6875 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  6876 | ` *   - Not executing within a class method` |
|        - |  6877 | ` *` |
|        - |  6878 | ` * Note` |
|        - |  6879 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  6880 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  6881 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  6882 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  6883 | ` *   declaring class.` |
|        - |  6884 | ` */` |
|       48 |  6885 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  6886 |  |
|       50 |  6887 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  6888 | `	ph7_vm_func *pVmFunc;` |
|        - |  6889 |  |
|        - |  6890 | `	/* Skip exception frames to find the actual method frame */` |
|       50 |  6891 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  6892 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6893 | `	}` |
|        - |  6894 |  |
|        - |  6895 | `	/* Check if we're in a method context */` |
|       50 |  6896 | `	if( pFrame->pParent ){` |
|       46 |  6897 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       46 |  6898 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  6899 | `			/* Return the declaring class */` |
|       46 |  6900 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  6901 | `		}` |
|      ! 0 |  6902 | `	}` |
|        - |  6903 |  |
|        5 |  6904 | `	return 0;` |
|       26 |  6905 |  |
|        - |  6906 |  |
|        - |  6907 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  6908 | `/*` |
|        - |  6909 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  6910 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  6911 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  6912 | ` * return value indicates failure.` |
|        - |  6913 | ` */` |
|     1150 |  6914 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  6915 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  6916 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  6917 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  6918 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  6919 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  6920 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  6921 | `	)` |
|        2 |  6922 |  |
|        - |  6923 | `	ph7_value *aStack;` |
|        - |  6924 | `	VmInstr aInstr[2];` |
|        - |  6925 | `	int iCursor;` |
|        - |  6926 | `	int i;` |
|        - |  6927 | `	/* Create a new operand stack */` |
|     1152 |  6928 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1152 |  6929 | `	if( aStack == 0 ){` |
|      ! 0 |  6930 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6931 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  6932 | `		return SXERR_MEM;` |
|        - |  6933 | `	}` |
|        - |  6934 | `	/* Fill the operand stack with the given arguments */` |
|     1702 |  6935 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      552 |  6936 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  6937 | `		/*` |
|        - |  6938 | `		 * Symisc eXtension:` |
|        - |  6939 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  6940 | `		 */` |
|      552 |  6941 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      277 |  6942 | `	}` |
|     1152 |  6943 | `	iCursor = nArg + 1;` |
|     1152 |  6944 | `	if( pThis ){` |
|        - |  6945 | `		/*` |
|        - |  6946 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  6947 | `		 */` |
|     1146 |  6948 | `		pThis->iRef++; /* Increment reference count */` |
|     1146 |  6949 | `		aStack[i].x.pOther = pThis;` |
|     1146 |  6950 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      572 |  6951 | `	}` |
|     1152 |  6952 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1152 |  6953 | `	i++;` |
|        - |  6954 | `	/* Push method name */` |
|     1152 |  6955 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1152 |  6956 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1152 |  6957 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1152 |  6958 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  6959 | `	/* Emit the CALL istruction */` |
|     1152 |  6960 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1152 |  6961 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1152 |  6962 | `	aInstr[0].iP2 = 0;` |
|     1152 |  6963 | `	aInstr[0].p3  = 0;` |
|        - |  6964 | `	/* Emit the DONE instruction */` |
|     1152 |  6965 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1152 |  6966 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1152 |  6967 | `	aInstr[1].iP2 = 0;` |
|     1152 |  6968 | `	aInstr[1].p3  = 0;` |
|        - |  6969 | `	/* Execute the method body (if available) */` |
|     1152 |  6970 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  6971 | `	/* Clean up the mess left behind */` |
|     1152 |  6972 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1152 |  6973 | `	return PH7_OK;` |
|      577 |  6974 |  |
|        - |  6975 | `/*` |
|        - |  6976 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  6977 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  6978 | ` * in the apArg[] array.` |
|        - |  6979 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  6980 | ` * return value indicates failure.` |
|        - |  6981 | ` */` |
|      926 |  6982 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  6983 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  6984 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  6985 | `	int nArg,          /* Total number of given arguments */` |
|        - |  6986 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  6987 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  6988 | `	)` |
|        2 |  6989 |  |
|        - |  6990 | `	ph7_value *aStack;` |
|        - |  6991 | `	VmInstr aInstr[2];` |
|        - |  6992 | `	int i;` |
|      928 |  6993 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6994 | `		/* Don't bother processing,it's invalid anyway */` |
|      471 |  6995 | `		if( pResult ){` |
|        - |  6996 | `			/* Assume a null return value */` |
|      ! 0 |  6997 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  6998 | `		}` |
|      471 |  6999 | `		return SXERR_INVALID;` |
|        - |  7000 | `	}` |
|      458 |  7001 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7002 | `		/* Class method */` |
|       11 |  7003 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  7004 | `		ph7_class_method *pMethod = 0;` |
|       11 |  7005 | `		ph7_class_instance *pThis = 0;` |
|       11 |  7006 | `		ph7_class *pClass = 0;` |
|        - |  7007 | `		ph7_value *pValue;` |
|        - |  7008 | `		sxi32 rc;` |
|       11 |  7009 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  7010 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  7011 | `			if( pResult ){` |
|        - |  7012 | `				/* Assume a null return value */` |
|      ! 0 |  7013 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7014 | `			}` |
|      ! 0 |  7015 | `			return SXRET_OK;` |
|        - |  7016 | `		}` |
|        - |  7017 | `		/* Extract the class name or an instance of it */` |
|       11 |  7018 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  7019 | `		if( pValue ){` |
|       11 |  7020 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  7021 | `		}` |
|       11 |  7022 | `		if( pClass == 0 ){` |
|        - |  7023 | `			/* No such class,return NULL */` |
|      ! 0 |  7024 | `			if( pResult ){` |
|      ! 0 |  7025 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7026 | `			}` |
|      ! 0 |  7027 | `			return SXRET_OK;` |
|        - |  7028 | `		}` |
|       11 |  7029 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  7030 | `			/* Point to the class instance */` |
|        5 |  7031 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  7032 | `		}` |
|        - |  7033 | `		/* Try to extract the method */` |
|       11 |  7034 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  7035 | `		if( pValue ){` |
|       11 |  7036 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  7037 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  7038 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  7039 | `			}` |
|        5 |  7040 | `		}` |
|       11 |  7041 | `		if( pMethod == 0 ){` |
|        - |  7042 | `			/* No such method,return NULL */` |
|      ! 0 |  7043 | `			if( pResult ){` |
|      ! 0 |  7044 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7045 | `			}` |
|      ! 0 |  7046 | `			return SXRET_OK;` |
|        - |  7047 | `		}` |
|        - |  7048 | `		/* Call the class method */` |
|       11 |  7049 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  7050 | `		return rc;` |
|        - |  7051 | `	}` |
|        - |  7052 | `	/* Create a new operand stack */` |
|      448 |  7053 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      448 |  7054 | `	if( aStack == 0 ){` |
|      ! 0 |  7055 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7056 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  7057 | `		if( pResult ){` |
|        - |  7058 | `			/* Assume a null return value */` |
|      ! 0 |  7059 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7060 | `		}` |
|      ! 0 |  7061 | `		return SXERR_MEM;` |
|        - |  7062 | `	}` |
|        - |  7063 | `	/* Fill the operand stack with the given arguments */` |
|     1470 |  7064 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1024 |  7065 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7066 | `		/*` |
|        - |  7067 | `		 * Symisc eXtension:` |
|        - |  7068 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7069 | `		 */` |
|     1024 |  7070 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      513 |  7071 | `	}` |
|        - |  7072 | `	/* Push the function name */` |
|      448 |  7073 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      448 |  7074 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7075 | `	/* Emit the CALL istruction */` |
|      448 |  7076 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      448 |  7077 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      448 |  7078 | `	aInstr[0].iP2 = 0;` |
|      448 |  7079 | `	aInstr[0].p3  = 0;` |
|        - |  7080 | `	/* Emit the DONE instruction */` |
|      448 |  7081 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      448 |  7082 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      448 |  7083 | `	aInstr[1].iP2 = 0;` |
|      448 |  7084 | `	aInstr[1].p3  = 0;` |
|        - |  7085 | `	/* Execute the function body (if available) */` |
|      448 |  7086 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  7087 | `	/* Clean up the mess left behind */` |
|      448 |  7088 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      448 |  7089 | `	return PH7_OK;` |
|      465 |  7090 |  |
|        - |  7091 | `/*` |
|        - |  7092 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  7093 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  7094 | ` * parameter.` |
|        - |  7095 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7096 | ` * return value indicates failure.` |
|        - |  7097 | ` */` |
|      236 |  7098 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  7099 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7100 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7101 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  7102 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  7103 | `	)` |
|        1 |  7104 |  |
|        - |  7105 | `	ph7_value *pArg;` |
|        - |  7106 | `	SySet aArg;` |
|        - |  7107 | `	va_list ap;` |
|        - |  7108 | `	sxi32 rc;` |
|      237 |  7109 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7110 | `	/* Copy arguments one after one */` |
|      237 |  7111 | `	va_start(ap,pResult);` |
|      393 |  7112 | `	for(;;){` |
|      787 |  7113 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  7114 | `		if( pArg == 0 ){` |
|      237 |  7115 | `			break;` |
|        - |  7116 | `		}` |
|      551 |  7117 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  7118 | `	}` |
|        - |  7119 | `	/* Call the core routine */` |
|      237 |  7120 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  7121 | `	/* Cleanup */` |
|      237 |  7122 | `	SySetRelease(&aArg);` |
|      237 |  7123 | `	return rc;` |
|        1 |  7124 |  |
|        - |  7125 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  7126 | `/*` |
|        - |  7127 | ` * bool defined(string $name)` |
|        - |  7128 | ` *  Checks whether a given named constant exists.` |
|        - |  7129 | ` * Parameter:` |
|        - |  7130 | ` *  Name of the desired constant.` |
|        - |  7131 | ` * Return` |
|        - |  7132 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  7133 | ` */` |
|       14 |  7134 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7135 |  |
|        - |  7136 | `	const char *zName;` |
|       16 |  7137 | `	int nLen = 0;` |
|       16 |  7138 | `	int res = 0;` |
|       16 |  7139 | `	if( nArg < 1 ){` |
|        - |  7140 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  7141 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  7142 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7143 | `		return SXRET_OK;` |
|        - |  7144 | `	}` |
|        - |  7145 | `	/* Extract constant name */` |
|       16 |  7146 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7147 | `	/* Perform the lookup */` |
|       16 |  7148 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7149 | `		/* Already defined */` |
|       10 |  7150 | `		res = 1;` |
|        4 |  7151 | `	}` |
|       16 |  7152 | `	ph7_result_bool(pCtx,res);` |
|       16 |  7153 | `	return SXRET_OK;` |
|        9 |  7154 |  |
|        - |  7155 | `/*` |
|        - |  7156 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  7157 | ` * below.` |
|        - |  7158 | ` */` |
|        8 |  7159 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  7160 |  |
|       10 |  7161 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7162 | `	/* Expand constant value */` |
|       10 |  7163 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7164 |  |
|        - |  7165 | `/*` |
|        - |  7166 | ` * bool define(string $constant_name,expression value)` |
|        - |  7167 | ` *  Defines a named constant at runtime.` |
|        - |  7168 | ` * Parameter:` |
|        - |  7169 | ` *  $constant_name` |
|        - |  7170 | ` *   The name of the constant` |
|        - |  7171 | ` *  $value` |
|        - |  7172 | ` *   Constant value` |
|        - |  7173 | ` * Return:` |
|        - |  7174 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7175 | ` */` |
|       10 |  7176 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7177 |  |
|        - |  7178 | `	const char *zName;  /* Constant name */` |
|        - |  7179 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7180 | `	int nLen = 0;       /* Name length */` |
|        - |  7181 | `	sxi32 rc;` |
|       12 |  7182 | `	if( nArg < 2 ){` |
|        - |  7183 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7184 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7185 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7186 | `		return SXRET_OK;` |
|        - |  7187 | `	}` |
|       12 |  7188 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7189 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7190 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7191 | `		return SXRET_OK;` |
|        - |  7192 | `	}` |
|        - |  7193 | `	/* Extract constant name */` |
|       12 |  7194 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7195 | `	if( nLen < 1 ){` |
|      ! 0 |  7196 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7197 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7198 | `		return SXRET_OK;` |
|        - |  7199 | `	}` |
|        - |  7200 | `	/* Duplicate constant value */` |
|       12 |  7201 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7202 | `	if( pValue == 0 ){` |
|      ! 0 |  7203 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7204 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7205 | `		return SXRET_OK;` |
|        - |  7206 | `	}` |
|        - |  7207 | `	/* Initialize the memory object */` |
|       12 |  7208 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7209 | `	/* Register the constant */` |
|       12 |  7210 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7211 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7212 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7213 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7214 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7215 | `		return SXRET_OK;` |
|        - |  7216 | `	}` |
|        - |  7217 | `	/* Duplicate constant value */` |
|       12 |  7218 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7219 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7220 | `		/* Lower case the constant name */` |
|      ! 0 |  7221 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7222 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7223 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7224 | `				/* UTF-8 stream */` |
|      ! 0 |  7225 | `				zCur++;` |
|      ! 0 |  7226 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  7227 | `					zCur++;` |
|      ! 0 |  7228 | `				}` |
|      ! 0 |  7229 | `				continue;` |
|        - |  7230 | `			}` |
|      ! 0 |  7231 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  7232 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  7233 | `				zCur[0] = (char)c;` |
|      ! 0 |  7234 | `			}` |
|      ! 0 |  7235 | `			zCur++;` |
|      ! 0 |  7236 | `		}` |
|        - |  7237 | `		/* Finally,register the constant */` |
|      ! 0 |  7238 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  7239 | `	}` |
|        - |  7240 | `	/* All done,return TRUE */` |
|       12 |  7241 | `	ph7_result_bool(pCtx,1);` |
|       12 |  7242 | `	return SXRET_OK;` |
|        7 |  7243 |  |
|        - |  7244 | `/*` |
|        - |  7245 | ` * value constant(string $name)` |
|        - |  7246 | ` *  Returns the value of a constant` |
|        - |  7247 | ` * Parameter` |
|        - |  7248 | ` *  $name` |
|        - |  7249 | ` *    Name of the constant.` |
|        - |  7250 | ` * Return` |
|        - |  7251 | ` *  Constant value or NULL if not defined.` |
|        - |  7252 | ` */` |
|        8 |  7253 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7254 |  |
|        - |  7255 | `	SyHashEntry *pEntry;` |
|        - |  7256 | `	ph7_constant *pCons;` |
|        - |  7257 | `	const char *zName; /* Constant name */` |
|        - |  7258 | `	ph7_value sVal;    /* Constant value */` |
|        - |  7259 | `	int nLen;` |
|       10 |  7260 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  7261 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  7262 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  7263 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7264 | `		return SXRET_OK;` |
|        - |  7265 | `	}` |
|        - |  7266 | `	/* Extract the constant name */` |
|       10 |  7267 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7268 | `	/* Perform the query */` |
|       10 |  7269 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  7270 | `	if( pEntry == 0 ){` |
|        3 |  7271 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  7272 | `		ph7_result_null(pCtx);` |
|        3 |  7273 | `		return SXRET_OK;` |
|        - |  7274 | `	}` |
|        8 |  7275 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  7276 | `	/* Point to the structure that describe the constant */` |
|        8 |  7277 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  7278 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  7279 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  7280 | `	/* Return that value */` |
|        8 |  7281 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  7282 | `	/* Cleanup */` |
|        8 |  7283 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  7284 | `	return SXRET_OK;` |
|        6 |  7285 |  |
|        - |  7286 | `/*` |
|        - |  7287 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  7288 | ` * defined below.` |
|        - |  7289 | ` */` |
|      416 |  7290 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7291 |  |
|      417 |  7292 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  7293 | `	ph7_value sName;` |
|        - |  7294 | `	sxi32 rc;` |
|        - |  7295 | `	/* Prepare the constant name for insertion */` |
|      417 |  7296 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      417 |  7297 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  7298 | `	/* Perform the insertion */` |
|      417 |  7299 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      417 |  7300 | `	PH7_MemObjRelease(&sName);` |
|      417 |  7301 | `	return rc;` |
|        1 |  7302 |  |
|        - |  7303 | `/*` |
|        - |  7304 | ` * array get_defined_constants(void)` |
|        - |  7305 | ` *  Returns an associative array with the names of all defined` |
|        - |  7306 | ` *  constants.` |
|        - |  7307 | ` * Parameters` |
|        - |  7308 | ` *  NONE.` |
|        - |  7309 | ` * Returns` |
|        - |  7310 | ` *  Returns the names of all the constants currently defined.` |
|        - |  7311 | ` */` |
|        2 |  7312 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7313 |  |
|        - |  7314 | `	ph7_value *pArray;` |
|        - |  7315 | `	/* Create the array first*/` |
|        3 |  7316 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7317 | `	if( pArray == 0 ){` |
|      ! 0 |  7318 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7319 | `		SXUNUSED(apArg);` |
|        - |  7320 | `		/* Return NULL */` |
|      ! 0 |  7321 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7322 | `		return SXRET_OK;` |
|        - |  7323 | `	}` |
|        - |  7324 | `	/* Fill the array with the defined constants */` |
|        3 |  7325 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  7326 | `	/* Return the created array */` |
|        3 |  7327 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7328 | `	return SXRET_OK;` |
|        2 |  7329 |  |
|        - |  7330 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  7331 | `/*` |
|        - |  7332 | ` * Section:` |
|        - |  7333 | ` *  Random numbers/string generators.` |
|        - |  7334 | ` * Status:` |
|        - |  7335 | ` *    Stable.` |
|        - |  7336 | ` */` |
|        - |  7337 | `/*` |
|        - |  7338 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  7339 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  7340 | ` * used by te SQLite3 library.` |
|        - |  7341 | ` */` |
|     2250 |  7342 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  7343 |  |
|        - |  7344 | `	sxu32 iNum;` |
|     2252 |  7345 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2252 |  7346 | `	return iNum;` |
|        2 |  7347 |  |
|        - |  7348 | `/*` |
|        - |  7349 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  7350 | ` * Note that the generated string is NOT null terminated.` |
|        - |  7351 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  7352 | ` * by te SQLite3 library.` |
|        - |  7353 | ` */` |
|    70420 |  7354 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  7355 |  |
|        - |  7356 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  7357 | `	int i;` |
|        - |  7358 | `	/* Generate a binary string first */` |
|    70422 |  7359 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  7360 | `	/* Turn the binary string into english based alphabet */` |
|   774790 |  7361 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   704370 |  7362 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   352186 |  7363 | `	 }` |
|    70422 |  7364 |  |
|        - |  7365 | `/*` |
|        - |  7366 | ` * int rand()` |
|        - |  7367 | ` * int mt_rand()` |
|        - |  7368 | ` * int rand(int $min,int $max)` |
|        - |  7369 | ` * int mt_rand(int $min,int $max)` |
|        - |  7370 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  7371 | ` * Parameter` |
|        - |  7372 | ` *  $min` |
|        - |  7373 | ` *    The lowest value to return (default: 0)` |
|        - |  7374 | ` *  $max` |
|        - |  7375 | ` *   The highest value to return (default: getrandmax())` |
|        - |  7376 | ` * Return` |
|        - |  7377 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  7378 | ` * Note:` |
|        - |  7379 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7380 | ` *  by te SQLite3 library.` |
|        - |  7381 | ` */` |
|       20 |  7382 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7383 |  |
|        - |  7384 | `	sxu32 iNum;` |
|        - |  7385 | `	/* Generate the random number */` |
|       21 |  7386 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  7387 | `	if( nArg > 1 ){` |
|        - |  7388 | `		sxu32 iMin,iMax;` |
|        3 |  7389 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  7390 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  7391 | `		if( iMin < iMax ){` |
|        3 |  7392 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  7393 | `			if( iDiv > 0 ){` |
|        3 |  7394 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  7395 | `			}` |
|        1 |  7396 | `		}else if(iMax > 0 ){` |
|      ! 0 |  7397 | `			iNum %= iMax;` |
|      ! 0 |  7398 | `		}` |
|        1 |  7399 | `	}` |
|        - |  7400 | `	/* Return the number */` |
|       21 |  7401 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  7402 | `	return SXRET_OK;` |
|        1 |  7403 |  |
|        - |  7404 | `/*` |
|        - |  7405 | ` * int getrandmax(void)` |
|        - |  7406 | ` * int mt_getrandmax(void)` |
|        - |  7407 | ` * int rc4_getrandmax(void)` |
|        - |  7408 | ` *   Show largest possible random value` |
|        - |  7409 | ` * Return` |
|        - |  7410 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  7411 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  7412 | ` * Note:` |
|        - |  7413 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7414 | ` *  by te SQLite3 library.` |
|        - |  7415 | ` */` |
|        4 |  7416 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7417 |  |
|        2 |  7418 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  7419 | `	SXUNUSED(apArg);` |
|        5 |  7420 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  7421 | `	return SXRET_OK;` |
|        1 |  7422 |  |
|        - |  7423 | `/*` |
|        - |  7424 | ` * string rand_str()` |
|        - |  7425 | ` * string rand_str(int $len)` |
|        - |  7426 | ` *  Generate a random string (English alphabet).` |
|        - |  7427 | ` * Parameter` |
|        - |  7428 | ` *  $len` |
|        - |  7429 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  7430 | ` * Return` |
|        - |  7431 | ` *   A pseudo random string.` |
|        - |  7432 | ` * Note:` |
|        - |  7433 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7434 | ` *  by te SQLite3 library.` |
|        - |  7435 | ` *  This function is a symisc extension.` |
|        - |  7436 | ` */` |
|      120 |  7437 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7438 |  |
|        - |  7439 | `	char zString[1024];` |
|      122 |  7440 | `	int iLen = 0x10;` |
|      122 |  7441 | `	if( nArg > 0 ){` |
|        - |  7442 | `		/* Get the desired length */` |
|      122 |  7443 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  7444 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  7445 | `			/* Default length */` |
|        3 |  7446 | `			iLen = 0x10;` |
|        1 |  7447 | `		}` |
|       60 |  7448 | `	}` |
|        - |  7449 | `	/* Generate the random string */` |
|      122 |  7450 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  7451 | `	/* Return the generated string */` |
|      122 |  7452 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  7453 | `	return SXRET_OK;` |
|        2 |  7454 |  |
|        - |  7455 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  7456 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  7457 | `/* Unique ID private data */` |
|        - |  7458 | `struct unique_id_data` |
|        - |  7459 |  |
|        - |  7460 | `	ph7_context *pCtx; /* Call context */` |
|        - |  7461 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  7462 | `};` |
|        - |  7463 | `/*` |
|        - |  7464 | ` * Binary to hex consumer callback.` |
|        - |  7465 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  7466 | ` * defined below.` |
|        - |  7467 | ` */` |
|      192 |  7468 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  7469 |  |
|      193 |  7470 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  7471 | `	sxu32 nBuflen;` |
|        - |  7472 | `	/* Extract result buffer length */` |
|      193 |  7473 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  7474 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  7475 | `			/*` |
|        - |  7476 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  7477 | `			 * string will be 13 characters long` |
|        - |  7478 | `			 */` |
|       25 |  7479 | `		return SXERR_ABORT;` |
|        - |  7480 | `	}` |
|      169 |  7481 | `	if( nBuflen > 22 ){` |
|      ! 0 |  7482 | `		return SXERR_ABORT;` |
|        - |  7483 | `	}` |
|        - |  7484 | `	/* Safely Consume the hex stream */` |
|      169 |  7485 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  7486 | `	return SXRET_OK;` |
|       97 |  7487 |  |
|        - |  7488 | `/*` |
|        - |  7489 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  7490 | ` *  Generate a unique ID` |
|        - |  7491 | ` * Parameter` |
|        - |  7492 | ` * $prefix` |
|        - |  7493 | ` *  Append this prefix to the generated unique ID.` |
|        - |  7494 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  7495 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  7496 | ` * $more_entropy` |
|        - |  7497 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  7498 | ` *  that the result will be unique.` |
|        - |  7499 | ` * Return` |
|        - |  7500 | ` *  Returns the unique identifier, as a string.` |
|        - |  7501 | ` */` |
|       24 |  7502 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7503 |  |
|        - |  7504 | `	struct unique_id_data sUniq;` |
|        - |  7505 | `	unsigned char zDigest[20];` |
|       25 |  7506 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7507 | `	const char *zPrefix;` |
|        - |  7508 | `	SHA1Context sCtx;` |
|        - |  7509 | `	char zRandom[7];` |
|        - |  7510 | `	int nPrefix;` |
|        - |  7511 | `	int entropy;` |
|        - |  7512 | `	/* Generate a random string first */` |
|       25 |  7513 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  7514 | `	/* Initialize fields */` |
|       25 |  7515 | `	zPrefix = 0;` |
|       25 |  7516 | `	nPrefix = 0;` |
|       25 |  7517 | `	entropy = 0;` |
|       25 |  7518 | `	if( nArg > 0 ){` |
|        - |  7519 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  7520 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  7521 | `		if( nArg > 1 ){` |
|      ! 0 |  7522 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  7523 | `		}` |
|      ! 0 |  7524 | `	}` |
|       25 |  7525 | `	SHA1Init(&sCtx);` |
|        - |  7526 | `	/* Generate the random ID */` |
|       25 |  7527 | `	if( nPrefix > 0 ){` |
|      ! 0 |  7528 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  7529 | `	}` |
|        - |  7530 | `	/* Append the random ID */` |
|       25 |  7531 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  7532 | `	/* Append the random string */` |
|       25 |  7533 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  7534 | `	/* Increment the number */` |
|       25 |  7535 | `	pVm->unique_id++;` |
|       25 |  7536 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  7537 | `	/* Hexify the digest */` |
|       25 |  7538 | `	sUniq.pCtx = pCtx;` |
|       25 |  7539 | `	sUniq.entropy = entropy;` |
|       25 |  7540 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  7541 | `	/* All done */` |
|       25 |  7542 | `	return PH7_OK;` |
|        1 |  7543 |  |
|        - |  7544 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  7545 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  7546 | `/*` |
|        - |  7547 | ` * Section:` |
|        - |  7548 | ` *  Language construct implementation as foreign functions.` |
|        - |  7549 | ` * Status:` |
|        - |  7550 | ` *    Stable.` |
|        - |  7551 | ` */` |
|        - |  7552 | `/*` |
|        - |  7553 | ` * void echo($string...)` |
|        - |  7554 | ` *  Output one or more messages.` |
|        - |  7555 | ` * Parameters` |
|        - |  7556 | ` *  $string` |
|        - |  7557 | ` *   Message to output.` |
|        - |  7558 | ` * Return` |
|        - |  7559 | ` *  NULL.` |
|        - |  7560 | ` */` |
|      ! 0 |  7561 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7562 |  |
|        - |  7563 | `	const char *zData;` |
|      ! 0 |  7564 | `	int nDataLen = 0;` |
|        - |  7565 | `	ph7_vm *pVm;` |
|        - |  7566 | `	int i,rc;` |
|        - |  7567 | `	/* Point to the target VM */` |
|      ! 0 |  7568 | `	pVm = pCtx->pVm;` |
|        - |  7569 | `	/* Output */` |
|      ! 0 |  7570 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  7571 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  7572 | `		if( nDataLen > 0 ){` |
|      ! 0 |  7573 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  7574 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7575 | `				/* Increment output length */` |
|      ! 0 |  7576 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  7577 | `			}` |
|      ! 0 |  7578 | `			if( rc == SXERR_ABORT ){` |
|        - |  7579 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7580 | `				return PH7_ABORT;` |
|        - |  7581 | `			}` |
|      ! 0 |  7582 | `		}` |
|      ! 0 |  7583 | `	}` |
|      ! 0 |  7584 | `	return SXRET_OK;` |
|      ! 0 |  7585 |  |
|        - |  7586 | `/*` |
|        - |  7587 | ` * int print($string...)` |
|        - |  7588 | ` *  Output one or more messages.` |
|        - |  7589 | ` * Parameters` |
|        - |  7590 | ` *  $string` |
|        - |  7591 | ` *   Message to output.` |
|        - |  7592 | ` * Return` |
|        - |  7593 | ` *  1 always.` |
|        - |  7594 | ` */` |
|        2 |  7595 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7596 |  |
|        - |  7597 | `	const char *zData;` |
|        3 |  7598 | `	int nDataLen = 0;` |
|        - |  7599 | `	ph7_vm *pVm;` |
|        - |  7600 | `	int i,rc;` |
|        - |  7601 | `	/* Point to the target VM */` |
|        3 |  7602 | `	pVm = pCtx->pVm;` |
|        - |  7603 | `	/* Output */` |
|        5 |  7604 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  7605 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  7606 | `		if( nDataLen > 0 ){` |
|        3 |  7607 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  7608 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7609 | `				/* Increment output length */` |
|        3 |  7610 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  7611 | `			}` |
|        3 |  7612 | `			if( rc == SXERR_ABORT ){` |
|        - |  7613 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7614 | `				return PH7_ABORT;` |
|        - |  7615 | `			}` |
|        1 |  7616 | `		}` |
|        2 |  7617 | `	}` |
|        - |  7618 | `	/* Return 1 */` |
|        3 |  7619 | `	ph7_result_int(pCtx,1);` |
|        3 |  7620 | `	return SXRET_OK;` |
|        2 |  7621 |  |
|        - |  7622 | `/*` |
|        - |  7623 | ` * void exit(string $msg)` |
|        - |  7624 | ` * void exit(int $status)` |
|        - |  7625 | ` * void die(string $ms)` |
|        - |  7626 | ` * void die(int $status)` |
|        - |  7627 | ` *   Output a message and terminate program execution.` |
|        - |  7628 | ` * Parameter` |
|        - |  7629 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  7630 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  7631 | ` *  and not printed` |
|        - |  7632 | ` * Return` |
|        - |  7633 | ` *  NULL` |
|        - |  7634 | ` */` |
|      ! 0 |  7635 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7636 |  |
|      ! 0 |  7637 | `	if( nArg > 0 ){` |
|      ! 0 |  7638 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  7639 | `			const char *zData;` |
|      ! 0 |  7640 | `			int iLen = 0;` |
|        - |  7641 | `			/* Print exit message */` |
|      ! 0 |  7642 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  7643 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  7644 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  7645 | `			sxi32 iExitStatus;` |
|        - |  7646 | `			/* Record exit status code */` |
|      ! 0 |  7647 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  7648 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  7649 | `		}` |
|      ! 0 |  7650 | `	}` |
|        - |  7651 | `	/* Check if we are in an included file */` |
|      ! 0 |  7652 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  7653 | `		/* Exit the entire process */` |
|      ! 0 |  7654 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  7655 | `	}` |
|        - |  7656 | `	/* Abort processing immediately */` |
|      ! 0 |  7657 | `	return PH7_ABORT;` |
|      ! 0 |  7658 |  |
|        - |  7659 | `/*` |
|        - |  7660 | ` * bool isset($var,...)` |
|        - |  7661 | ` *  Finds out whether a variable is set.` |
|        - |  7662 | ` * Parameters` |
|        - |  7663 | ` *  One or more variable to check.` |
|        - |  7664 | ` * Return` |
|        - |  7665 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  7666 | ` */` |
|    70234 |  7667 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7668 |  |
|        - |  7669 | `	ph7_value *pObj;` |
|    70236 |  7670 | `	int res = 0;` |
|        - |  7671 | `	int i;` |
|    70236 |  7672 | `	if( nArg < 1 ){` |
|        - |  7673 | `		/* Missing arguments,return false */` |
|      ! 0 |  7674 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  7675 | `		return SXRET_OK;` |
|        - |  7676 | `	}` |
|        - |  7677 | `	/* Iterate over available arguments */` |
|    92800 |  7678 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    70236 |  7679 | `		pObj = apArg[i];` |
|    70236 |  7680 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    47176 |  7681 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7682 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  7683 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  7684 | `			}` |
|    23587 |  7685 | `		}` |
|    70236 |  7686 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    70236 |  7687 | `		if( !res ){` |
|        - |  7688 | `			/* Variable not set,return FALSE */` |
|    47672 |  7689 | `			ph7_result_bool(pCtx,0);` |
|    47672 |  7690 | `			return SXRET_OK;` |
|        - |  7691 | `		}` |
|    11284 |  7692 | `	}` |
|        - |  7693 | `	/* All given variable are set,return TRUE */` |
|    22566 |  7694 | `	ph7_result_bool(pCtx,1);` |
|    22566 |  7695 | `	return SXRET_OK;` |
|    35119 |  7696 |  |
|        - |  7697 | `/*` |
|        - |  7698 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  7699 | ` * frame,the reference table and discard it's contents.` |
|        - |  7700 | ` * This function never fail and always return SXRET_OK.` |
|        - |  7701 | ` */` |
|  2958542 |  7702 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  7703 |  |
|        - |  7704 | `	ph7_value *pObj;` |
|        - |  7705 | `	VmRefObj *pRef;` |
|  2958544 |  7706 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2958544 |  7707 | `	if( pObj ){` |
|        - |  7708 | `		/* Release the object */` |
|  2958544 |  7709 | `		PH7_MemObjRelease(pObj);` |
|  1479271 |  7710 | `	}` |
|        - |  7711 | `	/* Remove old reference links */` |
|  2958544 |  7712 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2958544 |  7713 | `	if( pRef ){` |
|  2958524 |  7714 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  7715 | `		/* Unlink from the reference table */` |
|  2958524 |  7716 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2958524 |  7717 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  7718 | `			VmSlot sFree;` |
|        - |  7719 | `			/* Restore to the free list */` |
|  2958518 |  7720 | `			sFree.nIdx = nObjIdx;` |
|  2958518 |  7721 | `			sFree.pUserData = 0;` |
|  2958518 |  7722 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1479258 |  7723 | `		}` |
|  1479261 |  7724 | `	}` |
|  2958544 |  7725 | `	return SXRET_OK;` |
|        2 |  7726 |  |
|        - |  7727 | `/*` |
|        - |  7728 | ` * void unset($var,...)` |
|        - |  7729 | ` *   Unset one or more given variable.` |
|        - |  7730 | ` * Parameters` |
|        - |  7731 | ` *  One or more variable to unset.` |
|        - |  7732 | ` * Return` |
|        - |  7733 | ` *  Nothing.` |
|        - |  7734 | ` */` |
|     3258 |  7735 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7736 |  |
|        - |  7737 | `	ph7_value *pObj;` |
|        - |  7738 | `	ph7_vm *pVm;` |
|        - |  7739 | `	int i;` |
|        - |  7740 | `	/* Point to the target VM */` |
|     3260 |  7741 | `	pVm = pCtx->pVm;` |
|        - |  7742 | `	/* Iterate and unset */` |
|     9662 |  7743 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6404 |  7744 | `		pObj = apArg[i];` |
|     6404 |  7745 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      868 |  7746 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7747 | `				/* Throw an error */` |
|      ! 0 |  7748 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  7749 | `			}` |
|      435 |  7750 | `		}else{` |
|     5537 |  7751 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  7752 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     5537 |  7753 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     5531 |  7754 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2765 |  7755 | `			}` |
|        - |  7756 | `		}` |
|     3203 |  7757 | `	}` |
|     3260 |  7758 | `	return SXRET_OK;` |
|        2 |  7759 |  |
|        - |  7760 | `/*` |
|        - |  7761 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  7762 | ` */` |
|      110 |  7763 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7764 |  |
|      111 |  7765 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  7766 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  7767 | `	ph7_value *pObj;` |
|        - |  7768 | `	sxu32 nIdx;` |
|        - |  7769 | `	/* Extract the memory object */` |
|      111 |  7770 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  7771 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  7772 | `	if( pObj ){` |
|      111 |  7773 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  7774 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  7775 | `				SyString sName;` |
|        - |  7776 | `				ph7_value sKey;` |
|        - |  7777 | `				/* Perform the insertion */` |
|      109 |  7778 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  7779 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  7780 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  7781 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  7782 | `			}` |
|       54 |  7783 | `		}` |
|       55 |  7784 | `	}` |
|      111 |  7785 | `	return SXRET_OK;` |
|        1 |  7786 |  |
|        - |  7787 | `/*` |
|        - |  7788 | ` * array get_defined_vars(void)` |
|        - |  7789 | ` *  Returns an array of all defined variables.` |
|        - |  7790 | ` * Parameter` |
|        - |  7791 | ` *  None` |
|        - |  7792 | ` * Return` |
|        - |  7793 | ` *  An array with all the variables defined in the current scope.` |
|        - |  7794 | ` */` |
|        2 |  7795 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7796 |  |
|        3 |  7797 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7798 | `	ph7_value *pArray;` |
|        - |  7799 | `	/* Create a new array */` |
|        3 |  7800 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7801 | ` 	if( pArray == 0 ){` |
|      ! 0 |  7802 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7803 | `		SXUNUSED(apArg);` |
|        - |  7804 | `		/* Return NULL */` |
|      ! 0 |  7805 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7806 | `		return SXRET_OK;` |
|        - |  7807 | `	}` |
|        - |  7808 | `	/* Superglobals first */` |
|        3 |  7809 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  7810 | `	/* Then variable defined in the current frame */` |
|        3 |  7811 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  7812 | `	/* Finally,return the created array */` |
|        3 |  7813 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7814 | `	return SXRET_OK;` |
|        2 |  7815 |  |
|        - |  7816 | `/*` |
|        - |  7817 | ` * bool gettype($var)` |
|        - |  7818 | ` *  Get the type of a variable` |
|        - |  7819 | ` * Parameters` |
|        - |  7820 | ` *   $var` |
|        - |  7821 | ` *    The variable being type checked.` |
|        - |  7822 | ` * Return` |
|        - |  7823 | ` *   String representation of the given variable type.` |
|        - |  7824 | ` */` |
|       32 |  7825 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7826 |  |
|       34 |  7827 | `	const char *zType = "Empty";` |
|       34 |  7828 | `	if( nArg > 0 ){` |
|       34 |  7829 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  7830 | `	}` |
|        - |  7831 | `	/* Return the variable type */` |
|       34 |  7832 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  7833 | `	return SXRET_OK;` |
|        2 |  7834 |  |
|        - |  7835 | `/*` |
|        - |  7836 | ` * string get_resource_type(resource $handle)` |
|        - |  7837 | ` *  This function gets the type of the given resource.` |
|        - |  7838 | ` * Parameters` |
|        - |  7839 | ` *  $handle` |
|        - |  7840 | ` *  The evaluated resource handle.` |
|        - |  7841 | ` * Return` |
|        - |  7842 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  7843 | ` *  representing its type. If the type is not identified by this function` |
|        - |  7844 | ` *  the return value will be the string Unknown.` |
|        - |  7845 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  7846 | ` *  is not a resource.` |
|        - |  7847 | ` */` |
|        2 |  7848 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7849 |  |
|        3 |  7850 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  7851 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  7852 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7853 | `		return PH7_OK;` |
|        - |  7854 | `	}` |
|        3 |  7855 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  7856 | `	return SXRET_OK;` |
|        2 |  7857 |  |
|        - |  7858 | `/*` |
|        - |  7859 | ` * void var_dump(expression,....)` |
|        - |  7860 | ` *   var_dump � Dumps information about a variable` |
|        - |  7861 | ` * Parameters` |
|        - |  7862 | ` *   One or more expression to dump.` |
|        - |  7863 | ` * Returns` |
|        - |  7864 | ` *  Nothing.` |
|        - |  7865 | ` */` |
|      218 |  7866 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7867 |  |
|        - |  7868 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  7869 | `	int i;` |
|      220 |  7870 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  7871 | `	/* Dump one or more expressions */` |
|      444 |  7872 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  7873 | `		ph7_value *pObj = apArg[i];` |
|        - |  7874 | `		/* Reset the working buffer */` |
|      226 |  7875 | `		SyBlobReset(&sDump);` |
|        - |  7876 | `		/* Dump the given expression */` |
|      226 |  7877 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  7878 | `		/* Output */` |
|      226 |  7879 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  7880 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  7881 | `		}` |
|      114 |  7882 | `	}` |
|        - |  7883 | `	/* Release the working buffer */` |
|      220 |  7884 | `	SyBlobRelease(&sDump);` |
|      220 |  7885 | `	return SXRET_OK;` |
|        2 |  7886 |  |
|        - |  7887 | `/*` |
|        - |  7888 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  7889 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  7890 | ` * Parameters` |
|        - |  7891 | ` *   expression: Expression to dump` |
|        - |  7892 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  7893 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  7894 | ` *            print_r() will return the information rather than print it.` |
|        - |  7895 | ` * Return` |
|        - |  7896 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  7897 | ` *  Otherwise, the return value is TRUE.` |
|        - |  7898 | ` */` |
|       16 |  7899 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7900 |  |
|       17 |  7901 | `	int ret_string = 0;` |
|        - |  7902 | `	SyBlob sDump;` |
|       17 |  7903 | `	if( nArg < 1 ){` |
|        - |  7904 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  7905 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7906 | `		return SXRET_OK;` |
|        - |  7907 | `	}` |
|       17 |  7908 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  7909 | `	if ( nArg > 1 ){` |
|        - |  7910 | `		/* Where to redirect output */` |
|       11 |  7911 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  7912 | `	}` |
|        - |  7913 | `	/* Generate dump */` |
|       17 |  7914 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  7915 | `	if( !ret_string ){` |
|        - |  7916 | `		/* Output dump */` |
|        7 |  7917 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7918 | `		/* Return true */` |
|        7 |  7919 | `		ph7_result_bool(pCtx,1);` |
|        4 |  7920 | `	}else{` |
|        - |  7921 | `		/* Generated dump as return value */` |
|       11 |  7922 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7923 | `	}` |
|        - |  7924 | `	/* Release the working buffer */` |
|       17 |  7925 | `	SyBlobRelease(&sDump);` |
|       17 |  7926 | `	return SXRET_OK;` |
|        9 |  7927 |  |
|        - |  7928 | `/*` |
|        - |  7929 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  7930 | ` * Same job as print_r. (see coment above)` |
|        - |  7931 | ` */` |
|        2 |  7932 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7933 |  |
|        3 |  7934 | `	int ret_string = 0;` |
|        - |  7935 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  7936 | `	if( nArg < 1 ){` |
|        - |  7937 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  7938 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7939 | `		return SXRET_OK;` |
|        - |  7940 | `	}` |
|        3 |  7941 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  7942 | `	if ( nArg > 1 ){` |
|        - |  7943 | `		/* Where to redirect output */` |
|        3 |  7944 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  7945 | `	}` |
|        - |  7946 | `	/* Generate dump */` |
|        3 |  7947 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  7948 | `	if( !ret_string ){` |
|        - |  7949 | `		/* Output dump */` |
|      ! 0 |  7950 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7951 | `		/* Return NULL */` |
|      ! 0 |  7952 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7953 | `	}else{` |
|        - |  7954 | `		/* Generated dump as return value */` |
|        3 |  7955 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7956 | `	}` |
|        - |  7957 | `	/* Release the working buffer */` |
|        3 |  7958 | `	SyBlobRelease(&sDump);` |
|        3 |  7959 | `	return SXRET_OK;` |
|        2 |  7960 |  |
|        - |  7961 | `/*` |
|        - |  7962 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  7963 | ` *  Set/get the various assert flags.` |
|        - |  7964 | ` * Parameter` |
|        - |  7965 | ` * $what` |
|        - |  7966 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  7967 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  7968 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  7969 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  7970 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  7971 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  7972 | ` * $value` |
|        - |  7973 | ` *   An optional new value for the option.` |
|        - |  7974 | ` * Return` |
|        - |  7975 | ` *  Old setting on success or FALSE on failure.` |
|        - |  7976 | ` */` |
|       30 |  7977 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7978 |  |
|       32 |  7979 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7980 | `	int iOption;` |
|        - |  7981 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       32 |  7982 | `	if( nArg < 1 ){` |
|        3 |  7983 | `		return PH7_VmThrowException(pCtx,` |
|        - |  7984 | `			"ArgumentCountError",` |
|        - |  7985 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  7986 | `			);` |
|        - |  7987 | `	}` |
|        - |  7988 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       28 |  7989 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       30 |  7990 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  7991 | `		return PH7_VmThrowException(pCtx,` |
|        - |  7992 | `			"TypeError",` |
|        - |  7993 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  7994 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  7995 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  7996 | `			);` |
|        - |  7997 | `	}` |
|       30 |  7998 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  7999 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  8000 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  8001 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       30 |  8002 | `	switch( iOption ){` |
|        6 |  8003 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  8004 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       14 |  8005 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       14 |  8006 | `		if( nArg > 1 ){` |
|        5 |  8007 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  8008 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  8009 | `			}else{` |
|        3 |  8010 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  8011 | `			}` |
|        2 |  8012 | `		}` |
|       14 |  8013 | `		break;` |
|        1 |  8014 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  8015 | `		/* Return old callback or null */` |
|        3 |  8016 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  8017 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  8018 | `		}else{` |
|        3 |  8019 | `			ph7_result_null(pCtx);` |
|        - |  8020 | `		}` |
|        3 |  8021 | `		if( nArg > 1 ){` |
|      ! 0 |  8022 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  8023 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  8024 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  8025 | `			}else{` |
|      ! 0 |  8026 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  8027 | `			}` |
|      ! 0 |  8028 | `		}` |
|        3 |  8029 | `		break;` |
|        5 |  8030 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  8031 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  8032 | `		if( nArg > 1 ){` |
|        5 |  8033 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  8034 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  8035 | `			}else{` |
|        3 |  8036 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  8037 | `			}` |
|        2 |  8038 | `		}` |
|       11 |  8039 | `		break;` |
|      ! 0 |  8040 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  8041 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  8042 | `		break;` |
|        1 |  8043 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  8044 | `		ph7_result_int(pCtx, 1);` |
|        3 |  8045 | `		break;` |
|      ! 0 |  8046 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  8047 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  8048 | `		break;` |
|        1 |  8049 | `	default:` |
|        - |  8050 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  8051 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8052 | `			"ValueError",` |
|        - |  8053 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  8054 | `			);` |
|        - |  8055 | `	}` |
|       28 |  8056 | `	return PH7_OK;` |
|       17 |  8057 |  |
|        - |  8058 | `/*` |
|        - |  8059 | ` * bool assert(mixed $assertion)` |
|        - |  8060 | ` *  Checks if assertion is FALSE.` |
|        - |  8061 | ` * Parameter` |
|        - |  8062 | ` *  $assertion` |
|        - |  8063 | ` *    The assertion to test.` |
|        - |  8064 | ` * Return` |
|        - |  8065 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  8066 | ` */` |
|       26 |  8067 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8068 |  |
|       28 |  8069 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8070 | `	int iFlags,iResult;` |
|        - |  8071 | `	const char *zDesc;` |
|        - |  8072 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       28 |  8073 | `	if( nArg < 1 ){` |
|        3 |  8074 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8075 | `			"ArgumentCountError",` |
|        - |  8076 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  8077 | `			);` |
|        - |  8078 | `	}` |
|       26 |  8079 | `	iFlags = pVm->iAssertFlags;` |
|       26 |  8080 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  8081 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  8082 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  8083 | `		return PH7_OK;` |
|        - |  8084 | `	}` |
|        - |  8085 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       26 |  8086 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       26 |  8087 | `	if( !iResult ){` |
|        - |  8088 | `		/* Assertion failed */` |
|        - |  8089 | `		/* Extract optional description */` |
|       13 |  8090 | `		zDesc = 0;` |
|       13 |  8091 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  8092 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  8093 | `		}` |
|       13 |  8094 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  8095 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  8096 | `			ph7_value sFile,sLine;` |
|        - |  8097 | `			ph7_value *apCbArg[3];` |
|        - |  8098 | `			SyString *pFile;` |
|        - |  8099 | `			/* Extract the processed script */` |
|      ! 0 |  8100 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  8101 | `			if( pFile == 0 ){` |
|      ! 0 |  8102 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  8103 | `			}` |
|        - |  8104 | `			/* Invoke the callback */` |
|      ! 0 |  8105 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  8106 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  8107 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  8108 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  8109 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  8110 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  8111 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  8112 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  8113 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  8114 | `		}` |
|       13 |  8115 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  8116 | `			/* Abort VM execution immediately */` |
|      ! 0 |  8117 | `			return PH7_ABORT;` |
|        - |  8118 | `		}` |
|        - |  8119 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  8120 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  8121 | `			return PH7_VmThrowException(pCtx,` |
|        - |  8122 | `				"AssertionError",` |
|        - |  8123 | `				"%s",` |
|        1 |  8124 | `				zDesc` |
|        - |  8125 | `				);` |
|      ! 0 |  8126 | `		}else{` |
|       11 |  8127 | `			return PH7_VmThrowException(pCtx,` |
|        - |  8128 | `				"AssertionError",` |
|        - |  8129 | `				"assert(false)"` |
|        - |  8130 | `				);` |
|        - |  8131 | `		}` |
|        - |  8132 | `	}` |
|        - |  8133 | `	/* Assertion passed */` |
|       14 |  8134 | `	ph7_result_bool(pCtx,1);` |
|       14 |  8135 | `	return PH7_OK;` |
|       15 |  8136 |  |
|        - |  8137 | `/*` |
|        - |  8138 | ` * Section:` |
|        - |  8139 | ` *  Error reporting functions.` |
|        - |  8140 | ` * Status:` |
|        - |  8141 | ` *    Stable.` |
|        - |  8142 | ` */` |
|        - |  8143 | `/*` |
|        - |  8144 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  8145 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  8146 | ` * Parameters` |
|        - |  8147 | ` *  $error_msg` |
|        - |  8148 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  8149 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  8150 | ` * $error_type` |
|        - |  8151 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  8152 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  8153 | ` * Return` |
|        - |  8154 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  8155 | ` */` |
|       12 |  8156 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8157 |  |
|       14 |  8158 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  8159 | `	int rc = PH7_OK;` |
|       14 |  8160 | `	if( nArg > 0 ){` |
|        - |  8161 | `		const char *zErr;` |
|        - |  8162 | `		int nLen;` |
|        - |  8163 | `		/* Extract the error message */` |
|       12 |  8164 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8165 | `		if( nArg > 1 ){` |
|        - |  8166 | `			/* Extract the error type */` |
|       12 |  8167 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  8168 | `			switch( nErr ){` |
|        1 |  8169 | `			case 1:   /* E_ERROR */` |
|        - |  8170 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  8171 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  8172 | `			case 256: /* E_USER_ERROR */` |
|        3 |  8173 | `				nErr = PH7_CTX_ERR;` |
|        3 |  8174 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  8175 | `				break;` |
|        1 |  8176 | `			case 2:   /* E_WARNING */` |
|        - |  8177 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  8178 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  8179 | `			case 512: /* E_USER_WARNING */` |
|        3 |  8180 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  8181 | `				break;` |
|        3 |  8182 | `			default:` |
|        8 |  8183 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  8184 | `				break;` |
|        - |  8185 | `			}` |
|        5 |  8186 | `		}` |
|        - |  8187 | `		/* Report error */` |
|       12 |  8188 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  8189 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  8190 | `			return rc;` |
|        - |  8191 | `		}` |
|        - |  8192 | `		/* Return true */` |
|       12 |  8193 | `		ph7_result_bool(pCtx,1);` |
|        7 |  8194 | `	}else{` |
|        - |  8195 | `		/* Missing arguments,return FALSE */` |
|        3 |  8196 | `		ph7_result_bool(pCtx,0);` |
|        - |  8197 | `	}` |
|       14 |  8198 | `	return rc;` |
|        8 |  8199 |  |
|        - |  8200 | `/*` |
|        - |  8201 | ` * int error_reporting([int $level])` |
|        - |  8202 | ` *  Sets which PHP errors are reported.` |
|        - |  8203 | ` * Parameters` |
|        - |  8204 | ` *  $level` |
|        - |  8205 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  8206 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  8207 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  8208 | ` *   levels will not always behave as expected.` |
|        - |  8209 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  8210 | ` *   in the predefined constants.` |
|        - |  8211 | ` * Return` |
|        - |  8212 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  8213 | ` *   parameter is given.` |
|        - |  8214 | ` */` |
|       40 |  8215 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8216 |  |
|       42 |  8217 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8218 | `	int nOld;` |
|        - |  8219 | `	/* Extract the old reporting level */` |
|       42 |  8220 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       42 |  8221 | `	if( nArg > 0 ){` |
|        - |  8222 | `		int nNew;` |
|        - |  8223 | `		/* Extract the desired error reporting level */` |
|       34 |  8224 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       34 |  8225 | `		if( !nNew ){` |
|        - |  8226 | `			/* Do not report errors at all */` |
|        5 |  8227 | `			pVm->bErrReport = 0;` |
|        3 |  8228 | `		}else{` |
|        - |  8229 | `			/* Report all errors */` |
|       30 |  8230 | `			pVm->bErrReport = 1;` |
|        - |  8231 | `		}` |
|       16 |  8232 | `	}` |
|        - |  8233 | `	/* Return the old level */` |
|       42 |  8234 | `	ph7_result_int(pCtx,nOld);` |
|       42 |  8235 | `	return PH7_OK;` |
|        2 |  8236 |  |
|        - |  8237 | `/*` |
|        - |  8238 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  8239 | ` *  Send an error message somewhere.` |
|        - |  8240 | ` * Parameter` |
|        - |  8241 | ` *  $message` |
|        - |  8242 | ` *   The error message that should be logged.` |
|        - |  8243 | ` *  $message_type` |
|        - |  8244 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  8245 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  8246 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  8247 | ` *       This is the default option.` |
|        - |  8248 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  8249 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  8250 | ` *    2  No longer an option.` |
|        - |  8251 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  8252 | ` *       to the end of the message string.` |
|        - |  8253 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  8254 | ` *  $destination` |
|        - |  8255 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  8256 | ` *  $extra_headers` |
|        - |  8257 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  8258 | ` * Return` |
|        - |  8259 | ` *  TRUE on success or FALSE on failure.` |
|        - |  8260 | ` * NOTE:` |
|        - |  8261 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  8262 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  8263 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  8264 | ` *  Otherwise this function is no-op.` |
|        - |  8265 | ` */` |
|        4 |  8266 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8267 |  |
|        - |  8268 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  8269 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  8270 | `	int iType = 0;` |
|        5 |  8271 | `	if( nArg < 1 ){` |
|        - |  8272 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  8273 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8274 | `		return PH7_OK;` |
|        - |  8275 | `	}` |
|        5 |  8276 | `	if( pVm->xErrLog  ){` |
|        - |  8277 | `		/* Invoke the user callback */` |
|      ! 0 |  8278 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  8279 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  8280 | `		if( nArg > 1 ){` |
|      ! 0 |  8281 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  8282 | `			if( nArg > 2 ){` |
|      ! 0 |  8283 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  8284 | `				if( nArg > 3 ){` |
|      ! 0 |  8285 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  8286 | `				}` |
|      ! 0 |  8287 | `			}` |
|      ! 0 |  8288 | `		}` |
|      ! 0 |  8289 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  8290 | `	}` |
|        - |  8291 | `	/* Retun TRUE */` |
|        5 |  8292 | `	ph7_result_bool(pCtx,1);` |
|        5 |  8293 | `	return PH7_OK;` |
|        3 |  8294 |  |
|        - |  8295 | `/*` |
|        - |  8296 | ` * bool restore_exception_handler(void)` |
|        - |  8297 | ` *  Restores the previously defined exception handler function.` |
|        - |  8298 | ` * Parameter` |
|        - |  8299 | ` *  None` |
|        - |  8300 | ` * Return` |
|        - |  8301 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  8302 | ` */` |
|        4 |  8303 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8304 |  |
|        5 |  8305 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8306 | `	ph7_value *pOld,*pNew;` |
|        - |  8307 | `	/* Point to the old and the new handler */` |
|        5 |  8308 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  8309 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  8310 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8311 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8312 | `		SXUNUSED(apArg);` |
|        - |  8313 | `		/* No installed handler,return FALSE */` |
|        5 |  8314 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8315 | `		return PH7_OK;` |
|        - |  8316 | `	}` |
|        - |  8317 | `	/* Copy the old handler */` |
|      ! 0 |  8318 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8319 | `	PH7_MemObjRelease(pOld);` |
|        - |  8320 | `	/* Return TRUE */` |
|      ! 0 |  8321 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8322 | `	return PH7_OK;` |
|        3 |  8323 |  |
|        - |  8324 | `/*` |
|        - |  8325 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  8326 | ` *  Sets a user-defined exception handler function.` |
|        - |  8327 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  8328 | ` * NOTE` |
|        - |  8329 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  8330 | ` *  the satndard PHP engine.` |
|        - |  8331 | ` * Parameters` |
|        - |  8332 | ` *  $exception_handler` |
|        - |  8333 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  8334 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  8335 | ` *   that was thrown.` |
|        - |  8336 | ` *  Note:` |
|        - |  8337 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8338 | ` * Return` |
|        - |  8339 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  8340 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8341 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8342 | ` */` |
|        4 |  8343 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8344 |  |
|        6 |  8345 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8346 | `	ph7_value *pOld,*pNew;` |
|        - |  8347 | `	/* Point to the old and the new handler */` |
|        6 |  8348 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  8349 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  8350 | `	/* Return the old handler */` |
|        6 |  8351 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  8352 | `	if( nArg > 0 ){` |
|        6 |  8353 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8354 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  8355 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  8356 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  8357 | `		}else{` |
|        6 |  8358 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8359 | `			/* Install the new handler */` |
|        6 |  8360 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8361 | `		}` |
|        2 |  8362 | `	}` |
|        6 |  8363 | `	return PH7_OK;` |
|        2 |  8364 |  |
|        - |  8365 | `/*` |
|        - |  8366 | ` * bool restore_error_handler(void)` |
|        - |  8367 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8368 | ` * Parameters:` |
|        - |  8369 | ` *  None.` |
|        - |  8370 | ` * Return` |
|        - |  8371 | ` *  Always TRUE.` |
|        - |  8372 | ` */` |
|        4 |  8373 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8374 |  |
|        5 |  8375 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8376 | `	ph7_value *pOld,*pNew;` |
|        - |  8377 | `	/* Point to the old and the new handler */` |
|        5 |  8378 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  8379 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  8380 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8381 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8382 | `		SXUNUSED(apArg);` |
|        - |  8383 | `		/* No installed callback,return FALSE */` |
|        5 |  8384 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8385 | `		return PH7_OK;` |
|        - |  8386 | `	}` |
|        - |  8387 | `	/* Copy the old callback */` |
|      ! 0 |  8388 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8389 | `	PH7_MemObjRelease(pOld);` |
|        - |  8390 | `	/* Return TRUE */` |
|      ! 0 |  8391 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8392 | `	return PH7_OK;` |
|        3 |  8393 |  |
|        - |  8394 | `/*` |
|        - |  8395 | ` * value set_error_handler(callable $error_handler)` |
|        - |  8396 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8397 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8398 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8399 | ` *  Sets a user-defined error handler function.` |
|        - |  8400 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  8401 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  8402 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  8403 | ` *  conditions (using trigger_error()).` |
|        - |  8404 | ` * Parameters` |
|        - |  8405 | ` *  $error_handler` |
|        - |  8406 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  8407 | ` *   describing the error.` |
|        - |  8408 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  8409 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  8410 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  8411 | ` *   The function can be shown as:` |
|        - |  8412 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  8413 | ` *     errno` |
|        - |  8414 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  8415 | ` *   errstr` |
|        - |  8416 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  8417 | ` *   errfile` |
|        - |  8418 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  8419 | ` *     was raised in, as a string.` |
|        - |  8420 | ` *  Note:` |
|        - |  8421 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8422 | ` * Return` |
|        - |  8423 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  8424 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8425 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8426 | ` */` |
|     8722 |  8427 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8428 |  |
|     8724 |  8429 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8430 | `	ph7_value *pOld,*pNew;` |
|        - |  8431 | `	/* Point to the old and the new handler */` |
|     8724 |  8432 | `	pOld = &pVm->aErrCB[0];` |
|     8724 |  8433 | `	pNew = &pVm->aErrCB[1];` |
|        - |  8434 | `	/* Return the old handler */` |
|     8724 |  8435 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8724 |  8436 | `	if( nArg > 0 ){` |
|     8724 |  8437 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8438 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4361 |  8439 | `			PH7_MemObjRelease(pNew);` |
|     4361 |  8440 | `			ph7_result_bool(pCtx,1);` |
|     2181 |  8441 | `		}else{` |
|     4364 |  8442 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8443 | `			/* Install the new handler */` |
|     4364 |  8444 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8445 | `		}` |
|     4361 |  8446 | `	}` |
|     8724 |  8447 | `	return PH7_OK;` |
|        2 |  8448 |  |
|        - |  8449 | `/*` |
|        - |  8450 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  8451 | ` *  Generates a backtrace.` |
|        - |  8452 | ` * Paramaeter` |
|        - |  8453 | ` *  $options` |
|        - |  8454 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  8455 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  8456 | ` *   all the function/method arguments, to save memory.` |
|        - |  8457 | ` * $limit` |
|        - |  8458 | ` *   (Not Used)` |
|        - |  8459 | ` * Return` |
|        - |  8460 | ` *  An array.The possible returned elements are as follows:` |
|        - |  8461 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  8462 | ` *          Name        Type      Description` |
|        - |  8463 | ` *          ------      ------     -----------` |
|        - |  8464 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  8465 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  8466 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  8467 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  8468 | ` *          object      object    The current object.` |
|        - |  8469 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  8470 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  8471 | ` */` |
|      492 |  8472 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8473 |  |
|      494 |  8474 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8475 | `	ph7_value *pArray;` |
|        - |  8476 | `	ph7_class *pClass;` |
|        - |  8477 | `	ph7_value *pValue;` |
|        - |  8478 | `	SyString *pFile;` |
|        - |  8479 | `	/* Create a new array */` |
|      494 |  8480 | `	pArray = ph7_context_new_array(pCtx);` |
|      494 |  8481 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      494 |  8482 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8483 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8484 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  8485 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8486 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8487 | `		SXUNUSED(apArg);` |
|      ! 0 |  8488 | `		return PH7_OK;` |
|        - |  8489 | `	}` |
|        - |  8490 | `	/* Dump running function name and it's arguments  */` |
|      494 |  8491 | `	if( pVm->pFrame->pParent ){` |
|      494 |  8492 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8493 | `		ph7_vm_func *pFunc;` |
|        - |  8494 | `		ph7_value *pArg;` |
|      494 |  8495 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8496 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  8497 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  8498 | `		}` |
|      494 |  8499 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      494 |  8500 | `		if( pFrame->pParent && pFunc ){` |
|      494 |  8501 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      494 |  8502 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      494 |  8503 | `			ph7_value_reset_string_cursor(pValue);` |
|      246 |  8504 | `		}` |
|        - |  8505 | `		/* Function arguments */` |
|      494 |  8506 | `		pArg = ph7_context_new_array(pCtx);` |
|      494 |  8507 | `		if( pArg  ){` |
|        - |  8508 | `			ph7_value *pObj;` |
|        - |  8509 | `			VmSlot *aSlot;` |
|        - |  8510 | `			sxu32 n;` |
|        - |  8511 | `			/* Start filling the array with the given arguments */` |
|      494 |  8512 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     1962 |  8513 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1470 |  8514 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1470 |  8515 | `				if( pObj ){` |
|     1470 |  8516 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      734 |  8517 | `				}` |
|      736 |  8518 | `			}` |
|        - |  8519 | `			/* Save the array */` |
|      494 |  8520 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      246 |  8521 | `		}` |
|      246 |  8522 | `	}` |
|      494 |  8523 | `	ph7_value_int(pValue,1);` |
|        - |  8524 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  8525 | `	 * line numbers at run-time. )` |
|        - |  8526 | `	 */` |
|      494 |  8527 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  8528 | `	/* Current processed script */` |
|      494 |  8529 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      494 |  8530 | `	if( pFile ){` |
|      494 |  8531 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      494 |  8532 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      494 |  8533 | `		ph7_value_reset_string_cursor(pValue);` |
|      246 |  8534 | `	}` |
|        - |  8535 | `	/* Top class */` |
|      494 |  8536 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      494 |  8537 | `	if( pClass ){` |
|      490 |  8538 | `		ph7_value_reset_string_cursor(pValue);` |
|      490 |  8539 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      490 |  8540 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      244 |  8541 | `	}` |
|        - |  8542 | `	/* Return the freshly created array */` |
|      494 |  8543 | `	ph7_result_value(pCtx,pArray);` |
|        - |  8544 | `	/*` |
|        - |  8545 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  8546 | `	 * as soon we return from this function.` |
|        - |  8547 | `	 */` |
|      494 |  8548 | `	return PH7_OK;` |
|      248 |  8549 |  |
|        - |  8550 | `/*` |
|        - |  8551 | ` * Generate a small backtrace.` |
|        - |  8552 | ` * Store the generated dump in the given BLOB` |
|        - |  8553 | ` */` |
|        4 |  8554 | `static int VmMiniBacktrace(` |
|        - |  8555 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8556 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  8557 | `	)` |
|        1 |  8558 |  |
|        5 |  8559 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8560 | `	ph7_vm_func *pFunc;` |
|        - |  8561 | `	ph7_class *pClass;` |
|        - |  8562 | `	SyString *pFile;` |
|        - |  8563 | `	/* Called function */` |
|        5 |  8564 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8565 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  8566 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  8567 | `	}` |
|        5 |  8568 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  8569 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8570 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  8571 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  8572 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  8573 | `	}else{` |
|      ! 0 |  8574 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  8575 | `	}` |
|        5 |  8576 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  8577 | `	/* Current processed script */` |
|        5 |  8578 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  8579 | `	if( pFile ){` |
|        5 |  8580 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8581 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  8582 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  8583 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  8584 | `	}` |
|        - |  8585 | `	/* Top class */` |
|        5 |  8586 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  8587 | `	if( pClass ){` |
|      ! 0 |  8588 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  8589 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  8590 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  8591 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  8592 | `	}` |
|        5 |  8593 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  8594 | `	/* All done */` |
|        5 |  8595 | `	return SXRET_OK;` |
|        1 |  8596 |  |
|        - |  8597 | `/*` |
|        - |  8598 | ` * void debug_print_backtrace()` |
|        - |  8599 | ` *  Prints a backtrace` |
|        - |  8600 | ` * Parameters` |
|        - |  8601 | ` * None` |
|        - |  8602 | ` * Return` |
|        - |  8603 | ` * NULL` |
|        - |  8604 | ` */` |
|        2 |  8605 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8606 |  |
|        3 |  8607 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8608 | `	SyBlob sDump;` |
|        3 |  8609 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8610 | `	/* Generate the backtrace */` |
|        3 |  8611 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8612 | `	/* Output backtrace */` |
|        3 |  8613 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8614 | `	/* All done,cleanup */` |
|        3 |  8615 | `	SyBlobRelease(&sDump);` |
|        1 |  8616 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8617 | `	SXUNUSED(apArg);` |
|        3 |  8618 | `	return PH7_OK;` |
|        1 |  8619 |  |
|        - |  8620 | `/*` |
|        - |  8621 | ` * string debug_string_backtrace()` |
|        - |  8622 | ` *  Generate a backtrace` |
|        - |  8623 | ` * Parameters` |
|        - |  8624 | ` * None` |
|        - |  8625 | ` * Return` |
|        - |  8626 | ` *  A mini backtrace().` |
|        - |  8627 | ` * Note that this is a symisc extension.` |
|        - |  8628 | ` */` |
|        2 |  8629 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8630 |  |
|        3 |  8631 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8632 | `	SyBlob sDump;` |
|        3 |  8633 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8634 | `	/* Generate the backtrace */` |
|        3 |  8635 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8636 | `	/* Return the backtrace */` |
|        3 |  8637 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  8638 | `	/* All done,cleanup */` |
|        3 |  8639 | `	SyBlobRelease(&sDump);` |
|        1 |  8640 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8641 | `	SXUNUSED(apArg);` |
|        3 |  8642 | `	return PH7_OK;` |
|        1 |  8643 |  |
|        - |  8644 | `/*` |
|        - |  8645 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  8646 | ` * exception is triggered.` |
|        - |  8647 | ` */` |
|      472 |  8648 | `static sxi32 VmUncaughtException(` |
|        - |  8649 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8650 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8651 | `	)` |
|        1 |  8652 |  |
|        - |  8653 | `	ph7_value *apArg[2],sArg;` |
|      473 |  8654 | `	int nArg = 1;` |
|        - |  8655 | `	sxi32 rc;` |
|      473 |  8656 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  8657 | `		/* Nesting limit reached */` |
|      ! 0 |  8658 | `		return SXRET_OK;` |
|        - |  8659 | `	}` |
|        - |  8660 | `	/* Call any exception handler if available */` |
|      473 |  8661 | `	PH7_MemObjInit(pVm,&sArg);` |
|      473 |  8662 | `	if( pThis ){` |
|        - |  8663 | `		/* Load the exception instance */` |
|      473 |  8664 | `		sArg.x.pOther = pThis;` |
|      473 |  8665 | `		pThis->iRef++;` |
|      473 |  8666 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      237 |  8667 | `	}else{` |
|      ! 0 |  8668 | `		nArg = 0;` |
|        - |  8669 | `	}` |
|      473 |  8670 | `	apArg[0] = &sArg;` |
|        - |  8671 | `	/* Call the exception handler if available */` |
|      473 |  8672 | `	pVm->nExceptDepth++;` |
|      473 |  8673 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      473 |  8674 | `	pVm->nExceptDepth--;` |
|      473 |  8675 | `	if( rc != SXRET_OK ){` |
|        - |  8676 | `		SyBlob sMsgBuf;` |
|      471 |  8677 | `		const char *zClass = "Exception";` |
|      471 |  8678 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  8679 | `		const char *zMsg;` |
|        - |  8680 | `		sxu32 nMsg;` |
|        - |  8681 | `		const char *zFuncName;` |
|        - |  8682 | `		int nFuncLen;` |
|      471 |  8683 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      471 |  8684 | `		if( pThis ){` |
|        - |  8685 | `			ph7_class_method *pGetMessage;` |
|        - |  8686 | `			ph7_value sMsg;` |
|        - |  8687 | `			const char *zTmp;` |
|        - |  8688 | `			int nTmp;` |
|      471 |  8689 | `			zClass = pThis->pClass->sName.zString;` |
|      471 |  8690 | `			nClass = pThis->pClass->sName.nByte;` |
|      471 |  8691 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      471 |  8692 | `			if( pGetMessage ){` |
|      471 |  8693 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      471 |  8694 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      471 |  8695 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      471 |  8696 | `					if( zTmp && nTmp > 0 ){` |
|      471 |  8697 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      235 |  8698 | `					}` |
|      235 |  8699 | `				}` |
|      471 |  8700 | `				PH7_MemObjRelease(&sMsg);` |
|      235 |  8701 | `			}` |
|      235 |  8702 | `		}` |
|      471 |  8703 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  8704 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  8705 | `		}` |
|      471 |  8706 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      471 |  8707 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      471 |  8708 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      471 |  8709 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      471 |  8710 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  8711 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      471 |  8712 | `		rc = SXERR_ABORT;` |
|      235 |  8713 | `	}` |
|      473 |  8714 | `	PH7_MemObjRelease(&sArg);` |
|      473 |  8715 | `	return rc;` |
|      237 |  8716 |  |
|        - |  8717 | `/*` |
|        - |  8718 | ` * Throw an user exception.` |
|        - |  8719 | ` */` |
|      490 |  8720 | `static sxi32 VmThrowException(` |
|        - |  8721 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  8722 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8723 | `	)` |
|        2 |  8724 |  |
|        - |  8725 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  8726 | `	ph7_exception **apException;` |
|        - |  8727 | `	ph7_exception *pException;` |
|        - |  8728 | `	/* Point to the stack of loaded exceptions */` |
|      492 |  8729 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      492 |  8730 | `	pException = 0;` |
|      492 |  8731 | `	pCatch = 0;` |
|      492 |  8732 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8733 | `		ph7_exception_block *aCatch;` |
|        - |  8734 | `		ph7_class *pClass;` |
|        - |  8735 | `		sxu32 j;` |
|        - |  8736 | `		/* Locate the appropriate block to execute */` |
|       20 |  8737 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       20 |  8738 | `		(void)SySetPop(&pVm->aException);` |
|       20 |  8739 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       20 |  8740 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       20 |  8741 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  8742 | `			/* Extract the target class */` |
|       20 |  8743 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       20 |  8744 | `			if( pClass == 0 ){` |
|        - |  8745 | `				/* No such class */` |
|      ! 0 |  8746 | `				continue;` |
|        - |  8747 | `			}` |
|       20 |  8748 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  8749 | `				/* Catch block found,break immeditaley */` |
|       20 |  8750 | `				pCatch = &aCatch[j];` |
|       20 |  8751 | `				break;` |
|        - |  8752 | `			}` |
|      ! 0 |  8753 | `		}` |
|        9 |  8754 | `	}` |
|        - |  8755 | `	/* Execute the cached block if available */` |
|      492 |  8756 | `	if( pCatch == 0 ){` |
|        - |  8757 | `		sxi32 rc;` |
|      473 |  8758 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      473 |  8759 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  8760 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  8761 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8762 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  8763 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  8764 | `			}` |
|      ! 0 |  8765 | `			if( pException->pFrame == pFrame ){` |
|        - |  8766 | `				/* Tell the upper layer that the exception was caught */` |
|      ! 0 |  8767 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  8768 | `			}` |
|      ! 0 |  8769 | `		}` |
|      473 |  8770 | `		return rc;` |
|      ! 0 |  8771 | `	}else{` |
|       20 |  8772 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8773 | `		sxi32 rc;` |
|       30 |  8774 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8775 | `			/* Safely ignore the exception frame */` |
|       12 |  8776 | `			pFrame = pFrame->pParent;` |
|        2 |  8777 | `		}` |
|       20 |  8778 | `		if( pException->pFrame == pFrame ){` |
|        - |  8779 | `			/* Tell the upper layer that the exception was caught */` |
|       12 |  8780 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        5 |  8781 | `		}` |
|        - |  8782 | `		/* Create a private frame first */` |
|       20 |  8783 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       20 |  8784 | `		if( rc == SXRET_OK ){` |
|        - |  8785 | `			/* Mark as catch frame */` |
|       20 |  8786 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       20 |  8787 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       20 |  8788 | `			if( pObj ){` |
|        - |  8789 | `				/* Install the exception instance */` |
|       20 |  8790 | `				pThis->iRef++; /* Increment reference count */` |
|       20 |  8791 | `				pObj->x.pOther = pThis;` |
|       20 |  8792 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        9 |  8793 | `			}` |
|        - |  8794 | `			/* Exceute the block */` |
|       20 |  8795 | `			VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  8796 | `			/* Leave the frame */` |
|       20 |  8797 | `			VmLeaveFrame(&(*pVm));` |
|        9 |  8798 | `		}` |
|        - |  8799 | `	}` |
|        - |  8800 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  8801 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  8802 | `	 */` |
|       20 |  8803 | `	return SXRET_OK;` |
|      247 |  8804 |  |
|        - |  8805 | `/*` |
|        - |  8806 | ` * Section:` |
|        - |  8807 | ` *  Version,Credits and Copyright related functions.` |
|        - |  8808 | ` * Status:` |
|        - |  8809 | ` *    Stable.` |
|        - |  8810 | ` */` |
|        - |  8811 | `/*` |
|        - |  8812 | ` * string ph7version(void)` |
|        - |  8813 | ` *  Returns the running version of the PH7 version.` |
|        - |  8814 | ` * Parameters` |
|        - |  8815 | ` *  None` |
|        - |  8816 | ` * Return` |
|        - |  8817 | ` * Current PH7 version.` |
|        - |  8818 | ` */` |
|        2 |  8819 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8820 |  |
|        1 |  8821 | `	SXUNUSED(nArg);` |
|        1 |  8822 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  8823 | `	/* Current engine version */` |
|        3 |  8824 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  8825 | `	return PH7_OK;` |
|        1 |  8826 |  |
|        - |  8827 | `/*` |
|        - |  8828 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  8829 | ` */` |
|        - |  8830 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  8831 | ` "<html><head>"\` |
|        - |  8832 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  8833 | ` "<style type=\"text/css\">"\` |
|        - |  8834 | ` "div {"\` |
|        - |  8835 | `     "border: 1px solid #cccccc;"\` |
|        - |  8836 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  8837 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  8838 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  8839 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  8840 | `     "-webkit-border-radius: 10px;"\` |
|        - |  8841 | `     "-o-border-radius: 10px;"\` |
|        - |  8842 | `     "border-radius: 10px;"\` |
|        - |  8843 | `     "padding-left: 2em;"\` |
|        - |  8844 | `     "background-color: white;"\` |
|        - |  8845 | `     "margin-left: auto;"\` |
|        - |  8846 | `     "font-family: verdana;"\` |
|        - |  8847 | `     "padding-right: 2em;"\` |
|        - |  8848 | `     "margin-right: auto;"\` |
|        - |  8849 | `     "}"\` |
|        - |  8850 | `     "body {"\` |
|        - |  8851 | `     "padding: 0.2em;"\` |
|        - |  8852 | `     "font-style: normal;"\` |
|        - |  8853 | `     "font-size: medium;"\` |
|        - |  8854 | `     "background-color: #f2f2f2;"\` |
|        - |  8855 | `     "}"\` |
|        - |  8856 | `     "hr {"\` |
|        - |  8857 | `     "border-style: solid none none;"\` |
|        - |  8858 | `     "border-width: 1px medium medium;"\` |
|        - |  8859 | `     "border-top: 1px solid #cccccc;"\` |
|        - |  8860 | `     "height: 1px;"\` |
|        - |  8861 | `     "}"\` |
|        - |  8862 | `     "a {"\` |
|        - |  8863 | `     "color: #3366cc;"\` |
|        - |  8864 | `     "text-decoration: none;"\` |
|        - |  8865 | `     "}"\` |
|        - |  8866 | `     "a:hover {"\` |
|        - |  8867 | `     "color: #999999;"\` |
|        - |  8868 | `     "}"\` |
|        - |  8869 | `     "a:active {"\` |
|        - |  8870 | `     "color: #663399;"\` |
|        - |  8871 | `     "}"\` |
|        - |  8872 | `     "h1 {"\` |
|        - |  8873 | `     "margin: 0;"\` |
|        - |  8874 | `     "padding: 0;"\` |
|        - |  8875 | `     "font-family: Verdana;"\` |
|        - |  8876 | `     "font-weight: bold;"\` |
|        - |  8877 | `     "font-style: normal;"\` |
|        - |  8878 | `     "font-size: medium;"\` |
|        - |  8879 | `     "text-transform: capitalize;"\` |
|        - |  8880 | `     "color: #0a328c;"\` |
|        - |  8881 | `     "}"\` |
|        - |  8882 | `     "p {"\` |
|        - |  8883 | `     "margin: 0 auto;"\` |
|        - |  8884 | `     "font-size: medium;"\` |
|        - |  8885 | `     "font-style: normal;"\` |
|        - |  8886 | `     "font-family: verdana;"\` |
|        - |  8887 | `     "}"\` |
|        - |  8888 | `"</style></head><body>"\` |
|        - |  8889 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - |  8890 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - |  8891 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - |  8892 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - |  8893 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - |  8894 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - |  8895 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - |  8896 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - |  8897 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - |  8898 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - |  8899 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - |  8900 |  |
|        - |  8901 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8902 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - |  8903 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - |  8904 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - |  8905 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8906 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - |  8907 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  8908 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - |  8909 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  8910 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - |  8911 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8912 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - |  8913 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - |  8914 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - |  8915 |  |
|        - |  8916 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - |  8917 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - |  8918 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - |  8919 | `"&nbsp;*<br>"\` |
|        - |  8920 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - |  8921 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - |  8922 | `"&nbsp;* are met:<br>"\` |
|        - |  8923 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - |  8924 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - |  8925 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - |  8926 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - |  8927 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - |  8928 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - |  8929 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - |  8930 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - |  8931 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - |  8932 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - |  8933 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - |  8934 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - |  8935 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - |  8936 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - |  8937 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - |  8938 | `"&nbsp;*<br>"\` |
|        - |  8939 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - |  8940 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - |  8941 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - |  8942 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - |  8943 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - |  8944 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - |  8945 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - |  8946 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - |  8947 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - |  8948 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - |  8949 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - |  8950 | `"&nbsp;*/<br>"\` |
|        - |  8951 | `"</span></small></small></p>"\` |
|        - |  8952 | `"</div></body></html>"` |
|        - |  8953 | `/*` |
|        - |  8954 | ` * bool ph7credits(void)` |
|        - |  8955 | ` * bool ph7info(void)` |
|        - |  8956 | ` * bool ph7copyright(void)` |
|        - |  8957 | ` *  Prints out the credits for PH7 engine` |
|        - |  8958 | ` * Parameters` |
|        - |  8959 | ` *  None` |
|        - |  8960 | ` * Return` |
|        - |  8961 | ` *  Always TRUE` |
|        - |  8962 | ` */` |
|        2 |  8963 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8964 |  |
|        3 |  8965 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - |  8966 | `	/* Expand the HTML page above*/` |
|        3 |  8967 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 |  8968 | `	ph7_context_output_format(` |
|        1 |  8969 | `		pCtx,` |
|        - |  8970 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 |  8971 | `		ph7_lib_version(),   /* Engine version */` |
|        1 |  8972 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 |  8973 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 |  8974 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 |  8975 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 |  8976 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - |  8977 | `#ifdef __WINNT__` |
|        - |  8978 | `		"Windows NT"` |
|        - |  8979 | `#elif defined(__UNIXES__)` |
|        - |  8980 | `		"UNIX-Like"` |
|        - |  8981 | `#else` |
|        - |  8982 | `		"Other OS"` |
|        - |  8983 | `#endif` |
|        - |  8984 | `		);` |
|        3 |  8985 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 |  8986 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8987 | `	SXUNUSED(apArg);` |
|        - |  8988 | `	/* Return TRUE */` |
|        - |  8989 | `	//ph7_result_bool(pCtx,1);` |
|        3 |  8990 | `	return PH7_OK;` |
|        1 |  8991 |  |
|        - |  8992 | `/*` |
|        - |  8993 | ` * Section:` |
|        - |  8994 | ` *    URL related routines.` |
|        - |  8995 | ` * Status:` |
|        - |  8996 | ` *    Stable.` |
|        - |  8997 | ` */` |
|        - |  8998 | `/*` |
|        - |  8999 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - |  9000 | ` *  Parse a URL and return its fields.` |
|        - |  9001 | ` * Parameters` |
|        - |  9002 | ` *  $url` |
|        - |  9003 | ` *   The URL to parse.` |
|        - |  9004 | ` * $component` |
|        - |  9005 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - |  9006 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - |  9007 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - |  9008 | ` *  in which case the return value will be an integer).` |
|        - |  9009 | ` * Return` |
|        - |  9010 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - |  9011 | ` *  At least one element will be present within the array. Potential keys within` |
|        - |  9012 | ` *  this array are:` |
|        - |  9013 | ` *   scheme - e.g. http` |
|        - |  9014 | ` *   host` |
|        - |  9015 | ` *   port` |
|        - |  9016 | ` *   user` |
|        - |  9017 | ` *   pass` |
|        - |  9018 | ` *   path` |
|        - |  9019 | ` *   query - after the question mark ?` |
|        - |  9020 | ` *   fragment - after the hashmark #` |
|        - |  9021 | ` * Note:` |
|        - |  9022 | ` *  FALSE is returned on failure.` |
|        - |  9023 | ` *  This function work with relative URL unlike the one shipped` |
|        - |  9024 | ` *  with the standard PHP engine.` |
|        - |  9025 | ` */` |
|       28 |  9026 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9027 |  |
|        - |  9028 | `	const char *zStr; /* Input string */` |
|        - |  9029 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - |  9030 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - |  9031 | `	int nLen;` |
|        - |  9032 | `	sxi32 rc;` |
|       29 |  9033 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  9034 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  9035 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9036 | `		return PH7_OK;` |
|        - |  9037 | `	}` |
|        - |  9038 | `	/* Extract the given URI */` |
|       29 |  9039 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 |  9040 | `	if( nLen < 1 ){` |
|        - |  9041 | `		/* Nothing to process,return FALSE */` |
|        3 |  9042 | `		ph7_result_bool(pCtx,0);` |
|        3 |  9043 | `		return PH7_OK;` |
|        - |  9044 | `	}` |
|        - |  9045 | `	/* Get a parse */` |
|       27 |  9046 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 |  9047 | `	if( rc != SXRET_OK ){` |
|        - |  9048 | `		/* Malformed input,return FALSE */` |
|      ! 0 |  9049 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9050 | `		return PH7_OK;` |
|        - |  9051 | `	}` |
|       27 |  9052 | `	if( nArg > 1 ){` |
|      ! 0 |  9053 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - |  9054 | `		/* Refer to constant.c for constants values */` |
|      ! 0 |  9055 | `		switch(nComponent){` |
|      ! 0 |  9056 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 |  9057 | `			pComp = &sURI.sScheme;` |
|      ! 0 |  9058 | `			if( pComp->nByte < 1 ){` |
|        - |  9059 | `				/* No available value,return NULL */` |
|      ! 0 |  9060 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9061 | `			}else{` |
|      ! 0 |  9062 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9063 | `			}` |
|      ! 0 |  9064 | `			break;` |
|      ! 0 |  9065 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 |  9066 | `			pComp = &sURI.sHost;` |
|      ! 0 |  9067 | `			if( pComp->nByte < 1 ){` |
|        - |  9068 | `				/* No available value,return NULL */` |
|      ! 0 |  9069 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9070 | `			}else{` |
|      ! 0 |  9071 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9072 | `			}` |
|      ! 0 |  9073 | `			break;` |
|      ! 0 |  9074 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 |  9075 | `			pComp = &sURI.sPort;` |
|      ! 0 |  9076 | `			if( pComp->nByte < 1 ){` |
|        - |  9077 | `				/* No available value,return NULL */` |
|      ! 0 |  9078 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9079 | `			}else{` |
|      ! 0 |  9080 | `				int iPort = 0;` |
|        - |  9081 | `				/* Cast the value to integer */` |
|      ! 0 |  9082 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 |  9083 | `				ph7_result_int(pCtx,iPort);` |
|        - |  9084 | `			}` |
|      ! 0 |  9085 | `			break;` |
|      ! 0 |  9086 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 |  9087 | `			pComp = &sURI.sUser;` |
|      ! 0 |  9088 | `			if( pComp->nByte < 1 ){` |
|        - |  9089 | `				/* No available value,return NULL */` |
|      ! 0 |  9090 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9091 | `			}else{` |
|      ! 0 |  9092 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9093 | `			}` |
|      ! 0 |  9094 | `			break;` |
|      ! 0 |  9095 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 |  9096 | `			pComp = &sURI.sPass;` |
|      ! 0 |  9097 | `			if( pComp->nByte < 1 ){` |
|        - |  9098 | `				/* No available value,return NULL */` |
|      ! 0 |  9099 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9100 | `			}else{` |
|      ! 0 |  9101 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9102 | `			}` |
|      ! 0 |  9103 | `			break;` |
|      ! 0 |  9104 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 |  9105 | `			pComp = &sURI.sQuery;` |
|      ! 0 |  9106 | `			if( pComp->nByte < 1 ){` |
|        - |  9107 | `				/* No available value,return NULL */` |
|      ! 0 |  9108 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9109 | `			}else{` |
|      ! 0 |  9110 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9111 | `			}` |
|      ! 0 |  9112 | `			break;` |
|      ! 0 |  9113 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 |  9114 | `			pComp = &sURI.sFragment;` |
|      ! 0 |  9115 | `			if( pComp->nByte < 1 ){` |
|        - |  9116 | `				/* No available value,return NULL */` |
|      ! 0 |  9117 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9118 | `			}else{` |
|      ! 0 |  9119 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9120 | `			}` |
|      ! 0 |  9121 | `			break;` |
|      ! 0 |  9122 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 |  9123 | `			pComp = &sURI.sPath;` |
|      ! 0 |  9124 | `			if( pComp->nByte < 1 ){` |
|        - |  9125 | `				/* No available value,return NULL */` |
|      ! 0 |  9126 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9127 | `			}else{` |
|      ! 0 |  9128 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9129 | `			}` |
|      ! 0 |  9130 | `			break;` |
|      ! 0 |  9131 | `		default:` |
|        - |  9132 | `			/* No such entry,return NULL */` |
|      ! 0 |  9133 | `			ph7_result_null(pCtx);` |
|      ! 0 |  9134 | `			break;` |
|        - |  9135 | `		}` |
|      ! 0 |  9136 | `	}else{` |
|        - |  9137 | `		ph7_value *pArray,*pValue;` |
|        - |  9138 | `		/* Return an associative array */` |
|       27 |  9139 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 |  9140 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 |  9141 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9142 | `			/* Out of memory */` |
|      ! 0 |  9143 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9144 | `			/* Return false */` |
|      ! 0 |  9145 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |  9146 | `			return PH7_OK;` |
|        - |  9147 | `		}` |
|        - |  9148 | `		/* Fill the array */` |
|       27 |  9149 | `		pComp = &sURI.sScheme;` |
|       27 |  9150 | `		if( pComp->nByte > 0 ){` |
|       19 |  9151 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 |  9152 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 |  9153 | `		}` |
|        - |  9154 | `		/* Reset the string cursor */` |
|       27 |  9155 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9156 | `		pComp = &sURI.sHost;` |
|       27 |  9157 | `		if( pComp->nByte > 0 ){` |
|       25 |  9158 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 |  9159 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 |  9160 | `		}` |
|        - |  9161 | `		/* Reset the string cursor */` |
|       27 |  9162 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9163 | `		pComp = &sURI.sPort;` |
|       27 |  9164 | `		if( pComp->nByte > 0 ){` |
|       11 |  9165 | `			int iPort = 0;/* cc warning */` |
|        - |  9166 | `			/* Convert to integer */` |
|       11 |  9167 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 |  9168 | `			ph7_value_int(pValue,iPort);` |
|       11 |  9169 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 |  9170 | `		}` |
|        - |  9171 | `		/* Reset the string cursor */` |
|       27 |  9172 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9173 | `		pComp = &sURI.sUser;` |
|       27 |  9174 | `		if( pComp->nByte > 0 ){` |
|        7 |  9175 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9176 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 |  9177 | `		}` |
|        - |  9178 | `		/* Reset the string cursor */` |
|       27 |  9179 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9180 | `		pComp = &sURI.sPass;` |
|       27 |  9181 | `		if( pComp->nByte > 0 ){` |
|        7 |  9182 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9183 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 |  9184 | `		}` |
|        - |  9185 | `		/* Reset the string cursor */` |
|       27 |  9186 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9187 | `		pComp = &sURI.sPath;` |
|       27 |  9188 | `		if( pComp->nByte > 0 ){` |
|       17 |  9189 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 |  9190 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 |  9191 | `		}` |
|        - |  9192 | `		/* Reset the string cursor */` |
|       27 |  9193 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9194 | `		pComp = &sURI.sQuery;` |
|       27 |  9195 | `		if( pComp->nByte > 0 ){` |
|        5 |  9196 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9197 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 |  9198 | `		}` |
|        - |  9199 | `		/* Reset the string cursor */` |
|       27 |  9200 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9201 | `		pComp = &sURI.sFragment;` |
|       27 |  9202 | `		if( pComp->nByte > 0 ){` |
|        5 |  9203 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9204 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 |  9205 | `		}` |
|        - |  9206 | `		/* Return the created array */` |
|       27 |  9207 | `		ph7_result_value(pCtx,pArray);` |
|        - |  9208 | `		/* NOTE:` |
|        - |  9209 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - |  9210 | `		 * automatically as soon we return from this function.` |
|        - |  9211 | `		 */` |
|        - |  9212 | `	}` |
|        - |  9213 | `	/* All done */` |
|       27 |  9214 | `	return PH7_OK;` |
|       15 |  9215 |  |
|        - |  9216 | `/*` |
|        - |  9217 | ` * Section:` |
|        - |  9218 | ` *   Array related routines.` |
|        - |  9219 | ` * Status:` |
|        - |  9220 | ` *    Stable.` |
|        - |  9221 | ` * Note 2012-5-21 01:04:15:` |
|        - |  9222 | ` *  Array related functions that need access to the underlying` |
|        - |  9223 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - |  9224 | ` */` |
|        - |  9225 | `/*` |
|        - |  9226 | ` * The [compact()] function store it's state information in an instance` |
|        - |  9227 | ` * of the following structure.` |
|        - |  9228 | ` */` |
|        - |  9229 | `struct compact_data` |
|        - |  9230 |  |
|        - |  9231 | `	ph7_value *pArray;  /* Target array */` |
|        - |  9232 | `	int nRecCount;      /* Recursion count */` |
|        - |  9233 | `};` |
|        - |  9234 | `/*` |
|        - |  9235 | ` * Walker callback for the [compact()] function defined below.` |
|        - |  9236 | ` */` |
|      ! 0 |  9237 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 |  9238 |  |
|      ! 0 |  9239 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 |  9240 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 |  9241 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9242 | `	/* Act according to the hashmap value */` |
|      ! 0 |  9243 | `	if( ph7_value_is_string(pValue) ){` |
|        - |  9244 | `		SyString sVar;` |
|      ! 0 |  9245 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 |  9246 | `		if( sVar.nByte > 0 ){` |
|        - |  9247 | `			/* Query the current frame */` |
|      ! 0 |  9248 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - |  9249 | `			/* ^` |
|        - |  9250 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - |  9251 | `			 */` |
|      ! 0 |  9252 | `			if( pKey ){` |
|        - |  9253 | `				/* Perform the insertion */` |
|      ! 0 |  9254 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 |  9255 | `			}` |
|      ! 0 |  9256 | `		}` |
|      ! 0 |  9257 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - |  9258 | `		int rc;` |
|        - |  9259 | `		/* Recursively traverse this array */` |
|      ! 0 |  9260 | `		pData->nRecCount++;` |
|      ! 0 |  9261 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 |  9262 | `		pData->nRecCount--;` |
|      ! 0 |  9263 | `		return rc;` |
|        - |  9264 | `	}` |
|      ! 0 |  9265 | `	return SXRET_OK;` |
|      ! 0 |  9266 |  |
|        - |  9267 | `/*` |
|        - |  9268 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - |  9269 | ` *  Create array containing variables and their values.` |
|        - |  9270 | ` *  For each of these, compact() looks for a variable with that name` |
|        - |  9271 | ` *  in the current symbol table and adds it to the output array such` |
|        - |  9272 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - |  9273 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - |  9274 | ` *  Any strings that are not set will simply be skipped.` |
|        - |  9275 | ` * Parameters` |
|        - |  9276 | ` *  $varname` |
|        - |  9277 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - |  9278 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - |  9279 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - |  9280 | ` *   it recursively.` |
|        - |  9281 | ` * Return` |
|        - |  9282 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - |  9283 | ` */` |
|        2 |  9284 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9285 |  |
|        - |  9286 | `	ph7_value *pArray,*pObj;` |
|        3 |  9287 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9288 | `	const char *zName;` |
|        - |  9289 | `	SyString sVar;` |
|        - |  9290 | `	int i,nLen;` |
|        3 |  9291 | `	if( nArg < 1 ){` |
|        - |  9292 | `		/* Missing arguments,return NULL */` |
|      ! 0 |  9293 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9294 | `		return PH7_OK;` |
|        - |  9295 | `	}` |
|        - |  9296 | `	/* Create the array */` |
|        3 |  9297 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9298 | `	if( pArray == 0 ){` |
|        - |  9299 | `		/* Out of memory */` |
|      ! 0 |  9300 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9301 | `		/* Return NULL */` |
|      ! 0 |  9302 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9303 | `		return PH7_OK;` |
|        - |  9304 | `	}` |
|        - |  9305 | `	/* Perform the requested operation */` |
|        7 |  9306 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 |  9307 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 |  9308 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - |  9309 | `				struct compact_data sData;` |
|      ! 0 |  9310 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - |  9311 | `				/* Recursively walk the array */` |
|      ! 0 |  9312 | `				sData.nRecCount = 0;` |
|      ! 0 |  9313 | `				sData.pArray = pArray;` |
|      ! 0 |  9314 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 |  9315 | `			}` |
|      ! 0 |  9316 | `		}else{` |
|        - |  9317 | `			/* Extract variable name */` |
|        5 |  9318 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 |  9319 | `			if( nLen > 0 ){` |
|        5 |  9320 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - |  9321 | `				/* Check if the variable is available in the current frame */` |
|        5 |  9322 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 |  9323 | `				if( pObj ){` |
|        5 |  9324 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 |  9325 | `				}` |
|        2 |  9326 | `			}` |
|        - |  9327 | `		}` |
|        3 |  9328 | `	}` |
|        - |  9329 | `	/* Return the array */` |
|        3 |  9330 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9331 | `	return PH7_OK;` |
|        2 |  9332 |  |
|        - |  9333 | `/*` |
|        - |  9334 | ` * The [extract()] function store it's state information in an instance` |
|        - |  9335 | ` * of the following structure.` |
|        - |  9336 | ` */` |
|        - |  9337 | `typedef struct extract_aux_data extract_aux_data;` |
|        - |  9338 | `struct extract_aux_data` |
|        - |  9339 |  |
|        - |  9340 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - |  9341 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - |  9342 | `	const char *zPrefix;  /* Prefix name */` |
|        - |  9343 | `	int Prefixlen;        /* Prefix  length */` |
|        - |  9344 | `	int iFlags;           /* Control flags */` |
|        - |  9345 | `	char zWorker[1024];   /* Working buffer */` |
|        - |  9346 | `};` |
|        - |  9347 | `/* Forward declaration */` |
|        - |  9348 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - |  9349 | `/*` |
|        - |  9350 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - |  9351 | ` *   Import variables into the current symbol table from an array.` |
|        - |  9352 | ` * Parameters` |
|        - |  9353 | ` * $var_array` |
|        - |  9354 | ` *  An associative array. This function treats keys as variable names and values` |
|        - |  9355 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - |  9356 | ` *  table, subject to extract_type and prefix parameters.` |
|        - |  9357 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - |  9358 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - |  9359 | ` * $extract_type` |
|        - |  9360 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - |  9361 | ` *  It can be one of the following values:` |
|        - |  9362 | ` *   EXTR_OVERWRITE` |
|        - |  9363 | ` *       If there is a collision, overwrite the existing variable.` |
|        - |  9364 | ` *   EXTR_SKIP` |
|        - |  9365 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - |  9366 | ` *   EXTR_PREFIX_SAME` |
|        - |  9367 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - |  9368 | ` *   EXTR_PREFIX_ALL` |
|        - |  9369 | ` *       Prefix all variable names with prefix.` |
|        - |  9370 | ` *   EXTR_PREFIX_INVALID` |
|        - |  9371 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - |  9372 | ` *   EXTR_IF_EXISTS` |
|        - |  9373 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - |  9374 | ` *       otherwise do nothing.` |
|        - |  9375 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - |  9376 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - |  9377 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - |  9378 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - |  9379 | ` *      the current symbol table.` |
|        - |  9380 | ` * $prefix` |
|        - |  9381 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - |  9382 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - |  9383 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - |  9384 | ` *  underscore character.` |
|        - |  9385 | ` * Return` |
|        - |  9386 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - |  9387 | ` */` |
|        4 |  9388 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9389 |  |
|        - |  9390 | `	extract_aux_data sAux;` |
|        - |  9391 | `	ph7_hashmap *pMap;` |
|        5 |  9392 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - |  9393 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 |  9394 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9395 | `		return PH7_OK;` |
|        - |  9396 | `	}` |
|        - |  9397 | `	/* Point to the target hashmap */` |
|        5 |  9398 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 |  9399 | `	if( pMap->nEntry < 1 ){` |
|        - |  9400 | `		/* Empty map,return  0 */` |
|      ! 0 |  9401 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9402 | `		return PH7_OK;` |
|        - |  9403 | `	}` |
|        - |  9404 | `	/* Prepare the aux data */` |
|        5 |  9405 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 |  9406 | `	if( nArg > 1 ){` |
|        3 |  9407 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 |  9408 | `		if( nArg > 2 ){` |
|      ! 0 |  9409 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 |  9410 | `		}` |
|        1 |  9411 | `	}` |
|        5 |  9412 | `	sAux.pVm = pCtx->pVm;` |
|        - |  9413 | `	/* Invoke the worker callback */` |
|        5 |  9414 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - |  9415 | `	/* Number of variables successfully imported */` |
|        5 |  9416 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 |  9417 | `	return PH7_OK;` |
|        3 |  9418 |  |
|        - |  9419 | `/*` |
|        - |  9420 | ` * Worker callback for the [extract()] function defined` |
|        - |  9421 | ` * below.` |
|        - |  9422 | ` */` |
|        8 |  9423 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9424 |  |
|        9 |  9425 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 |  9426 | `	int iFlags = pAux->iFlags;` |
|        9 |  9427 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9428 | `	ph7_value *pObj;` |
|        - |  9429 | `	SyString sVar;` |
|        9 |  9430 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 |  9431 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 |  9432 | `	}` |
|        - |  9433 | `	/* Perform a string cast */` |
|        9 |  9434 | `	PH7_MemObjToString(pKey);` |
|        9 |  9435 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9436 | `		/* Unavailable variable name */` |
|      ! 0 |  9437 | `		return SXRET_OK;` |
|        - |  9438 | `	}` |
|        9 |  9439 | `	sVar.nByte = 0; /* cc warning */` |
|        9 |  9440 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 |  9441 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9442 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9443 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9444 | `			);` |
|      ! 0 |  9445 | `	}else{` |
|       13 |  9446 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 |  9447 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9448 | `	}` |
|        9 |  9449 | `	sVar.zString = pAux->zWorker;` |
|        - |  9450 | `	/* Try to extract the variable */` |
|        9 |  9451 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 |  9452 | `	if( pObj ){` |
|        - |  9453 | `		/* Collision */` |
|        5 |  9454 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 |  9455 | `			return SXRET_OK;` |
|        - |  9456 | `		}` |
|        5 |  9457 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 |  9458 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - |  9459 | `				/* Already prefixed */` |
|      ! 0 |  9460 | `				return SXRET_OK;` |
|        - |  9461 | `			}` |
|      ! 0 |  9462 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9463 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9464 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9465 | `				);` |
|      ! 0 |  9466 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 |  9467 | `		}` |
|        3 |  9468 | `	}else{` |
|        - |  9469 | `		/* Create the variable */` |
|        5 |  9470 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - |  9471 | `	}` |
|        9 |  9472 | `	if( pObj ){` |
|        - |  9473 | `		/* Overwrite the old value */` |
|        9 |  9474 | `		PH7_MemObjStore(pValue,pObj);` |
|        - |  9475 | `		/* Increment counter */` |
|        9 |  9476 | `		pAux->iCount++;` |
|        4 |  9477 | `	}` |
|        9 |  9478 | `	return SXRET_OK;` |
|        5 |  9479 |  |
|        - |  9480 | `/*` |
|        - |  9481 | ` * Worker callback for the [import_request_variables()] function` |
|        - |  9482 | ` * defined below.` |
|        - |  9483 | ` */` |
|        2 |  9484 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9485 |  |
|        3 |  9486 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 |  9487 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9488 | `	ph7_value *pObj;` |
|        - |  9489 | `	SyString sVar;` |
|        - |  9490 | `	/* Perform a string cast */` |
|        3 |  9491 | `	PH7_MemObjToString(pKey);` |
|        3 |  9492 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9493 | `		/* Unavailable variable name */` |
|      ! 0 |  9494 | `		return SXRET_OK;` |
|        - |  9495 | `	}` |
|        3 |  9496 | `	sVar.nByte = 0; /* cc warning */` |
|        3 |  9497 | `	if( pAux->Prefixlen > 0 ){` |
|        4 |  9498 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 |  9499 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 |  9500 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9501 | `			);` |
|        2 |  9502 | `	}else{` |
|      ! 0 |  9503 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 |  9504 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9505 | `	}` |
|        3 |  9506 | `	sVar.zString = pAux->zWorker;` |
|        - |  9507 | `	/* Extract the variable */` |
|        3 |  9508 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 |  9509 | `	if( pObj ){` |
|        3 |  9510 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 |  9511 | `	}` |
|        3 |  9512 | `	return SXRET_OK;` |
|        2 |  9513 |  |
|        - |  9514 | `/*` |
|        - |  9515 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - |  9516 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - |  9517 | ` * Parameters` |
|        - |  9518 | ` * $types` |
|        - |  9519 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - |  9520 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - |  9521 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - |  9522 | ` *  POST includes the POST uploaded file information.` |
|        - |  9523 | ` *  Note:` |
|        - |  9524 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - |  9525 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - |  9526 | ` * $prefix` |
|        - |  9527 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - |  9528 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - |  9529 | ` *  variable named $pref_userid.` |
|        - |  9530 | ` * Return` |
|        - |  9531 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9532 | ` */` |
|        2 |  9533 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9534 |  |
|        - |  9535 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - |  9536 | `	extract_aux_data sAux;` |
|        - |  9537 | `	int nLen,nPrefixLen;` |
|        - |  9538 | `	ph7_value *pSuper;` |
|        - |  9539 | `	ph7_vm *pVm;` |
|        - |  9540 | `	/* By default import only $_GET variables  */` |
|        3 |  9541 | `	zImport = "G";` |
|        3 |  9542 | `	nLen = (int)sizeof(char);` |
|        3 |  9543 | `	zPrefix = 0;` |
|        3 |  9544 | `	nPrefixLen = 0;` |
|        3 |  9545 | `	if( nArg > 0 ){` |
|        3 |  9546 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 |  9547 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 |  9548 | `		}` |
|        3 |  9549 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9550 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 |  9551 | `		}` |
|        1 |  9552 | `	}` |
|        - |  9553 | `	/* Point to the underlying VM */` |
|        3 |  9554 | `	pVm = pCtx->pVm;` |
|        - |  9555 | `	/* Initialize the aux data */` |
|        3 |  9556 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 |  9557 | `	sAux.zPrefix = zPrefix;` |
|        3 |  9558 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 |  9559 | `	sAux.pVm = pVm;` |
|        - |  9560 | `	/* Extract */` |
|        3 |  9561 | `	zEnd = &zImport[nLen];` |
|        5 |  9562 | `	while( zImport < zEnd ){` |
|        3 |  9563 | `		int c = zImport[0];` |
|        3 |  9564 | `		pSuper = 0;` |
|        3 |  9565 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - |  9566 | `			/* Import $_GET variables */` |
|        3 |  9567 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 |  9568 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - |  9569 | `			/* Import $_POST variables */` |
|      ! 0 |  9570 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 |  9571 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - |  9572 | `			/* Import $_COOKIE variables */` |
|      ! 0 |  9573 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 |  9574 | `		}` |
|        3 |  9575 | `		if( pSuper ){` |
|        - |  9576 | `			/* Iterate throw array entries */` |
|        3 |  9577 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 |  9578 | `		}` |
|        - |  9579 | `		/* Advance the cursor */` |
|        3 |  9580 | `		zImport++;` |
|        1 |  9581 | `	}` |
|        - |  9582 | `	/* All done,return TRUE*/` |
|        3 |  9583 | `	ph7_result_bool(pCtx,0);` |
|        3 |  9584 | `	return PH7_OK;` |
|        1 |  9585 |  |
|        - |  9586 | `/*` |
|        - |  9587 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |  9588 | ` * Refer to the eval() language construct implementation for more` |
|        - |  9589 | ` * information.` |
|        - |  9590 | ` */` |
|     9960 |  9591 | `static sxi32 VmEvalChunk(` |
|        - |  9592 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |  9593 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |  9594 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |  9595 | `	int iFlags,         /* Compile flag */` |
|        - |  9596 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |  9597 | `	)` |
|        2 |  9598 |  |
|        - |  9599 | `	SySet *pByteCode,aByteCode;` |
|        - |  9600 | `	SyBlob sSavedNs;` |
|     9962 |  9601 | `	ProcConsumer xErr = 0;` |
|     9962 |  9602 | `	void *pErrData = 0;` |
|        - |  9603 | `	/* Initialize bytecode container */` |
|     9962 |  9604 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     9962 |  9605 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |  9606 | `	/* Reset the code generator */` |
|     9962 |  9607 | `	if( bTrueReturn ){` |
|        - |  9608 | `		/* Included file,log compile-time errors */` |
|     7535 |  9609 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7535 |  9610 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3767 |  9611 | `	}` |
|     9962 |  9612 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |  9613 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - |  9614 | `	 * Each included file has its own namespace scope; after execution,` |
|        - |  9615 | `	 * the caller's namespace is restored. */` |
|     9962 |  9616 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|     9962 |  9617 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|     9962 |  9618 | `	if( bTrueReturn ){` |
|        - |  9619 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     7535 |  9620 | `		SyBlobReset(&pVm->sNamespace);` |
|     3767 |  9621 | `	}` |
|        - |  9622 | `	/* Swap bytecode container */` |
|     9962 |  9623 | `	pByteCode = pVm->pByteContainer;` |
|     9962 |  9624 | `	pVm->pByteContainer = &aByteCode;` |
|        - |  9625 | `	/* Compile the chunk */` |
|     9962 |  9626 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    14942 |  9627 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |  9628 | `		/* Compilation error,return false */` |
|        3 |  9629 | `		if( pCtx ){` |
|        3 |  9630 | `			ph7_result_bool(pCtx,0);` |
|        1 |  9631 | `		}` |
|        2 |  9632 | `	}else{` |
|        - |  9633 | `		/* Mount any newly defined classes */` |
|        - |  9634 | `		SyHashEntry *pEntry;` |
|        - |  9635 | `		ph7_class *pClass;` |
|        - |  9636 | `		ph7_value sResult; /* Return value */` |
|        - |  9637 | `		sxi32 rc;` |
|     9960 |  9638 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   274455 |  9639 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   259518 |  9640 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  9641 | `			/* Only mount classes that haven't been mounted yet */` |
|   259518 |  9642 | `			if( !pClass->bMounted ){` |
|    61036 |  9643 | `				rc = VmMountUserClass(pVm,pClass);` |
|    61036 |  9644 | `				if( rc != SXRET_OK ){` |
|        - |  9645 | `					/* Mount failure (likely memory error) */` |
|      ! 0 |  9646 | `					if( pCtx ){` |
|      ! 0 |  9647 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 |  9648 | `					}` |
|      ! 0 |  9649 | `					goto Cleanup;` |
|        - |  9650 | `				}` |
|    30517 |  9651 | `			}` |
|        2 |  9652 | `		}` |
|     9960 |  9653 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  9654 | `			/* Out of memory */` |
|      ! 0 |  9655 | `			if( pCtx ){` |
|      ! 0 |  9656 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  9657 | `			}` |
|      ! 0 |  9658 | `			goto Cleanup;` |
|        - |  9659 | `		}` |
|     9960 |  9660 | `		if( bTrueReturn ){` |
|        - |  9661 | `			/* Assume a boolean true return value */` |
|     7535 |  9662 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3768 |  9663 | `		}else{` |
|        - |  9664 | `			/* Assume a null return value */` |
|     2426 |  9665 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  9666 | `		}` |
|        - |  9667 | `		/* Execute the compiled chunk */` |
|     9960 |  9668 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|     9960 |  9669 | `		if( pCtx ){` |
|        - |  9670 | `			/* Set the execution result */` |
|     7548 |  9671 | `			ph7_result_value(pCtx,&sResult);` |
|     3773 |  9672 | `		}` |
|     9960 |  9673 | `		PH7_MemObjRelease(&sResult);` |
|        - |  9674 | `	}` |
|     4980 |  9675 | `Cleanup:` |
|        - |  9676 | `	/* Cleanup the mess left behind */` |
|     9962 |  9677 | `	pVm->pByteContainer = pByteCode;` |
|     9962 |  9678 | `	SySetRelease(&aByteCode);` |
|        - |  9679 | `	/* Restore caller's namespace state */` |
|     9962 |  9680 | `	SyBlobReset(&pVm->sNamespace);` |
|     9962 |  9681 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|     9962 |  9682 | `	SyBlobRelease(&sSavedNs);` |
|     9962 |  9683 | `	return SXRET_OK;` |
|        2 |  9684 |  |
|        - |  9685 | `/*` |
|        - |  9686 | ` * value eval(string $code)` |
|        - |  9687 | ` *   Evaluate a string as PHP code.` |
|        - |  9688 | ` * Parameter` |
|        - |  9689 | ` *  code: PHP code to evaluate.` |
|        - |  9690 | ` * Return` |
|        - |  9691 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  9692 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  9693 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  9694 | ` */` |
|       16 |  9695 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9696 |  |
|        - |  9697 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 |  9698 | `	if( nArg < 1 ){` |
|        - |  9699 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9700 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9701 | `		return SXRET_OK;` |
|        - |  9702 | `	}` |
|        - |  9703 | `	/* Chunk to evaluate */` |
|       18 |  9704 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 |  9705 | `	if( sChunk.nByte < 1 ){` |
|        - |  9706 | `		/* Empty string,return NULL */` |
|        3 |  9707 | `		ph7_result_null(pCtx);` |
|        3 |  9708 | `		return SXRET_OK;` |
|        - |  9709 | `	}` |
|        - |  9710 | `	/* Eval the chunk */` |
|       16 |  9711 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 |  9712 | `	return SXRET_OK;` |
|       10 |  9713 |  |
|        - |  9714 | `/*` |
|        - |  9715 | ` * Check if a file path is already included.` |
|        - |  9716 | ` */` |
|    15064 |  9717 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 |  9718 |  |
|        - |  9719 | `	SyString *aEntries;` |
|        - |  9720 | `	sxu32 n;` |
|    15065 |  9721 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  9722 | `	/* Perform a linear search */` |
| 56720651 |  9723 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 56705593 |  9724 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  9725 | `			/* Already included */` |
|        7 |  9726 | `			return TRUE;` |
|        - |  9727 | `		}` |
| 28352794 |  9728 | `	}` |
|    15059 |  9729 | `	return FALSE;` |
|     7533 |  9730 |  |
|        - |  9731 | `/*` |
|        - |  9732 | ` * Push a file path in the appropriate VM container.` |
|        - |  9733 | ` */` |
|    17468 |  9734 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 |  9735 |  |
|        - |  9736 | `	SyString sPath;` |
|        - |  9737 | `	char *zDup;` |
|        - |  9738 | `#ifdef __WINNT__` |
|        - |  9739 | `	char *zCur;` |
|        - |  9740 | `#endif` |
|        - |  9741 | `	sxi32 rc;` |
|    17470 |  9742 | `	if( nLen < 0 ){` |
|     2406 |  9743 | `		nLen = SyStrlen(zPath);` |
|     1202 |  9744 | `	}` |
|        - |  9745 | `	/* Duplicate the file path first */` |
|    17470 |  9746 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    17470 |  9747 | `	if( zDup == 0 ){` |
|      ! 0 |  9748 | `		return SXERR_MEM;` |
|        - |  9749 | `	}` |
|        - |  9750 | `#ifdef __WINNT__` |
|        - |  9751 | `	/* Normalize path on windows` |
|        - |  9752 | `	 * Example:` |
|        - |  9753 | `	 *    Path/To/File.php` |
|        - |  9754 | `	 * becomes` |
|        - |  9755 | `	 *   path\to\file.php` |
|        - |  9756 | `	 */` |
|        2 |  9757 | `	zCur = zDup;` |
|        2 |  9758 | `	while( zCur[0] != 0 ){` |
|        2 |  9759 | `		if( zCur[0] == '/' ){` |
|        2 |  9760 | `			zCur[0] = '\\';` |
|        2 |  9761 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 |  9762 | `			int c = SyToLower(zCur[0]);` |
|        1 |  9763 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - |  9764 | `		}` |
|        2 |  9765 | `		zCur++;` |
|        2 |  9766 | `	}` |
|        - |  9767 | `#endif` |
|        - |  9768 | `	/* Install the file path */` |
|    17470 |  9769 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    17470 |  9770 | `	if( !bMain ){` |
|    15065 |  9771 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  9772 | `			/* Already included */` |
|        7 |  9773 | `			*pNew = 0;` |
|        4 |  9774 | `		}else{` |
|        - |  9775 | `			/* Insert in the corresponding container */` |
|    15059 |  9776 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    15059 |  9777 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9778 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  9779 | `				return rc;` |
|        - |  9780 | `			}` |
|    15059 |  9781 | `			*pNew = 1;` |
|        - |  9782 | `		}` |
|     7532 |  9783 | `	}` |
|    17470 |  9784 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    17470 |  9785 | `	return SXRET_OK;` |
|     8736 |  9786 |  |
|        - |  9787 | `/*` |
|        - |  9788 | ` * Compile and Execute a PHP script at run-time.` |
|        - |  9789 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - |  9790 | ` * indicates failure.` |
|        - |  9791 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - |  9792 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - |  9793 | ` * operations.` |
|        - |  9794 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - |  9795 | ` * this function is a no-op.` |
|        - |  9796 | ` * Refer to the implementation of the include(),include_once() language` |
|        - |  9797 | ` * constructs for more information.` |
|        - |  9798 | ` */` |
|     7540 |  9799 | `static sxi32 VmExecIncludedFile(` |
|        - |  9800 | `	 ph7_context *pCtx, /* Call Context */` |
|        - |  9801 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - |  9802 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - |  9803 | `	 )` |
|        2 |  9804 |  |
|        - |  9805 | `	sxi32 rc;` |
|        - |  9806 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9807 | `	const ph7_io_stream *pStream;` |
|        - |  9808 | `	SyBlob sContents;` |
|        - |  9809 | `	void *pHandle;` |
|        - |  9810 | `	ph7_vm *pVm;` |
|        - |  9811 | `	int isNew;` |
|        - |  9812 | `	/* Initialize fields */` |
|     7542 |  9813 | `	pVm = pCtx->pVm;` |
|     7542 |  9814 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7542 |  9815 | `	isNew = 0;` |
|        - |  9816 | `	/* Extract the associated stream */` |
|     7542 |  9817 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - |  9818 | `	/*` |
|        - |  9819 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - |  9820 | `	 * in a read-only mode.` |
|        - |  9821 | `	 */` |
|     7542 |  9822 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7542 |  9823 | `	if( pHandle == 0 ){` |
|        3 |  9824 | `		return SXERR_IO;` |
|        - |  9825 | `	}` |
|     7539 |  9826 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7539 |  9827 | `	if( IncludeOnce && !isNew ){` |
|        - |  9828 | `		/* Already included */` |
|        5 |  9829 | `		rc = SXERR_EXISTS;` |
|        3 |  9830 | `	}else{` |
|        - |  9831 | `		/* Read the whole file contents */` |
|     7535 |  9832 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7535 |  9833 | `		if( rc == SXRET_OK ){` |
|        - |  9834 | `			SyString sScript;` |
|        - |  9835 | `			/* Compile and execute the script */` |
|     7535 |  9836 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7535 |  9837 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3767 |  9838 | `		}` |
|        - |  9839 | `	}` |
|        - |  9840 | `	/* Pop from the set of included file */` |
|     7539 |  9841 | `	(void)SySetPop(&pVm->aFiles);` |
|        - |  9842 | `	/* Close the handle */` |
|     7539 |  9843 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - |  9844 | `	/* Release the working buffer */` |
|     7539 |  9845 | `	SyBlobRelease(&sContents);` |
|        - |  9846 | `#else` |
|        - |  9847 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - |  9848 | `	SXUNUSED(pPath);` |
|        - |  9849 | `	SXUNUSED(IncludeOnce);` |
|        - |  9850 | `	rc = SXERR_IO;` |
|        - |  9851 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7539 |  9852 | `	return rc;` |
|     3772 |  9853 |  |
|        - |  9854 | `/*` |
|        - |  9855 | ` * string get_include_path(void)` |
|        - |  9856 | ` *  Gets the current include_path configuration option.` |
|        - |  9857 | ` * Parameter` |
|        - |  9858 | ` *  None` |
|        - |  9859 | ` * Return` |
|        - |  9860 | ` *  Included paths as a string` |
|        - |  9861 | ` */` |
|        2 |  9862 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9863 |  |
|        3 |  9864 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9865 | `	SyString *aEntry;` |
|        - |  9866 | `	int dir_sep;` |
|        - |  9867 | `	sxu32 n;` |
|        - |  9868 | `#ifdef __WINNT__` |
|        1 |  9869 | `	dir_sep = ';';` |
|        - |  9870 | `#else` |
|        - |  9871 | `	/* Assume UNIX path separator */` |
|        2 |  9872 | `	dir_sep = ':';` |
|        - |  9873 | `#endif` |
|        1 |  9874 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9875 | `	SXUNUSED(apArg);` |
|        - |  9876 | `	/* Point to the list of import paths */` |
|        3 |  9877 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 |  9878 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 |  9879 | `		SyString *pEntry = &aEntry[n];` |
|        3 |  9880 | `		if( n > 0 ){` |
|        - |  9881 | `			/* Append dir seprator */` |
|      ! 0 |  9882 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 |  9883 | `		}` |
|        - |  9884 | `		/* Append path */` |
|        3 |  9885 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 |  9886 | `	}` |
|        3 |  9887 | `	return PH7_OK;` |
|        1 |  9888 |  |
|        - |  9889 | `/*` |
|        - |  9890 | ` * string get_get_included_files(void)` |
|        - |  9891 | ` *  Gets the current include_path configuration option.` |
|        - |  9892 | ` * Parameter` |
|        - |  9893 | ` *  None` |
|        - |  9894 | ` * Return` |
|        - |  9895 | ` *  Included paths as a string` |
|        - |  9896 | ` */` |
|        2 |  9897 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9898 |  |
|        3 |  9899 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - |  9900 | `	ph7_value *pArray,*pWorker;` |
|        - |  9901 | `	SyString *pEntry;` |
|        - |  9902 | `	int c,d;` |
|        - |  9903 | `	/* Create an array and a working value */` |
|        3 |  9904 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 |  9905 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 |  9906 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - |  9907 | `		/* Out of memory,return null */` |
|      ! 0 |  9908 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9909 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9910 | `		SXUNUSED(apArg);` |
|      ! 0 |  9911 | `		return PH7_OK;` |
|        - |  9912 | `	}` |
|        3 |  9913 | `	c = d = '/';` |
|        - |  9914 | `#ifdef __WINNT__` |
|        1 |  9915 | `	d = '\\';` |
|        - |  9916 | `#endif` |
|        - |  9917 | `	/* Iterate throw entries */` |
|        3 |  9918 | `	SySetResetCursor(pFiles);` |
|     3691 |  9919 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - |  9920 | `		const char *zBase,*zEnd;` |
|        - |  9921 | `		int iLen;` |
|        - |  9922 | `		/* reset the string cursor */` |
|     3689 |  9923 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - |  9924 | `		/* Extract base name */` |
|     3689 |  9925 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - |  9926 | `		/* Ignore trailing '/' */` |
|     5533 |  9927 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 |  9928 | `			zEnd--;` |
|      ! 0 |  9929 | `		}` |
|     3689 |  9930 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   113825 |  9931 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   108293 |  9932 | `			zEnd--;` |
|        1 |  9933 | `		}` |
|     3689 |  9934 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3689 |  9935 | `		zEnd = &pEntry->zString[iLen];` |
|        - |  9936 | `		/* Copy entry name */` |
|     3689 |  9937 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - |  9938 | `		/* Perform the insertion */` |
|     3689 |  9939 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 |  9940 | `	}` |
|        - |  9941 | `	/* All done,return the created array */` |
|        3 |  9942 | `	ph7_result_value(pCtx,pArray);` |
|        - |  9943 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - |  9944 | `	 * by the engine as soon we return from this foreign` |
|        - |  9945 | `	 * function.` |
|        - |  9946 | `	 */` |
|        3 |  9947 | `	return PH7_OK;` |
|        2 |  9948 |  |
|        - |  9949 | `/*` |
|        - |  9950 | ` * include:` |
|        - |  9951 | ` * According to the PHP reference manual.` |
|        - |  9952 | ` *  The include() function includes and evaluates the specified file.` |
|        - |  9953 | ` *  Files are included based on the file path given or, if none is given` |
|        - |  9954 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - |  9955 | ` *  include() will finally check in the calling script's own directory` |
|        - |  9956 | ` *  and the current working directory before failing. The include()` |
|        - |  9957 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - |  9958 | ` *  behavior from require(), which will emit a fatal error.` |
|        - |  9959 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - |  9960 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - |  9961 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - |  9962 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - |  9963 | ` *  directory to find the requested file.` |
|        - |  9964 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - |  9965 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - |  9966 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - |  9967 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - |  9968 | ` */` |
|     7528 |  9969 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9970 |  |
|        - |  9971 | `	SyString sFile;` |
|        - |  9972 | `	sxi32 rc;` |
|     7530 |  9973 | `	if( nArg < 1 ){` |
|        - |  9974 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9975 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9976 | `		return SXRET_OK;` |
|        - |  9977 | `	}` |
|        - |  9978 | `	/* File to include */` |
|     7530 |  9979 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7530 |  9980 | `	if( sFile.nByte < 1 ){` |
|        - |  9981 | `		/* Empty string,return NULL */` |
|      ! 0 |  9982 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9983 | `		return SXRET_OK;` |
|        - |  9984 | `	}` |
|        - |  9985 | `	/* Open,compile and execute the desired script */` |
|     7530 |  9986 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7530 |  9987 | `	if( rc != SXRET_OK ){` |
|        - |  9988 | `		/* Emit a warning and return false */` |
|        3 |  9989 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 |  9990 | `		ph7_result_bool(pCtx,0);` |
|        1 |  9991 | `	}` |
|     7530 |  9992 | `	return SXRET_OK;` |
|     3766 |  9993 |  |
|        - |  9994 | `/*` |
|        - |  9995 | ` * include_once:` |
|        - |  9996 | ` *  According to the PHP reference manual.` |
|        - |  9997 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - |  9998 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - |  9999 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 10000 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 10001 | ` *   just once.` |
|        - | 10002 | ` */` |
|        4 | 10003 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10004 |  |
|        - | 10005 | `	SyString sFile;` |
|        - | 10006 | `	sxi32 rc;` |
|        5 | 10007 | `	if( nArg < 1 ){` |
|        - | 10008 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10009 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10010 | `		return SXRET_OK;` |
|        - | 10011 | `	}` |
|        - | 10012 | `	/* File to include */` |
|        5 | 10013 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10014 | `	if( sFile.nByte < 1 ){` |
|        - | 10015 | `		/* Empty string,return NULL */` |
|      ! 0 | 10016 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10017 | `		return SXRET_OK;` |
|        - | 10018 | `	}` |
|        - | 10019 | `	/* Open,compile and execute the desired script */` |
|        5 | 10020 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 10021 | `	if( rc == SXERR_EXISTS ){` |
|        - | 10022 | `		/* File already included,return TRUE */` |
|        3 | 10023 | `		ph7_result_bool(pCtx,1);` |
|        3 | 10024 | `		return SXRET_OK;` |
|        - | 10025 | `	}` |
|        3 | 10026 | `	if( rc != SXRET_OK ){` |
|        - | 10027 | `		/* Emit a warning and return false */` |
|      ! 0 | 10028 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10029 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10030 | ` 	}` |
|        3 | 10031 | `	return SXRET_OK;` |
|        3 | 10032 |  |
|        - | 10033 | `/*` |
|        - | 10034 | ` * require.` |
|        - | 10035 | ` *  According to the PHP reference manual.` |
|        - | 10036 | ` *   require() is identical to include() except upon failure it will` |
|        - | 10037 | ` *   also produce a fatal level error.` |
|        - | 10038 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 10039 | ` *   emits a warning  which allows the script to continue.` |
|        - | 10040 | ` */` |
|        4 | 10041 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10042 |  |
|        - | 10043 | `	SyString sFile;` |
|        - | 10044 | `	sxi32 rc;` |
|        5 | 10045 | `	if( nArg < 1 ){` |
|        - | 10046 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10047 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10048 | `		return SXRET_OK;` |
|        - | 10049 | `	}` |
|        - | 10050 | `	/* File to include */` |
|        5 | 10051 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10052 | `	if( sFile.nByte < 1 ){` |
|        - | 10053 | `		/* Empty string,return NULL */` |
|      ! 0 | 10054 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10055 | `		return SXRET_OK;` |
|        - | 10056 | `	}` |
|        - | 10057 | `	/* Open,compile and execute the desired script */` |
|        5 | 10058 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 10059 | `	if( rc != SXRET_OK ){` |
|        - | 10060 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10061 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10062 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10063 | `		return PH7_ABORT;` |
|        - | 10064 | `	}` |
|        5 | 10065 | `	return SXRET_OK;` |
|        3 | 10066 |  |
|        - | 10067 | `/*` |
|        - | 10068 | ` * require_once:` |
|        - | 10069 | ` *  According to the PHP reference manual.` |
|        - | 10070 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 10071 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 10072 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 10073 | ` *   and how it differs from its non _once siblings.` |
|        - | 10074 | ` */` |
|        4 | 10075 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10076 |  |
|        - | 10077 | `	SyString sFile;` |
|        - | 10078 | `	sxi32 rc;` |
|        5 | 10079 | `	if( nArg < 1 ){` |
|        - | 10080 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10081 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10082 | `		return SXRET_OK;` |
|        - | 10083 | `	}` |
|        - | 10084 | `	/* File to include */` |
|        5 | 10085 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10086 | `	if( sFile.nByte < 1 ){` |
|        - | 10087 | `		/* Empty string,return NULL */` |
|      ! 0 | 10088 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10089 | `		return SXRET_OK;` |
|        - | 10090 | `	}` |
|        - | 10091 | `	/* Open,compile and execute the desired script */` |
|        5 | 10092 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 10093 | `	if( rc == SXERR_EXISTS ){` |
|        - | 10094 | `		/* File already included,return TRUE */` |
|        3 | 10095 | `		ph7_result_bool(pCtx,1);` |
|        3 | 10096 | `		return SXRET_OK;` |
|        - | 10097 | `	}` |
|        3 | 10098 | `	if( rc != SXRET_OK ){` |
|        - | 10099 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10100 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10101 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10102 | `		return PH7_ABORT;` |
|        - | 10103 | `	}` |
|        3 | 10104 | `	return SXRET_OK;` |
|        3 | 10105 |  |
|        - | 10106 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 10107 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 10108 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 10109 | `/* Table of built-in VM functions. */` |
|        - | 10110 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 10111 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 10112 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 10113 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 10114 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 10115 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 10116 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 10117 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 10118 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 10119 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 10120 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 10121 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 10122 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 10123 | `	    /* Constants management */` |
|        - | 10124 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 10125 | `	{ "define",   vm_builtin_define               },` |
|        - | 10126 | `	{ "constant", vm_builtin_constant             },` |
|        - | 10127 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 10128 | `	   /* Class/Object functions */` |
|        - | 10129 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 10130 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 10131 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 10132 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 10133 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 10134 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 10135 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 10136 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 10137 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 10138 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 10139 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 10140 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 10141 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 10142 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 10143 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 10144 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 10145 | `	   /* Random numbers/strings generators */` |
|        - | 10146 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 10147 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 10148 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 10149 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 10150 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 10151 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10152 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 10153 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 10154 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 10155 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10156 | `	   /* Language constructs functions */` |
|        - | 10157 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 10158 | `	{ "print", vm_builtin_print                   },` |
|        - | 10159 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 10160 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 10161 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 10162 | `	  /* Variable handling functions */` |
|        - | 10163 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 10164 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 10165 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 10166 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 10167 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 10168 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 10169 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 10170 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 10171 | `	  /* Ouput control functions */` |
|        - | 10172 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 10173 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 10174 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 10175 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 10176 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 10177 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 10178 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 10179 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 10180 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 10181 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 10182 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 10183 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 10184 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 10185 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 10186 | `	  /* Assertion functions */` |
|        - | 10187 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 10188 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 10189 | `	  /* Error reporting functions */` |
|        - | 10190 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 10191 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 10192 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 10193 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 10194 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 10195 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 10196 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 10197 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 10198 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 10199 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 10200 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 10201 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 10202 | `	  /* Release info */` |
|        - | 10203 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 10204 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 10205 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 10206 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 10207 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 10208 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 10209 | `	  /* hashmap */` |
|        - | 10210 | `	{"compact",          vm_builtin_compact       },` |
|        - | 10211 | `	{"extract",          vm_builtin_extract       },` |
|        - | 10212 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 10213 | `	  /* URL related function */` |
|        - | 10214 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 10215 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 10216 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10217 | `	   /* XML processing functions */` |
|        - | 10218 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 10219 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 10220 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 10221 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 10222 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 10223 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 10224 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 10225 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 10226 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 10227 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 10228 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 10229 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 10230 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 10231 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 10232 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 10233 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 10234 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 10235 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 10236 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 10237 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 10238 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 10239 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10240 | `	   /* UTF-8 encoding/decoding */` |
|        - | 10241 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 10242 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 10243 | `	   /* Command line processing */` |
|        - | 10244 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 10245 | `	   /* JSON encoding/decoding */` |
|        - | 10246 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 10247 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 10248 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 10249 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 10250 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 10251 | `	   /* Files/URI inclusion facility */` |
|        - | 10252 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 10253 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 10254 | `	{ "include",      vm_builtin_include          },` |
|        - | 10255 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 10256 | `	{ "require",      vm_builtin_require          },` |
|        - | 10257 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 10258 | `};` |
|        - | 10259 | `/*` |
|        - | 10260 | ` * Register the built-in VM functions defined above.` |
|        - | 10261 | ` */` |
|     2178 | 10262 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 10263 |  |
|        - | 10264 | `	sxi32 rc;` |
|        - | 10265 | `	sxu32 n;` |
|   272252 | 10266 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 10267 | `		/* Note that these special functions have access` |
|        - | 10268 | `		 * to the underlying virtual machine as their` |
|        - | 10269 | `		 * private data.` |
|        - | 10270 | `		 */` |
|   270074 | 10271 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   270074 | 10272 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 10273 | `			return rc;` |
|        - | 10274 | `		}` |
|   135038 | 10275 | `	}` |
|     2180 | 10276 | `	return SXRET_OK;` |
|     1091 | 10277 |  |
|        - | 10278 | `/*` |
|        - | 10279 | ` * Check if the given name refer to an installed class.` |
|        - | 10280 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 10281 | ` */` |
|    15778 | 10282 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 10283 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 10284 | `	const char *zName,  /* Name of the target class */` |
|        - | 10285 | `	sxu32 nByte,        /* zName length */` |
|        - | 10286 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 10287 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 10288 | `						 */` |
|        - | 10289 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 10290 | `	)` |
|        2 | 10291 |  |
|        - | 10292 | `	SyHashEntry *pEntry;` |
|        - | 10293 | `	ph7_class *pClass;` |
|     7889 | 10294 | `	SXUNUSED(iNest);` |
|        - | 10295 | `	/* Exact class lookup.` |
|        - | 10296 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 10297 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    15780 | 10298 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    15780 | 10299 | `	if( pEntry == 0 ){` |
|        7 | 10300 | `		return 0;` |
|        - | 10301 | `	}` |
|    15774 | 10302 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    15774 | 10303 | `	if( !iLoadable ){` |
|    14688 | 10304 | `		return pClass;` |
|        - | 10305 | `	}` |
|        - | 10306 | `	/* Filter for loadable classes (skip interfaces/abstract/traits) */` |
|     1088 | 10307 | `	while(pClass){` |
|     1088 | 10308 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1088 | 10309 | `			return pClass;` |
|        - | 10310 | `		}` |
|      ! 0 | 10311 | `		pClass = pClass->pNextName;` |
|      ! 0 | 10312 | `	}` |
|      ! 0 | 10313 | `	return 0;` |
|     7891 | 10314 |  |
|        - | 10315 | `/*` |
|        - | 10316 | ` * Reference Table Implementation` |
|        - | 10317 | ` * Status: stable <chm@symisc.net>` |
|        - | 10318 | ` * Intro` |
|        - | 10319 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 10320 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 10321 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 10322 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 10323 | ` *  Refer to the official for more information on this powerful` |
|        - | 10324 | ` *  extension.` |
|        - | 10325 | ` */` |
|        - | 10326 | `/*` |
|        - | 10327 | ` * Allocate a new reference entry.` |
|        - | 10328 | ` */` |
|  2989492 | 10329 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 10330 |  |
|        - | 10331 | `	VmRefObj *pRef;` |
|        - | 10332 | `	/* Allocate a new instance */` |
|  2989494 | 10333 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  2989494 | 10334 | `	if( pRef == 0 ){` |
|      ! 0 | 10335 | `		return 0;` |
|        - | 10336 | `	}` |
|        - | 10337 | `	/* Zero the structure */` |
|  2989494 | 10338 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 10339 | `	/* Initialize fields */` |
|  2989494 | 10340 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  2989494 | 10341 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  2989494 | 10342 | `	pRef->nIdx = nIdx;` |
|  2989494 | 10343 | `	return pRef;` |
|  1494748 | 10344 |  |
|        - | 10345 | `/*` |
|        - | 10346 | ` * Default hash function used by the reference table` |
|        - | 10347 | ` * for lookup/insertion operations.` |
|        - | 10348 | ` */` |
| 16589292 | 10349 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 10350 |  |
|        - | 10351 | `	/* Calculate the hash based on the memory object index */` |
| 16589294 | 10352 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 10353 |  |
|        - | 10354 | `/*` |
|        - | 10355 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 10356 | ` * in the reference table.` |
|        - | 10357 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 10358 | ` * otherwise.` |
|        - | 10359 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10360 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10361 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10362 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10363 | ` * Refer to the official for more information on this powerful` |
|        - | 10364 | ` * extension.` |
|        - | 10365 | ` */` |
|  8925534 | 10366 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 10367 |  |
|        - | 10368 | `	VmRefObj *pRef;` |
|        - | 10369 | `	sxu32 nBucket;` |
|        - | 10370 | `	/* Point to the appropriate bucket */` |
|  8925536 | 10371 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 10372 | `	/* Perform the lookup */` |
|  8925536 | 10373 | `	pRef = pVm->apRefObj[nBucket];` |
| 18827984 | 10374 | `	for(;;){` |
| 37650647 | 10375 | `		if( pRef == 0 ){` |
|  3063882 | 10376 | `			break;` |
|        - | 10377 | `		}` |
| 34586767 | 10378 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 10379 | `			/* Entry found */` |
|  5861656 | 10380 | `			return pRef;` |
|        - | 10381 | `		}` |
|        - | 10382 | `		/* Point to the next entry */` |
| 28725113 | 10383 | `		pRef = pRef->pNextCollide;` |
|        2 | 10384 | `	}` |
|        - | 10385 | `	/* No such entry,return NULL */` |
|  3063882 | 10386 | `	return 0;` |
|  4462769 | 10387 |  |
|        - | 10388 | `/*` |
|        - | 10389 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10390 | ` *` |
|        - | 10391 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10392 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10393 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10394 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10395 | ` * Refer to the official for more information on this powerful` |
|        - | 10396 | ` * extension.` |
|        - | 10397 | ` */` |
|  2989492 | 10398 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10399 |  |
|        - | 10400 | `	sxu32 nBucket;` |
|  2989494 | 10401 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 10402 | `		VmRefObj **apNew;` |
|        - | 10403 | `		sxu32 nNew;` |
|        - | 10404 | `		/* Allocate a larger table */` |
|     3422 | 10405 | `		nNew = pVm->nRefSize << 1;` |
|     3422 | 10406 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     3422 | 10407 | `		if( apNew ){` |
|     3422 | 10408 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 10409 | `			sxu32 n;` |
|        - | 10410 | `			/* Zero the structure */` |
|     3422 | 10411 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 10412 | `			/* Rehash all referenced entries */` |
|  2834312 | 10413 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 10414 | `				/* Remove old collision links */` |
|  2830892 | 10415 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 10416 | `				/* Point to the appropriate bucket */` |
|  2830892 | 10417 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 10418 | `				/* Insert the entry  */` |
|  2830892 | 10419 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2830892 | 10420 | `				if( apNew[nBucket] ){` |
|  2298896 | 10421 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 10422 | `				}` |
|  2830892 | 10423 | `				apNew[nBucket] = pEntry;` |
|        - | 10424 | `				/* Point to the next entry */` |
|  2830892 | 10425 | `				pEntry = pEntry->pNext;` |
|  1415447 | 10426 | `			}` |
|        - | 10427 | `			/* Release the old table */` |
|     3422 | 10428 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 10429 | `			/* Install the new one */` |
|     3422 | 10430 | `			pVm->apRefObj = apNew;` |
|     3422 | 10431 | `			pVm->nRefSize = nNew;` |
|     1710 | 10432 | `		}` |
|     1710 | 10433 | `	}` |
|        - | 10434 | `	/* Point to the appropriate bucket */` |
|  2989494 | 10435 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 10436 | `	/* Insert the entry */` |
|  2989494 | 10437 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  2989494 | 10438 | `	if( pVm->apRefObj[nBucket] ){` |
|  2482292 | 10439 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1241020 | 10440 | `	}` |
|  2989494 | 10441 | `	pVm->apRefObj[nBucket] = pRef;` |
|  2989494 | 10442 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  2989494 | 10443 | `	pVm->nRefUsed++;` |
|  2989494 | 10444 | `	return SXRET_OK;` |
|        2 | 10445 |  |
|        - | 10446 | `/*` |
|        - | 10447 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 10448 | ` * the reference table.` |
|        - | 10449 | ` * This function is invoked when the user perform an unset` |
|        - | 10450 | ` * call [i.e: unset($var); ].` |
|        - | 10451 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10452 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10453 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10454 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10455 | ` * Refer to the official for more information on this powerful` |
|        - | 10456 | ` * extension.` |
|        - | 10457 | ` */` |
|  2958522 | 10458 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10459 |  |
|        - | 10460 | `	ph7_hashmap_node **apNode;` |
|        - | 10461 | `	SyHashEntry **apEntry;` |
|        - | 10462 | `	sxu32 n;` |
|        - | 10463 | `	/* Point to the reference table */` |
|  2958524 | 10464 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2958524 | 10465 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 10466 | `	/* Unlink the entry from the reference table */` |
|  3037874 | 10467 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    79352 | 10468 | `		if( apEntry[n] ){` |
|    79302 | 10469 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    39650 | 10470 | `		}` |
|    39677 | 10471 | `	}` |
|  5839592 | 10472 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2881070 | 10473 | `		if( apNode[n] ){` |
|     5635 | 10474 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2817 | 10475 | `		}` |
|  1440536 | 10476 | `	}` |
|  2958524 | 10477 | `	if( pRef->pPrevCollide ){` |
|  1115148 | 10478 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   557905 | 10479 | `	}else{` |
|  1843378 | 10480 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 10481 | `	}` |
|  2958524 | 10482 | `	if( pRef->pNextCollide ){` |
|  1670845 | 10483 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   835284 | 10484 | `	}` |
|  2958524 | 10485 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 10486 | `	/* Release the node */` |
|  2958524 | 10487 | `	SySetRelease(&pRef->aReference);` |
|  2958524 | 10488 | `	SySetRelease(&pRef->aArrEntries);` |
|  2958524 | 10489 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2958524 | 10490 | `	pVm->nRefUsed--;` |
|  2958524 | 10491 | `	return SXRET_OK;` |
|        2 | 10492 |  |
|        - | 10493 | `/*` |
|        - | 10494 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10495 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10496 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10497 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10498 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10499 | ` * Refer to the official for more information on this powerful` |
|        - | 10500 | ` * extension.` |
|        - | 10501 | ` */` |
|  3017130 | 10502 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 10503 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10504 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10505 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10506 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 10507 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 10508 | `	)` |
|        2 | 10509 |  |
|  3017132 | 10510 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10511 | `	VmRefObj *pRef;` |
|        - | 10512 | `	/* Check if the referenced object already exists */` |
|  3017132 | 10513 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3017132 | 10514 | `	if( pRef == 0 ){` |
|        - | 10515 | `		/* Create a new entry */` |
|  2989494 | 10516 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  2989494 | 10517 | `		if( pRef == 0 ){` |
|      ! 0 | 10518 | `			return SXERR_MEM;` |
|        - | 10519 | `		}` |
|  2989494 | 10520 | `		pRef->iFlags = iFlags;` |
|        - | 10521 | `		/* Install the entry */` |
|  2989494 | 10522 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1494746 | 10523 | `	}` |
|  3017208 | 10524 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 10525 | `		/* Safely ignore the exception frame */` |
|       78 | 10526 | `		pFrame = pFrame->pParent;` |
|        2 | 10527 | `	}` |
|  3017132 | 10528 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 10529 | `		VmSlot sRef;` |
|        - | 10530 | `		/* Local frame,record referenced entry so that it can` |
|        - | 10531 | `		 * be deleted when we leave this frame.` |
|        - | 10532 | `		 */` |
|    74424 | 10533 | `		sRef.nIdx = nIdx;` |
|    74424 | 10534 | `		sRef.pUserData = pEntry;` |
|    74424 | 10535 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 10536 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 10537 | `		}` |
|    37211 | 10538 | `	}` |
|  3017132 | 10539 | `	if( pEntry ){` |
|        - | 10540 | `		/* Address of the hash-entry */` |
|   101872 | 10541 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    50935 | 10542 | `	}` |
|  3017132 | 10543 | `	if( pMapEntry ){` |
|        - | 10544 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2910482 | 10545 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1455240 | 10546 | `	}` |
|  3017132 | 10547 | `	return SXRET_OK;` |
|  1508567 | 10548 |  |
|        - | 10549 | `/*` |
|        - | 10550 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 10551 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10552 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10553 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10554 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10555 | ` * Refer to the official for more information on this powerful` |
|        - | 10556 | ` * extension.` |
|        - | 10557 | ` */` |
|  2949862 | 10558 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 10559 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10560 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10561 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10562 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 10563 | `	)` |
|        2 | 10564 |  |
|        - | 10565 | `	VmRefObj *pRef;` |
|        - | 10566 | `	sxu32 n;` |
|        - | 10567 | `	/* Check if the referenced object already exists */` |
|  2949864 | 10568 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2949864 | 10569 | `	if( pRef == 0 ){` |
|        - | 10570 | `		/* Not such entry */` |
|    74370 | 10571 | `		return SXERR_NOTFOUND;` |
|        - | 10572 | `	}` |
|        - | 10573 | `	/* Remove the desired entry */` |
|  2875496 | 10574 | `	if( pEntry ){` |
|        - | 10575 | `		SyHashEntry **apEntry;` |
|       56 | 10576 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 10577 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 10578 | `			if( apEntry[n] == pEntry ){` |
|        - | 10579 | `				/* Nullify the entry */` |
|       56 | 10580 | `				apEntry[n] = 0;` |
|        - | 10581 | `				/*` |
|        - | 10582 | `				 * NOTE:` |
|        - | 10583 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 10584 | `				 * we avoid wasting spaces.` |
|        - | 10585 | `				 */` |
|       27 | 10586 | `			}` |
|       79 | 10587 | `		}` |
|       27 | 10588 | `	}` |
|  2875496 | 10589 | `	if( pMapEntry ){` |
|        - | 10590 | `		ph7_hashmap_node **apNode;` |
|  2875442 | 10591 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5750970 | 10592 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2875530 | 10593 | `			if( apNode[n] == pMapEntry ){` |
|        - | 10594 | `				/* nullify the entry */` |
|  2875442 | 10595 | `				apNode[n] = 0;` |
|  1437720 | 10596 | `			}` |
|  1437766 | 10597 | `		}` |
|  1437720 | 10598 | `	}` |
|  2875496 | 10599 | `	return SXRET_OK;` |
|  1474933 | 10600 |  |
|        - | 10601 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 10602 | `/*` |
|        - | 10603 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 10604 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 10605 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 10606 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 10607 | ` * For more information on how to register IO stream devices,please` |
|        - | 10608 | ` * refer to the official documentation.` |
|        - | 10609 | ` */` |
|    22882 | 10610 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 10611 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 10612 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 10613 | `	int nByte              /* *pzDevice length*/` |
|        - | 10614 | `	)` |
|        2 | 10615 |  |
|        - | 10616 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 10617 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 10618 | `	SyString sDev,sCur;` |
|        - | 10619 | `	sxu32 n,nEntry;` |
|        - | 10620 | `	int rc;` |
|        - | 10621 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    22884 | 10622 | `	zNext = zCur = zIn = *pzDevice;` |
|    22884 | 10623 | `	zEnd = &zIn[nByte];` |
|  1464569 | 10624 | `	while( zIn < zEnd ){` |
|  1441689 | 10625 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 10626 | `			/* Got one */` |
|        3 | 10627 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 10628 | `			break;` |
|        - | 10629 | `		}` |
|        - | 10630 | `		/* Advance the cursor */` |
|  1441687 | 10631 | `		zIn++;` |
|        2 | 10632 | `	}` |
|    22884 | 10633 | `	if( zIn >= zEnd ){` |
|        - | 10634 | `		/* No such scheme,return the default stream */` |
|    22882 | 10635 | `		return pVm->pDefStream;` |
|        - | 10636 | `	}` |
|        3 | 10637 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 10638 | `	/* Remove leading and trailing white spaces */` |
|        3 | 10639 | `	SyStringFullTrim(&sDev);` |
|        - | 10640 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 10641 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 10642 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 10643 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 10644 | `		pStream = apStream[n];` |
|        3 | 10645 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 10646 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 10647 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 10648 | `		if( rc == 0 ){` |
|        - | 10649 | `			/* Stream device found */` |
|        3 | 10650 | `			*pzDevice = zNext;` |
|        3 | 10651 | `			return pStream;` |
|        - | 10652 | `		}` |
|      ! 0 | 10653 | `	}` |
|        - | 10654 | `	/* No such stream,return NULL */` |
|      ! 0 | 10655 | `	return 0;` |
|    11443 | 10656 |  |
|        - | 10657 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 10658 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 10659 |  |
