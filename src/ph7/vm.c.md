# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4228/5450 lines (77.58%)

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
|   775858 |    86 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    87 |  |
|   775860 |    88 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       32 |    89 | `		return TRUE;` |
|        - |    90 | `	}` |
|   775830 |    91 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |    92 | `		return TRUE;` |
|        - |    93 | `	}` |
|   775822 |    94 | `	return FALSE;` |
|   387953 |    95 |  |
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
|   480316 |   110 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   480318 |   121 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   480318 |   122 | `	if( pEntry ){` |
|        - |   123 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   124 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   125 | `		pCons->xExpand = xExpand;` |
|        6 |   126 | `		pCons->pUserData = pUserData;` |
|        6 |   127 | `		return SXRET_OK;` |
|        - |   128 | `	}` |
|        - |   129 | `	/* Allocate a new constant instance */` |
|   480314 |   130 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   480314 |   131 | `	if( pCons == 0 ){` |
|      ! 0 |   132 | `		return 0;` |
|        - |   133 | `	}` |
|        - |   134 | `	/* Duplicate constant name */` |
|   480314 |   135 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   480314 |   136 | `	if( zDupName == 0 ){` |
|      ! 0 |   137 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   138 | `		return 0;` |
|        - |   139 | `	}` |
|        - |   140 | `	/* Install the constant */` |
|   480314 |   141 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   480314 |   142 | `	pCons->xExpand = xExpand;` |
|   480314 |   143 | `	pCons->pUserData = pUserData;` |
|   480314 |   144 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   480314 |   145 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   146 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return rc;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* All done,constant can be invoked from PHP code */` |
|   480314 |   151 | `	return SXRET_OK;` |
|   240160 |   152 |  |
|        - |   153 | `/*` |
|        - |   154 | ` * Allocate a new foreign function instance.` |
|        - |   155 | ` * This function return SXRET_OK on success. Any other` |
|        - |   156 | ` * return value indicates failure.` |
|        - |   157 | ` * Please refer to the official documentation for an introduction to` |
|        - |   158 | ` * the foreign function mechanism.` |
|        - |   159 | ` */` |
|  1045772 |   160 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1045774 |   171 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1045774 |   172 | `	if( pFunc == 0 ){` |
|      ! 0 |   173 | `		return SXERR_MEM;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* Duplicate function name */` |
|  1045774 |   176 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1045774 |   177 | `	if( zDup == 0 ){` |
|      ! 0 |   178 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   179 | `		return SXERR_MEM;` |
|        - |   180 | `	}` |
|        - |   181 | `	/* Zero the structure */` |
|  1045774 |   182 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   183 | `	/* Initialize structure fields */` |
|  1045774 |   184 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1045774 |   185 | `	pFunc->pVm   = pVm;` |
|  1045774 |   186 | `	pFunc->xFunc = xFunc;` |
|  1045774 |   187 | `	pFunc->pUserData = pUserData;` |
|  1045774 |   188 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   189 | `	/* Write a pointer to the new function */` |
|  1045774 |   190 | `	*ppOut = pFunc;` |
|  1045774 |   191 | `	return SXRET_OK;` |
|   522888 |   192 |  |
|        - |   193 | `/*` |
|        - |   194 | ` * Install a foreign function and it's associated callback so that` |
|        - |   195 | ` * it can be invoked from the target PHP code.` |
|        - |   196 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   197 | ` * return value indicates failure.` |
|        - |   198 | ` * Please refer to the official documentation for an introduction to` |
|        - |   199 | ` * the foreign function mechanism.` |
|        - |   200 | ` */` |
|  1048138 |   201 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1048140 |   212 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1048140 |   213 | `	if( pEntry ){` |
|     2368 |   214 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2368 |   215 | `		pFunc->pUserData = pUserData;` |
|     2368 |   216 | `		pFunc->xFunc = xFunc;` |
|     2368 |   217 | `		SySetReset(&pFunc->aAux);` |
|     2368 |   218 | `		return SXRET_OK;` |
|        - |   219 | `	}` |
|        - |   220 | `	/* Create a new user function */` |
|  1045774 |   221 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1045774 |   222 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   223 | `		return rc;` |
|        - |   224 | `	}` |
|        - |   225 | `	/* Install the function in the corresponding hashtable */` |
|  1045774 |   226 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1045774 |   227 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   228 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   229 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   230 | `		return rc;` |
|        - |   231 | `	}` |
|        - |   232 | `	/* User function successfully installed */` |
|  1045774 |   233 | `	return SXRET_OK;` |
|   524071 |   234 |  |
|        - |   235 | `/*` |
|        - |   236 | ` * Initialize a VM function.` |
|        - |   237 | ` */` |
|   111318 |   238 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   239 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   240 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   241 | `	const char *zName,  /* Function name */` |
|        - |   242 | `	sxu32 nByte,        /* zName length */` |
|        - |   243 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   244 | `	void *pUserData     /* Function private data */` |
|        - |   245 | `	)` |
|        2 |   246 |  |
|        - |   247 | `	/* Zero the structure */` |
|   111320 |   248 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   249 | `	/* Initialize structure fields */` |
|        - |   250 | `	/* Arguments container */` |
|   111320 |   251 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   252 | `	/* Static variable container */` |
|   111320 |   253 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   254 | `	/* Bytecode container */` |
|   111320 |   255 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   256 | `    /* Preallocate some instruction slots */` |
|   111320 |   257 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   258 | `	/* Closure environment */` |
|   111320 |   259 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   111320 |   260 | `	pFunc->iFlags = iFlags;` |
|   111320 |   261 | `	pFunc->pUserData = pUserData;` |
|   111320 |   262 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   111320 |   263 | `	return SXRET_OK;` |
|        2 |   264 |  |
|        - |   265 | `/*` |
|        - |   266 | ` * Namespace-aware function lookup.` |
|        - |   267 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   268 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   269 | ` */` |
|        - |   270 | `/*` |
|        - |   271 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   272 | ` */` |
|   404650 |   273 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   274 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   275 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   276 | `	SyString *pName     /* Function name */` |
|        - |   277 | `	)` |
|        2 |   278 |  |
|        - |   279 | `	SyHashEntry *pEntry;` |
|        - |   280 | `	sxi32 rc;` |
|   404652 |   281 | `	if( pName == 0 ){` |
|        - |   282 | `		/* Use the built-in name */` |
|    34674 |   283 | `		pName = &pFunc->sName;` |
|    17336 |   284 | `	}` |
|        - |   285 | `	/* Check for duplicates (functions with the same name) first */` |
|   404652 |   286 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   404652 |   287 | `	if( pEntry ){` |
|   314618 |   288 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   314618 |   289 | `		if( pLink != pFunc ){` |
|        - |   290 | `			/* Link */` |
|      184 |   291 | `			pFunc->pNextName = pLink;` |
|      184 |   292 | `			pEntry->pUserData = pFunc;` |
|       91 |   293 | `		}` |
|   314618 |   294 | `		return SXRET_OK;` |
|        - |   295 | `	}` |
|        - |   296 | `	/* First time seen */` |
|    90036 |   297 | `	pFunc->pNextName = 0;` |
|    90036 |   298 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    90036 |   299 | `	return rc;` |
|   202327 |   300 |  |
|        - |   301 | `/*` |
|        - |   302 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   303 | ` */` |
|    31994 |   304 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   305 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   306 | `	ph7_class *pClass /* Target Class */` |
|        - |   307 | `	)` |
|        2 |   308 |  |
|    31996 |   309 | `	SyString *pName = &pClass->sName;` |
|        - |   310 | `	SyHashEntry *pEntry;` |
|        - |   311 | `	sxi32 rc;` |
|        - |   312 | `	/* Check for duplicates */` |
|    31996 |   313 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    31996 |   314 | `	if( pEntry ){` |
|       31 |   315 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   316 | `		/* Link entry with the same name */` |
|       31 |   317 | `		pClass->pNextName = pLink;` |
|       31 |   318 | `		pEntry->pUserData = pClass;` |
|       31 |   319 | `		return SXRET_OK;` |
|        - |   320 | `	}` |
|    31966 |   321 | `	pClass->pNextName = 0;` |
|        - |   322 | `	/* Perform a simple hashtable insertion */` |
|    31966 |   323 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    31966 |   324 | `	return rc;` |
|    15999 |   325 |  |
|        - |   326 | `/*` |
|        - |   327 | ` * Instruction builder interface.` |
|        - |   328 | ` */` |
|  2964462 |   329 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  2964464 |   341 | `	sInstr.iOp = (sxu8)iOp;` |
|  2964464 |   342 | `	sInstr.iP1 = iP1;` |
|  2964464 |   343 | `	sInstr.iP2 = iP2;` |
|  2964464 |   344 | `	sInstr.p3  = p3;` |
|  2964464 |   345 | `	if( pIndex ){` |
|        - |   346 | `		/* Instruction index in the bytecode array */` |
|   187486 |   347 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    93742 |   348 | `	}` |
|        - |   349 | `	/* Finally,record the instruction */` |
|  2964464 |   350 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  2964464 |   351 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   352 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   353 | `		/* Fall throw */` |
|      ! 0 |   354 | `	}` |
|  2964464 |   355 | `	return rc;` |
|        2 |   356 |  |
|        - |   357 | `/*` |
|        - |   358 | ` * Swap the current bytecode container with the given one.` |
|        - |   359 | ` */` |
|   270516 |   360 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   361 |  |
|   270518 |   362 | `	if( pContainer == 0 ){` |
|        - |   363 | `		/* Point to the default container */` |
|      ! 0 |   364 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   365 | `	}else{` |
|        - |   366 | `		/* Change container */` |
|   270518 |   367 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   368 | `	}` |
|   270518 |   369 | `	return SXRET_OK;` |
|        2 |   370 |  |
|        - |   371 | `/*` |
|        - |   372 | ` * Return the current bytecode container.` |
|        - |   373 | ` */` |
|   135258 |   374 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   375 |  |
|   135260 |   376 | `	return pVm->pByteContainer;` |
|        2 |   377 |  |
|        - |   378 | `/*` |
|        - |   379 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   380 | ` */` |
|   184780 |   381 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   382 |  |
|        - |   383 | `	VmInstr *pInstr;` |
|   184782 |   384 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   184782 |   385 | `	return pInstr;` |
|        2 |   386 |  |
|        - |   387 | `/*` |
|        - |   388 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   389 | ` */` |
|   825110 |   390 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   391 |  |
|   825112 |   392 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Pop the last VM instruction.` |
|        - |   396 | ` */` |
|   175734 |   397 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   398 |  |
|   175736 |   399 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   400 |  |
|        - |   401 | `/*` |
|        - |   402 | ` * Peek the last VM instruction.` |
|        - |   403 | ` */` |
|   576946 |   404 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   405 |  |
|   576948 |   406 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   407 |  |
|    26928 |   408 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   409 |  |
|        - |   410 | `	VmInstr *aInstr;` |
|        - |   411 | `	sxu32 n;` |
|    26930 |   412 | `	n = SySetUsed(pVm->pByteContainer);` |
|    26930 |   413 | `	if( n < 2 ){` |
|      ! 0 |   414 | `		return 0;` |
|        - |   415 | `	}` |
|    26930 |   416 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    26930 |   417 | `	return &aInstr[n - 2];` |
|    13466 |   418 |  |
|        - |   419 | `/*` |
|        - |   420 | ` * Allocate a new virtual machine frame.` |
|        - |   421 | ` */` |
|    15356 |   422 | `static VmFrame * VmNewFrame(` |
|        - |   423 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   424 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   425 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   426 | `	)` |
|        2 |   427 |  |
|        - |   428 | `	VmFrame *pFrame;` |
|        - |   429 | `	/* Allocate a new vm frame */` |
|    15358 |   430 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    15358 |   431 | `	if( pFrame == 0 ){` |
|      ! 0 |   432 | `		return 0;` |
|        - |   433 | `	}` |
|        - |   434 | `	/* Zero the structure */` |
|    15358 |   435 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   436 | `	/* Initialize frame fields */` |
|    15358 |   437 | `	pFrame->pUserData = pUserData;` |
|    15358 |   438 | `	pFrame->pThis = pThis;` |
|    15358 |   439 | `	pFrame->pVm = pVm;` |
|    15358 |   440 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    15358 |   441 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    15358 |   442 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    15358 |   443 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    15358 |   444 | `	return pFrame;` |
|     7680 |   445 |  |
|        - |   446 | `/* Forward declaration */` |
|        - |   447 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   448 | `/*` |
|        - |   449 | ` * Enter a VM frame.` |
|        - |   450 | ` */` |
|    15356 |   451 | `static sxi32 VmEnterFrame(` |
|        - |   452 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   453 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   454 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   455 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   456 | `	)` |
|        2 |   457 |  |
|        - |   458 | `	VmFrame *pFrame;` |
|        - |   459 | `	/* Allocate a new frame */` |
|    15358 |   460 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    15358 |   461 | `	if( pFrame == 0 ){` |
|      ! 0 |   462 | `		return SXERR_MEM;` |
|        - |   463 | `	}` |
|        - |   464 | `	/* Link to the list of active VM frame */` |
|    15358 |   465 | `	pFrame->pParent = pVm->pFrame;` |
|    15358 |   466 | `	pVm->pFrame = pFrame;` |
|    15358 |   467 | `	if( ppFrame ){` |
|        - |   468 | `		/* Write a pointer to the new VM frame */` |
|    12730 |   469 | `		*ppFrame = pFrame;` |
|     6364 |   470 | `	}` |
|    15358 |   471 | `	return SXRET_OK;` |
|     7680 |   472 |  |
|        - |   473 | `/*` |
|        - |   474 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   475 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   476 | ` * information.` |
|        - |   477 | ` */` |
|       52 |   478 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        2 |   479 |  |
|        - |   480 | `	VmFrame *pTarget,*pFrame;` |
|       54 |   481 | `	SyHashEntry *pEntry = 0;` |
|        - |   482 | `	sxi32 rc;` |
|        - |   483 | `	/* Point to the upper frame */` |
|       54 |   484 | `	pFrame = pVm->pFrame;` |
|       54 |   485 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       54 |   486 | `	pTarget = pFrame;` |
|       54 |   487 | `	pFrame = pTarget->pParent;` |
|       54 |   488 | `	while( pFrame ){` |
|       54 |   489 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   490 | `			/* Query the current frame */` |
|       54 |   491 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       54 |   492 | `			if( pEntry ){` |
|        - |   493 | `				/* Variable found */` |
|       54 |   494 | `				break;` |
|        - |   495 | `			}` |
|      ! 0 |   496 | `		}` |
|        - |   497 | `		/* Point to the upper frame */` |
|      ! 0 |   498 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   499 | `	}` |
|       54 |   500 | `	if( pEntry == 0 ){` |
|        - |   501 | `		/* Inexistant variable */` |
|      ! 0 |   502 | `		return SXERR_NOTFOUND;` |
|        - |   503 | `	}` |
|        - |   504 | `	/* Link to the current frame */` |
|       54 |   505 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       54 |   506 | `	if( rc == SXRET_OK ){` |
|        - |   507 | `		sxu32 nIdx;` |
|       54 |   508 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       54 |   509 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       26 |   510 | `	}` |
|       54 |   511 | `	return rc;` |
|       28 |   512 |  |
|        - |   513 | `/*` |
|        - |   514 | ` * Leave the top-most active frame.` |
|        - |   515 | ` */` |
|    12728 |   516 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   517 |  |
|    12730 |   518 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    12730 |   519 | `	if( pCurFrame ){` |
|        - |   520 | `		/* Unlink from the list of active VM frame */` |
|    12730 |   521 | `		pVm->pFrame = pCurFrame->pParent;` |
|    12730 |   522 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   523 | `			VmSlot  *aSlot;` |
|        - |   524 | `			sxu32 n;` |
|        - |   525 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    12672 |   526 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    89994 |   527 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   528 | `				/* Unset the local variable */` |
|    77324 |   529 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    38663 |   530 | `			}` |
|        - |   531 | `			/* Remove local reference */` |
|    12672 |   532 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    90050 |   533 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    77380 |   534 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    38691 |   535 | `			}` |
|     6335 |   536 | `		}` |
|        - |   537 | `		/* Release internal containers */` |
|    12730 |   538 | `		SyHashRelease(&pCurFrame->hVar);` |
|    12730 |   539 | `		SySetRelease(&pCurFrame->sArg);` |
|    12730 |   540 | `		SySetRelease(&pCurFrame->sLocal);` |
|    12730 |   541 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   542 | `		/* Release the whole structure */` |
|    12730 |   543 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6364 |   544 | `	}` |
|    12730 |   545 |  |
|        - |   546 | `/*` |
|        - |   547 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   548 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   549 | ` * should be skipped when looking for the real execution context.` |
|        - |   550 | ` */` |
|  6268692 |   551 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   552 |  |
|  6268964 |   553 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      272 |   554 | `		pFrame = pFrame->pParent;` |
|        2 |   555 | `	}` |
|  6268694 |   556 | `	return pFrame;` |
|        2 |   557 |  |
|        - |   558 | `/*` |
|        - |   559 | ` * Compare two functions signature and return the comparison result.` |
|        - |   560 | ` */` |
|      818 |   561 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   562 |  |
|      819 |   563 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      819 |   564 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      819 |   565 | `	const char *zSin = pSecond->zString;` |
|      819 |   566 | `	const char *zFin = pFirst->zString;` |
|      819 |   567 | `	const char *zPtr = zFin;` |
|      409 |   568 | `	for(;;){` |
|      819 |   569 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      410 |   570 | `			break;` |
|        - |   571 | `		}` |
|      ! 0 |   572 | `		if( zFin[0] != zSin[0] ){` |
|        - |   573 | `			/* mismatch */` |
|      ! 0 |   574 | `			break;` |
|        - |   575 | `		}` |
|      ! 0 |   576 | `		zFin++;` |
|      ! 0 |   577 | `		zSin++;` |
|      ! 0 |   578 | `	}` |
|      819 |   579 | `	return (int)(zFin-zPtr);` |
|        1 |   580 |  |
|        - |   581 | `/*` |
|        - |   582 | ` * Select the appropriate VM function for the current call context.` |
|        - |   583 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   584 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   585 | ` * Refer to the official documentation for more information.` |
|        - |   586 | ` */` |
|      132 |   587 | `static ph7_vm_func * VmOverload(` |
|        - |   588 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   589 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   590 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   591 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   592 | `	)` |
|        2 |   593 |  |
|        - |   594 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   595 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   596 | `	ph7_vm_func *pLink;` |
|        - |   597 | `	SyString sArgSig;` |
|        - |   598 | `	SyBlob sSig;` |
|        - |   599 |  |
|      134 |   600 | `	pLink = pList;` |
|      134 |   601 | `	i = 0;` |
|        - |   602 | `	/* Put functions expecting the same number of passed arguments */` |
|     1062 |   603 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1000 |   604 | `		if( pLink == 0 ){` |
|       72 |   605 | `			break;` |
|        - |   606 | `		}` |
|      930 |   607 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   608 | `			/* Candidate for overloading */` |
|      884 |   609 | `			apSet[i++] = pLink;` |
|      441 |   610 | `		}` |
|        - |   611 | `		/* Point to the next entry */` |
|      930 |   612 | `		pLink = pLink->pNextName;` |
|        2 |   613 | `	}` |
|      134 |   614 | `	if( i < 1 ){` |
|        - |   615 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   616 | `		return pList;` |
|        - |   617 | `	}` |
|      134 |   618 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   619 | `		/* Return the only candidate */` |
|       32 |   620 | `		return apSet[0];` |
|        - |   621 | `	}` |
|        - |   622 | `	/* Calculate function signature */` |
|      103 |   623 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      355 |   624 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      253 |   625 | `		int c = 'n'; /* null */` |
|      253 |   626 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   627 | `			/* Hashmap */` |
|       45 |   628 | `			c = 'h';` |
|      231 |   629 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   630 | `			/* bool */` |
|      ! 0 |   631 | `			c = 'b';` |
|      209 |   632 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   633 | `			/* int */` |
|        5 |   634 | `			c = 'i';` |
|      207 |   635 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   636 | `			/* String */` |
|      105 |   637 | `			c = 's';` |
|      153 |   638 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   639 | `			/* Float */` |
|      ! 0 |   640 | `			c = 'f';` |
|      101 |   641 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   642 | `			/* Class instance */` |
|      ! 0 |   643 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|      ! 0 |   644 | `			SyString *pName = &pClass->sName;` |
|      ! 0 |   645 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|      ! 0 |   646 | `			c = -1;` |
|      ! 0 |   647 | `		}` |
|      253 |   648 | `		if( c > 0 ){` |
|      253 |   649 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      126 |   650 | `		}` |
|      127 |   651 | `	}` |
|      103 |   652 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      103 |   653 | `	iTarget = 0;` |
|      103 |   654 | `	iMax = -1;` |
|        - |   655 | `	/* Select the appropriate function */` |
|      921 |   656 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   657 | `		/* Compare the two signatures */` |
|      819 |   658 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      819 |   659 | `		if( iCur > iMax ){` |
|      103 |   660 | `			iMax = iCur;` |
|      103 |   661 | `			iTarget = j;` |
|       51 |   662 | `		}` |
|      410 |   663 | `	}` |
|      103 |   664 | `	SyBlobRelease(&sSig);` |
|        - |   665 | `	/* Appropriate function for the current call context */` |
|      103 |   666 | `	return apSet[iTarget];` |
|       68 |   667 |  |
|        - |   668 | `/* Forward declaration */` |
|        - |   669 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   670 | `/*` |
|        - |   671 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   672 | ` * it can be instanciated from the executed PHP script.` |
|        - |   673 | ` */` |
|    92698 |   674 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   675 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   676 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   677 | `	)` |
|        2 |   678 |  |
|        - |   679 | `	ph7_class_method *pMeth;` |
|        - |   680 | `	ph7_class_attr *pAttr;` |
|        - |   681 | `	SyHashEntry *pEntry;` |
|        - |   682 | `	sxi32 rc;` |
|        - |   683 | `	/* Reset the loop cursor */` |
|    92700 |   684 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   685 | `	/* Process only static and constant attribute */` |
|   370289 |   686 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   687 | `		/* Extract the current attribute */` |
|   231242 |   688 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   231242 |   689 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   690 | `			ph7_value *pMemObj;` |
|        - |   691 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1294 |   692 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1294 |   693 | `			if( pMemObj == 0 ){` |
|      ! 0 |   694 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   695 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   696 | `					&pClass->sName,&pAttr->sName` |
|        - |   697 | `					);` |
|      ! 0 |   698 | `				return SXERR_MEM;` |
|        - |   699 | `			}` |
|     1294 |   700 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   701 | `				/* Initialize attribute default value (any complex expression) */` |
|     1294 |   702 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      646 |   703 | `			}` |
|        - |   704 | `			/* Record attribute index */` |
|     1294 |   705 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   706 | `			/* Install static attribute in the reference table */` |
|     1294 |   707 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      646 |   708 | `		}` |
|        2 |   709 | `	}` |
|        - |   710 | `	/* Install class methods */` |
|    92700 |   711 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   712 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   713 | `		 */` |
|    47428 |   714 | `		return SXRET_OK;` |
|        - |   715 | `	}` |
|        - |   716 | `	/* Create constructor alias if not yet done */` |
|    45274 |   717 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   718 | `		/* User constructor with the same base class name */` |
|      286 |   719 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      286 |   720 | `		if( pEntry ){` |
|      ! 0 |   721 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   722 | `			/* Create the alias */` |
|      ! 0 |   723 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   724 | `		}` |
|      142 |   725 | `	}` |
|        - |   726 | `	/* Install the methods now */` |
|    45274 |   727 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   437894 |   728 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   369986 |   729 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   369986 |   730 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   369980 |   731 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   369980 |   732 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   733 | `				return rc;` |
|        - |   734 | `			}` |
|   184989 |   735 | `		}` |
|        2 |   736 | `	}` |
|        - |   737 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    45274 |   738 | `	pClass->bMounted = TRUE;` |
|    45274 |   739 | `	return SXRET_OK;` |
|    46351 |   740 |  |
|        - |   741 | `/*` |
|        - |   742 | ` * Allocate a private frame for attributes of the given` |
|        - |   743 | ` * class instance (Object in the PHP jargon).` |
|        - |   744 | ` */` |
|     1136 |   745 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   746 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   747 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   748 | `	)` |
|        2 |   749 |  |
|     1138 |   750 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   751 | `	ph7_class_attr *pAttr;` |
|        - |   752 | `	SyHashEntry *pEntry;` |
|        - |   753 | `	sxi32 rc;` |
|        - |   754 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1138 |   755 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4724 |   756 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   757 | `		VmClassAttr *pVmAttr;` |
|        - |   758 | `		/* Extract the current attribute */` |
|     3588 |   759 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3588 |   760 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3588 |   761 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   762 | `			return SXERR_MEM;` |
|        - |   763 | `		}` |
|     3588 |   764 | `		pVmAttr->pAttr = pAttr;` |
|     3588 |   765 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   766 | `			ph7_value *pMemObj;` |
|        - |   767 | `			/* Reserve a memory object for this attribute */` |
|     3582 |   768 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3582 |   769 | `			if( pMemObj == 0 ){` |
|      ! 0 |   770 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   771 | `				return SXERR_MEM;` |
|        - |   772 | `			}` |
|     3582 |   773 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3582 |   774 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   775 | `				/* Initialize attribute default value (any complex expression) */` |
|     1176 |   776 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      587 |   777 | `			}` |
|     3582 |   778 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3582 |   779 | `			if( rc != SXRET_OK ){` |
|        - |   780 | `				VmSlot sSlot;` |
|        - |   781 | `				/* Restore memory object */` |
|      ! 0 |   782 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   783 | `				sSlot.pUserData = 0;` |
|      ! 0 |   784 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   785 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   786 | `				return SXERR_MEM;` |
|        - |   787 | `			}` |
|        - |   788 | `			/* Install attribute in the reference table */` |
|     3582 |   789 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1792 |   790 | `		}else{` |
|        - |   791 | `			/* Install static/constant attribute */` |
|        8 |   792 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   793 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   794 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   795 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   796 | `				return SXERR_MEM;` |
|        - |   797 | `			}` |
|        - |   798 | `		}` |
|        2 |   799 | `	}` |
|     1138 |   800 | `	return SXRET_OK;` |
|      570 |   801 |  |
|        - |   802 | `/* Forward declaration */` |
|        - |   803 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   804 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   805 | `/*` |
|        - |   806 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   807 | ` */` |
|        - |   808 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   809 | `/*` |
|        - |   810 | ` * Reserve a constant memory object.` |
|        - |   811 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   812 | ` */` |
|   320798 |   813 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   814 |  |
|        - |   815 | `	ph7_value *pObj;` |
|        - |   816 | `	sxi32 rc;` |
|   320800 |   817 | `	if( pIndex ){` |
|        - |   818 | `		/* Object index in the object table */` |
|   312916 |   819 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   156457 |   820 | `	}` |
|        - |   821 | `	/* Reserve a slot for the new object */` |
|   320800 |   822 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   320800 |   823 | `	if( rc != SXRET_OK ){` |
|        - |   824 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   825 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   826 | `		 */` |
|      ! 0 |   827 | `		return 0;` |
|        - |   828 | `	}` |
|   320800 |   829 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   320800 |   830 | `	return pObj;` |
|   160401 |   831 |  |
|        - |   832 | `/*` |
|        - |   833 | ` * Reserve a memory object.` |
|        - |   834 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   835 | ` */` |
|  2142660 |   836 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   837 |  |
|        - |   838 | `	ph7_value *pObj;` |
|        - |   839 | `	sxi32 rc;` |
|  2142662 |   840 | `	if( pIndex ){` |
|        - |   841 | `		/* Object index in the object table */` |
|  2142662 |   842 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1071330 |   843 | `	}` |
|        - |   844 | `	/* Reserve a slot for the new object */` |
|  2142662 |   845 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2142662 |   846 | `	if( rc != SXRET_OK ){` |
|        - |   847 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   848 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   849 | `		 */` |
|      ! 0 |   850 | `		return 0;` |
|        - |   851 | `	}` |
|  2142662 |   852 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2142662 |   853 | `	return pObj;` |
|  1071332 |   854 |  |
|        - |   855 | `/* Forward declaration */` |
|        - |   856 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   857 | `/*` |
|        - |   858 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   859 | ` * directly as foreign functions.` |
|        - |   860 | ` */` |
|        - |   861 | `#define PH7_BUILTIN_LIB \` |
|        - |   862 | `	"class Exception { "\` |
|        - |   863 | `    "protected $message = 'Unknown exception';"\` |
|        - |   864 | `    "protected $code = 0;"\` |
|        - |   865 | `    "protected $file;"\` |
|        - |   866 | `    "protected $line;"\` |
|        - |   867 | `    "protected $trace;"\` |
|        - |   868 | `    "protected $previous;"\` |
|        - |   869 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   870 | `	"   if( isset($message) ){"\` |
|        - |   871 | `	"	  $this->message = $message;"\` |
|        - |   872 | `	"   }"\` |
|        - |   873 | `	"   $this->code = $code;"\` |
|        - |   874 | `	"   $this->file = __FILE__;"\` |
|        - |   875 | `	"   $this->line = __LINE__;"\` |
|        - |   876 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   877 | `	"   if( isset($previous) ){"\` |
|        - |   878 | `	"     $this->previous = $previous;"\` |
|        - |   879 | `	"   }"\` |
|        - |   880 | `	"}"\` |
|        - |   881 | `	"public function getMessage(){"\` |
|        - |   882 | `	"   return $this->message;"\` |
|        - |   883 | `	"}"\` |
|        - |   884 | `	" public function getCode(){"\` |
|        - |   885 | `	"  return $this->code;"\` |
|        - |   886 | `	"}"\` |
|        - |   887 | `	"public function getFile(){"\` |
|        - |   888 | `	"  return $this->file;"\` |
|        - |   889 | `	"}"\` |
|        - |   890 | `	"public function getLine(){"\` |
|        - |   891 | `	"  return $this->line;"\` |
|        - |   892 | `	"}"\` |
|        - |   893 | `	"public function getTrace(){"\` |
|        - |   894 | `	"   return $this->trace;"\` |
|        - |   895 | `	"}"\` |
|        - |   896 | `	"public function getTraceAsString(){"\` |
|        - |   897 | `	"  return debug_string_backtrace();"\` |
|        - |   898 | `	"}"\` |
|        - |   899 | `	"public function getPrevious(){"\` |
|        - |   900 | `	"    return $this->previous;"\` |
|        - |   901 | `	"}"\` |
|        - |   902 | `	"public function __toString(){"\` |
|        - |   903 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   904 | `    "}"\` |
|        - |   905 | `	"}"\` |
|        - |   906 | `	"class Error extends Exception { }"\` |
|        - |   907 | `	"class TypeError extends Error { }"\` |
|        - |   908 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   909 | `	"class ValueError extends Error { }"\` |
|        - |   910 | `	"class AssertionError extends Error { }"\` |
|        - |   911 | `	"class ErrorException extends Exception { "\` |
|        - |   912 | `	"protected $severity;"\` |
|        - |   913 | `	"public function __construct(string $message = null,"\` |
|        - |   914 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   915 | `	"   if( isset($message) ){"\` |
|        - |   916 | `	"	  $this->message = $message;"\` |
|        - |   917 | `	"   }"\` |
|        - |   918 | `	"   $this->severity = $severity;"\` |
|        - |   919 | `	"   $this->code = $code;"\` |
|        - |   920 | `	"   $this->file = $filename;"\` |
|        - |   921 | `	"   $this->line = $lineno;"\` |
|        - |   922 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   923 | `	"   if( isset($previous) ){"\` |
|        - |   924 | `	"     $this->previous = $previous;"\` |
|        - |   925 | `	"   }"\` |
|        - |   926 | `	"}"\` |
|        - |   927 | `	"public function getSeverity(){"\` |
|        - |   928 | `	"   return $this->severity;"\` |
|        - |   929 | `    "}"\` |
|        - |   930 | `	"}"\` |
|        - |   931 | `	"interface Iterator {"\` |
|        - |   932 | `	"public function current();"\` |
|        - |   933 | `	"public function key();"\` |
|        - |   934 | `	"public function next();"\` |
|        - |   935 | `	"public function rewind();"\` |
|        - |   936 | `	"public function valid();"\` |
|        - |   937 | `	"}"\` |
|        - |   938 | `	"interface IteratorAggregate {"\` |
|        - |   939 | `	"public function getIterator();"\` |
|        - |   940 | `	"}"\` |
|        - |   941 | `	"interface Serializable {"\` |
|        - |   942 | `	"public function serialize();"\` |
|        - |   943 | `	"public function unserialize(string $serialized);"\` |
|        - |   944 | `	"}"\` |
|        - |   945 | `	"/* Directory releated IO */"\` |
|        - |   946 | `	"class Directory {"\` |
|        - |   947 | `	"public $handle = null;"\` |
|        - |   948 | `	"public $path  = null;"\` |
|        - |   949 | `	"public function __construct(string $path)"\` |
|        - |   950 | `	"{"\` |
|        - |   951 | `	"   $this->handle = opendir($path);"\` |
|        - |   952 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   953 | `	"      $this->path = $path;"\` |
|        - |   954 | `	"   }"\` |
|        - |   955 | `	"}"\` |
|        - |   956 | `	"public function __destruct()"\` |
|        - |   957 | `	"{"\` |
|        - |   958 | `	"  if( $this->handle != null ){"\` |
|        - |   959 | `	"       closedir($this->handle);"\` |
|        - |   960 | `	"  }"\` |
|        - |   961 | `	"}"\` |
|        - |   962 | `	"public function read()"\` |
|        - |   963 | `	"{"\` |
|        - |   964 | `	"    return readdir($this->handle);"\` |
|        - |   965 | `	"}"\` |
|        - |   966 | `	"public function rewind()"\` |
|        - |   967 | `	"{"\` |
|        - |   968 | `	"    rewinddir($this->handle);"\` |
|        - |   969 | `	"}"\` |
|        - |   970 | `	"public function close()"\` |
|        - |   971 | `	"{"\` |
|        - |   972 | `	"    closedir($this->handle);"\` |
|        - |   973 | `	"    $this->handle = null;"\` |
|        - |   974 | `	"}"\` |
|        - |   975 | `	"}"\` |
|        - |   976 | `	"class stdClass{"\` |
|        - |   977 | `	"  public $value;"\` |
|        - |   978 | `	" /* Magic methods */"\` |
|        - |   979 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |   980 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |   981 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |   982 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |   983 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |   984 | `	"}"\` |
|        - |   985 | `	"function dir(string $path){"\` |
|        - |   986 | `	"   return new Directory($path);"\` |
|        - |   987 | `	"}"\` |
|        - |   988 | `	"function Dir(string $path){"\` |
|        - |   989 | `	"   return new Directory($path);"\` |
|        - |   990 | `	"}"\` |
|        - |   991 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |   992 | `    "{"\` |
|        - |   993 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |   994 | `	"  $aDir = array();"\` |
|        - |   995 | `	"  $pHandle = opendir($directory);"\` |
|        - |   996 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |   997 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |   998 | `	"      $aDir[] = $pEntry;"\` |
|        - |   999 | `	"   }"\` |
|        - |  1000 | `	"  closedir($pHandle);"\` |
|        - |  1001 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1002 | `	"      rsort($aDir);"\` |
|        - |  1003 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1004 | `	"      sort($aDir);"\` |
|        - |  1005 | `	"  }"\` |
|        - |  1006 | `	"  return $aDir;"\` |
|        - |  1007 | `	"}"\` |
|        - |  1008 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1009 | `	"/* Open the target directory */"\` |
|        - |  1010 | `	"$zDir = dirname($pattern);"\` |
|        - |  1011 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1012 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1013 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1014 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1015 | `	"	return FALSE;"\` |
|        - |  1016 | `	"}"\` |
|        - |  1017 | `	"$pattern = basename($pattern);"\` |
|        - |  1018 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1019 | `	"/* Loop throw available entries */"\` |
|        - |  1020 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1021 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1022 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1023 | `	"	if( $rc ){"\` |
|        - |  1024 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1025 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1026 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1027 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1028 | `	"		  }"\` |
|        - |  1029 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1030 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1031 | `	"		 continue;"\` |
|        - |  1032 | `	"	   }"\` |
|        - |  1033 | `	"	   /* Add the entry */"\` |
|        - |  1034 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1035 | `	"	}"\` |
|        - |  1036 | `	" }"\` |
|        - |  1037 | `	"/* Close the handle */"\` |
|        - |  1038 | `	"closedir($pHandle);"\` |
|        - |  1039 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1040 | `	"  /* Sort the array */"\` |
|        - |  1041 | `	"  sort($pArray);"\` |
|        - |  1042 | `	"}"\` |
|        - |  1043 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1044 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1045 | `	"  $pArray[] = $pattern;"\` |
|        - |  1046 | `	"}"\` |
|        - |  1047 | `	"/* Return the created array */"\` |
|        - |  1048 | `	"return $pArray;"\` |
|        - |  1049 | `   "}"\` |
|        - |  1050 | `   "/* Creates a temporary file */"\` |
|        - |  1051 | `   "function tmpfile(){"\` |
|        - |  1052 | `   "  /* Extract the temp directory */"\` |
|        - |  1053 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1054 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1055 | `   "    /* Use the current dir */"\` |
|        - |  1056 | `   "    $zTempDir = '.';"\` |
|        - |  1057 | `   "  }"\` |
|        - |  1058 | `   "  /* Create the file */"\` |
|        - |  1059 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1060 | `   "  return $pHandle;"\` |
|        - |  1061 | `   "}"\` |
|        - |  1062 | `   "/* Creates a temporary filename */"\` |
|        - |  1063 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1064 | `   "{"\` |
|        - |  1065 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1066 | `   "}"\` |
|        - |  1067 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1068 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1069 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1070 | `   "/* Copy arguments */"\` |
|        - |  1071 | `   "$nArgs = func_num_args();"\` |
|        - |  1072 | `   "$pNew = array();"\` |
|        - |  1073 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1074 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1075 | `    "}"\` |
|        - |  1076 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1077 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1078 | `	"/* Erase */"\` |
|        - |  1079 | `	"array_erase($pArray);"\` |
|        - |  1080 | `	"/* Unshift */"\` |
|        - |  1081 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1082 | `	"return sizeof($pArray);"\` |
|        - |  1083 | `    "}"\` |
|        - |  1084 | `	"function array_merge_recursive(){"\` |
|        - |  1085 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1086 | `    "$arrays = func_get_args();"\` |
|        - |  1087 | `    "$narrays = count($arrays);"\` |
|        - |  1088 | `    "$ret = array();"\` |
|        - |  1089 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1090 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1091 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1092 | `	 " }"\` |
|        - |  1093 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1094 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1095 | `     "  if( $keyIsInt ) {"\` |
|        - |  1096 | `     "   $ret[] = $value;"\` |
|        - |  1097 | `     "  } else {"\` |
|        - |  1098 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1099 | `     "    $cur = $ret[$key];"\` |
|        - |  1100 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1101 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1102 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1103 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1104 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1105 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1106 | `     "    } else {"\` |
|        - |  1107 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1108 | `     "    }"\` |
|        - |  1109 | `     "   } else {"\` |
|        - |  1110 | `     "    $ret[$key] = $value;"\` |
|        - |  1111 | `     "   }"\` |
|        - |  1112 | `     "  }"\` |
|        - |  1113 | `     " }"\` |
|        - |  1114 | `	 " }"\` |
|        - |  1115 | `	 " return $ret;"\` |
|        - |  1116 | `    "}"\` |
|        - |  1117 | `	"function max(){"\` |
|        - |  1118 | `    "  $pArgs = func_get_args();"\` |
|        - |  1119 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1120 | `	"  return null;"\` |
|        - |  1121 | `    " }"\` |
|        - |  1122 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1123 | `    " $pArg = $pArgs[0];"\` |
|        - |  1124 | `	" if( !is_array($pArg) ){"\` |
|        - |  1125 | `	"   return $pArg; "\` |
|        - |  1126 | `	" }"\` |
|        - |  1127 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1128 | `	"   return null;"\` |
|        - |  1129 | `	" }"\` |
|        - |  1130 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1131 | `	" reset($pArg);"\` |
|        - |  1132 | `	" $max = current($pArg);"\` |
|        - |  1133 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1134 | `	"   if( $val > $max ){"\` |
|        - |  1135 | `	"     $max = $val;"\` |
|        - |  1136 | `    " }"\` |
|        - |  1137 | `	" }"\` |
|        - |  1138 | `	" return $max;"\` |
|        - |  1139 | `    " }"\` |
|        - |  1140 | `    " $max = $pArgs[0];"\` |
|        - |  1141 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1142 | `    " $val = $pArgs[$i];"\` |
|        - |  1143 | `	"if( $val > $max ){"\` |
|        - |  1144 | `	" $max = $val;"\` |
|        - |  1145 | `	"}"\` |
|        - |  1146 | `    " }"\` |
|        - |  1147 | `	" return $max;"\` |
|        - |  1148 | `    "}"\` |
|        - |  1149 | `	"function min(){"\` |
|        - |  1150 | `    "  $pArgs = func_get_args();"\` |
|        - |  1151 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1152 | `	"  return null;"\` |
|        - |  1153 | `    " }"\` |
|        - |  1154 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1155 | `    " $pArg = $pArgs[0];"\` |
|        - |  1156 | `	" if( !is_array($pArg) ){"\` |
|        - |  1157 | `	"   return $pArg; "\` |
|        - |  1158 | `	" }"\` |
|        - |  1159 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1160 | `	"   return null;"\` |
|        - |  1161 | `	" }"\` |
|        - |  1162 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1163 | `	" reset($pArg);"\` |
|        - |  1164 | `	" $min = current($pArg);"\` |
|        - |  1165 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1166 | `	"   if( $val < $min ){"\` |
|        - |  1167 | `	"     $min = $val;"\` |
|        - |  1168 | `    " }"\` |
|        - |  1169 | `	" }"\` |
|        - |  1170 | `	" return $min;"\` |
|        - |  1171 | `    " }"\` |
|        - |  1172 | `    " $min = $pArgs[0];"\` |
|        - |  1173 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1174 | `    " $val = $pArgs[$i];"\` |
|        - |  1175 | `	"if( $val < $min ){"\` |
|        - |  1176 | `	" $min = $val;"\` |
|        - |  1177 | `	" }"\` |
|        - |  1178 | `    " }"\` |
|        - |  1179 | `	" return $min;"\` |
|        - |  1180 | `	"}"\` |
|        - |  1181 | `	"function fileowner(string $file){"\` |
|        - |  1182 | `    " $a = stat($file);"\` |
|        - |  1183 | `	" if( !is_array($a) ){"\` |
|        - |  1184 | `	"	return false;"\` |
|        - |  1185 | `	" }"\` |
|        - |  1186 | `	" return $a['uid'];"\` |
|        - |  1187 | `    "}"\` |
|        - |  1188 | `    "function filegroup(string $file){"\` |
|        - |  1189 | `	" $a = stat($file);"\` |
|        - |  1190 | `	" if( !is_array($a) ){"\` |
|        - |  1191 | `	"	return false;"\` |
|        - |  1192 | `	" }"\` |
|        - |  1193 | `	" return $a['gid'];"\` |
|        - |  1194 | `    "}"\` |
|        - |  1195 | `	 "function fileinode(string $file){"\` |
|        - |  1196 | `	" $a = stat($file);"\` |
|        - |  1197 | `	" if( !is_array($a) ){"\` |
|        - |  1198 | `	"	return false;"\` |
|        - |  1199 | `	" }"\` |
|        - |  1200 | `	" return $a['ino'];"\` |
|        - |  1201 | `    "}"` |
|        - |  1202 |  |
|        - |  1203 | `/*` |
|        - |  1204 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1205 | ` * start compiling the target PHP program.` |
|        - |  1206 | ` */` |
|     2628 |  1207 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1208 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1209 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1210 | `	 )` |
|        2 |  1211 |  |
|        - |  1212 | `	SyString sBuiltin;` |
|        - |  1213 | `	ph7_value *pObj;` |
|        - |  1214 | `	sxi32 rc;` |
|        - |  1215 | `	/* Zero the structure */` |
|     2630 |  1216 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1217 | `	/* Initialize VM fields */` |
|     2630 |  1218 | `	pVm->pEngine = &(*pEngine);` |
|     2630 |  1219 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1220 | `	/* Instructions containers */` |
|     2630 |  1221 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2630 |  1222 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2630 |  1223 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1224 | `	/* Object containers */` |
|     2630 |  1225 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2630 |  1226 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1227 | `	/* Virtual machine internal containers */` |
|     2630 |  1228 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2630 |  1229 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2630 |  1230 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2630 |  1231 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2630 |  1232 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2630 |  1233 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2630 |  1234 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2630 |  1235 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2630 |  1236 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2630 |  1237 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2630 |  1238 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2630 |  1239 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2630 |  1240 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2630 |  1241 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2630 |  1242 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2630 |  1243 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2630 |  1244 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2630 |  1245 | `	pVm->pPendingException = 0;` |
|        - |  1246 | `	/* Configuration containers */` |
|     2630 |  1247 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2630 |  1248 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2630 |  1249 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2630 |  1250 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2630 |  1251 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2630 |  1252 | `	pVm->iResponseStatus = 200;` |
|     2630 |  1253 | `	pVm->bHeadersSent = 0;` |
|     2630 |  1254 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1255 | `	/* Error callbacks containers */` |
|     2630 |  1256 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2630 |  1257 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2630 |  1258 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2630 |  1259 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2630 |  1260 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1261 | `	/* Set a default recursion limit */` |
|        - |  1262 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2630 |  1263 | `	pVm->nMaxDepth = 32;` |
|        - |  1264 | `#else` |
|        - |  1265 | `	pVm->nMaxDepth = 16;` |
|        - |  1266 | `#endif` |
|        - |  1267 | `	/* Default assertion flags */` |
|     2630 |  1268 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1269 | `	/* JSON return status */` |
|     2630 |  1270 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1271 | `	/* PRNG context */` |
|     2630 |  1272 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1273 | `	/* Install the null constant */` |
|     2630 |  1274 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2630 |  1275 | `	if( pObj == 0 ){` |
|      ! 0 |  1276 | `		rc = SXERR_MEM;` |
|      ! 0 |  1277 | `		goto Err;` |
|        - |  1278 | `	}` |
|     2630 |  1279 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1280 | `	/* Install the boolean TRUE constant */` |
|     2630 |  1281 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2630 |  1282 | `	if( pObj == 0 ){` |
|      ! 0 |  1283 | `		rc = SXERR_MEM;` |
|      ! 0 |  1284 | `		goto Err;` |
|        - |  1285 | `	}` |
|     2630 |  1286 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1287 | `	/* Install the boolean FALSE constant */` |
|     2630 |  1288 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2630 |  1289 | `	if( pObj == 0 ){` |
|      ! 0 |  1290 | `		rc = SXERR_MEM;` |
|      ! 0 |  1291 | `		goto Err;` |
|        - |  1292 | `	}` |
|     2630 |  1293 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1294 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1295 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1296 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2630 |  1297 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2630 |  1298 | `	if( pObj == 0 ){` |
|      ! 0 |  1299 | `		rc = SXERR_MEM;` |
|      ! 0 |  1300 | `		goto Err;` |
|        - |  1301 | `	}` |
|     2630 |  1302 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1303 | `	/* Create the global frame */` |
|     2630 |  1304 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2630 |  1305 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1306 | `		goto Err;` |
|        - |  1307 | `	}` |
|        - |  1308 | `	/* Initialize the code generator */` |
|     2630 |  1309 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2630 |  1310 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1311 | `		goto Err;` |
|        - |  1312 | `	}` |
|        - |  1313 | `	/* VM correctly initialized,set the magic number */` |
|     2630 |  1314 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2630 |  1315 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1316 | `	/* Compile the built-in library */` |
|     2630 |  1317 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1318 | `	/* Reset the code generator */` |
|     2630 |  1319 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2630 |  1320 | `	return SXRET_OK;` |
|      ! 0 |  1321 | `Err:` |
|      ! 0 |  1322 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1323 | `	return rc;` |
|     1316 |  1324 |  |
|        - |  1325 | `/*` |
|        - |  1326 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1327 | ` * routine which store the output in an internal blob.` |
|        - |  1328 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1329 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1330 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1331 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1332 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1333 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1334 | ` * to finish executing and extracting the output.` |
|        - |  1335 | ` */` |
|       38 |  1336 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1337 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1338 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1339 | `	void *pUserData     /* User private data */` |
|        - |  1340 | `	)` |
|      ! 0 |  1341 |  |
|        - |  1342 | `	 sxi32 rc;` |
|        - |  1343 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1344 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1345 | `	 return rc;` |
|      ! 0 |  1346 |  |
|        - |  1347 | `/*` |
|        - |  1348 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1349 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1350 | ` */` |
|    13170 |  1351 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1352 |  |
|    13172 |  1353 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    13172 |  1354 | `	if( xCons != VmObConsumer ){` |
|     6260 |  1355 | `		pVm->nOutputLen += nLen;` |
|     6260 |  1356 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      858 |  1357 | `			pVm->bHeadersSent = 1;` |
|      428 |  1358 | `		}` |
|     3129 |  1359 | `	}` |
|    13172 |  1360 |  |
|        - |  1361 | `#define VM_STACK_GUARD 16` |
|        - |  1362 | `/*` |
|        - |  1363 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1364 | ` * our compiled PHP program.` |
|        - |  1365 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1366 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1367 | ` */` |
|    31520 |  1368 | `static ph7_value * VmNewOperandStack(` |
|        - |  1369 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1370 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1371 | `	)` |
|        2 |  1372 |  |
|        - |  1373 | `	ph7_value *pStack;` |
|        - |  1374 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1375 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1376 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1377 | `  ** on the maximum stack depth required.` |
|        - |  1378 | `  **` |
|        - |  1379 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1380 | `  */` |
|    31522 |  1381 | `	nInstr += VM_STACK_GUARD;` |
|    31522 |  1382 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    31522 |  1383 | `	if( pStack == 0 ){` |
|      ! 0 |  1384 | `		return 0;` |
|        - |  1385 | `	}` |
|        - |  1386 | `	/* Initialize the operand stack */` |
|  2004262 |  1387 | `	while( nInstr > 0 ){` |
|  1972742 |  1388 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1972742 |  1389 | `		--nInstr;` |
|        2 |  1390 | `	}` |
|        - |  1391 | `	/* Ready for bytecode execution */` |
|    31522 |  1392 | `	return pStack;` |
|    15762 |  1393 |  |
|        - |  1394 | `/* Forward declaration */` |
|        - |  1395 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1396 | `/*` |
|        - |  1397 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1398 | ` * This routine gets called by the PH7 engine after` |
|        - |  1399 | ` * successful compilation of the target PHP program.` |
|        - |  1400 | ` */` |
|     2366 |  1401 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1402 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1403 | `	)` |
|        2 |  1404 |  |
|        - |  1405 | `	SyHashEntry *pEntry;` |
|        - |  1406 | `	sxi32 rc;` |
|     2368 |  1407 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1408 | `		/* Initialize your VM first */` |
|      ! 0 |  1409 | `		return SXERR_CORRUPT;` |
|        - |  1410 | `	}` |
|        - |  1411 | `	/* Mark the VM ready for byte-code execution */` |
|     2368 |  1412 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1413 | `	/* Release the code generator now we have compiled our program */` |
|     2368 |  1414 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1415 | `	/* Emit the DONE instruction */` |
|     2368 |  1416 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2368 |  1417 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1418 | `		return SXERR_MEM;` |
|        - |  1419 | `	}` |
|        - |  1420 | `	/* Script return value */` |
|     2368 |  1421 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1422 | `	/* Allocate a new operand stack */` |
|     2368 |  1423 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2368 |  1424 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1425 | `		return SXERR_MEM;` |
|        - |  1426 | `	}` |
|        - |  1427 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1428 | `	 * private data. */` |
|     2368 |  1429 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2368 |  1430 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1431 | `	/* Allocate the reference table */` |
|     2368 |  1432 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2368 |  1433 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2368 |  1434 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1435 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1436 | `		return SXERR_MEM;` |
|        - |  1437 | `	}` |
|        - |  1438 | `	/* Zero the reference table */` |
|     2368 |  1439 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1440 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2368 |  1441 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2368 |  1442 | `	if( rc != SXRET_OK ){` |
|        - |  1443 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1444 | `		return rc;` |
|        - |  1445 | `	}` |
|        - |  1446 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2368 |  1447 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2368 |  1448 | `	if( rc != SXRET_OK ){` |
|        - |  1449 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1450 | `		return rc;` |
|        - |  1451 | `	}` |
|        - |  1452 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2368 |  1453 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1454 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2368 |  1455 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1456 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2368 |  1457 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1458 | `	/* Initialize and install static and constants class attributes */` |
|     2368 |  1459 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    30924 |  1460 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    28558 |  1461 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    28558 |  1462 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1463 | `			return rc;` |
|        - |  1464 | `		}` |
|        2 |  1465 | `	}` |
|        - |  1466 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2368 |  1467 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1468 | `	/* VM is ready for bytecode execution */` |
|     2368 |  1469 | `	return SXRET_OK;` |
|     1185 |  1470 |  |
|        - |  1471 | `/*` |
|        - |  1472 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1473 | ` */` |
|      ! 0 |  1474 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1475 |  |
|      ! 0 |  1476 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1477 | `		return SXERR_CORRUPT;` |
|        - |  1478 | `	}` |
|        - |  1479 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1480 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1481 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1482 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1483 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1484 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1485 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1486 | `	pVm->bHttpContext = 0;` |
|        - |  1487 | `	/* Set the ready flag */` |
|      ! 0 |  1488 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1489 | `	return SXRET_OK;` |
|      ! 0 |  1490 |  |
|        - |  1491 | `/*` |
|        - |  1492 | ` * Release a Virtual Machine.` |
|        - |  1493 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1494 | ` */` |
|     2358 |  1495 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1496 |  |
|        - |  1497 | `	/* Set the stale magic number */` |
|     2360 |  1498 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1499 | `	/* Release the private memory subsystem */` |
|     2360 |  1500 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2360 |  1501 | `	return SXRET_OK;` |
|        2 |  1502 |  |
|        - |  1503 | `/*` |
|        - |  1504 | ` * Initialize a foreign function call context.` |
|        - |  1505 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1506 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1507 | ` * functions.` |
|        - |  1508 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1509 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1510 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1511 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1512 | ` */` |
|   561312 |  1513 | `static sxi32 VmInitCallContext(` |
|        - |  1514 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1515 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1516 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1517 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1518 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1519 | `	)` |
|        2 |  1520 |  |
|   561314 |  1521 | `	pOut->pFunc = pFunc;` |
|   561314 |  1522 | `	pOut->pVm   = pVm;` |
|   561314 |  1523 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   561314 |  1524 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1525 | `	/* Assume a null return value */` |
|   561314 |  1526 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   561314 |  1527 | `	pOut->pRet = pRet;` |
|   561314 |  1528 | `	pOut->iFlags = iFlags;` |
|   561314 |  1529 | `	return SXRET_OK;` |
|        2 |  1530 |  |
|        - |  1531 | `/*` |
|        - |  1532 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1533 | ` * left behind.` |
|        - |  1534 | ` */` |
|   561312 |  1535 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1536 |  |
|        - |  1537 | `	sxu32 n;` |
|   561314 |  1538 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6798 |  1539 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    19400 |  1540 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    12604 |  1541 | `			if( apObj[n] == 0 ){` |
|        - |  1542 | `				/* Already released */` |
|      250 |  1543 | `				continue;` |
|        - |  1544 | `			}` |
|    12356 |  1545 | `			PH7_MemObjRelease(apObj[n]);` |
|    12356 |  1546 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6179 |  1547 | `		}` |
|     6798 |  1548 | `		SySetRelease(&pCtx->sVar);` |
|     3398 |  1549 | `	}` |
|   561314 |  1550 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1551 | `		ph7_aux_data *aAux;` |
|        - |  1552 | `		void *pChunk;` |
|        - |  1553 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1554 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1555 | `		 */` |
|        9 |  1556 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1557 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1558 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1559 | `			/* Release the chunk */` |
|       25 |  1560 | `			if( pChunk ){` |
|       25 |  1561 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1562 | `			}` |
|       13 |  1563 | `		}` |
|        9 |  1564 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1565 | `	}` |
|   561314 |  1566 |  |
|        - |  1567 | `/*` |
|        - |  1568 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1569 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1570 | ` */` |
|      248 |  1571 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1572 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1573 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1574 | `	)` |
|        2 |  1575 |  |
|      250 |  1576 | `	if( pValue == 0 ){` |
|        - |  1577 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1578 | `		return;` |
|        - |  1579 | `	}` |
|      250 |  1580 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      250 |  1581 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1582 | `		sxu32 n;` |
|      936 |  1583 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      936 |  1584 | `			if( apObj[n] == pValue ){` |
|      250 |  1585 | `				PH7_MemObjRelease(pValue);` |
|      250 |  1586 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1587 | `				/* Mark as released */` |
|      250 |  1588 | `				apObj[n] = 0;` |
|      250 |  1589 | `				break;` |
|        - |  1590 | `			}` |
|      345 |  1591 | `		}` |
|      124 |  1592 | `	}` |
|      126 |  1593 |  |
|        - |  1594 | `/*` |
|        - |  1595 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1596 | ` */` |
|  3283638 |  1597 | `static void VmPopOperand(` |
|        - |  1598 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1599 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1600 | `	)` |
|        2 |  1601 |  |
|  3283640 |  1602 | `	ph7_value *pTos = *ppTos;` |
|  6973642 |  1603 | `	while( nPop > 0 ){` |
|  3690004 |  1604 | `		PH7_MemObjRelease(pTos);` |
|  3690004 |  1605 | `		pTos--;` |
|  3690004 |  1606 | `		nPop--;` |
|        2 |  1607 | `	}` |
|        - |  1608 | `	/* Top of the stack */` |
|  3283640 |  1609 | `	*ppTos = pTos;` |
|  3283640 |  1610 |  |
|        - |  1611 | `/*` |
|        - |  1612 | ` * Reserve a memory object.` |
|        - |  1613 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1614 | ` */` |
|  3011164 |  1615 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1616 |  |
|  3011166 |  1617 | `	ph7_value *pObj = 0;` |
|        - |  1618 | `	VmSlot *pSlot;` |
|        - |  1619 | `	sxu32 nIdx;` |
|        - |  1620 | `	/* Check for a free slot */` |
|  3011166 |  1621 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3011166 |  1622 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3011166 |  1623 | `	if( pSlot ){` |
|   868506 |  1624 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   868506 |  1625 | `		nIdx = pSlot->nIdx;` |
|   434252 |  1626 | `	}` |
|  3011166 |  1627 | `	if( pObj == 0 ){` |
|        - |  1628 | `		/* Reserve a new memory object */` |
|  2142662 |  1629 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2142662 |  1630 | `		if( pObj == 0 ){` |
|      ! 0 |  1631 | `			return 0;` |
|        - |  1632 | `		}` |
|  1071330 |  1633 | `	}` |
|        - |  1634 | `	/* Set a null default value */` |
|  3011166 |  1635 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3011166 |  1636 | `	pObj->nIdx = nIdx;` |
|  3011166 |  1637 | `	return pObj;` |
|  1505584 |  1638 |  |
|        - |  1639 | `/*` |
|        - |  1640 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1641 | ` */` |
|    30490 |  1642 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1643 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1644 | `	const char *zKey,  /* Entry key */` |
|        - |  1645 | `	sxu32 nByte,       /* Key length */` |
|        - |  1646 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1647 | `	)` |
|        2 |  1648 |  |
|        - |  1649 | `	ph7_value sKey;` |
|        - |  1650 | `	sxi32 rc;` |
|    30492 |  1651 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    30492 |  1652 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1653 | `	/* Perform the insertion */` |
|    30492 |  1654 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    30492 |  1655 | `	PH7_MemObjRelease(&sKey);` |
|    30492 |  1656 | `	return rc;` |
|        2 |  1657 |  |
|        - |  1658 | `/*` |
|        - |  1659 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1660 | ` * Return a pointer to the variable value on success.` |
|        - |  1661 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1662 | ` */` |
|  3068506 |  1663 | `static ph7_value * VmExtractMemObj(` |
|        - |  1664 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1665 | `	const SyString *pName, /* Variable name */` |
|        - |  1666 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1667 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1668 | `	)` |
|        2 |  1669 |  |
|  3068508 |  1670 | `	int bNullify = FALSE;` |
|        - |  1671 | `	SyHashEntry *pEntry;` |
|        - |  1672 | `	VmFrame *pFrame;` |
|        - |  1673 | `	ph7_value *pObj;` |
|        - |  1674 | `	sxu32 nIdx;` |
|        - |  1675 | `	sxi32 rc;` |
|        - |  1676 | `	/* Point to the top active frame */` |
|  3068508 |  1677 | `	pFrame = pVm->pFrame;` |
|  3068508 |  1678 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1679 | `	/* Perform the lookup */` |
|  3068508 |  1680 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1681 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1682 | `		pName = &sAnnon;` |
|        - |  1683 | `		/* Always nullify the object */` |
|      ! 0 |  1684 | `		bNullify = TRUE;` |
|      ! 0 |  1685 | `		bDup = FALSE;` |
|      ! 0 |  1686 | `	}` |
|        - |  1687 | `	/* Check the superglobals table first */` |
|  3068508 |  1688 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3068508 |  1689 | `	if( pEntry == 0 ){` |
|        - |  1690 | `		/* Query the top active frame */` |
|  3068468 |  1691 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3068468 |  1692 | `		if( pEntry == 0 ){` |
|    84162 |  1693 | `			char *zName = (char *)pName->zString;` |
|        - |  1694 | `			VmSlot sLocal;` |
|    84162 |  1695 | `			if( !bCreate ){` |
|        - |  1696 | `				/* Do not create the variable,return NULL instead */` |
|       36 |  1697 | `				return 0;` |
|        - |  1698 | `			}` |
|        - |  1699 | `			/* No such variable,automatically create a new one and install` |
|        - |  1700 | `			 * it in the current frame.` |
|        - |  1701 | `			 */` |
|    84128 |  1702 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    84128 |  1703 | `			if( pObj == 0 ){` |
|      ! 0 |  1704 | `				return 0;` |
|        - |  1705 | `			}` |
|    84128 |  1706 | `			nIdx = pObj->nIdx;` |
|    84128 |  1707 | `			if( bDup ){` |
|        - |  1708 | `				/* Duplicate name */` |
|      164 |  1709 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      164 |  1710 | `				if( zName == 0 ){` |
|      ! 0 |  1711 | `					return 0;` |
|        - |  1712 | `				}` |
|       81 |  1713 | `			}` |
|        - |  1714 | `			/* Link to the top active VM frame */` |
|    84128 |  1715 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    84128 |  1716 | `			if( rc != SXRET_OK ){` |
|        - |  1717 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1718 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1719 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1720 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1721 | `				return 0;` |
|        - |  1722 | `			}` |
|    84128 |  1723 | `			if( pFrame->pParent != 0 ){` |
|        - |  1724 | `				/* Local variable */` |
|    77324 |  1725 | `				sLocal.nIdx = nIdx;` |
|    77324 |  1726 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    38663 |  1727 | `			}else{` |
|        - |  1728 | `				/* Register in the $GLOBALS array */` |
|     6806 |  1729 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1730 | `			}` |
|        - |  1731 | `			/* Install in the reference table */` |
|    84128 |  1732 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1733 | `			/* Save object index */` |
|    84128 |  1734 | `			pObj->nIdx = nIdx;` |
|    42065 |  1735 | `		}else{` |
|        - |  1736 | `			/* Extract variable contents */` |
|  2984308 |  1737 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  2984308 |  1738 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2984308 |  1739 | `			if( bNullify && pObj ){` |
|      ! 0 |  1740 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1741 | `			}` |
|        - |  1742 | `		}` |
|  1534328 |  1743 | `	}else{` |
|        - |  1744 | `		/* Superglobal */` |
|       42 |  1745 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1746 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1747 | `	}` |
|  3068474 |  1748 | `	return pObj;` |
|  1534365 |  1749 |  |
|        - |  1750 | `/*` |
|        - |  1751 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1752 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1753 | ` */` |
|     2670 |  1754 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1755 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1756 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1757 | `	sxu32 nByte        /* zName length */` |
|        - |  1758 | `	)` |
|        2 |  1759 |  |
|        - |  1760 | `	SyHashEntry *pEntry;` |
|        - |  1761 | `	ph7_value *pValue;` |
|        - |  1762 | `	sxu32 nIdx;` |
|        - |  1763 | `	/* Query the superglobal table */` |
|     2672 |  1764 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2672 |  1765 | `	if( pEntry == 0 ){` |
|        - |  1766 | `		/* No such entry */` |
|      ! 0 |  1767 | `		return 0;` |
|        - |  1768 | `	}` |
|        - |  1769 | `	/* Extract the superglobal index in the global object pool */` |
|     2672 |  1770 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1771 | `	/* Extract the variable value  */` |
|     2672 |  1772 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2672 |  1773 | `	return pValue;` |
|     1337 |  1774 |  |
|        - |  1775 | `/*` |
|        - |  1776 | ` * Perform a raw hashmap insertion.` |
|        - |  1777 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1778 | ` */` |
|     2700 |  1779 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1780 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1781 | `	const char *zKey,   /* Entry key */` |
|        - |  1782 | `	int nKeylen,        /* zKey length*/` |
|        - |  1783 | `	const char *zData,  /* Entry data */` |
|        - |  1784 | `	int nLen            /* zData length */` |
|        - |  1785 | `	)` |
|        2 |  1786 |  |
|        - |  1787 | `	ph7_value sKey,sValue;` |
|        - |  1788 | `	sxi32 rc;` |
|     2702 |  1789 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2702 |  1790 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2702 |  1791 | `	if( zKey ){` |
|     2680 |  1792 | `		if( nKeylen < 0 ){` |
|     2628 |  1793 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1313 |  1794 | `		}` |
|     2680 |  1795 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1339 |  1796 | `	}` |
|     2702 |  1797 | `	if( zData ){` |
|     2702 |  1798 | `		if( nLen < 0 ){` |
|        - |  1799 | `			/* Compute length automatically */` |
|      144 |  1800 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  1801 | `		}` |
|     2702 |  1802 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1350 |  1803 | `	}` |
|        - |  1804 | `	/* Perform the insertion */` |
|     2702 |  1805 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2702 |  1806 | `	PH7_MemObjRelease(&sKey);` |
|     2702 |  1807 | `	PH7_MemObjRelease(&sValue);` |
|     2702 |  1808 | `	return rc;` |
|        2 |  1809 |  |
|        - |  1810 | `/*` |
|        - |  1811 | ` * Configure a working virtual machine instance.` |
|        - |  1812 | ` *` |
|        - |  1813 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1814 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1815 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1816 | ` * The second argument to this function is an integer configuration option` |
|        - |  1817 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1818 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1819 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1820 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1821 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1822 | ` */` |
|    38186 |  1823 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1824 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1825 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1826 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1827 | `	)` |
|        2 |  1828 |  |
|    38188 |  1829 | `	sxi32 rc = SXRET_OK;` |
|    38188 |  1830 | `	switch(nOp){` |
|     1175 |  1831 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2352 |  1832 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2352 |  1833 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1834 | `		/* VM output consumer callback */` |
|        - |  1835 | `#ifdef UNTRUST` |
|        - |  1836 | `		if( xConsumer == 0 ){` |
|        - |  1837 | `			rc = SXERR_CORRUPT;` |
|        - |  1838 | `			break;` |
|        - |  1839 | `		}` |
|        - |  1840 | `#endif` |
|        - |  1841 | `		/* Install the output consumer */` |
|     2352 |  1842 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2352 |  1843 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2352 |  1844 | `		break;` |
|        - |  1845 | `							   }` |
|     1183 |  1846 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1847 | `		/* Import path */` |
|        - |  1848 | `		  const char *zPath;` |
|        - |  1849 | `		  SyString sPath;` |
|     2368 |  1850 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1851 | `#if defined(UNTRUST)` |
|        - |  1852 | `		  if( zPath == 0 ){` |
|        - |  1853 | `			  rc = SXERR_EMPTY;` |
|        - |  1854 | `			  break;` |
|        - |  1855 | `		  }` |
|        - |  1856 | `#endif` |
|     2368 |  1857 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1858 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1859 | `#ifdef __WINNT__` |
|        2 |  1860 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1861 | `#endif` |
|     4734 |  1862 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1863 | `		  /* Remove leading and trailing white spaces */` |
|     2368 |  1864 | `		  SyStringFullTrim(&sPath);` |
|     2368 |  1865 | `		  if( sPath.nByte > 0 ){` |
|        - |  1866 | `			  /* Store the path in the corresponding conatiner */` |
|     2368 |  1867 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1183 |  1868 | `		  }` |
|     2368 |  1869 | `		  break;` |
|        - |  1870 | `									 }` |
|     1183 |  1871 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1872 | `		/* Run-Time Error report */` |
|     2368 |  1873 | `		pVm->bErrReport = 1;` |
|     2368 |  1874 | `		break;` |
|      ! 0 |  1875 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1876 | `		/* Recursion depth */` |
|      ! 0 |  1877 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1878 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1879 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1880 | `		}` |
|      ! 0 |  1881 | `		break;` |
|        - |  1882 | `									   }` |
|      ! 0 |  1883 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1884 | `		/* VM output length in bytes */` |
|      ! 0 |  1885 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1886 | `#ifdef UNTRUST` |
|        - |  1887 | `		if( pOut == 0 ){` |
|        - |  1888 | `			rc = SXERR_CORRUPT;` |
|        - |  1889 | `			break;` |
|        - |  1890 | `		}` |
|        - |  1891 | `#endif` |
|      ! 0 |  1892 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1893 | `		break;` |
|        - |  1894 | `							   }` |
|        - |  1895 |  |
|    11830 |  1896 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1897 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1898 | `		/* Create a new superglobal/global variable */` |
|    23662 |  1899 | `		const char *zName = va_arg(ap,const char *);` |
|    23662 |  1900 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1901 | `		SyHashEntry *pEntry;` |
|        - |  1902 | `		ph7_value *pObj;` |
|        - |  1903 | `		sxu32 nByte;` |
|        - |  1904 | `		sxu32 nIdx;` |
|        - |  1905 | `#ifdef UNTRUST` |
|        - |  1906 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1907 | `			rc = SXERR_CORRUPT;` |
|        - |  1908 | `			break;` |
|        - |  1909 | `		}` |
|        - |  1910 | `#endif` |
|    23662 |  1911 | `		nByte = SyStrlen(zName);` |
|    23662 |  1912 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1913 | `			/* Check if the superglobal is already installed */` |
|    23662 |  1914 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    11832 |  1915 | `		}else{` |
|        - |  1916 | `			/* Query the top active VM frame */` |
|      ! 0 |  1917 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1918 | `		}` |
|    23662 |  1919 | `		if( pEntry ){` |
|        - |  1920 | `			/* Variable already installed */` |
|      ! 0 |  1921 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1922 | `			/* Extract contents */` |
|      ! 0 |  1923 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  1924 | `			if( pObj ){` |
|        - |  1925 | `				/* Overwrite old contents */` |
|      ! 0 |  1926 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  1927 | `			}` |
|      ! 0 |  1928 | `		}else{` |
|        - |  1929 | `			/* Install a new variable */` |
|    23662 |  1930 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    23662 |  1931 | `			if( pObj == 0 ){` |
|      ! 0 |  1932 | `				rc = SXERR_MEM;` |
|      ! 0 |  1933 | `				break;` |
|        - |  1934 | `			}` |
|    23662 |  1935 | `			nIdx = pObj->nIdx;` |
|        - |  1936 | `			/* Copy value */` |
|    23662 |  1937 | `			PH7_MemObjStore(pValue,pObj);` |
|    23662 |  1938 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1939 | `				/* Install the superglobal */` |
|    23662 |  1940 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    11832 |  1941 | `			}else{` |
|        - |  1942 | `				/* Install in the current frame */` |
|      ! 0 |  1943 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1944 | `			}` |
|    23662 |  1945 | `			if( rc == SXRET_OK ){` |
|        - |  1946 | `				SyHashEntry *pRef;` |
|    23662 |  1947 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    23662 |  1948 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    11832 |  1949 | `				}else{` |
|      ! 0 |  1950 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1951 | `				}` |
|        - |  1952 | `				/* Install in the reference table */` |
|    23662 |  1953 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    23662 |  1954 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1955 | `					/* Register in the $GLOBALS array */` |
|    23662 |  1956 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    11830 |  1957 | `				}` |
|    11830 |  1958 | `			}` |
|        - |  1959 | `		}` |
|    23662 |  1960 | `		break;` |
|        - |  1961 | `									}` |
|     1313 |  1962 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1963 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1964 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1965 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1966 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1967 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1968 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2628 |  1969 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2628 |  1970 | `		const char *zValue = va_arg(ap,const char *);` |
|     2628 |  1971 | `		int nLen = va_arg(ap,int);` |
|        - |  1972 | `		ph7_hashmap *pMap;` |
|        - |  1973 | `		ph7_value *pValue;` |
|     2628 |  1974 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1975 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1976 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2627 |  1977 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  1978 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  1979 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2626 |  1980 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  1981 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  1982 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2626 |  1983 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  1984 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  1985 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2626 |  1986 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  1987 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  1988 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2626 |  1989 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  1990 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  1991 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  1992 | `		}else{` |
|        - |  1993 | `			/* Extract the $_SERVER superglobal */` |
|     2626 |  1994 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  1995 | `		}` |
|     2628 |  1996 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1997 | `			/* No such entry */` |
|      ! 0 |  1998 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1999 | `			break;` |
|        - |  2000 | `		}` |
|        - |  2001 | `		/* Point to the hashmap */` |
|     2628 |  2002 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2003 | `		/* Perform the insertion */` |
|     2628 |  2004 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2628 |  2005 | `		break;` |
|        - |  2006 | `								   }` |
|       11 |  2007 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2008 | `		/* Script arguments */` |
|       24 |  2009 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2010 | `		ph7_hashmap *pMap;` |
|        - |  2011 | `		ph7_value *pValue;` |
|        - |  2012 | `		sxu32 n;` |
|       24 |  2013 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2014 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2015 | `			break;` |
|        - |  2016 | `		}` |
|        - |  2017 | `		/* Extract the $argv array */` |
|       24 |  2018 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2019 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2020 | `			/* No such entry */` |
|      ! 0 |  2021 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2022 | `			break;` |
|        - |  2023 | `		}` |
|        - |  2024 | `		/* Point to the hashmap */` |
|       24 |  2025 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2026 | `		/* Perform the insertion */` |
|       24 |  2027 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2028 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2029 | `		if( rc == SXRET_OK ){` |
|       24 |  2030 | `			if( pMap->nEntry > 1 ){` |
|        - |  2031 | `				/* Append space separator first */` |
|       18 |  2032 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2033 | `			}` |
|       24 |  2034 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2035 | `		}` |
|       24 |  2036 | `		break;` |
|        - |  2037 | `								  }` |
|      ! 0 |  2038 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2039 | `		/* error_log() consumer */` |
|      ! 0 |  2040 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2041 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2042 | `		break;` |
|        - |  2043 | `										}` |
|      ! 0 |  2044 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2045 | `		/* Script return value */` |
|      ! 0 |  2046 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2047 | `#ifdef UNTRUST` |
|        - |  2048 | `		if( ppValue == 0 ){` |
|        - |  2049 | `			rc = SXERR_CORRUPT;` |
|        - |  2050 | `			break;` |
|        - |  2051 | `		}` |
|        - |  2052 | `#endif` |
|      ! 0 |  2053 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2054 | `		break;` |
|        - |  2055 | `								   }` |
|     2366 |  2056 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2057 | `		/* Register an IO stream device */` |
|     4734 |  2058 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2059 | `		/* Make sure we are dealing with a valid IO stream */` |
|     7098 |  2060 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4734 |  2061 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2062 | `				/* Invalid stream */` |
|      ! 0 |  2063 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2064 | `				break;` |
|        - |  2065 | `		}` |
|     4734 |  2066 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2067 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2368 |  2068 | `			pVm->pDefStream = pStream;` |
|     1183 |  2069 | `		}` |
|        - |  2070 | `		/* Insert in the appropriate container */` |
|     4734 |  2071 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4734 |  2072 | `		break;` |
|        - |  2073 | `								  }` |
|        8 |  2074 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2075 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2076 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2077 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2078 | `#ifdef UNTRUST` |
|        - |  2079 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2080 | `			rc = SXERR_CORRUPT;` |
|        - |  2081 | `			break;` |
|        - |  2082 | `		}` |
|        - |  2083 | `#endif` |
|       16 |  2084 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2085 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2086 | `		break;` |
|        - |  2087 | `									   }` |
|        8 |  2088 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2089 | `		/* Raw HTTP request*/` |
|       16 |  2090 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2091 | `		int nByte = va_arg(ap,int);` |
|       16 |  2092 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2093 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2094 | `			break;` |
|        - |  2095 | `		}` |
|       16 |  2096 | `		if( nByte < 0 ){` |
|        - |  2097 | `			/* Compute length automatically */` |
|      ! 0 |  2098 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2099 | `		}` |
|        - |  2100 | `		/* Process the request */` |
|       16 |  2101 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2102 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2103 | `		if( rc == SXRET_OK ){` |
|       16 |  2104 | `			pVm->bHttpContext = 1;` |
|        8 |  2105 | `		}` |
|       16 |  2106 | `		break;` |
|        - |  2107 | `									}` |
|        8 |  2108 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2109 | `		/* Extract HTTP response status code */` |
|       16 |  2110 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2111 | `		if( pStatus ){` |
|       16 |  2112 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2113 | `		}` |
|       16 |  2114 | `		break;` |
|        - |  2115 | `										}` |
|        8 |  2116 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2117 | `		/* Iterate response headers via callback */` |
|        - |  2118 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2119 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2120 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2121 | `		if( xCallback ){` |
|       16 |  2122 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2123 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2124 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2125 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2126 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2127 | `							   pUserData);` |
|       12 |  2128 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2129 | `					break;` |
|        - |  2130 | `				}` |
|        6 |  2131 | `			}` |
|        8 |  2132 | `		}` |
|       16 |  2133 | `		break;` |
|        - |  2134 | `										 }` |
|      ! 0 |  2135 | `	default:` |
|        - |  2136 | `		/* Unknown configuration option */` |
|      ! 0 |  2137 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2138 | `		break;` |
|        - |  2139 | `	}` |
|    38188 |  2140 | `	return rc;` |
|        2 |  2141 |  |
|        - |  2142 | `/* Forward declaration */` |
|        - |  2143 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2144 | `/*` |
|        - |  2145 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2146 | ` * format.` |
|        - |  2147 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2148 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2149 | ` * (STDOUT).` |
|        - |  2150 | ` */` |
|        2 |  2151 | `static sxi32 VmByteCodeDump(` |
|        - |  2152 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2153 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2154 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2155 | `	)` |
|        1 |  2156 |  |
|        - |  2157 | `	static const char zDump[] = {` |
|        - |  2158 | `		"====================================================\n"` |
|        - |  2159 | `		"PH7 VM Dump\n"` |
|        - |  2160 | `		"====================================================\n"` |
|        - |  2161 | `	};` |
|        - |  2162 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2163 | `	sxi32 rc = SXRET_OK;` |
|        - |  2164 | `	sxu32 n;` |
|        - |  2165 | `	/* Point to the PH7 instructions */` |
|        3 |  2166 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2167 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2168 | `	n = 0;` |
|        3 |  2169 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2170 | `	/* Dump instructions */` |
|        7 |  2171 | `	for(;;){` |
|       15 |  2172 | `		if( pInstr >= pEnd ){` |
|        - |  2173 | `			/* No more instructions */` |
|        3 |  2174 | `			break;` |
|        - |  2175 | `		}` |
|        - |  2176 | `		/* Format and call the consumer callback */` |
|       19 |  2177 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2178 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2179 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2180 | `		if( rc != SXRET_OK ){` |
|        - |  2181 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2182 | `			return rc;` |
|        - |  2183 | `		}` |
|       13 |  2184 | `		++n;` |
|       13 |  2185 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2186 | `	}` |
|        3 |  2187 | `	return rc;` |
|        2 |  2188 |  |
|        - |  2189 | `/* Forward declaration */` |
|        - |  2190 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2191 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2192 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2193 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2194 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2195 | `/*` |
|        - |  2196 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2197 | ` * consumer callback.` |
|        - |  2198 | ` */` |
|      544 |  2199 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2200 |  |
|      545 |  2201 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      545 |  2202 | `	sxi32 rc = SXRET_OK;` |
|        - |  2203 | `	/* Append a new line */` |
|        - |  2204 | `#ifdef __WINNT__` |
|        1 |  2205 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2206 | `#else` |
|      544 |  2207 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2208 | `#endif` |
|        - |  2209 | `	/* Invoke the output consumer callback */` |
|      545 |  2210 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      545 |  2211 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      545 |  2212 | `	return rc;` |
|        1 |  2213 |  |
|        - |  2214 | `/*` |
|        - |  2215 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2216 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2217 | ` * information.` |
|        - |  2218 | ` */` |
|      132 |  2219 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2220 |  |
|      134 |  2221 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2222 | `		ph7_value apArg[4];` |
|        - |  2223 | `		ph7_value *apArgPtr[4];` |
|        - |  2224 | `		ph7_value sResult;` |
|        - |  2225 | `		SyString sErr;` |
|        - |  2226 | `		/* Prepare arguments */` |
|       61 |  2227 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2228 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2229 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2230 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2231 | `		if( pFile ){` |
|       61 |  2232 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2233 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2234 | `		}else{` |
|      ! 0 |  2235 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2236 | `		}` |
|       61 |  2237 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2238 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2239 | `		/* Set up pointer array */` |
|       61 |  2240 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2241 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2242 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2243 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2244 | `		/* Call the handler */` |
|       61 |  2245 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2246 | `		/* Check return value */` |
|       61 |  2247 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2248 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2249 | `		}` |
|        - |  2250 | `		/* Release */` |
|       61 |  2251 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2252 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2253 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2254 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2255 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2256 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2257 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2258 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2259 | `	}` |
|        - |  2260 | `	/* No handler, always call error handler */` |
|       73 |  2261 | `	return TRUE;` |
|       68 |  2262 |  |
|       96 |  2263 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2264 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2265 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2266 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2267 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2268 | `	)` |
|        2 |  2269 |  |
|       98 |  2270 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2271 | `	SyString *pFile;` |
|        - |  2272 | `	char *zErr;` |
|       98 |  2273 | `	sxi32 rc = SXRET_OK;` |
|       98 |  2274 | `	if( !pVm->bErrReport ){` |
|        - |  2275 | `		/* Don't bother reporting errors */` |
|        3 |  2276 | `		return SXRET_OK;` |
|        - |  2277 | `	}` |
|        - |  2278 | `	/* Reset the working buffer */` |
|       96 |  2279 | `	SyBlobReset(pWorker);` |
|        - |  2280 | `	/* Peek the processed file if available */` |
|       96 |  2281 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       96 |  2282 | `	if( pFile ){` |
|        - |  2283 | `		/* Append file name */` |
|       96 |  2284 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       96 |  2285 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       47 |  2286 | `	}` |
|        - |  2287 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2288 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2289 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2290 | `	 * E_DEPRECATED). */` |
|       96 |  2291 | `	zErr = "Error:  ";` |
|       96 |  2292 | `	switch(iErr){` |
|       18 |  2293 | `	case PH7_CTX_WARNING:` |
|       38 |  2294 | `		zErr = "Warning:  ";` |
|       38 |  2295 | `		break;` |
|        6 |  2296 | `	case PH7_CTX_NOTICE:` |
|       14 |  2297 | `		zErr = "Notice:  ";` |
|       12 |  2298 | `		break;` |
|       23 |  2299 | `	default:` |
|        - |  2300 | `		/* keep iErr unchanged */` |
|       46 |  2301 | `		break;` |
|        - |  2302 | `	}` |
|       96 |  2303 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       96 |  2304 | `	if( pFuncName ){` |
|        - |  2305 | `		/* Append function name first */` |
|       23 |  2306 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2307 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2308 | `	}` |
|       96 |  2309 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2310 | `	/* Check for user error handler.  compute length of C string */` |
|       96 |  2311 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       47 |  2312 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       23 |  2313 | `	}` |
|       96 |  2314 | `	return rc;` |
|       50 |  2315 |  |
|        - |  2316 | `/*` |
|        - |  2317 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2318 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2319 | ` * information.` |
|        - |  2320 | ` */` |
|       38 |  2321 | `static sxi32 VmThrowErrorAp(` |
|        - |  2322 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2323 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2324 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2325 | `	const char *zFormat, /* Format message */` |
|        - |  2326 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2327 | `	)` |
|        2 |  2328 |  |
|       40 |  2329 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2330 | `	SyBlob sMsg;` |
|        - |  2331 | `	SyString *pFile;` |
|        - |  2332 | `	char *zErr;` |
|       40 |  2333 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2334 | `	if( !pVm->bErrReport ){` |
|        - |  2335 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2336 | `		return SXRET_OK;` |
|        - |  2337 | `	}` |
|        - |  2338 | `	/* Reset the working buffer */` |
|       40 |  2339 | `	SyBlobReset(pWorker);` |
|        - |  2340 | `	/* Peek the processed file if available */` |
|       40 |  2341 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2342 | `	if( pFile ){` |
|        - |  2343 | `		/* Append file name */` |
|       40 |  2344 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2345 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2346 | `	}` |
|        - |  2347 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2348 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2349 | `	 * the correct errno value. */` |
|       40 |  2350 | `	zErr = "Error:  ";` |
|       40 |  2351 | `	switch(iErr){` |
|        4 |  2352 | `	case PH7_CTX_WARNING:` |
|        9 |  2353 | `		zErr = "Warning:  ";` |
|        9 |  2354 | `		break;` |
|        3 |  2355 | `	case PH7_CTX_NOTICE:` |
|        7 |  2356 | `		zErr = "Notice:  ";` |
|        6 |  2357 | `		break;` |
|       12 |  2358 | `	default:` |
|        - |  2359 | `		/* do not change iErr */` |
|       24 |  2360 | `		break;` |
|        - |  2361 | `	}` |
|       40 |  2362 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2363 | `	if( pFuncName ){` |
|        - |  2364 | `		/* Append function name first */` |
|       26 |  2365 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2366 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2367 | `	}` |
|        - |  2368 | `	/* Format the raw message */` |
|       40 |  2369 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2370 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2371 | `	/* Check if a user error handler is installed */` |
|       40 |  2372 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2373 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2374 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2375 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2376 | `	}` |
|       40 |  2377 | `	SyBlobRelease(&sMsg);` |
|       40 |  2378 | `	return rc;` |
|       21 |  2379 |  |
|        - |  2380 | `/*` |
|        - |  2381 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2382 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2383 | ` * information.` |
|        - |  2384 | ` * ------------------------------------` |
|        - |  2385 | ` * Simple boring wrapper function.` |
|        - |  2386 | ` * ------------------------------------` |
|        - |  2387 | ` */` |
|       14 |  2388 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2389 |  |
|        - |  2390 | `	va_list ap;` |
|        - |  2391 | `	sxi32 rc;` |
|       15 |  2392 | `	va_start(ap,zFormat);` |
|       15 |  2393 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2394 | `	va_end(ap);` |
|       15 |  2395 | `	return rc;` |
|        1 |  2396 |  |
|        - |  2397 | `/*` |
|        - |  2398 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2399 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2400 | ` * information.` |
|        - |  2401 | ` * ------------------------------------` |
|        - |  2402 | ` * Simple boring wrapper function.` |
|        - |  2403 | ` * ------------------------------------` |
|        - |  2404 | ` */` |
|       24 |  2405 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2406 |  |
|        - |  2407 | `	sxi32 rc;` |
|       26 |  2408 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2409 | `	return rc;` |
|        2 |  2410 |  |
|        - |  2411 | `/*` |
|        - |  2412 | ` * Resolve function context from the current frame.` |
|        - |  2413 | ` */` |
|      934 |  2414 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2415 |  |
|        - |  2416 | `	VmFrame *pFrame;` |
|        - |  2417 | `	ph7_vm_func *pFunc;` |
|      935 |  2418 | `	*pzFuncName = 0;` |
|      935 |  2419 | `	*pnFuncLen = 0;` |
|      935 |  2420 | `	pFrame = pVm->pFrame;` |
|      935 |  2421 | `	if( pFrame == 0 ){` |
|      ! 0 |  2422 | `		return;` |
|        - |  2423 | `	}` |
|      935 |  2424 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      935 |  2425 | `	if( pFrame->pParent == 0 ){` |
|      929 |  2426 | `		return;` |
|        - |  2427 | `	}` |
|        7 |  2428 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        7 |  2429 | `	if( pFunc == 0 ){` |
|      ! 0 |  2430 | `		return;` |
|        - |  2431 | `	}` |
|        7 |  2432 | `	*pzFuncName = pFunc->sName.zString;` |
|        7 |  2433 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      468 |  2434 |  |
|        - |  2435 | `/*` |
|        - |  2436 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2437 | ` */` |
|      470 |  2438 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2439 |  |
|        - |  2440 | `	SyBlob sOut;` |
|        - |  2441 | `	SyString *pFile;` |
|      471 |  2442 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2443 | `		return PH7_OK;` |
|        - |  2444 | `	}` |
|      471 |  2445 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2446 | `		zClass = "Exception";` |
|      ! 0 |  2447 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2448 | `	}` |
|      471 |  2449 | `	if( zMsg == 0 ){` |
|      ! 0 |  2450 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2451 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2452 | `	}` |
|      471 |  2453 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      465 |  2454 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      232 |  2455 | `	}` |
|      471 |  2456 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      471 |  2457 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      471 |  2458 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      471 |  2459 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      471 |  2460 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      471 |  2461 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      471 |  2462 | `	if( pFile ){` |
|      471 |  2463 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      471 |  2464 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2465 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      235 |  2466 | `	}` |
|      471 |  2467 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      471 |  2468 | `	if( pFile ){` |
|      471 |  2469 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      471 |  2470 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2471 | `		if( zFuncName && nFuncLen > 0 ){` |
|        7 |  2472 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        4 |  2473 | `		}else{` |
|      465 |  2474 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2475 | `		}` |
|      235 |  2476 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2477 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2478 | `	}else{` |
|      ! 0 |  2479 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2480 | `	}` |
|      471 |  2481 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      471 |  2482 | `	if( pFile ){` |
|      471 |  2483 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      471 |  2484 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      471 |  2485 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2486 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      235 |  2487 | `	}` |
|      471 |  2488 | `	VmCallErrorHandler(pVm,&sOut);` |
|      471 |  2489 | `	SyBlobRelease(&sOut);` |
|      471 |  2490 | `	return PH7_ABORT;` |
|      236 |  2491 |  |
|        - |  2492 | `/*` |
|        - |  2493 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2494 | ` */` |
|      468 |  2495 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2496 |  |
|        - |  2497 | `	ph7_vm *pVm;` |
|        - |  2498 | `	ph7_class *pClass;` |
|        - |  2499 | `	ph7_class_instance *pThis;` |
|        - |  2500 | `	ph7_class_method *pCons;` |
|        - |  2501 | `	ph7_value sArg;` |
|        - |  2502 | `	ph7_value *apArg[1];` |
|        - |  2503 | `	SyBlob sMsg;` |
|        - |  2504 | `	SyString sMsgStr;` |
|        - |  2505 | `	VmFrame *pFrame;` |
|        - |  2506 | `	va_list ap;` |
|        - |  2507 | `	sxi32 rc;` |
|        - |  2508 |  |
|      470 |  2509 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2510 | `		return PH7_ABORT;` |
|        - |  2511 | `	}` |
|      470 |  2512 | `	pVm = pCtx->pVm;` |
|      470 |  2513 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2514 | `		zClass = "Error";` |
|      ! 0 |  2515 | `	}` |
|      470 |  2516 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      470 |  2517 | `	if( pClass == 0 ){` |
|      ! 0 |  2518 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2519 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2520 | `			zClass` |
|        - |  2521 | `			);` |
|        - |  2522 | `	}` |
|      470 |  2523 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      470 |  2524 | `	if( pThis == 0 ){` |
|      ! 0 |  2525 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2526 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2527 | `			);` |
|        - |  2528 | `	}` |
|        - |  2529 |  |
|      470 |  2530 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      470 |  2531 | `	va_start(ap,zFormat);` |
|      470 |  2532 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      470 |  2533 | `	va_end(ap);` |
|        - |  2534 |  |
|      470 |  2535 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      470 |  2536 | `	if( pCons ){` |
|      470 |  2537 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      470 |  2538 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      470 |  2539 | `		apArg[0] = &sArg;` |
|      470 |  2540 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      470 |  2541 | `		PH7_MemObjRelease(&sArg);` |
|      234 |  2542 | `	}` |
|      470 |  2543 | `	SyBlobRelease(&sMsg);` |
|        - |  2544 |  |
|      470 |  2545 | `	pFrame = pVm->pFrame;` |
|      470 |  2546 | `	if( pFrame ){` |
|      470 |  2547 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      470 |  2548 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      234 |  2549 | `	}` |
|      470 |  2550 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      470 |  2551 | `	PH7_ClassInstanceUnref(pThis);` |
|      470 |  2552 | `	if( rc == SXERR_ABORT ){` |
|      463 |  2553 | `		return PH7_ABORT;` |
|        - |  2554 | `	}` |
|        7 |  2555 | `	return PH7_EXCEPTION;` |
|      236 |  2556 |  |
|        - |  2557 | `/*` |
|        - |  2558 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2559 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2560 | ` */` |
|      ! 0 |  2561 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2562 |  |
|        - |  2563 | `	ph7_vm *pVm;` |
|        - |  2564 | `	SyBlob sMsg;` |
|      ! 0 |  2565 | `	const char *zFuncName = 0;` |
|      ! 0 |  2566 | `	int nFuncLen = 0;` |
|        - |  2567 | `	va_list ap;` |
|        - |  2568 | `	sxi32 rc;` |
|        - |  2569 |  |
|      ! 0 |  2570 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2571 | `		return PH7_OK;` |
|        - |  2572 | `	}` |
|      ! 0 |  2573 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2574 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2575 | `		zClass = "Error";` |
|      ! 0 |  2576 | `	}` |
|        - |  2577 |  |
|      ! 0 |  2578 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2579 |  |
|      ! 0 |  2580 | `	va_start(ap,zFormat);` |
|      ! 0 |  2581 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2582 | `	va_end(ap);` |
|        - |  2583 |  |
|      ! 0 |  2584 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2585 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2586 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2587 | `	}` |
|      ! 0 |  2588 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2589 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2590 | `	}` |
|      ! 0 |  2591 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2592 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2593 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2594 | `	return rc;` |
|      ! 0 |  2595 |  |
|        - |  2596 | `/*` |
|        - |  2597 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2598 | ` *` |
|        - |  2599 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2600 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2601 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2602 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2603 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2604 | ` * then the program execution is halted.` |
|        - |  2605 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2606 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2607 | ` * or to reset the VM to it's initial state.` |
|        - |  2608 | ` */` |
|    31520 |  2609 | `static sxi32 VmByteCodeExec(` |
|        - |  2610 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2611 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2612 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2613 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2614 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2615 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2616 | `	int is_callback      /* TRUE if we are executing a callback */` |
|        - |  2617 | `	)` |
|        2 |  2618 |  |
|        - |  2619 | `	VmInstr *pInstr;` |
|        - |  2620 | `	ph7_value *pTos;` |
|        - |  2621 | `	SySet aArg;` |
|        - |  2622 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  2623 | `	sxi32 pc;` |
|        - |  2624 | `	sxi32 rc;` |
|        - |  2625 | `	/* Argument container */` |
|    31522 |  2626 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    31522 |  2627 | `	if( nTos < 0 ){` |
|    29778 |  2628 | `		pTos = &pStack[-1];` |
|    14890 |  2629 | `	}else{` |
|     1746 |  2630 | `		pTos = &pStack[nTos];` |
|        - |  2631 | `	}` |
|    31522 |  2632 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    31522 |  2633 | `	pc = 0;` |
|        - |  2634 | `	/* Execute as much as we can */` |
|  4914222 |  2635 | `	for(;;){` |
|        - |  2636 | `		/* Fetch the instruction to execute */` |
|  9827742 |  2637 | `		pInstr = &aInstr[pc];` |
|  9827742 |  2638 | `		rc = SXRET_OK;` |
|        - |  2639 | `/*` |
|        - |  2640 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2641 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2642 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2643 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2644 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2645 | ` */` |
|  9827742 |  2646 | `		switch(pInstr->iOp){` |
|        - |  2647 | `/*` |
|        - |  2648 | ` * DONE: P1 * *` |
|        - |  2649 | ` *` |
|        - |  2650 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2651 | ` * and return immediately.` |
|        - |  2652 | ` */` |
|    15517 |  2653 | `case PH7_OP_DONE:` |
|    31036 |  2654 | `	if( pInstr->iP1 ){` |
|        - |  2655 | `#ifdef UNTRUST` |
|        - |  2656 | `		if( pTos < pStack ){` |
|        - |  2657 | `			goto Abort;` |
|        - |  2658 | `		}` |
|        - |  2659 | `#endif` |
|    17866 |  2660 | `		if( pLastRef ){` |
|    11680 |  2661 | `			*pLastRef = pTos->nIdx;` |
|     5839 |  2662 | `		}` |
|    17866 |  2663 | `		if( pResult ){` |
|        - |  2664 | `			/* Execution result */` |
|    17012 |  2665 | `			PH7_MemObjStore(pTos,pResult);` |
|     8505 |  2666 | `		}` |
|    17866 |  2667 | `		VmPopOperand(&pTos,1);` |
|    22104 |  2668 | `	}else if( pLastRef ){` |
|        - |  2669 | `		/* Nothing referenced */` |
|      958 |  2670 | `		*pLastRef = SXU32_HIGH;` |
|      478 |  2671 | `	}` |
|        - |  2672 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  2673 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  2674 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  2675 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  2676 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  2677 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  2678 | `	 * block can override it.` |
|        - |  2679 | `	 */` |
|    31038 |  2680 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  2681 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  2682 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  2683 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  2684 | `		pExc->pFrame = 0;` |
|        3 |  2685 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  2686 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  2687 | `			pExc->iFinallyDone = 1;` |
|        - |  2688 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  2689 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  2690 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2691 | `				goto Abort;` |
|        - |  2692 | `			}` |
|        1 |  2693 | `		}` |
|        1 |  2694 | `	}` |
|    31036 |  2695 | `	goto Done;` |
|        - |  2696 | `/*` |
|        - |  2697 | ` * HALT: P1 * *` |
|        - |  2698 | ` *` |
|        - |  2699 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2700 | ` * and abort immediately.` |
|        - |  2701 | ` */` |
|        4 |  2702 | `case PH7_OP_HALT:` |
|        9 |  2703 | `	if( pInstr->iP1 ){` |
|        - |  2704 | `#ifdef UNTRUST` |
|        - |  2705 | `		if( pTos < pStack ){` |
|        - |  2706 | `			goto Abort;` |
|        - |  2707 | `		}` |
|        - |  2708 | `#endif` |
|        9 |  2709 | `		if( pLastRef ){` |
|      ! 0 |  2710 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2711 | `		}` |
|        9 |  2712 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2713 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2714 | `				/* Output the exit message */` |
|        7 |  2715 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2716 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2717 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  2718 | `			}` |
|        7 |  2719 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2720 | `			/* Record exit status */` |
|        5 |  2721 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2722 | `		}` |
|        9 |  2723 | `		VmPopOperand(&pTos,1);` |
|        4 |  2724 | `	}else if( pLastRef ){` |
|        - |  2725 | `		/* Nothing referenced */` |
|      ! 0 |  2726 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2727 | `	}` |
|        - |  2728 | `	/* Check if we're in an included file context */` |
|        9 |  2729 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2730 | `		/* Terminate the entire process */` |
|        9 |  2731 | `		exit(pVm->iExitStatus);` |
|        - |  2732 | `	}` |
|      ! 0 |  2733 | `	goto Abort;` |
|        - |  2734 | `/*` |
|        - |  2735 | ` * JMP: * P2 *` |
|        - |  2736 | ` *` |
|        - |  2737 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2738 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2739 | ` */` |
|   211902 |  2740 | `case PH7_OP_JMP:` |
|   423850 |  2741 | `	pc = pInstr->iP2 - 1;` |
|   423850 |  2742 | `	break;` |
|        - |  2743 | `/*` |
|        - |  2744 | ` * JZ: P1 P2 *` |
|        - |  2745 | ` *` |
|        - |  2746 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2747 | ` * entry in the stack if P1 is zero.` |
|        - |  2748 | ` */` |
|   494776 |  2749 | `case PH7_OP_JZ:` |
|        - |  2750 | `#ifdef UNTRUST` |
|        - |  2751 | `	if( pTos < pStack ){` |
|        - |  2752 | `		goto Abort;` |
|        - |  2753 | `	}` |
|        - |  2754 | `#endif` |
|        - |  2755 | `	/* Get a boolean value */` |
|   989642 |  2756 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      138 |  2757 | `		PH7_MemObjToBool(pTos);` |
|       68 |  2758 | `	}` |
|   989642 |  2759 | `	if( !pTos->x.iVal ){` |
|        - |  2760 | `		/* Take the jump */` |
|   498920 |  2761 | `		pc = pInstr->iP2 - 1;` |
|   249459 |  2762 | `	}` |
|   989642 |  2763 | `	if( !pInstr->iP1 ){` |
|   788872 |  2764 | `		VmPopOperand(&pTos,1);` |
|   394457 |  2765 | `	}` |
|   989642 |  2766 | `	break;` |
|        - |  2767 | `/*` |
|        - |  2768 | ` * JNZ: P1 P2 *` |
|        - |  2769 | ` *` |
|        - |  2770 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2771 | ` * entry in the stack if P1 is zero.` |
|        - |  2772 | ` */` |
|    53399 |  2773 | `case PH7_OP_JNZ:` |
|        - |  2774 | `#ifdef UNTRUST` |
|        - |  2775 | `	if( pTos < pStack ){` |
|        - |  2776 | `		goto Abort;` |
|        - |  2777 | `	}` |
|        - |  2778 | `#endif` |
|        - |  2779 | `	/* Get a boolean value */` |
|   106800 |  2780 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2781 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2782 | `	}` |
|   106800 |  2783 | `	if( pTos->x.iVal ){` |
|        - |  2784 | `		/* Take the jump */` |
|     4394 |  2785 | `		pc = pInstr->iP2 - 1;` |
|     2196 |  2786 | `	}` |
|   106800 |  2787 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2788 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2789 | `	}` |
|   106800 |  2790 | `	break;` |
|        - |  2791 | `/*` |
|        - |  2792 | ` * NOOP: * * *` |
|        - |  2793 | ` *` |
|        - |  2794 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2795 | ` * destination.` |
|        - |  2796 | ` */` |
|      ! 0 |  2797 | `case PH7_OP_NOOP:` |
|      ! 0 |  2798 | `	break;` |
|        - |  2799 | `/*` |
|        - |  2800 | ` * POP: P1 * *` |
|        - |  2801 | ` *` |
|        - |  2802 | ` * Pop P1 elements from the operand stack.` |
|        - |  2803 | ` */` |
|   386746 |  2804 | `case PH7_OP_POP: {` |
|   773538 |  2805 | `	sxi32 n = pInstr->iP1;` |
|   773538 |  2806 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2807 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2808 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2809 | `	}` |
|   773538 |  2810 | `	VmPopOperand(&pTos,n);` |
|   773538 |  2811 | `	break;` |
|        - |  2812 | `				 }` |
|        - |  2813 | `/*` |
|        - |  2814 | ` * DUP: * * *` |
|        - |  2815 | ` *` |
|        - |  2816 | ` * Duplicate the top of the stack.` |
|        - |  2817 | ` */` |
|       35 |  2818 | `case PH7_OP_DUP:` |
|        - |  2819 | `#ifdef UNTRUST` |
|        - |  2820 | `	if( pTos < pStack ){` |
|        - |  2821 | `		goto Abort;` |
|        - |  2822 | `	}` |
|        - |  2823 | `#endif` |
|       72 |  2824 | `	pTos++;` |
|       72 |  2825 | `	PH7_MemObjInit(pVm,pTos);` |
|       72 |  2826 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       72 |  2827 | `	break;` |
|        - |  2828 | `/*` |
|        - |  2829 | ` * NSSWITCH: * * P3` |
|        - |  2830 | ` *` |
|        - |  2831 | ` * Switch the active namespace at runtime.` |
|        - |  2832 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  2833 | ` */` |
|     6347 |  2834 | `case PH7_OP_NSSWITCH:` |
|    12696 |  2835 | `	SyBlobReset(&pVm->sNamespace);` |
|    12696 |  2836 | `	if( pInstr->p3 ){` |
|       51 |  2837 | `		const char *zNs = (const char *)pInstr->p3;` |
|       51 |  2838 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       25 |  2839 | `	}` |
|    12696 |  2840 | `	break;` |
|        - |  2841 | `/*` |
|        - |  2842 | ` * CVT_INT: * * *` |
|        - |  2843 | ` *` |
|        - |  2844 | ` * Force the top of the stack to be an integer.` |
|        - |  2845 | ` */` |
|       35 |  2846 | `case PH7_OP_CVT_INT:` |
|        - |  2847 | `#ifdef UNTRUST` |
|        - |  2848 | `	if( pTos < pStack ){` |
|        - |  2849 | `		goto Abort;` |
|        - |  2850 | `	}` |
|        - |  2851 | `#endif` |
|       72 |  2852 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2853 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2854 | `	}` |
|        - |  2855 | `	/* Invalidate any prior representation */` |
|       72 |  2856 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2857 | `	break;` |
|        - |  2858 | `/*` |
|        - |  2859 | ` * CVT_REAL: * * *` |
|        - |  2860 | ` *` |
|        - |  2861 | ` * Force the top of the stack to be a real.` |
|        - |  2862 | ` */` |
|        4 |  2863 | `case PH7_OP_CVT_REAL:` |
|        - |  2864 | `#ifdef UNTRUST` |
|        - |  2865 | `	if( pTos < pStack ){` |
|        - |  2866 | `		goto Abort;` |
|        - |  2867 | `	}` |
|        - |  2868 | `#endif` |
|        9 |  2869 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2870 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2871 | `	}` |
|        - |  2872 | `	/* Invalidate any prior representation */` |
|        9 |  2873 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2874 | `	break;` |
|        - |  2875 | `/*` |
|        - |  2876 | ` * CVT_STR: * * *` |
|        - |  2877 | ` *` |
|        - |  2878 | ` * Force the top of the stack to be a string.` |
|        - |  2879 | ` */` |
|      146 |  2880 | `case PH7_OP_CVT_STR:` |
|        - |  2881 | `#ifdef UNTRUST` |
|        - |  2882 | `	if( pTos < pStack ){` |
|        - |  2883 | `		goto Abort;` |
|        - |  2884 | `	}` |
|        - |  2885 | `#endif` |
|      294 |  2886 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  2887 | `		PH7_MemObjToString(pTos);` |
|      146 |  2888 | `	}` |
|      294 |  2889 | `	break;` |
|        - |  2890 | `/*` |
|        - |  2891 | ` * CVT_BOOL: * * *` |
|        - |  2892 | ` *` |
|        - |  2893 | ` * Force the top of the stack to be a boolean.` |
|        - |  2894 | ` */` |
|        5 |  2895 | `case PH7_OP_CVT_BOOL:` |
|        - |  2896 | `#ifdef UNTRUST` |
|        - |  2897 | `	if( pTos < pStack ){` |
|        - |  2898 | `		goto Abort;` |
|        - |  2899 | `	}` |
|        - |  2900 | `#endif` |
|       11 |  2901 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2902 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2903 | `	}` |
|       11 |  2904 | `	break;` |
|        - |  2905 | `/*` |
|        - |  2906 | ` * CVT_NULL: * * *` |
|        - |  2907 | ` *` |
|        - |  2908 | ` * Nullify the top of the stack.` |
|        - |  2909 | ` */` |
|        3 |  2910 | `case PH7_OP_CVT_NULL:` |
|        - |  2911 | `#ifdef UNTRUST` |
|        - |  2912 | `	if( pTos < pStack ){` |
|        - |  2913 | `		goto Abort;` |
|        - |  2914 | `	}` |
|        - |  2915 | `#endif` |
|        7 |  2916 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2917 | `	break;` |
|        - |  2918 | `/*` |
|        - |  2919 | ` * CVT_NUMC: * * *` |
|        - |  2920 | ` *` |
|        - |  2921 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2922 | ` */` |
|      ! 0 |  2923 | `case PH7_OP_CVT_NUMC:` |
|        - |  2924 | `#ifdef UNTRUST` |
|        - |  2925 | `	if( pTos < pStack ){` |
|        - |  2926 | `		goto Abort;` |
|        - |  2927 | `	}` |
|        - |  2928 | `#endif` |
|        - |  2929 | `	/* Force a numeric cast */` |
|      ! 0 |  2930 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2931 | `	break;` |
|        - |  2932 | `/*` |
|        - |  2933 | ` * CVT_ARRAY: * * *` |
|        - |  2934 | ` *` |
|        - |  2935 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2936 | ` */` |
|       10 |  2937 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2938 | `#ifdef UNTRUST` |
|        - |  2939 | `	if( pTos < pStack ){` |
|        - |  2940 | `		goto Abort;` |
|        - |  2941 | `	}` |
|        - |  2942 | `#endif` |
|        - |  2943 | `	/* Force a hashmap cast */` |
|       21 |  2944 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  2945 | `	if( rc != SXRET_OK ){` |
|        - |  2946 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  2947 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  2948 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  2949 | `	}` |
|       21 |  2950 | `	break;` |
|        - |  2951 | `/*` |
|        - |  2952 | ` * CVT_OBJ: * * *` |
|        - |  2953 | ` *` |
|        - |  2954 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  2955 | ` */` |
|        8 |  2956 | `case PH7_OP_CVT_OBJ:` |
|        - |  2957 | `#ifdef UNTRUST` |
|        - |  2958 | `	if( pTos < pStack ){` |
|        - |  2959 | `		goto Abort;` |
|        - |  2960 | `	}` |
|        - |  2961 | `#endif` |
|       17 |  2962 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  2963 | `		/* Force a 'stdClass()' cast */` |
|       17 |  2964 | `		PH7_MemObjToObject(pTos);` |
|        8 |  2965 | `	}` |
|       17 |  2966 | `	break;` |
|        - |  2967 | `/*` |
|        - |  2968 | ` * ERR_CTRL * * *` |
|        - |  2969 | ` *` |
|        - |  2970 | ` * Error control operator.` |
|        - |  2971 | ` */` |
|    12584 |  2972 | `case PH7_OP_ERR_CTRL:` |
|        - |  2973 | `	/*` |
|        - |  2974 | `	 * TICKET 1433-038:` |
|        - |  2975 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2976 | `	 * use the public API,to control error output.` |
|        - |  2977 | `	 */` |
|    25168 |  2978 | `	break;` |
|        - |  2979 | `/*` |
|        - |  2980 | ` * IS_A * * *` |
|        - |  2981 | ` *` |
|        - |  2982 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  2983 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  2984 | ` * holding a class name or an object).` |
|        - |  2985 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  2986 | ` */` |
|       23 |  2987 | `case PH7_OP_IS_A:{` |
|       48 |  2988 | `	ph7_value *pNos = &pTos[-1];` |
|       48 |  2989 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  2990 | `#ifdef UNTRUST` |
|        - |  2991 | `	if( pNos < pStack ){` |
|        - |  2992 | `		goto Abort;` |
|        - |  2993 | `	}` |
|        - |  2994 | `#endif` |
|       48 |  2995 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       46 |  2996 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       46 |  2997 | `		ph7_class *pClass = 0;` |
|        - |  2998 | `		/* Extract the target class */` |
|       46 |  2999 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  3000 | `			/* Instance already loaded */` |
|      ! 0 |  3001 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       46 |  3002 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       46 |  3003 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       46 |  3004 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  3005 | `			/* Handle self/static/parent keywords */` |
|       46 |  3006 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  3007 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       44 |  3008 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  3009 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       43 |  3010 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  3011 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  3012 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  3013 | `					pClass = pSelf->pBase;` |
|        2 |  3014 | `				}` |
|        3 |  3015 | `			}else{` |
|       36 |  3016 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  3017 | `			}` |
|       22 |  3018 | `		}` |
|       46 |  3019 | `		if( pClass ){` |
|        - |  3020 | `			/* Perform the query */` |
|       46 |  3021 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       22 |  3022 | `		}` |
|       22 |  3023 | `	}` |
|        - |  3024 | `	/* Push result */` |
|       48 |  3025 | `	VmPopOperand(&pTos,1);` |
|       48 |  3026 | `	PH7_MemObjRelease(pTos);` |
|       48 |  3027 | `	pTos->x.iVal = iRes;` |
|       48 |  3028 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       48 |  3029 | `	break;` |
|        - |  3030 | `				 }` |
|        - |  3031 |  |
|        - |  3032 | `/*` |
|        - |  3033 | ` * LOADC P1 P2 *` |
|        - |  3034 | ` *` |
|        - |  3035 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  3036 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  3037 | ` */` |
|   818393 |  3038 | `case PH7_OP_LOADC: {` |
|        - |  3039 | `	ph7_value *pObj;` |
|        - |  3040 | `	/* Reserve a room */` |
|  1636832 |  3041 | `	pTos++;` |
|  2447204 |  3042 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1636832 |  3043 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3044 | `			SyHashEntry *pEntry;` |
|        - |  3045 | `			/* Candidate for expansion via user defined callbacks */` |
|    16134 |  3046 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16134 |  3047 | `			if( pEntry ){` |
|    16130 |  3048 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3049 | `				/* Set a NULL default value */` |
|    16130 |  3050 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    16130 |  3051 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3052 | `				/* Invoke the callback and deal with the expanded value */` |
|    16130 |  3053 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3054 | `				/* Mark as constant */` |
|    16130 |  3055 | `				pTos->nIdx = SXU32_HIGH;` |
|    16130 |  3056 | `				break;` |
|        - |  3057 | `			}` |
|        - |  3058 | `			/* Constant not found.  For qualified names (containing '\')` |
|        - |  3059 | `			 * this is always an error — bare unqualified names still fall` |
|        - |  3060 | `			 * through to string value for backward compatibility. */` |
|        - |  3061 | `			{` |
|        6 |  3062 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  3063 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  3064 | `				sxu32 j;` |
|       32 |  3065 | `				for( j = 0; j < nLit; j++ ){` |
|       30 |  3066 | `					if( zLit[j] == '\\' ){` |
|        - |  3067 | `						/* Qualified name: must be a real constant.` |
|        - |  3068 | `						 * Format as PHP Fatal error to match PHP behavior. */` |
|        - |  3069 | `						{` |
|        3 |  3070 | `							SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  3071 | `							SyBlob sErr;` |
|        3 |  3072 | `							SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  3073 | `							SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  3074 | `							if( pErrFile ){` |
|        3 |  3075 | `								SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  3076 | `							}` |
|        3 |  3077 | `							SyBlobAppend(&sErr,"\n",1);` |
|        3 |  3078 | `							VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3079 | `							SyBlobRelease(&sErr);` |
|        - |  3080 | `						}` |
|        3 |  3081 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3082 | `						pTos->nIdx = SXU32_HIGH;` |
|        3 |  3083 | `						goto LoadC_Done;` |
|        - |  3084 | `					}` |
|       15 |  3085 | `				}` |
|        - |  3086 | `			}` |
|        1 |  3087 | `		}` |
|  1620702 |  3088 | `		PH7_MemObjLoad(pObj,pTos);` |
|   810374 |  3089 | `	}else{` |
|        - |  3090 | `		/* Set a NULL value */` |
|      ! 0 |  3091 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3092 | `	}` |
|   810329 |  3093 | `LoadC_Done:` |
|        - |  3094 | `	/* Mark as constant */` |
|  1620704 |  3095 | `	pTos->nIdx = SXU32_HIGH;` |
|  1620704 |  3096 | `	break;` |
|        - |  3097 | `				  }` |
|        - |  3098 | `/*` |
|        - |  3099 | ` * LOAD: P1 * P3` |
|        - |  3100 | ` *` |
|        - |  3101 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3102 | ` * from the P3 operand.` |
|        - |  3103 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3104 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3105 | ` */` |
|  1336427 |  3106 | `case PH7_OP_LOAD:{` |
|        - |  3107 | `	ph7_value *pObj;` |
|        - |  3108 | `	SyString sName;` |
|  2673076 |  3109 | `	if( pInstr->p3 == 0 ){` |
|        - |  3110 | `		/* Take the variable name from the top of the stack */` |
|        - |  3111 | `#ifdef UNTRUST` |
|        - |  3112 | `		if( pTos < pStack ){` |
|        - |  3113 | `			goto Abort;` |
|        - |  3114 | `		}` |
|        - |  3115 | `#endif` |
|        - |  3116 | `		/* Force a string cast */` |
|       19 |  3117 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3118 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3119 | `		}` |
|       19 |  3120 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3121 | `	}else{` |
|  2673058 |  3122 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3123 | `		/* Reserve a room for the target object */` |
|  2673058 |  3124 | `		pTos++;` |
|        - |  3125 | `	}` |
|        - |  3126 | `	/* Extract the requested memory object */` |
|  2673076 |  3127 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2673076 |  3128 | `	if( pObj == 0 ){` |
|       26 |  3129 | `		if( pInstr->iP1 ){` |
|        - |  3130 | `			/* Variable not found,load NULL */` |
|       26 |  3131 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3132 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3133 | `			}else{` |
|       26 |  3134 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3135 | `			}` |
|       26 |  3136 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1336441 |  3137 | `			break;` |
|      ! 0 |  3138 | `		}else{` |
|        - |  3139 | `			/* Fatal error */` |
|      ! 0 |  3140 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3141 | `			goto Abort;` |
|        - |  3142 | `		}` |
|        - |  3143 | `	}` |
|        - |  3144 | `	/* Load variable contents */` |
|  2673052 |  3145 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2673052 |  3146 | `	pTos->nIdx = pObj->nIdx;` |
|  2673052 |  3147 | `	break;` |
|        - |  3148 | `				   }` |
|        - |  3149 | `/*` |
|        - |  3150 | ` * LOAD_MAP P1 * *` |
|        - |  3151 | ` *` |
|        - |  3152 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3153 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3154 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3155 | ` */` |
|    18191 |  3156 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3157 | `	ph7_hashmap *pMap;` |
|        - |  3158 | `	/* Allocate a new hashmap instance */` |
|    36384 |  3159 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    36384 |  3160 | `	if( pMap == 0 ){` |
|      ! 0 |  3161 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3162 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3163 | `		goto Abort;` |
|        - |  3164 | `	}` |
|    36384 |  3165 | `	if( pInstr->iP1 > 0 ){` |
|     2238 |  3166 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3167 | `		/* Perform the insertion */` |
|     6838 |  3168 | `		while( pEntry < pTos ){` |
|     4602 |  3169 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3170 | `				/* Insertion by reference */` |
|      142 |  3171 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3172 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3173 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3174 | `					);` |
|       48 |  3175 | `			}else{` |
|        - |  3176 | `				/* Standard insertion */` |
|     6761 |  3177 | `				PH7_HashmapInsert(pMap,` |
|     4506 |  3178 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2253 |  3179 | `					&pEntry[1]` |
|        - |  3180 | `				);` |
|        - |  3181 | `			}` |
|        - |  3182 | `			/* Next pair on the stack */` |
|     4602 |  3183 | `			pEntry += 2;` |
|        2 |  3184 | `		}` |
|        - |  3185 | `		/* Pop P1 elements */` |
|     2238 |  3186 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1118 |  3187 | `	}` |
|        - |  3188 | `	/* Push the hashmap */` |
|    36384 |  3189 | `	pTos++;` |
|    36384 |  3190 | `	pTos->nIdx = SXU32_HIGH;` |
|    36384 |  3191 | `	pTos->x.pOther = pMap;` |
|    36384 |  3192 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    36384 |  3193 | `	break;` |
|        - |  3194 | `					  }` |
|        - |  3195 | `/*` |
|        - |  3196 | ` * LOAD_LIST: P1 * *` |
|        - |  3197 | ` *` |
|        - |  3198 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3199 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3200 | ` * Caveats:` |
|        - |  3201 | ` *  This implementation support only a single nesting level.` |
|        - |  3202 | ` */` |
|       26 |  3203 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3204 | `	ph7_value *pEntry;` |
|       54 |  3205 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3206 | `		/* Empty list,break immediately */` |
|      ! 0 |  3207 | `		break;` |
|        - |  3208 | `	}` |
|       54 |  3209 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3210 | `#ifdef UNTRUST` |
|        - |  3211 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3212 | `		goto Abort;` |
|        - |  3213 | `	}` |
|        - |  3214 | `#endif` |
|       54 |  3215 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       50 |  3216 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3217 | `		ph7_hashmap_node *pNode;` |
|        - |  3218 | `		ph7_value sKey,*pObj;` |
|        - |  3219 | `		/* Start Copying */` |
|       50 |  3220 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      154 |  3221 | `		while( pEntry <= pTos ){` |
|      106 |  3222 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       98 |  3223 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       98 |  3224 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       98 |  3225 | `					if( rc == SXRET_OK ){` |
|        - |  3226 | `						/* Store node value */` |
|       98 |  3227 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       50 |  3228 | `					}else{` |
|        - |  3229 | `						/* Nullify the variable */` |
|      ! 0 |  3230 | `						PH7_MemObjRelease(pObj);` |
|        - |  3231 | `					}` |
|       48 |  3232 | `				}` |
|       48 |  3233 | `			}` |
|      106 |  3234 | `			sKey.x.iVal++; /* Next numeric index */` |
|      106 |  3235 | `			pEntry++;` |
|        2 |  3236 | `		}` |
|       24 |  3237 | `	}` |
|       54 |  3238 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       54 |  3239 | `	break;` |
|        - |  3240 | `					   }` |
|        - |  3241 | `/*` |
|        - |  3242 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3243 | ` *` |
|        - |  3244 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3245 | ` * from the stack.` |
|        - |  3246 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3247 | ` * instead.` |
|        - |  3248 | ` */` |
|   215584 |  3249 | `case PH7_OP_LOAD_IDX: {` |
|   431214 |  3250 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   431214 |  3251 | `	ph7_hashmap *pMap = 0;` |
|        - |  3252 | `	ph7_value *pIdx;` |
|   431214 |  3253 | `	pIdx = 0;` |
|   431214 |  3254 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3255 | `		if( !pInstr->iP2){` |
|        - |  3256 | `			/* No available index,load NULL */` |
|      ! 0 |  3257 | `			if( pTos >= pStack ){` |
|      ! 0 |  3258 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3259 | `			}else{` |
|        - |  3260 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3261 | `				pTos++;` |
|      ! 0 |  3262 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3263 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3264 | `			}` |
|        - |  3265 | `			/* Emit a notice */` |
|      ! 0 |  3266 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3267 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3268 | `			break;` |
|        - |  3269 | `		}` |
|      ! 0 |  3270 | `	}else{` |
|   431214 |  3271 | `		pIdx = pTos;` |
|   431214 |  3272 | `		pTos--;` |
|        - |  3273 | `	}` |
|   431214 |  3274 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3275 | `		/* String access */` |
|   340922 |  3276 | `		if( pIdx ){` |
|        - |  3277 | `			sxu32 nOfft;` |
|   340922 |  3278 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3279 | `				/* Force an int cast */` |
|      ! 0 |  3280 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3281 | `			}` |
|   340922 |  3282 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   340922 |  3283 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3284 | `				/* Invalid offset,load null */` |
|      ! 0 |  3285 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3286 | `			}else{` |
|   340922 |  3287 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   340922 |  3288 | `				int c = zData[nOfft];` |
|   340922 |  3289 | `				PH7_MemObjRelease(pTos);` |
|   340922 |  3290 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   340922 |  3291 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3292 | `			}` |
|   170484 |  3293 | `		}else{` |
|        - |  3294 | `			/* No available index,load NULL */` |
|      ! 0 |  3295 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3296 | `		}` |
|   340922 |  3297 | `		break;` |
|        - |  3298 | `	}` |
|    90294 |  3299 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3300 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3301 | `			ph7_value *pObj;` |
|      ! 0 |  3302 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3303 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3304 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3305 | `			}` |
|      ! 0 |  3306 | `		}` |
|      ! 0 |  3307 | `	}` |
|    90294 |  3308 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    90294 |  3309 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|    90294 |  3310 | `		if( pInstr->iP2 ){` |
|        - |  3311 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  3312 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  3313 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  3314 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      875 |  3315 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      437 |  3316 | `		}` |
|        - |  3317 | `		/* Point to the hashmap */` |
|    90294 |  3318 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    90294 |  3319 | `		if( pIdx ){` |
|        - |  3320 | `			/* Load the desired entry */` |
|    90294 |  3321 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    45146 |  3322 | `		}` |
|    90294 |  3323 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3324 | `			/* Create a new empty entry */` |
|      265 |  3325 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      265 |  3326 | `			if( rc == SXRET_OK ){` |
|        - |  3327 | `				/* Point to the last inserted entry */` |
|      265 |  3328 | `				pNode = pMap->pLast;` |
|      132 |  3329 | `			}` |
|      132 |  3330 | `		}` |
|    45146 |  3331 | `	}` |
|    90294 |  3332 | `	if( pIdx ){` |
|    90294 |  3333 | `		PH7_MemObjRelease(pIdx);` |
|    45146 |  3334 | `	}` |
|    90294 |  3335 | `	if( rc == SXRET_OK ){` |
|        - |  3336 | `		/* Load entry contents */` |
|    41346 |  3337 | `		if( pMap->iRef < 2 ){` |
|        - |  3338 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3339 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3340 | `			 */` |
|       24 |  3341 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3342 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3343 | `		}else{` |
|    41324 |  3344 | `			pTos->nIdx = pNode->nValIdx;` |
|    41324 |  3345 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    41324 |  3346 | `			PH7_HashmapUnref(pMap);` |
|        - |  3347 | `		}` |
|    20674 |  3348 | `	}else{` |
|        - |  3349 | `		/* No such entry,load NULL */` |
|    48950 |  3350 | `		PH7_MemObjRelease(pTos);` |
|    48950 |  3351 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3352 | `	}` |
|    90294 |  3353 | `	break;` |
|        - |  3354 | `					  }` |
|        - |  3355 | `/*` |
|        - |  3356 | ` * LOAD_CLOSURE * * P3` |
|        - |  3357 | ` *` |
|        - |  3358 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3359 | ` * name in the stack.` |
|        - |  3360 | ` */` |
|        3 |  3361 | `case PH7_OP_LOAD_CLOSURE:{` |
|        7 |  3362 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        7 |  3363 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3364 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3365 | `		ph7_vm_func *pClosure;` |
|        - |  3366 | `		char *zName;` |
|        - |  3367 | `		sxu32 mLen;` |
|        - |  3368 | `		sxu32 n;` |
|        - |  3369 | `		/* Create a new VM function */` |
|        7 |  3370 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3371 | `		/* Generate an unique closure name */` |
|        7 |  3372 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        7 |  3373 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3374 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3375 | `			goto Abort;` |
|        - |  3376 | `		}` |
|        7 |  3377 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        7 |  3378 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3379 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3380 | `		}` |
|        - |  3381 | `		/* Zero the stucture */` |
|        7 |  3382 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3383 | `		/* Perform a structure assignment on read-only items */` |
|        7 |  3384 | `		pClosure->aArgs = pFunc->aArgs;` |
|        7 |  3385 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        7 |  3386 | `		pClosure->aStatic = pFunc->aStatic;` |
|        7 |  3387 | `		pClosure->iFlags = pFunc->iFlags;` |
|        7 |  3388 | `		pClosure->pUserData = pFunc->pUserData;` |
|        7 |  3389 | `		pClosure->sSignature = pFunc->sSignature;` |
|        7 |  3390 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3391 | `		/* Register the closure */` |
|        7 |  3392 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3393 | `		/* Set up closure environment */` |
|        7 |  3394 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        7 |  3395 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       19 |  3396 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3397 | `			ph7_value *pValue;` |
|       13 |  3398 | `			pEnv = &aEnv[n];` |
|       13 |  3399 | `			sEnv.sName  = pEnv->sName;` |
|       13 |  3400 | `			sEnv.iFlags = pEnv->iFlags;` |
|       13 |  3401 | `			sEnv.nIdx = SXU32_HIGH;` |
|       13 |  3402 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|       13 |  3403 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3404 | `				/* Pass by reference */` |
|      ! 0 |  3405 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3406 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3407 | `					);` |
|      ! 0 |  3408 | `			}` |
|        - |  3409 | `			/* Standard pass by value */` |
|       13 |  3410 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|       13 |  3411 | `			if( pValue ){` |
|        - |  3412 | `				/* Copy imported value */` |
|        7 |  3413 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        3 |  3414 | `			}` |
|        - |  3415 | `			/* Insert the imported variable */` |
|       13 |  3416 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        7 |  3417 | `		}` |
|        - |  3418 | `		/* Finally,load the closure name on the stack */` |
|        7 |  3419 | `		pTos++;` |
|        7 |  3420 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        3 |  3421 | `	}` |
|        7 |  3422 | `	break;` |
|        - |  3423 | `						 }` |
|        - |  3424 | `/*` |
|        - |  3425 | ` * STORE * P2 P3` |
|        - |  3426 | ` *` |
|        - |  3427 | ` * Perform a store (Assignment) operation.` |
|        - |  3428 | ` */` |
|   112239 |  3429 | `case PH7_OP_STORE: {` |
|        - |  3430 | `	ph7_value *pObj;` |
|        - |  3431 | `	SyString sName;` |
|        - |  3432 | `#ifdef UNTRUST` |
|        - |  3433 | `	if( pTos < pStack ){` |
|        - |  3434 | `		goto Abort;` |
|        - |  3435 | `	}` |
|        - |  3436 | `#endif` |
|   224480 |  3437 | `	if( pInstr->iP2 ){` |
|        - |  3438 | `		sxu32 nIdx;` |
|        - |  3439 | `		/* Member store operation */` |
|     2924 |  3440 | `		nIdx = pTos->nIdx;` |
|     2924 |  3441 | `		VmPopOperand(&pTos,1);` |
|     2924 |  3442 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3443 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3444 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3445 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3446 | `		}else{` |
|        - |  3447 | `			/* Point to the desired memory object */` |
|     2920 |  3448 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2920 |  3449 | `			if( pObj ){` |
|        - |  3450 | `				/* Perform the store operation */` |
|     2920 |  3451 | `				PH7_MemObjStore(pTos,pObj);` |
|     1459 |  3452 | `			}` |
|        - |  3453 | `		}` |
|   113702 |  3454 | `		break;` |
|   221558 |  3455 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3456 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3457 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3458 | `			/* Force a string cast */` |
|      ! 0 |  3459 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3460 | `		}` |
|        7 |  3461 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3462 | `		pTos--;` |
|        - |  3463 | `#ifdef UNTRUST` |
|        - |  3464 | `		if( pTos < pStack  ){` |
|        - |  3465 | `			goto Abort;` |
|        - |  3466 | `		}` |
|        - |  3467 | `#endif` |
|        4 |  3468 | `	}else{` |
|   221552 |  3469 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3470 | `	}` |
|        - |  3471 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   221558 |  3472 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   221558 |  3473 | `	if( pObj == 0 ){` |
|      ! 0 |  3474 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3475 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3476 | `		goto Abort;` |
|        - |  3477 | `	}` |
|   221558 |  3478 | `	if( !pInstr->p3 ){` |
|        7 |  3479 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3480 | `	}` |
|        - |  3481 | `	/* Perform the store operation */` |
|   221558 |  3482 | `	PH7_MemObjStore(pTos,pObj);` |
|   221558 |  3483 | `	break;` |
|        - |  3484 | `				   }` |
|        - |  3485 | `/*` |
|        - |  3486 | ` * STORE_IDX:   P1 * P3` |
|        - |  3487 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3488 | ` *` |
|        - |  3489 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3490 | ` */` |
|    81131 |  3491 | `case PH7_OP_STORE_IDX:` |
|        - |  3492 | `case PH7_OP_STORE_IDX_REF: {` |
|   162264 |  3493 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3494 | `	ph7_value *pKey;` |
|        - |  3495 | `	sxu32 nIdx;` |
|   162264 |  3496 | `	if( pInstr->iP1 ){` |
|        - |  3497 | `		/* Key is next on stack */` |
|    57206 |  3498 | `		pKey = pTos;` |
|    57206 |  3499 | `		pTos--;` |
|    28604 |  3500 | `	}else{` |
|   105060 |  3501 | `		pKey = 0;` |
|        - |  3502 | `	}` |
|   162264 |  3503 | `	nIdx = pTos->nIdx;` |
|   162264 |  3504 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3505 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  3506 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  3507 | `		 * checking true sharing count, then re-add after separation. */` |
|   162212 |  3508 | `		if( nIdx != SXU32_HIGH ){` |
|   162212 |  3509 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   243317 |  3510 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   162212 |  3511 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3512 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  3513 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  3514 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  3515 | `				 * refcounts if the backing array was already separated. */` |
|   162212 |  3516 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   162212 |  3517 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   162212 |  3518 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   162212 |  3519 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   162212 |  3520 | `					pTos->x.pOther = pMap;` |
|    81107 |  3521 | `				}else{` |
|        - |  3522 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  3523 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  3524 | `					pMap = pCur;` |
|        - |  3525 | `				}` |
|    81107 |  3526 | `			}else{` |
|      ! 0 |  3527 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3528 | `			}` |
|    81107 |  3529 | `		}else{` |
|      ! 0 |  3530 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3531 | `		}` |
|   162212 |  3532 | `		if( pMap->iRef < 2 ){` |
|        - |  3533 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  3534 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  3535 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  3536 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  3537 | `			pMap->iRef = 2;` |
|      ! 0 |  3538 | `		}` |
|    81107 |  3539 | `	}else{` |
|        - |  3540 | `		ph7_value *pObj;` |
|       53 |  3541 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3542 | `		if( pObj == 0 ){` |
|      ! 0 |  3543 | `			if( pKey ){` |
|      ! 0 |  3544 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3545 | `			}` |
|      ! 0 |  3546 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3547 | `			break;` |
|        - |  3548 | `		}` |
|        - |  3549 | `		/* Phase#1: Load the array */` |
|       53 |  3550 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3551 | `			VmPopOperand(&pTos,1);` |
|       53 |  3552 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3553 | `				/* Force a string cast */` |
|      ! 0 |  3554 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3555 | `			}` |
|       53 |  3556 | `			if( pKey == 0 ){` |
|        - |  3557 | `				/* Append string */` |
|        3 |  3558 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3559 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3560 | `				}` |
|        2 |  3561 | `			}else{` |
|        - |  3562 | `				sxu32 nOfft;` |
|       51 |  3563 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3564 | `					/* Force an int cast */` |
|       51 |  3565 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3566 | `				}` |
|       51 |  3567 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3568 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3569 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3570 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3571 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3572 | `				}else{` |
|      ! 0 |  3573 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3574 | `						/* Perform an append operation */` |
|      ! 0 |  3575 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3576 | `					}` |
|        - |  3577 | `				}` |
|        - |  3578 | `			}` |
|       53 |  3579 | `			if( pKey ){` |
|       51 |  3580 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3581 | `			}` |
|       53 |  3582 | `			break;` |
|      ! 0 |  3583 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3584 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3585 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3586 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3587 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3588 | `				goto Abort;` |
|        - |  3589 | `			}` |
|      ! 0 |  3590 | `		}` |
|        - |  3591 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  3592 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  3593 | `	}` |
|   162212 |  3594 | `	VmPopOperand(&pTos,1);` |
|        - |  3595 | `	/* Phase#2: Perform the insertion */` |
|   162212 |  3596 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3597 | `		/* Insertion by reference */` |
|       15 |  3598 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3599 | `	}else{` |
|   162198 |  3600 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3601 | `	}` |
|   162212 |  3602 | `	if( pKey ){` |
|    57156 |  3603 | `		PH7_MemObjRelease(pKey);` |
|    28577 |  3604 | `	}` |
|   162212 |  3605 | `	break;` |
|        - |  3606 | `					   }` |
|        - |  3607 | `/*` |
|        - |  3608 | ` * INCR: P1 * *` |
|        - |  3609 | ` *` |
|        - |  3610 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3611 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3612 | ` * the stack and increment after that.` |
|        - |  3613 | ` */` |
|   151331 |  3614 | `case PH7_OP_INCR:` |
|        - |  3615 | `#ifdef UNTRUST` |
|        - |  3616 | `	if( pTos < pStack ){` |
|        - |  3617 | `		goto Abort;` |
|        - |  3618 | `	}` |
|        - |  3619 | `#endif` |
|   302708 |  3620 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   302708 |  3621 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3622 | `			ph7_value *pObj;` |
|   302708 |  3623 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3624 | `				/* Force a numeric cast */` |
|   302708 |  3625 | `				PH7_MemObjToNumeric(pObj);` |
|   302708 |  3626 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3627 | `					pObj->rVal++;` |
|        - |  3628 | `					/* Try to get an integer representation */` |
|      ! 0 |  3629 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3630 | `				}else{` |
|   302708 |  3631 | `					pObj->x.iVal++;` |
|   302708 |  3632 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3633 | `				}` |
|   302708 |  3634 | `				if( pInstr->iP1 ){` |
|        - |  3635 | `					/* Pre-icrement */` |
|       71 |  3636 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3637 | `				}` |
|   151375 |  3638 | `			}` |
|   151377 |  3639 | `		}else{` |
|      ! 0 |  3640 | `			if( pInstr->iP1 ){` |
|        - |  3641 | `				/* Force a numeric cast */` |
|      ! 0 |  3642 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3643 | `				/* Pre-increment */` |
|      ! 0 |  3644 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3645 | `					pTos->rVal++;` |
|        - |  3646 | `					/* Try to get an integer representation */` |
|      ! 0 |  3647 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3648 | `				}else{` |
|      ! 0 |  3649 | `					pTos->x.iVal++;` |
|      ! 0 |  3650 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3651 | `				}` |
|      ! 0 |  3652 | `			}` |
|        - |  3653 | `		}` |
|   151375 |  3654 | `	}` |
|   302708 |  3655 | `	break;` |
|        - |  3656 | `/*` |
|        - |  3657 | ` * DECR: P1 * *` |
|        - |  3658 | ` *` |
|        - |  3659 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3660 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3661 | ` * and decrement after that.` |
|        - |  3662 | ` */` |
|        2 |  3663 | `case PH7_OP_DECR:` |
|        - |  3664 | `#ifdef UNTRUST` |
|        - |  3665 | `	if( pTos < pStack ){` |
|        - |  3666 | `		goto Abort;` |
|        - |  3667 | `	}` |
|        - |  3668 | `#endif` |
|        5 |  3669 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3670 | `		/* Force a numeric cast */` |
|        5 |  3671 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3672 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3673 | `			ph7_value *pObj;` |
|        5 |  3674 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3675 | `				/* Force a numeric cast */` |
|        5 |  3676 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3677 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3678 | `					pObj->rVal--;` |
|        - |  3679 | `					/* Try to get an integer representation */` |
|      ! 0 |  3680 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3681 | `				}else{` |
|        5 |  3682 | `					pObj->x.iVal--;` |
|        5 |  3683 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3684 | `				}` |
|        5 |  3685 | `				if( pInstr->iP1 ){` |
|        - |  3686 | `					/* Pre-icrement */` |
|      ! 0 |  3687 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3688 | `				}` |
|        2 |  3689 | `			}` |
|        3 |  3690 | `		}else{` |
|      ! 0 |  3691 | `			if( pInstr->iP1 ){` |
|        - |  3692 | `				/* Pre-increment */` |
|      ! 0 |  3693 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3694 | `					pTos->rVal--;` |
|        - |  3695 | `					/* Try to get an integer representation */` |
|      ! 0 |  3696 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3697 | `				}else{` |
|      ! 0 |  3698 | `					pTos->x.iVal--;` |
|      ! 0 |  3699 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3700 | `				}` |
|      ! 0 |  3701 | `			}` |
|        - |  3702 | `		}` |
|        2 |  3703 | `	}` |
|        5 |  3704 | `	break;` |
|        - |  3705 | `/*` |
|        - |  3706 | ` * UMINUS: * * *` |
|        - |  3707 | ` *` |
|        - |  3708 | ` * Perform a unary minus operation.` |
|        - |  3709 | ` */` |
|    23509 |  3710 | `case PH7_OP_UMINUS:` |
|        - |  3711 | `#ifdef UNTRUST` |
|        - |  3712 | `	if( pTos < pStack ){` |
|        - |  3713 | `		goto Abort;` |
|        - |  3714 | `	}` |
|        - |  3715 | `#endif` |
|        - |  3716 | `	/* Force a numeric (integer,real or both) cast */` |
|    47020 |  3717 | `	PH7_MemObjToNumeric(pTos);` |
|    47020 |  3718 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       32 |  3719 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3720 | `	}` |
|    47020 |  3721 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    46990 |  3722 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    23494 |  3723 | `	}` |
|    47020 |  3724 | `	break;` |
|        - |  3725 | `/*` |
|        - |  3726 | ` * UPLUS: * * *` |
|        - |  3727 | ` *` |
|        - |  3728 | ` * Perform a unary plus operation.` |
|        - |  3729 | ` */` |
|       16 |  3730 | `case PH7_OP_UPLUS:` |
|        - |  3731 | `#ifdef UNTRUST` |
|        - |  3732 | `	if( pTos < pStack ){` |
|        - |  3733 | `		goto Abort;` |
|        - |  3734 | `	}` |
|        - |  3735 | `#endif` |
|        - |  3736 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3737 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3738 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3739 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3740 | `	}` |
|       33 |  3741 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3742 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3743 | `	}` |
|       33 |  3744 | `	break;` |
|        - |  3745 | `/*` |
|        - |  3746 | ` * OP_LNOT: * * *` |
|        - |  3747 | ` *` |
|        - |  3748 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3749 | ` * with its complement.` |
|        - |  3750 | ` */` |
|    39800 |  3751 | `case PH7_OP_LNOT:` |
|        - |  3752 | `#ifdef UNTRUST` |
|        - |  3753 | `	if( pTos < pStack ){` |
|        - |  3754 | `		goto Abort;` |
|        - |  3755 | `	}` |
|        - |  3756 | `#endif` |
|        - |  3757 | `	/* Force a boolean cast */` |
|    79646 |  3758 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3759 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3760 | `	}` |
|    79646 |  3761 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    79646 |  3762 | `	break;` |
|        - |  3763 | `/*` |
|        - |  3764 | ` * OP_BITNOT: * * *` |
|        - |  3765 | ` *` |
|        - |  3766 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3767 | ` * with its ones-complement.` |
|        - |  3768 | ` */` |
|       14 |  3769 | `case PH7_OP_BITNOT:` |
|        - |  3770 | `#ifdef UNTRUST` |
|        - |  3771 | `	if( pTos < pStack ){` |
|        - |  3772 | `		goto Abort;` |
|        - |  3773 | `	}` |
|        - |  3774 | `#endif` |
|        - |  3775 | `	/* Force an integer cast */` |
|       30 |  3776 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3777 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3778 | `	}` |
|       30 |  3779 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  3780 | `	break;` |
|        - |  3781 | `/* OP_MUL * * *` |
|        - |  3782 | ` * OP_MUL_STORE * * *` |
|        - |  3783 | ` *` |
|        - |  3784 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3785 | ` * and push the result back onto the stack.` |
|        - |  3786 | ` */` |
|     1243 |  3787 | `case PH7_OP_MUL:` |
|        - |  3788 | `case PH7_OP_MUL_STORE: {` |
|     2488 |  3789 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3790 | `	/* Force the operand to be numeric */` |
|        - |  3791 | `#ifdef UNTRUST` |
|        - |  3792 | `	if( pNos < pStack ){` |
|        - |  3793 | `		goto Abort;` |
|        - |  3794 | `	}` |
|        - |  3795 | `#endif` |
|     2488 |  3796 | `	PH7_MemObjToNumeric(pTos);` |
|     2488 |  3797 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3798 | `	/* Perform the requested operation */` |
|     2488 |  3799 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3800 | `		/* Floating point arithemic */` |
|        - |  3801 | `		ph7_real a,b,r;` |
|       17 |  3802 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3803 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3804 | `		}` |
|       17 |  3805 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3806 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3807 | `		}` |
|       17 |  3808 | `		a = pNos->rVal;` |
|       17 |  3809 | `		b = pTos->rVal;` |
|       17 |  3810 | `		r = a * b;` |
|        - |  3811 | `		/* Push the result */` |
|       17 |  3812 | `		pNos->rVal = r;` |
|       17 |  3813 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3814 | `		/* Try to get an integer representation */` |
|       17 |  3815 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3816 | `	}else{` |
|        - |  3817 | `		/* Integer arithmetic */` |
|        - |  3818 | `		sxi64 a,b,r;` |
|     2472 |  3819 | `		a = pNos->x.iVal;` |
|     2472 |  3820 | `		b = pTos->x.iVal;` |
|     2472 |  3821 | `		r = a * b;` |
|        - |  3822 | `		/* Push the result */` |
|     2472 |  3823 | `		pNos->x.iVal = r;` |
|     2472 |  3824 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3825 | `	}` |
|     2488 |  3826 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3827 | `		ph7_value *pObj;` |
|       25 |  3828 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3829 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       25 |  3830 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       25 |  3831 | `			PH7_MemObjStore(pNos,pObj);` |
|       12 |  3832 | `		}` |
|       12 |  3833 | `	}` |
|     2488 |  3834 | `	VmPopOperand(&pTos,1);` |
|     2488 |  3835 | `	break;` |
|        - |  3836 | `				 }` |
|        - |  3837 | `/* OP_ADD * * *` |
|        - |  3838 | ` *` |
|        - |  3839 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3840 | ` * and push the result back onto the stack.` |
|        - |  3841 | ` */` |
|      438 |  3842 | `case PH7_OP_ADD:{` |
|      878 |  3843 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3844 | `#ifdef UNTRUST` |
|        - |  3845 | `	if( pNos < pStack ){` |
|        - |  3846 | `		goto Abort;` |
|        - |  3847 | `	}` |
|        - |  3848 | `#endif` |
|        - |  3849 | `	/* Perform the addition */` |
|      878 |  3850 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      878 |  3851 | `	VmPopOperand(&pTos,1);` |
|      878 |  3852 | `	break;` |
|        - |  3853 | `				}` |
|        - |  3854 | `/*` |
|        - |  3855 | ` * OP_ADD_STORE * * *` |
|        - |  3856 | ` *` |
|        - |  3857 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3858 | ` * and push the result back onto the stack.` |
|        - |  3859 | ` */` |
|      483 |  3860 | `case PH7_OP_ADD_STORE:{` |
|      968 |  3861 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3862 | `	ph7_value *pObj;` |
|        - |  3863 | `	sxu32 nIdx;` |
|        - |  3864 | `#ifdef UNTRUST` |
|        - |  3865 | `	if( pNos < pStack ){` |
|        - |  3866 | `		goto Abort;` |
|        - |  3867 | `	}` |
|        - |  3868 | `#endif` |
|        - |  3869 | `	/* Perform the addition */` |
|      968 |  3870 | `	nIdx = pTos->nIdx;` |
|      968 |  3871 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3872 | `	/* Peform the store operation */` |
|      968 |  3873 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3874 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      968 |  3875 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      968 |  3876 | `		PH7_MemObjStore(pTos,pObj);` |
|      483 |  3877 | `	}` |
|        - |  3878 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      968 |  3879 | `	PH7_MemObjStore(pTos,pNos);` |
|      968 |  3880 | `	VmPopOperand(&pTos,1);` |
|      968 |  3881 | `	break;` |
|        - |  3882 | `				}` |
|        - |  3883 | `/* OP_SUB * * *` |
|        - |  3884 | ` *` |
|        - |  3885 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3886 | ` * first (what was next on the stack) from the second (the` |
|        - |  3887 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3888 | ` */` |
|      299 |  3889 | `case PH7_OP_SUB: {` |
|      600 |  3890 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3891 | `#ifdef UNTRUST` |
|        - |  3892 | `	if( pNos < pStack ){` |
|        - |  3893 | `		goto Abort;` |
|        - |  3894 | `	}` |
|        - |  3895 | `#endif` |
|      600 |  3896 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3897 | `		/* Floating point arithemic */` |
|        - |  3898 | `		ph7_real a,b,r;` |
|       95 |  3899 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3900 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3901 | `		}` |
|       95 |  3902 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3903 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3904 | `		}` |
|       95 |  3905 | `		a = pNos->rVal;` |
|       95 |  3906 | `		b = pTos->rVal;` |
|       95 |  3907 | `		r = a - b;` |
|        - |  3908 | `		/* Push the result */` |
|       95 |  3909 | `		pNos->rVal = r;` |
|       95 |  3910 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3911 | `		/* Try to get an integer representation */` |
|       95 |  3912 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  3913 | `	}else{` |
|        - |  3914 | `		/* Integer arithmetic */` |
|        - |  3915 | `		sxi64 a,b,r;` |
|      506 |  3916 | `		a = pNos->x.iVal;` |
|      506 |  3917 | `		b = pTos->x.iVal;` |
|      506 |  3918 | `		r = a - b;` |
|        - |  3919 | `		/* Push the result */` |
|      506 |  3920 | `		pNos->x.iVal = r;` |
|      506 |  3921 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3922 | `	}` |
|      600 |  3923 | `	VmPopOperand(&pTos,1);` |
|      600 |  3924 | `	break;` |
|        - |  3925 | `				 }` |
|        - |  3926 | `/* OP_SUB_STORE * * *` |
|        - |  3927 | ` *` |
|        - |  3928 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3929 | ` * first (what was next on the stack) from the second (the` |
|        - |  3930 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3931 | ` */` |
|        1 |  3932 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3933 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3934 | `	ph7_value *pObj;` |
|        - |  3935 | `#ifdef UNTRUST` |
|        - |  3936 | `	if( pNos < pStack ){` |
|        - |  3937 | `		goto Abort;` |
|        - |  3938 | `	}` |
|        - |  3939 | `#endif` |
|        3 |  3940 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3941 | `		/* Floating point arithemic */` |
|        - |  3942 | `		ph7_real a,b,r;` |
|      ! 0 |  3943 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3944 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3945 | `		}` |
|      ! 0 |  3946 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3947 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3948 | `		}` |
|      ! 0 |  3949 | `		a = pTos->rVal;` |
|      ! 0 |  3950 | `		b = pNos->rVal;` |
|      ! 0 |  3951 | `		r = a - b;` |
|        - |  3952 | `		/* Push the result */` |
|      ! 0 |  3953 | `		pNos->rVal = r;` |
|      ! 0 |  3954 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3955 | `		/* Try to get an integer representation */` |
|      ! 0 |  3956 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3957 | `	}else{` |
|        - |  3958 | `		/* Integer arithmetic */` |
|        - |  3959 | `		sxi64 a,b,r;` |
|        3 |  3960 | `		a = pTos->x.iVal;` |
|        3 |  3961 | `		b = pNos->x.iVal;` |
|        3 |  3962 | `		r = a - b;` |
|        - |  3963 | `		/* Push the result */` |
|        3 |  3964 | `		pNos->x.iVal = r;` |
|        3 |  3965 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3966 | `	}` |
|        3 |  3967 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3968 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3969 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3970 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3971 | `	}` |
|        3 |  3972 | `	VmPopOperand(&pTos,1);` |
|        3 |  3973 | `	break;` |
|        - |  3974 | `				 }` |
|        - |  3975 |  |
|        - |  3976 | `/*` |
|        - |  3977 | ` * OP_MOD * * *` |
|        - |  3978 | ` *` |
|        - |  3979 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3980 | ` * first (what was next on the stack) from the second (the` |
|        - |  3981 | ` * top of the stack) and push the remainder after division` |
|        - |  3982 | ` * onto the stack.` |
|        - |  3983 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3984 | ` */` |
|      305 |  3985 | `case PH7_OP_MOD:{` |
|      612 |  3986 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3987 | `	sxi64 a,b,r;` |
|        - |  3988 | `#ifdef UNTRUST` |
|        - |  3989 | `	if( pNos < pStack ){` |
|        - |  3990 | `		goto Abort;` |
|        - |  3991 | `	}` |
|        - |  3992 | `#endif` |
|        - |  3993 | `	/* Force the operands to be integer */` |
|      612 |  3994 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3995 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3996 | `	}` |
|      612 |  3997 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3998 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3999 | `	}` |
|        - |  4000 | `	/* Perform the requested operation */` |
|      612 |  4001 | `	a = pNos->x.iVal;` |
|      612 |  4002 | `	b = pTos->x.iVal;` |
|      612 |  4003 | `	if( b == 0 ){` |
|        3 |  4004 | `		r = 0;` |
|        3 |  4005 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4006 | `		/* goto Abort; */` |
|        2 |  4007 | `	}else{` |
|      609 |  4008 | `		r = a%b;` |
|        - |  4009 | `	}` |
|        - |  4010 | `	/* Push the result */` |
|      612 |  4011 | `	pNos->x.iVal = r;` |
|      612 |  4012 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      612 |  4013 | `	VmPopOperand(&pTos,1);` |
|      612 |  4014 | `	break;` |
|        - |  4015 | `				}` |
|        - |  4016 | `/*` |
|        - |  4017 | ` * OP_MOD_STORE * * *` |
|        - |  4018 | ` *` |
|        - |  4019 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4020 | ` * first (what was next on the stack) from the second (the` |
|        - |  4021 | ` * top of the stack) and push the remainder after division` |
|        - |  4022 | ` * onto the stack.` |
|        - |  4023 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4024 | ` */` |
|        1 |  4025 | `case PH7_OP_MOD_STORE: {` |
|        3 |  4026 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4027 | `	ph7_value *pObj;` |
|        - |  4028 | `	sxi64 a,b,r;` |
|        - |  4029 | `#ifdef UNTRUST` |
|        - |  4030 | `	if( pNos < pStack ){` |
|        - |  4031 | `		goto Abort;` |
|        - |  4032 | `	}` |
|        - |  4033 | `#endif` |
|        - |  4034 | `	/* Force the operands to be integer */` |
|        3 |  4035 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4036 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4037 | `	}` |
|        3 |  4038 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4039 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4040 | `	}` |
|        - |  4041 | `	/* Perform the requested operation */` |
|        3 |  4042 | `	a = pTos->x.iVal;` |
|        3 |  4043 | `	b = pNos->x.iVal;` |
|        3 |  4044 | `	if( b == 0 ){` |
|      ! 0 |  4045 | `		r = 0;` |
|      ! 0 |  4046 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4047 | `		/* goto Abort; */` |
|      ! 0 |  4048 | `	}else{` |
|        3 |  4049 | `		r = a%b;` |
|        - |  4050 | `	}` |
|        - |  4051 | `	/* Push the result */` |
|        3 |  4052 | `	pNos->x.iVal = r;` |
|        3 |  4053 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  4054 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4055 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4056 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4057 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4058 | `	}` |
|        3 |  4059 | `	VmPopOperand(&pTos,1);` |
|        3 |  4060 | `	break;` |
|        - |  4061 | `				}` |
|        - |  4062 | `/*` |
|        - |  4063 | ` * OP_DIV * * *` |
|        - |  4064 | ` *` |
|        - |  4065 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4066 | ` * first (what was next on the stack) from the second (the` |
|        - |  4067 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4068 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4069 | ` */` |
|       28 |  4070 | `case PH7_OP_DIV:{` |
|       58 |  4071 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4072 | `	ph7_real a,b,r;` |
|        - |  4073 | `#ifdef UNTRUST` |
|        - |  4074 | `	if( pNos < pStack ){` |
|        - |  4075 | `		goto Abort;` |
|        - |  4076 | `	}` |
|        - |  4077 | `#endif` |
|        - |  4078 | `	/* Force the operands to be real */` |
|       58 |  4079 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  4080 | `		PH7_MemObjToReal(pTos);` |
|       26 |  4081 | `	}` |
|       58 |  4082 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  4083 | `		PH7_MemObjToReal(pNos);` |
|        9 |  4084 | `	}` |
|        - |  4085 | `	/* Perform the requested operation */` |
|       58 |  4086 | `	a = pNos->rVal;` |
|       58 |  4087 | `	b = pTos->rVal;` |
|       58 |  4088 | `	if( b == 0 ){` |
|        - |  4089 | `		/* Division by zero */` |
|        3 |  4090 | `		pNos->rVal = 0;` |
|        3 |  4091 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  4092 | `		/* goto Abort; */` |
|        2 |  4093 | `	}else{` |
|       55 |  4094 | `		r = a/b;` |
|        - |  4095 | `		/* Push the result */` |
|       55 |  4096 | `		pNos->rVal = r;` |
|       55 |  4097 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4098 | `		/* Try to get an integer representation */` |
|       55 |  4099 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4100 | `	}` |
|       58 |  4101 | `	VmPopOperand(&pTos,1);` |
|       58 |  4102 | `	break;` |
|        - |  4103 | `				}` |
|        - |  4104 | `/*` |
|        - |  4105 | ` * OP_DIV_STORE * * *` |
|        - |  4106 | ` *` |
|        - |  4107 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4108 | ` * first (what was next on the stack) from the second (the` |
|        - |  4109 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4110 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4111 | ` */` |
|        1 |  4112 | `case PH7_OP_DIV_STORE:{` |
|        3 |  4113 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4114 | `	ph7_value *pObj;` |
|        - |  4115 | `	ph7_real a,b,r;` |
|        - |  4116 | `#ifdef UNTRUST` |
|        - |  4117 | `	if( pNos < pStack ){` |
|        - |  4118 | `		goto Abort;` |
|        - |  4119 | `	}` |
|        - |  4120 | `#endif` |
|        - |  4121 | `	/* Force the operands to be real */` |
|        3 |  4122 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4123 | `		PH7_MemObjToReal(pTos);` |
|        1 |  4124 | `	}` |
|        3 |  4125 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4126 | `		PH7_MemObjToReal(pNos);` |
|        1 |  4127 | `	}` |
|        - |  4128 | `	/* Perform the requested operation */` |
|        3 |  4129 | `	a = pTos->rVal;` |
|        3 |  4130 | `	b = pNos->rVal;` |
|        3 |  4131 | `	if( b == 0 ){` |
|        - |  4132 | `		/* Division by zero */` |
|      ! 0 |  4133 | `		r = 0;` |
|      ! 0 |  4134 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4135 | `		/* goto Abort; */` |
|      ! 0 |  4136 | `	}else{` |
|        3 |  4137 | `		r = a/b;` |
|        - |  4138 | `		/* Push the result */` |
|        3 |  4139 | `		pNos->rVal = r;` |
|        3 |  4140 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4141 | `		/* Try to get an integer representation */` |
|        3 |  4142 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4143 | `	}` |
|        3 |  4144 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4145 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4146 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4147 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4148 | `	}` |
|        3 |  4149 | `	VmPopOperand(&pTos,1);` |
|        3 |  4150 | `	break;` |
|        - |  4151 | `				}` |
|        - |  4152 | `/* OP_BAND * * *` |
|        - |  4153 | ` *` |
|        - |  4154 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4155 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4156 | ` * two elements.` |
|        - |  4157 | `*/` |
|        - |  4158 | `/* OP_BOR * * *` |
|        - |  4159 | ` *` |
|        - |  4160 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4161 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4162 | ` * two elements.` |
|        - |  4163 | ` */` |
|        - |  4164 | `/* OP_BXOR * * *` |
|        - |  4165 | ` *` |
|        - |  4166 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4167 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4168 | ` * two elements.` |
|        - |  4169 | ` */` |
|       30 |  4170 | `case PH7_OP_BAND:` |
|        - |  4171 | `case PH7_OP_BOR:` |
|        - |  4172 | `case PH7_OP_BXOR:{` |
|       62 |  4173 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4174 | `	sxi64 a,b,r;` |
|        - |  4175 | `#ifdef UNTRUST` |
|        - |  4176 | `	if( pNos < pStack ){` |
|        - |  4177 | `		goto Abort;` |
|        - |  4178 | `	}` |
|        - |  4179 | `#endif` |
|        - |  4180 | `	/* Force the operands to be integer */` |
|       62 |  4181 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4182 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4183 | `	}` |
|       62 |  4184 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4185 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4186 | `	}` |
|        - |  4187 | `	/* Perform the requested operation */` |
|       62 |  4188 | `	a = pNos->x.iVal;` |
|       62 |  4189 | `	b = pTos->x.iVal;` |
|       62 |  4190 | `	switch(pInstr->iOp){` |
|        6 |  4191 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4192 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4193 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4194 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       18 |  4195 | `	case PH7_OP_BAND_STORE:` |
|       18 |  4196 | `	case PH7_OP_BAND:` |
|       38 |  4197 | `	default:          r = a&b; break;` |
|        - |  4198 | `	}` |
|        - |  4199 | `	/* Push the result */` |
|       62 |  4200 | `	pNos->x.iVal = r;` |
|       62 |  4201 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       62 |  4202 | `	VmPopOperand(&pTos,1);` |
|       62 |  4203 | `	break;` |
|        - |  4204 | `				 }` |
|        - |  4205 | `/* OP_BAND_STORE * * *` |
|        - |  4206 | ` *` |
|        - |  4207 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4208 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4209 | ` * two elements.` |
|        - |  4210 | `*/` |
|        - |  4211 | `/* OP_BOR_STORE * * *` |
|        - |  4212 | ` *` |
|        - |  4213 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4214 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4215 | ` * two elements.` |
|        - |  4216 | ` */` |
|        - |  4217 | `/* OP_BXOR_STORE * * *` |
|        - |  4218 | ` *` |
|        - |  4219 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4220 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4221 | ` * two elements.` |
|        - |  4222 | ` */` |
|        7 |  4223 | `case PH7_OP_BAND_STORE:` |
|        - |  4224 | `case PH7_OP_BOR_STORE:` |
|        - |  4225 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4226 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4227 | `	ph7_value *pObj;` |
|        - |  4228 | `	sxi64 a,b,r;` |
|        - |  4229 | `#ifdef UNTRUST` |
|        - |  4230 | `	if( pNos < pStack ){` |
|        - |  4231 | `		goto Abort;` |
|        - |  4232 | `	}` |
|        - |  4233 | `#endif` |
|        - |  4234 | `	/* Force the operands to be integer */` |
|       15 |  4235 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4236 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4237 | `	}` |
|       15 |  4238 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4239 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4240 | `	}` |
|        - |  4241 | `	/* Perform the requested operation */` |
|       15 |  4242 | `	a = pTos->x.iVal;` |
|       15 |  4243 | `	b = pNos->x.iVal;` |
|       15 |  4244 | `	switch(pInstr->iOp){` |
|        2 |  4245 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4246 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4247 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4248 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4249 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4250 | `	case PH7_OP_BAND:` |
|        5 |  4251 | `	default:          r = a&b; break;` |
|        - |  4252 | `	}` |
|        - |  4253 | `	/* Push the result */` |
|       15 |  4254 | `	pNos->x.iVal = r;` |
|       15 |  4255 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4256 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4257 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4258 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4259 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4260 | `	}` |
|       15 |  4261 | `	VmPopOperand(&pTos,1);` |
|       15 |  4262 | `	break;` |
|        - |  4263 | `				 }` |
|        - |  4264 | `/* OP_SHL * * *` |
|        - |  4265 | ` *` |
|        - |  4266 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4267 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4268 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4269 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4270 | ` */` |
|        - |  4271 | `/* OP_SHR * * *` |
|        - |  4272 | ` *` |
|        - |  4273 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4274 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4275 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4276 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4277 | ` */` |
|        9 |  4278 | `case PH7_OP_SHL:` |
|        - |  4279 | `case PH7_OP_SHR: {` |
|       19 |  4280 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4281 | `	sxi64 a,r;` |
|        - |  4282 | `	sxi32 b;` |
|        - |  4283 | `#ifdef UNTRUST` |
|        - |  4284 | `	if( pNos < pStack ){` |
|        - |  4285 | `		goto Abort;` |
|        - |  4286 | `	}` |
|        - |  4287 | `#endif` |
|        - |  4288 | `	/* Force the operands to be integer */` |
|       19 |  4289 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4290 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4291 | `	}` |
|       19 |  4292 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4293 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4294 | `	}` |
|        - |  4295 | `	/* Perform the requested operation */` |
|       19 |  4296 | `	a = pNos->x.iVal;` |
|       19 |  4297 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4298 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4299 | `		r = a << b;` |
|        6 |  4300 | `	}else{` |
|        9 |  4301 | `		r = a >> b;` |
|        - |  4302 | `	}` |
|        - |  4303 | `	/* Push the result */` |
|       19 |  4304 | `	pNos->x.iVal = r;` |
|       19 |  4305 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4306 | `	VmPopOperand(&pTos,1);` |
|       19 |  4307 | `	break;` |
|        - |  4308 | `				 }` |
|        - |  4309 | `/*  OP_SHL_STORE * * *` |
|        - |  4310 | ` *` |
|        - |  4311 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4312 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4313 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4314 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4315 | ` */` |
|        - |  4316 | `/* OP_SHR_STORE * * *` |
|        - |  4317 | ` *` |
|        - |  4318 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4319 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4320 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4321 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4322 | ` */` |
|        7 |  4323 | `case PH7_OP_SHL_STORE:` |
|        - |  4324 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4325 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4326 | `	ph7_value *pObj;` |
|        - |  4327 | `	sxi64 a,r;` |
|        - |  4328 | `	sxi32 b;` |
|        - |  4329 | `#ifdef UNTRUST` |
|        - |  4330 | `	if( pNos < pStack ){` |
|        - |  4331 | `		goto Abort;` |
|        - |  4332 | `	}` |
|        - |  4333 | `#endif` |
|        - |  4334 | `	/* Force the operands to be integer */` |
|       15 |  4335 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4336 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4337 | `	}` |
|       15 |  4338 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4339 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4340 | `	}` |
|        - |  4341 | `	/* Perform the requested operation */` |
|       15 |  4342 | `	a = pTos->x.iVal;` |
|       15 |  4343 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4344 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4345 | `		r = a << b;` |
|        4 |  4346 | `	}else{` |
|        9 |  4347 | `		r = a >> b;` |
|        - |  4348 | `	}` |
|        - |  4349 | `	/* Push the result */` |
|       15 |  4350 | `	pNos->x.iVal = r;` |
|       15 |  4351 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4352 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4353 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4354 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4355 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4356 | `	}` |
|       15 |  4357 | `	VmPopOperand(&pTos,1);` |
|       15 |  4358 | `	break;` |
|        - |  4359 | `				 }` |
|        - |  4360 | `/* CAT:  P1 * *` |
|        - |  4361 | ` *` |
|        - |  4362 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4363 | ` * back.` |
|        - |  4364 | ` */` |
|    62433 |  4365 | `case PH7_OP_CAT:{` |
|        - |  4366 | `	ph7_value *pNos,*pCur;` |
|   124868 |  4367 | `	if( pInstr->iP1 < 1 ){` |
|    97910 |  4368 | `		pNos = &pTos[-1];` |
|    48956 |  4369 | `	}else{` |
|    26960 |  4370 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4371 | `	}` |
|        - |  4372 | `#ifdef UNTRUST` |
|        - |  4373 | `	if( pNos < pStack ){` |
|        - |  4374 | `		goto Abort;` |
|        - |  4375 | `	}` |
|        - |  4376 | `#endif` |
|        - |  4377 | `	/* Force a string cast */` |
|   124868 |  4378 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1146 |  4379 | `		PH7_MemObjToString(pNos);` |
|      572 |  4380 | `	}` |
|   124868 |  4381 | `	pCur = &pNos[1];` |
|   251698 |  4382 | `	while( pCur <= pTos ){` |
|   126832 |  4383 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50604 |  4384 | `			PH7_MemObjToString(pCur);` |
|    25301 |  4385 | `		}` |
|        - |  4386 | `		/* Perform the concatenation */` |
|   126832 |  4387 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   126794 |  4388 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    63396 |  4389 | `		}` |
|   126832 |  4390 | `		SyBlobRelease(&pCur->sBlob);` |
|   126832 |  4391 | `		pCur++;` |
|        2 |  4392 | `	}` |
|   124868 |  4393 | `	pTos = pNos;` |
|   124868 |  4394 | `	break;` |
|        - |  4395 | `				}` |
|        - |  4396 | `/*  CAT_STORE: * * *` |
|        - |  4397 | ` *` |
|        - |  4398 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4399 | ` * back.` |
|        - |  4400 | ` */` |
|     3453 |  4401 | `case PH7_OP_CAT_STORE:{` |
|     6908 |  4402 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4403 | `	ph7_value *pObj;` |
|        - |  4404 | `#ifdef UNTRUST` |
|        - |  4405 | `	if( pNos < pStack ){` |
|        - |  4406 | `		goto Abort;` |
|        - |  4407 | `	}` |
|        - |  4408 | `#endif` |
|     6908 |  4409 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4410 | `		/* Force a string cast */` |
|      ! 0 |  4411 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4412 | `	}` |
|     6908 |  4413 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4414 | `		/* Force a string cast */` |
|      ! 0 |  4415 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4416 | `	}` |
|        - |  4417 | `	/* Perform the concatenation (Reverse order) */` |
|     6908 |  4418 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     6908 |  4419 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3453 |  4420 | `	}` |
|        - |  4421 | `	/* Perform the store operation */` |
|     6908 |  4422 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4423 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     6908 |  4424 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     6908 |  4425 | `		PH7_MemObjStore(pTos,pObj);` |
|     3453 |  4426 | `	}` |
|     6908 |  4427 | `	PH7_MemObjStore(pTos,pNos);` |
|     6908 |  4428 | `	VmPopOperand(&pTos,1);` |
|     6908 |  4429 | `	break;` |
|        - |  4430 | `				}` |
|        - |  4431 | `/* OP_AND: * * *` |
|        - |  4432 | ` *` |
|        - |  4433 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4434 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4435 | ` * stack.` |
|        - |  4436 | ` */` |
|        - |  4437 | `/* OP_OR: * * *` |
|        - |  4438 | ` *` |
|        - |  4439 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4440 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4441 | ` * stack.` |
|        - |  4442 | ` */` |
|    94306 |  4443 | `case PH7_OP_LAND:` |
|        - |  4444 | `case PH7_OP_LOR: {` |
|   188658 |  4445 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4446 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4447 | `#ifdef UNTRUST` |
|        - |  4448 | `	if( pNos < pStack ){` |
|        - |  4449 | `		goto Abort;` |
|        - |  4450 | `	}` |
|        - |  4451 | `#endif` |
|        - |  4452 | `	/* Force a boolean cast */` |
|   188658 |  4453 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4454 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4455 | `	}` |
|   188658 |  4456 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4457 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4458 | `	}` |
|   188658 |  4459 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   188658 |  4460 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   188658 |  4461 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4462 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    86252 |  4463 | `		v1 = and_logic[v1*3+v2];` |
|    43149 |  4464 | `	}else{` |
|        - |  4465 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   102408 |  4466 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4467 | `	}` |
|   188658 |  4468 | `	if( v1 == 2 ){` |
|      ! 0 |  4469 | `		v1 = 1;` |
|      ! 0 |  4470 | `	}` |
|   188658 |  4471 | `	VmPopOperand(&pTos,1);` |
|   188658 |  4472 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   188658 |  4473 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   188658 |  4474 | `	break;` |
|        - |  4475 | `				 }` |
|        - |  4476 | `/* OP_LXOR: * * *` |
|        - |  4477 | ` *` |
|        - |  4478 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4479 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4480 | ` * stack.` |
|        - |  4481 | ` * According to the PHP language reference manual:` |
|        - |  4482 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4483 | ` *  TRUE,but not both.` |
|        - |  4484 | ` */` |
|        5 |  4485 | `case PH7_OP_LXOR:{` |
|       11 |  4486 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4487 | `	sxi32 v = 0;` |
|        - |  4488 | `#ifdef UNTRUST` |
|        - |  4489 | `	if( pNos < pStack ){` |
|        - |  4490 | `		goto Abort;` |
|        - |  4491 | `	}` |
|        - |  4492 | `#endif` |
|        - |  4493 | `	/* Force a boolean cast */` |
|       11 |  4494 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4495 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4496 | `	}` |
|       11 |  4497 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4498 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4499 | `	}` |
|       11 |  4500 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4501 | `		v = 1;` |
|        3 |  4502 | `	}` |
|       11 |  4503 | `	VmPopOperand(&pTos,1);` |
|       11 |  4504 | `	pTos->x.iVal = v;` |
|       11 |  4505 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4506 | `	break;` |
|        - |  4507 | `				 }` |
|        - |  4508 | `/* OP_EQ P1 P2 P3` |
|        - |  4509 | ` *` |
|        - |  4510 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4511 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4512 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4513 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4514 | ` */` |
|        - |  4515 | `/* OP_NEQ P1 P2 P3` |
|        - |  4516 | ` *` |
|        - |  4517 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4518 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4519 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4520 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4521 | ` */` |
|     3882 |  4522 | `case PH7_OP_EQ:` |
|        - |  4523 | `case PH7_OP_NEQ: {` |
|     7766 |  4524 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4525 | `	/* Perform the comparison and act accordingly */` |
|        - |  4526 | `#ifdef UNTRUST` |
|        - |  4527 | `	if( pNos < pStack ){` |
|        - |  4528 | `		goto Abort;` |
|        - |  4529 | `	}` |
|        - |  4530 | `#endif` |
|     7766 |  4531 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7766 |  4532 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       20 |  4533 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7757 |  4534 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7722 |  4535 | `		rc = rc == 0;` |
|     3862 |  4536 | `	}else{` |
|       28 |  4537 | `		rc = rc != 0;` |
|        - |  4538 | `	}` |
|     7766 |  4539 | `	VmPopOperand(&pTos,1);` |
|     7766 |  4540 | `	if( !pInstr->iP2 ){` |
|        - |  4541 | `		/* Push comparison result without taking the jump */` |
|     7766 |  4542 | `		PH7_MemObjRelease(pTos);` |
|     7766 |  4543 | `		pTos->x.iVal = rc;` |
|        - |  4544 | `		/* Invalidate any prior representation */` |
|     7766 |  4545 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3884 |  4546 | `	}else{` |
|      ! 0 |  4547 | `		if( rc ){` |
|        - |  4548 | `			/* Jump to the desired location */` |
|      ! 0 |  4549 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4550 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4551 | `		}` |
|        - |  4552 | `	}` |
|     7766 |  4553 | `	break;` |
|        - |  4554 | `				 }` |
|        - |  4555 | `/* OP_TEQ P1 P2 *` |
|        - |  4556 | ` *` |
|        - |  4557 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4558 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4559 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4560 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4561 | ` */` |
|   130772 |  4562 | `case PH7_OP_TEQ: {` |
|   261546 |  4563 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4564 | `	/* Perform the comparison and act accordingly */` |
|        - |  4565 | `#ifdef UNTRUST` |
|        - |  4566 | `	if( pNos < pStack ){` |
|        - |  4567 | `		goto Abort;` |
|        - |  4568 | `	}` |
|        - |  4569 | `#endif` |
|   261546 |  4570 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   261546 |  4571 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4572 | `		rc = 0;` |
|        2 |  4573 | `	}else{` |
|   261544 |  4574 | `		rc = rc == 0;` |
|        - |  4575 | `	}` |
|   261546 |  4576 | `	VmPopOperand(&pTos,1);` |
|   261546 |  4577 | `	if( !pInstr->iP2 ){` |
|        - |  4578 | `		/* Push comparison result without taking the jump */` |
|   261546 |  4579 | `		PH7_MemObjRelease(pTos);` |
|   261546 |  4580 | `		pTos->x.iVal = rc;` |
|        - |  4581 | `		/* Invalidate any prior representation */` |
|   261546 |  4582 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   130774 |  4583 | `	}else{` |
|      ! 0 |  4584 | `		if( rc ){` |
|        - |  4585 | `			/* Jump to the desired location */` |
|      ! 0 |  4586 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4587 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4588 | `		}` |
|        - |  4589 | `	}` |
|   261546 |  4590 | `	break;` |
|        - |  4591 | `				 }` |
|        - |  4592 | `/* OP_TNE P1 P2 *` |
|        - |  4593 | ` *` |
|        - |  4594 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4595 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4596 | ` * instruction.` |
|        - |  4597 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4598 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4599 | ` *` |
|        - |  4600 | ` */` |
|   101968 |  4601 | `case PH7_OP_TNE: {` |
|   203938 |  4602 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4603 | `	/* Perform the comparison and act accordingly */` |
|        - |  4604 | `#ifdef UNTRUST` |
|        - |  4605 | `	if( pNos < pStack ){` |
|        - |  4606 | `		goto Abort;` |
|        - |  4607 | `	}` |
|        - |  4608 | `#endif` |
|   203938 |  4609 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   203938 |  4610 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4611 | `		rc = 1;` |
|        2 |  4612 | `	}else{` |
|   203936 |  4613 | `		rc = rc != 0;` |
|        - |  4614 | `	}` |
|   203938 |  4615 | `	VmPopOperand(&pTos,1);` |
|   203938 |  4616 | `	if( !pInstr->iP2 ){` |
|        - |  4617 | `		/* Push comparison result without taking the jump */` |
|   203938 |  4618 | `		PH7_MemObjRelease(pTos);` |
|   203938 |  4619 | `		pTos->x.iVal = rc;` |
|        - |  4620 | `		/* Invalidate any prior representation */` |
|   203938 |  4621 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   101970 |  4622 | `	}else{` |
|      ! 0 |  4623 | `		if( rc ){` |
|        - |  4624 | `			/* Jump to the desired location */` |
|      ! 0 |  4625 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4626 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4627 | `		}` |
|        - |  4628 | `	}` |
|   203938 |  4629 | `	break;` |
|        - |  4630 | `				 }` |
|        - |  4631 | `/* OP_LT P1 P2 P3` |
|        - |  4632 | ` *` |
|        - |  4633 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4634 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4635 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4636 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4637 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4638 | ` *` |
|        - |  4639 | ` */` |
|        - |  4640 | `/* OP_LE P1 P2 P3` |
|        - |  4641 | ` *` |
|        - |  4642 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4643 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4644 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4645 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4646 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4647 | ` *` |
|        - |  4648 | ` */` |
|   102474 |  4649 | `case PH7_OP_LT:` |
|        - |  4650 | `case PH7_OP_LE: {` |
|   204994 |  4651 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4652 | `	/* Perform the comparison and act accordingly */` |
|        - |  4653 | `#ifdef UNTRUST` |
|        - |  4654 | `	if( pNos < pStack ){` |
|        - |  4655 | `		goto Abort;` |
|        - |  4656 | `	}` |
|        - |  4657 | `#endif` |
|   204994 |  4658 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   204994 |  4659 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4660 | `		rc = 0;` |
|   204990 |  4661 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      408 |  4662 | `		rc = rc < 1;` |
|      205 |  4663 | `	}else{` |
|   204580 |  4664 | `		rc = rc < 0;` |
|        - |  4665 | `	}` |
|   204994 |  4666 | `	VmPopOperand(&pTos,1);` |
|   204994 |  4667 | `	if( !pInstr->iP2 ){` |
|        - |  4668 | `		/* Push comparison result without taking the jump */` |
|   204994 |  4669 | `		PH7_MemObjRelease(pTos);` |
|   204994 |  4670 | `		pTos->x.iVal = rc;` |
|        - |  4671 | `		/* Invalidate any prior representation */` |
|   204994 |  4672 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   102520 |  4673 | `	}else{` |
|      ! 0 |  4674 | `		if( rc ){` |
|        - |  4675 | `			/* Jump to the desired location */` |
|      ! 0 |  4676 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4677 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4678 | `		}` |
|        - |  4679 | `	}` |
|   204994 |  4680 | `	break;` |
|        - |  4681 | `				}` |
|        - |  4682 | `/* OP_GT P1 P2 P3` |
|        - |  4683 | ` *` |
|        - |  4684 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4685 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4686 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4687 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4688 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4689 | ` *` |
|        - |  4690 | ` */` |
|        - |  4691 | `/* OP_GE P1 P2 P3` |
|        - |  4692 | ` *` |
|        - |  4693 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4694 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4695 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4696 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4697 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4698 | ` *` |
|        - |  4699 | ` */` |
|    48811 |  4700 | `case PH7_OP_GT:` |
|        - |  4701 | `case PH7_OP_GE: {` |
|    97624 |  4702 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4703 | `	/* Perform the comparison and act accordingly */` |
|        - |  4704 | `#ifdef UNTRUST` |
|        - |  4705 | `	if( pNos < pStack ){` |
|        - |  4706 | `		goto Abort;` |
|        - |  4707 | `	}` |
|        - |  4708 | `#endif` |
|    97624 |  4709 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    97624 |  4710 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4711 | `		rc = 0;` |
|    97620 |  4712 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    97468 |  4713 | `		rc = rc >= 0;` |
|    48735 |  4714 | `	}else{` |
|      150 |  4715 | `		rc = rc > 0;` |
|        - |  4716 | `	}` |
|    97624 |  4717 | `	VmPopOperand(&pTos,1);` |
|    97624 |  4718 | `	if( !pInstr->iP2 ){` |
|        - |  4719 | `		/* Push comparison result without taking the jump */` |
|    97624 |  4720 | `		PH7_MemObjRelease(pTos);` |
|    97624 |  4721 | `		pTos->x.iVal = rc;` |
|        - |  4722 | `		/* Invalidate any prior representation */` |
|    97624 |  4723 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    48813 |  4724 | `	}else{` |
|      ! 0 |  4725 | `		if( rc ){` |
|        - |  4726 | `			/* Jump to the desired location */` |
|      ! 0 |  4727 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4728 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4729 | `		}` |
|        - |  4730 | `	}` |
|    97624 |  4731 | `	break;` |
|        - |  4732 | `				}` |
|        - |  4733 | `/* OP_SEQ P1 P2 *` |
|        - |  4734 | ` * Strict string comparison.` |
|        - |  4735 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4736 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4737 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4738 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4739 | ` * use PH7_OP_EQ.` |
|        - |  4740 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4741 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4742 | ` */` |
|        - |  4743 | `/* OP_SNE P1 P2 *` |
|        - |  4744 | ` * Strict string comparison.` |
|        - |  4745 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4746 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4747 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4748 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4749 | ` * use PH7_OP_EQ.` |
|        - |  4750 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4751 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4752 | ` */` |
|       18 |  4753 | `case PH7_OP_SEQ:` |
|        - |  4754 | `case PH7_OP_SNE: {` |
|       38 |  4755 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4756 | `	SyString s1,s2;` |
|        - |  4757 | `	/* Perform the comparison and act accordingly */` |
|        - |  4758 | `#ifdef UNTRUST` |
|        - |  4759 | `	if( pNos < pStack ){` |
|        - |  4760 | `		goto Abort;` |
|        - |  4761 | `	}` |
|        - |  4762 | `#endif` |
|        - |  4763 | `	/* Force a string cast */` |
|       38 |  4764 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4765 | `		PH7_MemObjToString(pTos);` |
|        2 |  4766 | `	}` |
|       38 |  4767 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4768 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4769 | `	}` |
|       38 |  4770 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4771 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4772 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4773 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4774 | `		rc = rc != 0;` |
|      ! 0 |  4775 | `	}else{` |
|       38 |  4776 | `		rc = rc == 0;` |
|        - |  4777 | `	}` |
|       38 |  4778 | `	VmPopOperand(&pTos,1);` |
|       38 |  4779 | `	if( !pInstr->iP2 ){` |
|        - |  4780 | `		/* Push comparison result without taking the jump */` |
|       38 |  4781 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4782 | `		pTos->x.iVal = rc;` |
|        - |  4783 | `		/* Invalidate any prior representation */` |
|       38 |  4784 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4785 | `	}else{` |
|      ! 0 |  4786 | `		if( rc ){` |
|        - |  4787 | `			/* Jump to the desired location */` |
|      ! 0 |  4788 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4789 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4790 | `		}` |
|        - |  4791 | `	}` |
|       38 |  4792 | `	break;` |
|        - |  4793 | `				 }` |
|        - |  4794 | `/*` |
|        - |  4795 | ` * OP_LOAD_REF * * *` |
|        - |  4796 | ` * Push the index of a referenced object on the stack.` |
|        - |  4797 | ` */` |
|       57 |  4798 | `case PH7_OP_LOAD_REF: {` |
|        - |  4799 | `	sxu32 nIdx;` |
|        - |  4800 | `#ifdef UNTRUST` |
|        - |  4801 | `	if( pTos < pStack ){` |
|        - |  4802 | `		goto Abort;` |
|        - |  4803 | `	}` |
|        - |  4804 | `#endif` |
|        - |  4805 | `	/* Extract memory object index */` |
|      115 |  4806 | `	nIdx = pTos->nIdx;` |
|      115 |  4807 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4808 | `		/* Nullify the object */` |
|       95 |  4809 | `		PH7_MemObjRelease(pTos);` |
|        - |  4810 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4811 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4812 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4813 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4814 | `	}` |
|      115 |  4815 | `	break;` |
|        - |  4816 | `					  }` |
|        - |  4817 | `/*` |
|        - |  4818 | ` * OP_STORE_REF * * P3` |
|        - |  4819 | ` * Perform an assignment operation by reference.` |
|        - |  4820 | ` */` |
|       15 |  4821 | ` case PH7_OP_STORE_REF: {` |
|       32 |  4822 | `	 SyString sName = { 0 , 0 };` |
|        - |  4823 | `	 VmFrame *pFrameLocal;` |
|        - |  4824 | `	SyHashEntry *pEntry;` |
|        - |  4825 | `	sxu32 nIdx;` |
|        - |  4826 | `#ifdef UNTRUST` |
|        - |  4827 | `	if( pTos < pStack ){` |
|        - |  4828 | `		goto Abort;` |
|        - |  4829 | `	}` |
|        - |  4830 | `#endif` |
|       32 |  4831 | `	if( pInstr->p3 == 0 ){` |
|        - |  4832 | `		char *zName;` |
|        - |  4833 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4834 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4835 | `			/* Force a string cast */` |
|      ! 0 |  4836 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4837 | `		}` |
|      ! 0 |  4838 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4839 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4840 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4841 | `			if( zName ){` |
|      ! 0 |  4842 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4843 | `			}` |
|      ! 0 |  4844 | `		}` |
|      ! 0 |  4845 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4846 | `		pTos--;` |
|      ! 0 |  4847 | `	}else{` |
|       32 |  4848 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4849 | `	}` |
|       32 |  4850 | `	nIdx = pTos->nIdx;` |
|       32 |  4851 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4852 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4853 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4854 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4855 | `		}else{` |
|        - |  4856 | `			ph7_value *pObj;` |
|        - |  4857 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4858 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4859 | `			if( pObj == 0 ){` |
|      ! 0 |  4860 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4861 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4862 | `				goto Abort;` |
|        - |  4863 | `			}` |
|        - |  4864 | `			/* Perform the store operation */` |
|      ! 0 |  4865 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4866 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4867 | `		}` |
|       32 |  4868 | `	}else if( sName.nByte > 0){` |
|       32 |  4869 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4870 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4871 | `		}else{` |
|       32 |  4872 | `			pFrameLocal = pVm->pFrame;` |
|       32 |  4873 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  4874 | `			/* Query the local frame */` |
|       32 |  4875 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       32 |  4876 | `			if( pEntry ){` |
|      ! 0 |  4877 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4878 | `			}else{` |
|       32 |  4879 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       32 |  4880 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4881 | `					/* Insert in the $GLOBALS array */` |
|       28 |  4882 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       13 |  4883 | `				}` |
|       32 |  4884 | `				if( rc == SXRET_OK ){` |
|       32 |  4885 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       15 |  4886 | `				}` |
|        - |  4887 | `			}` |
|        - |  4888 | `		}` |
|       15 |  4889 | `	}` |
|       32 |  4890 | `	break;` |
|        - |  4891 | `				 }` |
|        - |  4892 | `/*` |
|        - |  4893 | ` * OP_UPLINK P1 * *` |
|        - |  4894 | ` * Link a variable to the top active VM frame.` |
|        - |  4895 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4896 | ` */` |
|       25 |  4897 | `case PH7_OP_UPLINK: {` |
|       52 |  4898 | `	if( pVm->pFrame->pParent ){` |
|       52 |  4899 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4900 | `		SyString sName;` |
|        - |  4901 | `		/* Perform the link */` |
|      104 |  4902 | `		while( pLink <= pTos ){` |
|       54 |  4903 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4904 | `				/* Force a string cast */` |
|      ! 0 |  4905 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4906 | `			}` |
|       54 |  4907 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  4908 | `			if( sName.nByte > 0 ){` |
|       54 |  4909 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  4910 | `			}` |
|       54 |  4911 | `			pLink++;` |
|        2 |  4912 | `		}` |
|       25 |  4913 | `	}` |
|       52 |  4914 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  4915 | `	break;` |
|        - |  4916 | `					}` |
|        - |  4917 | `/*` |
|        - |  4918 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4919 | ` * Push an exception in the corresponding container so that` |
|        - |  4920 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4921 | ` */` |
|       29 |  4922 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       60 |  4923 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4924 | `	VmFrame *pFrameLocal;` |
|        - |  4925 | `	/* Reset per-entry state so finally runs on each iteration */` |
|       60 |  4926 | `	pException->iFinallyDone = 0;` |
|       60 |  4927 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4928 | `	/* Create the exception frame */` |
|       60 |  4929 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       60 |  4930 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4931 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4932 | `		goto Abort;` |
|        - |  4933 | `	}` |
|        - |  4934 | `	/* Mark the special frame */` |
|       60 |  4935 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       60 |  4936 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4937 | `	/* Point to the frame that trigger the exception */` |
|       60 |  4938 | `	pFrameLocal = pFrameLocal->pParent;` |
|       60 |  4939 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       60 |  4940 | `	pException->pFrame = pFrameLocal;` |
|       60 |  4941 | `	break;` |
|        - |  4942 | `							}` |
|        - |  4943 | `/*` |
|        - |  4944 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4945 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4946 | ` */` |
|       28 |  4947 | `case PH7_OP_POP_EXCEPTION: {` |
|       58 |  4948 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       58 |  4949 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4950 | `		ph7_exception **apException;` |
|        - |  4951 | `		/* Pop the loaded exception */` |
|       28 |  4952 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  4953 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  4954 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  4955 | `		}` |
|       13 |  4956 | `	}` |
|       58 |  4957 | `	pException->pFrame = 0;` |
|        - |  4958 | `	/* Leave the exception frame */` |
|       58 |  4959 | `	VmLeaveFrame(&(*pVm));` |
|        - |  4960 | `	/* Execute the finally block if present and not already executed by catch path */` |
|       58 |  4961 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  4962 | `		sxi32 rcFinally;` |
|       19 |  4963 | `		pException->iFinallyDone = 1;` |
|       19 |  4964 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       19 |  4965 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  4966 | `			goto Abort;` |
|        - |  4967 | `		}` |
|        9 |  4968 | `	}` |
|       58 |  4969 | `	break;` |
|        - |  4970 | `							}` |
|        - |  4971 |  |
|        - |  4972 | `/*` |
|        - |  4973 | ` * OP_THROW * P2 *` |
|        - |  4974 | ` * Throw an user exception.` |
|        - |  4975 | ` */` |
|       17 |  4976 | `case PH7_OP_THROW: {` |
|       36 |  4977 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       36 |  4978 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4979 | `#ifdef UNTRUST` |
|        - |  4980 | `	if( pTos < pStack ){` |
|        - |  4981 | `		goto Abort;` |
|        - |  4982 | `	}` |
|        - |  4983 | `#endif` |
|       36 |  4984 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  4985 | `	/* Tell the upper layer that an exception was thrown */` |
|       36 |  4986 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       36 |  4987 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       36 |  4988 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4989 | `		ph7_class *pException;` |
|        - |  4990 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4991 | `		 */` |
|       36 |  4992 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       36 |  4993 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4994 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4995 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4996 | `			if( rc == SXERR_ABORT ){` |
|        - |  4997 | `				/* Abort processing immediately */` |
|      ! 0 |  4998 | `				goto Abort;` |
|        - |  4999 | `			}` |
|      ! 0 |  5000 | `		}else{` |
|        - |  5001 | `			/* Throw the exception */` |
|       36 |  5002 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       36 |  5003 | `			if( rc == SXERR_ABORT ){` |
|        - |  5004 | `				/* Abort processing immediately */` |
|        9 |  5005 | `				goto Abort;` |
|        - |  5006 | `			}` |
|        - |  5007 | `		}` |
|       15 |  5008 | `	}else{` |
|        - |  5009 | `		/* Expecting a class instance */` |
|      ! 0 |  5010 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  5011 | `		if( rc == SXERR_ABORT ){` |
|        - |  5012 | `			/* Abort processing immediately */` |
|      ! 0 |  5013 | `			goto Abort;` |
|        - |  5014 | `		}` |
|        - |  5015 | `	}` |
|        - |  5016 | `	/* Pop the top entry */` |
|       28 |  5017 | `	VmPopOperand(&pTos,1);` |
|        - |  5018 | `	/* Perform an unconditional jump */` |
|       28 |  5019 | `	pc = nJump - 1;` |
|       28 |  5020 | `	break;` |
|        - |  5021 | `				   }` |
|        - |  5022 | `/*` |
|        - |  5023 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  5024 | ` * Prepare a foreach step.` |
|        - |  5025 | ` */` |
|     4893 |  5026 | `case PH7_OP_FOREACH_INIT: {` |
|     9788 |  5027 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5028 | `	void *pName;` |
|        - |  5029 | `#ifdef UNTRUST` |
|        - |  5030 | `	if( pTos < pStack ){` |
|        - |  5031 | `		goto Abort;` |
|        - |  5032 | `	}` |
|        - |  5033 | `#endif` |
|     9788 |  5034 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5035 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  5036 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5037 | `			/* Force a string cast */` |
|      ! 0 |  5038 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5039 | `		}` |
|        - |  5040 | `		/* Duplicate name */` |
|      ! 0 |  5041 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5042 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5043 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5044 | `		}` |
|      ! 0 |  5045 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5046 | `	}` |
|     9788 |  5047 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  5048 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5049 | `			/* Force a string cast */` |
|      ! 0 |  5050 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5051 | `		}` |
|        - |  5052 | `		/* Duplicate name */` |
|      ! 0 |  5053 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5054 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5055 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5056 | `		}` |
|      ! 0 |  5057 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5058 | `	}` |
|        - |  5059 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     9788 |  5060 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5061 | `		/* Jump out of the loop */` |
|      ! 0 |  5062 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5063 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  5064 | `		}` |
|      ! 0 |  5065 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  5066 | `	}else{` |
|        - |  5067 | `		ph7_foreach_step *pStep;` |
|     9788 |  5068 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     9788 |  5069 | `		if( pStep == 0 ){` |
|      ! 0 |  5070 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  5071 | `			/* Jump out of the loop */` |
|      ! 0 |  5072 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5073 | `		}else{` |
|        - |  5074 | `			/* Zero the structure */` |
|     9788 |  5075 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  5076 | `			/* Prepare the step */` |
|     9788 |  5077 | `			pStep->iFlags = pInfo->iFlags;` |
|     9788 |  5078 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5079 | `				ph7_hashmap *pMap;` |
|        - |  5080 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  5081 | `				 * source array so mutations don't affect other sharers. */` |
|     9772 |  5082 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|       10 |  5083 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|       10 |  5084 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|       10 |  5085 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5086 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  5087 | `						 * variable still points at the same hashmap as` |
|        - |  5088 | `						 * the stack value. */` |
|       10 |  5089 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|       10 |  5090 | `							pCur->iRef--;` |
|       10 |  5091 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|       10 |  5092 | `							pTos->x.pOther = pBacking->x.pOther;` |
|       10 |  5093 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  5094 | `						}` |
|        4 |  5095 | `					}` |
|        4 |  5096 | `				}` |
|     9772 |  5097 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5098 | `				/* Reset the internal loop cursor */` |
|     9772 |  5099 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5100 | `				/* Mark the step */` |
|     9772 |  5101 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     9772 |  5102 | `				pStep->xIter.pMap = pMap;` |
|     9772 |  5103 | `				pMap->iRef++;` |
|     4887 |  5104 | `			}else{` |
|       18 |  5105 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5106 | `				ph7_class *pIteratorClass;` |
|        - |  5107 | `				/* Check if the object implements Iterator */` |
|       18 |  5108 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       21 |  5109 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  5110 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  5111 | `					ph7_class_method *pRewind;` |
|        7 |  5112 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        7 |  5113 | `					pStep->xIter.pThis = pThis;` |
|        7 |  5114 | `					pThis->iRef++;` |
|        7 |  5115 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|        7 |  5116 | `					if( pRewind ){` |
|        7 |  5117 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|        3 |  5118 | `					}` |
|        4 |  5119 | `				}else{` |
|        - |  5120 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  5121 | `					ph7_class *pIterAggClass;` |
|       12 |  5122 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  5123 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  5124 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  5125 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  5126 | `						ph7_class_method *pGetIter;` |
|        3 |  5127 | `						int iterAggOk = 0;` |
|        3 |  5128 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  5129 | `						if( pGetIter ){` |
|        - |  5130 | `							ph7_value sResult;` |
|        3 |  5131 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  5132 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  5133 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  5134 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  5135 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  5136 | `									ph7_class_method *pRewind;` |
|        3 |  5137 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  5138 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  5139 | `									pIterObj->iRef++;` |
|        - |  5140 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  5141 | `									pStep->pOwner = pThis;` |
|        3 |  5142 | `									pThis->iRef++;` |
|        3 |  5143 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  5144 | `									if( pRewind ){` |
|        3 |  5145 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  5146 | `									}` |
|        3 |  5147 | `									iterAggOk = 1;` |
|        1 |  5148 | `								}` |
|        1 |  5149 | `							}` |
|        3 |  5150 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  5151 | `						}` |
|        3 |  5152 | `						if( !iterAggOk ){` |
|        - |  5153 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  5154 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5155 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  5156 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  5157 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  5158 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  5159 | `						}` |
|        2 |  5160 | `					}else{` |
|        - |  5161 | `						/* Plain object iteration via hAttr */` |
|        9 |  5162 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  5163 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  5164 | `						pStep->xIter.pThis = pThis;` |
|        9 |  5165 | `						pThis->iRef++;` |
|        - |  5166 | `					}` |
|        - |  5167 | `				}` |
|        - |  5168 | `			}` |
|        - |  5169 | `		}` |
|     9788 |  5170 | `		if( pStep ){` |
|     9788 |  5171 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5172 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5173 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5174 | `				/* Jump out of the loop */` |
|      ! 0 |  5175 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  5176 | `			}` |
|     4893 |  5177 | `		}` |
|        - |  5178 | `	}` |
|     9788 |  5179 | `	VmPopOperand(&pTos,1);` |
|     9788 |  5180 | `	break;` |
|        - |  5181 | `						  }` |
|        - |  5182 | `/*` |
|        - |  5183 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5184 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5185 | ` */` |
|    78749 |  5186 | `case PH7_OP_FOREACH_STEP: {` |
|   157500 |  5187 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5188 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5189 | `	ph7_value *pValue;` |
|        - |  5190 | `	VmFrame *pFrameLocal;` |
|        - |  5191 | `	/* Peek the last step */` |
|   157500 |  5192 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   157500 |  5193 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   157500 |  5194 | `	pFrameLocal = pVm->pFrame;` |
|   157500 |  5195 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   157500 |  5196 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   157440 |  5197 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5198 | `		ph7_hashmap_node *pNode;` |
|        - |  5199 | `		/* Extract the current node value */` |
|   157440 |  5200 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   157440 |  5201 | `		if( pNode == 0 ){` |
|        - |  5202 | `			/* No more entry to process */` |
|     9770 |  5203 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     9770 |  5204 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5205 | `				/* Break the reference with the last element */` |
|        7 |  5206 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  5207 | `			}` |
|        - |  5208 | `			/* Automatically reset the loop cursor */` |
|     9770 |  5209 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5210 | `			/* Cleanup the mess left behind */` |
|     9770 |  5211 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     9770 |  5212 | `			SySetPop(&pInfo->aStep);` |
|     9770 |  5213 | `			PH7_HashmapUnref(pMap);` |
|     4886 |  5214 | `		}else{` |
|   147672 |  5215 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  5216 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  5217 | `				if( pKey ){` |
|      416 |  5218 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  5219 | `				}` |
|      207 |  5220 | `			}` |
|   147672 |  5221 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5222 | `				SyHashEntry *pEntry;` |
|        - |  5223 | `				/* Pass by reference */` |
|       24 |  5224 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       24 |  5225 | `				if( pEntry ){` |
|       22 |  5226 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       12 |  5227 | `				}else{` |
|        4 |  5228 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  5229 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5230 | `				}` |
|       13 |  5231 | `			}else{` |
|        - |  5232 | `				/* Make a copy of the entry value */` |
|   147650 |  5233 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   147650 |  5234 | `				if( pValue ){` |
|   147650 |  5235 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    73824 |  5236 | `				}` |
|        - |  5237 | `			}` |
|        2 |  5238 | `		}` |
|    78781 |  5239 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  5240 | `		/* Iterator-based iteration.` |
|        - |  5241 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  5242 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  5243 | `		 */` |
|       37 |  5244 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  5245 | `		ph7_class_method *pMethod;` |
|        - |  5246 | `		ph7_value sResult;` |
|       37 |  5247 | `		int isValid = 0;` |
|        - |  5248 | `		/* Call next() to advance — but skip on the first iteration */` |
|       37 |  5249 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|        9 |  5250 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|        5 |  5251 | `		}else{` |
|       29 |  5252 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       29 |  5253 | `			if( pMethod ){` |
|       29 |  5254 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       14 |  5255 | `			}` |
|        - |  5256 | `		}` |
|        - |  5257 | `		/* Call valid() */` |
|       37 |  5258 | `		PH7_MemObjInit(pVm,&sResult);` |
|       37 |  5259 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       37 |  5260 | `		if( pMethod ){` |
|       37 |  5261 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       37 |  5262 | `			PH7_MemObjToBool(&sResult);` |
|       37 |  5263 | `			isValid = (sResult.x.iVal != 0);` |
|       18 |  5264 | `		}` |
|       37 |  5265 | `		PH7_MemObjRelease(&sResult);` |
|       37 |  5266 | `		if( !isValid ){` |
|        - |  5267 | `			/* Iterator exhausted */` |
|        7 |  5268 | `			pc = pInstr->iP2 - 1;` |
|        - |  5269 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|        7 |  5270 | `			if( pStep->pOwner ){` |
|        3 |  5271 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  5272 | `			}` |
|        7 |  5273 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        7 |  5274 | `			SySetPop(&pInfo->aStep);` |
|        7 |  5275 | `			PH7_ClassInstanceUnref(pThis);` |
|        4 |  5276 | `		}else{` |
|        - |  5277 | `			/* Call current() to get value */` |
|       31 |  5278 | `			PH7_MemObjInit(pVm,&sResult);` |
|       31 |  5279 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       31 |  5280 | `			if( pMethod ){` |
|       31 |  5281 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       15 |  5282 | `			}` |
|       31 |  5283 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       31 |  5284 | `			if( pValue ){` |
|       31 |  5285 | `				PH7_MemObjStore(&sResult,pValue);` |
|       15 |  5286 | `			}` |
|       31 |  5287 | `			PH7_MemObjRelease(&sResult);` |
|        - |  5288 | `			/* Call key() if needed */` |
|       31 |  5289 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  5290 | `				ph7_value sKey;` |
|       23 |  5291 | `				PH7_MemObjInit(pVm,&sKey);` |
|       23 |  5292 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       23 |  5293 | `				if( pMethod ){` |
|       23 |  5294 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       11 |  5295 | `				}` |
|       23 |  5296 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       23 |  5297 | `				if( pValue ){` |
|       23 |  5298 | `					PH7_MemObjStore(&sKey,pValue);` |
|       11 |  5299 | `				}` |
|       23 |  5300 | `				PH7_MemObjRelease(&sKey);` |
|       11 |  5301 | `			}` |
|        - |  5302 | `		}` |
|       19 |  5303 | `	}else{` |
|       25 |  5304 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5305 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5306 | `		SyHashEntry *pEntry;` |
|        - |  5307 | `		/* Point to the next attribute */` |
|       29 |  5308 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5309 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5310 | `			/* Check access permission */` |
|       31 |  5311 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5312 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5313 | `					break; /* Access is granted */` |
|        - |  5314 | `			}` |
|        1 |  5315 | `		}` |
|       25 |  5316 | `		if( pEntry == 0 ){` |
|        - |  5317 | `			/* Clean up the mess left behind */` |
|        9 |  5318 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5319 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5320 | `				/* Break the reference with the last element */` |
|        3 |  5321 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5322 | `			}` |
|        9 |  5323 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5324 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5325 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5326 | `		}else{` |
|       17 |  5327 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5328 | `			ph7_value *pAttrValue;` |
|       17 |  5329 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5330 | `				/* Fill with the current attribute name */` |
|       17 |  5331 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5332 | `				if( pKey ){` |
|       17 |  5333 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5334 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5335 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5336 | `				}` |
|        8 |  5337 | `			}` |
|        - |  5338 | `			/* Extract attribute value */` |
|       17 |  5339 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5340 | `			if( pAttrValue ){` |
|       17 |  5341 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5342 | `					/* Pass by reference */` |
|        3 |  5343 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5344 | `					if( pEntry ){` |
|        3 |  5345 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5346 | `					}else{` |
|      ! 0 |  5347 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5348 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5349 | `					}` |
|        2 |  5350 | `				}else{` |
|        - |  5351 | `					/* Make a copy of the attribute value */` |
|       15 |  5352 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5353 | `					if( pValue ){` |
|       15 |  5354 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5355 | `					}` |
|        - |  5356 | `				}` |
|        8 |  5357 | `			}` |
|        - |  5358 | `		}` |
|        - |  5359 | `	}` |
|   157500 |  5360 | `	break;` |
|        - |  5361 | `						  }` |
|        - |  5362 | `/*` |
|        - |  5363 | ` * OP_MEMBER P1 P2` |
|        - |  5364 | ` * Load class attribute/method on the stack.` |
|        - |  5365 | ` */` |
|     2079 |  5366 | `case PH7_OP_MEMBER: {` |
|        - |  5367 | `	ph7_class_instance *pThis;` |
|        - |  5368 | `	ph7_value *pNos;` |
|        - |  5369 | `	SyString sName;` |
|     4160 |  5370 | `	if( !pInstr->iP1 ){` |
|     4062 |  5371 | `		pNos = &pTos[-1];` |
|        - |  5372 | `#ifdef UNTRUST` |
|        - |  5373 | `		if( pNos < pStack ){` |
|        - |  5374 | `			goto Abort;` |
|        - |  5375 | `		}` |
|        - |  5376 | `#endif` |
|     4062 |  5377 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5378 | `			ph7_class *pClass;` |
|        - |  5379 | `			/* Class already instantiated */` |
|     4062 |  5380 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5381 | `			/* Point to the instantiated class */` |
|     4062 |  5382 | `			pClass = pThis->pClass;` |
|        - |  5383 | `			/* Extract attribute name first */` |
|     4062 |  5384 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     4062 |  5385 | `			if( pInstr->iP2 ){` |
|        - |  5386 | `				/* Method call */` |
|      278 |  5387 | `				ph7_class_method *pMeth = 0;` |
|      278 |  5388 | `				if( sName.nByte > 0 ){` |
|        - |  5389 | `					/* Extract the target method */` |
|      278 |  5390 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      138 |  5391 | `				}` |
|      278 |  5392 | `				if( pMeth == 0 ){` |
|      ! 0 |  5393 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5394 | `						&pClass->sName,&sName` |
|        - |  5395 | `						);` |
|        - |  5396 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5397 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5398 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5399 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5400 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5401 | `				}else{` |
|        - |  5402 | `					/* Push method name on the stack */` |
|      278 |  5403 | `					PH7_MemObjRelease(pTos);` |
|      278 |  5404 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      278 |  5405 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5406 | `				}` |
|      278 |  5407 | `				pTos->nIdx = SXU32_HIGH;` |
|      140 |  5408 | `			}else{` |
|        - |  5409 | `				/* Attribute access */` |
|     3786 |  5410 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5411 | `				SyHashEntry *pEntry;` |
|        - |  5412 | `				/* Extract the target attribute */` |
|     3786 |  5413 | `				if( sName.nByte > 0 ){` |
|     3786 |  5414 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3786 |  5415 | `					if( pEntry ){` |
|        - |  5416 | `						/* Point to the attribute value */` |
|     3784 |  5417 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1891 |  5418 | `					}` |
|     1892 |  5419 | `				}` |
|     3786 |  5420 | `				if( pObjAttr == 0 ){` |
|        - |  5421 | `					/* No such attribute,load null */` |
|        4 |  5422 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5423 | `						&pClass->sName,&sName);` |
|        - |  5424 | `					/* Call the __get magic method if available */` |
|        3 |  5425 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5426 | `				}` |
|     3786 |  5427 | `				VmPopOperand(&pTos,1);` |
|        - |  5428 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5429 | `				 * This is due to the following case:` |
|        - |  5430 | `				 *     (new TestClass())->foo;` |
|        - |  5431 | `				 */` |
|     3786 |  5432 | `				pThis->iRef++;` |
|     3786 |  5433 | `				PH7_MemObjRelease(pTos);` |
|     3786 |  5434 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3786 |  5435 | `				if( pObjAttr ){` |
|     3784 |  5436 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5437 | `					/* Check attribute access */` |
|     3784 |  5438 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5439 | `						/* Load attribute */` |
|     3784 |  5440 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3784 |  5441 | `						if( pValue ){` |
|     3784 |  5442 | `							if( pThis->iRef < 2 ){` |
|        - |  5443 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5444 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5445 | `								 */` |
|        3 |  5446 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5447 | `							}else{` |
|        - |  5448 | `								/* Simple load */` |
|     3782 |  5449 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5450 | `							}` |
|     3784 |  5451 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3782 |  5452 | `								if( pThis->iRef > 1 ){` |
|        - |  5453 | `									/* Load attribute index */` |
|     3780 |  5454 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1889 |  5455 | `								}` |
|     1890 |  5456 | `							}` |
|     1891 |  5457 | `						}` |
|     1891 |  5458 | `					}` |
|     1891 |  5459 | `				}` |
|        - |  5460 | `				/* Safely unreference the object */` |
|     3786 |  5461 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5462 | `			}` |
|     2032 |  5463 | `		}else{` |
|      ! 0 |  5464 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5465 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5466 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5467 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5468 | `		}` |
|     2032 |  5469 | `	}else{` |
|        - |  5470 | `		/* Static member access using class name */` |
|      100 |  5471 | `		pNos = pTos;` |
|      100 |  5472 | `		pThis = 0;` |
|      100 |  5473 | `		if( !pInstr->p3 ){` |
|       88 |  5474 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       88 |  5475 | `			pNos--;` |
|        - |  5476 | `#ifdef UNTRUST` |
|        - |  5477 | `			if( pNos < pStack ){` |
|        - |  5478 | `				goto Abort;` |
|        - |  5479 | `			}` |
|        - |  5480 | `#endif` |
|       45 |  5481 | `		}else{` |
|        - |  5482 | `			/* Attribute name already computed */` |
|       14 |  5483 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5484 | `		}` |
|      100 |  5485 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      100 |  5486 | `			ph7_class *pClass = 0;` |
|      100 |  5487 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5488 | `				/* Class already instantiated */` |
|      ! 0 |  5489 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5490 | `				pClass = pThis->pClass;` |
|      ! 0 |  5491 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5492 | `			}else{` |
|        - |  5493 | `				/* Try to extract the target class */` |
|      100 |  5494 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      100 |  5495 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      100 |  5496 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5497 | `					/* Handle self/static/parent keywords */` |
|      100 |  5498 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       30 |  5499 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       30 |  5500 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  5501 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  5502 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  5503 | `						}` |
|       86 |  5504 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       16 |  5505 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       71 |  5506 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       14 |  5507 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       14 |  5508 | `						if( pSelf && pSelf->pBase ){` |
|       14 |  5509 | `							pClass = pSelf->pBase;` |
|        6 |  5510 | `						}` |
|        8 |  5511 | `					}else{` |
|       46 |  5512 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5513 | `					}` |
|       49 |  5514 | `				}` |
|        - |  5515 | `			}` |
|      100 |  5516 | `			if( pClass == 0 ){` |
|        - |  5517 | `				/* Undefined class */` |
|      ! 0 |  5518 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5519 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5520 | `					);` |
|      ! 0 |  5521 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5522 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5523 | `				}` |
|      ! 0 |  5524 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5525 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5526 | `			}else{` |
|      100 |  5527 | `				if( pInstr->iP2 ){` |
|        - |  5528 | `					/* Method call */` |
|       30 |  5529 | `					ph7_class_method *pMeth = 0;` |
|       30 |  5530 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5531 | `						/* Extract the target method */` |
|       30 |  5532 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       14 |  5533 | `					}` |
|       30 |  5534 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5535 | `						if( pMeth ){` |
|      ! 0 |  5536 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5537 | `								&pClass->sName,&sName` |
|        - |  5538 | `								);` |
|      ! 0 |  5539 | `						}else{` |
|      ! 0 |  5540 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5541 | `								&pClass->sName,&sName` |
|        - |  5542 | `								);` |
|        - |  5543 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5544 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5545 | `						}` |
|        - |  5546 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5547 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5548 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5549 | `						}` |
|      ! 0 |  5550 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5551 | `					}else{` |
|        - |  5552 | `						/* Push method name on the stack */` |
|       30 |  5553 | `						PH7_MemObjRelease(pTos);` |
|       30 |  5554 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       30 |  5555 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5556 | `					}` |
|       30 |  5557 | `					pTos->nIdx = SXU32_HIGH;` |
|       16 |  5558 | `				}else{` |
|        - |  5559 | `					/* Attribute access */` |
|       72 |  5560 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5561 | `					/* Check for special ::class pseudo-constant */` |
|      104 |  5562 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       64 |  5563 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5564 | `						/* ::class returns the fully qualified class name */` |
|        - |  5565 | `						/* Pop the attribute name from the stack */` |
|       54 |  5566 | `						if( !pInstr->p3 ){` |
|       54 |  5567 | `							VmPopOperand(&pTos,1);` |
|       26 |  5568 | `						}` |
|       54 |  5569 | `						PH7_MemObjRelease(pTos);` |
|        - |  5570 | `						/* Load the class name */` |
|       54 |  5571 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       54 |  5572 | `						pTos->nIdx = SXU32_HIGH;` |
|       28 |  5573 | `					}else{` |
|        - |  5574 | `						/* Extract the target attribute */` |
|       20 |  5575 | `						if( sName.nByte > 0 ){` |
|       20 |  5576 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        9 |  5577 | `						}` |
|       20 |  5578 | `						if( pAttr == 0 ){` |
|        - |  5579 | `							/* No such attribute,load null */` |
|      ! 0 |  5580 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5581 | `								&pClass->sName,&sName);` |
|        - |  5582 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5583 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5584 | `						}` |
|        - |  5585 | `						/* Pop the attribute name from the stack */` |
|       20 |  5586 | `						if( !pInstr->p3 ){` |
|        7 |  5587 | `							VmPopOperand(&pTos,1);` |
|        3 |  5588 | `						}` |
|       20 |  5589 | `						PH7_MemObjRelease(pTos);` |
|       20 |  5590 | `						pTos->nIdx = SXU32_HIGH;` |
|       20 |  5591 | `						if( pAttr ){` |
|       20 |  5592 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5593 | `								/* Access to a non static attribute */` |
|      ! 0 |  5594 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5595 | `									&pClass->sName,&pAttr->sName` |
|        - |  5596 | `									);` |
|      ! 0 |  5597 | `							}else{` |
|        - |  5598 | `								ph7_value *pValue;` |
|        - |  5599 | `								/* Check if the access to the attribute is allowed */` |
|       20 |  5600 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5601 | `									/* Load the desired attribute */` |
|       20 |  5602 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       20 |  5603 | `									if( pValue ){` |
|       20 |  5604 | `										PH7_MemObjLoad(pValue,pTos);` |
|       20 |  5605 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5606 | `											/* Load index number */` |
|       14 |  5607 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5608 | `										}` |
|        9 |  5609 | `									}` |
|        9 |  5610 | `								}` |
|        - |  5611 | `							}` |
|        9 |  5612 | `						}` |
|        - |  5613 | `					}` |
|        - |  5614 | `				}` |
|      100 |  5615 | `				if( pThis ){` |
|        - |  5616 | `					/* Safely unreference the object */` |
|      ! 0 |  5617 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5618 | `				}` |
|        - |  5619 | `			}` |
|       51 |  5620 | `		}else{` |
|        - |  5621 | `			/* Pop operands */` |
|      ! 0 |  5622 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5623 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5624 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5625 | `			}` |
|      ! 0 |  5626 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5627 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5628 | `		}` |
|        - |  5629 | `	}` |
|     4160 |  5630 | `	break;` |
|        - |  5631 | `					}` |
|        - |  5632 | `/*` |
|        - |  5633 | ` * OP_NEW P1 * * *` |
|        - |  5634 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5635 | ` */` |
|      305 |  5636 | `case PH7_OP_NEW: {` |
|      612 |  5637 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      612 |  5638 | `	ph7_class *pClass = 0;` |
|        - |  5639 | `	ph7_class_instance *pNew;` |
|      612 |  5640 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5641 | `		/* Try to extract the desired class */` |
|      917 |  5642 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      610 |  5643 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      305 |  5644 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5645 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5646 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5647 | `	}` |
|      612 |  5648 | `	if( pClass == 0 ){` |
|        - |  5649 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  5650 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  5651 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5652 | `			);` |
|        - |  5653 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  5654 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5655 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5656 | `			/* Pop given arguments */` |
|      ! 0 |  5657 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5658 | `		}` |
|      ! 0 |  5659 | `		goto Abort;` |
|      ! 0 |  5660 | `	}else{` |
|        - |  5661 | `		ph7_class_method *pCons;` |
|        - |  5662 | `		/* Create a new class instance */` |
|      612 |  5663 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      612 |  5664 | `		if( pNew == 0 ){` |
|      ! 0 |  5665 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5666 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5667 | `				&pClass->sName` |
|        - |  5668 | `			);` |
|      ! 0 |  5669 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5670 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5671 | `				/* Pop given arguments */` |
|      ! 0 |  5672 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5673 | `			}` |
|      ! 0 |  5674 | `			break;` |
|        - |  5675 | `		}` |
|        - |  5676 | `		/* Check if a constructor is available */` |
|      612 |  5677 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      612 |  5678 | `		if( pCons == 0 ){` |
|      528 |  5679 | `			SyString *pName = &pClass->sName;` |
|        - |  5680 | `			/* Check for a constructor with the same base class name */` |
|      528 |  5681 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      263 |  5682 | `		}` |
|      612 |  5683 | `		if( pCons ){` |
|        - |  5684 | `			/* Call the class constructor */` |
|       86 |  5685 | `			SySetReset(&aArg);` |
|      160 |  5686 | `			while( pArg < pTos ){` |
|       76 |  5687 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       76 |  5688 | `				pArg++;` |
|        2 |  5689 | `			}` |
|       86 |  5690 | `			if( pVm->bErrReport ){` |
|        - |  5691 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5692 | `				sxu32 n;` |
|       43 |  5693 | `				n = SySetUsed(&aArg);` |
|        - |  5694 | `				/* Emit a notice for missing arguments */` |
|       95 |  5695 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       53 |  5696 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       53 |  5697 | `					if( pFuncArg ){` |
|       53 |  5698 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5699 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5700 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5701 | `						}` |
|       26 |  5702 | `					}` |
|       53 |  5703 | `					n++;` |
|        1 |  5704 | `				}` |
|       21 |  5705 | `			}` |
|       86 |  5706 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5707 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       86 |  5708 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5709 | `				pNew->iRef = 1;` |
|      ! 0 |  5710 | `			}` |
|       42 |  5711 | `		}` |
|      612 |  5712 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5713 | `			/* Pop given arguments */` |
|       68 |  5714 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       33 |  5715 | `		}` |
|      612 |  5716 | `		PH7_MemObjRelease(pTos);` |
|      612 |  5717 | `		pTos->x.pOther = pNew;` |
|      612 |  5718 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5719 | `	}` |
|      612 |  5720 | `	break;` |
|        - |  5721 | `				 }` |
|        - |  5722 | `/*` |
|        - |  5723 | ` * OP_CLONE * * *` |
|        - |  5724 | ` * Perfome a clone operation.` |
|        - |  5725 | ` */` |
|       23 |  5726 | `case PH7_OP_CLONE: {` |
|        - |  5727 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5728 | `#ifdef UNTRUST` |
|        - |  5729 | `	if( pTos < pStack ){` |
|        - |  5730 | `		goto Abort;` |
|        - |  5731 | `	}` |
|        - |  5732 | `#endif` |
|        - |  5733 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5734 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5735 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5736 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5737 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5738 | `		break;` |
|        - |  5739 | `	}` |
|        - |  5740 | `	/* Point to the source */` |
|       44 |  5741 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5742 | `	/* Perform the clone operation */` |
|       44 |  5743 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5744 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5745 | `	if( pClone == 0 ){` |
|      ! 0 |  5746 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5747 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5748 | `	}else{` |
|        - |  5749 | `		/* Load the cloned object */` |
|       44 |  5750 | `		pTos->x.pOther = pClone;` |
|       44 |  5751 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5752 | `	}` |
|       44 |  5753 | `	break;` |
|        - |  5754 | `				   }` |
|        - |  5755 | `/*` |
|        - |  5756 | ` * OP_SWITCH * * P3` |
|        - |  5757 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5758 | ` */` |
|       18 |  5759 | `case PH7_OP_SWITCH: {` |
|       38 |  5760 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5761 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5762 | `	ph7_value sValue,sCaseValue;` |
|        - |  5763 | `	sxu32 n,nEntry;` |
|        - |  5764 | `#ifdef UNTRUST` |
|        - |  5765 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5766 | `		goto Abort;` |
|        - |  5767 | `	}` |
|        - |  5768 | `#endif` |
|        - |  5769 | `	/* Point to the case table  */` |
|       38 |  5770 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5771 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5772 | `	/* Select the appropriate case block to execute */` |
|       38 |  5773 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5774 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5775 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5776 | `		pCase = &aCase[n];` |
|       92 |  5777 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5778 | `		/* Execute the case expression first */` |
|       92 |  5779 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5780 | `		/* Compare the two expression */` |
|       92 |  5781 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5782 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5783 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5784 | `		if( rc == 0 ){` |
|        - |  5785 | `			/* Value match,jump to this block */` |
|       38 |  5786 | `			pc = pCase->nStart - 1;` |
|       38 |  5787 | `			break;` |
|        - |  5788 | `		}` |
|       29 |  5789 | `	}` |
|       38 |  5790 | `	VmPopOperand(&pTos,1);` |
|       38 |  5791 | `	if( n >= nEntry ){` |
|        - |  5792 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5793 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5794 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5795 | `		}else{` |
|        - |  5796 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5797 | `			pc = pSwitch->nOut - 1;` |
|        - |  5798 | `		}` |
|      ! 0 |  5799 | `	}` |
|       38 |  5800 | `	break;` |
|        - |  5801 | `					}` |
|        - |  5802 | `/*` |
|        - |  5803 | ` * OP_CALL P1 * *` |
|        - |  5804 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5805 | ` *  function on the stack.` |
|        - |  5806 | ` */` |
|   286958 |  5807 | `case PH7_OP_CALL: {` |
|   573962 |  5808 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5809 | `	SyHashEntry *pEntry;` |
|        - |  5810 | `	SyString sName;` |
|        - |  5811 | `	/* Extract function name */` |
|   573962 |  5812 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5813 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5814 | `			ph7_value sResult;` |
|      ! 0 |  5815 | `			SySetReset(&aArg);` |
|      ! 0 |  5816 | `			while( pArg < pTos ){` |
|      ! 0 |  5817 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5818 | `				pArg++;` |
|      ! 0 |  5819 | `			}` |
|      ! 0 |  5820 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5821 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5822 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5823 | `			SySetReset(&aArg);` |
|        - |  5824 | `			/* Pop given arguments */` |
|      ! 0 |  5825 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5826 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5827 | `			}` |
|        - |  5828 | `			/* Copy result */` |
|      ! 0 |  5829 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5830 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5831 | `		}else{` |
|        3 |  5832 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5833 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5834 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5835 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5836 | `			}else{` |
|        - |  5837 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5838 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5839 | `			}` |
|        - |  5840 | `			/* Pop given arguments */` |
|        3 |  5841 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5842 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5843 | `			}` |
|        - |  5844 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5845 | `			PH7_MemObjRelease(pTos);` |
|        - |  5846 | `		}` |
|   286725 |  5847 | `		break;` |
|        - |  5848 | `	}` |
|   573960 |  5849 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5850 | `	/* Check for a compiled function first.` |
|        - |  5851 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  5852 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   573960 |  5853 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  5854 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  5855 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  5856 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  5857 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  5858 | `	 * function calls inside namespaces. */` |
|   573960 |  5859 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  5860 | `		const char *zFunc;` |
|        - |  5861 | `		const char *zEnd;` |
|        - |  5862 | `		const char *z;` |
|        - |  5863 | `		SyString sGlobal;` |
|       15 |  5864 | `		zFunc = sName.zString;` |
|       15 |  5865 | `		zEnd  = zFunc + sName.nByte;` |
|       15 |  5866 | `		z = zEnd;` |
|        - |  5867 | `		/* Find last namespace separator */` |
|      133 |  5868 | `		while( z > zFunc ){` |
|      133 |  5869 | `			if( z[-1] == '\\' ){` |
|       15 |  5870 | `				break;` |
|        - |  5871 | `			}` |
|      119 |  5872 | `			z--;` |
|        1 |  5873 | `		}` |
|       15 |  5874 | `		if( z > zFunc && z < zEnd ){` |
|        - |  5875 | `			/* Retry lookup using the unqualified/global function name */` |
|       15 |  5876 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       15 |  5877 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        7 |  5878 | `		}` |
|        7 |  5879 | `	}` |
|   573960 |  5880 | `	if( pEntry ){` |
|        - |  5881 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5882 | `		ph7_class_instance *pThis;` |
|        - |  5883 | `		ph7_value *pFrameStack;` |
|        - |  5884 | `		ph7_vm_func *pVmFunc;` |
|        - |  5885 | `		ph7_class *pSelf;` |
|        - |  5886 | `		VmFrame *pFrame;` |
|        - |  5887 | `		ph7_value *pObj;` |
|        - |  5888 | `		VmSlot sArg;` |
|        - |  5889 | `		sxu32 n;` |
|        - |  5890 | `		/* initialize fields */` |
|    12644 |  5891 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    12644 |  5892 | `		pThis = 0;` |
|    12644 |  5893 | `		pSelf = 0;` |
|    12644 |  5894 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5895 | `			ph7_class_method *pMeth;` |
|        - |  5896 | `			/* Class method call */` |
|     1604 |  5897 | `			ph7_value *pTarget = &pTos[-1];` |
|     1604 |  5898 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5899 | `				/* Extract the 'this' pointer */` |
|     1604 |  5900 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5901 | `					/* Instance already loaded */` |
|     1570 |  5902 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1570 |  5903 | `					pThis->iRef++;` |
|     1570 |  5904 | `					pSelf = pThis->pClass;` |
|      784 |  5905 | `				}` |
|     1604 |  5906 | `				if( pSelf == 0 ){` |
|       36 |  5907 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5908 | `						/* "Late Static Binding" class name */` |
|       44 |  5909 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       14 |  5910 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       14 |  5911 | `					}` |
|       36 |  5912 | `					if( pSelf == 0 ){` |
|       13 |  5913 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        6 |  5914 | `					}` |
|       17 |  5915 | `				}` |
|     1604 |  5916 | `				if( pThis == 0  ){` |
|       36 |  5917 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       36 |  5918 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       36 |  5919 | `					if( pFrameLocal->pParent ){` |
|        - |  5920 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5921 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5922 | `						if( pThis ){` |
|       13 |  5923 | `							pThis->iRef++;` |
|        6 |  5924 | `						}` |
|        9 |  5925 | `					}` |
|       17 |  5926 | `				}` |
|     1604 |  5927 | `				VmPopOperand(&pTos,1);` |
|     1604 |  5928 | `				PH7_MemObjRelease(pTos);` |
|        - |  5929 | `				/* Synchronize pointers */` |
|     1604 |  5930 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5931 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5932 | `				 * user have already computed the random generated unique class method name` |
|        - |  5933 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5934 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5935 | `				 */` |
|     1604 |  5936 | `				while( pArg < pStack ){` |
|      ! 0 |  5937 | `					pArg++;` |
|      ! 0 |  5938 | `				}` |
|     1604 |  5939 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5940 | `					/* Check if the call is allowed */` |
|     1604 |  5941 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1604 |  5942 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        8 |  5943 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5944 | `							/* Pop given arguments */` |
|      ! 0 |  5945 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5946 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5947 | `							}` |
|        - |  5948 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5949 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5950 | `							break;` |
|        - |  5951 | `						}` |
|        3 |  5952 | `					}` |
|      801 |  5953 | `				}` |
|      801 |  5954 | `			}` |
|      801 |  5955 | `		}` |
|        - |  5956 | `		/* Check The recursion limit */` |
|    12644 |  5957 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5958 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5959 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5960 | `				&pVmFunc->sName);` |
|        - |  5961 | `			/* Pop given arguments */` |
|        3 |  5962 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5963 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5964 | `			}` |
|        - |  5965 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5966 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5967 | `			break;` |
|        - |  5968 | `		}` |
|    12642 |  5969 | `		if( pVmFunc->pNextName ){` |
|        - |  5970 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      134 |  5971 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       66 |  5972 | `		}` |
|        - |  5973 | `		/* Extract the formal argument set */` |
|    12642 |  5974 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5975 | `		/* Create a new VM frame  */` |
|    12642 |  5976 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    12642 |  5977 | `		if( rc != SXRET_OK ){` |
|        - |  5978 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5979 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5980 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5981 | `				&pVmFunc->sName);` |
|        - |  5982 | `			/* Pop given arguments */` |
|      ! 0 |  5983 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5984 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5985 | `			}` |
|        - |  5986 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5987 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5988 | `			break;` |
|        - |  5989 | `		}` |
|    12642 |  5990 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5991 | `			/* Install the '$this' variable */` |
|        - |  5992 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1580 |  5993 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1580 |  5994 | `			if( pObj ){` |
|        - |  5995 | `				/* Reflect the change */` |
|     1580 |  5996 | `				pObj->x.pOther = pThis;` |
|     1580 |  5997 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      789 |  5998 | `			}` |
|      789 |  5999 | `		}` |
|    12642 |  6000 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  6001 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  6002 | `			/* Install static variables */` |
|      ! 0 |  6003 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  6004 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  6005 | `				pStatic = &aStatic[n];` |
|      ! 0 |  6006 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  6007 | `					/* Initialize the static variables */` |
|      ! 0 |  6008 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  6009 | `					if( pObj ){` |
|        - |  6010 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  6011 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  6012 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  6013 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  6014 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  6015 | `						}` |
|      ! 0 |  6016 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  6017 | `					}else{` |
|      ! 0 |  6018 | `						continue;` |
|        - |  6019 | `					}` |
|      ! 0 |  6020 | `				}` |
|        - |  6021 | `				/* Install in the current frame */` |
|      ! 0 |  6022 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  6023 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  6024 | `			}` |
|      ! 0 |  6025 | `		}` |
|        - |  6026 | `		/* Push arguments in the local frame */` |
|    12642 |  6027 | `		n = 0;` |
|    34906 |  6028 | `		while( pArg < pTos ){` |
|    22266 |  6029 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    22116 |  6030 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  6031 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  6032 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  6033 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6034 | `						goto Abort;` |
|        - |  6035 | `					}` |
|      ! 0 |  6036 | `				}` |
|        - |  6037 | `				/* Make sure the given arguments are of the correct type */` |
|    22116 |  6038 | `				if( aFormalArg[n].nType > 0 ){` |
|     1098 |  6039 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  6040 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  6041 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  6042 | `						ph7_class *pClass;` |
|        - |  6043 | `						/* Try to extract the desired class */` |
|      ! 0 |  6044 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  6045 | `						if( pClass ){` |
|      ! 0 |  6046 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  6047 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6048 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6049 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6050 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6051 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6052 | `								}` |
|      ! 0 |  6053 | `							}else{` |
|        - |  6054 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  6055 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  6056 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  6057 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  6058 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6059 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6060 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6061 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6062 | `								}` |
|        - |  6063 | `							}` |
|      ! 0 |  6064 | `						}` |
|     1098 |  6065 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6066 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6067 | `						/* Cast to the desired type */` |
|      ! 0 |  6068 | `						xCast(pArg);` |
|      ! 0 |  6069 | `					}` |
|      548 |  6070 | `				}` |
|    22116 |  6071 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  6072 | `					/* Pass by reference */` |
|       50 |  6073 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  6074 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  6075 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  6076 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6077 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  6078 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  6079 | `						}` |
|        - |  6080 | `						/* Switch to pass by value */` |
|      ! 0 |  6081 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  6082 | `					}else{` |
|        - |  6083 | `						SyHashEntry *pRefEntry;` |
|        - |  6084 | `						/* Install the referenced variable in the private function frame */` |
|       50 |  6085 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       50 |  6086 | `						if( pRefEntry == 0 ){` |
|       74 |  6087 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       48 |  6088 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       50 |  6089 | `							sArg.nIdx = pArg->nIdx;` |
|       50 |  6090 | `							sArg.pUserData = 0;` |
|       50 |  6091 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       24 |  6092 | `						}` |
|       50 |  6093 | `						pObj = 0;` |
|        - |  6094 | `					}` |
|       26 |  6095 | `				}else{` |
|        - |  6096 | `					/* Pass by value,make a copy of the given argument */` |
|    22068 |  6097 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  6098 | `				}` |
|    11059 |  6099 | `			}else{` |
|        - |  6100 | `				char zName[32];` |
|        - |  6101 | `				SyString sArgName;` |
|        - |  6102 | `				/* Set a dummy name */` |
|      152 |  6103 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      152 |  6104 | `				sArgName.zString = zName;` |
|        - |  6105 | `				/* Annonymous argument */` |
|      152 |  6106 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  6107 | `			}` |
|    22266 |  6108 | `			if( pObj ){` |
|    22218 |  6109 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  6110 | `				/* Insert argument index  */` |
|    22218 |  6111 | `				sArg.nIdx = pObj->nIdx;` |
|    22218 |  6112 | `				sArg.pUserData = 0;` |
|    22218 |  6113 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    11108 |  6114 | `			}` |
|    22266 |  6115 | `			PH7_MemObjRelease(pArg);` |
|    22266 |  6116 | `			pArg++;` |
|    22266 |  6117 | `			++n;` |
|        2 |  6118 | `		}` |
|        - |  6119 | `		/* Set up closure environment */` |
|    12642 |  6120 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  6121 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  6122 | `			ph7_value *pValue;` |
|        - |  6123 | `			sxu32 iEnv;` |
|       11 |  6124 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       31 |  6125 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       21 |  6126 | `				pEnv = &aEnv[iEnv];` |
|       21 |  6127 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  6128 | `					/* Do not install null value */` |
|       11 |  6129 | `					continue;` |
|        - |  6130 | `				}` |
|       11 |  6131 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       11 |  6132 | `				if( pValue == 0 ){` |
|      ! 0 |  6133 | `					continue;` |
|        - |  6134 | `				}` |
|        - |  6135 | `				/* Invalidate any prior representation */` |
|       11 |  6136 | `				PH7_MemObjRelease(pValue);` |
|        - |  6137 | `				/* Duplicate bound variable value */` |
|       11 |  6138 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        6 |  6139 | `			}` |
|        5 |  6140 | `		}` |
|        - |  6141 | `		/* Process default values */` |
|    14516 |  6142 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1876 |  6143 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1870 |  6144 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1870 |  6145 | `				if( pObj ){` |
|        - |  6146 | `					/* Evaluate the default value and extract it's result */` |
|     1870 |  6147 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1870 |  6148 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6149 | `						goto Abort;` |
|        - |  6150 | `					}` |
|        - |  6151 | `					/* Insert argument index */` |
|     1870 |  6152 | `					sArg.nIdx = pObj->nIdx;` |
|     1870 |  6153 | `					sArg.pUserData = 0;` |
|     1870 |  6154 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  6155 | `					/* Make sure the default argument is of the correct type */` |
|     1870 |  6156 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6157 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6158 | `						/* Cast to the desired type */` |
|      ! 0 |  6159 | `						xCast(pObj);` |
|      ! 0 |  6160 | `					}` |
|      934 |  6161 | `				}` |
|      934 |  6162 | `			}` |
|     1876 |  6163 | `			++n;` |
|        2 |  6164 | `		}` |
|        - |  6165 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  6166 | `		 * does not return anything.` |
|        - |  6167 | `		 */` |
|    12642 |  6168 | `		PH7_MemObjRelease(pTos);` |
|    12642 |  6169 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  6170 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    12642 |  6171 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    12642 |  6172 | `		if( pFrameStack == 0 ){` |
|        - |  6173 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6174 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6175 | `				&pVmFunc->sName);` |
|      ! 0 |  6176 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6177 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6178 | `			}` |
|      ! 0 |  6179 | `			break;` |
|        - |  6180 | `		}` |
|    12642 |  6181 | `		if( pSelf ){` |
|        - |  6182 | `			/* Push class name */` |
|     1602 |  6183 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      800 |  6184 | `		}` |
|        - |  6185 | `		/* Increment nesting level */` |
|    12642 |  6186 | `		pVm->nRecursionDepth++;` |
|        - |  6187 | `		/* Execute function body */` |
|    12642 |  6188 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  6189 | `		/* Decrement nesting level */` |
|    12642 |  6190 | `		pVm->nRecursionDepth--;` |
|    12642 |  6191 | `		if( pSelf ){` |
|        - |  6192 | `			/* Pop class name */` |
|     1602 |  6193 | `			(void)SySetPop(&pVm->aSelf);` |
|      800 |  6194 | `		}` |
|        - |  6195 | `		/* Cleanup the mess left behind */` |
|    12642 |  6196 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  6197 | `			/* Return by reference,reflect that */` |
|        9 |  6198 | `			if( n != SXU32_HIGH ){` |
|        9 |  6199 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  6200 | `				sxu32 i;` |
|        - |  6201 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  6202 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  6203 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  6204 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  6205 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6206 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6207 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  6208 | `								&pVmFunc->sName);` |
|      ! 0 |  6209 | `						}` |
|      ! 0 |  6210 | `						n = SXU32_HIGH;` |
|      ! 0 |  6211 | `						break;` |
|        - |  6212 | `					}` |
|        3 |  6213 | `				}` |
|        5 |  6214 | `			}else{` |
|      ! 0 |  6215 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6216 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6217 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  6218 | `						&pVmFunc->sName);` |
|      ! 0 |  6219 | `				}` |
|        - |  6220 | `			}` |
|        9 |  6221 | `			pTos->nIdx = n;` |
|        4 |  6222 | `		}` |
|        - |  6223 | `		/* Cleanup the mess left behind */` |
|    12642 |  6224 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  6225 | `			/* An exception was throw in this frame */` |
|        7 |  6226 | `			pFrame = pFrame->pParent;` |
|        7 |  6227 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  6228 | `				/* Pop the resutlt */` |
|        5 |  6229 | `				VmPopOperand(&pTos,1);` |
|        - |  6230 | `				/* Jump to this destination */` |
|        5 |  6231 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  6232 | `				rc = PH7_OK;` |
|        3 |  6233 | `			}else{` |
|        3 |  6234 | `				if( pFrame->pParent ){` |
|        3 |  6235 | `					rc = PH7_EXCEPTION;` |
|        2 |  6236 | `				}else{` |
|        - |  6237 | `					/* Continue normal execution */` |
|      ! 0 |  6238 | `					rc = PH7_OK;` |
|        - |  6239 | `				}` |
|        - |  6240 | `			}` |
|        3 |  6241 | `		}` |
|        - |  6242 | `		/* Free the operand stack */` |
|    12642 |  6243 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  6244 | `		/* Leave the frame */` |
|    12642 |  6245 | `		VmLeaveFrame(&(*pVm));` |
|    12642 |  6246 | `		if( rc == PH7_ABORT ){` |
|        - |  6247 | `			/* Abort processing immeditaley */` |
|        7 |  6248 | `			goto Abort;` |
|    12636 |  6249 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6250 | `			goto Exception;` |
|        - |  6251 | `		}` |
|     6318 |  6252 | `	}else{` |
|        - |  6253 | `		ph7_user_func *pFunc;` |
|        - |  6254 | `		ph7_context sCtx;` |
|        - |  6255 | `		ph7_value sRet;` |
|        - |  6256 | `		/* Look for an installed foreign function.` |
|        - |  6257 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6258 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6259 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6260 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6261 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6262 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   561318 |  6263 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   561318 |  6264 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6265 | `			/* Compiler-qualified: try short name as global fallback */` |
|       15 |  6266 | `			const char *zShort = sName.zString;` |
|        - |  6267 | `			sxu32 i;` |
|      217 |  6268 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      203 |  6269 | `				if( sName.zString[i] == '\\' ){` |
|       19 |  6270 | `					zShort = &sName.zString[i + 1];` |
|        9 |  6271 | `				}` |
|      102 |  6272 | `			}` |
|       15 |  6273 | `			if( zShort != sName.zString ){` |
|       15 |  6274 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       15 |  6275 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        7 |  6276 | `			}` |
|        7 |  6277 | `		}` |
|   561318 |  6278 | `		if( pEntry == 0 ){` |
|        - |  6279 | `			/* Call to undefined function */` |
|        5 |  6280 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6281 | `			/* Pop given arguments */` |
|        5 |  6282 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6283 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6284 | `			}` |
|        - |  6285 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6286 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6287 | `			break;` |
|        - |  6288 | `		}` |
|   561314 |  6289 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6290 | `		/* Start collecting function arguments */` |
|   561314 |  6291 | `		SySetReset(&aArg);` |
|  1504606 |  6292 | `		while( pArg < pTos ){` |
|   943294 |  6293 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   943294 |  6294 | `			pArg++;` |
|        2 |  6295 | `		}` |
|        - |  6296 | `		/* Assume a null return value */` |
|   561314 |  6297 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6298 | `		/* Init the call context */` |
|   561314 |  6299 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6300 | `		/* Call the foreign function */` |
|   561314 |  6301 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6302 | `		/* Release the call context */` |
|   561314 |  6303 | `		VmReleaseCallContext(&sCtx);` |
|   561314 |  6304 | `		if( rc == PH7_ABORT ){` |
|      463 |  6305 | `			goto Abort;` |
|   560852 |  6306 | `		}else if( rc == PH7_EXCEPTION ){` |
|        7 |  6307 | `			VmFrame *pFrm = pVm->pFrame;` |
|        7 |  6308 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|        7 |  6309 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6310 | `				/* Exception was NOT caught, propagate */` |
|      ! 0 |  6311 | `				goto Exception;` |
|        - |  6312 | `			}` |
|        - |  6313 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6314 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6315 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6316 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6317 | `			}` |
|        - |  6318 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6319 | `			VmPopOperand(&pTos,1);` |
|        - |  6320 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6321 | `			pFrm = pVm->pFrame;` |
|        7 |  6322 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6323 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6324 | `			}` |
|        7 |  6325 | `			break;` |
|        - |  6326 | `		}` |
|   560846 |  6327 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6328 | `			/* Pop function name and arguments */` |
|   543254 |  6329 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   271648 |  6330 | `		}` |
|        - |  6331 | `		/* Save foreign function return value */` |
|   560846 |  6332 | `		PH7_MemObjStore(&sRet,pTos);` |
|   560846 |  6333 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6334 | `	}` |
|   573478 |  6335 | `	break;` |
|        - |  6336 | `				  }` |
|        - |  6337 | `/*` |
|        - |  6338 | ` * OP_CONSUME: P1 * *` |
|        - |  6339 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6340 | ` */` |
|    11138 |  6341 | `case PH7_OP_CONSUME: {` |
|    22278 |  6342 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    22278 |  6343 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6344 |  |
|    22278 |  6345 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    22278 |  6346 | `	pCur = pOut;` |
|        - |  6347 | `	/* Start the consume process  */` |
|    44554 |  6348 | `	while( pOut <= pTos ){` |
|        - |  6349 | `		/* Force a string cast */` |
|    22278 |  6350 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      304 |  6351 | `			PH7_MemObjToString(pOut);` |
|      151 |  6352 | `		}` |
|    22278 |  6353 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6354 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6355 | `			/* Invoke the output consumer callback */` |
|    12270 |  6356 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    12270 |  6357 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    12270 |  6358 | `			SyBlobRelease(&pOut->sBlob);` |
|    12270 |  6359 | `			if( rc == SXERR_ABORT ){` |
|        - |  6360 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6361 | `				goto Abort;` |
|        - |  6362 | `			}` |
|     6134 |  6363 | `		}` |
|    22278 |  6364 | `		pOut++;` |
|        2 |  6365 | `	}` |
|    22278 |  6366 | `	pTos = &pCur[-1];` |
|    22276 |  6367 | `	break;` |
|        - |  6368 | `					 }` |
|        - |  6369 |  |
|        - |  6370 | `		} /* Switch() */` |
|  9796222 |  6371 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6372 | `	} /* For(;;) */` |
|    15517 |  6373 | `Done:` |
|    31036 |  6374 | `	SySetRelease(&aArg);` |
|    31036 |  6375 | `	return SXRET_OK;` |
|      238 |  6376 | `Abort:` |
|      477 |  6377 | `	SySetRelease(&aArg);` |
|     1661 |  6378 | `	while( pTos >= pStack ){` |
|     1185 |  6379 | `		PH7_MemObjRelease(pTos);` |
|     1185 |  6380 | `		pTos--;` |
|        1 |  6381 | `	}` |
|      477 |  6382 | `	return PH7_ABORT;` |
|        1 |  6383 | `Exception:` |
|        3 |  6384 | `	SySetRelease(&aArg);` |
|        5 |  6385 | `	while( pTos >= pStack ){` |
|        3 |  6386 | `		PH7_MemObjRelease(pTos);` |
|        3 |  6387 | `		pTos--;` |
|        1 |  6388 | `	}` |
|        3 |  6389 | `	return PH7_EXCEPTION;` |
|    15758 |  6390 |  |
|        - |  6391 | `/*` |
|        - |  6392 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6393 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6394 | ` * See block-comment on that function for additional information.` |
|        - |  6395 | ` */` |
|    14770 |  6396 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6397 |  |
|        - |  6398 | `	ph7_value *pStack;` |
|        - |  6399 | `	sxi32 rc;` |
|        - |  6400 | `	/* Allocate a new operand stack */` |
|    14772 |  6401 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    14772 |  6402 | `	if( pStack == 0 ){` |
|      ! 0 |  6403 | `		return SXERR_MEM;` |
|        - |  6404 | `	}` |
|        - |  6405 | `	/* Execute the program */` |
|    14772 |  6406 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  6407 | `	/* Free the operand stack */` |
|    14772 |  6408 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6409 | `	/* Execution result */` |
|    14772 |  6410 | `	return rc;` |
|     7387 |  6411 |  |
|        - |  6412 | `/*` |
|        - |  6413 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6414 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6415 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6416 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6417 | ` * execution ends.` |
|        - |  6418 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6419 | ` * additional information.` |
|        - |  6420 | ` */` |
|     2358 |  6421 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6422 |  |
|        - |  6423 | `	VmShutdownCB *pEntry;` |
|        - |  6424 | `	ph7_value *apArg[10];` |
|        - |  6425 | `	sxu32 n,nEntry;` |
|        - |  6426 | `	int i;` |
|        - |  6427 | `	/* Point to the stack of registered callbacks */` |
|     2360 |  6428 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    25940 |  6429 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    23582 |  6430 | `		apArg[i] = 0;` |
|    11792 |  6431 | `	}` |
|     2362 |  6432 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6433 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6434 | `		if( pEntry ){` |
|        - |  6435 | `			/* Prepare callback arguments if any */` |
|        3 |  6436 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6437 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6438 | `					break;` |
|        - |  6439 | `				}` |
|      ! 0 |  6440 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6441 | `			}` |
|        - |  6442 | `			/* Invoke the callback */` |
|        3 |  6443 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6444 | `			/*` |
|        - |  6445 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6446 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6447 | `			 */` |
|        3 |  6448 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6449 | `			if( pEntry ){` |
|        3 |  6450 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6451 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6452 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6453 | `				}` |
|        1 |  6454 | `			}` |
|        1 |  6455 | `		}` |
|        2 |  6456 | `	}` |
|     2360 |  6457 | `	SySetReset(&pVm->aShutdown);` |
|     2360 |  6458 |  |
|        - |  6459 | `/*` |
|        - |  6460 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6461 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6462 | ` * See block-comment on that function for additional information.` |
|        - |  6463 | ` */` |
|     2366 |  6464 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6465 |  |
|        - |  6466 | `	/* Make sure we are ready to execute this program */` |
|     2368 |  6467 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6468 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6469 | `	}` |
|        - |  6470 | `	/* Set the execution magic number  */` |
|     2368 |  6471 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6472 | `	/* Execute the program */` |
|     2368 |  6473 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6474 | `	/* Invoke any shutdown callbacks */` |
|     2364 |  6475 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6476 | `	/*` |
|        - |  6477 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6478 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6479 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6480 | `	 */` |
|     2364 |  6481 | `	return SXRET_OK;` |
|     1185 |  6482 |  |
|        - |  6483 | `/*` |
|        - |  6484 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6485 | ` * the desired message.` |
|        - |  6486 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6487 | ` * in 'api.c' for additional information.` |
|        - |  6488 | ` */` |
|      350 |  6489 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6490 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6491 | `	SyString *pString /* Message to output */` |
|        - |  6492 | `	)` |
|        2 |  6493 |  |
|      352 |  6494 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      352 |  6495 | `	sxi32 rc = SXRET_OK;` |
|        - |  6496 | `	/* Call the output consumer */` |
|      352 |  6497 | `	if( pString->nByte > 0 ){` |
|      352 |  6498 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      352 |  6499 | `		VmTrackOutput(pVm, pString->nByte);` |
|      175 |  6500 | `	}` |
|      352 |  6501 | `	return rc;` |
|        2 |  6502 |  |
|        - |  6503 | `/*` |
|        - |  6504 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6505 | ` * callback to consume the formatted message.` |
|        - |  6506 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6507 | ` * in 'api.c' for additional information.` |
|        - |  6508 | ` */` |
|        2 |  6509 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6510 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6511 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6512 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6513 | `	)` |
|        1 |  6514 |  |
|        3 |  6515 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6516 | `	sxi32 rc = SXRET_OK;` |
|        - |  6517 | `	SyBlob sWorker;` |
|        - |  6518 | `	/* Format the message and call the output consumer */` |
|        3 |  6519 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6520 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6521 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6522 | `		/* Consume the formatted message */` |
|        3 |  6523 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6524 | `	}` |
|        3 |  6525 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  6526 | `	/* Release the working buffer */` |
|        3 |  6527 | `	SyBlobRelease(&sWorker);` |
|        3 |  6528 | `	return rc;` |
|        1 |  6529 |  |
|        - |  6530 | `/*` |
|        - |  6531 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6532 | ` * This function never fail and always return a pointer` |
|        - |  6533 | ` * to a null terminated string.` |
|        - |  6534 | ` */` |
|       12 |  6535 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6536 |  |
|       13 |  6537 | `	const char *zOp = "Unknown     ";` |
|       13 |  6538 | `	switch(nOp){` |
|        3 |  6539 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6540 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6541 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6542 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6543 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6544 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6545 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6546 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6547 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6548 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6549 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6550 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6551 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6552 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6553 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6554 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6555 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6556 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6557 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6558 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6559 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6560 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6561 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6562 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6563 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6564 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6565 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6566 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6567 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6568 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6569 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6570 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6571 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6572 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6573 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6574 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6575 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6576 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6577 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6578 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6579 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6580 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6581 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6582 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6583 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6584 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6585 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6586 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6587 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6588 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  6589 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  6590 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6591 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6592 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6593 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6594 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6595 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6596 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6597 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6598 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6599 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6600 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6601 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6602 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6603 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6604 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6605 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6606 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6607 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6608 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6609 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6610 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6611 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6612 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6613 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6614 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6615 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6616 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6617 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6618 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6619 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6620 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6621 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6622 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6623 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6624 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6625 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6626 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6627 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6628 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6629 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6630 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6631 | `	default:` |
|      ! 0 |  6632 | `		break;` |
|        - |  6633 | `	}` |
|       13 |  6634 | `	return zOp;` |
|        1 |  6635 |  |
|        - |  6636 | `/*` |
|        - |  6637 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6638 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6639 | ` * is responsible of consuming the generated dump.` |
|        - |  6640 | ` */` |
|        2 |  6641 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6642 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6643 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6644 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6645 | `	)` |
|        1 |  6646 |  |
|        - |  6647 | `	sxi32 rc;` |
|        3 |  6648 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6649 | `	return rc;` |
|        1 |  6650 |  |
|        - |  6651 | `/*` |
|        - |  6652 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6653 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6654 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6655 | ` * in 'compile.c' for additional information.` |
|        - |  6656 | ` */` |
|        8 |  6657 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6658 |  |
|        9 |  6659 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6660 | `	/* Evaluate and expand constant value */` |
|        9 |  6661 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6662 |  |
|        - |  6663 | `/*` |
|        - |  6664 | ` * Section:` |
|        - |  6665 | ` *  Function handling functions.` |
|        - |  6666 | ` * Status:` |
|        - |  6667 | ` *    Stable.` |
|        - |  6668 | ` */` |
|        - |  6669 | `/*` |
|        - |  6670 | ` * int func_num_args(void)` |
|        - |  6671 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6672 | ` * Parameters` |
|        - |  6673 | ` *   None.` |
|        - |  6674 | ` * Return` |
|        - |  6675 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6676 | ` *  or -1 if called from the globe scope.` |
|        - |  6677 | ` */` |
|      916 |  6678 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6679 |  |
|        - |  6680 | `	VmFrame *pFrame;` |
|        - |  6681 | `	ph7_vm *pVm;` |
|        - |  6682 | `	/* Point to the target VM */` |
|      918 |  6683 | `	pVm = pCtx->pVm;` |
|        - |  6684 | `	/* Current frame */` |
|      918 |  6685 | `	pFrame = pVm->pFrame;` |
|      918 |  6686 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      918 |  6687 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6688 | `		SXUNUSED(nArg);` |
|      ! 0 |  6689 | `		SXUNUSED(apArg);` |
|        - |  6690 | `		/* Global frame,return -1 */` |
|      ! 0 |  6691 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6692 | `		return SXRET_OK;` |
|        - |  6693 | `	}` |
|        - |  6694 | `	/* Total number of arguments passed to the enclosing function */` |
|      918 |  6695 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      918 |  6696 | `	ph7_result_int(pCtx,nArg);` |
|      918 |  6697 | `	return SXRET_OK;` |
|      460 |  6698 |  |
|        - |  6699 | `/*` |
|        - |  6700 | ` * value func_get_arg(int $arg_num)` |
|        - |  6701 | ` *   Return an item from the argument list.` |
|        - |  6702 | ` * Parameters` |
|        - |  6703 | ` *  Argument number(index start from zero).` |
|        - |  6704 | ` * Return` |
|        - |  6705 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6706 | ` */` |
|       22 |  6707 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6708 |  |
|       24 |  6709 | `	ph7_value *pObj = 0;` |
|       24 |  6710 | `	VmSlot *pSlot = 0;` |
|        - |  6711 | `	VmFrame *pFrame;` |
|        - |  6712 | `	ph7_vm *pVm;` |
|        - |  6713 | `	/* Point to the target VM */` |
|       24 |  6714 | `	pVm = pCtx->pVm;` |
|        - |  6715 | `	/* Current frame */` |
|       24 |  6716 | `	pFrame = pVm->pFrame;` |
|       24 |  6717 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  6718 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6719 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6720 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6721 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6722 | `		return SXRET_OK;` |
|        - |  6723 | `	}` |
|        - |  6724 | `	/* Extract the desired index */` |
|       21 |  6725 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  6726 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6727 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6728 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6729 | `		return SXRET_OK;` |
|        - |  6730 | `	}` |
|        - |  6731 | `	/* Extract the desired argument */` |
|       21 |  6732 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  6733 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6734 | `			/* Return the desired argument */` |
|       21 |  6735 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  6736 | `		}else{` |
|        - |  6737 | `			/* No such argument,return false */` |
|      ! 0 |  6738 | `			ph7_result_bool(pCtx,0);` |
|        - |  6739 | `		}` |
|       11 |  6740 | `	}else{` |
|        - |  6741 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6742 | `		ph7_result_bool(pCtx,0);` |
|        - |  6743 | `	}` |
|       21 |  6744 | `	return SXRET_OK;` |
|       13 |  6745 |  |
|        - |  6746 | `/*` |
|        - |  6747 | ` * array func_get_args_byref(void)` |
|        - |  6748 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6749 | ` * Parameters` |
|        - |  6750 | ` *  None.` |
|        - |  6751 | ` * Return` |
|        - |  6752 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6753 | ` *  member of the current user-defined function's argument list.` |
|        - |  6754 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6755 | ` * NOTE:` |
|        - |  6756 | ` *  Arguments are returned to the array by reference.` |
|        - |  6757 | ` */` |
|        2 |  6758 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6759 |  |
|        - |  6760 | `	ph7_value *pArray;` |
|        - |  6761 | `	VmFrame *pFrame;` |
|        - |  6762 | `	VmSlot *aSlot;` |
|        - |  6763 | `	sxu32 n;` |
|        - |  6764 | `	/* Point to the current frame */` |
|        3 |  6765 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6766 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  6767 | `	if( pFrame->pParent == 0 ){` |
|        - |  6768 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6769 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6770 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6771 | `		return SXRET_OK;` |
|        - |  6772 | `	}` |
|        - |  6773 | `	/* Create a new array */` |
|        3 |  6774 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6775 | `	if( pArray == 0 ){` |
|      ! 0 |  6776 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6777 | `		SXUNUSED(apArg);` |
|      ! 0 |  6778 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6779 | `		return SXRET_OK;` |
|        - |  6780 | `	}` |
|        - |  6781 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6782 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6783 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6784 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6785 | `	}` |
|        - |  6786 | `	/* Return the freshly created array */` |
|        3 |  6787 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6788 | `	return SXRET_OK;` |
|        2 |  6789 |  |
|        - |  6790 | `/*` |
|        - |  6791 | ` * array func_get_args(void)` |
|        - |  6792 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6793 | ` * Parameters` |
|        - |  6794 | ` *  None.` |
|        - |  6795 | ` * Return` |
|        - |  6796 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6797 | ` *  member of the current user-defined function's argument list.` |
|        - |  6798 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6799 | ` */` |
|       62 |  6800 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6801 |  |
|       64 |  6802 | `	ph7_value *pObj = 0;` |
|        - |  6803 | `	ph7_value *pArray;` |
|        - |  6804 | `	VmFrame *pFrame;` |
|        - |  6805 | `	VmSlot *aSlot;` |
|        - |  6806 | `	sxu32 n;` |
|        - |  6807 | `	/* Point to the current frame */` |
|       64 |  6808 | `	pFrame = pCtx->pVm->pFrame;` |
|       64 |  6809 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       64 |  6810 | `	if( pFrame->pParent == 0 ){` |
|        - |  6811 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6812 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6813 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6814 | `		return SXRET_OK;` |
|        - |  6815 | `	}` |
|        - |  6816 | `	/* Create a new array */` |
|       64 |  6817 | `	pArray = ph7_context_new_array(pCtx);` |
|       64 |  6818 | `	if( pArray == 0 ){` |
|      ! 0 |  6819 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6820 | `		SXUNUSED(apArg);` |
|      ! 0 |  6821 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6822 | `		return SXRET_OK;` |
|        - |  6823 | `	}` |
|        - |  6824 | `	/* Start filling the array with the given arguments */` |
|       64 |  6825 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      192 |  6826 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      130 |  6827 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      130 |  6828 | `		if( pObj ){` |
|      130 |  6829 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       64 |  6830 | `		}` |
|       66 |  6831 | `	}` |
|        - |  6832 | `	/* Return the freshly created array */` |
|       64 |  6833 | `	ph7_result_value(pCtx,pArray);` |
|       64 |  6834 | `	return SXRET_OK;` |
|       33 |  6835 |  |
|        - |  6836 | `/*` |
|        - |  6837 | ` * bool function_exists(string $name)` |
|        - |  6838 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6839 | ` * Parameters` |
|        - |  6840 | ` *  The name of the desired function.` |
|        - |  6841 | ` * Return` |
|        - |  6842 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6843 | ` */` |
|     1646 |  6844 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6845 |  |
|        - |  6846 | `	const char *zName;` |
|        - |  6847 | `	ph7_vm *pVm;` |
|        - |  6848 | `	int nLen;` |
|        - |  6849 | `	int res;` |
|     1648 |  6850 | `	if( nArg < 1 ){` |
|        - |  6851 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6852 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6853 | `		return SXRET_OK;` |
|        - |  6854 | `	}` |
|        - |  6855 | `	/* Point to the target VM */` |
|     1648 |  6856 | `	pVm = pCtx->pVm;` |
|        - |  6857 | `	/* Extract the function name */` |
|     1648 |  6858 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6859 | `	/* Assume the function is not defined */` |
|     1648 |  6860 | `	res = 0;` |
|        - |  6861 | `	/* Perform the lookup */` |
|     2469 |  6862 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1642 |  6863 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6864 | `			/* Function is defined */` |
|      206 |  6865 | `			res = 1;` |
|      102 |  6866 | `	}` |
|     1648 |  6867 | `	ph7_result_bool(pCtx,res);` |
|     1648 |  6868 | `	return SXRET_OK;` |
|      825 |  6869 |  |
|        - |  6870 | `/*` |
|        - |  6871 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6872 | ` * [i.e: Whether it is callable or not].` |
|        - |  6873 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6874 | ` */` |
|    16234 |  6875 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6876 |  |
|    16236 |  6877 | `	int res = 0;` |
|    16236 |  6878 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6879 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6880 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6881 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6882 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6883 | `		if( pMethod && CallInvoke ){` |
|        - |  6884 | `			ph7_value sResult;` |
|        - |  6885 | `			sxi32 rc;` |
|        - |  6886 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6887 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6888 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6889 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6890 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6891 | `			}` |
|      ! 0 |  6892 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6893 | `		}` |
|    16236 |  6894 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  6895 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  6896 | `		if( pMap->nEntry == 2 ){` |
|        - |  6897 | `			ph7_class *pClass;` |
|        - |  6898 | `			ph7_value *pV;` |
|        - |  6899 | `			/* Extract the target class */` |
|       12 |  6900 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  6901 | `			if( pV ){` |
|       12 |  6902 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  6903 | `				if( pClass ){` |
|        - |  6904 | `					ph7_class_method *pMethod;` |
|        - |  6905 | `					/* Extract the target method */` |
|       10 |  6906 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  6907 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6908 | `						/* Perform the lookup */` |
|       10 |  6909 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  6910 | `						if( pMethod ){` |
|        - |  6911 | `							/* Method is callable */` |
|        5 |  6912 | `							res = 1;` |
|        2 |  6913 | `						}` |
|        4 |  6914 | `					}` |
|        4 |  6915 | `				}` |
|        5 |  6916 | `			}` |
|        7 |  6917 | `		}` |
|    16223 |  6918 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6919 | `		const char *zName;` |
|        - |  6920 | `		int nLen;` |
|        - |  6921 | `		/* Extract the name */` |
|     4750 |  6922 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6923 | `		/* Perform the lookup */` |
|     4765 |  6924 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  6925 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6926 | `				/* Function is callable */` |
|     4732 |  6927 | `				res = 1;` |
|     2365 |  6928 | `		}` |
|     2374 |  6929 | `	}` |
|    16236 |  6930 | `	return res;` |
|        2 |  6931 |  |
|        - |  6932 | `/*` |
|        - |  6933 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6934 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6935 | ` * Parameters` |
|        - |  6936 | ` * $name` |
|        - |  6937 | ` *    The callback function to check` |
|        - |  6938 | ` * $syntax_only` |
|        - |  6939 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6940 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6941 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6942 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6943 | ` *    a string.` |
|        - |  6944 | ` * Return` |
|        - |  6945 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6946 | ` */` |
|       14 |  6947 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6948 |  |
|        - |  6949 | `	ph7_vm *pVm;` |
|        - |  6950 | `	int res;` |
|       15 |  6951 | `	if( nArg < 1 ){` |
|        - |  6952 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6953 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6954 | `		return SXRET_OK;` |
|        - |  6955 | `	}` |
|        - |  6956 | `	/* Point to the target VM */` |
|       15 |  6957 | `	pVm = pCtx->pVm;` |
|        - |  6958 | `	/* Perform the requested operation */` |
|       15 |  6959 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6960 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6961 | `	return SXRET_OK;` |
|        8 |  6962 |  |
|        - |  6963 | `/*` |
|        - |  6964 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6965 | ` * defined below.` |
|        - |  6966 | ` */` |
|     1096 |  6967 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6968 |  |
|     1097 |  6969 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6970 | `	ph7_value sName;` |
|        - |  6971 | `	sxi32 rc;` |
|        - |  6972 | `	/* Prepare the function name for insertion */` |
|     1097 |  6973 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1097 |  6974 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6975 | `	/* Perform the insertion */` |
|     1097 |  6976 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1097 |  6977 | `	PH7_MemObjRelease(&sName);` |
|     1097 |  6978 | `	return rc;` |
|        1 |  6979 |  |
|        - |  6980 | `/*` |
|        - |  6981 | ` * array get_defined_functions(void)` |
|        - |  6982 | ` *  Returns an array of all defined functions.` |
|        - |  6983 | ` * Parameter` |
|        - |  6984 | ` *  None.` |
|        - |  6985 | ` * Return` |
|        - |  6986 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6987 | ` *  both built-in (internal) and user-defined.` |
|        - |  6988 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6989 | ` *  defined ones using $arr["user"].` |
|        - |  6990 | ` * Note:` |
|        - |  6991 | ` *  NULL is returned on failure.` |
|        - |  6992 | ` */` |
|        2 |  6993 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6994 |  |
|        - |  6995 | `	ph7_value *pArray,*pEntry;` |
|        - |  6996 | `	/* NOTE:` |
|        - |  6997 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6998 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6999 | `	 */` |
|        3 |  7000 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7001 | ` 	if( pArray == 0 ){` |
|      ! 0 |  7002 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7003 | `		SXUNUSED(apArg);` |
|        - |  7004 | `		/* Return NULL */` |
|      ! 0 |  7005 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7006 | `		return SXRET_OK;` |
|        - |  7007 | `	}` |
|        3 |  7008 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  7009 | `	if( pEntry == 0 ){` |
|        - |  7010 | `		/* Return NULL */` |
|      ! 0 |  7011 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7012 | `		return SXRET_OK;` |
|        - |  7013 | `	}` |
|        - |  7014 | `	/* Fill with the appropriate information */` |
|        3 |  7015 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  7016 | `	/* Create the 'internal' index */` |
|        3 |  7017 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  7018 | `	/* Create the user-func array */` |
|        3 |  7019 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  7020 | `	if( pEntry == 0 ){` |
|        - |  7021 | `		/* Return NULL */` |
|      ! 0 |  7022 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7023 | `		return SXRET_OK;` |
|        - |  7024 | `	}` |
|        - |  7025 | `	/* Fill with the appropriate information */` |
|        3 |  7026 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  7027 | `	/* Create the 'user' index */` |
|        3 |  7028 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  7029 | `	/* Return the multi-dimensional array */` |
|        3 |  7030 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7031 | `	return SXRET_OK;` |
|        2 |  7032 |  |
|        - |  7033 | `/*` |
|        - |  7034 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  7035 | ` *  Register a function for execution on shutdown.` |
|        - |  7036 | ` * Note` |
|        - |  7037 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  7038 | ` *  be called in the same order as they were registered.` |
|        - |  7039 | ` * Parameters` |
|        - |  7040 | ` *  $callback` |
|        - |  7041 | ` *   The shutdown callback to register.` |
|        - |  7042 | ` * $param` |
|        - |  7043 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  7044 | ` * Return` |
|        - |  7045 | ` *  Nothing.` |
|        - |  7046 | ` */` |
|        2 |  7047 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7048 |  |
|        - |  7049 | `	VmShutdownCB sEntry;` |
|        - |  7050 | `	int i,j;` |
|        3 |  7051 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  7052 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  7053 | `		return PH7_OK;` |
|        - |  7054 | `	}` |
|        - |  7055 | `	/* Zero the Entry */` |
|        3 |  7056 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  7057 | `	/* Initialize fields */` |
|        3 |  7058 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  7059 | `	/* Save the callback name for later invocation name */` |
|        3 |  7060 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  7061 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  7062 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  7063 | `	}` |
|        - |  7064 | `	/* Copy arguments */` |
|        3 |  7065 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  7066 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  7067 | `			/* Limit reached */` |
|      ! 0 |  7068 | `			break;` |
|        - |  7069 | `		}` |
|      ! 0 |  7070 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  7071 | `	}` |
|        3 |  7072 | `	sEntry.nArg = j;` |
|        - |  7073 | `	/* Install the callback */` |
|        3 |  7074 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  7075 | `	return PH7_OK;` |
|        2 |  7076 |  |
|        - |  7077 | `/*` |
|        - |  7078 | ` * Section:` |
|        - |  7079 | ` *  Class handling functions.` |
|        - |  7080 | ` * Status:` |
|        - |  7081 | ` *    Stable.` |
|        - |  7082 | ` */` |
|        - |  7083 | `/*` |
|        - |  7084 | ` * Extract the top active class. NULL is returned` |
|        - |  7085 | ` * if the class stack is empty.` |
|        - |  7086 | ` */` |
|      550 |  7087 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  7088 |  |
|      552 |  7089 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  7090 | `	ph7_class **apClass;` |
|      552 |  7091 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  7092 | `		/* Empty stack,return NULL */` |
|       15 |  7093 | `		return 0;` |
|        - |  7094 | `	}` |
|        - |  7095 | `	/* Peek the last entry */` |
|      538 |  7096 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      538 |  7097 | `	return apClass[pSet->nUsed - 1];` |
|      277 |  7098 |  |
|        - |  7099 | `/*` |
|        - |  7100 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  7101 | ` *   Get the class that declared the currently executing method.` |
|        - |  7102 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  7103 | ` *` |
|        - |  7104 | ` * Parameters` |
|        - |  7105 | ` *   pVm: Target VM` |
|        - |  7106 | ` *` |
|        - |  7107 | ` * Return` |
|        - |  7108 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  7109 | ` *   - Not executing within a class method` |
|        - |  7110 | ` *` |
|        - |  7111 | ` * Note` |
|        - |  7112 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  7113 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  7114 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  7115 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  7116 | ` *   declaring class.` |
|        - |  7117 | ` */` |
|       52 |  7118 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  7119 |  |
|       54 |  7120 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  7121 | `	ph7_vm_func *pVmFunc;` |
|        - |  7122 |  |
|        - |  7123 | `	/* Skip exception frames to find the actual method frame */` |
|       54 |  7124 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  7125 |  |
|        - |  7126 | `	/* Check if we're in a method context */` |
|       54 |  7127 | `	if( pFrame->pParent ){` |
|       50 |  7128 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       50 |  7129 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  7130 | `			/* Return the declaring class */` |
|       50 |  7131 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  7132 | `		}` |
|      ! 0 |  7133 | `	}` |
|        - |  7134 |  |
|        5 |  7135 | `	return 0;` |
|       28 |  7136 |  |
|        - |  7137 |  |
|        - |  7138 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  7139 | `/*` |
|        - |  7140 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  7141 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  7142 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  7143 | ` * return value indicates failure.` |
|        - |  7144 | ` */` |
|     1298 |  7145 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  7146 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7147 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  7148 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  7149 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  7150 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  7151 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  7152 | `	)` |
|        2 |  7153 |  |
|        - |  7154 | `	ph7_value *aStack;` |
|        - |  7155 | `	VmInstr aInstr[2];` |
|        - |  7156 | `	int iCursor;` |
|        - |  7157 | `	int i;` |
|        - |  7158 | `	/* Create a new operand stack */` |
|     1300 |  7159 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1300 |  7160 | `	if( aStack == 0 ){` |
|      ! 0 |  7161 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7162 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  7163 | `		return SXERR_MEM;` |
|        - |  7164 | `	}` |
|        - |  7165 | `	/* Fill the operand stack with the given arguments */` |
|     1872 |  7166 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      574 |  7167 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7168 | `		/*` |
|        - |  7169 | `		 * Symisc eXtension:` |
|        - |  7170 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7171 | `		 */` |
|      574 |  7172 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      288 |  7173 | `	}` |
|     1300 |  7174 | `	iCursor = nArg + 1;` |
|     1300 |  7175 | `	if( pThis ){` |
|        - |  7176 | `		/*` |
|        - |  7177 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  7178 | `		 */` |
|     1294 |  7179 | `		pThis->iRef++; /* Increment reference count */` |
|     1294 |  7180 | `		aStack[i].x.pOther = pThis;` |
|     1294 |  7181 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      646 |  7182 | `	}` |
|     1300 |  7183 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1300 |  7184 | `	i++;` |
|        - |  7185 | `	/* Push method name */` |
|     1300 |  7186 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1300 |  7187 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1300 |  7188 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1300 |  7189 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  7190 | `	/* Emit the CALL istruction */` |
|     1300 |  7191 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1300 |  7192 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1300 |  7193 | `	aInstr[0].iP2 = 0;` |
|     1300 |  7194 | `	aInstr[0].p3  = 0;` |
|        - |  7195 | `	/* Emit the DONE instruction */` |
|     1300 |  7196 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1300 |  7197 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1300 |  7198 | `	aInstr[1].iP2 = 0;` |
|     1300 |  7199 | `	aInstr[1].p3  = 0;` |
|        - |  7200 | `	/* Execute the method body (if available) */` |
|     1300 |  7201 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  7202 | `	/* Clean up the mess left behind */` |
|     1300 |  7203 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1300 |  7204 | `	return PH7_OK;` |
|      651 |  7205 |  |
|        - |  7206 | `/*` |
|        - |  7207 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  7208 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  7209 | ` * in the apArg[] array.` |
|        - |  7210 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7211 | ` * return value indicates failure.` |
|        - |  7212 | ` */` |
|      926 |  7213 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  7214 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7215 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7216 | `	int nArg,          /* Total number of given arguments */` |
|        - |  7217 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  7218 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  7219 | `	)` |
|        2 |  7220 |  |
|        - |  7221 | `	ph7_value *aStack;` |
|        - |  7222 | `	VmInstr aInstr[2];` |
|        - |  7223 | `	int i;` |
|      928 |  7224 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  7225 | `		/* Don't bother processing,it's invalid anyway */` |
|      471 |  7226 | `		if( pResult ){` |
|        - |  7227 | `			/* Assume a null return value */` |
|      ! 0 |  7228 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7229 | `		}` |
|      471 |  7230 | `		return SXERR_INVALID;` |
|        - |  7231 | `	}` |
|      458 |  7232 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7233 | `		/* Class method */` |
|       11 |  7234 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  7235 | `		ph7_class_method *pMethod = 0;` |
|       11 |  7236 | `		ph7_class_instance *pThis = 0;` |
|       11 |  7237 | `		ph7_class *pClass = 0;` |
|        - |  7238 | `		ph7_value *pValue;` |
|        - |  7239 | `		sxi32 rc;` |
|       11 |  7240 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  7241 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  7242 | `			if( pResult ){` |
|        - |  7243 | `				/* Assume a null return value */` |
|      ! 0 |  7244 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7245 | `			}` |
|      ! 0 |  7246 | `			return SXRET_OK;` |
|        - |  7247 | `		}` |
|        - |  7248 | `		/* Extract the class name or an instance of it */` |
|       11 |  7249 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  7250 | `		if( pValue ){` |
|       11 |  7251 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  7252 | `		}` |
|       11 |  7253 | `		if( pClass == 0 ){` |
|        - |  7254 | `			/* No such class,return NULL */` |
|      ! 0 |  7255 | `			if( pResult ){` |
|      ! 0 |  7256 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7257 | `			}` |
|      ! 0 |  7258 | `			return SXRET_OK;` |
|        - |  7259 | `		}` |
|       11 |  7260 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  7261 | `			/* Point to the class instance */` |
|        5 |  7262 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  7263 | `		}` |
|        - |  7264 | `		/* Try to extract the method */` |
|       11 |  7265 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  7266 | `		if( pValue ){` |
|       11 |  7267 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  7268 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  7269 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  7270 | `			}` |
|        5 |  7271 | `		}` |
|       11 |  7272 | `		if( pMethod == 0 ){` |
|        - |  7273 | `			/* No such method,return NULL */` |
|      ! 0 |  7274 | `			if( pResult ){` |
|      ! 0 |  7275 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7276 | `			}` |
|      ! 0 |  7277 | `			return SXRET_OK;` |
|        - |  7278 | `		}` |
|        - |  7279 | `		/* Call the class method */` |
|       11 |  7280 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  7281 | `		return rc;` |
|        - |  7282 | `	}` |
|        - |  7283 | `	/* Create a new operand stack */` |
|      448 |  7284 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      448 |  7285 | `	if( aStack == 0 ){` |
|      ! 0 |  7286 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7287 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  7288 | `		if( pResult ){` |
|        - |  7289 | `			/* Assume a null return value */` |
|      ! 0 |  7290 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7291 | `		}` |
|      ! 0 |  7292 | `		return SXERR_MEM;` |
|        - |  7293 | `	}` |
|        - |  7294 | `	/* Fill the operand stack with the given arguments */` |
|     1470 |  7295 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1024 |  7296 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7297 | `		/*` |
|        - |  7298 | `		 * Symisc eXtension:` |
|        - |  7299 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7300 | `		 */` |
|     1024 |  7301 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      513 |  7302 | `	}` |
|        - |  7303 | `	/* Push the function name */` |
|      448 |  7304 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      448 |  7305 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7306 | `	/* Emit the CALL istruction */` |
|      448 |  7307 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      448 |  7308 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      448 |  7309 | `	aInstr[0].iP2 = 0;` |
|      448 |  7310 | `	aInstr[0].p3  = 0;` |
|        - |  7311 | `	/* Emit the DONE instruction */` |
|      448 |  7312 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      448 |  7313 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      448 |  7314 | `	aInstr[1].iP2 = 0;` |
|      448 |  7315 | `	aInstr[1].p3  = 0;` |
|        - |  7316 | `	/* Execute the function body (if available) */` |
|      448 |  7317 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  7318 | `	/* Clean up the mess left behind */` |
|      448 |  7319 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      448 |  7320 | `	return PH7_OK;` |
|      465 |  7321 |  |
|        - |  7322 | `/*` |
|        - |  7323 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  7324 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  7325 | ` * parameter.` |
|        - |  7326 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7327 | ` * return value indicates failure.` |
|        - |  7328 | ` */` |
|      236 |  7329 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  7330 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7331 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7332 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  7333 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  7334 | `	)` |
|        1 |  7335 |  |
|        - |  7336 | `	ph7_value *pArg;` |
|        - |  7337 | `	SySet aArg;` |
|        - |  7338 | `	va_list ap;` |
|        - |  7339 | `	sxi32 rc;` |
|      237 |  7340 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7341 | `	/* Copy arguments one after one */` |
|      237 |  7342 | `	va_start(ap,pResult);` |
|      393 |  7343 | `	for(;;){` |
|      787 |  7344 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  7345 | `		if( pArg == 0 ){` |
|      237 |  7346 | `			break;` |
|        - |  7347 | `		}` |
|      551 |  7348 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  7349 | `	}` |
|        - |  7350 | `	/* Call the core routine */` |
|      237 |  7351 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  7352 | `	/* Cleanup */` |
|      237 |  7353 | `	SySetRelease(&aArg);` |
|      237 |  7354 | `	return rc;` |
|        1 |  7355 |  |
|        - |  7356 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  7357 | `/*` |
|        - |  7358 | ` * bool defined(string $name)` |
|        - |  7359 | ` *  Checks whether a given named constant exists.` |
|        - |  7360 | ` * Parameter:` |
|        - |  7361 | ` *  Name of the desired constant.` |
|        - |  7362 | ` * Return` |
|        - |  7363 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  7364 | ` */` |
|       14 |  7365 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7366 |  |
|        - |  7367 | `	const char *zName;` |
|       16 |  7368 | `	int nLen = 0;` |
|       16 |  7369 | `	int res = 0;` |
|       16 |  7370 | `	if( nArg < 1 ){` |
|        - |  7371 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  7372 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  7373 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7374 | `		return SXRET_OK;` |
|        - |  7375 | `	}` |
|        - |  7376 | `	/* Extract constant name */` |
|       16 |  7377 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7378 | `	/* Perform the lookup */` |
|       16 |  7379 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7380 | `		/* Already defined */` |
|       10 |  7381 | `		res = 1;` |
|        4 |  7382 | `	}` |
|       16 |  7383 | `	ph7_result_bool(pCtx,res);` |
|       16 |  7384 | `	return SXRET_OK;` |
|        9 |  7385 |  |
|        - |  7386 | `/*` |
|        - |  7387 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  7388 | ` * below.` |
|        - |  7389 | ` */` |
|        8 |  7390 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  7391 |  |
|       10 |  7392 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7393 | `	/* Expand constant value */` |
|       10 |  7394 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7395 |  |
|        - |  7396 | `/*` |
|        - |  7397 | ` * bool define(string $constant_name,expression value)` |
|        - |  7398 | ` *  Defines a named constant at runtime.` |
|        - |  7399 | ` * Parameter:` |
|        - |  7400 | ` *  $constant_name` |
|        - |  7401 | ` *   The name of the constant` |
|        - |  7402 | ` *  $value` |
|        - |  7403 | ` *   Constant value` |
|        - |  7404 | ` * Return:` |
|        - |  7405 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7406 | ` */` |
|       10 |  7407 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7408 |  |
|        - |  7409 | `	const char *zName;  /* Constant name */` |
|        - |  7410 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7411 | `	int nLen = 0;       /* Name length */` |
|        - |  7412 | `	sxi32 rc;` |
|       12 |  7413 | `	if( nArg < 2 ){` |
|        - |  7414 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7415 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7416 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7417 | `		return SXRET_OK;` |
|        - |  7418 | `	}` |
|       12 |  7419 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7420 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7421 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7422 | `		return SXRET_OK;` |
|        - |  7423 | `	}` |
|        - |  7424 | `	/* Extract constant name */` |
|       12 |  7425 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7426 | `	if( nLen < 1 ){` |
|      ! 0 |  7427 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7428 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7429 | `		return SXRET_OK;` |
|        - |  7430 | `	}` |
|        - |  7431 | `	/* Duplicate constant value */` |
|       12 |  7432 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7433 | `	if( pValue == 0 ){` |
|      ! 0 |  7434 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7435 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7436 | `		return SXRET_OK;` |
|        - |  7437 | `	}` |
|        - |  7438 | `	/* Initialize the memory object */` |
|       12 |  7439 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7440 | `	/* Register the constant */` |
|       12 |  7441 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7442 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7443 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7444 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7445 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7446 | `		return SXRET_OK;` |
|        - |  7447 | `	}` |
|        - |  7448 | `	/* Duplicate constant value */` |
|       12 |  7449 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7450 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7451 | `		/* Lower case the constant name */` |
|      ! 0 |  7452 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7453 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7454 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7455 | `				/* UTF-8 stream */` |
|      ! 0 |  7456 | `				zCur++;` |
|      ! 0 |  7457 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  7458 | `					zCur++;` |
|      ! 0 |  7459 | `				}` |
|      ! 0 |  7460 | `				continue;` |
|        - |  7461 | `			}` |
|      ! 0 |  7462 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  7463 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  7464 | `				zCur[0] = (char)c;` |
|      ! 0 |  7465 | `			}` |
|      ! 0 |  7466 | `			zCur++;` |
|      ! 0 |  7467 | `		}` |
|        - |  7468 | `		/* Finally,register the constant */` |
|      ! 0 |  7469 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  7470 | `	}` |
|        - |  7471 | `	/* All done,return TRUE */` |
|       12 |  7472 | `	ph7_result_bool(pCtx,1);` |
|       12 |  7473 | `	return SXRET_OK;` |
|        7 |  7474 |  |
|        - |  7475 | `/*` |
|        - |  7476 | ` * value constant(string $name)` |
|        - |  7477 | ` *  Returns the value of a constant` |
|        - |  7478 | ` * Parameter` |
|        - |  7479 | ` *  $name` |
|        - |  7480 | ` *    Name of the constant.` |
|        - |  7481 | ` * Return` |
|        - |  7482 | ` *  Constant value or NULL if not defined.` |
|        - |  7483 | ` */` |
|        8 |  7484 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7485 |  |
|        - |  7486 | `	SyHashEntry *pEntry;` |
|        - |  7487 | `	ph7_constant *pCons;` |
|        - |  7488 | `	const char *zName; /* Constant name */` |
|        - |  7489 | `	ph7_value sVal;    /* Constant value */` |
|        - |  7490 | `	int nLen;` |
|       10 |  7491 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  7492 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  7493 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  7494 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7495 | `		return SXRET_OK;` |
|        - |  7496 | `	}` |
|        - |  7497 | `	/* Extract the constant name */` |
|       10 |  7498 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7499 | `	/* Perform the query */` |
|       10 |  7500 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  7501 | `	if( pEntry == 0 ){` |
|        3 |  7502 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  7503 | `		ph7_result_null(pCtx);` |
|        3 |  7504 | `		return SXRET_OK;` |
|        - |  7505 | `	}` |
|        8 |  7506 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  7507 | `	/* Point to the structure that describe the constant */` |
|        8 |  7508 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  7509 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  7510 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  7511 | `	/* Return that value */` |
|        8 |  7512 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  7513 | `	/* Cleanup */` |
|        8 |  7514 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  7515 | `	return SXRET_OK;` |
|        6 |  7516 |  |
|        - |  7517 | `/*` |
|        - |  7518 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  7519 | ` * defined below.` |
|        - |  7520 | ` */` |
|      416 |  7521 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7522 |  |
|      417 |  7523 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  7524 | `	ph7_value sName;` |
|        - |  7525 | `	sxi32 rc;` |
|        - |  7526 | `	/* Prepare the constant name for insertion */` |
|      417 |  7527 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      417 |  7528 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  7529 | `	/* Perform the insertion */` |
|      417 |  7530 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      417 |  7531 | `	PH7_MemObjRelease(&sName);` |
|      417 |  7532 | `	return rc;` |
|        1 |  7533 |  |
|        - |  7534 | `/*` |
|        - |  7535 | ` * array get_defined_constants(void)` |
|        - |  7536 | ` *  Returns an associative array with the names of all defined` |
|        - |  7537 | ` *  constants.` |
|        - |  7538 | ` * Parameters` |
|        - |  7539 | ` *  NONE.` |
|        - |  7540 | ` * Returns` |
|        - |  7541 | ` *  Returns the names of all the constants currently defined.` |
|        - |  7542 | ` */` |
|        2 |  7543 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7544 |  |
|        - |  7545 | `	ph7_value *pArray;` |
|        - |  7546 | `	/* Create the array first*/` |
|        3 |  7547 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7548 | `	if( pArray == 0 ){` |
|      ! 0 |  7549 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7550 | `		SXUNUSED(apArg);` |
|        - |  7551 | `		/* Return NULL */` |
|      ! 0 |  7552 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7553 | `		return SXRET_OK;` |
|        - |  7554 | `	}` |
|        - |  7555 | `	/* Fill the array with the defined constants */` |
|        3 |  7556 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  7557 | `	/* Return the created array */` |
|        3 |  7558 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7559 | `	return SXRET_OK;` |
|        2 |  7560 |  |
|        - |  7561 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  7562 | `/*` |
|        - |  7563 | ` * Section:` |
|        - |  7564 | ` *  Random numbers/string generators.` |
|        - |  7565 | ` * Status:` |
|        - |  7566 | ` *    Stable.` |
|        - |  7567 | ` */` |
|        - |  7568 | `/*` |
|        - |  7569 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  7570 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  7571 | ` * used by te SQLite3 library.` |
|        - |  7572 | ` */` |
|     2438 |  7573 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  7574 |  |
|        - |  7575 | `	sxu32 iNum;` |
|     2440 |  7576 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2440 |  7577 | `	return iNum;` |
|        2 |  7578 |  |
|        - |  7579 | `/*` |
|        - |  7580 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  7581 | ` * Note that the generated string is NOT null terminated.` |
|        - |  7582 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  7583 | ` * by te SQLite3 library.` |
|        - |  7584 | ` */` |
|    76784 |  7585 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  7586 |  |
|        - |  7587 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  7588 | `	int i;` |
|        - |  7589 | `	/* Generate a binary string first */` |
|    76786 |  7590 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  7591 | `	/* Turn the binary string into english based alphabet */` |
|   844794 |  7592 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   768010 |  7593 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   384006 |  7594 | `	 }` |
|    76786 |  7595 |  |
|        - |  7596 | `/*` |
|        - |  7597 | ` * int rand()` |
|        - |  7598 | ` * int mt_rand()` |
|        - |  7599 | ` * int rand(int $min,int $max)` |
|        - |  7600 | ` * int mt_rand(int $min,int $max)` |
|        - |  7601 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  7602 | ` * Parameter` |
|        - |  7603 | ` *  $min` |
|        - |  7604 | ` *    The lowest value to return (default: 0)` |
|        - |  7605 | ` *  $max` |
|        - |  7606 | ` *   The highest value to return (default: getrandmax())` |
|        - |  7607 | ` * Return` |
|        - |  7608 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  7609 | ` * Note:` |
|        - |  7610 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7611 | ` *  by te SQLite3 library.` |
|        - |  7612 | ` */` |
|       20 |  7613 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7614 |  |
|        - |  7615 | `	sxu32 iNum;` |
|        - |  7616 | `	/* Generate the random number */` |
|       21 |  7617 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  7618 | `	if( nArg > 1 ){` |
|        - |  7619 | `		sxu32 iMin,iMax;` |
|        3 |  7620 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  7621 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  7622 | `		if( iMin < iMax ){` |
|        3 |  7623 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  7624 | `			if( iDiv > 0 ){` |
|        3 |  7625 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  7626 | `			}` |
|        1 |  7627 | `		}else if(iMax > 0 ){` |
|      ! 0 |  7628 | `			iNum %= iMax;` |
|      ! 0 |  7629 | `		}` |
|        1 |  7630 | `	}` |
|        - |  7631 | `	/* Return the number */` |
|       21 |  7632 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  7633 | `	return SXRET_OK;` |
|        1 |  7634 |  |
|        - |  7635 | `/*` |
|        - |  7636 | ` * int getrandmax(void)` |
|        - |  7637 | ` * int mt_getrandmax(void)` |
|        - |  7638 | ` * int rc4_getrandmax(void)` |
|        - |  7639 | ` *   Show largest possible random value` |
|        - |  7640 | ` * Return` |
|        - |  7641 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  7642 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  7643 | ` * Note:` |
|        - |  7644 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7645 | ` *  by te SQLite3 library.` |
|        - |  7646 | ` */` |
|        4 |  7647 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7648 |  |
|        2 |  7649 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  7650 | `	SXUNUSED(apArg);` |
|        5 |  7651 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  7652 | `	return SXRET_OK;` |
|        1 |  7653 |  |
|        - |  7654 | `/*` |
|        - |  7655 | ` * string rand_str()` |
|        - |  7656 | ` * string rand_str(int $len)` |
|        - |  7657 | ` *  Generate a random string (English alphabet).` |
|        - |  7658 | ` * Parameter` |
|        - |  7659 | ` *  $len` |
|        - |  7660 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  7661 | ` * Return` |
|        - |  7662 | ` *   A pseudo random string.` |
|        - |  7663 | ` * Note:` |
|        - |  7664 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7665 | ` *  by te SQLite3 library.` |
|        - |  7666 | ` *  This function is a symisc extension.` |
|        - |  7667 | ` */` |
|      120 |  7668 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7669 |  |
|        - |  7670 | `	char zString[1024];` |
|      122 |  7671 | `	int iLen = 0x10;` |
|      122 |  7672 | `	if( nArg > 0 ){` |
|        - |  7673 | `		/* Get the desired length */` |
|      122 |  7674 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  7675 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  7676 | `			/* Default length */` |
|        3 |  7677 | `			iLen = 0x10;` |
|        1 |  7678 | `		}` |
|       60 |  7679 | `	}` |
|        - |  7680 | `	/* Generate the random string */` |
|      122 |  7681 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  7682 | `	/* Return the generated string */` |
|      122 |  7683 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  7684 | `	return SXRET_OK;` |
|        2 |  7685 |  |
|        - |  7686 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  7687 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  7688 | `/* Unique ID private data */` |
|        - |  7689 | `struct unique_id_data` |
|        - |  7690 |  |
|        - |  7691 | `	ph7_context *pCtx; /* Call context */` |
|        - |  7692 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  7693 | `};` |
|        - |  7694 | `/*` |
|        - |  7695 | ` * Binary to hex consumer callback.` |
|        - |  7696 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  7697 | ` * defined below.` |
|        - |  7698 | ` */` |
|      192 |  7699 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  7700 |  |
|      193 |  7701 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  7702 | `	sxu32 nBuflen;` |
|        - |  7703 | `	/* Extract result buffer length */` |
|      193 |  7704 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  7705 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  7706 | `			/*` |
|        - |  7707 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  7708 | `			 * string will be 13 characters long` |
|        - |  7709 | `			 */` |
|       25 |  7710 | `		return SXERR_ABORT;` |
|        - |  7711 | `	}` |
|      169 |  7712 | `	if( nBuflen > 22 ){` |
|      ! 0 |  7713 | `		return SXERR_ABORT;` |
|        - |  7714 | `	}` |
|        - |  7715 | `	/* Safely Consume the hex stream */` |
|      169 |  7716 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  7717 | `	return SXRET_OK;` |
|       97 |  7718 |  |
|        - |  7719 | `/*` |
|        - |  7720 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  7721 | ` *  Generate a unique ID` |
|        - |  7722 | ` * Parameter` |
|        - |  7723 | ` * $prefix` |
|        - |  7724 | ` *  Append this prefix to the generated unique ID.` |
|        - |  7725 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  7726 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  7727 | ` * $more_entropy` |
|        - |  7728 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  7729 | ` *  that the result will be unique.` |
|        - |  7730 | ` * Return` |
|        - |  7731 | ` *  Returns the unique identifier, as a string.` |
|        - |  7732 | ` */` |
|       24 |  7733 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7734 |  |
|        - |  7735 | `	struct unique_id_data sUniq;` |
|        - |  7736 | `	unsigned char zDigest[20];` |
|       25 |  7737 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7738 | `	const char *zPrefix;` |
|        - |  7739 | `	SHA1Context sCtx;` |
|        - |  7740 | `	char zRandom[7];` |
|        - |  7741 | `	int nPrefix;` |
|        - |  7742 | `	int entropy;` |
|        - |  7743 | `	/* Generate a random string first */` |
|       25 |  7744 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  7745 | `	/* Initialize fields */` |
|       25 |  7746 | `	zPrefix = 0;` |
|       25 |  7747 | `	nPrefix = 0;` |
|       25 |  7748 | `	entropy = 0;` |
|       25 |  7749 | `	if( nArg > 0 ){` |
|        - |  7750 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  7751 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  7752 | `		if( nArg > 1 ){` |
|      ! 0 |  7753 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  7754 | `		}` |
|      ! 0 |  7755 | `	}` |
|       25 |  7756 | `	SHA1Init(&sCtx);` |
|        - |  7757 | `	/* Generate the random ID */` |
|       25 |  7758 | `	if( nPrefix > 0 ){` |
|      ! 0 |  7759 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  7760 | `	}` |
|        - |  7761 | `	/* Append the random ID */` |
|       25 |  7762 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  7763 | `	/* Append the random string */` |
|       25 |  7764 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  7765 | `	/* Increment the number */` |
|       25 |  7766 | `	pVm->unique_id++;` |
|       25 |  7767 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  7768 | `	/* Hexify the digest */` |
|       25 |  7769 | `	sUniq.pCtx = pCtx;` |
|       25 |  7770 | `	sUniq.entropy = entropy;` |
|       25 |  7771 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  7772 | `	/* All done */` |
|       25 |  7773 | `	return PH7_OK;` |
|        1 |  7774 |  |
|        - |  7775 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  7776 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  7777 | `/*` |
|        - |  7778 | ` * Section:` |
|        - |  7779 | ` *  Language construct implementation as foreign functions.` |
|        - |  7780 | ` * Status:` |
|        - |  7781 | ` *    Stable.` |
|        - |  7782 | ` */` |
|        - |  7783 | `/*` |
|        - |  7784 | ` * void echo($string...)` |
|        - |  7785 | ` *  Output one or more messages.` |
|        - |  7786 | ` * Parameters` |
|        - |  7787 | ` *  $string` |
|        - |  7788 | ` *   Message to output.` |
|        - |  7789 | ` * Return` |
|        - |  7790 | ` *  NULL.` |
|        - |  7791 | ` */` |
|      ! 0 |  7792 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7793 |  |
|        - |  7794 | `	const char *zData;` |
|      ! 0 |  7795 | `	int nDataLen = 0;` |
|        - |  7796 | `	ph7_vm *pVm;` |
|        - |  7797 | `	int i,rc;` |
|        - |  7798 | `	/* Point to the target VM */` |
|      ! 0 |  7799 | `	pVm = pCtx->pVm;` |
|        - |  7800 | `	/* Output */` |
|      ! 0 |  7801 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  7802 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  7803 | `		if( nDataLen > 0 ){` |
|      ! 0 |  7804 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  7805 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 |  7806 | `			if( rc == SXERR_ABORT ){` |
|        - |  7807 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7808 | `				return PH7_ABORT;` |
|        - |  7809 | `			}` |
|      ! 0 |  7810 | `		}` |
|      ! 0 |  7811 | `	}` |
|      ! 0 |  7812 | `	return SXRET_OK;` |
|      ! 0 |  7813 |  |
|        - |  7814 | `/*` |
|        - |  7815 | ` * int print($string...)` |
|        - |  7816 | ` *  Output one or more messages.` |
|        - |  7817 | ` * Parameters` |
|        - |  7818 | ` *  $string` |
|        - |  7819 | ` *   Message to output.` |
|        - |  7820 | ` * Return` |
|        - |  7821 | ` *  1 always.` |
|        - |  7822 | ` */` |
|        2 |  7823 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7824 |  |
|        - |  7825 | `	const char *zData;` |
|        3 |  7826 | `	int nDataLen = 0;` |
|        - |  7827 | `	ph7_vm *pVm;` |
|        - |  7828 | `	int i,rc;` |
|        - |  7829 | `	/* Point to the target VM */` |
|        3 |  7830 | `	pVm = pCtx->pVm;` |
|        - |  7831 | `	/* Output */` |
|        5 |  7832 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  7833 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  7834 | `		if( nDataLen > 0 ){` |
|        3 |  7835 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  7836 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 |  7837 | `			if( rc == SXERR_ABORT ){` |
|        - |  7838 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7839 | `				return PH7_ABORT;` |
|        - |  7840 | `			}` |
|        1 |  7841 | `		}` |
|        2 |  7842 | `	}` |
|        - |  7843 | `	/* Return 1 */` |
|        3 |  7844 | `	ph7_result_int(pCtx,1);` |
|        3 |  7845 | `	return SXRET_OK;` |
|        2 |  7846 |  |
|        - |  7847 | `/*` |
|        - |  7848 | ` * void exit(string $msg)` |
|        - |  7849 | ` * void exit(int $status)` |
|        - |  7850 | ` * void die(string $ms)` |
|        - |  7851 | ` * void die(int $status)` |
|        - |  7852 | ` *   Output a message and terminate program execution.` |
|        - |  7853 | ` * Parameter` |
|        - |  7854 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  7855 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  7856 | ` *  and not printed` |
|        - |  7857 | ` * Return` |
|        - |  7858 | ` *  NULL` |
|        - |  7859 | ` */` |
|      ! 0 |  7860 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7861 |  |
|      ! 0 |  7862 | `	if( nArg > 0 ){` |
|      ! 0 |  7863 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  7864 | `			const char *zData;` |
|      ! 0 |  7865 | `			int iLen = 0;` |
|        - |  7866 | `			/* Print exit message */` |
|      ! 0 |  7867 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  7868 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  7869 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  7870 | `			sxi32 iExitStatus;` |
|        - |  7871 | `			/* Record exit status code */` |
|      ! 0 |  7872 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  7873 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  7874 | `		}` |
|      ! 0 |  7875 | `	}` |
|        - |  7876 | `	/* Check if we are in an included file */` |
|      ! 0 |  7877 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  7878 | `		/* Exit the entire process */` |
|      ! 0 |  7879 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  7880 | `	}` |
|        - |  7881 | `	/* Abort processing immediately */` |
|      ! 0 |  7882 | `	return PH7_ABORT;` |
|      ! 0 |  7883 |  |
|        - |  7884 | `/*` |
|        - |  7885 | ` * bool isset($var,...)` |
|        - |  7886 | ` *  Finds out whether a variable is set.` |
|        - |  7887 | ` * Parameters` |
|        - |  7888 | ` *  One or more variable to check.` |
|        - |  7889 | ` * Return` |
|        - |  7890 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  7891 | ` */` |
|    72634 |  7892 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7893 |  |
|        - |  7894 | `	ph7_value *pObj;` |
|    72636 |  7895 | `	int res = 0;` |
|        - |  7896 | `	int i;` |
|    72636 |  7897 | `	if( nArg < 1 ){` |
|        - |  7898 | `		/* Missing arguments,return false */` |
|      ! 0 |  7899 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  7900 | `		return SXRET_OK;` |
|        - |  7901 | `	}` |
|        - |  7902 | `	/* Iterate over available arguments */` |
|    95820 |  7903 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    72636 |  7904 | `		pObj = apArg[i];` |
|    72636 |  7905 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    48944 |  7906 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7907 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  7908 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  7909 | `			}` |
|    24471 |  7910 | `		}` |
|    72636 |  7911 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    72636 |  7912 | `		if( !res ){` |
|        - |  7913 | `			/* Variable not set,return FALSE */` |
|    49452 |  7914 | `			ph7_result_bool(pCtx,0);` |
|    49452 |  7915 | `			return SXRET_OK;` |
|        - |  7916 | `		}` |
|    11594 |  7917 | `	}` |
|        - |  7918 | `	/* All given variable are set,return TRUE */` |
|    23186 |  7919 | `	ph7_result_bool(pCtx,1);` |
|    23186 |  7920 | `	return SXRET_OK;` |
|    36319 |  7921 |  |
|        - |  7922 | `/*` |
|        - |  7923 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  7924 | ` * frame,the reference table and discard it's contents.` |
|        - |  7925 | ` * This function never fail and always return SXRET_OK.` |
|        - |  7926 | ` */` |
|  2974900 |  7927 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  7928 |  |
|        - |  7929 | `	ph7_value *pObj;` |
|        - |  7930 | `	VmRefObj *pRef;` |
|  2974902 |  7931 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2974902 |  7932 | `	if( pObj ){` |
|        - |  7933 | `		/* Release the object */` |
|  2974902 |  7934 | `		PH7_MemObjRelease(pObj);` |
|  1487450 |  7935 | `	}` |
|        - |  7936 | `	/* Remove old reference links */` |
|  2974902 |  7937 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2974902 |  7938 | `	if( pRef ){` |
|  2974896 |  7939 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  7940 | `		/* Unlink from the reference table */` |
|  2974896 |  7941 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2974896 |  7942 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  7943 | `			VmSlot sFree;` |
|        - |  7944 | `			/* Restore to the free list */` |
|  2974890 |  7945 | `			sFree.nIdx = nObjIdx;` |
|  2974890 |  7946 | `			sFree.pUserData = 0;` |
|  2974890 |  7947 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1487444 |  7948 | `		}` |
|  1487447 |  7949 | `	}` |
|  2974902 |  7950 | `	return SXRET_OK;` |
|        2 |  7951 |  |
|        - |  7952 | `/*` |
|        - |  7953 | ` * void unset($var,...)` |
|        - |  7954 | ` *   Unset one or more given variable.` |
|        - |  7955 | ` * Parameters` |
|        - |  7956 | ` *  One or more variable to unset.` |
|        - |  7957 | ` * Return` |
|        - |  7958 | ` *  Nothing.` |
|        - |  7959 | ` */` |
|     6678 |  7960 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7961 |  |
|        - |  7962 | `	ph7_value *pObj;` |
|        - |  7963 | `	ph7_vm *pVm;` |
|        - |  7964 | `	int i;` |
|        - |  7965 | `	/* Point to the target VM */` |
|     6680 |  7966 | `	pVm = pCtx->pVm;` |
|        - |  7967 | `	/* Iterate and unset */` |
|    13358 |  7968 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6680 |  7969 | `		pObj = apArg[i];` |
|     6680 |  7970 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 |  7971 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7972 | `				/* Throw an error */` |
|      ! 0 |  7973 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  7974 | `			}` |
|      ! 0 |  7975 | `		}else{` |
|     6680 |  7976 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  7977 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6680 |  7978 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6674 |  7979 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3336 |  7980 | `			}` |
|        - |  7981 | `		}` |
|     3341 |  7982 | `	}` |
|     6680 |  7983 | `	return SXRET_OK;` |
|        2 |  7984 |  |
|        - |  7985 | `/*` |
|        - |  7986 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  7987 | ` */` |
|      110 |  7988 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7989 |  |
|      111 |  7990 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  7991 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  7992 | `	ph7_value *pObj;` |
|        - |  7993 | `	sxu32 nIdx;` |
|        - |  7994 | `	/* Extract the memory object */` |
|      111 |  7995 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  7996 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  7997 | `	if( pObj ){` |
|      111 |  7998 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  7999 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  8000 | `				SyString sName;` |
|        - |  8001 | `				ph7_value sKey;` |
|        - |  8002 | `				/* Perform the insertion */` |
|      109 |  8003 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  8004 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  8005 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  8006 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  8007 | `			}` |
|       54 |  8008 | `		}` |
|       55 |  8009 | `	}` |
|      111 |  8010 | `	return SXRET_OK;` |
|        1 |  8011 |  |
|        - |  8012 | `/*` |
|        - |  8013 | ` * array get_defined_vars(void)` |
|        - |  8014 | ` *  Returns an array of all defined variables.` |
|        - |  8015 | ` * Parameter` |
|        - |  8016 | ` *  None` |
|        - |  8017 | ` * Return` |
|        - |  8018 | ` *  An array with all the variables defined in the current scope.` |
|        - |  8019 | ` */` |
|        2 |  8020 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8021 |  |
|        3 |  8022 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8023 | `	ph7_value *pArray;` |
|        - |  8024 | `	/* Create a new array */` |
|        3 |  8025 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8026 | ` 	if( pArray == 0 ){` |
|      ! 0 |  8027 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8028 | `		SXUNUSED(apArg);` |
|        - |  8029 | `		/* Return NULL */` |
|      ! 0 |  8030 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8031 | `		return SXRET_OK;` |
|        - |  8032 | `	}` |
|        - |  8033 | `	/* Superglobals first */` |
|        3 |  8034 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  8035 | `	/* Then variable defined in the current frame */` |
|        3 |  8036 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  8037 | `	/* Finally,return the created array */` |
|        3 |  8038 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8039 | `	return SXRET_OK;` |
|        2 |  8040 |  |
|        - |  8041 | `/*` |
|        - |  8042 | ` * bool gettype($var)` |
|        - |  8043 | ` *  Get the type of a variable` |
|        - |  8044 | ` * Parameters` |
|        - |  8045 | ` *   $var` |
|        - |  8046 | ` *    The variable being type checked.` |
|        - |  8047 | ` * Return` |
|        - |  8048 | ` *   String representation of the given variable type.` |
|        - |  8049 | ` */` |
|       32 |  8050 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8051 |  |
|       34 |  8052 | `	const char *zType = "Empty";` |
|       34 |  8053 | `	if( nArg > 0 ){` |
|       34 |  8054 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  8055 | `	}` |
|        - |  8056 | `	/* Return the variable type */` |
|       34 |  8057 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  8058 | `	return SXRET_OK;` |
|        2 |  8059 |  |
|        - |  8060 | `/*` |
|        - |  8061 | ` * string get_resource_type(resource $handle)` |
|        - |  8062 | ` *  This function gets the type of the given resource.` |
|        - |  8063 | ` * Parameters` |
|        - |  8064 | ` *  $handle` |
|        - |  8065 | ` *  The evaluated resource handle.` |
|        - |  8066 | ` * Return` |
|        - |  8067 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  8068 | ` *  representing its type. If the type is not identified by this function` |
|        - |  8069 | ` *  the return value will be the string Unknown.` |
|        - |  8070 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  8071 | ` *  is not a resource.` |
|        - |  8072 | ` */` |
|        2 |  8073 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8074 |  |
|        3 |  8075 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  8076 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  8077 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8078 | `		return PH7_OK;` |
|        - |  8079 | `	}` |
|        3 |  8080 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  8081 | `	return SXRET_OK;` |
|        2 |  8082 |  |
|        - |  8083 | `/*` |
|        - |  8084 | ` * void var_dump(expression,....)` |
|        - |  8085 | ` *   var_dump � Dumps information about a variable` |
|        - |  8086 | ` * Parameters` |
|        - |  8087 | ` *   One or more expression to dump.` |
|        - |  8088 | ` * Returns` |
|        - |  8089 | ` *  Nothing.` |
|        - |  8090 | ` */` |
|      218 |  8091 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8092 |  |
|        - |  8093 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  8094 | `	int i;` |
|      220 |  8095 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  8096 | `	/* Dump one or more expressions */` |
|      444 |  8097 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  8098 | `		ph7_value *pObj = apArg[i];` |
|        - |  8099 | `		/* Reset the working buffer */` |
|      226 |  8100 | `		SyBlobReset(&sDump);` |
|        - |  8101 | `		/* Dump the given expression */` |
|      226 |  8102 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  8103 | `		/* Output */` |
|      226 |  8104 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  8105 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  8106 | `		}` |
|      114 |  8107 | `	}` |
|        - |  8108 | `	/* Release the working buffer */` |
|      220 |  8109 | `	SyBlobRelease(&sDump);` |
|      220 |  8110 | `	return SXRET_OK;` |
|        2 |  8111 |  |
|        - |  8112 | `/*` |
|        - |  8113 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  8114 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  8115 | ` * Parameters` |
|        - |  8116 | ` *   expression: Expression to dump` |
|        - |  8117 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  8118 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  8119 | ` *            print_r() will return the information rather than print it.` |
|        - |  8120 | ` * Return` |
|        - |  8121 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  8122 | ` *  Otherwise, the return value is TRUE.` |
|        - |  8123 | ` */` |
|       16 |  8124 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8125 |  |
|       17 |  8126 | `	int ret_string = 0;` |
|        - |  8127 | `	SyBlob sDump;` |
|       17 |  8128 | `	if( nArg < 1 ){` |
|        - |  8129 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  8130 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8131 | `		return SXRET_OK;` |
|        - |  8132 | `	}` |
|       17 |  8133 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  8134 | `	if ( nArg > 1 ){` |
|        - |  8135 | `		/* Where to redirect output */` |
|       11 |  8136 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  8137 | `	}` |
|        - |  8138 | `	/* Generate dump */` |
|       17 |  8139 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  8140 | `	if( !ret_string ){` |
|        - |  8141 | `		/* Output dump */` |
|        7 |  8142 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8143 | `		/* Return true */` |
|        7 |  8144 | `		ph7_result_bool(pCtx,1);` |
|        4 |  8145 | `	}else{` |
|        - |  8146 | `		/* Generated dump as return value */` |
|       11 |  8147 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8148 | `	}` |
|        - |  8149 | `	/* Release the working buffer */` |
|       17 |  8150 | `	SyBlobRelease(&sDump);` |
|       17 |  8151 | `	return SXRET_OK;` |
|        9 |  8152 |  |
|        - |  8153 | `/*` |
|        - |  8154 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  8155 | ` * Same job as print_r. (see coment above)` |
|        - |  8156 | ` */` |
|        2 |  8157 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8158 |  |
|        3 |  8159 | `	int ret_string = 0;` |
|        - |  8160 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  8161 | `	if( nArg < 1 ){` |
|        - |  8162 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  8163 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8164 | `		return SXRET_OK;` |
|        - |  8165 | `	}` |
|        3 |  8166 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  8167 | `	if ( nArg > 1 ){` |
|        - |  8168 | `		/* Where to redirect output */` |
|        3 |  8169 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  8170 | `	}` |
|        - |  8171 | `	/* Generate dump */` |
|        3 |  8172 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  8173 | `	if( !ret_string ){` |
|        - |  8174 | `		/* Output dump */` |
|      ! 0 |  8175 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8176 | `		/* Return NULL */` |
|      ! 0 |  8177 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8178 | `	}else{` |
|        - |  8179 | `		/* Generated dump as return value */` |
|        3 |  8180 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8181 | `	}` |
|        - |  8182 | `	/* Release the working buffer */` |
|        3 |  8183 | `	SyBlobRelease(&sDump);` |
|        3 |  8184 | `	return SXRET_OK;` |
|        2 |  8185 |  |
|        - |  8186 | `/*` |
|        - |  8187 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  8188 | ` *  Set/get the various assert flags.` |
|        - |  8189 | ` * Parameter` |
|        - |  8190 | ` * $what` |
|        - |  8191 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  8192 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  8193 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  8194 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  8195 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  8196 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  8197 | ` * $value` |
|        - |  8198 | ` *   An optional new value for the option.` |
|        - |  8199 | ` * Return` |
|        - |  8200 | ` *  Old setting on success or FALSE on failure.` |
|        - |  8201 | ` */` |
|       30 |  8202 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8203 |  |
|       32 |  8204 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8205 | `	int iOption;` |
|        - |  8206 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       32 |  8207 | `	if( nArg < 1 ){` |
|        3 |  8208 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8209 | `			"ArgumentCountError",` |
|        - |  8210 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  8211 | `			);` |
|        - |  8212 | `	}` |
|        - |  8213 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       28 |  8214 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       30 |  8215 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  8216 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8217 | `			"TypeError",` |
|        - |  8218 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  8219 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  8220 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  8221 | `			);` |
|        - |  8222 | `	}` |
|       30 |  8223 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  8224 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  8225 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  8226 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       30 |  8227 | `	switch( iOption ){` |
|        6 |  8228 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  8229 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       14 |  8230 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       14 |  8231 | `		if( nArg > 1 ){` |
|        5 |  8232 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  8233 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  8234 | `			}else{` |
|        3 |  8235 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  8236 | `			}` |
|        2 |  8237 | `		}` |
|       14 |  8238 | `		break;` |
|        1 |  8239 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  8240 | `		/* Return old callback or null */` |
|        3 |  8241 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  8242 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  8243 | `		}else{` |
|        3 |  8244 | `			ph7_result_null(pCtx);` |
|        - |  8245 | `		}` |
|        3 |  8246 | `		if( nArg > 1 ){` |
|      ! 0 |  8247 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  8248 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  8249 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  8250 | `			}else{` |
|      ! 0 |  8251 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  8252 | `			}` |
|      ! 0 |  8253 | `		}` |
|        3 |  8254 | `		break;` |
|        5 |  8255 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  8256 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  8257 | `		if( nArg > 1 ){` |
|        5 |  8258 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  8259 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  8260 | `			}else{` |
|        3 |  8261 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  8262 | `			}` |
|        2 |  8263 | `		}` |
|       11 |  8264 | `		break;` |
|      ! 0 |  8265 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  8266 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  8267 | `		break;` |
|        1 |  8268 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  8269 | `		ph7_result_int(pCtx, 1);` |
|        3 |  8270 | `		break;` |
|      ! 0 |  8271 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  8272 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  8273 | `		break;` |
|        1 |  8274 | `	default:` |
|        - |  8275 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  8276 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8277 | `			"ValueError",` |
|        - |  8278 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  8279 | `			);` |
|        - |  8280 | `	}` |
|       28 |  8281 | `	return PH7_OK;` |
|       17 |  8282 |  |
|        - |  8283 | `/*` |
|        - |  8284 | ` * bool assert(mixed $assertion)` |
|        - |  8285 | ` *  Checks if assertion is FALSE.` |
|        - |  8286 | ` * Parameter` |
|        - |  8287 | ` *  $assertion` |
|        - |  8288 | ` *    The assertion to test.` |
|        - |  8289 | ` * Return` |
|        - |  8290 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  8291 | ` */` |
|       26 |  8292 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8293 |  |
|       28 |  8294 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8295 | `	int iFlags,iResult;` |
|        - |  8296 | `	const char *zDesc;` |
|        - |  8297 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       28 |  8298 | `	if( nArg < 1 ){` |
|        3 |  8299 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8300 | `			"ArgumentCountError",` |
|        - |  8301 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  8302 | `			);` |
|        - |  8303 | `	}` |
|       26 |  8304 | `	iFlags = pVm->iAssertFlags;` |
|       26 |  8305 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  8306 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  8307 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  8308 | `		return PH7_OK;` |
|        - |  8309 | `	}` |
|        - |  8310 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       26 |  8311 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       26 |  8312 | `	if( !iResult ){` |
|        - |  8313 | `		/* Assertion failed */` |
|        - |  8314 | `		/* Extract optional description */` |
|       13 |  8315 | `		zDesc = 0;` |
|       13 |  8316 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  8317 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  8318 | `		}` |
|       13 |  8319 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  8320 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  8321 | `			ph7_value sFile,sLine;` |
|        - |  8322 | `			ph7_value *apCbArg[3];` |
|        - |  8323 | `			SyString *pFile;` |
|        - |  8324 | `			/* Extract the processed script */` |
|      ! 0 |  8325 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  8326 | `			if( pFile == 0 ){` |
|      ! 0 |  8327 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  8328 | `			}` |
|        - |  8329 | `			/* Invoke the callback */` |
|      ! 0 |  8330 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  8331 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  8332 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  8333 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  8334 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  8335 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  8336 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  8337 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  8338 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  8339 | `		}` |
|       13 |  8340 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  8341 | `			/* Abort VM execution immediately */` |
|      ! 0 |  8342 | `			return PH7_ABORT;` |
|        - |  8343 | `		}` |
|        - |  8344 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  8345 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  8346 | `			return PH7_VmThrowException(pCtx,` |
|        - |  8347 | `				"AssertionError",` |
|        - |  8348 | `				"%s",` |
|        1 |  8349 | `				zDesc` |
|        - |  8350 | `				);` |
|      ! 0 |  8351 | `		}else{` |
|       11 |  8352 | `			return PH7_VmThrowException(pCtx,` |
|        - |  8353 | `				"AssertionError",` |
|        - |  8354 | `				"assert(false)"` |
|        - |  8355 | `				);` |
|        - |  8356 | `		}` |
|        - |  8357 | `	}` |
|        - |  8358 | `	/* Assertion passed */` |
|       14 |  8359 | `	ph7_result_bool(pCtx,1);` |
|       14 |  8360 | `	return PH7_OK;` |
|       15 |  8361 |  |
|        - |  8362 | `/*` |
|        - |  8363 | ` * Section:` |
|        - |  8364 | ` *  Error reporting functions.` |
|        - |  8365 | ` * Status:` |
|        - |  8366 | ` *    Stable.` |
|        - |  8367 | ` */` |
|        - |  8368 | `/*` |
|        - |  8369 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  8370 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  8371 | ` * Parameters` |
|        - |  8372 | ` *  $error_msg` |
|        - |  8373 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  8374 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  8375 | ` * $error_type` |
|        - |  8376 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  8377 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  8378 | ` * Return` |
|        - |  8379 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  8380 | ` */` |
|       12 |  8381 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8382 |  |
|       14 |  8383 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  8384 | `	int rc = PH7_OK;` |
|       14 |  8385 | `	if( nArg > 0 ){` |
|        - |  8386 | `		const char *zErr;` |
|        - |  8387 | `		int nLen;` |
|        - |  8388 | `		/* Extract the error message */` |
|       12 |  8389 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8390 | `		if( nArg > 1 ){` |
|        - |  8391 | `			/* Extract the error type */` |
|       12 |  8392 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  8393 | `			switch( nErr ){` |
|        1 |  8394 | `			case 1:   /* E_ERROR */` |
|        - |  8395 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  8396 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  8397 | `			case 256: /* E_USER_ERROR */` |
|        3 |  8398 | `				nErr = PH7_CTX_ERR;` |
|        3 |  8399 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  8400 | `				break;` |
|        1 |  8401 | `			case 2:   /* E_WARNING */` |
|        - |  8402 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  8403 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  8404 | `			case 512: /* E_USER_WARNING */` |
|        3 |  8405 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  8406 | `				break;` |
|        3 |  8407 | `			default:` |
|        8 |  8408 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  8409 | `				break;` |
|        - |  8410 | `			}` |
|        5 |  8411 | `		}` |
|        - |  8412 | `		/* Report error */` |
|       12 |  8413 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  8414 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  8415 | `			return rc;` |
|        - |  8416 | `		}` |
|        - |  8417 | `		/* Return true */` |
|       12 |  8418 | `		ph7_result_bool(pCtx,1);` |
|        7 |  8419 | `	}else{` |
|        - |  8420 | `		/* Missing arguments,return FALSE */` |
|        3 |  8421 | `		ph7_result_bool(pCtx,0);` |
|        - |  8422 | `	}` |
|       14 |  8423 | `	return rc;` |
|        8 |  8424 |  |
|        - |  8425 | `/*` |
|        - |  8426 | ` * int error_reporting([int $level])` |
|        - |  8427 | ` *  Sets which PHP errors are reported.` |
|        - |  8428 | ` * Parameters` |
|        - |  8429 | ` *  $level` |
|        - |  8430 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  8431 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  8432 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  8433 | ` *   levels will not always behave as expected.` |
|        - |  8434 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  8435 | ` *   in the predefined constants.` |
|        - |  8436 | ` * Return` |
|        - |  8437 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  8438 | ` *   parameter is given.` |
|        - |  8439 | ` */` |
|       42 |  8440 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8441 |  |
|       44 |  8442 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8443 | `	int nOld;` |
|        - |  8444 | `	/* Extract the old reporting level */` |
|       44 |  8445 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       44 |  8446 | `	if( nArg > 0 ){` |
|        - |  8447 | `		int nNew;` |
|        - |  8448 | `		/* Extract the desired error reporting level */` |
|       36 |  8449 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       36 |  8450 | `		if( !nNew ){` |
|        - |  8451 | `			/* Do not report errors at all */` |
|        5 |  8452 | `			pVm->bErrReport = 0;` |
|        3 |  8453 | `		}else{` |
|        - |  8454 | `			/* Report all errors */` |
|       32 |  8455 | `			pVm->bErrReport = 1;` |
|        - |  8456 | `		}` |
|       17 |  8457 | `	}` |
|        - |  8458 | `	/* Return the old level */` |
|       44 |  8459 | `	ph7_result_int(pCtx,nOld);` |
|       44 |  8460 | `	return PH7_OK;` |
|        2 |  8461 |  |
|        - |  8462 | `/*` |
|        - |  8463 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  8464 | ` *  Send an error message somewhere.` |
|        - |  8465 | ` * Parameter` |
|        - |  8466 | ` *  $message` |
|        - |  8467 | ` *   The error message that should be logged.` |
|        - |  8468 | ` *  $message_type` |
|        - |  8469 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  8470 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  8471 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  8472 | ` *       This is the default option.` |
|        - |  8473 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  8474 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  8475 | ` *    2  No longer an option.` |
|        - |  8476 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  8477 | ` *       to the end of the message string.` |
|        - |  8478 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  8479 | ` *  $destination` |
|        - |  8480 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  8481 | ` *  $extra_headers` |
|        - |  8482 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  8483 | ` * Return` |
|        - |  8484 | ` *  TRUE on success or FALSE on failure.` |
|        - |  8485 | ` * NOTE:` |
|        - |  8486 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  8487 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  8488 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  8489 | ` *  Otherwise this function is no-op.` |
|        - |  8490 | ` */` |
|        4 |  8491 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8492 |  |
|        - |  8493 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  8494 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  8495 | `	int iType = 0;` |
|        5 |  8496 | `	if( nArg < 1 ){` |
|        - |  8497 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  8498 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8499 | `		return PH7_OK;` |
|        - |  8500 | `	}` |
|        5 |  8501 | `	if( pVm->xErrLog  ){` |
|        - |  8502 | `		/* Invoke the user callback */` |
|      ! 0 |  8503 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  8504 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  8505 | `		if( nArg > 1 ){` |
|      ! 0 |  8506 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  8507 | `			if( nArg > 2 ){` |
|      ! 0 |  8508 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  8509 | `				if( nArg > 3 ){` |
|      ! 0 |  8510 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  8511 | `				}` |
|      ! 0 |  8512 | `			}` |
|      ! 0 |  8513 | `		}` |
|      ! 0 |  8514 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  8515 | `	}` |
|        - |  8516 | `	/* Retun TRUE */` |
|        5 |  8517 | `	ph7_result_bool(pCtx,1);` |
|        5 |  8518 | `	return PH7_OK;` |
|        3 |  8519 |  |
|        - |  8520 | `/*` |
|        - |  8521 | ` * bool restore_exception_handler(void)` |
|        - |  8522 | ` *  Restores the previously defined exception handler function.` |
|        - |  8523 | ` * Parameter` |
|        - |  8524 | ` *  None` |
|        - |  8525 | ` * Return` |
|        - |  8526 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  8527 | ` */` |
|        4 |  8528 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8529 |  |
|        5 |  8530 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8531 | `	ph7_value *pOld,*pNew;` |
|        - |  8532 | `	/* Point to the old and the new handler */` |
|        5 |  8533 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  8534 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  8535 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8536 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8537 | `		SXUNUSED(apArg);` |
|        - |  8538 | `		/* No installed handler,return FALSE */` |
|        5 |  8539 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8540 | `		return PH7_OK;` |
|        - |  8541 | `	}` |
|        - |  8542 | `	/* Copy the old handler */` |
|      ! 0 |  8543 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8544 | `	PH7_MemObjRelease(pOld);` |
|        - |  8545 | `	/* Return TRUE */` |
|      ! 0 |  8546 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8547 | `	return PH7_OK;` |
|        3 |  8548 |  |
|        - |  8549 | `/*` |
|        - |  8550 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  8551 | ` *  Sets a user-defined exception handler function.` |
|        - |  8552 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  8553 | ` * NOTE` |
|        - |  8554 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  8555 | ` *  the satndard PHP engine.` |
|        - |  8556 | ` * Parameters` |
|        - |  8557 | ` *  $exception_handler` |
|        - |  8558 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  8559 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  8560 | ` *   that was thrown.` |
|        - |  8561 | ` *  Note:` |
|        - |  8562 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8563 | ` * Return` |
|        - |  8564 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  8565 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8566 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8567 | ` */` |
|        4 |  8568 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8569 |  |
|        6 |  8570 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8571 | `	ph7_value *pOld,*pNew;` |
|        - |  8572 | `	/* Point to the old and the new handler */` |
|        6 |  8573 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  8574 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  8575 | `	/* Return the old handler */` |
|        6 |  8576 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  8577 | `	if( nArg > 0 ){` |
|        6 |  8578 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8579 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  8580 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  8581 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  8582 | `		}else{` |
|        6 |  8583 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8584 | `			/* Install the new handler */` |
|        6 |  8585 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8586 | `		}` |
|        2 |  8587 | `	}` |
|        6 |  8588 | `	return PH7_OK;` |
|        2 |  8589 |  |
|        - |  8590 | `/*` |
|        - |  8591 | ` * bool restore_error_handler(void)` |
|        - |  8592 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8593 | ` * Parameters:` |
|        - |  8594 | ` *  None.` |
|        - |  8595 | ` * Return` |
|        - |  8596 | ` *  Always TRUE.` |
|        - |  8597 | ` */` |
|        4 |  8598 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8599 |  |
|        5 |  8600 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8601 | `	ph7_value *pOld,*pNew;` |
|        - |  8602 | `	/* Point to the old and the new handler */` |
|        5 |  8603 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  8604 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  8605 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8606 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8607 | `		SXUNUSED(apArg);` |
|        - |  8608 | `		/* No installed callback,return FALSE */` |
|        5 |  8609 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8610 | `		return PH7_OK;` |
|        - |  8611 | `	}` |
|        - |  8612 | `	/* Copy the old callback */` |
|      ! 0 |  8613 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8614 | `	PH7_MemObjRelease(pOld);` |
|        - |  8615 | `	/* Return TRUE */` |
|      ! 0 |  8616 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8617 | `	return PH7_OK;` |
|        3 |  8618 |  |
|        - |  8619 | `/*` |
|        - |  8620 | ` * value set_error_handler(callable $error_handler)` |
|        - |  8621 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8622 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8623 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8624 | ` *  Sets a user-defined error handler function.` |
|        - |  8625 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  8626 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  8627 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  8628 | ` *  conditions (using trigger_error()).` |
|        - |  8629 | ` * Parameters` |
|        - |  8630 | ` *  $error_handler` |
|        - |  8631 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  8632 | ` *   describing the error.` |
|        - |  8633 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  8634 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  8635 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  8636 | ` *   The function can be shown as:` |
|        - |  8637 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  8638 | ` *     errno` |
|        - |  8639 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  8640 | ` *   errstr` |
|        - |  8641 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  8642 | ` *   errfile` |
|        - |  8643 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  8644 | ` *     was raised in, as a string.` |
|        - |  8645 | ` *  Note:` |
|        - |  8646 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8647 | ` * Return` |
|        - |  8648 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  8649 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8650 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8651 | ` */` |
|     8822 |  8652 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8653 |  |
|     8824 |  8654 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8655 | `	ph7_value *pOld,*pNew;` |
|        - |  8656 | `	/* Point to the old and the new handler */` |
|     8824 |  8657 | `	pOld = &pVm->aErrCB[0];` |
|     8824 |  8658 | `	pNew = &pVm->aErrCB[1];` |
|        - |  8659 | `	/* Return the old handler */` |
|     8824 |  8660 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8824 |  8661 | `	if( nArg > 0 ){` |
|     8824 |  8662 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8663 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4411 |  8664 | `			PH7_MemObjRelease(pNew);` |
|     4411 |  8665 | `			ph7_result_bool(pCtx,1);` |
|     2206 |  8666 | `		}else{` |
|     4414 |  8667 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8668 | `			/* Install the new handler */` |
|     4414 |  8669 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8670 | `		}` |
|     4411 |  8671 | `	}` |
|     8824 |  8672 | `	return PH7_OK;` |
|        2 |  8673 |  |
|        - |  8674 | `/*` |
|        - |  8675 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  8676 | ` *  Generates a backtrace.` |
|        - |  8677 | ` * Paramaeter` |
|        - |  8678 | ` *  $options` |
|        - |  8679 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  8680 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  8681 | ` *   all the function/method arguments, to save memory.` |
|        - |  8682 | ` * $limit` |
|        - |  8683 | ` *   (Not Used)` |
|        - |  8684 | ` * Return` |
|        - |  8685 | ` *  An array.The possible returned elements are as follows:` |
|        - |  8686 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  8687 | ` *          Name        Type      Description` |
|        - |  8688 | ` *          ------      ------     -----------` |
|        - |  8689 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  8690 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  8691 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  8692 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  8693 | ` *          object      object    The current object.` |
|        - |  8694 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  8695 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  8696 | ` */` |
|      504 |  8697 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8698 |  |
|      506 |  8699 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8700 | `	ph7_value *pArray;` |
|        - |  8701 | `	ph7_class *pClass;` |
|        - |  8702 | `	ph7_value *pValue;` |
|        - |  8703 | `	SyString *pFile;` |
|        - |  8704 | `	/* Create a new array */` |
|      506 |  8705 | `	pArray = ph7_context_new_array(pCtx);` |
|      506 |  8706 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      506 |  8707 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8708 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8709 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  8710 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8711 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8712 | `		SXUNUSED(apArg);` |
|      ! 0 |  8713 | `		return PH7_OK;` |
|        - |  8714 | `	}` |
|        - |  8715 | `	/* Dump running function name and it's arguments  */` |
|      506 |  8716 | `	if( pVm->pFrame->pParent ){` |
|      506 |  8717 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8718 | `		ph7_vm_func *pFunc;` |
|        - |  8719 | `		ph7_value *pArg;` |
|      506 |  8720 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      506 |  8721 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      506 |  8722 | `		if( pFrame->pParent && pFunc ){` |
|      506 |  8723 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      506 |  8724 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      506 |  8725 | `			ph7_value_reset_string_cursor(pValue);` |
|      252 |  8726 | `		}` |
|        - |  8727 | `		/* Function arguments */` |
|      506 |  8728 | `		pArg = ph7_context_new_array(pCtx);` |
|      506 |  8729 | `		if( pArg  ){` |
|        - |  8730 | `			ph7_value *pObj;` |
|        - |  8731 | `			VmSlot *aSlot;` |
|        - |  8732 | `			sxu32 n;` |
|        - |  8733 | `			/* Start filling the array with the given arguments */` |
|      506 |  8734 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2010 |  8735 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1506 |  8736 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1506 |  8737 | `				if( pObj ){` |
|     1506 |  8738 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      752 |  8739 | `				}` |
|      754 |  8740 | `			}` |
|        - |  8741 | `			/* Save the array */` |
|      506 |  8742 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      252 |  8743 | `		}` |
|      252 |  8744 | `	}` |
|      506 |  8745 | `	ph7_value_int(pValue,1);` |
|        - |  8746 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  8747 | `	 * line numbers at run-time. )` |
|        - |  8748 | `	 */` |
|      506 |  8749 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  8750 | `	/* Current processed script */` |
|      506 |  8751 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      506 |  8752 | `	if( pFile ){` |
|      506 |  8753 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      506 |  8754 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      506 |  8755 | `		ph7_value_reset_string_cursor(pValue);` |
|      252 |  8756 | `	}` |
|        - |  8757 | `	/* Top class */` |
|      506 |  8758 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      506 |  8759 | `	if( pClass ){` |
|      502 |  8760 | `		ph7_value_reset_string_cursor(pValue);` |
|      502 |  8761 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      502 |  8762 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      250 |  8763 | `	}` |
|        - |  8764 | `	/* Return the freshly created array */` |
|      506 |  8765 | `	ph7_result_value(pCtx,pArray);` |
|        - |  8766 | `	/*` |
|        - |  8767 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  8768 | `	 * as soon we return from this function.` |
|        - |  8769 | `	 */` |
|      506 |  8770 | `	return PH7_OK;` |
|      254 |  8771 |  |
|        - |  8772 | `/*` |
|        - |  8773 | ` * Generate a small backtrace.` |
|        - |  8774 | ` * Store the generated dump in the given BLOB` |
|        - |  8775 | ` */` |
|        4 |  8776 | `static int VmMiniBacktrace(` |
|        - |  8777 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8778 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  8779 | `	)` |
|        1 |  8780 |  |
|        5 |  8781 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8782 | `	ph7_vm_func *pFunc;` |
|        - |  8783 | `	ph7_class *pClass;` |
|        - |  8784 | `	SyString *pFile;` |
|        - |  8785 | `	/* Called function */` |
|        5 |  8786 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  8787 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  8788 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8789 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  8790 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  8791 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  8792 | `	}else{` |
|      ! 0 |  8793 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  8794 | `	}` |
|        5 |  8795 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  8796 | `	/* Current processed script */` |
|        5 |  8797 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  8798 | `	if( pFile ){` |
|        5 |  8799 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8800 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  8801 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  8802 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  8803 | `	}` |
|        - |  8804 | `	/* Top class */` |
|        5 |  8805 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  8806 | `	if( pClass ){` |
|      ! 0 |  8807 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  8808 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  8809 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  8810 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  8811 | `	}` |
|        5 |  8812 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  8813 | `	/* All done */` |
|        5 |  8814 | `	return SXRET_OK;` |
|        1 |  8815 |  |
|        - |  8816 | `/*` |
|        - |  8817 | ` * void debug_print_backtrace()` |
|        - |  8818 | ` *  Prints a backtrace` |
|        - |  8819 | ` * Parameters` |
|        - |  8820 | ` * None` |
|        - |  8821 | ` * Return` |
|        - |  8822 | ` * NULL` |
|        - |  8823 | ` */` |
|        2 |  8824 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8825 |  |
|        3 |  8826 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8827 | `	SyBlob sDump;` |
|        3 |  8828 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8829 | `	/* Generate the backtrace */` |
|        3 |  8830 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8831 | `	/* Output backtrace */` |
|        3 |  8832 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8833 | `	/* All done,cleanup */` |
|        3 |  8834 | `	SyBlobRelease(&sDump);` |
|        1 |  8835 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8836 | `	SXUNUSED(apArg);` |
|        3 |  8837 | `	return PH7_OK;` |
|        1 |  8838 |  |
|        - |  8839 | `/*` |
|        - |  8840 | ` * string debug_string_backtrace()` |
|        - |  8841 | ` *  Generate a backtrace` |
|        - |  8842 | ` * Parameters` |
|        - |  8843 | ` * None` |
|        - |  8844 | ` * Return` |
|        - |  8845 | ` *  A mini backtrace().` |
|        - |  8846 | ` * Note that this is a symisc extension.` |
|        - |  8847 | ` */` |
|        2 |  8848 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8849 |  |
|        3 |  8850 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8851 | `	SyBlob sDump;` |
|        3 |  8852 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8853 | `	/* Generate the backtrace */` |
|        3 |  8854 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8855 | `	/* Return the backtrace */` |
|        3 |  8856 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  8857 | `	/* All done,cleanup */` |
|        3 |  8858 | `	SyBlobRelease(&sDump);` |
|        1 |  8859 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8860 | `	SXUNUSED(apArg);` |
|        3 |  8861 | `	return PH7_OK;` |
|        1 |  8862 |  |
|        - |  8863 | `/*` |
|        - |  8864 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  8865 | ` * exception is triggered.` |
|        - |  8866 | ` */` |
|      472 |  8867 | `static sxi32 VmUncaughtException(` |
|        - |  8868 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8869 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8870 | `	)` |
|        1 |  8871 |  |
|        - |  8872 | `	ph7_value *apArg[2],sArg;` |
|      473 |  8873 | `	int nArg = 1;` |
|        - |  8874 | `	sxi32 rc;` |
|      473 |  8875 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  8876 | `		/* Nesting limit reached */` |
|      ! 0 |  8877 | `		return SXRET_OK;` |
|        - |  8878 | `	}` |
|        - |  8879 | `	/* Call any exception handler if available */` |
|      473 |  8880 | `	PH7_MemObjInit(pVm,&sArg);` |
|      473 |  8881 | `	if( pThis ){` |
|        - |  8882 | `		/* Load the exception instance */` |
|      473 |  8883 | `		sArg.x.pOther = pThis;` |
|      473 |  8884 | `		pThis->iRef++;` |
|      473 |  8885 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      237 |  8886 | `	}else{` |
|      ! 0 |  8887 | `		nArg = 0;` |
|        - |  8888 | `	}` |
|      473 |  8889 | `	apArg[0] = &sArg;` |
|        - |  8890 | `	/* Call the exception handler if available */` |
|      473 |  8891 | `	pVm->nExceptDepth++;` |
|      473 |  8892 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      473 |  8893 | `	pVm->nExceptDepth--;` |
|      473 |  8894 | `	if( rc != SXRET_OK ){` |
|        - |  8895 | `		SyBlob sMsgBuf;` |
|      471 |  8896 | `		const char *zClass = "Exception";` |
|      471 |  8897 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  8898 | `		const char *zMsg;` |
|        - |  8899 | `		sxu32 nMsg;` |
|        - |  8900 | `		const char *zFuncName;` |
|        - |  8901 | `		int nFuncLen;` |
|      471 |  8902 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      471 |  8903 | `		if( pThis ){` |
|        - |  8904 | `			ph7_class_method *pGetMessage;` |
|        - |  8905 | `			ph7_value sMsg;` |
|        - |  8906 | `			const char *zTmp;` |
|        - |  8907 | `			int nTmp;` |
|      471 |  8908 | `			zClass = pThis->pClass->sName.zString;` |
|      471 |  8909 | `			nClass = pThis->pClass->sName.nByte;` |
|      471 |  8910 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      471 |  8911 | `			if( pGetMessage ){` |
|      471 |  8912 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      471 |  8913 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      471 |  8914 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      471 |  8915 | `					if( zTmp && nTmp > 0 ){` |
|      471 |  8916 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      235 |  8917 | `					}` |
|      235 |  8918 | `				}` |
|      471 |  8919 | `				PH7_MemObjRelease(&sMsg);` |
|      235 |  8920 | `			}` |
|      235 |  8921 | `		}` |
|      471 |  8922 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  8923 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  8924 | `		}` |
|      471 |  8925 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      471 |  8926 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      471 |  8927 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      471 |  8928 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      471 |  8929 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  8930 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      471 |  8931 | `		rc = SXERR_ABORT;` |
|      235 |  8932 | `	}` |
|      473 |  8933 | `	PH7_MemObjRelease(&sArg);` |
|      473 |  8934 | `	return rc;` |
|      237 |  8935 |  |
|        - |  8936 | `/*` |
|        - |  8937 | ` * Throw a user exception.` |
|        - |  8938 | ` *` |
|        - |  8939 | ` * Exception dispatch follows this sequence:` |
|        - |  8940 | ` *` |
|        - |  8941 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - |  8942 | ` *    try/catch whose catch block matches the exception class.` |
|        - |  8943 | ` *` |
|        - |  8944 | ` * 2. If NO catch matches:` |
|        - |  8945 | ` *    a. Run finally (if present) for the current try block.` |
|        - |  8946 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - |  8947 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - |  8948 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - |  8949 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - |  8950 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - |  8951 | ` *    d. Otherwise, report as truly uncaught.` |
|        - |  8952 | ` *` |
|        - |  8953 | ` * 3. If a catch DOES match:` |
|        - |  8954 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - |  8955 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - |  8956 | ` *       inside the catch body from immediately propagating past our` |
|        - |  8957 | ` *       finally block.` |
|        - |  8958 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - |  8959 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - |  8960 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - |  8961 | ` *       in pPendingException (step 2c).` |
|        - |  8962 | ` *    c. Restore outer handlers from the saved copy.` |
|        - |  8963 | ` *    d. Run finally (if present).` |
|        - |  8964 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - |  8965 | ` *       that handlers are restored and finally has run.` |
|        - |  8966 | ` */` |
|      508 |  8967 | `static sxi32 VmThrowException(` |
|        - |  8968 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  8969 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8970 | `	)` |
|        2 |  8971 |  |
|        - |  8972 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  8973 | `	ph7_exception **apException;` |
|        - |  8974 | `	ph7_exception *pException;` |
|        - |  8975 | `	/* Point to the stack of loaded exceptions */` |
|      510 |  8976 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      510 |  8977 | `	pException = 0;` |
|      510 |  8978 | `	pCatch = 0;` |
|      510 |  8979 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8980 | `		ph7_exception_block *aCatch;` |
|        - |  8981 | `		ph7_class *pClass;` |
|        - |  8982 | `		sxu32 j;` |
|        - |  8983 | `		/* Locate the appropriate block to execute */` |
|       34 |  8984 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       34 |  8985 | `		(void)SySetPop(&pVm->aException);` |
|       34 |  8986 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       34 |  8987 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       32 |  8988 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  8989 | `			/* Extract the target class */` |
|       32 |  8990 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       32 |  8991 | `			if( pClass == 0 ){` |
|        - |  8992 | `				/* No such class */` |
|      ! 0 |  8993 | `				continue;` |
|        - |  8994 | `			}` |
|       32 |  8995 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  8996 | `				/* Catch block found,break immeditaley */` |
|       32 |  8997 | `				pCatch = &aCatch[j];` |
|       32 |  8998 | `				break;` |
|        - |  8999 | `			}` |
|      ! 0 |  9000 | `		}` |
|       16 |  9001 | `	}` |
|        - |  9002 | `	/* Execute the cached block if available */` |
|      510 |  9003 | `	if( pCatch == 0 ){` |
|        - |  9004 | `		sxi32 rc;` |
|        - |  9005 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      480 |  9006 | `		if( pException && pException->iHasFinally ){` |
|        3 |  9007 | `			pException->iFinallyDone = 1;` |
|        3 |  9008 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 |  9009 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9010 | `				return SXERR_ABORT;` |
|        - |  9011 | `			}` |
|        1 |  9012 | `		}` |
|        - |  9013 | `		/* Check if there is an outer exception handler on the stack */` |
|      480 |  9014 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  9015 | `			/* Re-throw to the outer handler */` |
|        3 |  9016 | `			return VmThrowException(&(*pVm),pThis);` |
|        - |  9017 | `		}` |
|        - |  9018 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - |  9019 | `		 * (catch body re-throw with finally pending), defer the` |
|        - |  9020 | `		 * exception instead of reporting it uncaught.` |
|        - |  9021 | `		 */` |
|      478 |  9022 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - |  9023 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - |  9024 | `			 * by looking for a catch frame on the stack.` |
|        - |  9025 | `			 */` |
|      478 |  9026 | `			VmFrame *pF = pVm->pFrame;` |
|      478 |  9027 | `			int inCatch = 0;` |
|      956 |  9028 | `			while( pF ){` |
|      484 |  9029 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        6 |  9030 | `					inCatch = 1;` |
|        6 |  9031 | `					break;` |
|        - |  9032 | `				}` |
|      479 |  9033 | `				pF = pF->pParent;` |
|        1 |  9034 | `			}` |
|      478 |  9035 | `			if( inCatch ){` |
|        - |  9036 | `				/* Defer — will be re-thrown after finally runs */` |
|        6 |  9037 | `				pThis->iRef++;` |
|        6 |  9038 | `				pVm->pPendingException = pThis;` |
|        6 |  9039 | `				return SXRET_OK;` |
|        - |  9040 | `			}` |
|      236 |  9041 | `		}` |
|        - |  9042 | `		/* Truly uncaught */` |
|      473 |  9043 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      473 |  9044 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  9045 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  9046 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 |  9047 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 |  9048 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  9049 | `			}` |
|      ! 0 |  9050 | `		}` |
|      473 |  9051 | `		return rc;` |
|      ! 0 |  9052 | `	}else{` |
|       32 |  9053 | `		VmFrame *pFrame = pVm->pFrame;` |
|       32 |  9054 | `		ph7_exception **apSaved = 0;` |
|        - |  9055 | `		sxu32 nSavedCount;` |
|        - |  9056 | `		sxi32 rc;` |
|       32 |  9057 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       32 |  9058 | `		if( pException->pFrame == pFrame ){` |
|       24 |  9059 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       11 |  9060 | `		}` |
|        - |  9061 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - |  9062 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - |  9063 | `		 * our finally block. We save the stack contents and restore after.` |
|        - |  9064 | `		 */` |
|       32 |  9065 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       32 |  9066 | `		if( nSavedCount > 0 ){` |
|       11 |  9067 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        3 |  9068 | `				nSavedCount * sizeof(ph7_exception *));` |
|        8 |  9069 | `			if( apSaved ){` |
|       11 |  9070 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        3 |  9071 | `					nSavedCount * sizeof(ph7_exception *));` |
|        8 |  9072 | `				SySetReset(&pVm->aException);` |
|        3 |  9073 | `			}` |
|        3 |  9074 | `		}` |
|        - |  9075 | `		/* Create a private frame first */` |
|       32 |  9076 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       32 |  9077 | `		if( rc == SXRET_OK ){` |
|       32 |  9078 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       32 |  9079 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       32 |  9080 | `			if( pObj ){` |
|       32 |  9081 | `				pThis->iRef++;` |
|       32 |  9082 | `				pObj->x.pOther = pThis;` |
|       32 |  9083 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       15 |  9084 | `			}` |
|        - |  9085 | `			/* Execute the catch block */` |
|       32 |  9086 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  9087 | `			/* Leave the frame */` |
|       32 |  9088 | `			VmLeaveFrame(&(*pVm));` |
|       15 |  9089 | `		}` |
|        - |  9090 | `		/* Restore the outer exception handlers */` |
|       32 |  9091 | `		if( apSaved ){` |
|        - |  9092 | `			sxu32 k;` |
|        - |  9093 | `			/* Any new entries pushed during catch execution (from nested` |
|        - |  9094 | `			 * try blocks inside the catch body) are already consumed.` |
|        - |  9095 | `			 * Restore the original outer entries.` |
|        - |  9096 | `			 */` |
|        8 |  9097 | `			SySetReset(&pVm->aException);` |
|       14 |  9098 | `			for(k = 0; k < nSavedCount; k++){` |
|        8 |  9099 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 |  9100 | `			}` |
|        8 |  9101 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        3 |  9102 | `		}` |
|        - |  9103 | `		/* Execute the finally block after catch */` |
|       32 |  9104 | `		if( pException->iHasFinally ){` |
|       11 |  9105 | `			pException->iFinallyDone = 1;` |
|        - |  9106 | `			{` |
|       11 |  9107 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       11 |  9108 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 |  9109 | `					return SXERR_ABORT;` |
|        - |  9110 | `				}` |
|        - |  9111 | `			}` |
|        5 |  9112 | `		}` |
|       32 |  9113 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9114 | `			return SXERR_ABORT;` |
|        - |  9115 | `		}` |
|        - |  9116 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - |  9117 | `		 * pPendingException (because outer handlers were hidden).` |
|        - |  9118 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - |  9119 | `		 */` |
|       32 |  9120 | `		if( pVm->pPendingException ){` |
|        6 |  9121 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        6 |  9122 | `			pVm->pPendingException = 0;` |
|        6 |  9123 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - |  9124 | `		}` |
|        - |  9125 | `	}` |
|        - |  9126 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  9127 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  9128 | `	 */` |
|       28 |  9129 | `	return SXRET_OK;` |
|      256 |  9130 |  |
|        - |  9131 | `/*` |
|        - |  9132 | ` * Section:` |
|        - |  9133 | ` *  Version,Credits and Copyright related functions.` |
|        - |  9134 | ` * Status:` |
|        - |  9135 | ` *    Stable.` |
|        - |  9136 | ` */` |
|        - |  9137 | `/*` |
|        - |  9138 | ` * string ph7version(void)` |
|        - |  9139 | ` *  Returns the running version of the PH7 version.` |
|        - |  9140 | ` * Parameters` |
|        - |  9141 | ` *  None` |
|        - |  9142 | ` * Return` |
|        - |  9143 | ` * Current PH7 version.` |
|        - |  9144 | ` */` |
|        2 |  9145 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9146 |  |
|        1 |  9147 | `	SXUNUSED(nArg);` |
|        1 |  9148 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  9149 | `	/* Current engine version */` |
|        3 |  9150 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  9151 | `	return PH7_OK;` |
|        1 |  9152 |  |
|        - |  9153 | `/*` |
|        - |  9154 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  9155 | ` */` |
|        - |  9156 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  9157 | ` "<html><head>"\` |
|        - |  9158 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  9159 | ` "<style type=\"text/css\">"\` |
|        - |  9160 | ` "div {"\` |
|        - |  9161 | `     "border: 1px solid #cccccc;"\` |
|        - |  9162 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  9163 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  9164 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  9165 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  9166 | `     "-webkit-border-radius: 10px;"\` |
|        - |  9167 | `     "-o-border-radius: 10px;"\` |
|        - |  9168 | `     "border-radius: 10px;"\` |
|        - |  9169 | `     "padding-left: 2em;"\` |
|        - |  9170 | `     "background-color: white;"\` |
|        - |  9171 | `     "margin-left: auto;"\` |
|        - |  9172 | `     "font-family: verdana;"\` |
|        - |  9173 | `     "padding-right: 2em;"\` |
|        - |  9174 | `     "margin-right: auto;"\` |
|        - |  9175 | `     "}"\` |
|        - |  9176 | `     "body {"\` |
|        - |  9177 | `     "padding: 0.2em;"\` |
|        - |  9178 | `     "font-style: normal;"\` |
|        - |  9179 | `     "font-size: medium;"\` |
|        - |  9180 | `     "background-color: #f2f2f2;"\` |
|        - |  9181 | `     "}"\` |
|        - |  9182 | `     "hr {"\` |
|        - |  9183 | `     "border-style: solid none none;"\` |
|        - |  9184 | `     "border-width: 1px medium medium;"\` |
|        - |  9185 | `     "border-top: 1px solid #cccccc;"\` |
|        - |  9186 | `     "height: 1px;"\` |
|        - |  9187 | `     "}"\` |
|        - |  9188 | `     "a {"\` |
|        - |  9189 | `     "color: #3366cc;"\` |
|        - |  9190 | `     "text-decoration: none;"\` |
|        - |  9191 | `     "}"\` |
|        - |  9192 | `     "a:hover {"\` |
|        - |  9193 | `     "color: #999999;"\` |
|        - |  9194 | `     "}"\` |
|        - |  9195 | `     "a:active {"\` |
|        - |  9196 | `     "color: #663399;"\` |
|        - |  9197 | `     "}"\` |
|        - |  9198 | `     "h1 {"\` |
|        - |  9199 | `     "margin: 0;"\` |
|        - |  9200 | `     "padding: 0;"\` |
|        - |  9201 | `     "font-family: Verdana;"\` |
|        - |  9202 | `     "font-weight: bold;"\` |
|        - |  9203 | `     "font-style: normal;"\` |
|        - |  9204 | `     "font-size: medium;"\` |
|        - |  9205 | `     "text-transform: capitalize;"\` |
|        - |  9206 | `     "color: #0a328c;"\` |
|        - |  9207 | `     "}"\` |
|        - |  9208 | `     "p {"\` |
|        - |  9209 | `     "margin: 0 auto;"\` |
|        - |  9210 | `     "font-size: medium;"\` |
|        - |  9211 | `     "font-style: normal;"\` |
|        - |  9212 | `     "font-family: verdana;"\` |
|        - |  9213 | `     "}"\` |
|        - |  9214 | `"</style></head><body>"\` |
|        - |  9215 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - |  9216 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - |  9217 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - |  9218 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - |  9219 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - |  9220 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - |  9221 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - |  9222 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - |  9223 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - |  9224 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - |  9225 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - |  9226 |  |
|        - |  9227 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9228 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - |  9229 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - |  9230 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - |  9231 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9232 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - |  9233 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  9234 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - |  9235 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  9236 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - |  9237 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9238 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - |  9239 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - |  9240 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - |  9241 |  |
|        - |  9242 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - |  9243 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - |  9244 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - |  9245 | `"&nbsp;*<br>"\` |
|        - |  9246 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - |  9247 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - |  9248 | `"&nbsp;* are met:<br>"\` |
|        - |  9249 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - |  9250 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - |  9251 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - |  9252 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - |  9253 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - |  9254 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - |  9255 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - |  9256 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - |  9257 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - |  9258 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - |  9259 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - |  9260 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - |  9261 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - |  9262 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - |  9263 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - |  9264 | `"&nbsp;*<br>"\` |
|        - |  9265 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - |  9266 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - |  9267 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - |  9268 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - |  9269 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - |  9270 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - |  9271 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - |  9272 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - |  9273 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - |  9274 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - |  9275 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - |  9276 | `"&nbsp;*/<br>"\` |
|        - |  9277 | `"</span></small></small></p>"\` |
|        - |  9278 | `"</div></body></html>"` |
|        - |  9279 | `/*` |
|        - |  9280 | ` * bool ph7credits(void)` |
|        - |  9281 | ` * bool ph7info(void)` |
|        - |  9282 | ` * bool ph7copyright(void)` |
|        - |  9283 | ` *  Prints out the credits for PH7 engine` |
|        - |  9284 | ` * Parameters` |
|        - |  9285 | ` *  None` |
|        - |  9286 | ` * Return` |
|        - |  9287 | ` *  Always TRUE` |
|        - |  9288 | ` */` |
|        2 |  9289 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9290 |  |
|        3 |  9291 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - |  9292 | `	/* Expand the HTML page above*/` |
|        3 |  9293 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 |  9294 | `	ph7_context_output_format(` |
|        1 |  9295 | `		pCtx,` |
|        - |  9296 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 |  9297 | `		ph7_lib_version(),   /* Engine version */` |
|        1 |  9298 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 |  9299 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 |  9300 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 |  9301 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 |  9302 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - |  9303 | `#ifdef __WINNT__` |
|        - |  9304 | `		"Windows NT"` |
|        - |  9305 | `#elif defined(__UNIXES__)` |
|        - |  9306 | `		"UNIX-Like"` |
|        - |  9307 | `#else` |
|        - |  9308 | `		"Other OS"` |
|        - |  9309 | `#endif` |
|        - |  9310 | `		);` |
|        3 |  9311 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 |  9312 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9313 | `	SXUNUSED(apArg);` |
|        - |  9314 | `	/* Return TRUE */` |
|        - |  9315 | `	//ph7_result_bool(pCtx,1);` |
|        3 |  9316 | `	return PH7_OK;` |
|        1 |  9317 |  |
|        - |  9318 | `/*` |
|        - |  9319 | ` * Section:` |
|        - |  9320 | ` *    URL related routines.` |
|        - |  9321 | ` * Status:` |
|        - |  9322 | ` *    Stable.` |
|        - |  9323 | ` */` |
|        - |  9324 | `/*` |
|        - |  9325 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - |  9326 | ` *  Parse a URL and return its fields.` |
|        - |  9327 | ` * Parameters` |
|        - |  9328 | ` *  $url` |
|        - |  9329 | ` *   The URL to parse.` |
|        - |  9330 | ` * $component` |
|        - |  9331 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - |  9332 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - |  9333 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - |  9334 | ` *  in which case the return value will be an integer).` |
|        - |  9335 | ` * Return` |
|        - |  9336 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - |  9337 | ` *  At least one element will be present within the array. Potential keys within` |
|        - |  9338 | ` *  this array are:` |
|        - |  9339 | ` *   scheme - e.g. http` |
|        - |  9340 | ` *   host` |
|        - |  9341 | ` *   port` |
|        - |  9342 | ` *   user` |
|        - |  9343 | ` *   pass` |
|        - |  9344 | ` *   path` |
|        - |  9345 | ` *   query - after the question mark ?` |
|        - |  9346 | ` *   fragment - after the hashmark #` |
|        - |  9347 | ` * Note:` |
|        - |  9348 | ` *  FALSE is returned on failure.` |
|        - |  9349 | ` *  This function work with relative URL unlike the one shipped` |
|        - |  9350 | ` *  with the standard PHP engine.` |
|        - |  9351 | ` */` |
|       28 |  9352 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9353 |  |
|        - |  9354 | `	const char *zStr; /* Input string */` |
|        - |  9355 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - |  9356 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - |  9357 | `	int nLen;` |
|        - |  9358 | `	sxi32 rc;` |
|       29 |  9359 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  9360 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  9361 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9362 | `		return PH7_OK;` |
|        - |  9363 | `	}` |
|        - |  9364 | `	/* Extract the given URI */` |
|       29 |  9365 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 |  9366 | `	if( nLen < 1 ){` |
|        - |  9367 | `		/* Nothing to process,return FALSE */` |
|        3 |  9368 | `		ph7_result_bool(pCtx,0);` |
|        3 |  9369 | `		return PH7_OK;` |
|        - |  9370 | `	}` |
|        - |  9371 | `	/* Get a parse */` |
|       27 |  9372 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 |  9373 | `	if( rc != SXRET_OK ){` |
|        - |  9374 | `		/* Malformed input,return FALSE */` |
|      ! 0 |  9375 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9376 | `		return PH7_OK;` |
|        - |  9377 | `	}` |
|       27 |  9378 | `	if( nArg > 1 ){` |
|      ! 0 |  9379 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - |  9380 | `		/* Refer to constant.c for constants values */` |
|      ! 0 |  9381 | `		switch(nComponent){` |
|      ! 0 |  9382 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 |  9383 | `			pComp = &sURI.sScheme;` |
|      ! 0 |  9384 | `			if( pComp->nByte < 1 ){` |
|        - |  9385 | `				/* No available value,return NULL */` |
|      ! 0 |  9386 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9387 | `			}else{` |
|      ! 0 |  9388 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9389 | `			}` |
|      ! 0 |  9390 | `			break;` |
|      ! 0 |  9391 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 |  9392 | `			pComp = &sURI.sHost;` |
|      ! 0 |  9393 | `			if( pComp->nByte < 1 ){` |
|        - |  9394 | `				/* No available value,return NULL */` |
|      ! 0 |  9395 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9396 | `			}else{` |
|      ! 0 |  9397 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9398 | `			}` |
|      ! 0 |  9399 | `			break;` |
|      ! 0 |  9400 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 |  9401 | `			pComp = &sURI.sPort;` |
|      ! 0 |  9402 | `			if( pComp->nByte < 1 ){` |
|        - |  9403 | `				/* No available value,return NULL */` |
|      ! 0 |  9404 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9405 | `			}else{` |
|      ! 0 |  9406 | `				int iPort = 0;` |
|        - |  9407 | `				/* Cast the value to integer */` |
|      ! 0 |  9408 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 |  9409 | `				ph7_result_int(pCtx,iPort);` |
|        - |  9410 | `			}` |
|      ! 0 |  9411 | `			break;` |
|      ! 0 |  9412 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 |  9413 | `			pComp = &sURI.sUser;` |
|      ! 0 |  9414 | `			if( pComp->nByte < 1 ){` |
|        - |  9415 | `				/* No available value,return NULL */` |
|      ! 0 |  9416 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9417 | `			}else{` |
|      ! 0 |  9418 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9419 | `			}` |
|      ! 0 |  9420 | `			break;` |
|      ! 0 |  9421 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 |  9422 | `			pComp = &sURI.sPass;` |
|      ! 0 |  9423 | `			if( pComp->nByte < 1 ){` |
|        - |  9424 | `				/* No available value,return NULL */` |
|      ! 0 |  9425 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9426 | `			}else{` |
|      ! 0 |  9427 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9428 | `			}` |
|      ! 0 |  9429 | `			break;` |
|      ! 0 |  9430 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 |  9431 | `			pComp = &sURI.sQuery;` |
|      ! 0 |  9432 | `			if( pComp->nByte < 1 ){` |
|        - |  9433 | `				/* No available value,return NULL */` |
|      ! 0 |  9434 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9435 | `			}else{` |
|      ! 0 |  9436 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9437 | `			}` |
|      ! 0 |  9438 | `			break;` |
|      ! 0 |  9439 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 |  9440 | `			pComp = &sURI.sFragment;` |
|      ! 0 |  9441 | `			if( pComp->nByte < 1 ){` |
|        - |  9442 | `				/* No available value,return NULL */` |
|      ! 0 |  9443 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9444 | `			}else{` |
|      ! 0 |  9445 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9446 | `			}` |
|      ! 0 |  9447 | `			break;` |
|      ! 0 |  9448 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 |  9449 | `			pComp = &sURI.sPath;` |
|      ! 0 |  9450 | `			if( pComp->nByte < 1 ){` |
|        - |  9451 | `				/* No available value,return NULL */` |
|      ! 0 |  9452 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9453 | `			}else{` |
|      ! 0 |  9454 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9455 | `			}` |
|      ! 0 |  9456 | `			break;` |
|      ! 0 |  9457 | `		default:` |
|        - |  9458 | `			/* No such entry,return NULL */` |
|      ! 0 |  9459 | `			ph7_result_null(pCtx);` |
|      ! 0 |  9460 | `			break;` |
|        - |  9461 | `		}` |
|      ! 0 |  9462 | `	}else{` |
|        - |  9463 | `		ph7_value *pArray,*pValue;` |
|        - |  9464 | `		/* Return an associative array */` |
|       27 |  9465 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 |  9466 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 |  9467 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9468 | `			/* Out of memory */` |
|      ! 0 |  9469 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9470 | `			/* Return false */` |
|      ! 0 |  9471 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |  9472 | `			return PH7_OK;` |
|        - |  9473 | `		}` |
|        - |  9474 | `		/* Fill the array */` |
|       27 |  9475 | `		pComp = &sURI.sScheme;` |
|       27 |  9476 | `		if( pComp->nByte > 0 ){` |
|       19 |  9477 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 |  9478 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 |  9479 | `		}` |
|        - |  9480 | `		/* Reset the string cursor */` |
|       27 |  9481 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9482 | `		pComp = &sURI.sHost;` |
|       27 |  9483 | `		if( pComp->nByte > 0 ){` |
|       25 |  9484 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 |  9485 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 |  9486 | `		}` |
|        - |  9487 | `		/* Reset the string cursor */` |
|       27 |  9488 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9489 | `		pComp = &sURI.sPort;` |
|       27 |  9490 | `		if( pComp->nByte > 0 ){` |
|       11 |  9491 | `			int iPort = 0;/* cc warning */` |
|        - |  9492 | `			/* Convert to integer */` |
|       11 |  9493 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 |  9494 | `			ph7_value_int(pValue,iPort);` |
|       11 |  9495 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 |  9496 | `		}` |
|        - |  9497 | `		/* Reset the string cursor */` |
|       27 |  9498 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9499 | `		pComp = &sURI.sUser;` |
|       27 |  9500 | `		if( pComp->nByte > 0 ){` |
|        7 |  9501 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9502 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 |  9503 | `		}` |
|        - |  9504 | `		/* Reset the string cursor */` |
|       27 |  9505 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9506 | `		pComp = &sURI.sPass;` |
|       27 |  9507 | `		if( pComp->nByte > 0 ){` |
|        7 |  9508 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9509 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 |  9510 | `		}` |
|        - |  9511 | `		/* Reset the string cursor */` |
|       27 |  9512 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9513 | `		pComp = &sURI.sPath;` |
|       27 |  9514 | `		if( pComp->nByte > 0 ){` |
|       17 |  9515 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 |  9516 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 |  9517 | `		}` |
|        - |  9518 | `		/* Reset the string cursor */` |
|       27 |  9519 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9520 | `		pComp = &sURI.sQuery;` |
|       27 |  9521 | `		if( pComp->nByte > 0 ){` |
|        5 |  9522 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9523 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 |  9524 | `		}` |
|        - |  9525 | `		/* Reset the string cursor */` |
|       27 |  9526 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9527 | `		pComp = &sURI.sFragment;` |
|       27 |  9528 | `		if( pComp->nByte > 0 ){` |
|        5 |  9529 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9530 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 |  9531 | `		}` |
|        - |  9532 | `		/* Return the created array */` |
|       27 |  9533 | `		ph7_result_value(pCtx,pArray);` |
|        - |  9534 | `		/* NOTE:` |
|        - |  9535 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - |  9536 | `		 * automatically as soon we return from this function.` |
|        - |  9537 | `		 */` |
|        - |  9538 | `	}` |
|        - |  9539 | `	/* All done */` |
|       27 |  9540 | `	return PH7_OK;` |
|       15 |  9541 |  |
|        - |  9542 | `/*` |
|        - |  9543 | ` * Section:` |
|        - |  9544 | ` *   Array related routines.` |
|        - |  9545 | ` * Status:` |
|        - |  9546 | ` *    Stable.` |
|        - |  9547 | ` * Note 2012-5-21 01:04:15:` |
|        - |  9548 | ` *  Array related functions that need access to the underlying` |
|        - |  9549 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - |  9550 | ` */` |
|        - |  9551 | `/*` |
|        - |  9552 | ` * The [compact()] function store it's state information in an instance` |
|        - |  9553 | ` * of the following structure.` |
|        - |  9554 | ` */` |
|        - |  9555 | `struct compact_data` |
|        - |  9556 |  |
|        - |  9557 | `	ph7_value *pArray;  /* Target array */` |
|        - |  9558 | `	int nRecCount;      /* Recursion count */` |
|        - |  9559 | `};` |
|        - |  9560 | `/*` |
|        - |  9561 | ` * Walker callback for the [compact()] function defined below.` |
|        - |  9562 | ` */` |
|      ! 0 |  9563 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 |  9564 |  |
|      ! 0 |  9565 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 |  9566 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 |  9567 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9568 | `	/* Act according to the hashmap value */` |
|      ! 0 |  9569 | `	if( ph7_value_is_string(pValue) ){` |
|        - |  9570 | `		SyString sVar;` |
|      ! 0 |  9571 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 |  9572 | `		if( sVar.nByte > 0 ){` |
|        - |  9573 | `			/* Query the current frame */` |
|      ! 0 |  9574 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - |  9575 | `			/* ^` |
|        - |  9576 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - |  9577 | `			 */` |
|      ! 0 |  9578 | `			if( pKey ){` |
|        - |  9579 | `				/* Perform the insertion */` |
|      ! 0 |  9580 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 |  9581 | `			}` |
|      ! 0 |  9582 | `		}` |
|      ! 0 |  9583 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - |  9584 | `		int rc;` |
|        - |  9585 | `		/* Recursively traverse this array */` |
|      ! 0 |  9586 | `		pData->nRecCount++;` |
|      ! 0 |  9587 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 |  9588 | `		pData->nRecCount--;` |
|      ! 0 |  9589 | `		return rc;` |
|        - |  9590 | `	}` |
|      ! 0 |  9591 | `	return SXRET_OK;` |
|      ! 0 |  9592 |  |
|        - |  9593 | `/*` |
|        - |  9594 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - |  9595 | ` *  Create array containing variables and their values.` |
|        - |  9596 | ` *  For each of these, compact() looks for a variable with that name` |
|        - |  9597 | ` *  in the current symbol table and adds it to the output array such` |
|        - |  9598 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - |  9599 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - |  9600 | ` *  Any strings that are not set will simply be skipped.` |
|        - |  9601 | ` * Parameters` |
|        - |  9602 | ` *  $varname` |
|        - |  9603 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - |  9604 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - |  9605 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - |  9606 | ` *   it recursively.` |
|        - |  9607 | ` * Return` |
|        - |  9608 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - |  9609 | ` */` |
|        2 |  9610 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9611 |  |
|        - |  9612 | `	ph7_value *pArray,*pObj;` |
|        3 |  9613 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9614 | `	const char *zName;` |
|        - |  9615 | `	SyString sVar;` |
|        - |  9616 | `	int i,nLen;` |
|        3 |  9617 | `	if( nArg < 1 ){` |
|        - |  9618 | `		/* Missing arguments,return NULL */` |
|      ! 0 |  9619 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9620 | `		return PH7_OK;` |
|        - |  9621 | `	}` |
|        - |  9622 | `	/* Create the array */` |
|        3 |  9623 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9624 | `	if( pArray == 0 ){` |
|        - |  9625 | `		/* Out of memory */` |
|      ! 0 |  9626 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9627 | `		/* Return NULL */` |
|      ! 0 |  9628 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9629 | `		return PH7_OK;` |
|        - |  9630 | `	}` |
|        - |  9631 | `	/* Perform the requested operation */` |
|        7 |  9632 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 |  9633 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 |  9634 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - |  9635 | `				struct compact_data sData;` |
|      ! 0 |  9636 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - |  9637 | `				/* Recursively walk the array */` |
|      ! 0 |  9638 | `				sData.nRecCount = 0;` |
|      ! 0 |  9639 | `				sData.pArray = pArray;` |
|      ! 0 |  9640 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 |  9641 | `			}` |
|      ! 0 |  9642 | `		}else{` |
|        - |  9643 | `			/* Extract variable name */` |
|        5 |  9644 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 |  9645 | `			if( nLen > 0 ){` |
|        5 |  9646 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - |  9647 | `				/* Check if the variable is available in the current frame */` |
|        5 |  9648 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 |  9649 | `				if( pObj ){` |
|        5 |  9650 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 |  9651 | `				}` |
|        2 |  9652 | `			}` |
|        - |  9653 | `		}` |
|        3 |  9654 | `	}` |
|        - |  9655 | `	/* Return the array */` |
|        3 |  9656 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9657 | `	return PH7_OK;` |
|        2 |  9658 |  |
|        - |  9659 | `/*` |
|        - |  9660 | ` * The [extract()] function store it's state information in an instance` |
|        - |  9661 | ` * of the following structure.` |
|        - |  9662 | ` */` |
|        - |  9663 | `typedef struct extract_aux_data extract_aux_data;` |
|        - |  9664 | `struct extract_aux_data` |
|        - |  9665 |  |
|        - |  9666 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - |  9667 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - |  9668 | `	const char *zPrefix;  /* Prefix name */` |
|        - |  9669 | `	int Prefixlen;        /* Prefix  length */` |
|        - |  9670 | `	int iFlags;           /* Control flags */` |
|        - |  9671 | `	char zWorker[1024];   /* Working buffer */` |
|        - |  9672 | `};` |
|        - |  9673 | `/* Forward declaration */` |
|        - |  9674 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - |  9675 | `/*` |
|        - |  9676 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - |  9677 | ` *   Import variables into the current symbol table from an array.` |
|        - |  9678 | ` * Parameters` |
|        - |  9679 | ` * $var_array` |
|        - |  9680 | ` *  An associative array. This function treats keys as variable names and values` |
|        - |  9681 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - |  9682 | ` *  table, subject to extract_type and prefix parameters.` |
|        - |  9683 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - |  9684 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - |  9685 | ` * $extract_type` |
|        - |  9686 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - |  9687 | ` *  It can be one of the following values:` |
|        - |  9688 | ` *   EXTR_OVERWRITE` |
|        - |  9689 | ` *       If there is a collision, overwrite the existing variable.` |
|        - |  9690 | ` *   EXTR_SKIP` |
|        - |  9691 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - |  9692 | ` *   EXTR_PREFIX_SAME` |
|        - |  9693 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - |  9694 | ` *   EXTR_PREFIX_ALL` |
|        - |  9695 | ` *       Prefix all variable names with prefix.` |
|        - |  9696 | ` *   EXTR_PREFIX_INVALID` |
|        - |  9697 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - |  9698 | ` *   EXTR_IF_EXISTS` |
|        - |  9699 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - |  9700 | ` *       otherwise do nothing.` |
|        - |  9701 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - |  9702 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - |  9703 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - |  9704 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - |  9705 | ` *      the current symbol table.` |
|        - |  9706 | ` * $prefix` |
|        - |  9707 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - |  9708 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - |  9709 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - |  9710 | ` *  underscore character.` |
|        - |  9711 | ` * Return` |
|        - |  9712 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - |  9713 | ` */` |
|        4 |  9714 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9715 |  |
|        - |  9716 | `	extract_aux_data sAux;` |
|        - |  9717 | `	ph7_hashmap *pMap;` |
|        5 |  9718 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - |  9719 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 |  9720 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9721 | `		return PH7_OK;` |
|        - |  9722 | `	}` |
|        - |  9723 | `	/* Point to the target hashmap */` |
|        5 |  9724 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 |  9725 | `	if( pMap->nEntry < 1 ){` |
|        - |  9726 | `		/* Empty map,return  0 */` |
|      ! 0 |  9727 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9728 | `		return PH7_OK;` |
|        - |  9729 | `	}` |
|        - |  9730 | `	/* Prepare the aux data */` |
|        5 |  9731 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 |  9732 | `	if( nArg > 1 ){` |
|        3 |  9733 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 |  9734 | `		if( nArg > 2 ){` |
|      ! 0 |  9735 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 |  9736 | `		}` |
|        1 |  9737 | `	}` |
|        5 |  9738 | `	sAux.pVm = pCtx->pVm;` |
|        - |  9739 | `	/* Invoke the worker callback */` |
|        5 |  9740 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - |  9741 | `	/* Number of variables successfully imported */` |
|        5 |  9742 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 |  9743 | `	return PH7_OK;` |
|        3 |  9744 |  |
|        - |  9745 | `/*` |
|        - |  9746 | ` * Worker callback for the [extract()] function defined` |
|        - |  9747 | ` * below.` |
|        - |  9748 | ` */` |
|        8 |  9749 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9750 |  |
|        9 |  9751 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 |  9752 | `	int iFlags = pAux->iFlags;` |
|        9 |  9753 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9754 | `	ph7_value *pObj;` |
|        - |  9755 | `	SyString sVar;` |
|        9 |  9756 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 |  9757 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 |  9758 | `	}` |
|        - |  9759 | `	/* Perform a string cast */` |
|        9 |  9760 | `	PH7_MemObjToString(pKey);` |
|        9 |  9761 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9762 | `		/* Unavailable variable name */` |
|      ! 0 |  9763 | `		return SXRET_OK;` |
|        - |  9764 | `	}` |
|        9 |  9765 | `	sVar.nByte = 0; /* cc warning */` |
|        9 |  9766 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 |  9767 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9768 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9769 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9770 | `			);` |
|      ! 0 |  9771 | `	}else{` |
|       13 |  9772 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 |  9773 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9774 | `	}` |
|        9 |  9775 | `	sVar.zString = pAux->zWorker;` |
|        - |  9776 | `	/* Try to extract the variable */` |
|        9 |  9777 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 |  9778 | `	if( pObj ){` |
|        - |  9779 | `		/* Collision */` |
|        5 |  9780 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 |  9781 | `			return SXRET_OK;` |
|        - |  9782 | `		}` |
|        5 |  9783 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 |  9784 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - |  9785 | `				/* Already prefixed */` |
|      ! 0 |  9786 | `				return SXRET_OK;` |
|        - |  9787 | `			}` |
|      ! 0 |  9788 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9789 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9790 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9791 | `				);` |
|      ! 0 |  9792 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 |  9793 | `		}` |
|        3 |  9794 | `	}else{` |
|        - |  9795 | `		/* Create the variable */` |
|        5 |  9796 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - |  9797 | `	}` |
|        9 |  9798 | `	if( pObj ){` |
|        - |  9799 | `		/* Overwrite the old value */` |
|        9 |  9800 | `		PH7_MemObjStore(pValue,pObj);` |
|        - |  9801 | `		/* Increment counter */` |
|        9 |  9802 | `		pAux->iCount++;` |
|        4 |  9803 | `	}` |
|        9 |  9804 | `	return SXRET_OK;` |
|        5 |  9805 |  |
|        - |  9806 | `/*` |
|        - |  9807 | ` * Worker callback for the [import_request_variables()] function` |
|        - |  9808 | ` * defined below.` |
|        - |  9809 | ` */` |
|        2 |  9810 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9811 |  |
|        3 |  9812 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 |  9813 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9814 | `	ph7_value *pObj;` |
|        - |  9815 | `	SyString sVar;` |
|        - |  9816 | `	/* Perform a string cast */` |
|        3 |  9817 | `	PH7_MemObjToString(pKey);` |
|        3 |  9818 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9819 | `		/* Unavailable variable name */` |
|      ! 0 |  9820 | `		return SXRET_OK;` |
|        - |  9821 | `	}` |
|        3 |  9822 | `	sVar.nByte = 0; /* cc warning */` |
|        3 |  9823 | `	if( pAux->Prefixlen > 0 ){` |
|        4 |  9824 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 |  9825 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 |  9826 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9827 | `			);` |
|        2 |  9828 | `	}else{` |
|      ! 0 |  9829 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 |  9830 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9831 | `	}` |
|        3 |  9832 | `	sVar.zString = pAux->zWorker;` |
|        - |  9833 | `	/* Extract the variable */` |
|        3 |  9834 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 |  9835 | `	if( pObj ){` |
|        3 |  9836 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 |  9837 | `	}` |
|        3 |  9838 | `	return SXRET_OK;` |
|        2 |  9839 |  |
|        - |  9840 | `/*` |
|        - |  9841 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - |  9842 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - |  9843 | ` * Parameters` |
|        - |  9844 | ` * $types` |
|        - |  9845 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - |  9846 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - |  9847 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - |  9848 | ` *  POST includes the POST uploaded file information.` |
|        - |  9849 | ` *  Note:` |
|        - |  9850 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - |  9851 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - |  9852 | ` * $prefix` |
|        - |  9853 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - |  9854 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - |  9855 | ` *  variable named $pref_userid.` |
|        - |  9856 | ` * Return` |
|        - |  9857 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9858 | ` */` |
|        2 |  9859 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9860 |  |
|        - |  9861 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - |  9862 | `	extract_aux_data sAux;` |
|        - |  9863 | `	int nLen,nPrefixLen;` |
|        - |  9864 | `	ph7_value *pSuper;` |
|        - |  9865 | `	ph7_vm *pVm;` |
|        - |  9866 | `	/* By default import only $_GET variables  */` |
|        3 |  9867 | `	zImport = "G";` |
|        3 |  9868 | `	nLen = (int)sizeof(char);` |
|        3 |  9869 | `	zPrefix = 0;` |
|        3 |  9870 | `	nPrefixLen = 0;` |
|        3 |  9871 | `	if( nArg > 0 ){` |
|        3 |  9872 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 |  9873 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 |  9874 | `		}` |
|        3 |  9875 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9876 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 |  9877 | `		}` |
|        1 |  9878 | `	}` |
|        - |  9879 | `	/* Point to the underlying VM */` |
|        3 |  9880 | `	pVm = pCtx->pVm;` |
|        - |  9881 | `	/* Initialize the aux data */` |
|        3 |  9882 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 |  9883 | `	sAux.zPrefix = zPrefix;` |
|        3 |  9884 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 |  9885 | `	sAux.pVm = pVm;` |
|        - |  9886 | `	/* Extract */` |
|        3 |  9887 | `	zEnd = &zImport[nLen];` |
|        5 |  9888 | `	while( zImport < zEnd ){` |
|        3 |  9889 | `		int c = zImport[0];` |
|        3 |  9890 | `		pSuper = 0;` |
|        3 |  9891 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - |  9892 | `			/* Import $_GET variables */` |
|        3 |  9893 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 |  9894 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - |  9895 | `			/* Import $_POST variables */` |
|      ! 0 |  9896 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 |  9897 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - |  9898 | `			/* Import $_COOKIE variables */` |
|      ! 0 |  9899 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 |  9900 | `		}` |
|        3 |  9901 | `		if( pSuper ){` |
|        - |  9902 | `			/* Iterate throw array entries */` |
|        3 |  9903 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 |  9904 | `		}` |
|        - |  9905 | `		/* Advance the cursor */` |
|        3 |  9906 | `		zImport++;` |
|        1 |  9907 | `	}` |
|        - |  9908 | `	/* All done,return TRUE*/` |
|        3 |  9909 | `	ph7_result_bool(pCtx,0);` |
|        3 |  9910 | `	return PH7_OK;` |
|        1 |  9911 |  |
|        - |  9912 | `/*` |
|        - |  9913 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |  9914 | ` * Refer to the eval() language construct implementation for more` |
|        - |  9915 | ` * information.` |
|        - |  9916 | ` */` |
|    10278 |  9917 | `static sxi32 VmEvalChunk(` |
|        - |  9918 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |  9919 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |  9920 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |  9921 | `	int iFlags,         /* Compile flag */` |
|        - |  9922 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |  9923 | `	)` |
|        2 |  9924 |  |
|        - |  9925 | `	SySet *pByteCode,aByteCode;` |
|        - |  9926 | `	SyBlob sSavedNs;` |
|    10280 |  9927 | `	ProcConsumer xErr = 0;` |
|    10280 |  9928 | `	void *pErrData = 0;` |
|        - |  9929 | `	/* Initialize bytecode container */` |
|    10280 |  9930 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    10280 |  9931 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |  9932 | `	/* Reset the code generator */` |
|    10280 |  9933 | `	if( bTrueReturn ){` |
|        - |  9934 | `		/* Included file,log compile-time errors */` |
|     7637 |  9935 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7637 |  9936 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3818 |  9937 | `	}` |
|    10280 |  9938 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |  9939 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - |  9940 | `	 * Each included file has its own namespace scope; after execution,` |
|        - |  9941 | `	 * the caller's namespace is restored. */` |
|    10280 |  9942 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    10280 |  9943 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    10280 |  9944 | `	if( bTrueReturn ){` |
|        - |  9945 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     7637 |  9946 | `		SyBlobReset(&pVm->sNamespace);` |
|     3818 |  9947 | `	}` |
|        - |  9948 | `	/* Swap bytecode container */` |
|    10280 |  9949 | `	pByteCode = pVm->pByteContainer;` |
|    10280 |  9950 | `	pVm->pByteContainer = &aByteCode;` |
|        - |  9951 | `	/* Compile the chunk */` |
|    10280 |  9952 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    15419 |  9953 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |  9954 | `		/* Compilation error,return false */` |
|        3 |  9955 | `		if( pCtx ){` |
|        3 |  9956 | `			ph7_result_bool(pCtx,0);` |
|        1 |  9957 | `		}` |
|        2 |  9958 | `	}else{` |
|        - |  9959 | `		/* Mount any newly defined classes */` |
|        - |  9960 | `		SyHashEntry *pEntry;` |
|        - |  9961 | `		ph7_class *pClass;` |
|        - |  9962 | `		ph7_value sResult; /* Return value */` |
|        - |  9963 | `		sxi32 rc;` |
|    10278 |  9964 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   282550 |  9965 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   267136 |  9966 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  9967 | `			/* Only mount classes that haven't been mounted yet */` |
|   267136 |  9968 | `			if( !pClass->bMounted ){` |
|    64144 |  9969 | `				rc = VmMountUserClass(pVm,pClass);` |
|    64144 |  9970 | `				if( rc != SXRET_OK ){` |
|        - |  9971 | `					/* Mount failure (likely memory error) */` |
|      ! 0 |  9972 | `					if( pCtx ){` |
|      ! 0 |  9973 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 |  9974 | `					}` |
|      ! 0 |  9975 | `					goto Cleanup;` |
|        - |  9976 | `				}` |
|    32071 |  9977 | `			}` |
|        2 |  9978 | `		}` |
|    10278 |  9979 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  9980 | `			/* Out of memory */` |
|      ! 0 |  9981 | `			if( pCtx ){` |
|      ! 0 |  9982 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  9983 | `			}` |
|      ! 0 |  9984 | `			goto Cleanup;` |
|        - |  9985 | `		}` |
|    10278 |  9986 | `		if( bTrueReturn ){` |
|        - |  9987 | `			/* Assume a boolean true return value */` |
|     7637 |  9988 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3819 |  9989 | `		}else{` |
|        - |  9990 | `			/* Assume a null return value */` |
|     2642 |  9991 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  9992 | `		}` |
|        - |  9993 | `		/* Execute the compiled chunk */` |
|    10278 |  9994 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    10278 |  9995 | `		if( pCtx ){` |
|        - |  9996 | `			/* Set the execution result */` |
|     7650 |  9997 | `			ph7_result_value(pCtx,&sResult);` |
|     3824 |  9998 | `		}` |
|    10278 |  9999 | `		PH7_MemObjRelease(&sResult);` |
|        - | 10000 | `	}` |
|     5139 | 10001 | `Cleanup:` |
|        - | 10002 | `	/* Cleanup the mess left behind */` |
|    10280 | 10003 | `	pVm->pByteContainer = pByteCode;` |
|    10280 | 10004 | `	SySetRelease(&aByteCode);` |
|        - | 10005 | `	/* Restore caller's namespace state */` |
|    10280 | 10006 | `	SyBlobReset(&pVm->sNamespace);` |
|    10280 | 10007 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    10280 | 10008 | `	SyBlobRelease(&sSavedNs);` |
|    10280 | 10009 | `	return SXRET_OK;` |
|        2 | 10010 |  |
|        - | 10011 | `/*` |
|        - | 10012 | ` * value eval(string $code)` |
|        - | 10013 | ` *   Evaluate a string as PHP code.` |
|        - | 10014 | ` * Parameter` |
|        - | 10015 | ` *  code: PHP code to evaluate.` |
|        - | 10016 | ` * Return` |
|        - | 10017 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 10018 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 10019 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 10020 | ` */` |
|       16 | 10021 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10022 |  |
|        - | 10023 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 | 10024 | `	if( nArg < 1 ){` |
|        - | 10025 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10026 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10027 | `		return SXRET_OK;` |
|        - | 10028 | `	}` |
|        - | 10029 | `	/* Chunk to evaluate */` |
|       18 | 10030 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 | 10031 | `	if( sChunk.nByte < 1 ){` |
|        - | 10032 | `		/* Empty string,return NULL */` |
|        3 | 10033 | `		ph7_result_null(pCtx);` |
|        3 | 10034 | `		return SXRET_OK;` |
|        - | 10035 | `	}` |
|        - | 10036 | `	/* Eval the chunk */` |
|       16 | 10037 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 | 10038 | `	return SXRET_OK;` |
|       10 | 10039 |  |
|        - | 10040 | `/*` |
|        - | 10041 | ` * Check if a file path is already included.` |
|        - | 10042 | ` */` |
|    15268 | 10043 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 | 10044 |  |
|        - | 10045 | `	SyString *aEntries;` |
|        - | 10046 | `	sxu32 n;` |
|    15269 | 10047 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 10048 | `	/* Perform a linear search */` |
| 58267061 | 10049 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 58251799 | 10050 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 10051 | `			/* Already included */` |
|        7 | 10052 | `			return TRUE;` |
|        - | 10053 | `		}` |
| 29125897 | 10054 | `	}` |
|    15263 | 10055 | `	return FALSE;` |
|     7635 | 10056 |  |
|        - | 10057 | `/*` |
|        - | 10058 | ` * Push a file path in the appropriate VM container.` |
|        - | 10059 | ` */` |
|    17888 | 10060 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 10061 |  |
|        - | 10062 | `	SyString sPath;` |
|        - | 10063 | `	char *zDup;` |
|        - | 10064 | `#ifdef __WINNT__` |
|        - | 10065 | `	char *zCur;` |
|        - | 10066 | `#endif` |
|        - | 10067 | `	sxi32 rc;` |
|    17890 | 10068 | `	if( nLen < 0 ){` |
|     2622 | 10069 | `		nLen = SyStrlen(zPath);` |
|     1310 | 10070 | `	}` |
|        - | 10071 | `	/* Duplicate the file path first */` |
|    17890 | 10072 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    17890 | 10073 | `	if( zDup == 0 ){` |
|      ! 0 | 10074 | `		return SXERR_MEM;` |
|        - | 10075 | `	}` |
|        - | 10076 | `#ifdef __WINNT__` |
|        - | 10077 | `	/* Normalize path on windows` |
|        - | 10078 | `	 * Example:` |
|        - | 10079 | `	 *    Path/To/File.php` |
|        - | 10080 | `	 * becomes` |
|        - | 10081 | `	 *   path\to\file.php` |
|        - | 10082 | `	 */` |
|        2 | 10083 | `	zCur = zDup;` |
|        2 | 10084 | `	while( zCur[0] != 0 ){` |
|        2 | 10085 | `		if( zCur[0] == '/' ){` |
|        2 | 10086 | `			zCur[0] = '\\';` |
|        2 | 10087 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 10088 | `			int c = SyToLower(zCur[0]);` |
|        1 | 10089 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 10090 | `		}` |
|        2 | 10091 | `		zCur++;` |
|        2 | 10092 | `	}` |
|        - | 10093 | `#endif` |
|        - | 10094 | `	/* Install the file path */` |
|    17890 | 10095 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    17890 | 10096 | `	if( !bMain ){` |
|    15269 | 10097 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 10098 | `			/* Already included */` |
|        7 | 10099 | `			*pNew = 0;` |
|        4 | 10100 | `		}else{` |
|        - | 10101 | `			/* Insert in the corresponding container */` |
|    15263 | 10102 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    15263 | 10103 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10104 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 10105 | `				return rc;` |
|        - | 10106 | `			}` |
|    15263 | 10107 | `			*pNew = 1;` |
|        - | 10108 | `		}` |
|     7634 | 10109 | `	}` |
|    17890 | 10110 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    17890 | 10111 | `	return SXRET_OK;` |
|     8946 | 10112 |  |
|        - | 10113 | `/*` |
|        - | 10114 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 10115 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 10116 | ` * indicates failure.` |
|        - | 10117 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 10118 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 10119 | ` * operations.` |
|        - | 10120 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 10121 | ` * this function is a no-op.` |
|        - | 10122 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 10123 | ` * constructs for more information.` |
|        - | 10124 | ` */` |
|     7642 | 10125 | `static sxi32 VmExecIncludedFile(` |
|        - | 10126 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 10127 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 10128 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 10129 | `	 )` |
|        2 | 10130 |  |
|        - | 10131 | `	sxi32 rc;` |
|        - | 10132 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10133 | `	const ph7_io_stream *pStream;` |
|        - | 10134 | `	SyBlob sContents;` |
|        - | 10135 | `	void *pHandle;` |
|        - | 10136 | `	ph7_vm *pVm;` |
|        - | 10137 | `	int isNew;` |
|        - | 10138 | `	/* Initialize fields */` |
|     7644 | 10139 | `	pVm = pCtx->pVm;` |
|     7644 | 10140 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7644 | 10141 | `	isNew = 0;` |
|        - | 10142 | `	/* Extract the associated stream */` |
|     7644 | 10143 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 10144 | `	/*` |
|        - | 10145 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 10146 | `	 * in a read-only mode.` |
|        - | 10147 | `	 */` |
|     7644 | 10148 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7644 | 10149 | `	if( pHandle == 0 ){` |
|        3 | 10150 | `		return SXERR_IO;` |
|        - | 10151 | `	}` |
|     7641 | 10152 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7641 | 10153 | `	if( IncludeOnce && !isNew ){` |
|        - | 10154 | `		/* Already included */` |
|        5 | 10155 | `		rc = SXERR_EXISTS;` |
|        3 | 10156 | `	}else{` |
|        - | 10157 | `		/* Read the whole file contents */` |
|     7637 | 10158 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7637 | 10159 | `		if( rc == SXRET_OK ){` |
|        - | 10160 | `			SyString sScript;` |
|        - | 10161 | `			/* Compile and execute the script */` |
|     7637 | 10162 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7637 | 10163 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3818 | 10164 | `		}` |
|        - | 10165 | `	}` |
|        - | 10166 | `	/* Pop from the set of included file */` |
|     7641 | 10167 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 10168 | `	/* Close the handle */` |
|     7641 | 10169 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 10170 | `	/* Release the working buffer */` |
|     7641 | 10171 | `	SyBlobRelease(&sContents);` |
|        - | 10172 | `#else` |
|        - | 10173 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 10174 | `	SXUNUSED(pPath);` |
|        - | 10175 | `	SXUNUSED(IncludeOnce);` |
|        - | 10176 | `	rc = SXERR_IO;` |
|        - | 10177 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7641 | 10178 | `	return rc;` |
|     3823 | 10179 |  |
|        - | 10180 | `/*` |
|        - | 10181 | ` * string get_include_path(void)` |
|        - | 10182 | ` *  Gets the current include_path configuration option.` |
|        - | 10183 | ` * Parameter` |
|        - | 10184 | ` *  None` |
|        - | 10185 | ` * Return` |
|        - | 10186 | ` *  Included paths as a string` |
|        - | 10187 | ` */` |
|        2 | 10188 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10189 |  |
|        3 | 10190 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10191 | `	SyString *aEntry;` |
|        - | 10192 | `	int dir_sep;` |
|        - | 10193 | `	sxu32 n;` |
|        - | 10194 | `#ifdef __WINNT__` |
|        1 | 10195 | `	dir_sep = ';';` |
|        - | 10196 | `#else` |
|        - | 10197 | `	/* Assume UNIX path separator */` |
|        2 | 10198 | `	dir_sep = ':';` |
|        - | 10199 | `#endif` |
|        1 | 10200 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10201 | `	SXUNUSED(apArg);` |
|        - | 10202 | `	/* Point to the list of import paths */` |
|        3 | 10203 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 10204 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 10205 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 10206 | `		if( n > 0 ){` |
|        - | 10207 | `			/* Append dir seprator */` |
|      ! 0 | 10208 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 10209 | `		}` |
|        - | 10210 | `		/* Append path */` |
|        3 | 10211 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 10212 | `	}` |
|        3 | 10213 | `	return PH7_OK;` |
|        1 | 10214 |  |
|        - | 10215 | `/*` |
|        - | 10216 | ` * string get_get_included_files(void)` |
|        - | 10217 | ` *  Gets the current include_path configuration option.` |
|        - | 10218 | ` * Parameter` |
|        - | 10219 | ` *  None` |
|        - | 10220 | ` * Return` |
|        - | 10221 | ` *  Included paths as a string` |
|        - | 10222 | ` */` |
|        2 | 10223 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10224 |  |
|        3 | 10225 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 10226 | `	ph7_value *pArray,*pWorker;` |
|        - | 10227 | `	SyString *pEntry;` |
|        - | 10228 | `	int c,d;` |
|        - | 10229 | `	/* Create an array and a working value */` |
|        3 | 10230 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 10231 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 10232 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 10233 | `		/* Out of memory,return null */` |
|      ! 0 | 10234 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10235 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10236 | `		SXUNUSED(apArg);` |
|      ! 0 | 10237 | `		return PH7_OK;` |
|        - | 10238 | `	}` |
|        3 | 10239 | `	c = d = '/';` |
|        - | 10240 | `#ifdef __WINNT__` |
|        1 | 10241 | `	d = '\\';` |
|        - | 10242 | `#endif` |
|        - | 10243 | `	/* Iterate throw entries */` |
|        3 | 10244 | `	SySetResetCursor(pFiles);` |
|     3689 | 10245 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 10246 | `		const char *zBase,*zEnd;` |
|        - | 10247 | `		int iLen;` |
|        - | 10248 | `		/* reset the string cursor */` |
|     3687 | 10249 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 10250 | `		/* Extract base name */` |
|     3687 | 10251 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 10252 | `		/* Ignore trailing '/' */` |
|     5530 | 10253 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 10254 | `			zEnd--;` |
|      ! 0 | 10255 | `		}` |
|     3687 | 10256 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   113770 | 10257 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   108241 | 10258 | `			zEnd--;` |
|        1 | 10259 | `		}` |
|     3687 | 10260 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3687 | 10261 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 10262 | `		/* Copy entry name */` |
|     3687 | 10263 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 10264 | `		/* Perform the insertion */` |
|     3687 | 10265 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 10266 | `	}` |
|        - | 10267 | `	/* All done,return the created array */` |
|        3 | 10268 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10269 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 10270 | `	 * by the engine as soon we return from this foreign` |
|        - | 10271 | `	 * function.` |
|        - | 10272 | `	 */` |
|        3 | 10273 | `	return PH7_OK;` |
|        2 | 10274 |  |
|        - | 10275 | `/*` |
|        - | 10276 | ` * include:` |
|        - | 10277 | ` * According to the PHP reference manual.` |
|        - | 10278 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 10279 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 10280 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 10281 | ` *  include() will finally check in the calling script's own directory` |
|        - | 10282 | ` *  and the current working directory before failing. The include()` |
|        - | 10283 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 10284 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 10285 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 10286 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 10287 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 10288 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 10289 | ` *  directory to find the requested file.` |
|        - | 10290 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 10291 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 10292 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 10293 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 10294 | ` */` |
|     7630 | 10295 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10296 |  |
|        - | 10297 | `	SyString sFile;` |
|        - | 10298 | `	sxi32 rc;` |
|     7632 | 10299 | `	if( nArg < 1 ){` |
|        - | 10300 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10301 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10302 | `		return SXRET_OK;` |
|        - | 10303 | `	}` |
|        - | 10304 | `	/* File to include */` |
|     7632 | 10305 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7632 | 10306 | `	if( sFile.nByte < 1 ){` |
|        - | 10307 | `		/* Empty string,return NULL */` |
|      ! 0 | 10308 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10309 | `		return SXRET_OK;` |
|        - | 10310 | `	}` |
|        - | 10311 | `	/* Open,compile and execute the desired script */` |
|     7632 | 10312 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7632 | 10313 | `	if( rc != SXRET_OK ){` |
|        - | 10314 | `		/* Emit a warning and return false */` |
|        3 | 10315 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 10316 | `		ph7_result_bool(pCtx,0);` |
|        1 | 10317 | `	}` |
|     7632 | 10318 | `	return SXRET_OK;` |
|     3817 | 10319 |  |
|        - | 10320 | `/*` |
|        - | 10321 | ` * include_once:` |
|        - | 10322 | ` *  According to the PHP reference manual.` |
|        - | 10323 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 10324 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 10325 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 10326 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 10327 | ` *   just once.` |
|        - | 10328 | ` */` |
|        4 | 10329 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10330 |  |
|        - | 10331 | `	SyString sFile;` |
|        - | 10332 | `	sxi32 rc;` |
|        5 | 10333 | `	if( nArg < 1 ){` |
|        - | 10334 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10335 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10336 | `		return SXRET_OK;` |
|        - | 10337 | `	}` |
|        - | 10338 | `	/* File to include */` |
|        5 | 10339 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10340 | `	if( sFile.nByte < 1 ){` |
|        - | 10341 | `		/* Empty string,return NULL */` |
|      ! 0 | 10342 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10343 | `		return SXRET_OK;` |
|        - | 10344 | `	}` |
|        - | 10345 | `	/* Open,compile and execute the desired script */` |
|        5 | 10346 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 10347 | `	if( rc == SXERR_EXISTS ){` |
|        - | 10348 | `		/* File already included,return TRUE */` |
|        3 | 10349 | `		ph7_result_bool(pCtx,1);` |
|        3 | 10350 | `		return SXRET_OK;` |
|        - | 10351 | `	}` |
|        3 | 10352 | `	if( rc != SXRET_OK ){` |
|        - | 10353 | `		/* Emit a warning and return false */` |
|      ! 0 | 10354 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10355 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10356 | ` 	}` |
|        3 | 10357 | `	return SXRET_OK;` |
|        3 | 10358 |  |
|        - | 10359 | `/*` |
|        - | 10360 | ` * require.` |
|        - | 10361 | ` *  According to the PHP reference manual.` |
|        - | 10362 | ` *   require() is identical to include() except upon failure it will` |
|        - | 10363 | ` *   also produce a fatal level error.` |
|        - | 10364 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 10365 | ` *   emits a warning  which allows the script to continue.` |
|        - | 10366 | ` */` |
|        4 | 10367 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10368 |  |
|        - | 10369 | `	SyString sFile;` |
|        - | 10370 | `	sxi32 rc;` |
|        5 | 10371 | `	if( nArg < 1 ){` |
|        - | 10372 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10373 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10374 | `		return SXRET_OK;` |
|        - | 10375 | `	}` |
|        - | 10376 | `	/* File to include */` |
|        5 | 10377 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10378 | `	if( sFile.nByte < 1 ){` |
|        - | 10379 | `		/* Empty string,return NULL */` |
|      ! 0 | 10380 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10381 | `		return SXRET_OK;` |
|        - | 10382 | `	}` |
|        - | 10383 | `	/* Open,compile and execute the desired script */` |
|        5 | 10384 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 10385 | `	if( rc != SXRET_OK ){` |
|        - | 10386 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10387 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10388 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10389 | `		return PH7_ABORT;` |
|        - | 10390 | `	}` |
|        5 | 10391 | `	return SXRET_OK;` |
|        3 | 10392 |  |
|        - | 10393 | `/*` |
|        - | 10394 | ` * require_once:` |
|        - | 10395 | ` *  According to the PHP reference manual.` |
|        - | 10396 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 10397 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 10398 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 10399 | ` *   and how it differs from its non _once siblings.` |
|        - | 10400 | ` */` |
|        4 | 10401 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10402 |  |
|        - | 10403 | `	SyString sFile;` |
|        - | 10404 | `	sxi32 rc;` |
|        5 | 10405 | `	if( nArg < 1 ){` |
|        - | 10406 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10407 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10408 | `		return SXRET_OK;` |
|        - | 10409 | `	}` |
|        - | 10410 | `	/* File to include */` |
|        5 | 10411 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10412 | `	if( sFile.nByte < 1 ){` |
|        - | 10413 | `		/* Empty string,return NULL */` |
|      ! 0 | 10414 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10415 | `		return SXRET_OK;` |
|        - | 10416 | `	}` |
|        - | 10417 | `	/* Open,compile and execute the desired script */` |
|        5 | 10418 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 10419 | `	if( rc == SXERR_EXISTS ){` |
|        - | 10420 | `		/* File already included,return TRUE */` |
|        3 | 10421 | `		ph7_result_bool(pCtx,1);` |
|        3 | 10422 | `		return SXRET_OK;` |
|        - | 10423 | `	}` |
|        3 | 10424 | `	if( rc != SXRET_OK ){` |
|        - | 10425 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10426 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10427 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10428 | `		return PH7_ABORT;` |
|        - | 10429 | `	}` |
|        3 | 10430 | `	return SXRET_OK;` |
|        3 | 10431 |  |
|        - | 10432 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 10433 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 10434 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 10435 | `/* Table of built-in VM functions. */` |
|        - | 10436 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 10437 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 10438 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 10439 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 10440 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 10441 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 10442 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 10443 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 10444 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 10445 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 10446 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 10447 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 10448 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 10449 | `	    /* Constants management */` |
|        - | 10450 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 10451 | `	{ "define",   vm_builtin_define               },` |
|        - | 10452 | `	{ "constant", vm_builtin_constant             },` |
|        - | 10453 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 10454 | `	   /* Class/Object functions */` |
|        - | 10455 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 10456 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 10457 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 10458 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 10459 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 10460 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 10461 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 10462 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 10463 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 10464 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 10465 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 10466 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 10467 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 10468 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 10469 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 10470 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 10471 | `	   /* Random numbers/strings generators */` |
|        - | 10472 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 10473 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 10474 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 10475 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 10476 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 10477 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10478 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 10479 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 10480 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 10481 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10482 | `	   /* Language constructs functions */` |
|        - | 10483 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 10484 | `	{ "print", vm_builtin_print                   },` |
|        - | 10485 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 10486 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 10487 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 10488 | `	  /* Variable handling functions */` |
|        - | 10489 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 10490 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 10491 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 10492 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 10493 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 10494 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 10495 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 10496 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 10497 | `	  /* Ouput control functions */` |
|        - | 10498 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 10499 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 10500 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 10501 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 10502 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 10503 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 10504 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 10505 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 10506 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 10507 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 10508 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 10509 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 10510 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 10511 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 10512 | `	  /* Assertion functions */` |
|        - | 10513 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 10514 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 10515 | `	  /* Error reporting functions */` |
|        - | 10516 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 10517 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 10518 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 10519 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 10520 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 10521 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 10522 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 10523 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 10524 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 10525 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 10526 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 10527 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 10528 | `	  /* Release info */` |
|        - | 10529 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 10530 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 10531 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 10532 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 10533 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 10534 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 10535 | `	  /* hashmap */` |
|        - | 10536 | `	{"compact",          vm_builtin_compact       },` |
|        - | 10537 | `	{"extract",          vm_builtin_extract       },` |
|        - | 10538 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 10539 | `	  /* URL related function */` |
|        - | 10540 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 10541 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 10542 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10543 | `	   /* XML processing functions */` |
|        - | 10544 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 10545 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 10546 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 10547 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 10548 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 10549 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 10550 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 10551 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 10552 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 10553 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 10554 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 10555 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 10556 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 10557 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 10558 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 10559 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 10560 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 10561 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 10562 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 10563 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 10564 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 10565 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10566 | `	   /* UTF-8 encoding/decoding */` |
|        - | 10567 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 10568 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 10569 | `	   /* Command line processing */` |
|        - | 10570 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 10571 | `	   /* JSON encoding/decoding */` |
|        - | 10572 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 10573 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 10574 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 10575 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 10576 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 10577 | `	   /* Files/URI inclusion facility */` |
|        - | 10578 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 10579 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 10580 | `	{ "include",      vm_builtin_include          },` |
|        - | 10581 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 10582 | `	{ "require",      vm_builtin_require          },` |
|        - | 10583 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 10584 | `};` |
|        - | 10585 | `/*` |
|        - | 10586 | ` * Register the built-in VM functions defined above.` |
|        - | 10587 | ` */` |
|     2366 | 10588 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 10589 |  |
|        - | 10590 | `	sxi32 rc;` |
|        - | 10591 | `	sxu32 n;` |
|   295752 | 10592 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 10593 | `		/* Note that these special functions have access` |
|        - | 10594 | `		 * to the underlying virtual machine as their` |
|        - | 10595 | `		 * private data.` |
|        - | 10596 | `		 */` |
|   293386 | 10597 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   293386 | 10598 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 10599 | `			return rc;` |
|        - | 10600 | `		}` |
|   146694 | 10601 | `	}` |
|     2368 | 10602 | `	return SXRET_OK;` |
|     1185 | 10603 |  |
|        - | 10604 | `/*` |
|        - | 10605 | ` * Check if the given name refer to an installed class.` |
|        - | 10606 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 10607 | ` */` |
|    17208 | 10608 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 10609 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 10610 | `	const char *zName,  /* Name of the target class */` |
|        - | 10611 | `	sxu32 nByte,        /* zName length */` |
|        - | 10612 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 10613 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 10614 | `						 */` |
|        - | 10615 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 10616 | `	)` |
|        2 | 10617 |  |
|        - | 10618 | `	SyHashEntry *pEntry;` |
|        - | 10619 | `	ph7_class *pClass;` |
|     8604 | 10620 | `	SXUNUSED(iNest);` |
|        - | 10621 | `	/* Exact class lookup.` |
|        - | 10622 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 10623 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    17210 | 10624 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    17210 | 10625 | `	if( pEntry == 0 ){` |
|       10 | 10626 | `		return 0;` |
|        - | 10627 | `	}` |
|    17202 | 10628 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    17202 | 10629 | `	if( !iLoadable ){` |
|    16060 | 10630 | `		return pClass;` |
|        - | 10631 | `	}` |
|        - | 10632 | `	/* Filter for loadable classes (skip interfaces/abstract/traits) */` |
|     1144 | 10633 | `	while(pClass){` |
|     1144 | 10634 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1144 | 10635 | `			return pClass;` |
|        - | 10636 | `		}` |
|      ! 0 | 10637 | `		pClass = pClass->pNextName;` |
|      ! 0 | 10638 | `	}` |
|      ! 0 | 10639 | `	return 0;` |
|     8606 | 10640 |  |
|        - | 10641 | `/*` |
|        - | 10642 | ` * Reference Table Implementation` |
|        - | 10643 | ` * Status: stable <chm@symisc.net>` |
|        - | 10644 | ` * Intro` |
|        - | 10645 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 10646 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 10647 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 10648 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 10649 | ` *  Refer to the official for more information on this powerful` |
|        - | 10650 | ` *  extension.` |
|        - | 10651 | ` */` |
|        - | 10652 | `/*` |
|        - | 10653 | ` * Allocate a new reference entry.` |
|        - | 10654 | ` */` |
|  3008798 | 10655 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 10656 |  |
|        - | 10657 | `	VmRefObj *pRef;` |
|        - | 10658 | `	/* Allocate a new instance */` |
|  3008800 | 10659 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3008800 | 10660 | `	if( pRef == 0 ){` |
|      ! 0 | 10661 | `		return 0;` |
|        - | 10662 | `	}` |
|        - | 10663 | `	/* Zero the structure */` |
|  3008800 | 10664 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 10665 | `	/* Initialize fields */` |
|  3008800 | 10666 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3008800 | 10667 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3008800 | 10668 | `	pRef->nIdx = nIdx;` |
|  3008800 | 10669 | `	return pRef;` |
|  1504401 | 10670 |  |
|        - | 10671 | `/*` |
|        - | 10672 | ` * Default hash function used by the reference table` |
|        - | 10673 | ` * for lookup/insertion operations.` |
|        - | 10674 | ` */` |
| 16680247 | 10675 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 10676 |  |
|        - | 10677 | `	/* Calculate the hash based on the memory object index */` |
| 16680249 | 10678 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 10679 |  |
|        - | 10680 | `/*` |
|        - | 10681 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 10682 | ` * in the reference table.` |
|        - | 10683 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 10684 | ` * otherwise.` |
|        - | 10685 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10686 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10687 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10688 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10689 | ` * Refer to the official for more information on this powerful` |
|        - | 10690 | ` * extension.` |
|        - | 10691 | ` */` |
|  8979400 | 10692 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 10693 |  |
|        - | 10694 | `	VmRefObj *pRef;` |
|        - | 10695 | `	sxu32 nBucket;` |
|        - | 10696 | `	/* Point to the appropriate bucket */` |
|  8979402 | 10697 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 10698 | `	/* Perform the lookup */` |
|  8979402 | 10699 | `	pRef = pVm->apRefObj[nBucket];` |
| 19312801 | 10700 | `	for(;;){` |
| 38612976 | 10701 | `		if( pRef == 0 ){` |
|  3086130 | 10702 | `			break;` |
|        - | 10703 | `		}` |
| 35526848 | 10704 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 10705 | `			/* Entry found */` |
|  5893274 | 10706 | `			return pRef;` |
|        - | 10707 | `		}` |
|        - | 10708 | `		/* Point to the next entry */` |
| 29633576 | 10709 | `		pRef = pRef->pNextCollide;` |
|        2 | 10710 | `	}` |
|        - | 10711 | `	/* No such entry,return NULL */` |
|  3086130 | 10712 | `	return 0;` |
|  4489702 | 10713 |  |
|        - | 10714 | `/*` |
|        - | 10715 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10716 | ` *` |
|        - | 10717 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10718 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10719 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10720 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10721 | ` * Refer to the official for more information on this powerful` |
|        - | 10722 | ` * extension.` |
|        - | 10723 | ` */` |
|  3008798 | 10724 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10725 |  |
|        - | 10726 | `	sxu32 nBucket;` |
|  3008800 | 10727 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 10728 | `		VmRefObj **apNew;` |
|        - | 10729 | `		sxu32 nNew;` |
|        - | 10730 | `		/* Allocate a larger table */` |
|     4076 | 10731 | `		nNew = pVm->nRefSize << 1;` |
|     4076 | 10732 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4076 | 10733 | `		if( apNew ){` |
|     4076 | 10734 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 10735 | `			sxu32 n;` |
|        - | 10736 | `			/* Zero the structure */` |
|     4076 | 10737 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 10738 | `			/* Rehash all referenced entries */` |
|  2841636 | 10739 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 10740 | `				/* Remove old collision links */` |
|  2837562 | 10741 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 10742 | `				/* Point to the appropriate bucket */` |
|  2837562 | 10743 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 10744 | `				/* Insert the entry  */` |
|  2837562 | 10745 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2837562 | 10746 | `				if( apNew[nBucket] ){` |
|  2298896 | 10747 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 10748 | `				}` |
|  2837562 | 10749 | `				apNew[nBucket] = pEntry;` |
|        - | 10750 | `				/* Point to the next entry */` |
|  2837562 | 10751 | `				pEntry = pEntry->pNext;` |
|  1418782 | 10752 | `			}` |
|        - | 10753 | `			/* Release the old table */` |
|     4076 | 10754 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 10755 | `			/* Install the new one */` |
|     4076 | 10756 | `			pVm->apRefObj = apNew;` |
|     4076 | 10757 | `			pVm->nRefSize = nNew;` |
|     2037 | 10758 | `		}` |
|     2037 | 10759 | `	}` |
|        - | 10760 | `	/* Point to the appropriate bucket */` |
|  3008800 | 10761 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 10762 | `	/* Insert the entry */` |
|  3008800 | 10763 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3008800 | 10764 | `	if( pVm->apRefObj[nBucket] ){` |
|  2491871 | 10765 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1246241 | 10766 | `	}` |
|  3008800 | 10767 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3008800 | 10768 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3008800 | 10769 | `	pVm->nRefUsed++;` |
|  3008800 | 10770 | `	return SXRET_OK;` |
|        2 | 10771 |  |
|        - | 10772 | `/*` |
|        - | 10773 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 10774 | ` * the reference table.` |
|        - | 10775 | ` * This function is invoked when the user perform an unset` |
|        - | 10776 | ` * call [i.e: unset($var); ].` |
|        - | 10777 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10778 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10779 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10780 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10781 | ` * Refer to the official for more information on this powerful` |
|        - | 10782 | ` * extension.` |
|        - | 10783 | ` */` |
|  2974894 | 10784 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10785 |  |
|        - | 10786 | `	ph7_hashmap_node **apNode;` |
|        - | 10787 | `	SyHashEntry **apEntry;` |
|        - | 10788 | `	sxu32 n;` |
|        - | 10789 | `	/* Point to the reference table */` |
|  2974896 | 10790 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2974896 | 10791 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 10792 | `	/* Unlink the entry from the reference table */` |
|  3058092 | 10793 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    83198 | 10794 | `		if( apEntry[n] ){` |
|    83148 | 10795 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    41573 | 10796 | `		}` |
|    41600 | 10797 | `	}` |
|  5869322 | 10798 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2894428 | 10799 | `		if( apNode[n] ){` |
|     6794 | 10800 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3396 | 10801 | `		}` |
|  1447215 | 10802 | `	}` |
|  2974896 | 10803 | `	if( pRef->pPrevCollide ){` |
|  1120407 | 10804 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   560436 | 10805 | `	}else{` |
|  1854491 | 10806 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 10807 | `	}` |
|  2974896 | 10808 | `	if( pRef->pNextCollide ){` |
|  1681271 | 10809 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   840882 | 10810 | `	}` |
|  2974896 | 10811 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 10812 | `	/* Release the node */` |
|  2974896 | 10813 | `	SySetRelease(&pRef->aReference);` |
|  2974896 | 10814 | `	SySetRelease(&pRef->aArrEntries);` |
|  2974896 | 10815 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2974896 | 10816 | `	pVm->nRefUsed--;` |
|  2974896 | 10817 | `	return SXRET_OK;` |
|        2 | 10818 |  |
|        - | 10819 | `/*` |
|        - | 10820 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10821 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10822 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10823 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10824 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10825 | ` * Refer to the official for more information on this powerful` |
|        - | 10826 | ` * extension.` |
|        - | 10827 | ` */` |
|  3039480 | 10828 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 10829 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10830 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10831 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10832 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 10833 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 10834 | `	)` |
|        2 | 10835 |  |
|  3039482 | 10836 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10837 | `	VmRefObj *pRef;` |
|        - | 10838 | `	/* Check if the referenced object already exists */` |
|  3039482 | 10839 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3039482 | 10840 | `	if( pRef == 0 ){` |
|        - | 10841 | `		/* Create a new entry */` |
|  3008800 | 10842 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3008800 | 10843 | `		if( pRef == 0 ){` |
|      ! 0 | 10844 | `			return SXERR_MEM;` |
|        - | 10845 | `		}` |
|  3008800 | 10846 | `		pRef->iFlags = iFlags;` |
|        - | 10847 | `		/* Install the entry */` |
|  3008800 | 10848 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1504399 | 10849 | `	}` |
|  3039482 | 10850 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3039482 | 10851 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 10852 | `		VmSlot sRef;` |
|        - | 10853 | `		/* Local frame,record referenced entry so that it can` |
|        - | 10854 | `		 * be deleted when we leave this frame.` |
|        - | 10855 | `		 */` |
|    77380 | 10856 | `		sRef.nIdx = nIdx;` |
|    77380 | 10857 | `		sRef.pUserData = pEntry;` |
|    77380 | 10858 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 10859 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 10860 | `		}` |
|    38689 | 10861 | `	}` |
|  3039482 | 10862 | `	if( pEntry ){` |
|        - | 10863 | `		/* Address of the hash-entry */` |
|   107870 | 10864 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    53934 | 10865 | `	}` |
|  3039482 | 10866 | `	if( pMapEntry ){` |
|        - | 10867 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2926742 | 10868 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1463370 | 10869 | `	}` |
|  3039482 | 10870 | `	return SXRET_OK;` |
|  1519742 | 10871 |  |
|        - | 10872 | `/*` |
|        - | 10873 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 10874 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10875 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10876 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10877 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10878 | ` * Refer to the official for more information on this powerful` |
|        - | 10879 | ` * extension.` |
|        - | 10880 | ` */` |
|  2965020 | 10881 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 10882 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10883 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10884 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10885 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 10886 | `	)` |
|        2 | 10887 |  |
|        - | 10888 | `	VmRefObj *pRef;` |
|        - | 10889 | `	sxu32 n;` |
|        - | 10890 | `	/* Check if the referenced object already exists */` |
|  2965022 | 10891 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2965022 | 10892 | `	if( pRef == 0 ){` |
|        - | 10893 | `		/* Not such entry */` |
|    77326 | 10894 | `		return SXERR_NOTFOUND;` |
|        - | 10895 | `	}` |
|        - | 10896 | `	/* Remove the desired entry */` |
|  2887698 | 10897 | `	if( pEntry ){` |
|        - | 10898 | `		SyHashEntry **apEntry;` |
|       56 | 10899 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 10900 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 10901 | `			if( apEntry[n] == pEntry ){` |
|        - | 10902 | `				/* Nullify the entry */` |
|       56 | 10903 | `				apEntry[n] = 0;` |
|        - | 10904 | `				/*` |
|        - | 10905 | `				 * NOTE:` |
|        - | 10906 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 10907 | `				 * we avoid wasting spaces.` |
|        - | 10908 | `				 */` |
|       27 | 10909 | `			}` |
|       79 | 10910 | `		}` |
|       27 | 10911 | `	}` |
|  2887698 | 10912 | `	if( pMapEntry ){` |
|        - | 10913 | `		ph7_hashmap_node **apNode;` |
|  2887644 | 10914 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5775380 | 10915 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2887738 | 10916 | `			if( apNode[n] == pMapEntry ){` |
|        - | 10917 | `				/* nullify the entry */` |
|  2887644 | 10918 | `				apNode[n] = 0;` |
|  1443821 | 10919 | `			}` |
|  1443870 | 10920 | `		}` |
|  1443821 | 10921 | `	}` |
|  2887698 | 10922 | `	return SXRET_OK;` |
|  1482512 | 10923 |  |
|        - | 10924 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 10925 | `/*` |
|        - | 10926 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 10927 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 10928 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 10929 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 10930 | ` * For more information on how to register IO stream devices,please` |
|        - | 10931 | ` * refer to the official documentation.` |
|        - | 10932 | ` */` |
|    23460 | 10933 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 10934 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 10935 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 10936 | `	int nByte              /* *pzDevice length*/` |
|        - | 10937 | `	)` |
|        2 | 10938 |  |
|        - | 10939 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 10940 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 10941 | `	SyString sDev,sCur;` |
|        - | 10942 | `	sxu32 n,nEntry;` |
|        - | 10943 | `	int rc;` |
|        - | 10944 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    23462 | 10945 | `	zNext = zCur = zIn = *pzDevice;` |
|    23462 | 10946 | `	zEnd = &zIn[nByte];` |
|  1498492 | 10947 | `	while( zIn < zEnd ){` |
|  1475034 | 10948 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 10949 | `			/* Got one */` |
|        3 | 10950 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 10951 | `			break;` |
|        - | 10952 | `		}` |
|        - | 10953 | `		/* Advance the cursor */` |
|  1475032 | 10954 | `		zIn++;` |
|        2 | 10955 | `	}` |
|    23462 | 10956 | `	if( zIn >= zEnd ){` |
|        - | 10957 | `		/* No such scheme,return the default stream */` |
|    23460 | 10958 | `		return pVm->pDefStream;` |
|        - | 10959 | `	}` |
|        3 | 10960 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 10961 | `	/* Remove leading and trailing white spaces */` |
|        3 | 10962 | `	SyStringFullTrim(&sDev);` |
|        - | 10963 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 10964 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 10965 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 10966 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 10967 | `		pStream = apStream[n];` |
|        3 | 10968 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 10969 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 10970 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 10971 | `		if( rc == 0 ){` |
|        - | 10972 | `			/* Stream device found */` |
|        3 | 10973 | `			*pzDevice = zNext;` |
|        3 | 10974 | `			return pStream;` |
|        - | 10975 | `		}` |
|      ! 0 | 10976 | `	}` |
|        - | 10977 | `	/* No such stream,return NULL */` |
|      ! 0 | 10978 | `	return 0;` |
|    11732 | 10979 |  |
|        - | 10980 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 10981 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 10982 |  |
