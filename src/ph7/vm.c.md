# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3978/5237 lines (75.96%)

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
|   742812 |    86 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    87 |  |
|   742814 |    88 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       32 |    89 | `		return TRUE;` |
|        - |    90 | `	}` |
|   742784 |    91 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |    92 | `		return TRUE;` |
|        - |    93 | `	}` |
|   742776 |    94 | `	return FALSE;` |
|   371430 |    95 |  |
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
|   420634 |   110 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   420636 |   121 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   420636 |   122 | `	if( pEntry ){` |
|        - |   123 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   124 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   125 | `		pCons->xExpand = xExpand;` |
|        6 |   126 | `		pCons->pUserData = pUserData;` |
|        6 |   127 | `		return SXRET_OK;` |
|        - |   128 | `	}` |
|        - |   129 | `	/* Allocate a new constant instance */` |
|   420632 |   130 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   420632 |   131 | `	if( pCons == 0 ){` |
|      ! 0 |   132 | `		return 0;` |
|        - |   133 | `	}` |
|        - |   134 | `	/* Duplicate constant name */` |
|   420632 |   135 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   420632 |   136 | `	if( zDupName == 0 ){` |
|      ! 0 |   137 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   138 | `		return 0;` |
|        - |   139 | `	}` |
|        - |   140 | `	/* Install the constant */` |
|   420632 |   141 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   420632 |   142 | `	pCons->xExpand = xExpand;` |
|   420632 |   143 | `	pCons->pUserData = pUserData;` |
|   420632 |   144 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   420632 |   145 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   146 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return rc;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* All done,constant can be invoked from PHP code */` |
|   420632 |   151 | `	return SXRET_OK;` |
|   210319 |   152 |  |
|        - |   153 | `/*` |
|        - |   154 | ` * Allocate a new foreign function instance.` |
|        - |   155 | ` * This function return SXRET_OK on success. Any other` |
|        - |   156 | ` * return value indicates failure.` |
|        - |   157 | ` * Please refer to the official documentation for an introduction to` |
|        - |   158 | ` * the foreign function mechanism.` |
|        - |   159 | ` */` |
|   901320 |   160 | `static sxi32 PH7_NewForeignFunction(` |
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
|   901322 |   171 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   901322 |   172 | `	if( pFunc == 0 ){` |
|      ! 0 |   173 | `		return SXERR_MEM;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* Duplicate function name */` |
|   901322 |   176 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   901322 |   177 | `	if( zDup == 0 ){` |
|      ! 0 |   178 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   179 | `		return SXERR_MEM;` |
|        - |   180 | `	}` |
|        - |   181 | `	/* Zero the structure */` |
|   901322 |   182 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   183 | `	/* Initialize structure fields */` |
|   901322 |   184 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   901322 |   185 | `	pFunc->pVm   = pVm;` |
|   901322 |   186 | `	pFunc->xFunc = xFunc;` |
|   901322 |   187 | `	pFunc->pUserData = pUserData;` |
|   901322 |   188 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   189 | `	/* Write a pointer to the new function */` |
|   901322 |   190 | `	*ppOut = pFunc;` |
|   901322 |   191 | `	return SXRET_OK;` |
|   450662 |   192 |  |
|        - |   193 | `/*` |
|        - |   194 | ` * Install a foreign function and it's associated callback so that` |
|        - |   195 | ` * it can be invoked from the target PHP code.` |
|        - |   196 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   197 | ` * return value indicates failure.` |
|        - |   198 | ` * Please refer to the official documentation for an introduction to` |
|        - |   199 | ` * the foreign function mechanism.` |
|        - |   200 | ` */` |
|   903392 |   201 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|   903394 |   212 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   903394 |   213 | `	if( pEntry ){` |
|     2074 |   214 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2074 |   215 | `		pFunc->pUserData = pUserData;` |
|     2074 |   216 | `		pFunc->xFunc = xFunc;` |
|     2074 |   217 | `		SySetReset(&pFunc->aAux);` |
|     2074 |   218 | `		return SXRET_OK;` |
|        - |   219 | `	}` |
|        - |   220 | `	/* Create a new user function */` |
|   901322 |   221 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   901322 |   222 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   223 | `		return rc;` |
|        - |   224 | `	}` |
|        - |   225 | `	/* Install the function in the corresponding hashtable */` |
|   901322 |   226 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   901322 |   227 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   228 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   229 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   230 | `		return rc;` |
|        - |   231 | `	}` |
|        - |   232 | `	/* User function successfully installed */` |
|   901322 |   233 | `	return SXRET_OK;` |
|   451698 |   234 |  |
|        - |   235 | `/*` |
|        - |   236 | ` * Initialize a VM function.` |
|        - |   237 | ` */` |
|    97494 |   238 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   239 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   240 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   241 | `	const char *zName,  /* Function name */` |
|        - |   242 | `	sxu32 nByte,        /* zName length */` |
|        - |   243 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   244 | `	void *pUserData     /* Function private data */` |
|        - |   245 | `	)` |
|        2 |   246 |  |
|        - |   247 | `	/* Zero the structure */` |
|    97496 |   248 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   249 | `	/* Initialize structure fields */` |
|        - |   250 | `	/* Arguments container */` |
|    97496 |   251 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   252 | `	/* Static variable container */` |
|    97496 |   253 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   254 | `	/* Bytecode container */` |
|    97496 |   255 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   256 | `    /* Preallocate some instruction slots */` |
|    97496 |   257 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   258 | `	/* Closure environment */` |
|    97496 |   259 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|    97496 |   260 | `	pFunc->iFlags = iFlags;` |
|    97496 |   261 | `	pFunc->pUserData = pUserData;` |
|    97496 |   262 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|    97496 |   263 | `	return SXRET_OK;` |
|        2 |   264 |  |
|        - |   265 | `/*` |
|        - |   266 | ` * Namespace-aware function lookup.` |
|        - |   267 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   268 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   269 | ` */` |
|        - |   270 | `/*` |
|        - |   271 | ` * Namespace-aware lookup in a single hash table.` |
|        - |   272 | ` * Resolution order: exact name -> use imports -> current NS\name.` |
|        - |   273 | ` */` |
|   561800 |   274 | `static SyHashEntry * VmNsAwareHashLookup(ph7_vm *pVm,SyHash *pHash,const SyString *pName)` |
|        2 |   275 |  |
|        - |   276 | `	SyHashEntry *pEntry;` |
|        - |   277 | `	/* 1. Try exact name */` |
|   561802 |   278 | `	pEntry = SyHashGet(pHash,(const void *)pName->zString,pName->nByte);` |
|   561802 |   279 | `	if( pEntry ){` |
|    26822 |   280 | `		return pEntry;` |
|        - |   281 | `	}` |
|        - |   282 | `	/* 2. Check use imports */` |
|        - |   283 | `	{` |
|        - |   284 | `		SyHashEntry *pImport;` |
|   534982 |   285 | `		pImport = SyHashGet(&pVm->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   534982 |   286 | `		if( pImport ){` |
|       11 |   287 | `			const char *zFQN = (const char *)pImport->pUserData;` |
|       11 |   288 | `			sxu32 nFQN = (sxu32)SyStrlen(zFQN);` |
|       11 |   289 | `			pEntry = SyHashGet(pHash,(const void *)zFQN,nFQN);` |
|       11 |   290 | `			if( pEntry ){` |
|       11 |   291 | `				return pEntry;` |
|        - |   292 | `			}` |
|      ! 0 |   293 | `		}` |
|        - |   294 | `	}` |
|        - |   295 | `	/* 3. Prepend current namespace */` |
|   534972 |   296 | `	if( SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |   297 | `		SyBlob sFQN;` |
|       19 |   298 | `		SyBlobInit(&sFQN,&pVm->sAllocator);` |
|       19 |   299 | `		SyBlobAppend(&sFQN,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|       19 |   300 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       19 |   301 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       19 |   302 | `		pEntry = SyHashGet(pHash,(const void *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|       19 |   303 | `		SyBlobRelease(&sFQN);` |
|       19 |   304 | `		if( pEntry ){` |
|        9 |   305 | `			return pEntry;` |
|        - |   306 | `		}` |
|        5 |   307 | `	}` |
|        - |   308 | `	/* Not found */` |
|   534964 |   309 | `	return 0;` |
|   280924 |   310 |  |
|        - |   311 | `/*` |
|        - |   312 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   313 | ` */` |
|   354520 |   314 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   315 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   316 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   317 | `	SyString *pName     /* Function name */` |
|        - |   318 | `	)` |
|        2 |   319 |  |
|        - |   320 | `	SyHashEntry *pEntry;` |
|        - |   321 | `	sxi32 rc;` |
|   354522 |   322 | `	if( pName == 0 ){` |
|        - |   323 | `		/* Use the built-in name */` |
|    30438 |   324 | `		pName = &pFunc->sName;` |
|    15218 |   325 | `	}` |
|        - |   326 | `	/* Check for duplicates (functions with the same name) first */` |
|   354522 |   327 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   354522 |   328 | `	if( pEntry ){` |
|   275668 |   329 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   275668 |   330 | `		if( pLink != pFunc ){` |
|        - |   331 | `			/* Link */` |
|      179 |   332 | `			pFunc->pNextName = pLink;` |
|      179 |   333 | `			pEntry->pUserData = pFunc;` |
|       89 |   334 | `		}` |
|   275668 |   335 | `		return SXRET_OK;` |
|        - |   336 | `	}` |
|        - |   337 | `	/* First time seen */` |
|    78856 |   338 | `	pFunc->pNextName = 0;` |
|    78856 |   339 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    78856 |   340 | `	return rc;` |
|   177262 |   341 |  |
|        - |   342 | `/*` |
|        - |   343 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   344 | ` */` |
|    27944 |   345 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   346 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   347 | `	ph7_class *pClass /* Target Class */` |
|        - |   348 | `	)` |
|        2 |   349 |  |
|    27946 |   350 | `	SyString *pName = &pClass->sName;` |
|        - |   351 | `	SyHashEntry *pEntry;` |
|        - |   352 | `	sxi32 rc;` |
|        - |   353 | `	/* Check for duplicates */` |
|    27946 |   354 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    27946 |   355 | `	if( pEntry ){` |
|       31 |   356 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   357 | `		/* Link entry with the same name */` |
|       31 |   358 | `		pClass->pNextName = pLink;` |
|       31 |   359 | `		pEntry->pUserData = pClass;` |
|       31 |   360 | `		return SXRET_OK;` |
|        - |   361 | `	}` |
|    27916 |   362 | `	pClass->pNextName = 0;` |
|        - |   363 | `	/* Perform a simple hashtable insertion */` |
|    27916 |   364 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    27916 |   365 | `	return rc;` |
|    13974 |   366 |  |
|        - |   367 | `/*` |
|        - |   368 | ` * Instruction builder interface.` |
|        - |   369 | ` */` |
|  2589348 |   370 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   371 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   372 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   373 | `	sxi32 iP1,    /* First operand */` |
|        - |   374 | `	sxu32 iP2,    /* Second operand */` |
|        - |   375 | `	void *p3,     /* Third operand */` |
|        - |   376 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   377 | `	)` |
|        2 |   378 |  |
|        - |   379 | `	VmInstr sInstr;` |
|        - |   380 | `	sxi32 rc;` |
|        - |   381 | `	/* Fill the VM instruction */` |
|  2589350 |   382 | `	sInstr.iOp = (sxu8)iOp;` |
|  2589350 |   383 | `	sInstr.iP1 = iP1;` |
|  2589350 |   384 | `	sInstr.iP2 = iP2;` |
|  2589350 |   385 | `	sInstr.p3  = p3;` |
|  2589350 |   386 | `	if( pIndex ){` |
|        - |   387 | `		/* Instruction index in the bytecode array */` |
|   165180 |   388 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    82589 |   389 | `	}` |
|        - |   390 | `	/* Finally,record the instruction */` |
|  2589350 |   391 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  2589350 |   392 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   393 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   394 | `		/* Fall throw */` |
|      ! 0 |   395 | `	}` |
|  2589350 |   396 | `	return rc;` |
|        2 |   397 |  |
|        - |   398 | `/*` |
|        - |   399 | ` * Swap the current bytecode container with the given one.` |
|        - |   400 | ` */` |
|   236960 |   401 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   402 |  |
|   236962 |   403 | `	if( pContainer == 0 ){` |
|        - |   404 | `		/* Point to the default container */` |
|      ! 0 |   405 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   406 | `	}else{` |
|        - |   407 | `		/* Change container */` |
|   236962 |   408 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   409 | `	}` |
|   236962 |   410 | `	return SXRET_OK;` |
|        2 |   411 |  |
|        - |   412 | `/*` |
|        - |   413 | ` * Return the current bytecode container.` |
|        - |   414 | ` */` |
|   118480 |   415 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   416 |  |
|   118482 |   417 | `	return pVm->pByteContainer;` |
|        2 |   418 |  |
|        - |   419 | `/*` |
|        - |   420 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   421 | ` */` |
|   162798 |   422 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   423 |  |
|        - |   424 | `	VmInstr *pInstr;` |
|   162800 |   425 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   162800 |   426 | `	return pInstr;` |
|        2 |   427 |  |
|        - |   428 | `/*` |
|        - |   429 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   430 | ` */` |
|   725440 |   431 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   432 |  |
|   725442 |   433 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   434 |  |
|        - |   435 | `/*` |
|        - |   436 | ` * Pop the last VM instruction.` |
|        - |   437 | ` */` |
|   154462 |   438 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   439 |  |
|   154464 |   440 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   441 |  |
|        - |   442 | `/*` |
|        - |   443 | ` * Peek the last VM instruction.` |
|        - |   444 | ` */` |
|   508912 |   445 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   446 |  |
|   508914 |   447 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   448 |  |
|    23568 |   449 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   450 |  |
|        - |   451 | `	VmInstr *aInstr;` |
|        - |   452 | `	sxu32 n;` |
|    23570 |   453 | `	n = SySetUsed(pVm->pByteContainer);` |
|    23570 |   454 | `	if( n < 2 ){` |
|      ! 0 |   455 | `		return 0;` |
|        - |   456 | `	}` |
|    23570 |   457 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    23570 |   458 | `	return &aInstr[n - 2];` |
|    11786 |   459 |  |
|        - |   460 | `/*` |
|        - |   461 | ` * Allocate a new virtual machine frame.` |
|        - |   462 | ` */` |
|    14120 |   463 | `static VmFrame * VmNewFrame(` |
|        - |   464 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   465 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   466 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   467 | `	)` |
|        2 |   468 |  |
|        - |   469 | `	VmFrame *pFrame;` |
|        - |   470 | `	/* Allocate a new vm frame */` |
|    14122 |   471 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    14122 |   472 | `	if( pFrame == 0 ){` |
|      ! 0 |   473 | `		return 0;` |
|        - |   474 | `	}` |
|        - |   475 | `	/* Zero the structure */` |
|    14122 |   476 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   477 | `	/* Initialize frame fields */` |
|    14122 |   478 | `	pFrame->pUserData = pUserData;` |
|    14122 |   479 | `	pFrame->pThis = pThis;` |
|    14122 |   480 | `	pFrame->pVm = pVm;` |
|    14122 |   481 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    14122 |   482 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    14122 |   483 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    14122 |   484 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    14122 |   485 | `	return pFrame;` |
|     7062 |   486 |  |
|        - |   487 | `/*` |
|        - |   488 | ` * Enter a VM frame.` |
|        - |   489 | ` */` |
|    14120 |   490 | `static sxi32 VmEnterFrame(` |
|        - |   491 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   492 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   493 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   494 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   495 | `	)` |
|        2 |   496 |  |
|        - |   497 | `	VmFrame *pFrame;` |
|        - |   498 | `	/* Allocate a new frame */` |
|    14122 |   499 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    14122 |   500 | `	if( pFrame == 0 ){` |
|      ! 0 |   501 | `		return SXERR_MEM;` |
|        - |   502 | `	}` |
|        - |   503 | `	/* Link to the list of active VM frame */` |
|    14122 |   504 | `	pFrame->pParent = pVm->pFrame;` |
|    14122 |   505 | `	pVm->pFrame = pFrame;` |
|    14122 |   506 | `	if( ppFrame ){` |
|        - |   507 | `		/* Write a pointer to the new VM frame */` |
|    11818 |   508 | `		*ppFrame = pFrame;` |
|     5908 |   509 | `	}` |
|    14122 |   510 | `	return SXRET_OK;` |
|     7062 |   511 |  |
|        - |   512 | `/*` |
|        - |   513 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   514 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   515 | ` * information.` |
|        - |   516 | ` */` |
|       52 |   517 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        2 |   518 |  |
|        - |   519 | `	VmFrame *pTarget,*pFrame;` |
|       54 |   520 | `	SyHashEntry *pEntry = 0;` |
|        - |   521 | `	sxi32 rc;` |
|        - |   522 | `	/* Point to the upper frame */` |
|       54 |   523 | `	pFrame = pVm->pFrame;` |
|       54 |   524 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |   525 | `		/* Safely ignore the exception frame */` |
|      ! 0 |   526 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   527 | `	}` |
|       54 |   528 | `	pTarget = pFrame;` |
|       54 |   529 | `	pFrame = pTarget->pParent;` |
|       54 |   530 | `	while( pFrame ){` |
|       54 |   531 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   532 | `			/* Query the current frame */` |
|       54 |   533 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       54 |   534 | `			if( pEntry ){` |
|        - |   535 | `				/* Variable found */` |
|       54 |   536 | `				break;` |
|        - |   537 | `			}` |
|      ! 0 |   538 | `		}` |
|        - |   539 | `		/* Point to the upper frame */` |
|      ! 0 |   540 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   541 | `	}` |
|       54 |   542 | `	if( pEntry == 0 ){` |
|        - |   543 | `		/* Inexistant variable */` |
|      ! 0 |   544 | `		return SXERR_NOTFOUND;` |
|        - |   545 | `	}` |
|        - |   546 | `	/* Link to the current frame */` |
|       54 |   547 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       54 |   548 | `	if( rc == SXRET_OK ){` |
|        - |   549 | `		sxu32 nIdx;` |
|       54 |   550 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       54 |   551 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       26 |   552 | `	}` |
|       54 |   553 | `	return rc;` |
|       28 |   554 |  |
|        - |   555 | `/*` |
|        - |   556 | ` * Leave the top-most active frame.` |
|        - |   557 | ` */` |
|    11816 |   558 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   559 |  |
|    11818 |   560 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    11818 |   561 | `	if( pCurFrame ){` |
|        - |   562 | `		/* Unlink from the list of active VM frame */` |
|    11818 |   563 | `		pVm->pFrame = pCurFrame->pParent;` |
|    11818 |   564 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   565 | `			VmSlot  *aSlot;` |
|        - |   566 | `			sxu32 n;` |
|        - |   567 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    11794 |   568 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    84934 |   569 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   570 | `				/* Unset the local variable */` |
|    73142 |   571 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    36572 |   572 | `			}` |
|        - |   573 | `			/* Remove local reference */` |
|    11794 |   574 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    84990 |   575 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    73198 |   576 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    36600 |   577 | `			}` |
|     5896 |   578 | `		}` |
|        - |   579 | `		/* Release internal containers */` |
|    11818 |   580 | `		SyHashRelease(&pCurFrame->hVar);` |
|    11818 |   581 | `		SySetRelease(&pCurFrame->sArg);` |
|    11818 |   582 | `		SySetRelease(&pCurFrame->sLocal);` |
|    11818 |   583 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   584 | `		/* Release the whole structure */` |
|    11818 |   585 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     5908 |   586 | `	}` |
|    11818 |   587 |  |
|        - |   588 | `/*` |
|        - |   589 | ` * Compare two functions signature and return the comparison result.` |
|        - |   590 | ` */` |
|      818 |   591 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   592 |  |
|      819 |   593 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      819 |   594 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      819 |   595 | `	const char *zSin = pSecond->zString;` |
|      819 |   596 | `	const char *zFin = pFirst->zString;` |
|      819 |   597 | `	const char *zPtr = zFin;` |
|      409 |   598 | `	for(;;){` |
|      819 |   599 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      410 |   600 | `			break;` |
|        - |   601 | `		}` |
|      ! 0 |   602 | `		if( zFin[0] != zSin[0] ){` |
|        - |   603 | `			/* mismatch */` |
|      ! 0 |   604 | `			break;` |
|        - |   605 | `		}` |
|      ! 0 |   606 | `		zFin++;` |
|      ! 0 |   607 | `		zSin++;` |
|      ! 0 |   608 | `	}` |
|      819 |   609 | `	return (int)(zFin-zPtr);` |
|        1 |   610 |  |
|        - |   611 | `/*` |
|        - |   612 | ` * Select the appropriate VM function for the current call context.` |
|        - |   613 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   614 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   615 | ` * Refer to the official documentation for more information.` |
|        - |   616 | ` */` |
|      122 |   617 | `static ph7_vm_func * VmOverload(` |
|        - |   618 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   619 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   620 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   621 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   622 | `	)` |
|        1 |   623 |  |
|        - |   624 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   625 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   626 | `	ph7_vm_func *pLink;` |
|        - |   627 | `	SyString sArgSig;` |
|        - |   628 | `	SyBlob sSig;` |
|        - |   629 |  |
|      123 |   630 | `	pLink = pList;` |
|      123 |   631 | `	i = 0;` |
|        - |   632 | `	/* Put functions expecting the same number of passed arguments */` |
|     1031 |   633 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|      969 |   634 | `		if( pLink == 0 ){` |
|       61 |   635 | `			break;` |
|        - |   636 | `		}` |
|      909 |   637 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   638 | `			/* Candidate for overloading */` |
|      863 |   639 | `			apSet[i++] = pLink;` |
|      431 |   640 | `		}` |
|        - |   641 | `		/* Point to the next entry */` |
|      909 |   642 | `		pLink = pLink->pNextName;` |
|        1 |   643 | `	}` |
|      123 |   644 | `	if( i < 1 ){` |
|        - |   645 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   646 | `		return pList;` |
|        - |   647 | `	}` |
|      123 |   648 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   649 | `		/* Return the only candidate */` |
|       21 |   650 | `		return apSet[0];` |
|        - |   651 | `	}` |
|        - |   652 | `	/* Calculate function signature */` |
|      103 |   653 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      355 |   654 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      253 |   655 | `		int c = 'n'; /* null */` |
|      253 |   656 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   657 | `			/* Hashmap */` |
|       45 |   658 | `			c = 'h';` |
|      231 |   659 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   660 | `			/* bool */` |
|      ! 0 |   661 | `			c = 'b';` |
|      209 |   662 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   663 | `			/* int */` |
|        5 |   664 | `			c = 'i';` |
|      207 |   665 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   666 | `			/* String */` |
|      105 |   667 | `			c = 's';` |
|      153 |   668 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   669 | `			/* Float */` |
|      ! 0 |   670 | `			c = 'f';` |
|      101 |   671 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   672 | `			/* Class instance */` |
|      ! 0 |   673 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|      ! 0 |   674 | `			SyString *pName = &pClass->sName;` |
|      ! 0 |   675 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|      ! 0 |   676 | `			c = -1;` |
|      ! 0 |   677 | `		}` |
|      253 |   678 | `		if( c > 0 ){` |
|      253 |   679 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      126 |   680 | `		}` |
|      127 |   681 | `	}` |
|      103 |   682 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      103 |   683 | `	iTarget = 0;` |
|      103 |   684 | `	iMax = -1;` |
|        - |   685 | `	/* Select the appropriate function */` |
|      921 |   686 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   687 | `		/* Compare the two signatures */` |
|      819 |   688 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      819 |   689 | `		if( iCur > iMax ){` |
|      103 |   690 | `			iMax = iCur;` |
|      103 |   691 | `			iTarget = j;` |
|       51 |   692 | `		}` |
|      410 |   693 | `	}` |
|      103 |   694 | `	SyBlobRelease(&sSig);` |
|        - |   695 | `	/* Appropriate function for the current call context */` |
|      103 |   696 | `	return apSet[iTarget];` |
|       62 |   697 |  |
|        - |   698 | `/* Forward declaration */` |
|        - |   699 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   700 | `/*` |
|        - |   701 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   702 | ` * it can be instanciated from the executed PHP script.` |
|        - |   703 | ` */` |
|    84670 |   704 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   705 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   706 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   707 | `	)` |
|        2 |   708 |  |
|        - |   709 | `	ph7_class_method *pMeth;` |
|        - |   710 | `	ph7_class_attr *pAttr;` |
|        - |   711 | `	SyHashEntry *pEntry;` |
|        - |   712 | `	sxi32 rc;` |
|        - |   713 | `	/* Reset the loop cursor */` |
|    84672 |   714 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   715 | `	/* Process only static and constant attribute */` |
|   329779 |   716 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   717 | `		/* Extract the current attribute */` |
|   202774 |   718 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   202774 |   719 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   720 | `			ph7_value *pMemObj;` |
|        - |   721 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1290 |   722 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1290 |   723 | `			if( pMemObj == 0 ){` |
|      ! 0 |   724 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   725 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   726 | `					&pClass->sName,&pAttr->sName` |
|        - |   727 | `					);` |
|      ! 0 |   728 | `				return SXERR_MEM;` |
|        - |   729 | `			}` |
|     1290 |   730 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   731 | `				/* Initialize attribute default value (any complex expression) */` |
|     1290 |   732 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      644 |   733 | `			}` |
|        - |   734 | `			/* Record attribute index */` |
|     1290 |   735 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   736 | `			/* Install static attribute in the reference table */` |
|     1290 |   737 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      644 |   738 | `		}` |
|        2 |   739 | `	}` |
|        - |   740 | `	/* Install class methods */` |
|    84672 |   741 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |   742 | `		/* Do not mount interface methods since they are signatures only.` |
|        - |   743 | `		 */` |
|    45018 |   744 | `		return SXRET_OK;` |
|        - |   745 | `	}` |
|        - |   746 | `	/* Create constructor alias if not yet done */` |
|    39656 |   747 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   748 | `		/* User constructor with the same base class name */` |
|      242 |   749 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      242 |   750 | `		if( pEntry ){` |
|      ! 0 |   751 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   752 | `			/* Create the alias */` |
|      ! 0 |   753 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   754 | `		}` |
|      120 |   755 | `	}` |
|        - |   756 | `	/* Install the methods now */` |
|    39656 |   757 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   383573 |   758 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   324092 |   759 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   324092 |   760 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   324086 |   761 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   324086 |   762 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   763 | `				return rc;` |
|        - |   764 | `			}` |
|   162042 |   765 | `		}` |
|        2 |   766 | `	}` |
|        - |   767 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    39656 |   768 | `	pClass->bMounted = TRUE;` |
|    39656 |   769 | `	return SXRET_OK;` |
|    42337 |   770 |  |
|        - |   771 | `/*` |
|        - |   772 | ` * Allocate a private frame for attributes of the given` |
|        - |   773 | ` * class instance (Object in the PHP jargon).` |
|        - |   774 | ` */` |
|     1074 |   775 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   776 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   777 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   778 | `	)` |
|        2 |   779 |  |
|     1076 |   780 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   781 | `	ph7_class_attr *pAttr;` |
|        - |   782 | `	SyHashEntry *pEntry;` |
|        - |   783 | `	sxi32 rc;` |
|        - |   784 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1076 |   785 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4558 |   786 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   787 | `		VmClassAttr *pVmAttr;` |
|        - |   788 | `		/* Extract the current attribute */` |
|     3484 |   789 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3484 |   790 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3484 |   791 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   792 | `			return SXERR_MEM;` |
|        - |   793 | `		}` |
|     3484 |   794 | `		pVmAttr->pAttr = pAttr;` |
|     3484 |   795 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   796 | `			ph7_value *pMemObj;` |
|        - |   797 | `			/* Reserve a memory object for this attribute */` |
|     3478 |   798 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3478 |   799 | `			if( pMemObj == 0 ){` |
|      ! 0 |   800 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   801 | `				return SXERR_MEM;` |
|        - |   802 | `			}` |
|     3478 |   803 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3478 |   804 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   805 | `				/* Initialize attribute default value (any complex expression) */` |
|     1136 |   806 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      567 |   807 | `			}` |
|     3478 |   808 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3478 |   809 | `			if( rc != SXRET_OK ){` |
|        - |   810 | `				VmSlot sSlot;` |
|        - |   811 | `				/* Restore memory object */` |
|      ! 0 |   812 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   813 | `				sSlot.pUserData = 0;` |
|      ! 0 |   814 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   815 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   816 | `				return SXERR_MEM;` |
|        - |   817 | `			}` |
|        - |   818 | `			/* Install attribute in the reference table */` |
|     3478 |   819 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1740 |   820 | `		}else{` |
|        - |   821 | `			/* Install static/constant attribute */` |
|        8 |   822 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   823 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   824 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   825 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   826 | `				return SXERR_MEM;` |
|        - |   827 | `			}` |
|        - |   828 | `		}` |
|        2 |   829 | `	}` |
|     1076 |   830 | `	return SXRET_OK;` |
|      539 |   831 |  |
|        - |   832 | `/* Forward declaration */` |
|        - |   833 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   834 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   835 | `/*` |
|        - |   836 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   837 | ` */` |
|        - |   838 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   839 | `/*` |
|        - |   840 | ` * Reserve a constant memory object.` |
|        - |   841 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   842 | ` */` |
|   283184 |   843 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   844 |  |
|        - |   845 | `	ph7_value *pObj;` |
|        - |   846 | `	sxi32 rc;` |
|   283186 |   847 | `	if( pIndex ){` |
|        - |   848 | `		/* Object index in the object table */` |
|   276274 |   849 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   138136 |   850 | `	}` |
|        - |   851 | `	/* Reserve a slot for the new object */` |
|   283186 |   852 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   283186 |   853 | `	if( rc != SXRET_OK ){` |
|        - |   854 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   855 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   856 | `		 */` |
|      ! 0 |   857 | `		return 0;` |
|        - |   858 | `	}` |
|   283186 |   859 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   283186 |   860 | `	return pObj;` |
|   141594 |   861 |  |
|        - |   862 | `/*` |
|        - |   863 | ` * Reserve a memory object.` |
|        - |   864 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   865 | ` */` |
|  2137290 |   866 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   867 |  |
|        - |   868 | `	ph7_value *pObj;` |
|        - |   869 | `	sxi32 rc;` |
|  2137292 |   870 | `	if( pIndex ){` |
|        - |   871 | `		/* Object index in the object table */` |
|  2137292 |   872 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1068645 |   873 | `	}` |
|        - |   874 | `	/* Reserve a slot for the new object */` |
|  2137292 |   875 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2137292 |   876 | `	if( rc != SXRET_OK ){` |
|        - |   877 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   878 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   879 | `		 */` |
|      ! 0 |   880 | `		return 0;` |
|        - |   881 | `	}` |
|  2137292 |   882 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2137292 |   883 | `	return pObj;` |
|  1068647 |   884 |  |
|        - |   885 | `/* Forward declaration */` |
|        - |   886 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   887 | `/*` |
|        - |   888 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   889 | ` * directly as foreign functions.` |
|        - |   890 | ` */` |
|        - |   891 | `#define PH7_BUILTIN_LIB \` |
|        - |   892 | `	"class Exception { "\` |
|        - |   893 | `    "protected $message = 'Unknown exception';"\` |
|        - |   894 | `    "protected $code = 0;"\` |
|        - |   895 | `    "protected $file;"\` |
|        - |   896 | `    "protected $line;"\` |
|        - |   897 | `    "protected $trace;"\` |
|        - |   898 | `    "protected $previous;"\` |
|        - |   899 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   900 | `	"   if( isset($message) ){"\` |
|        - |   901 | `	"	  $this->message = $message;"\` |
|        - |   902 | `	"   }"\` |
|        - |   903 | `	"   $this->code = $code;"\` |
|        - |   904 | `	"   $this->file = __FILE__;"\` |
|        - |   905 | `	"   $this->line = __LINE__;"\` |
|        - |   906 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   907 | `	"   if( isset($previous) ){"\` |
|        - |   908 | `	"     $this->previous = $previous;"\` |
|        - |   909 | `	"   }"\` |
|        - |   910 | `	"}"\` |
|        - |   911 | `	"public function getMessage(){"\` |
|        - |   912 | `	"   return $this->message;"\` |
|        - |   913 | `	"}"\` |
|        - |   914 | `	" public function getCode(){"\` |
|        - |   915 | `	"  return $this->code;"\` |
|        - |   916 | `	"}"\` |
|        - |   917 | `	"public function getFile(){"\` |
|        - |   918 | `	"  return $this->file;"\` |
|        - |   919 | `	"}"\` |
|        - |   920 | `	"public function getLine(){"\` |
|        - |   921 | `	"  return $this->line;"\` |
|        - |   922 | `	"}"\` |
|        - |   923 | `	"public function getTrace(){"\` |
|        - |   924 | `	"   return $this->trace;"\` |
|        - |   925 | `	"}"\` |
|        - |   926 | `	"public function getTraceAsString(){"\` |
|        - |   927 | `	"  return debug_string_backtrace();"\` |
|        - |   928 | `	"}"\` |
|        - |   929 | `	"public function getPrevious(){"\` |
|        - |   930 | `	"    return $this->previous;"\` |
|        - |   931 | `	"}"\` |
|        - |   932 | `	"public function __toString(){"\` |
|        - |   933 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   934 | `    "}"\` |
|        - |   935 | `	"}"\` |
|        - |   936 | `	"class Error extends Exception { }"\` |
|        - |   937 | `	"class TypeError extends Error { }"\` |
|        - |   938 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   939 | `	"class ValueError extends Error { }"\` |
|        - |   940 | `	"class AssertionError extends Error { }"\` |
|        - |   941 | `	"class ErrorException extends Exception { "\` |
|        - |   942 | `	"protected $severity;"\` |
|        - |   943 | `	"public function __construct(string $message = null,"\` |
|        - |   944 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   945 | `	"   if( isset($message) ){"\` |
|        - |   946 | `	"	  $this->message = $message;"\` |
|        - |   947 | `	"   }"\` |
|        - |   948 | `	"   $this->severity = $severity;"\` |
|        - |   949 | `	"   $this->code = $code;"\` |
|        - |   950 | `	"   $this->file = $filename;"\` |
|        - |   951 | `	"   $this->line = $lineno;"\` |
|        - |   952 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   953 | `	"   if( isset($previous) ){"\` |
|        - |   954 | `	"     $this->previous = $previous;"\` |
|        - |   955 | `	"   }"\` |
|        - |   956 | `	"}"\` |
|        - |   957 | `	"public function getSeverity(){"\` |
|        - |   958 | `	"   return $this->severity;"\` |
|        - |   959 | `    "}"\` |
|        - |   960 | `	"}"\` |
|        - |   961 | `	"interface Iterator {"\` |
|        - |   962 | `	"public function current();"\` |
|        - |   963 | `	"public function key();"\` |
|        - |   964 | `	"public function next();"\` |
|        - |   965 | `	"public function rewind();"\` |
|        - |   966 | `	"public function valid();"\` |
|        - |   967 | `	"}"\` |
|        - |   968 | `	"interface IteratorAggregate {"\` |
|        - |   969 | `	"public function getIterator();"\` |
|        - |   970 | `	"}"\` |
|        - |   971 | `	"interface Serializable {"\` |
|        - |   972 | `	"public function serialize();"\` |
|        - |   973 | `	"public function unserialize(string $serialized);"\` |
|        - |   974 | `	"}"\` |
|        - |   975 | `	"/* Directory releated IO */"\` |
|        - |   976 | `	"class Directory {"\` |
|        - |   977 | `	"public $handle = null;"\` |
|        - |   978 | `	"public $path  = null;"\` |
|        - |   979 | `	"public function __construct(string $path)"\` |
|        - |   980 | `	"{"\` |
|        - |   981 | `	"   $this->handle = opendir($path);"\` |
|        - |   982 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   983 | `	"      $this->path = $path;"\` |
|        - |   984 | `	"   }"\` |
|        - |   985 | `	"}"\` |
|        - |   986 | `	"public function __destruct()"\` |
|        - |   987 | `	"{"\` |
|        - |   988 | `	"  if( $this->handle != null ){"\` |
|        - |   989 | `	"       closedir($this->handle);"\` |
|        - |   990 | `	"  }"\` |
|        - |   991 | `	"}"\` |
|        - |   992 | `	"public function read()"\` |
|        - |   993 | `	"{"\` |
|        - |   994 | `	"    return readdir($this->handle);"\` |
|        - |   995 | `	"}"\` |
|        - |   996 | `	"public function rewind()"\` |
|        - |   997 | `	"{"\` |
|        - |   998 | `	"    rewinddir($this->handle);"\` |
|        - |   999 | `	"}"\` |
|        - |  1000 | `	"public function close()"\` |
|        - |  1001 | `	"{"\` |
|        - |  1002 | `	"    closedir($this->handle);"\` |
|        - |  1003 | `	"    $this->handle = null;"\` |
|        - |  1004 | `	"}"\` |
|        - |  1005 | `	"}"\` |
|        - |  1006 | `	"class stdClass{"\` |
|        - |  1007 | `	"  public $value;"\` |
|        - |  1008 | `	" /* Magic methods */"\` |
|        - |  1009 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1010 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1011 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1012 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1013 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1014 | `	"}"\` |
|        - |  1015 | `	"function dir(string $path){"\` |
|        - |  1016 | `	"   return new Directory($path);"\` |
|        - |  1017 | `	"}"\` |
|        - |  1018 | `	"function Dir(string $path){"\` |
|        - |  1019 | `	"   return new Directory($path);"\` |
|        - |  1020 | `	"}"\` |
|        - |  1021 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1022 | `    "{"\` |
|        - |  1023 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1024 | `	"  $aDir = array();"\` |
|        - |  1025 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1026 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1027 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1028 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1029 | `	"   }"\` |
|        - |  1030 | `	"  closedir($pHandle);"\` |
|        - |  1031 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1032 | `	"      rsort($aDir);"\` |
|        - |  1033 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1034 | `	"      sort($aDir);"\` |
|        - |  1035 | `	"  }"\` |
|        - |  1036 | `	"  return $aDir;"\` |
|        - |  1037 | `	"}"\` |
|        - |  1038 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1039 | `	"/* Open the target directory */"\` |
|        - |  1040 | `	"$zDir = dirname($pattern);"\` |
|        - |  1041 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1042 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1043 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1044 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1045 | `	"	return FALSE;"\` |
|        - |  1046 | `	"}"\` |
|        - |  1047 | `	"$pattern = basename($pattern);"\` |
|        - |  1048 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1049 | `	"/* Loop throw available entries */"\` |
|        - |  1050 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1051 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1052 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1053 | `	"	if( $rc ){"\` |
|        - |  1054 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1055 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1056 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1057 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1058 | `	"		  }"\` |
|        - |  1059 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1060 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1061 | `	"		 continue;"\` |
|        - |  1062 | `	"	   }"\` |
|        - |  1063 | `	"	   /* Add the entry */"\` |
|        - |  1064 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1065 | `	"	}"\` |
|        - |  1066 | `	" }"\` |
|        - |  1067 | `	"/* Close the handle */"\` |
|        - |  1068 | `	"closedir($pHandle);"\` |
|        - |  1069 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1070 | `	"  /* Sort the array */"\` |
|        - |  1071 | `	"  sort($pArray);"\` |
|        - |  1072 | `	"}"\` |
|        - |  1073 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1074 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1075 | `	"  $pArray[] = $pattern;"\` |
|        - |  1076 | `	"}"\` |
|        - |  1077 | `	"/* Return the created array */"\` |
|        - |  1078 | `	"return $pArray;"\` |
|        - |  1079 | `   "}"\` |
|        - |  1080 | `   "/* Creates a temporary file */"\` |
|        - |  1081 | `   "function tmpfile(){"\` |
|        - |  1082 | `   "  /* Extract the temp directory */"\` |
|        - |  1083 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1084 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1085 | `   "    /* Use the current dir */"\` |
|        - |  1086 | `   "    $zTempDir = '.';"\` |
|        - |  1087 | `   "  }"\` |
|        - |  1088 | `   "  /* Create the file */"\` |
|        - |  1089 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1090 | `   "  return $pHandle;"\` |
|        - |  1091 | `   "}"\` |
|        - |  1092 | `   "/* Creates a temporary filename */"\` |
|        - |  1093 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1094 | `   "{"\` |
|        - |  1095 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1096 | `   "}"\` |
|        - |  1097 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1098 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1099 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1100 | `   "/* Copy arguments */"\` |
|        - |  1101 | `   "$nArgs = func_num_args();"\` |
|        - |  1102 | `   "$pNew = array();"\` |
|        - |  1103 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1104 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1105 | `    "}"\` |
|        - |  1106 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1107 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1108 | `	"/* Erase */"\` |
|        - |  1109 | `	"array_erase($pArray);"\` |
|        - |  1110 | `	"/* Unshift */"\` |
|        - |  1111 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1112 | `	"return sizeof($pArray);"\` |
|        - |  1113 | `    "}"\` |
|        - |  1114 | `	"function array_merge_recursive(){"\` |
|        - |  1115 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1116 | `    "$arrays = func_get_args();"\` |
|        - |  1117 | `    "$narrays = count($arrays);"\` |
|        - |  1118 | `    "$ret = array();"\` |
|        - |  1119 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1120 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1121 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1122 | `	 " }"\` |
|        - |  1123 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1124 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1125 | `     "  if( $keyIsInt ) {"\` |
|        - |  1126 | `     "   $ret[] = $value;"\` |
|        - |  1127 | `     "  } else {"\` |
|        - |  1128 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1129 | `     "    $cur = $ret[$key];"\` |
|        - |  1130 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1131 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1132 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1133 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1134 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1135 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1136 | `     "    } else {"\` |
|        - |  1137 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1138 | `     "    }"\` |
|        - |  1139 | `     "   } else {"\` |
|        - |  1140 | `     "    $ret[$key] = $value;"\` |
|        - |  1141 | `     "   }"\` |
|        - |  1142 | `     "  }"\` |
|        - |  1143 | `     " }"\` |
|        - |  1144 | `	 " }"\` |
|        - |  1145 | `	 " return $ret;"\` |
|        - |  1146 | `    "}"\` |
|        - |  1147 | `	"function max(){"\` |
|        - |  1148 | `    "  $pArgs = func_get_args();"\` |
|        - |  1149 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1150 | `	"  return null;"\` |
|        - |  1151 | `    " }"\` |
|        - |  1152 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1153 | `    " $pArg = $pArgs[0];"\` |
|        - |  1154 | `	" if( !is_array($pArg) ){"\` |
|        - |  1155 | `	"   return $pArg; "\` |
|        - |  1156 | `	" }"\` |
|        - |  1157 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1158 | `	"   return null;"\` |
|        - |  1159 | `	" }"\` |
|        - |  1160 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1161 | `	" reset($pArg);"\` |
|        - |  1162 | `	" $max = current($pArg);"\` |
|        - |  1163 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1164 | `	"   if( $val > $max ){"\` |
|        - |  1165 | `	"     $max = $val;"\` |
|        - |  1166 | `    " }"\` |
|        - |  1167 | `	" }"\` |
|        - |  1168 | `	" return $max;"\` |
|        - |  1169 | `    " }"\` |
|        - |  1170 | `    " $max = $pArgs[0];"\` |
|        - |  1171 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1172 | `    " $val = $pArgs[$i];"\` |
|        - |  1173 | `	"if( $val > $max ){"\` |
|        - |  1174 | `	" $max = $val;"\` |
|        - |  1175 | `	"}"\` |
|        - |  1176 | `    " }"\` |
|        - |  1177 | `	" return $max;"\` |
|        - |  1178 | `    "}"\` |
|        - |  1179 | `	"function min(){"\` |
|        - |  1180 | `    "  $pArgs = func_get_args();"\` |
|        - |  1181 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1182 | `	"  return null;"\` |
|        - |  1183 | `    " }"\` |
|        - |  1184 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1185 | `    " $pArg = $pArgs[0];"\` |
|        - |  1186 | `	" if( !is_array($pArg) ){"\` |
|        - |  1187 | `	"   return $pArg; "\` |
|        - |  1188 | `	" }"\` |
|        - |  1189 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1190 | `	"   return null;"\` |
|        - |  1191 | `	" }"\` |
|        - |  1192 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1193 | `	" reset($pArg);"\` |
|        - |  1194 | `	" $min = current($pArg);"\` |
|        - |  1195 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1196 | `	"   if( $val < $min ){"\` |
|        - |  1197 | `	"     $min = $val;"\` |
|        - |  1198 | `    " }"\` |
|        - |  1199 | `	" }"\` |
|        - |  1200 | `	" return $min;"\` |
|        - |  1201 | `    " }"\` |
|        - |  1202 | `    " $min = $pArgs[0];"\` |
|        - |  1203 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1204 | `    " $val = $pArgs[$i];"\` |
|        - |  1205 | `	"if( $val < $min ){"\` |
|        - |  1206 | `	" $min = $val;"\` |
|        - |  1207 | `	" }"\` |
|        - |  1208 | `    " }"\` |
|        - |  1209 | `	" return $min;"\` |
|        - |  1210 | `	"}"\` |
|        - |  1211 | `	"function fileowner(string $file){"\` |
|        - |  1212 | `    " $a = stat($file);"\` |
|        - |  1213 | `	" if( !is_array($a) ){"\` |
|        - |  1214 | `	"	return false;"\` |
|        - |  1215 | `	" }"\` |
|        - |  1216 | `	" return $a['uid'];"\` |
|        - |  1217 | `    "}"\` |
|        - |  1218 | `    "function filegroup(string $file){"\` |
|        - |  1219 | `	" $a = stat($file);"\` |
|        - |  1220 | `	" if( !is_array($a) ){"\` |
|        - |  1221 | `	"	return false;"\` |
|        - |  1222 | `	" }"\` |
|        - |  1223 | `	" return $a['gid'];"\` |
|        - |  1224 | `    "}"\` |
|        - |  1225 | `	 "function fileinode(string $file){"\` |
|        - |  1226 | `	" $a = stat($file);"\` |
|        - |  1227 | `	" if( !is_array($a) ){"\` |
|        - |  1228 | `	"	return false;"\` |
|        - |  1229 | `	" }"\` |
|        - |  1230 | `	" return $a['ino'];"\` |
|        - |  1231 | `    "}"` |
|        - |  1232 |  |
|        - |  1233 | `/*` |
|        - |  1234 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1235 | ` * start compiling the target PHP program.` |
|        - |  1236 | ` */` |
|     2304 |  1237 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1238 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1239 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1240 | `	 )` |
|        2 |  1241 |  |
|        - |  1242 | `	SyString sBuiltin;` |
|        - |  1243 | `	ph7_value *pObj;` |
|        - |  1244 | `	sxi32 rc;` |
|        - |  1245 | `	/* Zero the structure */` |
|     2306 |  1246 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1247 | `	/* Initialize VM fields */` |
|     2306 |  1248 | `	pVm->pEngine = &(*pEngine);` |
|     2306 |  1249 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1250 | `	/* Instructions containers */` |
|     2306 |  1251 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2306 |  1252 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2306 |  1253 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1254 | `	/* Object containers */` |
|     2306 |  1255 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2306 |  1256 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1257 | `	/* Virtual machine internal containers */` |
|     2306 |  1258 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2306 |  1259 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2306 |  1260 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2306 |  1261 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2306 |  1262 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2306 |  1263 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2306 |  1264 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2306 |  1265 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2306 |  1266 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2306 |  1267 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2306 |  1268 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2306 |  1269 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2306 |  1270 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2306 |  1271 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2306 |  1272 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2306 |  1273 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2306 |  1274 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|        - |  1275 | `	/* Configuration containers */` |
|     2306 |  1276 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2306 |  1277 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2306 |  1278 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2306 |  1279 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2306 |  1280 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1281 | `	/* Error callbacks containers */` |
|     2306 |  1282 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2306 |  1283 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2306 |  1284 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2306 |  1285 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2306 |  1286 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1287 | `	/* Set a default recursion limit */` |
|        - |  1288 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2306 |  1289 | `	pVm->nMaxDepth = 32;` |
|        - |  1290 | `#else` |
|        - |  1291 | `	pVm->nMaxDepth = 16;` |
|        - |  1292 | `#endif` |
|        - |  1293 | `	/* Default assertion flags */` |
|     2306 |  1294 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1295 | `	/* JSON return status */` |
|     2306 |  1296 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1297 | `	/* PRNG context */` |
|     2306 |  1298 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1299 | `	/* Install the null constant */` |
|     2306 |  1300 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2306 |  1301 | `	if( pObj == 0 ){` |
|      ! 0 |  1302 | `		rc = SXERR_MEM;` |
|      ! 0 |  1303 | `		goto Err;` |
|        - |  1304 | `	}` |
|     2306 |  1305 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1306 | `	/* Install the boolean TRUE constant */` |
|     2306 |  1307 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2306 |  1308 | `	if( pObj == 0 ){` |
|      ! 0 |  1309 | `		rc = SXERR_MEM;` |
|      ! 0 |  1310 | `		goto Err;` |
|        - |  1311 | `	}` |
|     2306 |  1312 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1313 | `	/* Install the boolean FALSE constant */` |
|     2306 |  1314 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2306 |  1315 | `	if( pObj == 0 ){` |
|      ! 0 |  1316 | `		rc = SXERR_MEM;` |
|      ! 0 |  1317 | `		goto Err;` |
|        - |  1318 | `	}` |
|     2306 |  1319 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1320 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1321 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1322 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2306 |  1323 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2306 |  1324 | `	if( pObj == 0 ){` |
|      ! 0 |  1325 | `		rc = SXERR_MEM;` |
|      ! 0 |  1326 | `		goto Err;` |
|        - |  1327 | `	}` |
|     2306 |  1328 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1329 | `	/* Create the global frame */` |
|     2306 |  1330 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2306 |  1331 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1332 | `		goto Err;` |
|        - |  1333 | `	}` |
|        - |  1334 | `	/* Initialize the code generator */` |
|     2306 |  1335 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2306 |  1336 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1337 | `		goto Err;` |
|        - |  1338 | `	}` |
|        - |  1339 | `	/* VM correctly initialized,set the magic number */` |
|     2306 |  1340 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2306 |  1341 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1342 | `	/* Compile the built-in library */` |
|     2306 |  1343 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1344 | `	/* Reset the code generator */` |
|     2306 |  1345 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2306 |  1346 | `	return SXRET_OK;` |
|      ! 0 |  1347 | `Err:` |
|      ! 0 |  1348 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1349 | `	return rc;` |
|     1154 |  1350 |  |
|        - |  1351 | `/*` |
|        - |  1352 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1353 | ` * routine which store the output in an internal blob.` |
|        - |  1354 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1355 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1356 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1357 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1358 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1359 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1360 | ` * to finish executing and extracting the output.` |
|        - |  1361 | ` */` |
|      ! 0 |  1362 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1363 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1364 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1365 | `	void *pUserData     /* User private data */` |
|        - |  1366 | `	)` |
|      ! 0 |  1367 |  |
|        - |  1368 | `	 sxi32 rc;` |
|        - |  1369 | `	 /* Store the output in an internal BLOB */` |
|      ! 0 |  1370 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|      ! 0 |  1371 | `	 return rc;` |
|      ! 0 |  1372 |  |
|        - |  1373 | `#define VM_STACK_GUARD 16` |
|        - |  1374 | `/*` |
|        - |  1375 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1376 | ` * our compiled PHP program.` |
|        - |  1377 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1378 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1379 | ` */` |
|    29658 |  1380 | `static ph7_value * VmNewOperandStack(` |
|        - |  1381 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1382 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1383 | `	)` |
|        2 |  1384 |  |
|        - |  1385 | `	ph7_value *pStack;` |
|        - |  1386 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1387 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1388 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1389 | `  ** on the maximum stack depth required.` |
|        - |  1390 | `  **` |
|        - |  1391 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1392 | `  */` |
|    29660 |  1393 | `	nInstr += VM_STACK_GUARD;` |
|    29660 |  1394 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    29660 |  1395 | `	if( pStack == 0 ){` |
|      ! 0 |  1396 | `		return 0;` |
|        - |  1397 | `	}` |
|        - |  1398 | `	/* Initialize the operand stack */` |
|  1880160 |  1399 | `	while( nInstr > 0 ){` |
|  1850502 |  1400 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1850502 |  1401 | `		--nInstr;` |
|        2 |  1402 | `	}` |
|        - |  1403 | `	/* Ready for bytecode execution */` |
|    29660 |  1404 | `	return pStack;` |
|    14831 |  1405 |  |
|        - |  1406 | `/* Forward declaration */` |
|        - |  1407 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1408 | `/*` |
|        - |  1409 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1410 | ` * This routine gets called by the PH7 engine after` |
|        - |  1411 | ` * successful compilation of the target PHP program.` |
|        - |  1412 | ` */` |
|     2072 |  1413 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1414 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1415 | `	)` |
|        2 |  1416 |  |
|        - |  1417 | `	SyHashEntry *pEntry;` |
|        - |  1418 | `	sxi32 rc;` |
|     2074 |  1419 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1420 | `		/* Initialize your VM first */` |
|      ! 0 |  1421 | `		return SXERR_CORRUPT;` |
|        - |  1422 | `	}` |
|        - |  1423 | `	/* Mark the VM ready for byte-code execution */` |
|     2074 |  1424 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1425 | `	/* Release the code generator now we have compiled our program */` |
|     2074 |  1426 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1427 | `	/* Emit the DONE instruction */` |
|     2074 |  1428 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2074 |  1429 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1430 | `		return SXERR_MEM;` |
|        - |  1431 | `	}` |
|        - |  1432 | `	/* Script return value */` |
|     2074 |  1433 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1434 | `	/* Allocate a new operand stack */` |
|     2074 |  1435 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2074 |  1436 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1437 | `		return SXERR_MEM;` |
|        - |  1438 | `	}` |
|        - |  1439 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1440 | `	 * private data. */` |
|     2074 |  1441 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2074 |  1442 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1443 | `	/* Allocate the reference table */` |
|     2074 |  1444 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2074 |  1445 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2074 |  1446 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1447 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1448 | `		return SXERR_MEM;` |
|        - |  1449 | `	}` |
|        - |  1450 | `	/* Zero the reference table */` |
|     2074 |  1451 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1452 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2074 |  1453 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2074 |  1454 | `	if( rc != SXRET_OK ){` |
|        - |  1455 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1456 | `		return rc;` |
|        - |  1457 | `	}` |
|        - |  1458 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2074 |  1459 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2074 |  1460 | `	if( rc != SXRET_OK ){` |
|        - |  1461 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1462 | `		return rc;` |
|        - |  1463 | `	}` |
|        - |  1464 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2074 |  1465 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1466 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2074 |  1467 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1468 | `	/* Initialize and install static and constants class attributes */` |
|     2074 |  1469 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    27006 |  1470 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    24934 |  1471 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    24934 |  1472 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1473 | `			return rc;` |
|        - |  1474 | `		}` |
|        2 |  1475 | `	}` |
|        - |  1476 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2074 |  1477 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1478 | `	/* VM is ready for bytecode execution */` |
|     2074 |  1479 | `	return SXRET_OK;` |
|     1038 |  1480 |  |
|        - |  1481 | `/*` |
|        - |  1482 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1483 | ` */` |
|      ! 0 |  1484 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1485 |  |
|      ! 0 |  1486 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1487 | `		return SXERR_CORRUPT;` |
|        - |  1488 | `	}` |
|        - |  1489 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1490 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1491 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1492 | `	/* Set the ready flag */` |
|      ! 0 |  1493 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1494 | `	return SXRET_OK;` |
|      ! 0 |  1495 |  |
|        - |  1496 | `/*` |
|        - |  1497 | ` * Release a Virtual Machine.` |
|        - |  1498 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1499 | ` */` |
|     2064 |  1500 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1501 |  |
|        - |  1502 | `	/* Set the stale magic number */` |
|     2066 |  1503 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1504 | `	/* Release the private memory subsystem */` |
|     2066 |  1505 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2066 |  1506 | `	return SXRET_OK;` |
|        2 |  1507 |  |
|        - |  1508 | `/*` |
|        - |  1509 | ` * Initialize a foreign function call context.` |
|        - |  1510 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1511 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1512 | ` * functions.` |
|        - |  1513 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1514 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1515 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1516 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1517 | ` */` |
|   534958 |  1518 | `static sxi32 VmInitCallContext(` |
|        - |  1519 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1520 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1521 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1522 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1523 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1524 | `	)` |
|        2 |  1525 |  |
|   534960 |  1526 | `	pOut->pFunc = pFunc;` |
|   534960 |  1527 | `	pOut->pVm   = pVm;` |
|   534960 |  1528 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   534960 |  1529 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1530 | `	/* Assume a null return value */` |
|   534960 |  1531 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   534960 |  1532 | `	pOut->pRet = pRet;` |
|   534960 |  1533 | `	pOut->iFlags = iFlags;` |
|   534960 |  1534 | `	return SXRET_OK;` |
|        2 |  1535 |  |
|        - |  1536 | `/*` |
|        - |  1537 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1538 | ` * left behind.` |
|        - |  1539 | ` */` |
|   534958 |  1540 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1541 |  |
|        - |  1542 | `	sxu32 n;` |
|   534960 |  1543 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6560 |  1544 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    18684 |  1545 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    12126 |  1546 | `			if( apObj[n] == 0 ){` |
|        - |  1547 | `				/* Already released */` |
|      250 |  1548 | `				continue;` |
|        - |  1549 | `			}` |
|    11878 |  1550 | `			PH7_MemObjRelease(apObj[n]);` |
|    11878 |  1551 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     5940 |  1552 | `		}` |
|     6560 |  1553 | `		SySetRelease(&pCtx->sVar);` |
|     3279 |  1554 | `	}` |
|   534960 |  1555 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1556 | `		ph7_aux_data *aAux;` |
|        - |  1557 | `		void *pChunk;` |
|        - |  1558 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1559 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1560 | `		 */` |
|        9 |  1561 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1562 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1563 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1564 | `			/* Release the chunk */` |
|       25 |  1565 | `			if( pChunk ){` |
|       25 |  1566 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1567 | `			}` |
|       13 |  1568 | `		}` |
|        9 |  1569 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1570 | `	}` |
|   534960 |  1571 |  |
|        - |  1572 | `/*` |
|        - |  1573 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1574 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1575 | ` */` |
|      248 |  1576 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1577 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1578 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1579 | `	)` |
|        2 |  1580 |  |
|      250 |  1581 | `	if( pValue == 0 ){` |
|        - |  1582 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1583 | `		return;` |
|        - |  1584 | `	}` |
|      250 |  1585 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      250 |  1586 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1587 | `		sxu32 n;` |
|      936 |  1588 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      936 |  1589 | `			if( apObj[n] == pValue ){` |
|      250 |  1590 | `				PH7_MemObjRelease(pValue);` |
|      250 |  1591 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1592 | `				/* Mark as released */` |
|      250 |  1593 | `				apObj[n] = 0;` |
|      250 |  1594 | `				break;` |
|        - |  1595 | `			}` |
|      345 |  1596 | `		}` |
|      124 |  1597 | `	}` |
|      126 |  1598 |  |
|        - |  1599 | `/*` |
|        - |  1600 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1601 | ` */` |
|  3142164 |  1602 | `static void VmPopOperand(` |
|        - |  1603 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1604 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1605 | `	)` |
|        2 |  1606 |  |
|  3142166 |  1607 | `	ph7_value *pTos = *ppTos;` |
|  6674406 |  1608 | `	while( nPop > 0 ){` |
|  3532242 |  1609 | `		PH7_MemObjRelease(pTos);` |
|  3532242 |  1610 | `		pTos--;` |
|  3532242 |  1611 | `		nPop--;` |
|        2 |  1612 | `	}` |
|        - |  1613 | `	/* Top of the stack */` |
|  3142166 |  1614 | `	*ppTos = pTos;` |
|  3142166 |  1615 |  |
|        - |  1616 | `/*` |
|        - |  1617 | ` * Reserve a memory object.` |
|        - |  1618 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1619 | ` */` |
|  2985566 |  1620 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1621 |  |
|  2985568 |  1622 | `	ph7_value *pObj = 0;` |
|        - |  1623 | `	VmSlot *pSlot;` |
|        - |  1624 | `	sxu32 nIdx;` |
|        - |  1625 | `	/* Check for a free slot */` |
|  2985568 |  1626 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  2985568 |  1627 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  2985568 |  1628 | `	if( pSlot ){` |
|   848278 |  1629 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   848278 |  1630 | `		nIdx = pSlot->nIdx;` |
|   424138 |  1631 | `	}` |
|  2985568 |  1632 | `	if( pObj == 0 ){` |
|        - |  1633 | `		/* Reserve a new memory object */` |
|  2137292 |  1634 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2137292 |  1635 | `		if( pObj == 0 ){` |
|      ! 0 |  1636 | `			return 0;` |
|        - |  1637 | `		}` |
|  1068645 |  1638 | `	}` |
|        - |  1639 | `	/* Set a null default value */` |
|  2985568 |  1640 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  2985568 |  1641 | `	pObj->nIdx = nIdx;` |
|  2985568 |  1642 | `	return pObj;` |
|  1492785 |  1643 |  |
|        - |  1644 | `/*` |
|        - |  1645 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1646 | ` */` |
|    26334 |  1647 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1648 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1649 | `	const char *zKey,  /* Entry key */` |
|        - |  1650 | `	sxu32 nByte,       /* Key length */` |
|        - |  1651 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1652 | `	)` |
|        2 |  1653 |  |
|        - |  1654 | `	ph7_value sKey;` |
|        - |  1655 | `	sxi32 rc;` |
|    26336 |  1656 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    26336 |  1657 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1658 | `	/* Perform the insertion */` |
|    26336 |  1659 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    26336 |  1660 | `	PH7_MemObjRelease(&sKey);` |
|    26336 |  1661 | `	return rc;` |
|        2 |  1662 |  |
|        - |  1663 | `/*` |
|        - |  1664 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1665 | ` * Return a pointer to the variable value on success.` |
|        - |  1666 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1667 | ` */` |
|  2941462 |  1668 | `static ph7_value * VmExtractMemObj(` |
|        - |  1669 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1670 | `	const SyString *pName, /* Variable name */` |
|        - |  1671 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1672 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1673 | `	)` |
|        2 |  1674 |  |
|  2941464 |  1675 | `	int bNullify = FALSE;` |
|        - |  1676 | `	SyHashEntry *pEntry;` |
|        - |  1677 | `	VmFrame *pFrame;` |
|        - |  1678 | `	ph7_value *pObj;` |
|        - |  1679 | `	sxu32 nIdx;` |
|        - |  1680 | `	sxi32 rc;` |
|        - |  1681 | `	/* Point to the top active frame */` |
|  2941464 |  1682 | `	pFrame = pVm->pFrame;` |
|  2941476 |  1683 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1684 | `		/* Safely ignore the exception frame */` |
|       13 |  1685 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        1 |  1686 | `	}` |
|        - |  1687 | `	/* Perform the lookup */` |
|  2941464 |  1688 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1689 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1690 | `		pName = &sAnnon;` |
|        - |  1691 | `		/* Always nullify the object */` |
|      ! 0 |  1692 | `		bNullify = TRUE;` |
|      ! 0 |  1693 | `		bDup = FALSE;` |
|      ! 0 |  1694 | `	}` |
|        - |  1695 | `	/* Check the superglobals table first */` |
|  2941464 |  1696 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  2941464 |  1697 | `	if( pEntry == 0 ){` |
|        - |  1698 | `		/* Query the top active frame */` |
|  2941428 |  1699 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  2941428 |  1700 | `		if( pEntry == 0 ){` |
|    79362 |  1701 | `			char *zName = (char *)pName->zString;` |
|        - |  1702 | `			VmSlot sLocal;` |
|    79362 |  1703 | `			if( !bCreate ){` |
|        - |  1704 | `				/* Do not create the variable,return NULL instead */` |
|      632 |  1705 | `				return 0;` |
|        - |  1706 | `			}` |
|        - |  1707 | `			/* No such variable,automatically create a new one and install` |
|        - |  1708 | `			 * it in the current frame.` |
|        - |  1709 | `			 */` |
|    78732 |  1710 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    78732 |  1711 | `			if( pObj == 0 ){` |
|      ! 0 |  1712 | `				return 0;` |
|        - |  1713 | `			}` |
|    78732 |  1714 | `			nIdx = pObj->nIdx;` |
|    78732 |  1715 | `			if( bDup ){` |
|        - |  1716 | `				/* Duplicate name */` |
|      164 |  1717 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      164 |  1718 | `				if( zName == 0 ){` |
|      ! 0 |  1719 | `					return 0;` |
|        - |  1720 | `				}` |
|       81 |  1721 | `			}` |
|        - |  1722 | `			/* Link to the top active VM frame */` |
|    78732 |  1723 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    78732 |  1724 | `			if( rc != SXRET_OK ){` |
|        - |  1725 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1726 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1727 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1728 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1729 | `				return 0;` |
|        - |  1730 | `			}` |
|    78732 |  1731 | `			if( pFrame->pParent != 0 ){` |
|        - |  1732 | `				/* Local variable */` |
|    73142 |  1733 | `				sLocal.nIdx = nIdx;` |
|    73142 |  1734 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    36572 |  1735 | `			}else{` |
|        - |  1736 | `				/* Register in the $GLOBALS array */` |
|     5592 |  1737 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1738 | `			}` |
|        - |  1739 | `			/* Install in the reference table */` |
|    78732 |  1740 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1741 | `			/* Save object index */` |
|    78732 |  1742 | `			pObj->nIdx = nIdx;` |
|    39367 |  1743 | `		}else{` |
|        - |  1744 | `			/* Extract variable contents */` |
|  2862068 |  1745 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  2862068 |  1746 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2862068 |  1747 | `			if( bNullify && pObj ){` |
|      ! 0 |  1748 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1749 | `			}` |
|        - |  1750 | `		}` |
|  1470510 |  1751 | `	}else{` |
|        - |  1752 | `		/* Superglobal */` |
|       38 |  1753 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1754 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1755 | `	}` |
|  2940834 |  1756 | `	return pObj;` |
|  1470843 |  1757 |  |
|        - |  1758 | `/*` |
|        - |  1759 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1760 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1761 | ` */` |
|     2098 |  1762 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1763 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1764 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1765 | `	sxu32 nByte        /* zName length */` |
|        - |  1766 | `	)` |
|        2 |  1767 |  |
|        - |  1768 | `	SyHashEntry *pEntry;` |
|        - |  1769 | `	ph7_value *pValue;` |
|        - |  1770 | `	sxu32 nIdx;` |
|        - |  1771 | `	/* Query the superglobal table */` |
|     2100 |  1772 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2100 |  1773 | `	if( pEntry == 0 ){` |
|        - |  1774 | `		/* No such entry */` |
|      ! 0 |  1775 | `		return 0;` |
|        - |  1776 | `	}` |
|        - |  1777 | `	/* Extract the superglobal index in the global object pool */` |
|     2100 |  1778 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1779 | `	/* Extract the variable value  */` |
|     2100 |  1780 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2100 |  1781 | `	return pValue;` |
|     1051 |  1782 |  |
|        - |  1783 | `/*` |
|        - |  1784 | ` * Perform a raw hashmap insertion.` |
|        - |  1785 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1786 | ` */` |
|     2096 |  1787 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1788 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1789 | `	const char *zKey,   /* Entry key */` |
|        - |  1790 | `	int nKeylen,        /* zKey length*/` |
|        - |  1791 | `	const char *zData,  /* Entry data */` |
|        - |  1792 | `	int nLen            /* zData length */` |
|        - |  1793 | `	)` |
|        2 |  1794 |  |
|        - |  1795 | `	ph7_value sKey,sValue;` |
|        - |  1796 | `	sxi32 rc;` |
|     2098 |  1797 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2098 |  1798 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2098 |  1799 | `	if( zKey ){` |
|     2076 |  1800 | `		if( nKeylen < 0 ){` |
|     2076 |  1801 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1037 |  1802 | `		}` |
|     2076 |  1803 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1037 |  1804 | `	}` |
|     2098 |  1805 | `	if( zData ){` |
|     2098 |  1806 | `		if( nLen < 0 ){` |
|        - |  1807 | `			/* Compute length automatically */` |
|      ! 0 |  1808 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1809 | `		}` |
|     2098 |  1810 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1048 |  1811 | `	}` |
|        - |  1812 | `	/* Perform the insertion */` |
|     2098 |  1813 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2098 |  1814 | `	PH7_MemObjRelease(&sKey);` |
|     2098 |  1815 | `	PH7_MemObjRelease(&sValue);` |
|     2098 |  1816 | `	return rc;` |
|        2 |  1817 |  |
|        - |  1818 | `/*` |
|        - |  1819 | ` * Configure a working virtual machine instance.` |
|        - |  1820 | ` *` |
|        - |  1821 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1822 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1823 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1824 | ` * The second argument to this function is an integer configuration option` |
|        - |  1825 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1826 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1827 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1828 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1829 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1830 | ` */` |
|    33176 |  1831 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1832 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1833 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1834 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1835 | `	)` |
|        2 |  1836 |  |
|    33178 |  1837 | `	sxi32 rc = SXRET_OK;` |
|    33178 |  1838 | `	switch(nOp){` |
|     1036 |  1839 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2074 |  1840 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2074 |  1841 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1842 | `		/* VM output consumer callback */` |
|        - |  1843 | `#ifdef UNTRUST` |
|        - |  1844 | `		if( xConsumer == 0 ){` |
|        - |  1845 | `			rc = SXERR_CORRUPT;` |
|        - |  1846 | `			break;` |
|        - |  1847 | `		}` |
|        - |  1848 | `#endif` |
|        - |  1849 | `		/* Install the output consumer */` |
|     2074 |  1850 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2074 |  1851 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2074 |  1852 | `		break;` |
|        - |  1853 | `							   }` |
|     1036 |  1854 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1855 | `		/* Import path */` |
|        - |  1856 | `		  const char *zPath;` |
|        - |  1857 | `		  SyString sPath;` |
|     2074 |  1858 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1859 | `#if defined(UNTRUST)` |
|        - |  1860 | `		  if( zPath == 0 ){` |
|        - |  1861 | `			  rc = SXERR_EMPTY;` |
|        - |  1862 | `			  break;` |
|        - |  1863 | `		  }` |
|        - |  1864 | `#endif` |
|     2074 |  1865 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1866 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1867 | `#ifdef __WINNT__` |
|        2 |  1868 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1869 | `#endif` |
|     4146 |  1870 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1871 | `		  /* Remove leading and trailing white spaces */` |
|     2074 |  1872 | `		  SyStringFullTrim(&sPath);` |
|     2074 |  1873 | `		  if( sPath.nByte > 0 ){` |
|        - |  1874 | `			  /* Store the path in the corresponding conatiner */` |
|     2074 |  1875 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1036 |  1876 | `		  }` |
|     2074 |  1877 | `		  break;` |
|        - |  1878 | `									 }` |
|     1036 |  1879 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1880 | `		/* Run-Time Error report */` |
|     2074 |  1881 | `		pVm->bErrReport = 1;` |
|     2074 |  1882 | `		break;` |
|      ! 0 |  1883 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1884 | `		/* Recursion depth */` |
|      ! 0 |  1885 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1886 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1887 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1888 | `		}` |
|      ! 0 |  1889 | `		break;` |
|        - |  1890 | `									   }` |
|      ! 0 |  1891 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1892 | `		/* VM output length in bytes */` |
|      ! 0 |  1893 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1894 | `#ifdef UNTRUST` |
|        - |  1895 | `		if( pOut == 0 ){` |
|        - |  1896 | `			rc = SXERR_CORRUPT;` |
|        - |  1897 | `			break;` |
|        - |  1898 | `		}` |
|        - |  1899 | `#endif` |
|      ! 0 |  1900 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1901 | `		break;` |
|        - |  1902 | `							   }` |
|        - |  1903 |  |
|    10360 |  1904 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1905 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1906 | `		/* Create a new superglobal/global variable */` |
|    20722 |  1907 | `		const char *zName = va_arg(ap,const char *);` |
|    20722 |  1908 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1909 | `		SyHashEntry *pEntry;` |
|        - |  1910 | `		ph7_value *pObj;` |
|        - |  1911 | `		sxu32 nByte;` |
|        - |  1912 | `		sxu32 nIdx;` |
|        - |  1913 | `#ifdef UNTRUST` |
|        - |  1914 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1915 | `			rc = SXERR_CORRUPT;` |
|        - |  1916 | `			break;` |
|        - |  1917 | `		}` |
|        - |  1918 | `#endif` |
|    20722 |  1919 | `		nByte = SyStrlen(zName);` |
|    20722 |  1920 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1921 | `			/* Check if the superglobal is already installed */` |
|    20722 |  1922 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    10362 |  1923 | `		}else{` |
|        - |  1924 | `			/* Query the top active VM frame */` |
|      ! 0 |  1925 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1926 | `		}` |
|    20722 |  1927 | `		if( pEntry ){` |
|        - |  1928 | `			/* Variable already installed */` |
|      ! 0 |  1929 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1930 | `			/* Extract contents */` |
|      ! 0 |  1931 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  1932 | `			if( pObj ){` |
|        - |  1933 | `				/* Overwrite old contents */` |
|      ! 0 |  1934 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  1935 | `			}` |
|      ! 0 |  1936 | `		}else{` |
|        - |  1937 | `			/* Install a new variable */` |
|    20722 |  1938 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    20722 |  1939 | `			if( pObj == 0 ){` |
|      ! 0 |  1940 | `				rc = SXERR_MEM;` |
|      ! 0 |  1941 | `				break;` |
|        - |  1942 | `			}` |
|    20722 |  1943 | `			nIdx = pObj->nIdx;` |
|        - |  1944 | `			/* Copy value */` |
|    20722 |  1945 | `			PH7_MemObjStore(pValue,pObj);` |
|    20722 |  1946 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1947 | `				/* Install the superglobal */` |
|    20722 |  1948 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    10362 |  1949 | `			}else{` |
|        - |  1950 | `				/* Install in the current frame */` |
|      ! 0 |  1951 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1952 | `			}` |
|    20722 |  1953 | `			if( rc == SXRET_OK ){` |
|        - |  1954 | `				SyHashEntry *pRef;` |
|    20722 |  1955 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    20722 |  1956 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    10362 |  1957 | `				}else{` |
|      ! 0 |  1958 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1959 | `				}` |
|        - |  1960 | `				/* Install in the reference table */` |
|    20722 |  1961 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    20722 |  1962 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1963 | `					/* Register in the $GLOBALS array */` |
|    20722 |  1964 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    10360 |  1965 | `				}` |
|    10360 |  1966 | `			}` |
|        - |  1967 | `		}` |
|    20722 |  1968 | `		break;` |
|        - |  1969 | `									}` |
|     1037 |  1970 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1971 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1972 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1973 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1974 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1975 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1976 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2076 |  1977 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2076 |  1978 | `		const char *zValue = va_arg(ap,const char *);` |
|     2076 |  1979 | `		int nLen = va_arg(ap,int);` |
|        - |  1980 | `		ph7_hashmap *pMap;` |
|        - |  1981 | `		ph7_value *pValue;` |
|     2076 |  1982 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1983 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1984 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2075 |  1985 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  1986 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  1987 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2074 |  1988 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  1989 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  1990 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2074 |  1991 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  1992 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  1993 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2074 |  1994 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  1995 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  1996 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2074 |  1997 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  1998 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  1999 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2000 | `		}else{` |
|        - |  2001 | `			/* Extract the $_SERVER superglobal */` |
|     2074 |  2002 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2003 | `		}` |
|     2076 |  2004 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2005 | `			/* No such entry */` |
|      ! 0 |  2006 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2007 | `			break;` |
|        - |  2008 | `		}` |
|        - |  2009 | `		/* Point to the hashmap */` |
|     2076 |  2010 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2011 | `		/* Perform the insertion */` |
|     2076 |  2012 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2076 |  2013 | `		break;` |
|        - |  2014 | `								   }` |
|       11 |  2015 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2016 | `		/* Script arguments */` |
|       24 |  2017 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2018 | `		ph7_hashmap *pMap;` |
|        - |  2019 | `		ph7_value *pValue;` |
|        - |  2020 | `		sxu32 n;` |
|       24 |  2021 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2022 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2023 | `			break;` |
|        - |  2024 | `		}` |
|        - |  2025 | `		/* Extract the $argv array */` |
|       24 |  2026 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2027 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2028 | `			/* No such entry */` |
|      ! 0 |  2029 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2030 | `			break;` |
|        - |  2031 | `		}` |
|        - |  2032 | `		/* Point to the hashmap */` |
|       24 |  2033 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2034 | `		/* Perform the insertion */` |
|       24 |  2035 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2036 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2037 | `		if( rc == SXRET_OK ){` |
|       24 |  2038 | `			if( pMap->nEntry > 1 ){` |
|        - |  2039 | `				/* Append space separator first */` |
|       18 |  2040 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2041 | `			}` |
|       24 |  2042 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2043 | `		}` |
|       24 |  2044 | `		break;` |
|        - |  2045 | `								  }` |
|      ! 0 |  2046 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2047 | `		/* error_log() consumer */` |
|      ! 0 |  2048 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2049 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2050 | `		break;` |
|        - |  2051 | `										}` |
|      ! 0 |  2052 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2053 | `		/* Script return value */` |
|      ! 0 |  2054 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2055 | `#ifdef UNTRUST` |
|        - |  2056 | `		if( ppValue == 0 ){` |
|        - |  2057 | `			rc = SXERR_CORRUPT;` |
|        - |  2058 | `			break;` |
|        - |  2059 | `		}` |
|        - |  2060 | `#endif` |
|      ! 0 |  2061 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2062 | `		break;` |
|        - |  2063 | `								   }` |
|     2072 |  2064 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2065 | `		/* Register an IO stream device */` |
|     4146 |  2066 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2067 | `		/* Make sure we are dealing with a valid IO stream */` |
|     6216 |  2068 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4146 |  2069 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2070 | `				/* Invalid stream */` |
|      ! 0 |  2071 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2072 | `				break;` |
|        - |  2073 | `		}` |
|     4146 |  2074 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2075 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2074 |  2076 | `			pVm->pDefStream = pStream;` |
|     1036 |  2077 | `		}` |
|        - |  2078 | `		/* Insert in the appropriate container */` |
|     4146 |  2079 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4146 |  2080 | `		break;` |
|        - |  2081 | `								  }` |
|      ! 0 |  2082 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2083 | `		/* Point to the VM internal output consumer buffer */` |
|      ! 0 |  2084 | `		const void **ppOut = va_arg(ap,const void **);` |
|      ! 0 |  2085 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2086 | `#ifdef UNTRUST` |
|        - |  2087 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2088 | `			rc = SXERR_CORRUPT;` |
|        - |  2089 | `			break;` |
|        - |  2090 | `		}` |
|        - |  2091 | `#endif` |
|      ! 0 |  2092 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|      ! 0 |  2093 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|      ! 0 |  2094 | `		break;` |
|        - |  2095 | `									   }` |
|      ! 0 |  2096 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2097 | `		/* Raw HTTP request*/` |
|      ! 0 |  2098 | `		const char *zRequest = va_arg(ap,const char *);` |
|      ! 0 |  2099 | `		int nByte = va_arg(ap,int);` |
|      ! 0 |  2100 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2101 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2102 | `			break;` |
|        - |  2103 | `		}` |
|      ! 0 |  2104 | `		if( nByte < 0 ){` |
|        - |  2105 | `			/* Compute length automatically */` |
|      ! 0 |  2106 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2107 | `		}` |
|        - |  2108 | `		/* Process the request */` |
|      ! 0 |  2109 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|      ! 0 |  2110 | `		break;` |
|        - |  2111 | `									}` |
|      ! 0 |  2112 | `	default:` |
|        - |  2113 | `		/* Unknown configuration option */` |
|      ! 0 |  2114 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2115 | `		break;` |
|        - |  2116 | `	}` |
|    33178 |  2117 | `	return rc;` |
|        2 |  2118 |  |
|        - |  2119 | `/* Forward declaration */` |
|        - |  2120 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2121 | `/*` |
|        - |  2122 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2123 | ` * format.` |
|        - |  2124 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2125 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2126 | ` * (STDOUT).` |
|        - |  2127 | ` */` |
|        2 |  2128 | `static sxi32 VmByteCodeDump(` |
|        - |  2129 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2130 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2131 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2132 | `	)` |
|        1 |  2133 |  |
|        - |  2134 | `	static const char zDump[] = {` |
|        - |  2135 | `		"====================================================\n"` |
|        - |  2136 | `		"PH7 VM Dump\n"` |
|        - |  2137 | `		"====================================================\n"` |
|        - |  2138 | `	};` |
|        - |  2139 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2140 | `	sxi32 rc = SXRET_OK;` |
|        - |  2141 | `	sxu32 n;` |
|        - |  2142 | `	/* Point to the PH7 instructions */` |
|        3 |  2143 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2144 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2145 | `	n = 0;` |
|        3 |  2146 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2147 | `	/* Dump instructions */` |
|        6 |  2148 | `	for(;;){` |
|       13 |  2149 | `		if( pInstr >= pEnd ){` |
|        - |  2150 | `			/* No more instructions */` |
|        3 |  2151 | `			break;` |
|        - |  2152 | `		}` |
|        - |  2153 | `		/* Format and call the consumer callback */` |
|       16 |  2154 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       10 |  2155 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       10 |  2156 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       11 |  2157 | `		if( rc != SXRET_OK ){` |
|        - |  2158 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2159 | `			return rc;` |
|        - |  2160 | `		}` |
|       11 |  2161 | `		++n;` |
|       11 |  2162 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2163 | `	}` |
|        3 |  2164 | `	return rc;` |
|        2 |  2165 |  |
|        - |  2166 | `/* Forward declaration */` |
|        - |  2167 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2168 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2169 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2170 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2171 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2172 | `/*` |
|        - |  2173 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2174 | ` * consumer callback.` |
|        - |  2175 | ` */` |
|      542 |  2176 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2177 |  |
|      543 |  2178 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      543 |  2179 | `	sxi32 rc = SXRET_OK;` |
|        - |  2180 | `	/* Append a new line */` |
|        - |  2181 | `#ifdef __WINNT__` |
|        1 |  2182 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2183 | `#else` |
|      542 |  2184 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2185 | `#endif` |
|        - |  2186 | `	/* Invoke the output consumer callback */` |
|      543 |  2187 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      543 |  2188 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  2189 | `		/* Increment output length */` |
|      543 |  2190 | `		pVm->nOutputLen += SyBlobLength(pMsg);` |
|      271 |  2191 | `	}` |
|      543 |  2192 | `	return rc;` |
|        1 |  2193 |  |
|        - |  2194 | `/*` |
|        - |  2195 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2196 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2197 | ` * information.` |
|        - |  2198 | ` */` |
|      130 |  2199 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2200 |  |
|      132 |  2201 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2202 | `		ph7_value apArg[4];` |
|        - |  2203 | `		ph7_value *apArgPtr[4];` |
|        - |  2204 | `		ph7_value sResult;` |
|        - |  2205 | `		SyString sErr;` |
|        - |  2206 | `		/* Prepare arguments */` |
|       61 |  2207 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2208 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2209 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2210 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2211 | `		if( pFile ){` |
|       61 |  2212 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2213 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2214 | `		}else{` |
|      ! 0 |  2215 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2216 | `		}` |
|       61 |  2217 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2218 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2219 | `		/* Set up pointer array */` |
|       61 |  2220 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2221 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2222 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2223 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2224 | `		/* Call the handler */` |
|       61 |  2225 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2226 | `		/* Check return value */` |
|       61 |  2227 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2228 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2229 | `		}` |
|        - |  2230 | `		/* Release */` |
|       61 |  2231 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2232 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2233 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2234 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2235 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2236 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2237 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2238 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2239 | `	}` |
|        - |  2240 | `	/* No handler, always call error handler */` |
|       71 |  2241 | `	return TRUE;` |
|       67 |  2242 |  |
|       94 |  2243 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2244 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2245 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2246 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2247 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2248 | `	)` |
|        2 |  2249 |  |
|       96 |  2250 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2251 | `	SyString *pFile;` |
|        - |  2252 | `	char *zErr;` |
|       96 |  2253 | `	sxi32 rc = SXRET_OK;` |
|       96 |  2254 | `	if( !pVm->bErrReport ){` |
|        - |  2255 | `		/* Don't bother reporting errors */` |
|        3 |  2256 | `		return SXRET_OK;` |
|        - |  2257 | `	}` |
|        - |  2258 | `	/* Reset the working buffer */` |
|       94 |  2259 | `	SyBlobReset(pWorker);` |
|        - |  2260 | `	/* Peek the processed file if available */` |
|       94 |  2261 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       94 |  2262 | `	if( pFile ){` |
|        - |  2263 | `		/* Append file name */` |
|       94 |  2264 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       94 |  2265 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       46 |  2266 | `	}` |
|        - |  2267 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2268 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2269 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2270 | `	 * E_DEPRECATED). */` |
|       94 |  2271 | `	zErr = "Error:  ";` |
|       94 |  2272 | `	switch(iErr){` |
|       17 |  2273 | `	case PH7_CTX_WARNING:` |
|       36 |  2274 | `		zErr = "Warning:  ";` |
|       36 |  2275 | `		break;` |
|        6 |  2276 | `	case PH7_CTX_NOTICE:` |
|       14 |  2277 | `		zErr = "Notice:  ";` |
|       12 |  2278 | `		break;` |
|       23 |  2279 | `	default:` |
|        - |  2280 | `		/* keep iErr unchanged */` |
|       46 |  2281 | `		break;` |
|        - |  2282 | `	}` |
|       94 |  2283 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       94 |  2284 | `	if( pFuncName ){` |
|        - |  2285 | `		/* Append function name first */` |
|       21 |  2286 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       21 |  2287 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       10 |  2288 | `	}` |
|       94 |  2289 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2290 | `	/* Check for user error handler.  compute length of C string */` |
|       94 |  2291 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       45 |  2292 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       22 |  2293 | `	}` |
|       94 |  2294 | `	return rc;` |
|       49 |  2295 |  |
|        - |  2296 | `/*` |
|        - |  2297 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2298 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2299 | ` * information.` |
|        - |  2300 | ` */` |
|       38 |  2301 | `static sxi32 VmThrowErrorAp(` |
|        - |  2302 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2303 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2304 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2305 | `	const char *zFormat, /* Format message */` |
|        - |  2306 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2307 | `	)` |
|        2 |  2308 |  |
|       40 |  2309 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2310 | `	SyBlob sMsg;` |
|        - |  2311 | `	SyString *pFile;` |
|        - |  2312 | `	char *zErr;` |
|       40 |  2313 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2314 | `	if( !pVm->bErrReport ){` |
|        - |  2315 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2316 | `		return SXRET_OK;` |
|        - |  2317 | `	}` |
|        - |  2318 | `	/* Reset the working buffer */` |
|       40 |  2319 | `	SyBlobReset(pWorker);` |
|        - |  2320 | `	/* Peek the processed file if available */` |
|       40 |  2321 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2322 | `	if( pFile ){` |
|        - |  2323 | `		/* Append file name */` |
|       40 |  2324 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2325 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2326 | `	}` |
|        - |  2327 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2328 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2329 | `	 * the correct errno value. */` |
|       40 |  2330 | `	zErr = "Error:  ";` |
|       40 |  2331 | `	switch(iErr){` |
|        4 |  2332 | `	case PH7_CTX_WARNING:` |
|        9 |  2333 | `		zErr = "Warning:  ";` |
|        9 |  2334 | `		break;` |
|        3 |  2335 | `	case PH7_CTX_NOTICE:` |
|        7 |  2336 | `		zErr = "Notice:  ";` |
|        6 |  2337 | `		break;` |
|       12 |  2338 | `	default:` |
|        - |  2339 | `		/* do not change iErr */` |
|       24 |  2340 | `		break;` |
|        - |  2341 | `	}` |
|       40 |  2342 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2343 | `	if( pFuncName ){` |
|        - |  2344 | `		/* Append function name first */` |
|       26 |  2345 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2346 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2347 | `	}` |
|        - |  2348 | `	/* Format the raw message */` |
|       40 |  2349 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2350 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2351 | `	/* Check if a user error handler is installed */` |
|       40 |  2352 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2353 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2354 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2355 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2356 | `	}` |
|       40 |  2357 | `	SyBlobRelease(&sMsg);` |
|       40 |  2358 | `	return rc;` |
|       21 |  2359 |  |
|        - |  2360 | `/*` |
|        - |  2361 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2362 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2363 | ` * information.` |
|        - |  2364 | ` * ------------------------------------` |
|        - |  2365 | ` * Simple boring wrapper function.` |
|        - |  2366 | ` * ------------------------------------` |
|        - |  2367 | ` */` |
|       14 |  2368 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2369 |  |
|        - |  2370 | `	va_list ap;` |
|        - |  2371 | `	sxi32 rc;` |
|       15 |  2372 | `	va_start(ap,zFormat);` |
|       15 |  2373 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2374 | `	va_end(ap);` |
|       15 |  2375 | `	return rc;` |
|        1 |  2376 |  |
|        - |  2377 | `/*` |
|        - |  2378 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2379 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2380 | ` * information.` |
|        - |  2381 | ` * ------------------------------------` |
|        - |  2382 | ` * Simple boring wrapper function.` |
|        - |  2383 | ` * ------------------------------------` |
|        - |  2384 | ` */` |
|       24 |  2385 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2386 |  |
|        - |  2387 | `	sxi32 rc;` |
|       26 |  2388 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2389 | `	return rc;` |
|        2 |  2390 |  |
|        - |  2391 | `/*` |
|        - |  2392 | ` * Resolve function context from the current frame.` |
|        - |  2393 | ` */` |
|      934 |  2394 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2395 |  |
|        - |  2396 | `	VmFrame *pFrame;` |
|        - |  2397 | `	ph7_vm_func *pFunc;` |
|      935 |  2398 | `	*pzFuncName = 0;` |
|      935 |  2399 | `	*pnFuncLen = 0;` |
|      935 |  2400 | `	pFrame = pVm->pFrame;` |
|      935 |  2401 | `	if( pFrame == 0 ){` |
|      ! 0 |  2402 | `		return;` |
|        - |  2403 | `	}` |
|      935 |  2404 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  2405 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  2406 | `	}` |
|      935 |  2407 | `	if( pFrame->pParent == 0 ){` |
|      929 |  2408 | `		return;` |
|        - |  2409 | `	}` |
|        7 |  2410 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        7 |  2411 | `	if( pFunc == 0 ){` |
|      ! 0 |  2412 | `		return;` |
|        - |  2413 | `	}` |
|        7 |  2414 | `	*pzFuncName = pFunc->sName.zString;` |
|        7 |  2415 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      468 |  2416 |  |
|        - |  2417 | `/*` |
|        - |  2418 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2419 | ` */` |
|      470 |  2420 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2421 |  |
|        - |  2422 | `	SyBlob sOut;` |
|        - |  2423 | `	SyString *pFile;` |
|      471 |  2424 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2425 | `		return PH7_OK;` |
|        - |  2426 | `	}` |
|      471 |  2427 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2428 | `		zClass = "Exception";` |
|      ! 0 |  2429 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2430 | `	}` |
|      471 |  2431 | `	if( zMsg == 0 ){` |
|      ! 0 |  2432 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2433 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2434 | `	}` |
|      471 |  2435 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      465 |  2436 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      232 |  2437 | `	}` |
|      471 |  2438 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      471 |  2439 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      471 |  2440 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      471 |  2441 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      471 |  2442 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      471 |  2443 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      471 |  2444 | `	if( pFile ){` |
|      471 |  2445 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      471 |  2446 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2447 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      235 |  2448 | `	}` |
|      471 |  2449 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      471 |  2450 | `	if( pFile ){` |
|      471 |  2451 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      471 |  2452 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2453 | `		if( zFuncName && nFuncLen > 0 ){` |
|        7 |  2454 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        4 |  2455 | `		}else{` |
|      465 |  2456 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2457 | `		}` |
|      235 |  2458 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2459 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2460 | `	}else{` |
|      ! 0 |  2461 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2462 | `	}` |
|      471 |  2463 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      471 |  2464 | `	if( pFile ){` |
|      471 |  2465 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      471 |  2466 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      471 |  2467 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2468 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      235 |  2469 | `	}` |
|      471 |  2470 | `	VmCallErrorHandler(pVm,&sOut);` |
|      471 |  2471 | `	SyBlobRelease(&sOut);` |
|      471 |  2472 | `	return PH7_ABORT;` |
|      236 |  2473 |  |
|        - |  2474 | `/*` |
|        - |  2475 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2476 | ` */` |
|      468 |  2477 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2478 |  |
|        - |  2479 | `	ph7_vm *pVm;` |
|        - |  2480 | `	ph7_class *pClass;` |
|        - |  2481 | `	ph7_class_instance *pThis;` |
|        - |  2482 | `	ph7_class_method *pCons;` |
|        - |  2483 | `	ph7_value sArg;` |
|        - |  2484 | `	ph7_value *apArg[1];` |
|        - |  2485 | `	SyBlob sMsg;` |
|        - |  2486 | `	SyString sMsgStr;` |
|        - |  2487 | `	VmFrame *pFrame;` |
|        - |  2488 | `	va_list ap;` |
|        - |  2489 | `	sxi32 rc;` |
|        - |  2490 |  |
|      470 |  2491 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2492 | `		return PH7_ABORT;` |
|        - |  2493 | `	}` |
|      470 |  2494 | `	pVm = pCtx->pVm;` |
|      470 |  2495 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2496 | `		zClass = "Error";` |
|      ! 0 |  2497 | `	}` |
|      470 |  2498 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      470 |  2499 | `	if( pClass == 0 ){` |
|      ! 0 |  2500 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2501 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2502 | `			zClass` |
|        - |  2503 | `			);` |
|        - |  2504 | `	}` |
|      470 |  2505 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      470 |  2506 | `	if( pThis == 0 ){` |
|      ! 0 |  2507 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2508 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2509 | `			);` |
|        - |  2510 | `	}` |
|        - |  2511 |  |
|      470 |  2512 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      470 |  2513 | `	va_start(ap,zFormat);` |
|      470 |  2514 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      470 |  2515 | `	va_end(ap);` |
|        - |  2516 |  |
|      470 |  2517 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      470 |  2518 | `	if( pCons ){` |
|      470 |  2519 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      470 |  2520 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      470 |  2521 | `		apArg[0] = &sArg;` |
|      470 |  2522 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      470 |  2523 | `		PH7_MemObjRelease(&sArg);` |
|      234 |  2524 | `	}` |
|      470 |  2525 | `	SyBlobRelease(&sMsg);` |
|        - |  2526 |  |
|      470 |  2527 | `	pFrame = pVm->pFrame;` |
|      470 |  2528 | `	if( pFrame ){` |
|      476 |  2529 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        7 |  2530 | `			pFrame = pFrame->pParent;` |
|        1 |  2531 | `		}` |
|      470 |  2532 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      234 |  2533 | `	}` |
|      470 |  2534 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      470 |  2535 | `	PH7_ClassInstanceUnref(pThis);` |
|      470 |  2536 | `	if( rc == SXERR_ABORT ){` |
|      463 |  2537 | `		return PH7_ABORT;` |
|        - |  2538 | `	}` |
|        7 |  2539 | `	return PH7_EXCEPTION;` |
|      236 |  2540 |  |
|        - |  2541 | `/*` |
|        - |  2542 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2543 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2544 | ` */` |
|      ! 0 |  2545 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2546 |  |
|        - |  2547 | `	ph7_vm *pVm;` |
|        - |  2548 | `	SyBlob sMsg;` |
|      ! 0 |  2549 | `	const char *zFuncName = 0;` |
|      ! 0 |  2550 | `	int nFuncLen = 0;` |
|        - |  2551 | `	va_list ap;` |
|        - |  2552 | `	sxi32 rc;` |
|        - |  2553 |  |
|      ! 0 |  2554 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2555 | `		return PH7_OK;` |
|        - |  2556 | `	}` |
|      ! 0 |  2557 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2558 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2559 | `		zClass = "Error";` |
|      ! 0 |  2560 | `	}` |
|        - |  2561 |  |
|      ! 0 |  2562 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2563 |  |
|      ! 0 |  2564 | `	va_start(ap,zFormat);` |
|      ! 0 |  2565 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2566 | `	va_end(ap);` |
|        - |  2567 |  |
|      ! 0 |  2568 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2569 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2570 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2571 | `	}` |
|      ! 0 |  2572 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2573 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2574 | `	}` |
|      ! 0 |  2575 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2576 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2577 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2578 | `	return rc;` |
|      ! 0 |  2579 |  |
|        - |  2580 | `/*` |
|        - |  2581 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2582 | ` *` |
|        - |  2583 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2584 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2585 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2586 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2587 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2588 | ` * then the program execution is halted.` |
|        - |  2589 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2590 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2591 | ` * or to reset the VM to it's initial state.` |
|        - |  2592 | ` */` |
|    29658 |  2593 | `static sxi32 VmByteCodeExec(` |
|        - |  2594 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2595 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2596 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2597 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2598 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2599 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2600 | `	int is_callback      /* TRUE if we are executing a callback */` |
|        - |  2601 | `	)` |
|        2 |  2602 |  |
|        - |  2603 | `	VmInstr *pInstr;` |
|        - |  2604 | `	ph7_value *pTos;` |
|        - |  2605 | `	SySet aArg;` |
|        - |  2606 | `	sxi32 pc;` |
|        - |  2607 | `	sxi32 rc;` |
|        - |  2608 | `	/* Argument container */` |
|    29660 |  2609 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    29660 |  2610 | `	if( nTos < 0 ){` |
|    28068 |  2611 | `		pTos = &pStack[-1];` |
|    14035 |  2612 | `	}else{` |
|     1594 |  2613 | `		pTos = &pStack[nTos];` |
|        - |  2614 | `	}` |
|    29660 |  2615 | `	pc = 0;` |
|        - |  2616 | `	/* Execute as much as we can */` |
|  4701290 |  2617 | `	for(;;){` |
|        - |  2618 | `		/* Fetch the instruction to execute */` |
|  9401878 |  2619 | `		pInstr = &aInstr[pc];` |
|  9401878 |  2620 | `		rc = SXRET_OK;` |
|        - |  2621 | `/*` |
|        - |  2622 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2623 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2624 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2625 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2626 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2627 | ` */` |
|  9401878 |  2628 | `		switch(pInstr->iOp){` |
|        - |  2629 | `/*` |
|        - |  2630 | ` * DONE: P1 * *` |
|        - |  2631 | ` *` |
|        - |  2632 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2633 | ` * and return immediately.` |
|        - |  2634 | ` */` |
|    14586 |  2635 | `case PH7_OP_DONE:` |
|    29174 |  2636 | `	if( pInstr->iP1 ){` |
|        - |  2637 | `#ifdef UNTRUST` |
|        - |  2638 | `		if( pTos < pStack ){` |
|        - |  2639 | `			goto Abort;` |
|        - |  2640 | `		}` |
|        - |  2641 | `#endif` |
|    16842 |  2642 | `		if( pLastRef ){` |
|    10890 |  2643 | `			*pLastRef = pTos->nIdx;` |
|     5444 |  2644 | `		}` |
|    16842 |  2645 | `		if( pResult ){` |
|        - |  2646 | `			/* Execution result */` |
|    16050 |  2647 | `			PH7_MemObjStore(pTos,pResult);` |
|     8024 |  2648 | `		}` |
|    16842 |  2649 | `		VmPopOperand(&pTos,1);` |
|    20754 |  2650 | `	}else if( pLastRef ){` |
|        - |  2651 | `		/* Nothing referenced */` |
|      882 |  2652 | `		*pLastRef = SXU32_HIGH;` |
|      440 |  2653 | `	}` |
|    29174 |  2654 | `	goto Done;` |
|        - |  2655 | `/*` |
|        - |  2656 | ` * HALT: P1 * *` |
|        - |  2657 | ` *` |
|        - |  2658 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2659 | ` * and abort immediately.` |
|        - |  2660 | ` */` |
|        4 |  2661 | `case PH7_OP_HALT:` |
|        9 |  2662 | `	if( pInstr->iP1 ){` |
|        - |  2663 | `#ifdef UNTRUST` |
|        - |  2664 | `		if( pTos < pStack ){` |
|        - |  2665 | `			goto Abort;` |
|        - |  2666 | `		}` |
|        - |  2667 | `#endif` |
|        9 |  2668 | `		if( pLastRef ){` |
|      ! 0 |  2669 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2670 | `		}` |
|        9 |  2671 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2672 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2673 | `				/* Output the exit message */` |
|        7 |  2674 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2675 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2676 | `				if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  2677 | `					/* Increment output length */` |
|        5 |  2678 | `					pVm->nOutputLen += SyBlobLength(&pTos->sBlob);` |
|        2 |  2679 | `				}` |
|        3 |  2680 | `			}` |
|        7 |  2681 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2682 | `			/* Record exit status */` |
|        5 |  2683 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2684 | `		}` |
|        9 |  2685 | `		VmPopOperand(&pTos,1);` |
|        4 |  2686 | `	}else if( pLastRef ){` |
|        - |  2687 | `		/* Nothing referenced */` |
|      ! 0 |  2688 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2689 | `	}` |
|        - |  2690 | `	/* Check if we're in an included file context */` |
|        9 |  2691 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2692 | `		/* Terminate the entire process */` |
|        9 |  2693 | `		exit(pVm->iExitStatus);` |
|        - |  2694 | `	}` |
|      ! 0 |  2695 | `	goto Abort;` |
|        - |  2696 | `/*` |
|        - |  2697 | ` * JMP: * P2 *` |
|        - |  2698 | ` *` |
|        - |  2699 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2700 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2701 | ` */` |
|   203763 |  2702 | `case PH7_OP_JMP:` |
|   407572 |  2703 | `	pc = pInstr->iP2 - 1;` |
|   407572 |  2704 | `	break;` |
|        - |  2705 | `/*` |
|        - |  2706 | ` * JZ: P1 P2 *` |
|        - |  2707 | ` *` |
|        - |  2708 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2709 | ` * entry in the stack if P1 is zero.` |
|        - |  2710 | ` */` |
|   473631 |  2711 | `case PH7_OP_JZ:` |
|        - |  2712 | `#ifdef UNTRUST` |
|        - |  2713 | `	if( pTos < pStack ){` |
|        - |  2714 | `		goto Abort;` |
|        - |  2715 | `	}` |
|        - |  2716 | `#endif` |
|        - |  2717 | `	/* Get a boolean value */` |
|   947352 |  2718 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      138 |  2719 | `		PH7_MemObjToBool(pTos);` |
|       68 |  2720 | `	}` |
|   947352 |  2721 | `	if( !pTos->x.iVal ){` |
|        - |  2722 | `		/* Take the jump */` |
|   473890 |  2723 | `		pc = pInstr->iP2 - 1;` |
|   236944 |  2724 | `	}` |
|   947352 |  2725 | `	if( !pInstr->iP1 ){` |
|   754788 |  2726 | `		VmPopOperand(&pTos,1);` |
|   377415 |  2727 | `	}` |
|   947352 |  2728 | `	break;` |
|        - |  2729 | `/*` |
|        - |  2730 | ` * JNZ: P1 P2 *` |
|        - |  2731 | ` *` |
|        - |  2732 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2733 | ` * entry in the stack if P1 is zero.` |
|        - |  2734 | ` */` |
|    51051 |  2735 | `case PH7_OP_JNZ:` |
|        - |  2736 | `#ifdef UNTRUST` |
|        - |  2737 | `	if( pTos < pStack ){` |
|        - |  2738 | `		goto Abort;` |
|        - |  2739 | `	}` |
|        - |  2740 | `#endif` |
|        - |  2741 | `	/* Get a boolean value */` |
|   102104 |  2742 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2743 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2744 | `	}` |
|   102104 |  2745 | `	if( pTos->x.iVal ){` |
|        - |  2746 | `		/* Take the jump */` |
|     4190 |  2747 | `		pc = pInstr->iP2 - 1;` |
|     2094 |  2748 | `	}` |
|   102104 |  2749 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2750 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2751 | `	}` |
|   102104 |  2752 | `	break;` |
|        - |  2753 | `/*` |
|        - |  2754 | ` * NOOP: * * *` |
|        - |  2755 | ` *` |
|        - |  2756 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2757 | ` * destination.` |
|        - |  2758 | ` */` |
|      ! 0 |  2759 | `case PH7_OP_NOOP:` |
|      ! 0 |  2760 | `	break;` |
|        - |  2761 | `/*` |
|        - |  2762 | ` * POP: P1 * *` |
|        - |  2763 | ` *` |
|        - |  2764 | ` * Pop P1 elements from the operand stack.` |
|        - |  2765 | ` */` |
|   370399 |  2766 | `case PH7_OP_POP: {` |
|   740844 |  2767 | `	sxi32 n = pInstr->iP1;` |
|   740844 |  2768 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2769 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2770 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2771 | `	}` |
|   740844 |  2772 | `	VmPopOperand(&pTos,n);` |
|   740844 |  2773 | `	break;` |
|        - |  2774 | `				 }` |
|        - |  2775 | `/*` |
|        - |  2776 | ` * DUP: * * *` |
|        - |  2777 | ` *` |
|        - |  2778 | ` * Duplicate the top of the stack.` |
|        - |  2779 | ` */` |
|       33 |  2780 | `case PH7_OP_DUP:` |
|        - |  2781 | `#ifdef UNTRUST` |
|        - |  2782 | `	if( pTos < pStack ){` |
|        - |  2783 | `		goto Abort;` |
|        - |  2784 | `	}` |
|        - |  2785 | `#endif` |
|       68 |  2786 | `	pTos++;` |
|       68 |  2787 | `	PH7_MemObjInit(pVm,pTos);` |
|       68 |  2788 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       68 |  2789 | `	break;` |
|        - |  2790 | `/*` |
|        - |  2791 | ` * NSSWITCH: * * P3` |
|        - |  2792 | ` *` |
|        - |  2793 | ` * Switch the active namespace at runtime.` |
|        - |  2794 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  2795 | ` */` |
|       26 |  2796 | `case PH7_OP_NSSWITCH:` |
|       53 |  2797 | `	SyBlobReset(&pVm->sNamespace);` |
|       53 |  2798 | `	if( pInstr->p3 ){` |
|       51 |  2799 | `		const char *zNs = (const char *)pInstr->p3;` |
|       51 |  2800 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       25 |  2801 | `	}` |
|       53 |  2802 | `	break;` |
|        - |  2803 | `/*` |
|        - |  2804 | ` * CVT_INT: * * *` |
|        - |  2805 | ` *` |
|        - |  2806 | ` * Force the top of the stack to be an integer.` |
|        - |  2807 | ` */` |
|       35 |  2808 | `case PH7_OP_CVT_INT:` |
|        - |  2809 | `#ifdef UNTRUST` |
|        - |  2810 | `	if( pTos < pStack ){` |
|        - |  2811 | `		goto Abort;` |
|        - |  2812 | `	}` |
|        - |  2813 | `#endif` |
|       72 |  2814 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2815 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2816 | `	}` |
|        - |  2817 | `	/* Invalidate any prior representation */` |
|       72 |  2818 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2819 | `	break;` |
|        - |  2820 | `/*` |
|        - |  2821 | ` * CVT_REAL: * * *` |
|        - |  2822 | ` *` |
|        - |  2823 | ` * Force the top of the stack to be a real.` |
|        - |  2824 | ` */` |
|        4 |  2825 | `case PH7_OP_CVT_REAL:` |
|        - |  2826 | `#ifdef UNTRUST` |
|        - |  2827 | `	if( pTos < pStack ){` |
|        - |  2828 | `		goto Abort;` |
|        - |  2829 | `	}` |
|        - |  2830 | `#endif` |
|        9 |  2831 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2832 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2833 | `	}` |
|        - |  2834 | `	/* Invalidate any prior representation */` |
|        9 |  2835 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2836 | `	break;` |
|        - |  2837 | `/*` |
|        - |  2838 | ` * CVT_STR: * * *` |
|        - |  2839 | ` *` |
|        - |  2840 | ` * Force the top of the stack to be a string.` |
|        - |  2841 | ` */` |
|      146 |  2842 | `case PH7_OP_CVT_STR:` |
|        - |  2843 | `#ifdef UNTRUST` |
|        - |  2844 | `	if( pTos < pStack ){` |
|        - |  2845 | `		goto Abort;` |
|        - |  2846 | `	}` |
|        - |  2847 | `#endif` |
|      294 |  2848 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  2849 | `		PH7_MemObjToString(pTos);` |
|      146 |  2850 | `	}` |
|      294 |  2851 | `	break;` |
|        - |  2852 | `/*` |
|        - |  2853 | ` * CVT_BOOL: * * *` |
|        - |  2854 | ` *` |
|        - |  2855 | ` * Force the top of the stack to be a boolean.` |
|        - |  2856 | ` */` |
|        5 |  2857 | `case PH7_OP_CVT_BOOL:` |
|        - |  2858 | `#ifdef UNTRUST` |
|        - |  2859 | `	if( pTos < pStack ){` |
|        - |  2860 | `		goto Abort;` |
|        - |  2861 | `	}` |
|        - |  2862 | `#endif` |
|       11 |  2863 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2864 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2865 | `	}` |
|       11 |  2866 | `	break;` |
|        - |  2867 | `/*` |
|        - |  2868 | ` * CVT_NULL: * * *` |
|        - |  2869 | ` *` |
|        - |  2870 | ` * Nullify the top of the stack.` |
|        - |  2871 | ` */` |
|        3 |  2872 | `case PH7_OP_CVT_NULL:` |
|        - |  2873 | `#ifdef UNTRUST` |
|        - |  2874 | `	if( pTos < pStack ){` |
|        - |  2875 | `		goto Abort;` |
|        - |  2876 | `	}` |
|        - |  2877 | `#endif` |
|        7 |  2878 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2879 | `	break;` |
|        - |  2880 | `/*` |
|        - |  2881 | ` * CVT_NUMC: * * *` |
|        - |  2882 | ` *` |
|        - |  2883 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2884 | ` */` |
|      ! 0 |  2885 | `case PH7_OP_CVT_NUMC:` |
|        - |  2886 | `#ifdef UNTRUST` |
|        - |  2887 | `	if( pTos < pStack ){` |
|        - |  2888 | `		goto Abort;` |
|        - |  2889 | `	}` |
|        - |  2890 | `#endif` |
|        - |  2891 | `	/* Force a numeric cast */` |
|      ! 0 |  2892 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2893 | `	break;` |
|        - |  2894 | `/*` |
|        - |  2895 | ` * CVT_ARRAY: * * *` |
|        - |  2896 | ` *` |
|        - |  2897 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2898 | ` */` |
|       10 |  2899 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2900 | `#ifdef UNTRUST` |
|        - |  2901 | `	if( pTos < pStack ){` |
|        - |  2902 | `		goto Abort;` |
|        - |  2903 | `	}` |
|        - |  2904 | `#endif` |
|        - |  2905 | `	/* Force a hashmap cast */` |
|       21 |  2906 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  2907 | `	if( rc != SXRET_OK ){` |
|        - |  2908 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  2909 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  2910 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  2911 | `	}` |
|       21 |  2912 | `	break;` |
|        - |  2913 | `/*` |
|        - |  2914 | ` * CVT_OBJ: * * *` |
|        - |  2915 | ` *` |
|        - |  2916 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  2917 | ` */` |
|        8 |  2918 | `case PH7_OP_CVT_OBJ:` |
|        - |  2919 | `#ifdef UNTRUST` |
|        - |  2920 | `	if( pTos < pStack ){` |
|        - |  2921 | `		goto Abort;` |
|        - |  2922 | `	}` |
|        - |  2923 | `#endif` |
|       17 |  2924 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  2925 | `		/* Force a 'stdClass()' cast */` |
|       17 |  2926 | `		PH7_MemObjToObject(pTos);` |
|        8 |  2927 | `	}` |
|       17 |  2928 | `	break;` |
|        - |  2929 | `/*` |
|        - |  2930 | ` * ERR_CTRL * * *` |
|        - |  2931 | ` *` |
|        - |  2932 | ` * Error control operator.` |
|        - |  2933 | ` */` |
|    12048 |  2934 | `case PH7_OP_ERR_CTRL:` |
|        - |  2935 | `	/*` |
|        - |  2936 | `	 * TICKET 1433-038:` |
|        - |  2937 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2938 | `	 * use the public API,to control error output.` |
|        - |  2939 | `	 */` |
|    24096 |  2940 | `	break;` |
|        - |  2941 | `/*` |
|        - |  2942 | ` * IS_A * * *` |
|        - |  2943 | ` *` |
|        - |  2944 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  2945 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  2946 | ` * holding a class name or an object).` |
|        - |  2947 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  2948 | ` */` |
|       18 |  2949 | `case PH7_OP_IS_A:{` |
|       38 |  2950 | `	ph7_value *pNos = &pTos[-1];` |
|       38 |  2951 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  2952 | `#ifdef UNTRUST` |
|        - |  2953 | `	if( pNos < pStack ){` |
|        - |  2954 | `		goto Abort;` |
|        - |  2955 | `	}` |
|        - |  2956 | `#endif` |
|       38 |  2957 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       36 |  2958 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       36 |  2959 | `		ph7_class *pClass = 0;` |
|        - |  2960 | `		/* Extract the target class */` |
|       36 |  2961 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  2962 | `			/* Instance already loaded */` |
|      ! 0 |  2963 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       36 |  2964 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2965 | `			/* Perform the query */` |
|       53 |  2966 | `			pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|       34 |  2967 | `				SyBlobLength(&pTos->sBlob),FALSE,0);` |
|       17 |  2968 | `		}` |
|       36 |  2969 | `		if( pClass ){` |
|        - |  2970 | `			/* Perform the query */` |
|       36 |  2971 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       17 |  2972 | `		}` |
|       17 |  2973 | `	}` |
|        - |  2974 | `	/* Push result */` |
|       38 |  2975 | `	VmPopOperand(&pTos,1);` |
|       38 |  2976 | `	PH7_MemObjRelease(pTos);` |
|       38 |  2977 | `	pTos->x.iVal = iRes;` |
|       38 |  2978 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       38 |  2979 | `	break;` |
|        - |  2980 | `				 }` |
|        - |  2981 |  |
|        - |  2982 | `/*` |
|        - |  2983 | ` * LOADC P1 P2 *` |
|        - |  2984 | ` *` |
|        - |  2985 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  2986 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  2987 | ` */` |
|   781120 |  2988 | `case PH7_OP_LOADC: {` |
|        - |  2989 | `	ph7_value *pObj;` |
|        - |  2990 | `	/* Reserve a room */` |
|  1562286 |  2991 | `	pTos++;` |
|  2335755 |  2992 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1562286 |  2993 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2994 | `			SyHashEntry *pEntry;` |
|        - |  2995 | `			/* Candidate for expansion via user defined callbacks */` |
|    15428 |  2996 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    15428 |  2997 | `			if( pEntry ){` |
|    15390 |  2998 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2999 | `				/* Set a NULL default value */` |
|    15390 |  3000 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    15390 |  3001 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3002 | `				/* Invoke the callback and deal with the expanded value */` |
|    15390 |  3003 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3004 | `				/* Mark as constant */` |
|    15390 |  3005 | `				pTos->nIdx = SXU32_HIGH;` |
|    15390 |  3006 | `				break;` |
|        - |  3007 | `			}` |
|        - |  3008 | `			/* Constant not found.  For qualified names (containing '\')` |
|        - |  3009 | `			 * this is always an error — bare unqualified names still fall` |
|        - |  3010 | `			 * through to string value for backward compatibility. */` |
|        - |  3011 | `			{` |
|       40 |  3012 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|       40 |  3013 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  3014 | `				sxu32 j;` |
|      532 |  3015 | `				for( j = 0; j < nLit; j++ ){` |
|      496 |  3016 | `					if( zLit[j] == '\\' ){` |
|        - |  3017 | `						/* Qualified name: must be a real constant.` |
|        - |  3018 | `						 * Format as PHP Fatal error to match PHP behavior. */` |
|        - |  3019 | `						{` |
|        3 |  3020 | `							SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  3021 | `							SyBlob sErr;` |
|        3 |  3022 | `							SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  3023 | `							SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  3024 | `							if( pErrFile ){` |
|        3 |  3025 | `								SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  3026 | `							}` |
|        3 |  3027 | `							SyBlobAppend(&sErr,"\n",1);` |
|        3 |  3028 | `							VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3029 | `							SyBlobRelease(&sErr);` |
|        - |  3030 | `						}` |
|        3 |  3031 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3032 | `						pTos->nIdx = SXU32_HIGH;` |
|        3 |  3033 | `						goto LoadC_Done;` |
|        - |  3034 | `					}` |
|      248 |  3035 | `				}` |
|        - |  3036 | `			}` |
|       18 |  3037 | `		}` |
|  1546896 |  3038 | `		PH7_MemObjLoad(pObj,pTos);` |
|   773471 |  3039 | `	}else{` |
|        - |  3040 | `		/* Set a NULL value */` |
|      ! 0 |  3041 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3042 | `	}` |
|   773426 |  3043 | `LoadC_Done:` |
|        - |  3044 | `	/* Mark as constant */` |
|  1546898 |  3045 | `	pTos->nIdx = SXU32_HIGH;` |
|  1546898 |  3046 | `	break;` |
|        - |  3047 | `				  }` |
|        - |  3048 | `/*` |
|        - |  3049 | ` * LOAD: P1 * P3` |
|        - |  3050 | ` *` |
|        - |  3051 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3052 | ` * from the P3 operand.` |
|        - |  3053 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3054 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3055 | ` */` |
|  1282724 |  3056 | `case PH7_OP_LOAD:{` |
|        - |  3057 | `	ph7_value *pObj;` |
|        - |  3058 | `	SyString sName;` |
|  2565670 |  3059 | `	if( pInstr->p3 == 0 ){` |
|        - |  3060 | `		/* Take the variable name from the top of the stack */` |
|        - |  3061 | `#ifdef UNTRUST` |
|        - |  3062 | `		if( pTos < pStack ){` |
|        - |  3063 | `			goto Abort;` |
|        - |  3064 | `		}` |
|        - |  3065 | `#endif` |
|        - |  3066 | `		/* Force a string cast */` |
|       19 |  3067 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3068 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3069 | `		}` |
|       19 |  3070 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3071 | `	}else{` |
|  2565652 |  3072 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3073 | `		/* Reserve a room for the target object */` |
|  2565652 |  3074 | `		pTos++;` |
|        - |  3075 | `	}` |
|        - |  3076 | `	/* Extract the requested memory object */` |
|  2565670 |  3077 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2565670 |  3078 | `	if( pObj == 0 ){` |
|      624 |  3079 | `		if( pInstr->iP1 ){` |
|        - |  3080 | `			/* Variable not found,load NULL */` |
|      624 |  3081 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3082 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3083 | `			}else{` |
|      624 |  3084 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3085 | `			}` |
|      624 |  3086 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1283037 |  3087 | `			break;` |
|      ! 0 |  3088 | `		}else{` |
|        - |  3089 | `			/* Fatal error */` |
|      ! 0 |  3090 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3091 | `			goto Abort;` |
|        - |  3092 | `		}` |
|        - |  3093 | `	}` |
|        - |  3094 | `	/* Load variable contents */` |
|  2565048 |  3095 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2565048 |  3096 | `	pTos->nIdx = pObj->nIdx;` |
|  2565048 |  3097 | `	break;` |
|        - |  3098 | `				   }` |
|        - |  3099 | `/*` |
|        - |  3100 | ` * LOAD_MAP P1 * *` |
|        - |  3101 | ` *` |
|        - |  3102 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3103 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3104 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3105 | ` */` |
|    17384 |  3106 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3107 | `	ph7_hashmap *pMap;` |
|        - |  3108 | `	/* Allocate a new hashmap instance */` |
|    34770 |  3109 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    34770 |  3110 | `	if( pMap == 0 ){` |
|      ! 0 |  3111 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3112 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3113 | `		goto Abort;` |
|        - |  3114 | `	}` |
|    34770 |  3115 | `	if( pInstr->iP1 > 0 ){` |
|     2080 |  3116 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3117 | `		/* Perform the insertion */` |
|     6302 |  3118 | `		while( pEntry < pTos ){` |
|     4224 |  3119 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3120 | `				/* Insertion by reference */` |
|      142 |  3121 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3122 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3123 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3124 | `					);` |
|       48 |  3125 | `			}else{` |
|        - |  3126 | `				/* Standard insertion */` |
|     6194 |  3127 | `				PH7_HashmapInsert(pMap,` |
|     4128 |  3128 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2064 |  3129 | `					&pEntry[1]` |
|        - |  3130 | `				);` |
|        - |  3131 | `			}` |
|        - |  3132 | `			/* Next pair on the stack */` |
|     4224 |  3133 | `			pEntry += 2;` |
|        2 |  3134 | `		}` |
|        - |  3135 | `		/* Pop P1 elements */` |
|     2080 |  3136 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1039 |  3137 | `	}` |
|        - |  3138 | `	/* Push the hashmap */` |
|    34770 |  3139 | `	pTos++;` |
|    34770 |  3140 | `	pTos->nIdx = SXU32_HIGH;` |
|    34770 |  3141 | `	pTos->x.pOther = pMap;` |
|    34770 |  3142 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    34770 |  3143 | `	break;` |
|        - |  3144 | `					  }` |
|        - |  3145 | `/*` |
|        - |  3146 | ` * LOAD_LIST: P1 * *` |
|        - |  3147 | ` *` |
|        - |  3148 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3149 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3150 | ` * Caveats:` |
|        - |  3151 | ` *  This implementation support only a single nesting level.` |
|        - |  3152 | ` */` |
|       17 |  3153 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3154 | `	ph7_value *pEntry;` |
|       35 |  3155 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3156 | `		/* Empty list,break immediately */` |
|      ! 0 |  3157 | `		break;` |
|        - |  3158 | `	}` |
|       35 |  3159 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3160 | `#ifdef UNTRUST` |
|        - |  3161 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3162 | `		goto Abort;` |
|        - |  3163 | `	}` |
|        - |  3164 | `#endif` |
|       35 |  3165 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       31 |  3166 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3167 | `		ph7_hashmap_node *pNode;` |
|        - |  3168 | `		ph7_value sKey,*pObj;` |
|        - |  3169 | `		/* Start Copying */` |
|       31 |  3170 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|       99 |  3171 | `		while( pEntry <= pTos ){` |
|       69 |  3172 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       65 |  3173 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       65 |  3174 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       65 |  3175 | `					if( rc == SXRET_OK ){` |
|        - |  3176 | `						/* Store node value */` |
|       65 |  3177 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       33 |  3178 | `					}else{` |
|        - |  3179 | `						/* Nullify the variable */` |
|      ! 0 |  3180 | `						PH7_MemObjRelease(pObj);` |
|        - |  3181 | `					}` |
|       32 |  3182 | `				}` |
|       32 |  3183 | `			}` |
|       69 |  3184 | `			sKey.x.iVal++; /* Next numeric index */` |
|       69 |  3185 | `			pEntry++;` |
|        1 |  3186 | `		}` |
|       15 |  3187 | `	}` |
|       35 |  3188 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       35 |  3189 | `	break;` |
|        - |  3190 | `					   }` |
|        - |  3191 | `/*` |
|        - |  3192 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3193 | ` *` |
|        - |  3194 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3195 | ` * from the stack.` |
|        - |  3196 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3197 | ` * instead.` |
|        - |  3198 | ` */` |
|   206717 |  3199 | `case PH7_OP_LOAD_IDX: {` |
|   413480 |  3200 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   413480 |  3201 | `	ph7_hashmap *pMap = 0;` |
|        - |  3202 | `	ph7_value *pIdx;` |
|   413480 |  3203 | `	pIdx = 0;` |
|   413480 |  3204 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3205 | `		if( !pInstr->iP2){` |
|        - |  3206 | `			/* No available index,load NULL */` |
|      ! 0 |  3207 | `			if( pTos >= pStack ){` |
|      ! 0 |  3208 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3209 | `			}else{` |
|        - |  3210 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3211 | `				pTos++;` |
|      ! 0 |  3212 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3213 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3214 | `			}` |
|        - |  3215 | `			/* Emit a notice */` |
|      ! 0 |  3216 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3217 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3218 | `			break;` |
|        - |  3219 | `		}` |
|      ! 0 |  3220 | `	}else{` |
|   413480 |  3221 | `		pIdx = pTos;` |
|   413480 |  3222 | `		pTos--;` |
|        - |  3223 | `	}` |
|   413480 |  3224 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3225 | `		/* String access */` |
|   327298 |  3226 | `		if( pIdx ){` |
|        - |  3227 | `			sxu32 nOfft;` |
|   327298 |  3228 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3229 | `				/* Force an int cast */` |
|      ! 0 |  3230 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3231 | `			}` |
|   327298 |  3232 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   327298 |  3233 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3234 | `				/* Invalid offset,load null */` |
|      ! 0 |  3235 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3236 | `			}else{` |
|   327298 |  3237 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   327298 |  3238 | `				int c = zData[nOfft];` |
|   327298 |  3239 | `				PH7_MemObjRelease(pTos);` |
|   327298 |  3240 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   327298 |  3241 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3242 | `			}` |
|   163672 |  3243 | `		}else{` |
|        - |  3244 | `			/* No available index,load NULL */` |
|      ! 0 |  3245 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3246 | `		}` |
|   327298 |  3247 | `		break;` |
|        - |  3248 | `	}` |
|    86184 |  3249 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3250 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3251 | `			ph7_value *pObj;` |
|      ! 0 |  3252 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3253 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3254 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3255 | `			}` |
|      ! 0 |  3256 | `		}` |
|      ! 0 |  3257 | `	}` |
|    86184 |  3258 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    86184 |  3259 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3260 | `		/* Point to the hashmap */` |
|    86184 |  3261 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    86184 |  3262 | `		if( pIdx ){` |
|        - |  3263 | `			/* Load the desired entry */` |
|    86184 |  3264 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    43091 |  3265 | `		}` |
|    86184 |  3266 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3267 | `			/* Create a new empty entry */` |
|      ! 0 |  3268 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3269 | `			if( rc == SXRET_OK ){` |
|        - |  3270 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3271 | `				pNode = pMap->pLast;` |
|      ! 0 |  3272 | `			}` |
|      ! 0 |  3273 | `		}` |
|    43091 |  3274 | `	}` |
|    86184 |  3275 | `	if( pIdx ){` |
|    86184 |  3276 | `		PH7_MemObjRelease(pIdx);` |
|    43091 |  3277 | `	}` |
|    86184 |  3278 | `	if( rc == SXRET_OK ){` |
|        - |  3279 | `		/* Load entry contents */` |
|    39416 |  3280 | `		if( pMap->iRef < 2 ){` |
|        - |  3281 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3282 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3283 | `			 */` |
|        7 |  3284 | `			pTos->nIdx = SXU32_HIGH;` |
|        7 |  3285 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|        4 |  3286 | `		}else{` |
|    39410 |  3287 | `			pTos->nIdx = pNode->nValIdx;` |
|    39410 |  3288 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    39410 |  3289 | `			PH7_HashmapUnref(pMap);` |
|        - |  3290 | `		}` |
|    19709 |  3291 | `	}else{` |
|        - |  3292 | `		/* No such entry,load NULL */` |
|    46770 |  3293 | `		PH7_MemObjRelease(pTos);` |
|    46770 |  3294 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3295 | `	}` |
|    86184 |  3296 | `	break;` |
|        - |  3297 | `					  }` |
|        - |  3298 | `/*` |
|        - |  3299 | ` * LOAD_CLOSURE * * P3` |
|        - |  3300 | ` *` |
|        - |  3301 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3302 | ` * name in the stack.` |
|        - |  3303 | ` */` |
|        2 |  3304 | `case PH7_OP_LOAD_CLOSURE:{` |
|        5 |  3305 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        5 |  3306 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3307 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3308 | `		ph7_vm_func *pClosure;` |
|        - |  3309 | `		char *zName;` |
|        - |  3310 | `		sxu32 mLen;` |
|        - |  3311 | `		sxu32 n;` |
|        - |  3312 | `		/* Create a new VM function */` |
|        5 |  3313 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3314 | `		/* Generate an unique closure name */` |
|        5 |  3315 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        5 |  3316 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3317 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3318 | `			goto Abort;` |
|        - |  3319 | `		}` |
|        5 |  3320 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        5 |  3321 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3322 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3323 | `		}` |
|        - |  3324 | `		/* Zero the stucture */` |
|        5 |  3325 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3326 | `		/* Perform a structure assignment on read-only items */` |
|        5 |  3327 | `		pClosure->aArgs = pFunc->aArgs;` |
|        5 |  3328 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        5 |  3329 | `		pClosure->aStatic = pFunc->aStatic;` |
|        5 |  3330 | `		pClosure->iFlags = pFunc->iFlags;` |
|        5 |  3331 | `		pClosure->pUserData = pFunc->pUserData;` |
|        5 |  3332 | `		pClosure->sSignature = pFunc->sSignature;` |
|        5 |  3333 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3334 | `		/* Register the closure */` |
|        5 |  3335 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3336 | `		/* Set up closure environment */` |
|        5 |  3337 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        5 |  3338 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       13 |  3339 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3340 | `			ph7_value *pValue;` |
|        9 |  3341 | `			pEnv = &aEnv[n];` |
|        9 |  3342 | `			sEnv.sName  = pEnv->sName;` |
|        9 |  3343 | `			sEnv.iFlags = pEnv->iFlags;` |
|        9 |  3344 | `			sEnv.nIdx = SXU32_HIGH;` |
|        9 |  3345 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|        9 |  3346 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3347 | `				/* Pass by reference */` |
|      ! 0 |  3348 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3349 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3350 | `					);` |
|      ! 0 |  3351 | `			}` |
|        - |  3352 | `			/* Standard pass by value */` |
|        9 |  3353 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|        9 |  3354 | `			if( pValue ){` |
|        - |  3355 | `				/* Copy imported value */` |
|        5 |  3356 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        2 |  3357 | `			}` |
|        - |  3358 | `			/* Insert the imported variable */` |
|        9 |  3359 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        5 |  3360 | `		}` |
|        - |  3361 | `		/* Finally,load the closure name on the stack */` |
|        5 |  3362 | `		pTos++;` |
|        5 |  3363 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        2 |  3364 | `	}` |
|        5 |  3365 | `	break;` |
|        - |  3366 | `						 }` |
|        - |  3367 | `/*` |
|        - |  3368 | ` * STORE * P2 P3` |
|        - |  3369 | ` *` |
|        - |  3370 | ` * Perform a store (Assignment) operation.` |
|        - |  3371 | ` */` |
|   106950 |  3372 | `case PH7_OP_STORE: {` |
|        - |  3373 | `	ph7_value *pObj;` |
|        - |  3374 | `	SyString sName;` |
|        - |  3375 | `#ifdef UNTRUST` |
|        - |  3376 | `	if( pTos < pStack ){` |
|        - |  3377 | `		goto Abort;` |
|        - |  3378 | `	}` |
|        - |  3379 | `#endif` |
|   213902 |  3380 | `	if( pInstr->iP2 ){` |
|        - |  3381 | `		sxu32 nIdx;` |
|        - |  3382 | `		/* Member store operation */` |
|     2838 |  3383 | `		nIdx = pTos->nIdx;` |
|     2838 |  3384 | `		VmPopOperand(&pTos,1);` |
|     2838 |  3385 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3386 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3387 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3388 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3389 | `		}else{` |
|        - |  3390 | `			/* Point to the desired memory object */` |
|     2834 |  3391 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2834 |  3392 | `			if( pObj ){` |
|        - |  3393 | `				/* Perform the store operation */` |
|     2834 |  3394 | `				PH7_MemObjStore(pTos,pObj);` |
|     1416 |  3395 | `			}` |
|        - |  3396 | `		}` |
|   108370 |  3397 | `		break;` |
|   211066 |  3398 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3399 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3400 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3401 | `			/* Force a string cast */` |
|      ! 0 |  3402 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3403 | `		}` |
|        7 |  3404 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3405 | `		pTos--;` |
|        - |  3406 | `#ifdef UNTRUST` |
|        - |  3407 | `		if( pTos < pStack  ){` |
|        - |  3408 | `			goto Abort;` |
|        - |  3409 | `		}` |
|        - |  3410 | `#endif` |
|        4 |  3411 | `	}else{` |
|   211060 |  3412 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3413 | `	}` |
|        - |  3414 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   211066 |  3415 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   211066 |  3416 | `	if( pObj == 0 ){` |
|      ! 0 |  3417 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3418 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3419 | `		goto Abort;` |
|        - |  3420 | `	}` |
|   211066 |  3421 | `	if( !pInstr->p3 ){` |
|        7 |  3422 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3423 | `	}` |
|        - |  3424 | `	/* Perform the store operation */` |
|   211066 |  3425 | `	PH7_MemObjStore(pTos,pObj);` |
|   211066 |  3426 | `	break;` |
|        - |  3427 | `				   }` |
|        - |  3428 | `/*` |
|        - |  3429 | ` * STORE_IDX:   P1 * P3` |
|        - |  3430 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3431 | ` *` |
|        - |  3432 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3433 | ` */` |
|    78249 |  3434 | `case PH7_OP_STORE_IDX:` |
|        - |  3435 | `case PH7_OP_STORE_IDX_REF: {` |
|   156500 |  3436 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3437 | `	ph7_value *pKey;` |
|        - |  3438 | `	sxu32 nIdx;` |
|   156500 |  3439 | `	if( pInstr->iP1 ){` |
|        - |  3440 | `		/* Key is next on stack */` |
|    56148 |  3441 | `		pKey = pTos;` |
|    56148 |  3442 | `		pTos--;` |
|    28075 |  3443 | `	}else{` |
|   100354 |  3444 | `		pKey = 0;` |
|        - |  3445 | `	}` |
|   156500 |  3446 | `	nIdx = pTos->nIdx;` |
|   156500 |  3447 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3448 | `		/* Hashmap already loaded */` |
|   156448 |  3449 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   156448 |  3450 | `		if( pMap->iRef < 2 ){` |
|        - |  3451 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3452 | `			pMap->iRef = 2;` |
|      ! 0 |  3453 | `		}` |
|    78225 |  3454 | `	}else{` |
|        - |  3455 | `		ph7_value *pObj;` |
|       53 |  3456 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3457 | `		if( pObj == 0 ){` |
|      ! 0 |  3458 | `			if( pKey ){` |
|      ! 0 |  3459 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3460 | `			}` |
|      ! 0 |  3461 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3462 | `			break;` |
|        - |  3463 | `		}` |
|        - |  3464 | `		/* Phase#1: Load the array */` |
|       53 |  3465 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3466 | `			VmPopOperand(&pTos,1);` |
|       53 |  3467 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3468 | `				/* Force a string cast */` |
|      ! 0 |  3469 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3470 | `			}` |
|       53 |  3471 | `			if( pKey == 0 ){` |
|        - |  3472 | `				/* Append string */` |
|        3 |  3473 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3474 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3475 | `				}` |
|        2 |  3476 | `			}else{` |
|        - |  3477 | `				sxu32 nOfft;` |
|       51 |  3478 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3479 | `					/* Force an int cast */` |
|       51 |  3480 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3481 | `				}` |
|       51 |  3482 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3483 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3484 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3485 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3486 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3487 | `				}else{` |
|      ! 0 |  3488 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3489 | `						/* Perform an append operation */` |
|      ! 0 |  3490 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3491 | `					}` |
|        - |  3492 | `				}` |
|        - |  3493 | `			}` |
|       53 |  3494 | `			if( pKey ){` |
|       51 |  3495 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3496 | `			}` |
|       53 |  3497 | `			break;` |
|      ! 0 |  3498 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3499 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3500 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3501 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3502 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3503 | `				goto Abort;` |
|        - |  3504 | `			}` |
|      ! 0 |  3505 | `		}` |
|      ! 0 |  3506 | `		pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - |  3507 | `	}` |
|   156448 |  3508 | `	VmPopOperand(&pTos,1);` |
|        - |  3509 | `	/* Phase#2: Perform the insertion */` |
|   156448 |  3510 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3511 | `		/* Insertion by reference */` |
|       15 |  3512 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3513 | `	}else{` |
|   156434 |  3514 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3515 | `	}` |
|   156448 |  3516 | `	if( pKey ){` |
|    56098 |  3517 | `		PH7_MemObjRelease(pKey);` |
|    28048 |  3518 | `	}` |
|   156448 |  3519 | `	break;` |
|        - |  3520 | `					   }` |
|        - |  3521 | `/*` |
|        - |  3522 | ` * INCR: P1 * *` |
|        - |  3523 | ` *` |
|        - |  3524 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3525 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3526 | ` * the stack and increment after that.` |
|        - |  3527 | ` */` |
|   146555 |  3528 | `case PH7_OP_INCR:` |
|        - |  3529 | `#ifdef UNTRUST` |
|        - |  3530 | `	if( pTos < pStack ){` |
|        - |  3531 | `		goto Abort;` |
|        - |  3532 | `	}` |
|        - |  3533 | `#endif` |
|   293156 |  3534 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   293156 |  3535 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3536 | `			ph7_value *pObj;` |
|   293156 |  3537 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3538 | `				/* Force a numeric cast */` |
|   293156 |  3539 | `				PH7_MemObjToNumeric(pObj);` |
|   293156 |  3540 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3541 | `					pObj->rVal++;` |
|        - |  3542 | `					/* Try to get an integer representation */` |
|      ! 0 |  3543 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3544 | `				}else{` |
|   293156 |  3545 | `					pObj->x.iVal++;` |
|   293156 |  3546 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3547 | `				}` |
|   293156 |  3548 | `				if( pInstr->iP1 ){` |
|        - |  3549 | `					/* Pre-icrement */` |
|       71 |  3550 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3551 | `				}` |
|   146599 |  3552 | `			}` |
|   146601 |  3553 | `		}else{` |
|      ! 0 |  3554 | `			if( pInstr->iP1 ){` |
|        - |  3555 | `				/* Force a numeric cast */` |
|      ! 0 |  3556 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3557 | `				/* Pre-increment */` |
|      ! 0 |  3558 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3559 | `					pTos->rVal++;` |
|        - |  3560 | `					/* Try to get an integer representation */` |
|      ! 0 |  3561 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3562 | `				}else{` |
|      ! 0 |  3563 | `					pTos->x.iVal++;` |
|      ! 0 |  3564 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3565 | `				}` |
|      ! 0 |  3566 | `			}` |
|        - |  3567 | `		}` |
|   146599 |  3568 | `	}` |
|   293156 |  3569 | `	break;` |
|        - |  3570 | `/*` |
|        - |  3571 | ` * DECR: P1 * *` |
|        - |  3572 | ` *` |
|        - |  3573 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3574 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3575 | ` * and decrement after that.` |
|        - |  3576 | ` */` |
|        2 |  3577 | `case PH7_OP_DECR:` |
|        - |  3578 | `#ifdef UNTRUST` |
|        - |  3579 | `	if( pTos < pStack ){` |
|        - |  3580 | `		goto Abort;` |
|        - |  3581 | `	}` |
|        - |  3582 | `#endif` |
|        5 |  3583 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3584 | `		/* Force a numeric cast */` |
|        5 |  3585 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3586 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3587 | `			ph7_value *pObj;` |
|        5 |  3588 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3589 | `				/* Force a numeric cast */` |
|        5 |  3590 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3591 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3592 | `					pObj->rVal--;` |
|        - |  3593 | `					/* Try to get an integer representation */` |
|      ! 0 |  3594 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3595 | `				}else{` |
|        5 |  3596 | `					pObj->x.iVal--;` |
|        5 |  3597 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3598 | `				}` |
|        5 |  3599 | `				if( pInstr->iP1 ){` |
|        - |  3600 | `					/* Pre-icrement */` |
|      ! 0 |  3601 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3602 | `				}` |
|        2 |  3603 | `			}` |
|        3 |  3604 | `		}else{` |
|      ! 0 |  3605 | `			if( pInstr->iP1 ){` |
|        - |  3606 | `				/* Pre-increment */` |
|      ! 0 |  3607 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3608 | `					pTos->rVal--;` |
|        - |  3609 | `					/* Try to get an integer representation */` |
|      ! 0 |  3610 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3611 | `				}else{` |
|      ! 0 |  3612 | `					pTos->x.iVal--;` |
|      ! 0 |  3613 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3614 | `				}` |
|      ! 0 |  3615 | `			}` |
|        - |  3616 | `		}` |
|        2 |  3617 | `	}` |
|        5 |  3618 | `	break;` |
|        - |  3619 | `/*` |
|        - |  3620 | ` * UMINUS: * * *` |
|        - |  3621 | ` *` |
|        - |  3622 | ` * Perform a unary minus operation.` |
|        - |  3623 | ` */` |
|    22486 |  3624 | `case PH7_OP_UMINUS:` |
|        - |  3625 | `#ifdef UNTRUST` |
|        - |  3626 | `	if( pTos < pStack ){` |
|        - |  3627 | `		goto Abort;` |
|        - |  3628 | `	}` |
|        - |  3629 | `#endif` |
|        - |  3630 | `	/* Force a numeric (integer,real or both) cast */` |
|    44974 |  3631 | `	PH7_MemObjToNumeric(pTos);` |
|    44974 |  3632 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       32 |  3633 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3634 | `	}` |
|    44974 |  3635 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    44944 |  3636 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    22471 |  3637 | `	}` |
|    44974 |  3638 | `	break;` |
|        - |  3639 | `/*` |
|        - |  3640 | ` * UPLUS: * * *` |
|        - |  3641 | ` *` |
|        - |  3642 | ` * Perform a unary plus operation.` |
|        - |  3643 | ` */` |
|       16 |  3644 | `case PH7_OP_UPLUS:` |
|        - |  3645 | `#ifdef UNTRUST` |
|        - |  3646 | `	if( pTos < pStack ){` |
|        - |  3647 | `		goto Abort;` |
|        - |  3648 | `	}` |
|        - |  3649 | `#endif` |
|        - |  3650 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3651 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3652 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3653 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3654 | `	}` |
|       33 |  3655 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3656 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3657 | `	}` |
|       33 |  3658 | `	break;` |
|        - |  3659 | `/*` |
|        - |  3660 | ` * OP_LNOT: * * *` |
|        - |  3661 | ` *` |
|        - |  3662 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3663 | ` * with its complement.` |
|        - |  3664 | ` */` |
|    38728 |  3665 | `case PH7_OP_LNOT:` |
|        - |  3666 | `#ifdef UNTRUST` |
|        - |  3667 | `	if( pTos < pStack ){` |
|        - |  3668 | `		goto Abort;` |
|        - |  3669 | `	}` |
|        - |  3670 | `#endif` |
|        - |  3671 | `	/* Force a boolean cast */` |
|    77502 |  3672 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3673 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3674 | `	}` |
|    77502 |  3675 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    77502 |  3676 | `	break;` |
|        - |  3677 | `/*` |
|        - |  3678 | ` * OP_BITNOT: * * *` |
|        - |  3679 | ` *` |
|        - |  3680 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3681 | ` * with its ones-complement.` |
|        - |  3682 | ` */` |
|       14 |  3683 | `case PH7_OP_BITNOT:` |
|        - |  3684 | `#ifdef UNTRUST` |
|        - |  3685 | `	if( pTos < pStack ){` |
|        - |  3686 | `		goto Abort;` |
|        - |  3687 | `	}` |
|        - |  3688 | `#endif` |
|        - |  3689 | `	/* Force an integer cast */` |
|       30 |  3690 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3691 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3692 | `	}` |
|       30 |  3693 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  3694 | `	break;` |
|        - |  3695 | `/* OP_MUL * * *` |
|        - |  3696 | ` * OP_MUL_STORE * * *` |
|        - |  3697 | ` *` |
|        - |  3698 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3699 | ` * and push the result back onto the stack.` |
|        - |  3700 | ` */` |
|     1234 |  3701 | `case PH7_OP_MUL:` |
|        - |  3702 | `case PH7_OP_MUL_STORE: {` |
|     2470 |  3703 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3704 | `	/* Force the operand to be numeric */` |
|        - |  3705 | `#ifdef UNTRUST` |
|        - |  3706 | `	if( pNos < pStack ){` |
|        - |  3707 | `		goto Abort;` |
|        - |  3708 | `	}` |
|        - |  3709 | `#endif` |
|     2470 |  3710 | `	PH7_MemObjToNumeric(pTos);` |
|     2470 |  3711 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3712 | `	/* Perform the requested operation */` |
|     2470 |  3713 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3714 | `		/* Floating point arithemic */` |
|        - |  3715 | `		ph7_real a,b,r;` |
|       17 |  3716 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3717 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3718 | `		}` |
|       17 |  3719 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3720 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3721 | `		}` |
|       17 |  3722 | `		a = pNos->rVal;` |
|       17 |  3723 | `		b = pTos->rVal;` |
|       17 |  3724 | `		r = a * b;` |
|        - |  3725 | `		/* Push the result */` |
|       17 |  3726 | `		pNos->rVal = r;` |
|       17 |  3727 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3728 | `		/* Try to get an integer representation */` |
|       17 |  3729 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3730 | `	}else{` |
|        - |  3731 | `		/* Integer arithmetic */` |
|        - |  3732 | `		sxi64 a,b,r;` |
|     2454 |  3733 | `		a = pNos->x.iVal;` |
|     2454 |  3734 | `		b = pTos->x.iVal;` |
|     2454 |  3735 | `		r = a * b;` |
|        - |  3736 | `		/* Push the result */` |
|     2454 |  3737 | `		pNos->x.iVal = r;` |
|     2454 |  3738 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3739 | `	}` |
|     2470 |  3740 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3741 | `		ph7_value *pObj;` |
|       19 |  3742 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3743 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  3744 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  3745 | `			PH7_MemObjStore(pNos,pObj);` |
|        9 |  3746 | `		}` |
|        9 |  3747 | `	}` |
|     2470 |  3748 | `	VmPopOperand(&pTos,1);` |
|     2470 |  3749 | `	break;` |
|        - |  3750 | `				 }` |
|        - |  3751 | `/* OP_ADD * * *` |
|        - |  3752 | ` *` |
|        - |  3753 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3754 | ` * and push the result back onto the stack.` |
|        - |  3755 | ` */` |
|      427 |  3756 | `case PH7_OP_ADD:{` |
|      856 |  3757 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3758 | `#ifdef UNTRUST` |
|        - |  3759 | `	if( pNos < pStack ){` |
|        - |  3760 | `		goto Abort;` |
|        - |  3761 | `	}` |
|        - |  3762 | `#endif` |
|        - |  3763 | `	/* Perform the addition */` |
|      856 |  3764 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      856 |  3765 | `	VmPopOperand(&pTos,1);` |
|      856 |  3766 | `	break;` |
|        - |  3767 | `				}` |
|        - |  3768 | `/*` |
|        - |  3769 | ` * OP_ADD_STORE * * *` |
|        - |  3770 | ` *` |
|        - |  3771 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3772 | ` * and push the result back onto the stack.` |
|        - |  3773 | ` */` |
|      481 |  3774 | `case PH7_OP_ADD_STORE:{` |
|      963 |  3775 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3776 | `	ph7_value *pObj;` |
|        - |  3777 | `	sxu32 nIdx;` |
|        - |  3778 | `#ifdef UNTRUST` |
|        - |  3779 | `	if( pNos < pStack ){` |
|        - |  3780 | `		goto Abort;` |
|        - |  3781 | `	}` |
|        - |  3782 | `#endif` |
|        - |  3783 | `	/* Perform the addition */` |
|      963 |  3784 | `	nIdx = pTos->nIdx;` |
|      963 |  3785 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3786 | `	/* Peform the store operation */` |
|      963 |  3787 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3788 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      963 |  3789 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      963 |  3790 | `		PH7_MemObjStore(pTos,pObj);` |
|      481 |  3791 | `	}` |
|        - |  3792 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      963 |  3793 | `	PH7_MemObjStore(pTos,pNos);` |
|      963 |  3794 | `	VmPopOperand(&pTos,1);` |
|      963 |  3795 | `	break;` |
|        - |  3796 | `				}` |
|        - |  3797 | `/* OP_SUB * * *` |
|        - |  3798 | ` *` |
|        - |  3799 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3800 | ` * first (what was next on the stack) from the second (the` |
|        - |  3801 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3802 | ` */` |
|      294 |  3803 | `case PH7_OP_SUB: {` |
|      589 |  3804 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3805 | `#ifdef UNTRUST` |
|        - |  3806 | `	if( pNos < pStack ){` |
|        - |  3807 | `		goto Abort;` |
|        - |  3808 | `	}` |
|        - |  3809 | `#endif` |
|      589 |  3810 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3811 | `		/* Floating point arithemic */` |
|        - |  3812 | `		ph7_real a,b,r;` |
|       95 |  3813 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3814 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3815 | `		}` |
|       95 |  3816 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3817 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3818 | `		}` |
|       95 |  3819 | `		a = pNos->rVal;` |
|       95 |  3820 | `		b = pTos->rVal;` |
|       95 |  3821 | `		r = a - b;` |
|        - |  3822 | `		/* Push the result */` |
|       95 |  3823 | `		pNos->rVal = r;` |
|       95 |  3824 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3825 | `		/* Try to get an integer representation */` |
|       95 |  3826 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  3827 | `	}else{` |
|        - |  3828 | `		/* Integer arithmetic */` |
|        - |  3829 | `		sxi64 a,b,r;` |
|      495 |  3830 | `		a = pNos->x.iVal;` |
|      495 |  3831 | `		b = pTos->x.iVal;` |
|      495 |  3832 | `		r = a - b;` |
|        - |  3833 | `		/* Push the result */` |
|      495 |  3834 | `		pNos->x.iVal = r;` |
|      495 |  3835 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3836 | `	}` |
|      589 |  3837 | `	VmPopOperand(&pTos,1);` |
|      589 |  3838 | `	break;` |
|        - |  3839 | `				 }` |
|        - |  3840 | `/* OP_SUB_STORE * * *` |
|        - |  3841 | ` *` |
|        - |  3842 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3843 | ` * first (what was next on the stack) from the second (the` |
|        - |  3844 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3845 | ` */` |
|        1 |  3846 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3847 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3848 | `	ph7_value *pObj;` |
|        - |  3849 | `#ifdef UNTRUST` |
|        - |  3850 | `	if( pNos < pStack ){` |
|        - |  3851 | `		goto Abort;` |
|        - |  3852 | `	}` |
|        - |  3853 | `#endif` |
|        3 |  3854 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3855 | `		/* Floating point arithemic */` |
|        - |  3856 | `		ph7_real a,b,r;` |
|      ! 0 |  3857 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3858 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3859 | `		}` |
|      ! 0 |  3860 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3861 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3862 | `		}` |
|      ! 0 |  3863 | `		a = pTos->rVal;` |
|      ! 0 |  3864 | `		b = pNos->rVal;` |
|      ! 0 |  3865 | `		r = a - b;` |
|        - |  3866 | `		/* Push the result */` |
|      ! 0 |  3867 | `		pNos->rVal = r;` |
|      ! 0 |  3868 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3869 | `		/* Try to get an integer representation */` |
|      ! 0 |  3870 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3871 | `	}else{` |
|        - |  3872 | `		/* Integer arithmetic */` |
|        - |  3873 | `		sxi64 a,b,r;` |
|        3 |  3874 | `		a = pTos->x.iVal;` |
|        3 |  3875 | `		b = pNos->x.iVal;` |
|        3 |  3876 | `		r = a - b;` |
|        - |  3877 | `		/* Push the result */` |
|        3 |  3878 | `		pNos->x.iVal = r;` |
|        3 |  3879 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3880 | `	}` |
|        3 |  3881 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3882 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3883 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3884 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3885 | `	}` |
|        3 |  3886 | `	VmPopOperand(&pTos,1);` |
|        3 |  3887 | `	break;` |
|        - |  3888 | `				 }` |
|        - |  3889 |  |
|        - |  3890 | `/*` |
|        - |  3891 | ` * OP_MOD * * *` |
|        - |  3892 | ` *` |
|        - |  3893 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3894 | ` * first (what was next on the stack) from the second (the` |
|        - |  3895 | ` * top of the stack) and push the remainder after division` |
|        - |  3896 | ` * onto the stack.` |
|        - |  3897 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3898 | ` */` |
|      296 |  3899 | `case PH7_OP_MOD:{` |
|      594 |  3900 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3901 | `	sxi64 a,b,r;` |
|        - |  3902 | `#ifdef UNTRUST` |
|        - |  3903 | `	if( pNos < pStack ){` |
|        - |  3904 | `		goto Abort;` |
|        - |  3905 | `	}` |
|        - |  3906 | `#endif` |
|        - |  3907 | `	/* Force the operands to be integer */` |
|      594 |  3908 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3909 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3910 | `	}` |
|      594 |  3911 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3912 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3913 | `	}` |
|        - |  3914 | `	/* Perform the requested operation */` |
|      594 |  3915 | `	a = pNos->x.iVal;` |
|      594 |  3916 | `	b = pTos->x.iVal;` |
|      594 |  3917 | `	if( b == 0 ){` |
|        3 |  3918 | `		r = 0;` |
|        3 |  3919 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3920 | `		/* goto Abort; */` |
|        2 |  3921 | `	}else{` |
|      591 |  3922 | `		r = a%b;` |
|        - |  3923 | `	}` |
|        - |  3924 | `	/* Push the result */` |
|      594 |  3925 | `	pNos->x.iVal = r;` |
|      594 |  3926 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      594 |  3927 | `	VmPopOperand(&pTos,1);` |
|      594 |  3928 | `	break;` |
|        - |  3929 | `				}` |
|        - |  3930 | `/*` |
|        - |  3931 | ` * OP_MOD_STORE * * *` |
|        - |  3932 | ` *` |
|        - |  3933 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3934 | ` * first (what was next on the stack) from the second (the` |
|        - |  3935 | ` * top of the stack) and push the remainder after division` |
|        - |  3936 | ` * onto the stack.` |
|        - |  3937 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3938 | ` */` |
|        1 |  3939 | `case PH7_OP_MOD_STORE: {` |
|        3 |  3940 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3941 | `	ph7_value *pObj;` |
|        - |  3942 | `	sxi64 a,b,r;` |
|        - |  3943 | `#ifdef UNTRUST` |
|        - |  3944 | `	if( pNos < pStack ){` |
|        - |  3945 | `		goto Abort;` |
|        - |  3946 | `	}` |
|        - |  3947 | `#endif` |
|        - |  3948 | `	/* Force the operands to be integer */` |
|        3 |  3949 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3950 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3951 | `	}` |
|        3 |  3952 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3953 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3954 | `	}` |
|        - |  3955 | `	/* Perform the requested operation */` |
|        3 |  3956 | `	a = pTos->x.iVal;` |
|        3 |  3957 | `	b = pNos->x.iVal;` |
|        3 |  3958 | `	if( b == 0 ){` |
|      ! 0 |  3959 | `		r = 0;` |
|      ! 0 |  3960 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3961 | `		/* goto Abort; */` |
|      ! 0 |  3962 | `	}else{` |
|        3 |  3963 | `		r = a%b;` |
|        - |  3964 | `	}` |
|        - |  3965 | `	/* Push the result */` |
|        3 |  3966 | `	pNos->x.iVal = r;` |
|        3 |  3967 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  3968 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3969 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3970 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3971 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3972 | `	}` |
|        3 |  3973 | `	VmPopOperand(&pTos,1);` |
|        3 |  3974 | `	break;` |
|        - |  3975 | `				}` |
|        - |  3976 | `/*` |
|        - |  3977 | ` * OP_DIV * * *` |
|        - |  3978 | ` *` |
|        - |  3979 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3980 | ` * first (what was next on the stack) from the second (the` |
|        - |  3981 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3982 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3983 | ` */` |
|       28 |  3984 | `case PH7_OP_DIV:{` |
|       58 |  3985 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3986 | `	ph7_real a,b,r;` |
|        - |  3987 | `#ifdef UNTRUST` |
|        - |  3988 | `	if( pNos < pStack ){` |
|        - |  3989 | `		goto Abort;` |
|        - |  3990 | `	}` |
|        - |  3991 | `#endif` |
|        - |  3992 | `	/* Force the operands to be real */` |
|       58 |  3993 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  3994 | `		PH7_MemObjToReal(pTos);` |
|       26 |  3995 | `	}` |
|       58 |  3996 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  3997 | `		PH7_MemObjToReal(pNos);` |
|        9 |  3998 | `	}` |
|        - |  3999 | `	/* Perform the requested operation */` |
|       58 |  4000 | `	a = pNos->rVal;` |
|       58 |  4001 | `	b = pTos->rVal;` |
|       58 |  4002 | `	if( b == 0 ){` |
|        - |  4003 | `		/* Division by zero */` |
|        3 |  4004 | `		pNos->rVal = 0;` |
|        3 |  4005 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  4006 | `		/* goto Abort; */` |
|        2 |  4007 | `	}else{` |
|       55 |  4008 | `		r = a/b;` |
|        - |  4009 | `		/* Push the result */` |
|       55 |  4010 | `		pNos->rVal = r;` |
|       55 |  4011 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4012 | `		/* Try to get an integer representation */` |
|       55 |  4013 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4014 | `	}` |
|       58 |  4015 | `	VmPopOperand(&pTos,1);` |
|       58 |  4016 | `	break;` |
|        - |  4017 | `				}` |
|        - |  4018 | `/*` |
|        - |  4019 | ` * OP_DIV_STORE * * *` |
|        - |  4020 | ` *` |
|        - |  4021 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4022 | ` * first (what was next on the stack) from the second (the` |
|        - |  4023 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4024 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4025 | ` */` |
|        1 |  4026 | `case PH7_OP_DIV_STORE:{` |
|        3 |  4027 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4028 | `	ph7_value *pObj;` |
|        - |  4029 | `	ph7_real a,b,r;` |
|        - |  4030 | `#ifdef UNTRUST` |
|        - |  4031 | `	if( pNos < pStack ){` |
|        - |  4032 | `		goto Abort;` |
|        - |  4033 | `	}` |
|        - |  4034 | `#endif` |
|        - |  4035 | `	/* Force the operands to be real */` |
|        3 |  4036 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4037 | `		PH7_MemObjToReal(pTos);` |
|        1 |  4038 | `	}` |
|        3 |  4039 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4040 | `		PH7_MemObjToReal(pNos);` |
|        1 |  4041 | `	}` |
|        - |  4042 | `	/* Perform the requested operation */` |
|        3 |  4043 | `	a = pTos->rVal;` |
|        3 |  4044 | `	b = pNos->rVal;` |
|        3 |  4045 | `	if( b == 0 ){` |
|        - |  4046 | `		/* Division by zero */` |
|      ! 0 |  4047 | `		r = 0;` |
|      ! 0 |  4048 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4049 | `		/* goto Abort; */` |
|      ! 0 |  4050 | `	}else{` |
|        3 |  4051 | `		r = a/b;` |
|        - |  4052 | `		/* Push the result */` |
|        3 |  4053 | `		pNos->rVal = r;` |
|        3 |  4054 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4055 | `		/* Try to get an integer representation */` |
|        3 |  4056 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4057 | `	}` |
|        3 |  4058 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4059 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4060 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4061 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4062 | `	}` |
|        3 |  4063 | `	VmPopOperand(&pTos,1);` |
|        3 |  4064 | `	break;` |
|        - |  4065 | `				}` |
|        - |  4066 | `/* OP_BAND * * *` |
|        - |  4067 | ` *` |
|        - |  4068 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4069 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4070 | ` * two elements.` |
|        - |  4071 | `*/` |
|        - |  4072 | `/* OP_BOR * * *` |
|        - |  4073 | ` *` |
|        - |  4074 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4075 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4076 | ` * two elements.` |
|        - |  4077 | ` */` |
|        - |  4078 | `/* OP_BXOR * * *` |
|        - |  4079 | ` *` |
|        - |  4080 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4081 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4082 | ` * two elements.` |
|        - |  4083 | ` */` |
|       30 |  4084 | `case PH7_OP_BAND:` |
|        - |  4085 | `case PH7_OP_BOR:` |
|        - |  4086 | `case PH7_OP_BXOR:{` |
|       62 |  4087 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4088 | `	sxi64 a,b,r;` |
|        - |  4089 | `#ifdef UNTRUST` |
|        - |  4090 | `	if( pNos < pStack ){` |
|        - |  4091 | `		goto Abort;` |
|        - |  4092 | `	}` |
|        - |  4093 | `#endif` |
|        - |  4094 | `	/* Force the operands to be integer */` |
|       62 |  4095 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4096 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4097 | `	}` |
|       62 |  4098 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4099 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4100 | `	}` |
|        - |  4101 | `	/* Perform the requested operation */` |
|       62 |  4102 | `	a = pNos->x.iVal;` |
|       62 |  4103 | `	b = pTos->x.iVal;` |
|       62 |  4104 | `	switch(pInstr->iOp){` |
|        6 |  4105 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4106 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4107 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4108 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       18 |  4109 | `	case PH7_OP_BAND_STORE:` |
|       18 |  4110 | `	case PH7_OP_BAND:` |
|       38 |  4111 | `	default:          r = a&b; break;` |
|        - |  4112 | `	}` |
|        - |  4113 | `	/* Push the result */` |
|       62 |  4114 | `	pNos->x.iVal = r;` |
|       62 |  4115 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       62 |  4116 | `	VmPopOperand(&pTos,1);` |
|       62 |  4117 | `	break;` |
|        - |  4118 | `				 }` |
|        - |  4119 | `/* OP_BAND_STORE * * *` |
|        - |  4120 | ` *` |
|        - |  4121 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4122 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4123 | ` * two elements.` |
|        - |  4124 | `*/` |
|        - |  4125 | `/* OP_BOR_STORE * * *` |
|        - |  4126 | ` *` |
|        - |  4127 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4128 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4129 | ` * two elements.` |
|        - |  4130 | ` */` |
|        - |  4131 | `/* OP_BXOR_STORE * * *` |
|        - |  4132 | ` *` |
|        - |  4133 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4134 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4135 | ` * two elements.` |
|        - |  4136 | ` */` |
|        7 |  4137 | `case PH7_OP_BAND_STORE:` |
|        - |  4138 | `case PH7_OP_BOR_STORE:` |
|        - |  4139 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4140 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4141 | `	ph7_value *pObj;` |
|        - |  4142 | `	sxi64 a,b,r;` |
|        - |  4143 | `#ifdef UNTRUST` |
|        - |  4144 | `	if( pNos < pStack ){` |
|        - |  4145 | `		goto Abort;` |
|        - |  4146 | `	}` |
|        - |  4147 | `#endif` |
|        - |  4148 | `	/* Force the operands to be integer */` |
|       15 |  4149 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4150 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4151 | `	}` |
|       15 |  4152 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4153 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4154 | `	}` |
|        - |  4155 | `	/* Perform the requested operation */` |
|       15 |  4156 | `	a = pTos->x.iVal;` |
|       15 |  4157 | `	b = pNos->x.iVal;` |
|       15 |  4158 | `	switch(pInstr->iOp){` |
|        2 |  4159 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4160 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4161 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4162 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4163 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4164 | `	case PH7_OP_BAND:` |
|        5 |  4165 | `	default:          r = a&b; break;` |
|        - |  4166 | `	}` |
|        - |  4167 | `	/* Push the result */` |
|       15 |  4168 | `	pNos->x.iVal = r;` |
|       15 |  4169 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4170 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4171 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4172 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4173 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4174 | `	}` |
|       15 |  4175 | `	VmPopOperand(&pTos,1);` |
|       15 |  4176 | `	break;` |
|        - |  4177 | `				 }` |
|        - |  4178 | `/* OP_SHL * * *` |
|        - |  4179 | ` *` |
|        - |  4180 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4181 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4182 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4183 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4184 | ` */` |
|        - |  4185 | `/* OP_SHR * * *` |
|        - |  4186 | ` *` |
|        - |  4187 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4188 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4189 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4190 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4191 | ` */` |
|        9 |  4192 | `case PH7_OP_SHL:` |
|        - |  4193 | `case PH7_OP_SHR: {` |
|       19 |  4194 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4195 | `	sxi64 a,r;` |
|        - |  4196 | `	sxi32 b;` |
|        - |  4197 | `#ifdef UNTRUST` |
|        - |  4198 | `	if( pNos < pStack ){` |
|        - |  4199 | `		goto Abort;` |
|        - |  4200 | `	}` |
|        - |  4201 | `#endif` |
|        - |  4202 | `	/* Force the operands to be integer */` |
|       19 |  4203 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4204 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4205 | `	}` |
|       19 |  4206 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4207 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4208 | `	}` |
|        - |  4209 | `	/* Perform the requested operation */` |
|       19 |  4210 | `	a = pNos->x.iVal;` |
|       19 |  4211 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4212 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4213 | `		r = a << b;` |
|        6 |  4214 | `	}else{` |
|        9 |  4215 | `		r = a >> b;` |
|        - |  4216 | `	}` |
|        - |  4217 | `	/* Push the result */` |
|       19 |  4218 | `	pNos->x.iVal = r;` |
|       19 |  4219 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4220 | `	VmPopOperand(&pTos,1);` |
|       19 |  4221 | `	break;` |
|        - |  4222 | `				 }` |
|        - |  4223 | `/*  OP_SHL_STORE * * *` |
|        - |  4224 | ` *` |
|        - |  4225 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4226 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4227 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4228 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4229 | ` */` |
|        - |  4230 | `/* OP_SHR_STORE * * *` |
|        - |  4231 | ` *` |
|        - |  4232 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4233 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4234 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4235 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4236 | ` */` |
|        7 |  4237 | `case PH7_OP_SHL_STORE:` |
|        - |  4238 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4239 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4240 | `	ph7_value *pObj;` |
|        - |  4241 | `	sxi64 a,r;` |
|        - |  4242 | `	sxi32 b;` |
|        - |  4243 | `#ifdef UNTRUST` |
|        - |  4244 | `	if( pNos < pStack ){` |
|        - |  4245 | `		goto Abort;` |
|        - |  4246 | `	}` |
|        - |  4247 | `#endif` |
|        - |  4248 | `	/* Force the operands to be integer */` |
|       15 |  4249 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4250 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4251 | `	}` |
|       15 |  4252 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4253 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4254 | `	}` |
|        - |  4255 | `	/* Perform the requested operation */` |
|       15 |  4256 | `	a = pTos->x.iVal;` |
|       15 |  4257 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4258 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4259 | `		r = a << b;` |
|        4 |  4260 | `	}else{` |
|        9 |  4261 | `		r = a >> b;` |
|        - |  4262 | `	}` |
|        - |  4263 | `	/* Push the result */` |
|       15 |  4264 | `	pNos->x.iVal = r;` |
|       15 |  4265 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4266 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4267 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4268 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4269 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4270 | `	}` |
|       15 |  4271 | `	VmPopOperand(&pTos,1);` |
|       15 |  4272 | `	break;` |
|        - |  4273 | `				 }` |
|        - |  4274 | `/* CAT:  P1 * *` |
|        - |  4275 | ` *` |
|        - |  4276 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4277 | ` * back.` |
|        - |  4278 | ` */` |
|    60067 |  4279 | `case PH7_OP_CAT:{` |
|        - |  4280 | `	ph7_value *pNos,*pCur;` |
|   120136 |  4281 | `	if( pInstr->iP1 < 1 ){` |
|    93252 |  4282 | `		pNos = &pTos[-1];` |
|    46627 |  4283 | `	}else{` |
|    26886 |  4284 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4285 | `	}` |
|        - |  4286 | `#ifdef UNTRUST` |
|        - |  4287 | `	if( pNos < pStack ){` |
|        - |  4288 | `		goto Abort;` |
|        - |  4289 | `	}` |
|        - |  4290 | `#endif` |
|        - |  4291 | `	/* Force a string cast */` |
|   120136 |  4292 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      980 |  4293 | `		PH7_MemObjToString(pNos);` |
|      489 |  4294 | `	}` |
|   120136 |  4295 | `	pCur = &pNos[1];` |
|   242110 |  4296 | `	while( pCur <= pTos ){` |
|   121976 |  4297 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50450 |  4298 | `			PH7_MemObjToString(pCur);` |
|    25224 |  4299 | `		}` |
|        - |  4300 | `		/* Perform the concatenation */` |
|   121976 |  4301 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   121938 |  4302 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    60968 |  4303 | `		}` |
|   121976 |  4304 | `		SyBlobRelease(&pCur->sBlob);` |
|   121976 |  4305 | `		pCur++;` |
|        2 |  4306 | `	}` |
|   120136 |  4307 | `	pTos = pNos;` |
|   120136 |  4308 | `	break;` |
|        - |  4309 | `				}` |
|        - |  4310 | `/*  CAT_STORE: * * *` |
|        - |  4311 | ` *` |
|        - |  4312 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4313 | ` * back.` |
|        - |  4314 | ` */` |
|     3041 |  4315 | `case PH7_OP_CAT_STORE:{` |
|     6084 |  4316 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4317 | `	ph7_value *pObj;` |
|        - |  4318 | `#ifdef UNTRUST` |
|        - |  4319 | `	if( pNos < pStack ){` |
|        - |  4320 | `		goto Abort;` |
|        - |  4321 | `	}` |
|        - |  4322 | `#endif` |
|     6084 |  4323 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4324 | `		/* Force a string cast */` |
|      ! 0 |  4325 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4326 | `	}` |
|     6084 |  4327 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4328 | `		/* Force a string cast */` |
|      ! 0 |  4329 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4330 | `	}` |
|        - |  4331 | `	/* Perform the concatenation (Reverse order) */` |
|     6084 |  4332 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     6084 |  4333 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3041 |  4334 | `	}` |
|        - |  4335 | `	/* Perform the store operation */` |
|     6084 |  4336 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4337 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     6084 |  4338 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     6084 |  4339 | `		PH7_MemObjStore(pTos,pObj);` |
|     3041 |  4340 | `	}` |
|     6084 |  4341 | `	PH7_MemObjStore(pTos,pNos);` |
|     6084 |  4342 | `	VmPopOperand(&pTos,1);` |
|     6084 |  4343 | `	break;` |
|        - |  4344 | `				}` |
|        - |  4345 | `/* OP_AND: * * *` |
|        - |  4346 | ` *` |
|        - |  4347 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4348 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4349 | ` * stack.` |
|        - |  4350 | ` */` |
|        - |  4351 | `/* OP_OR: * * *` |
|        - |  4352 | ` *` |
|        - |  4353 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4354 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4355 | ` * stack.` |
|        - |  4356 | ` */` |
|    91019 |  4357 | `case PH7_OP_LAND:` |
|        - |  4358 | `case PH7_OP_LOR: {` |
|   182084 |  4359 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4360 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4361 | `#ifdef UNTRUST` |
|        - |  4362 | `	if( pNos < pStack ){` |
|        - |  4363 | `		goto Abort;` |
|        - |  4364 | `	}` |
|        - |  4365 | `#endif` |
|        - |  4366 | `	/* Force a boolean cast */` |
|   182084 |  4367 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4368 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4369 | `	}` |
|   182084 |  4370 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4371 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4372 | `	}` |
|   182084 |  4373 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   182084 |  4374 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   182084 |  4375 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4376 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    84170 |  4377 | `		v1 = and_logic[v1*3+v2];` |
|    42108 |  4378 | `	}else{` |
|        - |  4379 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|    97916 |  4380 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4381 | `	}` |
|   182084 |  4382 | `	if( v1 == 2 ){` |
|      ! 0 |  4383 | `		v1 = 1;` |
|      ! 0 |  4384 | `	}` |
|   182084 |  4385 | `	VmPopOperand(&pTos,1);` |
|   182084 |  4386 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   182084 |  4387 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   182084 |  4388 | `	break;` |
|        - |  4389 | `				 }` |
|        - |  4390 | `/* OP_LXOR: * * *` |
|        - |  4391 | ` *` |
|        - |  4392 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4393 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4394 | ` * stack.` |
|        - |  4395 | ` * According to the PHP language reference manual:` |
|        - |  4396 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4397 | ` *  TRUE,but not both.` |
|        - |  4398 | ` */` |
|        5 |  4399 | `case PH7_OP_LXOR:{` |
|       11 |  4400 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4401 | `	sxi32 v = 0;` |
|        - |  4402 | `#ifdef UNTRUST` |
|        - |  4403 | `	if( pNos < pStack ){` |
|        - |  4404 | `		goto Abort;` |
|        - |  4405 | `	}` |
|        - |  4406 | `#endif` |
|        - |  4407 | `	/* Force a boolean cast */` |
|       11 |  4408 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4409 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4410 | `	}` |
|       11 |  4411 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4412 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4413 | `	}` |
|       11 |  4414 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4415 | `		v = 1;` |
|        3 |  4416 | `	}` |
|       11 |  4417 | `	VmPopOperand(&pTos,1);` |
|       11 |  4418 | `	pTos->x.iVal = v;` |
|       11 |  4419 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4420 | `	break;` |
|        - |  4421 | `				 }` |
|        - |  4422 | `/* OP_EQ P1 P2 P3` |
|        - |  4423 | ` *` |
|        - |  4424 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4425 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4426 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4427 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4428 | ` */` |
|        - |  4429 | `/* OP_NEQ P1 P2 P3` |
|        - |  4430 | ` *` |
|        - |  4431 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4432 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4433 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4434 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4435 | ` */` |
|     3751 |  4436 | `case PH7_OP_EQ:` |
|        - |  4437 | `case PH7_OP_NEQ: {` |
|     7504 |  4438 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4439 | `	/* Perform the comparison and act accordingly */` |
|        - |  4440 | `#ifdef UNTRUST` |
|        - |  4441 | `	if( pNos < pStack ){` |
|        - |  4442 | `		goto Abort;` |
|        - |  4443 | `	}` |
|        - |  4444 | `#endif` |
|     7504 |  4445 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7504 |  4446 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       20 |  4447 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7495 |  4448 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7460 |  4449 | `		rc = rc == 0;` |
|     3731 |  4450 | `	}else{` |
|       28 |  4451 | `		rc = rc != 0;` |
|        - |  4452 | `	}` |
|     7504 |  4453 | `	VmPopOperand(&pTos,1);` |
|     7504 |  4454 | `	if( !pInstr->iP2 ){` |
|        - |  4455 | `		/* Push comparison result without taking the jump */` |
|     7504 |  4456 | `		PH7_MemObjRelease(pTos);` |
|     7504 |  4457 | `		pTos->x.iVal = rc;` |
|        - |  4458 | `		/* Invalidate any prior representation */` |
|     7504 |  4459 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3753 |  4460 | `	}else{` |
|      ! 0 |  4461 | `		if( rc ){` |
|        - |  4462 | `			/* Jump to the desired location */` |
|      ! 0 |  4463 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4464 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4465 | `		}` |
|        - |  4466 | `	}` |
|     7504 |  4467 | `	break;` |
|        - |  4468 | `				 }` |
|        - |  4469 | `/* OP_TEQ P1 P2 *` |
|        - |  4470 | ` *` |
|        - |  4471 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4472 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4473 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4474 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4475 | ` */` |
|   124047 |  4476 | `case PH7_OP_TEQ: {` |
|   248096 |  4477 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4478 | `	/* Perform the comparison and act accordingly */` |
|        - |  4479 | `#ifdef UNTRUST` |
|        - |  4480 | `	if( pNos < pStack ){` |
|        - |  4481 | `		goto Abort;` |
|        - |  4482 | `	}` |
|        - |  4483 | `#endif` |
|   248096 |  4484 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   248096 |  4485 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4486 | `		rc = 0;` |
|        2 |  4487 | `	}else{` |
|   248094 |  4488 | `		rc = rc == 0;` |
|        - |  4489 | `	}` |
|   248096 |  4490 | `	VmPopOperand(&pTos,1);` |
|   248096 |  4491 | `	if( !pInstr->iP2 ){` |
|        - |  4492 | `		/* Push comparison result without taking the jump */` |
|   248096 |  4493 | `		PH7_MemObjRelease(pTos);` |
|   248096 |  4494 | `		pTos->x.iVal = rc;` |
|        - |  4495 | `		/* Invalidate any prior representation */` |
|   248096 |  4496 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   124049 |  4497 | `	}else{` |
|      ! 0 |  4498 | `		if( rc ){` |
|        - |  4499 | `			/* Jump to the desired location */` |
|      ! 0 |  4500 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4501 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4502 | `		}` |
|        - |  4503 | `	}` |
|   248096 |  4504 | `	break;` |
|        - |  4505 | `				 }` |
|        - |  4506 | `/* OP_TNE P1 P2 *` |
|        - |  4507 | ` *` |
|        - |  4508 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4509 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4510 | ` * instruction.` |
|        - |  4511 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4512 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4513 | ` *` |
|        - |  4514 | ` */` |
|    97020 |  4515 | `case PH7_OP_TNE: {` |
|   194042 |  4516 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4517 | `	/* Perform the comparison and act accordingly */` |
|        - |  4518 | `#ifdef UNTRUST` |
|        - |  4519 | `	if( pNos < pStack ){` |
|        - |  4520 | `		goto Abort;` |
|        - |  4521 | `	}` |
|        - |  4522 | `#endif` |
|   194042 |  4523 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   194042 |  4524 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4525 | `		rc = 1;` |
|        2 |  4526 | `	}else{` |
|   194040 |  4527 | `		rc = rc != 0;` |
|        - |  4528 | `	}` |
|   194042 |  4529 | `	VmPopOperand(&pTos,1);` |
|   194042 |  4530 | `	if( !pInstr->iP2 ){` |
|        - |  4531 | `		/* Push comparison result without taking the jump */` |
|   194042 |  4532 | `		PH7_MemObjRelease(pTos);` |
|   194042 |  4533 | `		pTos->x.iVal = rc;` |
|        - |  4534 | `		/* Invalidate any prior representation */` |
|   194042 |  4535 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    97022 |  4536 | `	}else{` |
|      ! 0 |  4537 | `		if( rc ){` |
|        - |  4538 | `			/* Jump to the desired location */` |
|      ! 0 |  4539 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4540 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4541 | `		}` |
|        - |  4542 | `	}` |
|   194042 |  4543 | `	break;` |
|        - |  4544 | `				 }` |
|        - |  4545 | `/* OP_LT P1 P2 P3` |
|        - |  4546 | ` *` |
|        - |  4547 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4548 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4549 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4550 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4551 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4552 | ` *` |
|        - |  4553 | ` */` |
|        - |  4554 | `/* OP_LE P1 P2 P3` |
|        - |  4555 | ` *` |
|        - |  4556 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4557 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4558 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4559 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4560 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4561 | ` *` |
|        - |  4562 | ` */` |
|    99960 |  4563 | `case PH7_OP_LT:` |
|        - |  4564 | `case PH7_OP_LE: {` |
|   199966 |  4565 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4566 | `	/* Perform the comparison and act accordingly */` |
|        - |  4567 | `#ifdef UNTRUST` |
|        - |  4568 | `	if( pNos < pStack ){` |
|        - |  4569 | `		goto Abort;` |
|        - |  4570 | `	}` |
|        - |  4571 | `#endif` |
|   199966 |  4572 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   199966 |  4573 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4574 | `		rc = 0;` |
|   199962 |  4575 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      395 |  4576 | `		rc = rc < 1;` |
|      198 |  4577 | `	}else{` |
|   199564 |  4578 | `		rc = rc < 0;` |
|        - |  4579 | `	}` |
|   199966 |  4580 | `	VmPopOperand(&pTos,1);` |
|   199966 |  4581 | `	if( !pInstr->iP2 ){` |
|        - |  4582 | `		/* Push comparison result without taking the jump */` |
|   199966 |  4583 | `		PH7_MemObjRelease(pTos);` |
|   199966 |  4584 | `		pTos->x.iVal = rc;` |
|        - |  4585 | `		/* Invalidate any prior representation */` |
|   199966 |  4586 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   100006 |  4587 | `	}else{` |
|      ! 0 |  4588 | `		if( rc ){` |
|        - |  4589 | `			/* Jump to the desired location */` |
|      ! 0 |  4590 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4591 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4592 | `		}` |
|        - |  4593 | `	}` |
|   199966 |  4594 | `	break;` |
|        - |  4595 | `				}` |
|        - |  4596 | `/* OP_GT P1 P2 P3` |
|        - |  4597 | ` *` |
|        - |  4598 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4599 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4600 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4601 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4602 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4603 | ` *` |
|        - |  4604 | ` */` |
|        - |  4605 | `/* OP_GE P1 P2 P3` |
|        - |  4606 | ` *` |
|        - |  4607 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4608 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4609 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4610 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4611 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4612 | ` *` |
|        - |  4613 | ` */` |
|    46606 |  4614 | `case PH7_OP_GT:` |
|        - |  4615 | `case PH7_OP_GE: {` |
|    93214 |  4616 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4617 | `	/* Perform the comparison and act accordingly */` |
|        - |  4618 | `#ifdef UNTRUST` |
|        - |  4619 | `	if( pNos < pStack ){` |
|        - |  4620 | `		goto Abort;` |
|        - |  4621 | `	}` |
|        - |  4622 | `#endif` |
|    93214 |  4623 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    93214 |  4624 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4625 | `		rc = 0;` |
|    93210 |  4626 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    93058 |  4627 | `		rc = rc >= 0;` |
|    46530 |  4628 | `	}else{` |
|      150 |  4629 | `		rc = rc > 0;` |
|        - |  4630 | `	}` |
|    93214 |  4631 | `	VmPopOperand(&pTos,1);` |
|    93214 |  4632 | `	if( !pInstr->iP2 ){` |
|        - |  4633 | `		/* Push comparison result without taking the jump */` |
|    93214 |  4634 | `		PH7_MemObjRelease(pTos);` |
|    93214 |  4635 | `		pTos->x.iVal = rc;` |
|        - |  4636 | `		/* Invalidate any prior representation */` |
|    93214 |  4637 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    46608 |  4638 | `	}else{` |
|      ! 0 |  4639 | `		if( rc ){` |
|        - |  4640 | `			/* Jump to the desired location */` |
|      ! 0 |  4641 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4642 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4643 | `		}` |
|        - |  4644 | `	}` |
|    93214 |  4645 | `	break;` |
|        - |  4646 | `				}` |
|        - |  4647 | `/* OP_SEQ P1 P2 *` |
|        - |  4648 | ` * Strict string comparison.` |
|        - |  4649 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4650 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4651 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4652 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4653 | ` * use PH7_OP_EQ.` |
|        - |  4654 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4655 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4656 | ` */` |
|        - |  4657 | `/* OP_SNE P1 P2 *` |
|        - |  4658 | ` * Strict string comparison.` |
|        - |  4659 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4660 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4661 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4662 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4663 | ` * use PH7_OP_EQ.` |
|        - |  4664 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4665 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4666 | ` */` |
|       18 |  4667 | `case PH7_OP_SEQ:` |
|        - |  4668 | `case PH7_OP_SNE: {` |
|       38 |  4669 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4670 | `	SyString s1,s2;` |
|        - |  4671 | `	/* Perform the comparison and act accordingly */` |
|        - |  4672 | `#ifdef UNTRUST` |
|        - |  4673 | `	if( pNos < pStack ){` |
|        - |  4674 | `		goto Abort;` |
|        - |  4675 | `	}` |
|        - |  4676 | `#endif` |
|        - |  4677 | `	/* Force a string cast */` |
|       38 |  4678 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4679 | `		PH7_MemObjToString(pTos);` |
|        2 |  4680 | `	}` |
|       38 |  4681 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4682 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4683 | `	}` |
|       38 |  4684 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4685 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4686 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4687 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4688 | `		rc = rc != 0;` |
|      ! 0 |  4689 | `	}else{` |
|       38 |  4690 | `		rc = rc == 0;` |
|        - |  4691 | `	}` |
|       38 |  4692 | `	VmPopOperand(&pTos,1);` |
|       38 |  4693 | `	if( !pInstr->iP2 ){` |
|        - |  4694 | `		/* Push comparison result without taking the jump */` |
|       38 |  4695 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4696 | `		pTos->x.iVal = rc;` |
|        - |  4697 | `		/* Invalidate any prior representation */` |
|       38 |  4698 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4699 | `	}else{` |
|      ! 0 |  4700 | `		if( rc ){` |
|        - |  4701 | `			/* Jump to the desired location */` |
|      ! 0 |  4702 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4703 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4704 | `		}` |
|        - |  4705 | `	}` |
|       38 |  4706 | `	break;` |
|        - |  4707 | `				 }` |
|        - |  4708 | `/*` |
|        - |  4709 | ` * OP_LOAD_REF * * *` |
|        - |  4710 | ` * Push the index of a referenced object on the stack.` |
|        - |  4711 | ` */` |
|       57 |  4712 | `case PH7_OP_LOAD_REF: {` |
|        - |  4713 | `	sxu32 nIdx;` |
|        - |  4714 | `#ifdef UNTRUST` |
|        - |  4715 | `	if( pTos < pStack ){` |
|        - |  4716 | `		goto Abort;` |
|        - |  4717 | `	}` |
|        - |  4718 | `#endif` |
|        - |  4719 | `	/* Extract memory object index */` |
|      115 |  4720 | `	nIdx = pTos->nIdx;` |
|      115 |  4721 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4722 | `		/* Nullify the object */` |
|       95 |  4723 | `		PH7_MemObjRelease(pTos);` |
|        - |  4724 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4725 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4726 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4727 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4728 | `	}` |
|      115 |  4729 | `	break;` |
|        - |  4730 | `					  }` |
|        - |  4731 | `/*` |
|        - |  4732 | ` * OP_STORE_REF * * P3` |
|        - |  4733 | ` * Perform an assignment operation by reference.` |
|        - |  4734 | ` */` |
|       14 |  4735 | ` case PH7_OP_STORE_REF: {` |
|       30 |  4736 | `	 SyString sName = { 0 , 0 };` |
|        - |  4737 | `	 VmFrame *pFrameLocal;` |
|        - |  4738 | `	SyHashEntry *pEntry;` |
|        - |  4739 | `	sxu32 nIdx;` |
|        - |  4740 | `#ifdef UNTRUST` |
|        - |  4741 | `	if( pTos < pStack ){` |
|        - |  4742 | `		goto Abort;` |
|        - |  4743 | `	}` |
|        - |  4744 | `#endif` |
|       30 |  4745 | `	if( pInstr->p3 == 0 ){` |
|        - |  4746 | `		char *zName;` |
|        - |  4747 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4748 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4749 | `			/* Force a string cast */` |
|      ! 0 |  4750 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4751 | `		}` |
|      ! 0 |  4752 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4753 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4754 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4755 | `			if( zName ){` |
|      ! 0 |  4756 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4757 | `			}` |
|      ! 0 |  4758 | `		}` |
|      ! 0 |  4759 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4760 | `		pTos--;` |
|      ! 0 |  4761 | `	}else{` |
|       30 |  4762 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4763 | `	}` |
|       30 |  4764 | `	nIdx = pTos->nIdx;` |
|       30 |  4765 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4766 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4767 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4768 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4769 | `		}else{` |
|        - |  4770 | `			ph7_value *pObj;` |
|        - |  4771 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4772 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4773 | `			if( pObj == 0 ){` |
|      ! 0 |  4774 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4775 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4776 | `				goto Abort;` |
|        - |  4777 | `			}` |
|        - |  4778 | `			/* Perform the store operation */` |
|      ! 0 |  4779 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4780 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4781 | `		}` |
|       30 |  4782 | `	}else if( sName.nByte > 0){` |
|       30 |  4783 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4784 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4785 | `		}else{` |
|       30 |  4786 | `			pFrameLocal = pVm->pFrame;` |
|       30 |  4787 | `			while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4788 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  4789 | `				pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  4790 | `			}` |
|        - |  4791 | `			/* Query the local frame */` |
|       30 |  4792 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       30 |  4793 | `			if( pEntry ){` |
|      ! 0 |  4794 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4795 | `			}else{` |
|       30 |  4796 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       30 |  4797 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4798 | `					/* Insert in the $GLOBALS array */` |
|       26 |  4799 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       12 |  4800 | `				}` |
|       30 |  4801 | `				if( rc == SXRET_OK ){` |
|       30 |  4802 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       14 |  4803 | `				}` |
|        - |  4804 | `			}` |
|        - |  4805 | `		}` |
|       14 |  4806 | `	}` |
|       30 |  4807 | `	break;` |
|        - |  4808 | `				 }` |
|        - |  4809 | `/*` |
|        - |  4810 | ` * OP_UPLINK P1 * *` |
|        - |  4811 | ` * Link a variable to the top active VM frame.` |
|        - |  4812 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4813 | ` */` |
|       25 |  4814 | `case PH7_OP_UPLINK: {` |
|       52 |  4815 | `	if( pVm->pFrame->pParent ){` |
|       52 |  4816 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4817 | `		SyString sName;` |
|        - |  4818 | `		/* Perform the link */` |
|      104 |  4819 | `		while( pLink <= pTos ){` |
|       54 |  4820 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4821 | `				/* Force a string cast */` |
|      ! 0 |  4822 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4823 | `			}` |
|       54 |  4824 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  4825 | `			if( sName.nByte > 0 ){` |
|       54 |  4826 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  4827 | `			}` |
|       54 |  4828 | `			pLink++;` |
|        2 |  4829 | `		}` |
|       25 |  4830 | `	}` |
|       52 |  4831 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  4832 | `	break;` |
|        - |  4833 | `					}` |
|        - |  4834 | `/*` |
|        - |  4835 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4836 | ` * Push an exception in the corresponding container so that` |
|        - |  4837 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4838 | ` */` |
|       12 |  4839 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       26 |  4840 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4841 | `	VmFrame *pFrameLocal;` |
|       26 |  4842 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4843 | `	/* Create the exception frame */` |
|       26 |  4844 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       26 |  4845 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4846 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4847 | `		goto Abort;` |
|        - |  4848 | `	}` |
|        - |  4849 | `	/* Mark the special frame */` |
|       26 |  4850 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       26 |  4851 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4852 | `	/* Point to the frame that trigger the exception */` |
|       26 |  4853 | `	pFrameLocal = pFrameLocal->pParent;` |
|       28 |  4854 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        3 |  4855 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4856 | `	}` |
|       26 |  4857 | `	pException->pFrame = pFrameLocal;` |
|       26 |  4858 | `	break;` |
|        - |  4859 | `							}` |
|        - |  4860 | `/*` |
|        - |  4861 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4862 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4863 | ` */` |
|       12 |  4864 | `case PH7_OP_POP_EXCEPTION: {` |
|       26 |  4865 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       26 |  4866 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4867 | `		ph7_exception **apException;` |
|        - |  4868 | `		/* Pop the loaded exception */` |
|        7 |  4869 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        7 |  4870 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|        7 |  4871 | `			(void)SySetPop(&pVm->aException);` |
|        3 |  4872 | `		}` |
|        3 |  4873 | `	}` |
|       26 |  4874 | `	pException->pFrame = 0;` |
|        - |  4875 | `	/* Leave the exception frame */` |
|       26 |  4876 | `	VmLeaveFrame(&(*pVm));` |
|       26 |  4877 | `	break;` |
|        - |  4878 | `							}` |
|        - |  4879 |  |
|        - |  4880 | `/*` |
|        - |  4881 | ` * OP_THROW * P2 *` |
|        - |  4882 | ` * Throw an user exception.` |
|        - |  4883 | ` */` |
|       11 |  4884 | `case PH7_OP_THROW: {` |
|       24 |  4885 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       24 |  4886 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4887 | `#ifdef UNTRUST` |
|        - |  4888 | `	if( pTos < pStack ){` |
|        - |  4889 | `		goto Abort;` |
|        - |  4890 | `	}` |
|        - |  4891 | `#endif` |
|       28 |  4892 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4893 | `		/* Safely ignore the exception frame */` |
|        6 |  4894 | `		pFrameLocal = pFrameLocal->pParent;` |
|        2 |  4895 | `	}` |
|        - |  4896 | `	/* Tell the upper layer that an exception was thrown */` |
|       24 |  4897 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       24 |  4898 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       24 |  4899 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4900 | `		ph7_class *pException;` |
|        - |  4901 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4902 | `		 */` |
|       24 |  4903 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       24 |  4904 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4905 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4906 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4907 | `			if( rc == SXERR_ABORT ){` |
|        - |  4908 | `				/* Abort processing immediately */` |
|      ! 0 |  4909 | `				goto Abort;` |
|        - |  4910 | `			}` |
|      ! 0 |  4911 | `		}else{` |
|        - |  4912 | `			/* Throw the exception */` |
|       24 |  4913 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       24 |  4914 | `			if( rc == SXERR_ABORT ){` |
|        - |  4915 | `				/* Abort processing immediately */` |
|        9 |  4916 | `				goto Abort;` |
|        - |  4917 | `			}` |
|        - |  4918 | `		}` |
|        9 |  4919 | `	}else{` |
|        - |  4920 | `		/* Expecting a class instance */` |
|      ! 0 |  4921 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  4922 | `		if( rc == SXERR_ABORT ){` |
|        - |  4923 | `			/* Abort processing immediately */` |
|      ! 0 |  4924 | `			goto Abort;` |
|        - |  4925 | `		}` |
|        - |  4926 | `	}` |
|        - |  4927 | `	/* Pop the top entry */` |
|       16 |  4928 | `	VmPopOperand(&pTos,1);` |
|        - |  4929 | `	/* Perform an unconditional jump */` |
|       16 |  4930 | `	pc = nJump - 1;` |
|       16 |  4931 | `	break;` |
|        - |  4932 | `				   }` |
|        - |  4933 | `/*` |
|        - |  4934 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  4935 | ` * Prepare a foreach step.` |
|        - |  4936 | ` */` |
|     4669 |  4937 | `case PH7_OP_FOREACH_INIT: {` |
|     9340 |  4938 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4939 | `	void *pName;` |
|        - |  4940 | `#ifdef UNTRUST` |
|        - |  4941 | `	if( pTos < pStack ){` |
|        - |  4942 | `		goto Abort;` |
|        - |  4943 | `	}` |
|        - |  4944 | `#endif` |
|     9340 |  4945 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4946 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  4947 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4948 | `			/* Force a string cast */` |
|      ! 0 |  4949 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4950 | `		}` |
|        - |  4951 | `		/* Duplicate name */` |
|      ! 0 |  4952 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4953 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4954 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4955 | `		}` |
|      ! 0 |  4956 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4957 | `	}` |
|     9340 |  4958 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  4959 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4960 | `			/* Force a string cast */` |
|      ! 0 |  4961 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4962 | `		}` |
|        - |  4963 | `		/* Duplicate name */` |
|      ! 0 |  4964 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4965 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4966 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4967 | `		}` |
|      ! 0 |  4968 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4969 | `	}` |
|        - |  4970 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     9340 |  4971 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4972 | `		/* Jump out of the loop */` |
|      ! 0 |  4973 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4974 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4975 | `		}` |
|      ! 0 |  4976 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4977 | `	}else{` |
|        - |  4978 | `		ph7_foreach_step *pStep;` |
|     9340 |  4979 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     9340 |  4980 | `		if( pStep == 0 ){` |
|      ! 0 |  4981 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4982 | `			/* Jump out of the loop */` |
|      ! 0 |  4983 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4984 | `		}else{` |
|        - |  4985 | `			/* Zero the structure */` |
|     9340 |  4986 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4987 | `			/* Prepare the step */` |
|     9340 |  4988 | `			pStep->iFlags = pInfo->iFlags;` |
|     9340 |  4989 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     9332 |  4990 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4991 | `				/* Reset the internal loop cursor */` |
|     9332 |  4992 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4993 | `				/* Mark the step */` |
|     9332 |  4994 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     9332 |  4995 | `				pStep->xIter.pMap = pMap;` |
|     9332 |  4996 | `				pMap->iRef++;` |
|     4667 |  4997 | `			}else{` |
|        9 |  4998 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4999 | `				/* Reset the loop cursor */` |
|        9 |  5000 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|        - |  5001 | `				/* Mark the step */` |
|        9 |  5002 | `				pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  5003 | `				pStep->xIter.pThis = pThis;` |
|        9 |  5004 | `				pThis->iRef++;` |
|        - |  5005 | `			}` |
|        - |  5006 | `		}` |
|     9340 |  5007 | `		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5008 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5009 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5010 | `			/* Jump out of the loop */` |
|      ! 0 |  5011 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5012 | `		}` |
|        - |  5013 | `	}` |
|     9340 |  5014 | `	VmPopOperand(&pTos,1);` |
|     9340 |  5015 | `	break;` |
|        - |  5016 | `						  }` |
|        - |  5017 | `/*` |
|        - |  5018 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5019 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5020 | ` */` |
|    74709 |  5021 | `case PH7_OP_FOREACH_STEP: {` |
|   149420 |  5022 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5023 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5024 | `	ph7_value *pValue;` |
|        - |  5025 | `	VmFrame *pFrameLocal;` |
|        - |  5026 | `	/* Peek the last step */` |
|   149420 |  5027 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   149420 |  5028 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   149420 |  5029 | `	pFrameLocal = pVm->pFrame;` |
|   149420 |  5030 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5031 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  5032 | `		pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  5033 | `	}` |
|   149420 |  5034 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   149396 |  5035 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5036 | `		ph7_hashmap_node *pNode;` |
|        - |  5037 | `		/* Extract the current node value */` |
|   149396 |  5038 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   149396 |  5039 | `		if( pNode == 0 ){` |
|        - |  5040 | `			/* No more entry to process */` |
|     9332 |  5041 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     9332 |  5042 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5043 | `				/* Break the reference with the last element */` |
|        5 |  5044 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  5045 | `			}` |
|        - |  5046 | `			/* Automatically reset the loop cursor */` |
|     9332 |  5047 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5048 | `			/* Cleanup the mess left behind */` |
|     9332 |  5049 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     9332 |  5050 | `			SySetPop(&pInfo->aStep);` |
|     9332 |  5051 | `			PH7_HashmapUnref(pMap);` |
|     4667 |  5052 | `		}else{` |
|   140066 |  5053 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      408 |  5054 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      408 |  5055 | `				if( pKey ){` |
|      408 |  5056 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      203 |  5057 | `				}` |
|      203 |  5058 | `			}` |
|   140066 |  5059 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5060 | `				SyHashEntry *pEntry;` |
|        - |  5061 | `				/* Pass by reference */` |
|       13 |  5062 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       13 |  5063 | `				if( pEntry ){` |
|       13 |  5064 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        7 |  5065 | `				}else{` |
|      ! 0 |  5066 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5067 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5068 | `				}` |
|        7 |  5069 | `			}else{` |
|        - |  5070 | `				/* Make a copy of the entry value */` |
|   140054 |  5071 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   140054 |  5072 | `				if( pValue ){` |
|   140054 |  5073 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    70026 |  5074 | `				}` |
|        - |  5075 | `			}` |
|        - |  5076 | `		}` |
|    74699 |  5077 | `	}else{` |
|       25 |  5078 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5079 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5080 | `		SyHashEntry *pEntry;` |
|        - |  5081 | `		/* Point to the next attribute */` |
|       29 |  5082 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5083 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5084 | `			/* Check access permission */` |
|       31 |  5085 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5086 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5087 | `					break; /* Access is granted */` |
|        - |  5088 | `			}` |
|        1 |  5089 | `		}` |
|       25 |  5090 | `		if( pEntry == 0 ){` |
|        - |  5091 | `			/* Clean up the mess left behind */` |
|        9 |  5092 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5093 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5094 | `				/* Break the reference with the last element */` |
|        3 |  5095 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5096 | `			}` |
|        9 |  5097 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5098 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5099 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5100 | `		}else{` |
|       17 |  5101 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5102 | `			ph7_value *pAttrValue;` |
|       17 |  5103 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5104 | `				/* Fill with the current attribute name */` |
|       17 |  5105 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5106 | `				if( pKey ){` |
|       17 |  5107 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5108 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5109 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5110 | `				}` |
|        8 |  5111 | `			}` |
|        - |  5112 | `			/* Extract attribute value */` |
|       17 |  5113 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5114 | `			if( pAttrValue ){` |
|       17 |  5115 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5116 | `					/* Pass by reference */` |
|        3 |  5117 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5118 | `					if( pEntry ){` |
|        3 |  5119 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5120 | `					}else{` |
|      ! 0 |  5121 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5122 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5123 | `					}` |
|        2 |  5124 | `				}else{` |
|        - |  5125 | `					/* Make a copy of the attribute value */` |
|       15 |  5126 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5127 | `					if( pValue ){` |
|       15 |  5128 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5129 | `					}` |
|        - |  5130 | `				}` |
|        8 |  5131 | `			}` |
|        - |  5132 | `		}` |
|        - |  5133 | `	}` |
|   149420 |  5134 | `	break;` |
|        - |  5135 | `						  }` |
|        - |  5136 | `/*` |
|        - |  5137 | ` * OP_MEMBER P1 P2` |
|        - |  5138 | ` * Load class attribute/method on the stack.` |
|        - |  5139 | ` */` |
|     1855 |  5140 | `case PH7_OP_MEMBER: {` |
|        - |  5141 | `	ph7_class_instance *pThis;` |
|        - |  5142 | `	ph7_value *pNos;` |
|        - |  5143 | `	SyString sName;` |
|     3712 |  5144 | `	if( !pInstr->iP1 ){` |
|     3654 |  5145 | `		pNos = &pTos[-1];` |
|        - |  5146 | `#ifdef UNTRUST` |
|        - |  5147 | `		if( pNos < pStack ){` |
|        - |  5148 | `			goto Abort;` |
|        - |  5149 | `		}` |
|        - |  5150 | `#endif` |
|     3654 |  5151 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5152 | `			ph7_class *pClass;` |
|        - |  5153 | `			/* Class already instantiated */` |
|     3654 |  5154 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5155 | `			/* Point to the instantiated class */` |
|     3654 |  5156 | `			pClass = pThis->pClass;` |
|        - |  5157 | `			/* Extract attribute name first */` |
|     3654 |  5158 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     3654 |  5159 | `			if( pInstr->iP2 ){` |
|        - |  5160 | `				/* Method call */` |
|      158 |  5161 | `				ph7_class_method *pMeth = 0;` |
|      158 |  5162 | `				if( sName.nByte > 0 ){` |
|        - |  5163 | `					/* Extract the target method */` |
|      158 |  5164 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       78 |  5165 | `				}` |
|      158 |  5166 | `				if( pMeth == 0 ){` |
|      ! 0 |  5167 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5168 | `						&pClass->sName,&sName` |
|        - |  5169 | `						);` |
|        - |  5170 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5171 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5172 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5173 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5174 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5175 | `				}else{` |
|        - |  5176 | `					/* Push method name on the stack */` |
|      158 |  5177 | `					PH7_MemObjRelease(pTos);` |
|      158 |  5178 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      158 |  5179 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5180 | `				}` |
|      158 |  5181 | `				pTos->nIdx = SXU32_HIGH;` |
|       80 |  5182 | `			}else{` |
|        - |  5183 | `				/* Attribute access */` |
|     3498 |  5184 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5185 | `				SyHashEntry *pEntry;` |
|        - |  5186 | `				/* Extract the target attribute */` |
|     3498 |  5187 | `				if( sName.nByte > 0 ){` |
|     3498 |  5188 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3498 |  5189 | `					if( pEntry ){` |
|        - |  5190 | `						/* Point to the attribute value */` |
|     3496 |  5191 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1747 |  5192 | `					}` |
|     1748 |  5193 | `				}` |
|     3498 |  5194 | `				if( pObjAttr == 0 ){` |
|        - |  5195 | `					/* No such attribute,load null */` |
|        4 |  5196 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5197 | `						&pClass->sName,&sName);` |
|        - |  5198 | `					/* Call the __get magic method if available */` |
|        3 |  5199 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5200 | `				}` |
|     3498 |  5201 | `				VmPopOperand(&pTos,1);` |
|        - |  5202 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5203 | `				 * This is due to the following case:` |
|        - |  5204 | `				 *     (new TestClass())->foo;` |
|        - |  5205 | `				 */` |
|     3498 |  5206 | `				pThis->iRef++;` |
|     3498 |  5207 | `				PH7_MemObjRelease(pTos);` |
|     3498 |  5208 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3498 |  5209 | `				if( pObjAttr ){` |
|     3496 |  5210 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5211 | `					/* Check attribute access */` |
|     3496 |  5212 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5213 | `						/* Load attribute */` |
|     3496 |  5214 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3496 |  5215 | `						if( pValue ){` |
|     3496 |  5216 | `							if( pThis->iRef < 2 ){` |
|        - |  5217 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5218 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5219 | `								 */` |
|        3 |  5220 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5221 | `							}else{` |
|        - |  5222 | `								/* Simple load */` |
|     3494 |  5223 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5224 | `							}` |
|     3496 |  5225 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3494 |  5226 | `								if( pThis->iRef > 1 ){` |
|        - |  5227 | `									/* Load attribute index */` |
|     3492 |  5228 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1745 |  5229 | `								}` |
|     1746 |  5230 | `							}` |
|     1747 |  5231 | `						}` |
|     1747 |  5232 | `					}` |
|     1747 |  5233 | `				}` |
|        - |  5234 | `				/* Safely unreference the object */` |
|     3498 |  5235 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5236 | `			}` |
|     1828 |  5237 | `		}else{` |
|      ! 0 |  5238 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5239 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5240 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5241 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5242 | `		}` |
|     1828 |  5243 | `	}else{` |
|        - |  5244 | `		/* Static member access using class name */` |
|       59 |  5245 | `		pNos = pTos;` |
|       59 |  5246 | `		pThis = 0;` |
|       59 |  5247 | `		if( !pInstr->p3 ){` |
|       57 |  5248 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       57 |  5249 | `			pNos--;` |
|        - |  5250 | `#ifdef UNTRUST` |
|        - |  5251 | `			if( pNos < pStack ){` |
|        - |  5252 | `				goto Abort;` |
|        - |  5253 | `			}` |
|        - |  5254 | `#endif` |
|       29 |  5255 | `		}else{` |
|        - |  5256 | `			/* Attribute name already computed */` |
|        3 |  5257 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5258 | `		}` |
|       59 |  5259 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|       59 |  5260 | `			ph7_class *pClass = 0;` |
|       59 |  5261 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5262 | `				/* Class already instantiated */` |
|      ! 0 |  5263 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5264 | `				pClass = pThis->pClass;` |
|      ! 0 |  5265 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5266 | `			}else{` |
|        - |  5267 | `				/* Try to extract the target class */` |
|       59 |  5268 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       88 |  5269 | `					pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pNos->sBlob),` |
|       29 |  5270 | `						SyBlobLength(&pNos->sBlob),FALSE,0);` |
|       29 |  5271 | `				}` |
|        - |  5272 | `			}` |
|       59 |  5273 | `			if( pClass == 0 ){` |
|        - |  5274 | `				/* Undefined class */` |
|      ! 0 |  5275 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5276 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5277 | `					);` |
|      ! 0 |  5278 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5279 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5280 | `				}` |
|      ! 0 |  5281 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5282 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5283 | `			}else{` |
|       59 |  5284 | `				if( pInstr->iP2 ){` |
|        - |  5285 | `					/* Method call */` |
|       25 |  5286 | `					ph7_class_method *pMeth = 0;` |
|       25 |  5287 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5288 | `						/* Extract the target method */` |
|       25 |  5289 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       12 |  5290 | `					}` |
|       25 |  5291 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5292 | `						if( pMeth ){` |
|      ! 0 |  5293 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5294 | `								&pClass->sName,&sName` |
|        - |  5295 | `								);` |
|      ! 0 |  5296 | `						}else{` |
|      ! 0 |  5297 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5298 | `								&pClass->sName,&sName` |
|        - |  5299 | `								);` |
|        - |  5300 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5301 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5302 | `						}` |
|        - |  5303 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5304 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5305 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5306 | `						}` |
|      ! 0 |  5307 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5308 | `					}else{` |
|        - |  5309 | `						/* Push method name on the stack */` |
|       25 |  5310 | `						PH7_MemObjRelease(pTos);` |
|       25 |  5311 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       25 |  5312 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5313 | `					}` |
|       25 |  5314 | `					pTos->nIdx = SXU32_HIGH;` |
|       13 |  5315 | `				}else{` |
|        - |  5316 | `					/* Attribute access */` |
|       35 |  5317 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5318 | `					/* Check for special ::class pseudo-constant */` |
|       49 |  5319 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       28 |  5320 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5321 | `						/* ::class returns the fully qualified class name */` |
|        - |  5322 | `						/* Pop the attribute name from the stack */` |
|       27 |  5323 | `						if( !pInstr->p3 ){` |
|       27 |  5324 | `							VmPopOperand(&pTos,1);` |
|       13 |  5325 | `						}` |
|       27 |  5326 | `						PH7_MemObjRelease(pTos);` |
|        - |  5327 | `						/* Load the class name */` |
|       27 |  5328 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       27 |  5329 | `						pTos->nIdx = SXU32_HIGH;` |
|       14 |  5330 | `					}else{` |
|        - |  5331 | `						/* Extract the target attribute */` |
|        9 |  5332 | `						if( sName.nByte > 0 ){` |
|        9 |  5333 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        4 |  5334 | `						}` |
|        9 |  5335 | `						if( pAttr == 0 ){` |
|        - |  5336 | `							/* No such attribute,load null */` |
|      ! 0 |  5337 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5338 | `								&pClass->sName,&sName);` |
|        - |  5339 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5340 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5341 | `						}` |
|        - |  5342 | `						/* Pop the attribute name from the stack */` |
|        9 |  5343 | `						if( !pInstr->p3 ){` |
|        7 |  5344 | `							VmPopOperand(&pTos,1);` |
|        3 |  5345 | `						}` |
|        9 |  5346 | `						PH7_MemObjRelease(pTos);` |
|        9 |  5347 | `						pTos->nIdx = SXU32_HIGH;` |
|        9 |  5348 | `						if( pAttr ){` |
|        9 |  5349 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5350 | `								/* Access to a non static attribute */` |
|      ! 0 |  5351 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5352 | `									&pClass->sName,&pAttr->sName` |
|        - |  5353 | `									);` |
|      ! 0 |  5354 | `							}else{` |
|        - |  5355 | `								ph7_value *pValue;` |
|        - |  5356 | `								/* Check if the access to the attribute is allowed */` |
|        9 |  5357 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5358 | `									/* Load the desired attribute */` |
|        9 |  5359 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|        9 |  5360 | `									if( pValue ){` |
|        9 |  5361 | `										PH7_MemObjLoad(pValue,pTos);` |
|        9 |  5362 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5363 | `											/* Load index number */` |
|        3 |  5364 | `											pTos->nIdx = pAttr->nIdx;` |
|        1 |  5365 | `										}` |
|        4 |  5366 | `									}` |
|        4 |  5367 | `								}` |
|        - |  5368 | `							}` |
|        4 |  5369 | `						}` |
|        - |  5370 | `					}` |
|        - |  5371 | `				}` |
|       59 |  5372 | `				if( pThis ){` |
|        - |  5373 | `					/* Safely unreference the object */` |
|      ! 0 |  5374 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5375 | `				}` |
|        - |  5376 | `			}` |
|       30 |  5377 | `		}else{` |
|        - |  5378 | `			/* Pop operands */` |
|      ! 0 |  5379 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5380 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5381 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5382 | `			}` |
|      ! 0 |  5383 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5384 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5385 | `		}` |
|        - |  5386 | `	}` |
|     3712 |  5387 | `	break;` |
|        - |  5388 | `					}` |
|        - |  5389 | `/*` |
|        - |  5390 | ` * OP_NEW P1 * * *` |
|        - |  5391 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5392 | ` */` |
|      274 |  5393 | `case PH7_OP_NEW: {` |
|      550 |  5394 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      550 |  5395 | `	ph7_class *pClass = 0;` |
|        - |  5396 | `	ph7_class_instance *pNew;` |
|      550 |  5397 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5398 | `		/* Try to extract the desired class */` |
|      824 |  5399 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      548 |  5400 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      274 |  5401 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5402 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5403 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5404 | `	}` |
|      550 |  5405 | `	if( pClass == 0 ){` |
|        - |  5406 | `		/* No such class */` |
|      ! 0 |  5407 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined,PH7 is loading NULL",` |
|      ! 0 |  5408 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5409 | `			);` |
|      ! 0 |  5410 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5411 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5412 | `			/* Pop given arguments */` |
|      ! 0 |  5413 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5414 | `		}` |
|      ! 0 |  5415 | `	}else{` |
|        - |  5416 | `		ph7_class_method *pCons;` |
|        - |  5417 | `		/* Create a new class instance */` |
|      550 |  5418 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      550 |  5419 | `		if( pNew == 0 ){` |
|      ! 0 |  5420 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5421 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5422 | `				&pClass->sName` |
|        - |  5423 | `			);` |
|      ! 0 |  5424 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5425 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5426 | `				/* Pop given arguments */` |
|      ! 0 |  5427 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5428 | `			}` |
|      ! 0 |  5429 | `			break;` |
|        - |  5430 | `		}` |
|        - |  5431 | `		/* Check if a constructor is available */` |
|      550 |  5432 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      550 |  5433 | `		if( pCons == 0 ){` |
|      492 |  5434 | `			SyString *pName = &pClass->sName;` |
|        - |  5435 | `			/* Check for a constructor with the same base class name */` |
|      492 |  5436 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      245 |  5437 | `		}` |
|      550 |  5438 | `		if( pCons ){` |
|        - |  5439 | `			/* Call the class constructor */` |
|       60 |  5440 | `			SySetReset(&aArg);` |
|      108 |  5441 | `			while( pArg < pTos ){` |
|       50 |  5442 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       50 |  5443 | `				pArg++;` |
|        2 |  5444 | `			}` |
|       60 |  5445 | `			if( pVm->bErrReport ){` |
|        - |  5446 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5447 | `				sxu32 n;` |
|       17 |  5448 | `				n = SySetUsed(&aArg);` |
|        - |  5449 | `				/* Emit a notice for missing arguments */` |
|       45 |  5450 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       29 |  5451 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       29 |  5452 | `					if( pFuncArg ){` |
|       29 |  5453 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5454 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5455 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5456 | `						}` |
|       14 |  5457 | `					}` |
|       29 |  5458 | `					n++;` |
|        1 |  5459 | `				}` |
|        8 |  5460 | `			}` |
|       60 |  5461 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5462 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       60 |  5463 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5464 | `				pNew->iRef = 1;` |
|      ! 0 |  5465 | `			}` |
|       29 |  5466 | `		}` |
|      550 |  5467 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5468 | `			/* Pop given arguments */` |
|       44 |  5469 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       21 |  5470 | `		}` |
|      550 |  5471 | `		PH7_MemObjRelease(pTos);` |
|      550 |  5472 | `		pTos->x.pOther = pNew;` |
|      550 |  5473 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5474 | `	}` |
|      550 |  5475 | `	break;` |
|        - |  5476 | `				 }` |
|        - |  5477 | `/*` |
|        - |  5478 | ` * OP_CLONE * * *` |
|        - |  5479 | ` * Perfome a clone operation.` |
|        - |  5480 | ` */` |
|       23 |  5481 | `case PH7_OP_CLONE: {` |
|        - |  5482 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5483 | `#ifdef UNTRUST` |
|        - |  5484 | `	if( pTos < pStack ){` |
|        - |  5485 | `		goto Abort;` |
|        - |  5486 | `	}` |
|        - |  5487 | `#endif` |
|        - |  5488 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5489 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5490 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5491 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5492 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5493 | `		break;` |
|        - |  5494 | `	}` |
|        - |  5495 | `	/* Point to the source */` |
|       44 |  5496 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5497 | `	/* Perform the clone operation */` |
|       44 |  5498 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5499 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5500 | `	if( pClone == 0 ){` |
|      ! 0 |  5501 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5502 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5503 | `	}else{` |
|        - |  5504 | `		/* Load the cloned object */` |
|       44 |  5505 | `		pTos->x.pOther = pClone;` |
|       44 |  5506 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5507 | `	}` |
|       44 |  5508 | `	break;` |
|        - |  5509 | `				   }` |
|        - |  5510 | `/*` |
|        - |  5511 | ` * OP_SWITCH * * P3` |
|        - |  5512 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5513 | ` */` |
|       18 |  5514 | `case PH7_OP_SWITCH: {` |
|       38 |  5515 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5516 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5517 | `	ph7_value sValue,sCaseValue;` |
|        - |  5518 | `	sxu32 n,nEntry;` |
|        - |  5519 | `#ifdef UNTRUST` |
|        - |  5520 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5521 | `		goto Abort;` |
|        - |  5522 | `	}` |
|        - |  5523 | `#endif` |
|        - |  5524 | `	/* Point to the case table  */` |
|       38 |  5525 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5526 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5527 | `	/* Select the appropriate case block to execute */` |
|       38 |  5528 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5529 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5530 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5531 | `		pCase = &aCase[n];` |
|       92 |  5532 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5533 | `		/* Execute the case expression first */` |
|       92 |  5534 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5535 | `		/* Compare the two expression */` |
|       92 |  5536 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5537 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5538 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5539 | `		if( rc == 0 ){` |
|        - |  5540 | `			/* Value match,jump to this block */` |
|       38 |  5541 | `			pc = pCase->nStart - 1;` |
|       38 |  5542 | `			break;` |
|        - |  5543 | `		}` |
|       29 |  5544 | `	}` |
|       38 |  5545 | `	VmPopOperand(&pTos,1);` |
|       38 |  5546 | `	if( n >= nEntry ){` |
|        - |  5547 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5548 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5549 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5550 | `		}else{` |
|        - |  5551 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5552 | `			pc = pSwitch->nOut - 1;` |
|        - |  5553 | `		}` |
|      ! 0 |  5554 | `	}` |
|       38 |  5555 | `	break;` |
|        - |  5556 | `					}` |
|        - |  5557 | `/*` |
|        - |  5558 | ` * OP_CALL P1 * *` |
|        - |  5559 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5560 | ` *  function on the stack.` |
|        - |  5561 | ` */` |
|   273348 |  5562 | `case PH7_OP_CALL: {` |
|   546742 |  5563 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5564 | `	SyHashEntry *pEntry;` |
|        - |  5565 | `	SyString sName;` |
|        - |  5566 | `	/* Extract function name */` |
|   546742 |  5567 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5568 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5569 | `			ph7_value sResult;` |
|      ! 0 |  5570 | `			SySetReset(&aArg);` |
|      ! 0 |  5571 | `			while( pArg < pTos ){` |
|      ! 0 |  5572 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5573 | `				pArg++;` |
|      ! 0 |  5574 | `			}` |
|      ! 0 |  5575 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5576 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5577 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5578 | `			SySetReset(&aArg);` |
|        - |  5579 | `			/* Pop given arguments */` |
|      ! 0 |  5580 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5581 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5582 | `			}` |
|        - |  5583 | `			/* Copy result */` |
|      ! 0 |  5584 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5585 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5586 | `		}else{` |
|        3 |  5587 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5588 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5589 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5590 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5591 | `			}else{` |
|        - |  5592 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5593 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5594 | `			}` |
|        - |  5595 | `			/* Pop given arguments */` |
|        3 |  5596 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5597 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5598 | `			}` |
|        - |  5599 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5600 | `			PH7_MemObjRelease(pTos);` |
|        - |  5601 | `		}` |
|   273115 |  5602 | `		break;` |
|        - |  5603 | `	}` |
|   546740 |  5604 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5605 | `	/* Check for a compiled function first (namespace-aware) */` |
|   546740 |  5606 | `	pEntry = VmNsAwareHashLookup(pVm,&pVm->hFunction,&sName);` |
|   546740 |  5607 | `	if( pEntry ){` |
|        - |  5608 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5609 | `		ph7_class_instance *pThis;` |
|        - |  5610 | `		ph7_value *pFrameStack;` |
|        - |  5611 | `		ph7_vm_func *pVmFunc;` |
|        - |  5612 | `		ph7_class *pSelf;` |
|        - |  5613 | `		VmFrame *pFrame;` |
|        - |  5614 | `		ph7_value *pObj;` |
|        - |  5615 | `		VmSlot sArg;` |
|        - |  5616 | `		sxu32 n;` |
|        - |  5617 | `		/* initialize fields */` |
|    11778 |  5618 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    11778 |  5619 | `		pThis = 0;` |
|    11778 |  5620 | `		pSelf = 0;` |
|    11778 |  5621 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5622 | `			ph7_class_method *pMeth;` |
|        - |  5623 | `			/* Class method call */` |
|     1328 |  5624 | `			ph7_value *pTarget = &pTos[-1];` |
|     1328 |  5625 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5626 | `				/* Extract the 'this' pointer */` |
|     1328 |  5627 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5628 | `					/* Instance already loaded */` |
|     1298 |  5629 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1298 |  5630 | `					pThis->iRef++;` |
|     1298 |  5631 | `					pSelf = pThis->pClass;` |
|      648 |  5632 | `				}` |
|     1328 |  5633 | `				if( pSelf == 0 ){` |
|       31 |  5634 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5635 | `						/* "Late Static Binding" class name */` |
|       37 |  5636 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       12 |  5637 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       12 |  5638 | `					}` |
|       31 |  5639 | `					if( pSelf == 0 ){` |
|        7 |  5640 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 |  5641 | `					}` |
|       15 |  5642 | `				}` |
|     1328 |  5643 | `				if( pThis == 0  ){` |
|       31 |  5644 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       31 |  5645 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5646 | `						/* Safely ignore the exception frame */` |
|      ! 0 |  5647 | `						pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  5648 | `					}` |
|       31 |  5649 | `					if( pFrameLocal->pParent ){` |
|        - |  5650 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5651 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5652 | `						if( pThis ){` |
|       13 |  5653 | `							pThis->iRef++;` |
|        6 |  5654 | `						}` |
|        9 |  5655 | `					}` |
|       15 |  5656 | `				}` |
|     1328 |  5657 | `				VmPopOperand(&pTos,1);` |
|     1328 |  5658 | `				PH7_MemObjRelease(pTos);` |
|        - |  5659 | `				/* Synchronize pointers */` |
|     1328 |  5660 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5661 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5662 | `				 * user have already computed the random generated unique class method name` |
|        - |  5663 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5664 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5665 | `				 */` |
|     1328 |  5666 | `				while( pArg < pStack ){` |
|      ! 0 |  5667 | `					pArg++;` |
|      ! 0 |  5668 | `				}` |
|     1328 |  5669 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5670 | `					/* Check if the call is allowed */` |
|     1328 |  5671 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1328 |  5672 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        5 |  5673 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5674 | `							/* Pop given arguments */` |
|      ! 0 |  5675 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5676 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5677 | `							}` |
|        - |  5678 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5679 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5680 | `							break;` |
|        - |  5681 | `						}` |
|        2 |  5682 | `					}` |
|      663 |  5683 | `				}` |
|      663 |  5684 | `			}` |
|      663 |  5685 | `		}` |
|        - |  5686 | `		/* Check The recursion limit */` |
|    11778 |  5687 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5688 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5689 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5690 | `				&pVmFunc->sName);` |
|        - |  5691 | `			/* Pop given arguments */` |
|        3 |  5692 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5693 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5694 | `			}` |
|        - |  5695 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5696 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5697 | `			break;` |
|        - |  5698 | `		}` |
|    11776 |  5699 | `		if( pVmFunc->pNextName ){` |
|        - |  5700 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      123 |  5701 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       61 |  5702 | `		}` |
|        - |  5703 | `		/* Extract the formal argument set */` |
|    11776 |  5704 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5705 | `		/* Create a new VM frame  */` |
|    11776 |  5706 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    11776 |  5707 | `		if( rc != SXRET_OK ){` |
|        - |  5708 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5709 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5710 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5711 | `				&pVmFunc->sName);` |
|        - |  5712 | `			/* Pop given arguments */` |
|      ! 0 |  5713 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5714 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5715 | `			}` |
|        - |  5716 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5717 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5718 | `			break;` |
|        - |  5719 | `		}` |
|    11776 |  5720 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5721 | `			/* Install the '$this' variable */` |
|        - |  5722 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1308 |  5723 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1308 |  5724 | `			if( pObj ){` |
|        - |  5725 | `				/* Reflect the change */` |
|     1308 |  5726 | `				pObj->x.pOther = pThis;` |
|     1308 |  5727 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      653 |  5728 | `			}` |
|      653 |  5729 | `		}` |
|    11776 |  5730 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5731 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5732 | `			/* Install static variables */` |
|      ! 0 |  5733 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5734 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5735 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5736 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5737 | `					/* Initialize the static variables */` |
|      ! 0 |  5738 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5739 | `					if( pObj ){` |
|        - |  5740 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5741 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5742 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5743 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5744 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5745 | `						}` |
|      ! 0 |  5746 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5747 | `					}else{` |
|      ! 0 |  5748 | `						continue;` |
|        - |  5749 | `					}` |
|      ! 0 |  5750 | `				}` |
|        - |  5751 | `				/* Install in the current frame */` |
|      ! 0 |  5752 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5753 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5754 | `			}` |
|      ! 0 |  5755 | `		}` |
|        - |  5756 | `		/* Push arguments in the local frame */` |
|    11776 |  5757 | `		n = 0;` |
|    32874 |  5758 | `		while( pArg < pTos ){` |
|    21100 |  5759 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    20950 |  5760 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5761 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5762 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5763 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5764 | `						goto Abort;` |
|        - |  5765 | `					}` |
|      ! 0 |  5766 | `				}` |
|        - |  5767 | `				/* Make sure the given arguments are of the correct type */` |
|    20950 |  5768 | `				if( aFormalArg[n].nType > 0 ){` |
|     1088 |  5769 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5770 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5771 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5772 | `						ph7_class *pClass;` |
|        - |  5773 | `						/* Try to extract the desired class */` |
|      ! 0 |  5774 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5775 | `						if( pClass ){` |
|      ! 0 |  5776 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5777 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5778 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5779 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5780 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5781 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5782 | `								}` |
|      ! 0 |  5783 | `							}else{` |
|        - |  5784 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5785 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5786 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5787 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5788 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5789 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5790 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5791 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5792 | `								}` |
|        - |  5793 | `							}` |
|      ! 0 |  5794 | `						}` |
|     1088 |  5795 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5796 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5797 | `						/* Cast to the desired type */` |
|      ! 0 |  5798 | `						xCast(pArg);` |
|      ! 0 |  5799 | `					}` |
|      543 |  5800 | `				}` |
|    20950 |  5801 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5802 | `					/* Pass by reference */` |
|       48 |  5803 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5804 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5805 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5806 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5807 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5808 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5809 | `						}` |
|        - |  5810 | `						/* Switch to pass by value */` |
|      ! 0 |  5811 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5812 | `					}else{` |
|        - |  5813 | `						SyHashEntry *pRefEntry;` |
|        - |  5814 | `						/* Install the referenced variable in the private function frame */` |
|       48 |  5815 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       48 |  5816 | `						if( pRefEntry == 0 ){` |
|       71 |  5817 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       46 |  5818 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       48 |  5819 | `							sArg.nIdx = pArg->nIdx;` |
|       48 |  5820 | `							sArg.pUserData = 0;` |
|       48 |  5821 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  5822 | `						}` |
|       48 |  5823 | `						pObj = 0;` |
|        - |  5824 | `					}` |
|       25 |  5825 | `				}else{` |
|        - |  5826 | `					/* Pass by value,make a copy of the given argument */` |
|    20904 |  5827 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5828 | `				}` |
|    10476 |  5829 | `			}else{` |
|        - |  5830 | `				char zName[32];` |
|        - |  5831 | `				SyString sArgName;` |
|        - |  5832 | `				/* Set a dummy name */` |
|      152 |  5833 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      152 |  5834 | `				sArgName.zString = zName;` |
|        - |  5835 | `				/* Annonymous argument */` |
|      152 |  5836 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5837 | `			}` |
|    21100 |  5838 | `			if( pObj ){` |
|    21054 |  5839 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5840 | `				/* Insert argument index  */` |
|    21054 |  5841 | `				sArg.nIdx = pObj->nIdx;` |
|    21054 |  5842 | `				sArg.pUserData = 0;` |
|    21054 |  5843 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    10526 |  5844 | `			}` |
|    21100 |  5845 | `			PH7_MemObjRelease(pArg);` |
|    21100 |  5846 | `			pArg++;` |
|    21100 |  5847 | `			++n;` |
|        2 |  5848 | `		}` |
|        - |  5849 | `		/* Set up closure environment */` |
|    11776 |  5850 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5851 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  5852 | `			ph7_value *pValue;` |
|        - |  5853 | `			sxu32 iEnv;` |
|        9 |  5854 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  5855 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  5856 | `				pEnv = &aEnv[iEnv];` |
|       17 |  5857 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  5858 | `					/* Do not install null value */` |
|        9 |  5859 | `					continue;` |
|        - |  5860 | `				}` |
|        9 |  5861 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  5862 | `				if( pValue == 0 ){` |
|      ! 0 |  5863 | `					continue;` |
|        - |  5864 | `				}` |
|        - |  5865 | `				/* Invalidate any prior representation */` |
|        9 |  5866 | `				PH7_MemObjRelease(pValue);` |
|        - |  5867 | `				/* Duplicate bound variable value */` |
|        9 |  5868 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  5869 | `			}` |
|        4 |  5870 | `		}` |
|        - |  5871 | `		/* Process default values */` |
|    13614 |  5872 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1840 |  5873 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1834 |  5874 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1834 |  5875 | `				if( pObj ){` |
|        - |  5876 | `					/* Evaluate the default value and extract it's result */` |
|     1834 |  5877 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1834 |  5878 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5879 | `						goto Abort;` |
|        - |  5880 | `					}` |
|        - |  5881 | `					/* Insert argument index */` |
|     1834 |  5882 | `					sArg.nIdx = pObj->nIdx;` |
|     1834 |  5883 | `					sArg.pUserData = 0;` |
|     1834 |  5884 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  5885 | `					/* Make sure the default argument is of the correct type */` |
|     1834 |  5886 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5887 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5888 | `						/* Cast to the desired type */` |
|      ! 0 |  5889 | `						xCast(pObj);` |
|      ! 0 |  5890 | `					}` |
|      916 |  5891 | `				}` |
|      916 |  5892 | `			}` |
|     1840 |  5893 | `			++n;` |
|        2 |  5894 | `		}` |
|        - |  5895 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  5896 | `		 * does not return anything.` |
|        - |  5897 | `		 */` |
|    11776 |  5898 | `		PH7_MemObjRelease(pTos);` |
|    11776 |  5899 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  5900 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    11776 |  5901 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    11776 |  5902 | `		if( pFrameStack == 0 ){` |
|        - |  5903 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5904 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5905 | `				&pVmFunc->sName);` |
|      ! 0 |  5906 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5907 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5908 | `			}` |
|      ! 0 |  5909 | `			break;` |
|        - |  5910 | `		}` |
|    11776 |  5911 | `		if( pSelf ){` |
|        - |  5912 | `			/* Push class name */` |
|     1326 |  5913 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      662 |  5914 | `		}` |
|        - |  5915 | `		/* Increment nesting level */` |
|    11776 |  5916 | `		pVm->nRecursionDepth++;` |
|        - |  5917 | `		/* Execute function body */` |
|    11776 |  5918 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  5919 | `		/* Decrement nesting level */` |
|    11776 |  5920 | `		pVm->nRecursionDepth--;` |
|    11776 |  5921 | `		if( pSelf ){` |
|        - |  5922 | `			/* Pop class name */` |
|     1326 |  5923 | `			(void)SySetPop(&pVm->aSelf);` |
|      662 |  5924 | `		}` |
|        - |  5925 | `		/* Cleanup the mess left behind */` |
|    11776 |  5926 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  5927 | `			/* Return by reference,reflect that */` |
|        9 |  5928 | `			if( n != SXU32_HIGH ){` |
|        9 |  5929 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  5930 | `				sxu32 i;` |
|        - |  5931 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  5932 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  5933 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  5934 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  5935 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5936 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5937 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  5938 | `								&pVmFunc->sName);` |
|      ! 0 |  5939 | `						}` |
|      ! 0 |  5940 | `						n = SXU32_HIGH;` |
|      ! 0 |  5941 | `						break;` |
|        - |  5942 | `					}` |
|        3 |  5943 | `				}` |
|        5 |  5944 | `			}else{` |
|      ! 0 |  5945 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5946 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5947 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  5948 | `						&pVmFunc->sName);` |
|      ! 0 |  5949 | `				}` |
|        - |  5950 | `			}` |
|        9 |  5951 | `			pTos->nIdx = n;` |
|        4 |  5952 | `		}` |
|        - |  5953 | `		/* Cleanup the mess left behind */` |
|    11776 |  5954 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  5955 | `			/* An exception was throw in this frame */` |
|        7 |  5956 | `			pFrame = pFrame->pParent;` |
|        7 |  5957 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  5958 | `				/* Pop the resutlt */` |
|        5 |  5959 | `				VmPopOperand(&pTos,1);` |
|        - |  5960 | `				/* Jump to this destination */` |
|        5 |  5961 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  5962 | `				rc = PH7_OK;` |
|        3 |  5963 | `			}else{` |
|        3 |  5964 | `				if( pFrame->pParent ){` |
|        3 |  5965 | `					rc = PH7_EXCEPTION;` |
|        2 |  5966 | `				}else{` |
|        - |  5967 | `					/* Continue normal execution */` |
|      ! 0 |  5968 | `					rc = PH7_OK;` |
|        - |  5969 | `				}` |
|        - |  5970 | `			}` |
|        3 |  5971 | `		}` |
|        - |  5972 | `		/* Free the operand stack */` |
|    11776 |  5973 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  5974 | `		/* Leave the frame */` |
|    11776 |  5975 | `		VmLeaveFrame(&(*pVm));` |
|    11776 |  5976 | `		if( rc == PH7_ABORT ){` |
|        - |  5977 | `			/* Abort processing immeditaley */` |
|        7 |  5978 | `			goto Abort;` |
|    11770 |  5979 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5980 | `			goto Exception;` |
|        - |  5981 | `		}` |
|     5885 |  5982 | `	}else{` |
|        - |  5983 | `		ph7_user_func *pFunc;` |
|        - |  5984 | `		ph7_context sCtx;` |
|        - |  5985 | `		ph7_value sRet;` |
|        - |  5986 | `		/* Look for an installed foreign function.` |
|        - |  5987 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  5988 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  5989 | `		 * extract the short name (last component after \) and try that.` |
|        - |  5990 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  5991 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  5992 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   534964 |  5993 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   534964 |  5994 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  5995 | `			/* Compiler-qualified: try short name as global fallback */` |
|       11 |  5996 | `			const char *zShort = sName.zString;` |
|        - |  5997 | `			sxu32 i;` |
|      149 |  5998 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      139 |  5999 | `				if( sName.zString[i] == '\\' ){` |
|       15 |  6000 | `					zShort = &sName.zString[i + 1];` |
|        7 |  6001 | `				}` |
|       70 |  6002 | `			}` |
|       11 |  6003 | `			if( zShort != sName.zString ){` |
|       11 |  6004 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       11 |  6005 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        5 |  6006 | `			}` |
|        5 |  6007 | `		}` |
|   534964 |  6008 | `		if( pEntry == 0 ){` |
|        - |  6009 | `			/* Call to undefined function */` |
|        5 |  6010 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6011 | `			/* Pop given arguments */` |
|        5 |  6012 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6013 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6014 | `			}` |
|        - |  6015 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6016 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6017 | `			break;` |
|        - |  6018 | `		}` |
|   534960 |  6019 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6020 | `		/* Start collecting function arguments */` |
|   534960 |  6021 | `		SySetReset(&aArg);` |
|  1436536 |  6022 | `		while( pArg < pTos ){` |
|   901578 |  6023 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   901578 |  6024 | `			pArg++;` |
|        2 |  6025 | `		}` |
|        - |  6026 | `		/* Assume a null return value */` |
|   534960 |  6027 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6028 | `		/* Init the call context */` |
|   534960 |  6029 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6030 | `		/* Call the foreign function */` |
|   534960 |  6031 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6032 | `		/* Release the call context */` |
|   534960 |  6033 | `		VmReleaseCallContext(&sCtx);` |
|   534960 |  6034 | `		if( rc == PH7_ABORT ){` |
|      463 |  6035 | `			goto Abort;` |
|   534498 |  6036 | `		}else if( rc == PH7_EXCEPTION ){` |
|        7 |  6037 | `			VmFrame *pFrm = pVm->pFrame;` |
|       13 |  6038 | `			while( pFrm->pParent && (pFrm->iFlags & VM_FRAME_EXCEPTION) ){` |
|        7 |  6039 | `				pFrm = pFrm->pParent;` |
|        1 |  6040 | `			}` |
|        7 |  6041 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6042 | `				/* Exception was NOT caught, propagate */` |
|      ! 0 |  6043 | `				goto Exception;` |
|        - |  6044 | `			}` |
|        - |  6045 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6046 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6047 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6048 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6049 | `			}` |
|        - |  6050 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6051 | `			VmPopOperand(&pTos,1);` |
|        - |  6052 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6053 | `			pFrm = pVm->pFrame;` |
|        7 |  6054 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6055 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6056 | `			}` |
|        7 |  6057 | `			break;` |
|        - |  6058 | `		}` |
|   534492 |  6059 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6060 | `			/* Pop function name and arguments */` |
|   517208 |  6061 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   258625 |  6062 | `		}` |
|        - |  6063 | `		/* Save foreign function return value */` |
|   534492 |  6064 | `		PH7_MemObjStore(&sRet,pTos);` |
|   534492 |  6065 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6066 | `	}` |
|   546258 |  6067 | `	break;` |
|        - |  6068 | `				  }` |
|        - |  6069 | `/*` |
|        - |  6070 | ` * OP_CONSUME: P1 * *` |
|        - |  6071 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6072 | ` */` |
|    10475 |  6073 | `case PH7_OP_CONSUME: {` |
|    20952 |  6074 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    20952 |  6075 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6076 |  |
|    20952 |  6077 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    20952 |  6078 | `	pCur = pOut;` |
|        - |  6079 | `	/* Start the consume process  */` |
|    41902 |  6080 | `	while( pOut <= pTos ){` |
|        - |  6081 | `		/* Force a string cast */` |
|    20952 |  6082 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      204 |  6083 | `			PH7_MemObjToString(pOut);` |
|      101 |  6084 | `		}` |
|    20952 |  6085 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6086 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6087 | `			/* Invoke the output consumer callback */` |
|    11340 |  6088 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    11340 |  6089 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6090 | `				/* Increment output length */` |
|     4894 |  6091 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     2446 |  6092 | `			}` |
|    11340 |  6093 | `			SyBlobRelease(&pOut->sBlob);` |
|    11340 |  6094 | `			if( rc == SXERR_ABORT ){` |
|        - |  6095 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6096 | `				goto Abort;` |
|        - |  6097 | `			}` |
|     5669 |  6098 | `		}` |
|    20952 |  6099 | `		pOut++;` |
|        2 |  6100 | `	}` |
|    20952 |  6101 | `	pTos = &pCur[-1];` |
|    20950 |  6102 | `	break;` |
|        - |  6103 | `					 }` |
|        - |  6104 |  |
|        - |  6105 | `		} /* Switch() */` |
|  9372220 |  6106 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6107 | `	} /* For(;;) */` |
|    14586 |  6108 | `Done:` |
|    29174 |  6109 | `	SySetRelease(&aArg);` |
|    29174 |  6110 | `	return SXRET_OK;` |
|      238 |  6111 | `Abort:` |
|      477 |  6112 | `	SySetRelease(&aArg);` |
|     1661 |  6113 | `	while( pTos >= pStack ){` |
|     1185 |  6114 | `		PH7_MemObjRelease(pTos);` |
|     1185 |  6115 | `		pTos--;` |
|        1 |  6116 | `	}` |
|      477 |  6117 | `	return PH7_ABORT;` |
|        1 |  6118 | `Exception:` |
|        3 |  6119 | `	SySetRelease(&aArg);` |
|        5 |  6120 | `	while( pTos >= pStack ){` |
|        3 |  6121 | `		PH7_MemObjRelease(pTos);` |
|        3 |  6122 | `		pTos--;` |
|        1 |  6123 | `	}` |
|        3 |  6124 | `	return PH7_EXCEPTION;` |
|    14827 |  6125 |  |
|        - |  6126 | `/*` |
|        - |  6127 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6128 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6129 | ` * See block-comment on that function for additional information.` |
|        - |  6130 | ` */` |
|    14220 |  6131 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6132 |  |
|        - |  6133 | `	ph7_value *pStack;` |
|        - |  6134 | `	sxi32 rc;` |
|        - |  6135 | `	/* Allocate a new operand stack */` |
|    14222 |  6136 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    14222 |  6137 | `	if( pStack == 0 ){` |
|      ! 0 |  6138 | `		return SXERR_MEM;` |
|        - |  6139 | `	}` |
|        - |  6140 | `	/* Execute the program */` |
|    14222 |  6141 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  6142 | `	/* Free the operand stack */` |
|    14222 |  6143 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6144 | `	/* Execution result */` |
|    14222 |  6145 | `	return rc;` |
|     7112 |  6146 |  |
|        - |  6147 | `/*` |
|        - |  6148 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6149 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6150 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6151 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6152 | ` * execution ends.` |
|        - |  6153 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6154 | ` * additional information.` |
|        - |  6155 | ` */` |
|     2064 |  6156 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6157 |  |
|        - |  6158 | `	VmShutdownCB *pEntry;` |
|        - |  6159 | `	ph7_value *apArg[10];` |
|        - |  6160 | `	sxu32 n,nEntry;` |
|        - |  6161 | `	int i;` |
|        - |  6162 | `	/* Point to the stack of registered callbacks */` |
|     2066 |  6163 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    22706 |  6164 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    20642 |  6165 | `		apArg[i] = 0;` |
|    10322 |  6166 | `	}` |
|     2068 |  6167 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6168 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6169 | `		if( pEntry ){` |
|        - |  6170 | `			/* Prepare callback arguments if any */` |
|        3 |  6171 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6172 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6173 | `					break;` |
|        - |  6174 | `				}` |
|      ! 0 |  6175 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6176 | `			}` |
|        - |  6177 | `			/* Invoke the callback */` |
|        3 |  6178 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6179 | `			/*` |
|        - |  6180 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6181 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6182 | `			 */` |
|        3 |  6183 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6184 | `			if( pEntry ){` |
|        3 |  6185 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6186 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6187 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6188 | `				}` |
|        1 |  6189 | `			}` |
|        1 |  6190 | `		}` |
|        2 |  6191 | `	}` |
|     2066 |  6192 | `	SySetReset(&pVm->aShutdown);` |
|     2066 |  6193 |  |
|        - |  6194 | `/*` |
|        - |  6195 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6196 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6197 | ` * See block-comment on that function for additional information.` |
|        - |  6198 | ` */` |
|     2072 |  6199 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6200 |  |
|        - |  6201 | `	/* Make sure we are ready to execute this program */` |
|     2074 |  6202 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6203 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6204 | `	}` |
|        - |  6205 | `	/* Set the execution magic number  */` |
|     2074 |  6206 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6207 | `	/* Execute the program */` |
|     2074 |  6208 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6209 | `	/* Invoke any shutdown callbacks */` |
|     2070 |  6210 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6211 | `	/*` |
|        - |  6212 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6213 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6214 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6215 | `	 */` |
|     2070 |  6216 | `	return SXRET_OK;` |
|     1038 |  6217 |  |
|        - |  6218 | `/*` |
|        - |  6219 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6220 | ` * the desired message.` |
|        - |  6221 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6222 | ` * in 'api.c' for additional information.` |
|        - |  6223 | ` */` |
|      350 |  6224 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6225 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6226 | `	SyString *pString /* Message to output */` |
|        - |  6227 | `	)` |
|        2 |  6228 |  |
|      352 |  6229 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      352 |  6230 | `	sxi32 rc = SXRET_OK;` |
|        - |  6231 | `	/* Call the output consumer */` |
|      352 |  6232 | `	if( pString->nByte > 0 ){` |
|      352 |  6233 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      352 |  6234 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6235 | `			/* Increment output length */` |
|       17 |  6236 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6237 | `		}` |
|      175 |  6238 | `	}` |
|      352 |  6239 | `	return rc;` |
|        2 |  6240 |  |
|        - |  6241 | `/*` |
|        - |  6242 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6243 | ` * callback to consume the formatted message.` |
|        - |  6244 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6245 | ` * in 'api.c' for additional information.` |
|        - |  6246 | ` */` |
|        2 |  6247 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6248 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6249 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6250 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6251 | `	)` |
|        1 |  6252 |  |
|        3 |  6253 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6254 | `	sxi32 rc = SXRET_OK;` |
|        - |  6255 | `	SyBlob sWorker;` |
|        - |  6256 | `	/* Format the message and call the output consumer */` |
|        3 |  6257 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6258 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6259 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6260 | `		/* Consume the formatted message */` |
|        3 |  6261 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6262 | `	}` |
|        3 |  6263 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6264 | `		/* Increment output length */` |
|      ! 0 |  6265 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6266 | `	}` |
|        - |  6267 | `	/* Release the working buffer */` |
|        3 |  6268 | `	SyBlobRelease(&sWorker);` |
|        3 |  6269 | `	return rc;` |
|        1 |  6270 |  |
|        - |  6271 | `/*` |
|        - |  6272 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6273 | ` * This function never fail and always return a pointer` |
|        - |  6274 | ` * to a null terminated string.` |
|        - |  6275 | ` */` |
|       10 |  6276 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6277 |  |
|       11 |  6278 | `	const char *zOp = "Unknown     ";` |
|       11 |  6279 | `	switch(nOp){` |
|        3 |  6280 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6281 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6282 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6283 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6284 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6285 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6286 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6287 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6288 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6289 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6290 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6291 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6292 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6293 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6294 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6295 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6296 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6297 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6298 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6299 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6300 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6301 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6302 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6303 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6304 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6305 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6306 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6307 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6308 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6309 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6310 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6311 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6312 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6313 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6314 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6315 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6316 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6317 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6318 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6319 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6320 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6321 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6322 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6323 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6324 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6325 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6326 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6327 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6328 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6329 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|      ! 0 |  6330 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  6331 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6332 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6333 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6334 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6335 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6336 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6337 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6338 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6339 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6340 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6341 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6342 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6343 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6344 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6345 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6346 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6347 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6348 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6349 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6350 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6351 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6352 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6353 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6354 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6355 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6356 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6357 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6358 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6359 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6360 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6361 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6362 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6363 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6364 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6365 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6366 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6367 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6368 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6369 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6370 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6371 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6372 | `	default:` |
|      ! 0 |  6373 | `		break;` |
|        - |  6374 | `	}` |
|       11 |  6375 | `	return zOp;` |
|        1 |  6376 |  |
|        - |  6377 | `/*` |
|        - |  6378 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6379 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6380 | ` * is responsible of consuming the generated dump.` |
|        - |  6381 | ` */` |
|        2 |  6382 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6383 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6384 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6385 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6386 | `	)` |
|        1 |  6387 |  |
|        - |  6388 | `	sxi32 rc;` |
|        3 |  6389 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6390 | `	return rc;` |
|        1 |  6391 |  |
|        - |  6392 | `/*` |
|        - |  6393 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6394 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6395 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6396 | ` * in 'compile.c' for additional information.` |
|        - |  6397 | ` */` |
|        8 |  6398 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6399 |  |
|        9 |  6400 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6401 | `	/* Evaluate and expand constant value */` |
|        9 |  6402 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6403 |  |
|        - |  6404 | `/*` |
|        - |  6405 | ` * Section:` |
|        - |  6406 | ` *  Function handling functions.` |
|        - |  6407 | ` * Status:` |
|        - |  6408 | ` *    Stable.` |
|        - |  6409 | ` */` |
|        - |  6410 | `/*` |
|        - |  6411 | ` * int func_num_args(void)` |
|        - |  6412 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6413 | ` * Parameters` |
|        - |  6414 | ` *   None.` |
|        - |  6415 | ` * Return` |
|        - |  6416 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6417 | ` *  or -1 if called from the globe scope.` |
|        - |  6418 | ` */` |
|      906 |  6419 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6420 |  |
|        - |  6421 | `	VmFrame *pFrame;` |
|        - |  6422 | `	ph7_vm *pVm;` |
|        - |  6423 | `	/* Point to the target VM */` |
|      908 |  6424 | `	pVm = pCtx->pVm;` |
|        - |  6425 | `	/* Current frame */` |
|      908 |  6426 | `	pFrame = pVm->pFrame;` |
|      908 |  6427 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6428 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6429 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6430 | `	}` |
|      908 |  6431 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6432 | `		SXUNUSED(nArg);` |
|      ! 0 |  6433 | `		SXUNUSED(apArg);` |
|        - |  6434 | `		/* Global frame,return -1 */` |
|      ! 0 |  6435 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6436 | `		return SXRET_OK;` |
|        - |  6437 | `	}` |
|        - |  6438 | `	/* Total number of arguments passed to the enclosing function */` |
|      908 |  6439 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      908 |  6440 | `	ph7_result_int(pCtx,nArg);` |
|      908 |  6441 | `	return SXRET_OK;` |
|      455 |  6442 |  |
|        - |  6443 | `/*` |
|        - |  6444 | ` * value func_get_arg(int $arg_num)` |
|        - |  6445 | ` *   Return an item from the argument list.` |
|        - |  6446 | ` * Parameters` |
|        - |  6447 | ` *  Argument number(index start from zero).` |
|        - |  6448 | ` * Return` |
|        - |  6449 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6450 | ` */` |
|       22 |  6451 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6452 |  |
|       24 |  6453 | `	ph7_value *pObj = 0;` |
|       24 |  6454 | `	VmSlot *pSlot = 0;` |
|        - |  6455 | `	VmFrame *pFrame;` |
|        - |  6456 | `	ph7_vm *pVm;` |
|        - |  6457 | `	/* Point to the target VM */` |
|       24 |  6458 | `	pVm = pCtx->pVm;` |
|        - |  6459 | `	/* Current frame */` |
|       24 |  6460 | `	pFrame = pVm->pFrame;` |
|       24 |  6461 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6462 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6463 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6464 | `	}` |
|       24 |  6465 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6466 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6467 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6468 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6469 | `		return SXRET_OK;` |
|        - |  6470 | `	}` |
|        - |  6471 | `	/* Extract the desired index */` |
|       21 |  6472 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  6473 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6474 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6475 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6476 | `		return SXRET_OK;` |
|        - |  6477 | `	}` |
|        - |  6478 | `	/* Extract the desired argument */` |
|       21 |  6479 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  6480 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6481 | `			/* Return the desired argument */` |
|       21 |  6482 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  6483 | `		}else{` |
|        - |  6484 | `			/* No such argument,return false */` |
|      ! 0 |  6485 | `			ph7_result_bool(pCtx,0);` |
|        - |  6486 | `		}` |
|       11 |  6487 | `	}else{` |
|        - |  6488 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6489 | `		ph7_result_bool(pCtx,0);` |
|        - |  6490 | `	}` |
|       21 |  6491 | `	return SXRET_OK;` |
|       13 |  6492 |  |
|        - |  6493 | `/*` |
|        - |  6494 | ` * array func_get_args_byref(void)` |
|        - |  6495 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6496 | ` * Parameters` |
|        - |  6497 | ` *  None.` |
|        - |  6498 | ` * Return` |
|        - |  6499 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6500 | ` *  member of the current user-defined function's argument list.` |
|        - |  6501 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6502 | ` * NOTE:` |
|        - |  6503 | ` *  Arguments are returned to the array by reference.` |
|        - |  6504 | ` */` |
|        2 |  6505 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6506 |  |
|        - |  6507 | `	ph7_value *pArray;` |
|        - |  6508 | `	VmFrame *pFrame;` |
|        - |  6509 | `	VmSlot *aSlot;` |
|        - |  6510 | `	sxu32 n;` |
|        - |  6511 | `	/* Point to the current frame */` |
|        3 |  6512 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6513 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6514 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6515 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6516 | `	}` |
|        3 |  6517 | `	if( pFrame->pParent == 0 ){` |
|        - |  6518 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6519 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6520 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6521 | `		return SXRET_OK;` |
|        - |  6522 | `	}` |
|        - |  6523 | `	/* Create a new array */` |
|        3 |  6524 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6525 | `	if( pArray == 0 ){` |
|      ! 0 |  6526 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6527 | `		SXUNUSED(apArg);` |
|      ! 0 |  6528 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6529 | `		return SXRET_OK;` |
|        - |  6530 | `	}` |
|        - |  6531 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6532 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6533 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6534 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6535 | `	}` |
|        - |  6536 | `	/* Return the freshly created array */` |
|        3 |  6537 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6538 | `	return SXRET_OK;` |
|        2 |  6539 |  |
|        - |  6540 | `/*` |
|        - |  6541 | ` * array func_get_args(void)` |
|        - |  6542 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6543 | ` * Parameters` |
|        - |  6544 | ` *  None.` |
|        - |  6545 | ` * Return` |
|        - |  6546 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6547 | ` *  member of the current user-defined function's argument list.` |
|        - |  6548 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6549 | ` */` |
|       62 |  6550 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6551 |  |
|       64 |  6552 | `	ph7_value *pObj = 0;` |
|        - |  6553 | `	ph7_value *pArray;` |
|        - |  6554 | `	VmFrame *pFrame;` |
|        - |  6555 | `	VmSlot *aSlot;` |
|        - |  6556 | `	sxu32 n;` |
|        - |  6557 | `	/* Point to the current frame */` |
|       64 |  6558 | `	pFrame = pCtx->pVm->pFrame;` |
|       64 |  6559 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6560 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6561 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6562 | `	}` |
|       64 |  6563 | `	if( pFrame->pParent == 0 ){` |
|        - |  6564 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6565 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6566 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6567 | `		return SXRET_OK;` |
|        - |  6568 | `	}` |
|        - |  6569 | `	/* Create a new array */` |
|       64 |  6570 | `	pArray = ph7_context_new_array(pCtx);` |
|       64 |  6571 | `	if( pArray == 0 ){` |
|      ! 0 |  6572 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6573 | `		SXUNUSED(apArg);` |
|      ! 0 |  6574 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6575 | `		return SXRET_OK;` |
|        - |  6576 | `	}` |
|        - |  6577 | `	/* Start filling the array with the given arguments */` |
|       64 |  6578 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      192 |  6579 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      130 |  6580 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      130 |  6581 | `		if( pObj ){` |
|      130 |  6582 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       64 |  6583 | `		}` |
|       66 |  6584 | `	}` |
|        - |  6585 | `	/* Return the freshly created array */` |
|       64 |  6586 | `	ph7_result_value(pCtx,pArray);` |
|       64 |  6587 | `	return SXRET_OK;` |
|       33 |  6588 |  |
|        - |  6589 | `/*` |
|        - |  6590 | ` * bool function_exists(string $name)` |
|        - |  6591 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6592 | ` * Parameters` |
|        - |  6593 | ` *  The name of the desired function.` |
|        - |  6594 | ` * Return` |
|        - |  6595 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6596 | ` */` |
|     1644 |  6597 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6598 |  |
|        - |  6599 | `	const char *zName;` |
|        - |  6600 | `	ph7_vm *pVm;` |
|        - |  6601 | `	int nLen;` |
|        - |  6602 | `	int res;` |
|     1646 |  6603 | `	if( nArg < 1 ){` |
|        - |  6604 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6605 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6606 | `		return SXRET_OK;` |
|        - |  6607 | `	}` |
|        - |  6608 | `	/* Point to the target VM */` |
|     1646 |  6609 | `	pVm = pCtx->pVm;` |
|        - |  6610 | `	/* Extract the function name */` |
|     1646 |  6611 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6612 | `	/* Assume the function is not defined */` |
|     1646 |  6613 | `	res = 0;` |
|        - |  6614 | `	/* Perform the lookup */` |
|     2466 |  6615 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1640 |  6616 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6617 | `			/* Function is defined */` |
|      206 |  6618 | `			res = 1;` |
|      102 |  6619 | `	}` |
|     1646 |  6620 | `	ph7_result_bool(pCtx,res);` |
|     1646 |  6621 | `	return SXRET_OK;` |
|      824 |  6622 |  |
|        - |  6623 | `/*` |
|        - |  6624 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6625 | ` * [i.e: Whether it is callable or not].` |
|        - |  6626 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6627 | ` */` |
|    16002 |  6628 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6629 |  |
|    16004 |  6630 | `	int res = 0;` |
|    16004 |  6631 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6632 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6633 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6634 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6635 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6636 | `		if( pMethod && CallInvoke ){` |
|        - |  6637 | `			ph7_value sResult;` |
|        - |  6638 | `			sxi32 rc;` |
|        - |  6639 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6640 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6641 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6642 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6643 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6644 | `			}` |
|      ! 0 |  6645 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6646 | `		}` |
|    16004 |  6647 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  6648 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  6649 | `		if( pMap->nEntry == 2 ){` |
|        - |  6650 | `			ph7_class *pClass;` |
|        - |  6651 | `			ph7_value *pV;` |
|        - |  6652 | `			/* Extract the target class */` |
|       12 |  6653 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  6654 | `			if( pV ){` |
|       12 |  6655 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  6656 | `				if( pClass ){` |
|        - |  6657 | `					ph7_class_method *pMethod;` |
|        - |  6658 | `					/* Extract the target method */` |
|       10 |  6659 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  6660 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6661 | `						/* Perform the lookup */` |
|       10 |  6662 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  6663 | `						if( pMethod ){` |
|        - |  6664 | `							/* Method is callable */` |
|        5 |  6665 | `							res = 1;` |
|        2 |  6666 | `						}` |
|        4 |  6667 | `					}` |
|        4 |  6668 | `				}` |
|        5 |  6669 | `			}` |
|        7 |  6670 | `		}` |
|    15991 |  6671 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6672 | `		const char *zName;` |
|        - |  6673 | `		int nLen;` |
|        - |  6674 | `		/* Extract the name */` |
|     4700 |  6675 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6676 | `		/* Perform the lookup */` |
|     4715 |  6677 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  6678 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6679 | `				/* Function is callable */` |
|     4682 |  6680 | `				res = 1;` |
|     2340 |  6681 | `		}` |
|     2349 |  6682 | `	}` |
|    16004 |  6683 | `	return res;` |
|        2 |  6684 |  |
|        - |  6685 | `/*` |
|        - |  6686 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6687 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6688 | ` * Parameters` |
|        - |  6689 | ` * $name` |
|        - |  6690 | ` *    The callback function to check` |
|        - |  6691 | ` * $syntax_only` |
|        - |  6692 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6693 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6694 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6695 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6696 | ` *    a string.` |
|        - |  6697 | ` * Return` |
|        - |  6698 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6699 | ` */` |
|       14 |  6700 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6701 |  |
|        - |  6702 | `	ph7_vm *pVm;` |
|        - |  6703 | `	int res;` |
|       15 |  6704 | `	if( nArg < 1 ){` |
|        - |  6705 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6706 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6707 | `		return SXRET_OK;` |
|        - |  6708 | `	}` |
|        - |  6709 | `	/* Point to the target VM */` |
|       15 |  6710 | `	pVm = pCtx->pVm;` |
|        - |  6711 | `	/* Perform the requested operation */` |
|       15 |  6712 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6713 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6714 | `	return SXRET_OK;` |
|        8 |  6715 |  |
|        - |  6716 | `/*` |
|        - |  6717 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6718 | ` * defined below.` |
|        - |  6719 | ` */` |
|     1082 |  6720 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6721 |  |
|     1083 |  6722 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6723 | `	ph7_value sName;` |
|        - |  6724 | `	sxi32 rc;` |
|        - |  6725 | `	/* Prepare the function name for insertion */` |
|     1083 |  6726 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1083 |  6727 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6728 | `	/* Perform the insertion */` |
|     1083 |  6729 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1083 |  6730 | `	PH7_MemObjRelease(&sName);` |
|     1083 |  6731 | `	return rc;` |
|        1 |  6732 |  |
|        - |  6733 | `/*` |
|        - |  6734 | ` * array get_defined_functions(void)` |
|        - |  6735 | ` *  Returns an array of all defined functions.` |
|        - |  6736 | ` * Parameter` |
|        - |  6737 | ` *  None.` |
|        - |  6738 | ` * Return` |
|        - |  6739 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6740 | ` *  both built-in (internal) and user-defined.` |
|        - |  6741 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6742 | ` *  defined ones using $arr["user"].` |
|        - |  6743 | ` * Note:` |
|        - |  6744 | ` *  NULL is returned on failure.` |
|        - |  6745 | ` */` |
|        2 |  6746 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6747 |  |
|        - |  6748 | `	ph7_value *pArray,*pEntry;` |
|        - |  6749 | `	/* NOTE:` |
|        - |  6750 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6751 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6752 | `	 */` |
|        3 |  6753 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6754 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6755 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6756 | `		SXUNUSED(apArg);` |
|        - |  6757 | `		/* Return NULL */` |
|      ! 0 |  6758 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6759 | `		return SXRET_OK;` |
|        - |  6760 | `	}` |
|        3 |  6761 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6762 | `	if( pEntry == 0 ){` |
|        - |  6763 | `		/* Return NULL */` |
|      ! 0 |  6764 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6765 | `		return SXRET_OK;` |
|        - |  6766 | `	}` |
|        - |  6767 | `	/* Fill with the appropriate information */` |
|        3 |  6768 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6769 | `	/* Create the 'internal' index */` |
|        3 |  6770 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6771 | `	/* Create the user-func array */` |
|        3 |  6772 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6773 | `	if( pEntry == 0 ){` |
|        - |  6774 | `		/* Return NULL */` |
|      ! 0 |  6775 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6776 | `		return SXRET_OK;` |
|        - |  6777 | `	}` |
|        - |  6778 | `	/* Fill with the appropriate information */` |
|        3 |  6779 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6780 | `	/* Create the 'user' index */` |
|        3 |  6781 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6782 | `	/* Return the multi-dimensional array */` |
|        3 |  6783 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6784 | `	return SXRET_OK;` |
|        2 |  6785 |  |
|        - |  6786 | `/*` |
|        - |  6787 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6788 | ` *  Register a function for execution on shutdown.` |
|        - |  6789 | ` * Note` |
|        - |  6790 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6791 | ` *  be called in the same order as they were registered.` |
|        - |  6792 | ` * Parameters` |
|        - |  6793 | ` *  $callback` |
|        - |  6794 | ` *   The shutdown callback to register.` |
|        - |  6795 | ` * $param` |
|        - |  6796 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6797 | ` * Return` |
|        - |  6798 | ` *  Nothing.` |
|        - |  6799 | ` */` |
|        2 |  6800 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6801 |  |
|        - |  6802 | `	VmShutdownCB sEntry;` |
|        - |  6803 | `	int i,j;` |
|        3 |  6804 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6805 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6806 | `		return PH7_OK;` |
|        - |  6807 | `	}` |
|        - |  6808 | `	/* Zero the Entry */` |
|        3 |  6809 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6810 | `	/* Initialize fields */` |
|        3 |  6811 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6812 | `	/* Save the callback name for later invocation name */` |
|        3 |  6813 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6814 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6815 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6816 | `	}` |
|        - |  6817 | `	/* Copy arguments */` |
|        3 |  6818 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6819 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6820 | `			/* Limit reached */` |
|      ! 0 |  6821 | `			break;` |
|        - |  6822 | `		}` |
|      ! 0 |  6823 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6824 | `	}` |
|        3 |  6825 | `	sEntry.nArg = j;` |
|        - |  6826 | `	/* Install the callback */` |
|        3 |  6827 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6828 | `	return PH7_OK;` |
|        2 |  6829 |  |
|        - |  6830 | `/*` |
|        - |  6831 | ` * Section:` |
|        - |  6832 | ` *  Class handling functions.` |
|        - |  6833 | ` * Status:` |
|        - |  6834 | ` *    Stable.` |
|        - |  6835 | ` */` |
|        - |  6836 | `/*` |
|        - |  6837 | ` * Extract the top active class. NULL is returned` |
|        - |  6838 | ` * if the class stack is empty.` |
|        - |  6839 | ` */` |
|      516 |  6840 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6841 |  |
|      518 |  6842 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6843 | `	ph7_class **apClass;` |
|      518 |  6844 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6845 | `		/* Empty stack,return NULL */` |
|       15 |  6846 | `		return 0;` |
|        - |  6847 | `	}` |
|        - |  6848 | `	/* Peek the last entry */` |
|      504 |  6849 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      504 |  6850 | `	return apClass[pSet->nUsed - 1];` |
|      260 |  6851 |  |
|        - |  6852 | `/*` |
|        - |  6853 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  6854 | ` *   Get the class that declared the currently executing method.` |
|        - |  6855 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  6856 | ` *` |
|        - |  6857 | ` * Parameters` |
|        - |  6858 | ` *   pVm: Target VM` |
|        - |  6859 | ` *` |
|        - |  6860 | ` * Return` |
|        - |  6861 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  6862 | ` *   - Not executing within a class method` |
|        - |  6863 | ` *` |
|        - |  6864 | ` * Note` |
|        - |  6865 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  6866 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  6867 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  6868 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  6869 | ` *   declaring class.` |
|        - |  6870 | ` */` |
|       18 |  6871 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        1 |  6872 |  |
|       19 |  6873 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  6874 | `	ph7_vm_func *pVmFunc;` |
|        - |  6875 |  |
|        - |  6876 | `	/* Skip exception frames to find the actual method frame */` |
|       19 |  6877 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  6878 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6879 | `	}` |
|        - |  6880 |  |
|        - |  6881 | `	/* Check if we're in a method context */` |
|       19 |  6882 | `	if( pFrame->pParent ){` |
|       15 |  6883 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       15 |  6884 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  6885 | `			/* Return the declaring class */` |
|       15 |  6886 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  6887 | `		}` |
|      ! 0 |  6888 | `	}` |
|        - |  6889 |  |
|        5 |  6890 | `	return 0;` |
|       10 |  6891 |  |
|        - |  6892 |  |
|        - |  6893 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  6894 | `/*` |
|        - |  6895 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  6896 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  6897 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  6898 | ` * return value indicates failure.` |
|        - |  6899 | ` */` |
|     1146 |  6900 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  6901 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  6902 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  6903 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  6904 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  6905 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  6906 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  6907 | `	)` |
|        2 |  6908 |  |
|        - |  6909 | `	ph7_value *aStack;` |
|        - |  6910 | `	VmInstr aInstr[2];` |
|        - |  6911 | `	int iCursor;` |
|        - |  6912 | `	int i;` |
|        - |  6913 | `	/* Create a new operand stack */` |
|     1148 |  6914 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1148 |  6915 | `	if( aStack == 0 ){` |
|      ! 0 |  6916 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6917 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  6918 | `		return SXERR_MEM;` |
|        - |  6919 | `	}` |
|        - |  6920 | `	/* Fill the operand stack with the given arguments */` |
|     1694 |  6921 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      548 |  6922 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  6923 | `		/*` |
|        - |  6924 | `		 * Symisc eXtension:` |
|        - |  6925 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  6926 | `		 */` |
|      548 |  6927 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      275 |  6928 | `	}` |
|     1148 |  6929 | `	iCursor = nArg + 1;` |
|     1148 |  6930 | `	if( pThis ){` |
|        - |  6931 | `		/*` |
|        - |  6932 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  6933 | `		 */` |
|     1142 |  6934 | `		pThis->iRef++; /* Increment reference count */` |
|     1142 |  6935 | `		aStack[i].x.pOther = pThis;` |
|     1142 |  6936 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      570 |  6937 | `	}` |
|     1148 |  6938 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1148 |  6939 | `	i++;` |
|        - |  6940 | `	/* Push method name */` |
|     1148 |  6941 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1148 |  6942 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1148 |  6943 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1148 |  6944 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  6945 | `	/* Emit the CALL istruction */` |
|     1148 |  6946 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1148 |  6947 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1148 |  6948 | `	aInstr[0].iP2 = 0;` |
|     1148 |  6949 | `	aInstr[0].p3  = 0;` |
|        - |  6950 | `	/* Emit the DONE instruction */` |
|     1148 |  6951 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1148 |  6952 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1148 |  6953 | `	aInstr[1].iP2 = 0;` |
|     1148 |  6954 | `	aInstr[1].p3  = 0;` |
|        - |  6955 | `	/* Execute the method body (if available) */` |
|     1148 |  6956 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  6957 | `	/* Clean up the mess left behind */` |
|     1148 |  6958 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1148 |  6959 | `	return PH7_OK;` |
|      575 |  6960 |  |
|        - |  6961 | `/*` |
|        - |  6962 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  6963 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  6964 | ` * in the apArg[] array.` |
|        - |  6965 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  6966 | ` * return value indicates failure.` |
|        - |  6967 | ` */` |
|      926 |  6968 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  6969 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  6970 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  6971 | `	int nArg,          /* Total number of given arguments */` |
|        - |  6972 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  6973 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  6974 | `	)` |
|        2 |  6975 |  |
|        - |  6976 | `	ph7_value *aStack;` |
|        - |  6977 | `	VmInstr aInstr[2];` |
|        - |  6978 | `	int i;` |
|      928 |  6979 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6980 | `		/* Don't bother processing,it's invalid anyway */` |
|      471 |  6981 | `		if( pResult ){` |
|        - |  6982 | `			/* Assume a null return value */` |
|      ! 0 |  6983 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  6984 | `		}` |
|      471 |  6985 | `		return SXERR_INVALID;` |
|        - |  6986 | `	}` |
|      458 |  6987 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6988 | `		/* Class method */` |
|       11 |  6989 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  6990 | `		ph7_class_method *pMethod = 0;` |
|       11 |  6991 | `		ph7_class_instance *pThis = 0;` |
|       11 |  6992 | `		ph7_class *pClass = 0;` |
|        - |  6993 | `		ph7_value *pValue;` |
|        - |  6994 | `		sxi32 rc;` |
|       11 |  6995 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  6996 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  6997 | `			if( pResult ){` |
|        - |  6998 | `				/* Assume a null return value */` |
|      ! 0 |  6999 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7000 | `			}` |
|      ! 0 |  7001 | `			return SXRET_OK;` |
|        - |  7002 | `		}` |
|        - |  7003 | `		/* Extract the class name or an instance of it */` |
|       11 |  7004 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  7005 | `		if( pValue ){` |
|       11 |  7006 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  7007 | `		}` |
|       11 |  7008 | `		if( pClass == 0 ){` |
|        - |  7009 | `			/* No such class,return NULL */` |
|      ! 0 |  7010 | `			if( pResult ){` |
|      ! 0 |  7011 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7012 | `			}` |
|      ! 0 |  7013 | `			return SXRET_OK;` |
|        - |  7014 | `		}` |
|       11 |  7015 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  7016 | `			/* Point to the class instance */` |
|        5 |  7017 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  7018 | `		}` |
|        - |  7019 | `		/* Try to extract the method */` |
|       11 |  7020 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  7021 | `		if( pValue ){` |
|       11 |  7022 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  7023 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  7024 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  7025 | `			}` |
|        5 |  7026 | `		}` |
|       11 |  7027 | `		if( pMethod == 0 ){` |
|        - |  7028 | `			/* No such method,return NULL */` |
|      ! 0 |  7029 | `			if( pResult ){` |
|      ! 0 |  7030 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7031 | `			}` |
|      ! 0 |  7032 | `			return SXRET_OK;` |
|        - |  7033 | `		}` |
|        - |  7034 | `		/* Call the class method */` |
|       11 |  7035 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  7036 | `		return rc;` |
|        - |  7037 | `	}` |
|        - |  7038 | `	/* Create a new operand stack */` |
|      448 |  7039 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      448 |  7040 | `	if( aStack == 0 ){` |
|      ! 0 |  7041 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7042 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  7043 | `		if( pResult ){` |
|        - |  7044 | `			/* Assume a null return value */` |
|      ! 0 |  7045 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7046 | `		}` |
|      ! 0 |  7047 | `		return SXERR_MEM;` |
|        - |  7048 | `	}` |
|        - |  7049 | `	/* Fill the operand stack with the given arguments */` |
|     1470 |  7050 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1024 |  7051 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7052 | `		/*` |
|        - |  7053 | `		 * Symisc eXtension:` |
|        - |  7054 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7055 | `		 */` |
|     1024 |  7056 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      513 |  7057 | `	}` |
|        - |  7058 | `	/* Push the function name */` |
|      448 |  7059 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      448 |  7060 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7061 | `	/* Emit the CALL istruction */` |
|      448 |  7062 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      448 |  7063 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      448 |  7064 | `	aInstr[0].iP2 = 0;` |
|      448 |  7065 | `	aInstr[0].p3  = 0;` |
|        - |  7066 | `	/* Emit the DONE instruction */` |
|      448 |  7067 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      448 |  7068 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      448 |  7069 | `	aInstr[1].iP2 = 0;` |
|      448 |  7070 | `	aInstr[1].p3  = 0;` |
|        - |  7071 | `	/* Execute the function body (if available) */` |
|      448 |  7072 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  7073 | `	/* Clean up the mess left behind */` |
|      448 |  7074 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      448 |  7075 | `	return PH7_OK;` |
|      465 |  7076 |  |
|        - |  7077 | `/*` |
|        - |  7078 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  7079 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  7080 | ` * parameter.` |
|        - |  7081 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7082 | ` * return value indicates failure.` |
|        - |  7083 | ` */` |
|      236 |  7084 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  7085 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7086 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7087 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  7088 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  7089 | `	)` |
|        1 |  7090 |  |
|        - |  7091 | `	ph7_value *pArg;` |
|        - |  7092 | `	SySet aArg;` |
|        - |  7093 | `	va_list ap;` |
|        - |  7094 | `	sxi32 rc;` |
|      237 |  7095 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7096 | `	/* Copy arguments one after one */` |
|      237 |  7097 | `	va_start(ap,pResult);` |
|      393 |  7098 | `	for(;;){` |
|      787 |  7099 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  7100 | `		if( pArg == 0 ){` |
|      237 |  7101 | `			break;` |
|        - |  7102 | `		}` |
|      551 |  7103 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  7104 | `	}` |
|        - |  7105 | `	/* Call the core routine */` |
|      237 |  7106 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  7107 | `	/* Cleanup */` |
|      237 |  7108 | `	SySetRelease(&aArg);` |
|      237 |  7109 | `	return rc;` |
|        1 |  7110 |  |
|        - |  7111 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  7112 | `/*` |
|        - |  7113 | ` * bool defined(string $name)` |
|        - |  7114 | ` *  Checks whether a given named constant exists.` |
|        - |  7115 | ` * Parameter:` |
|        - |  7116 | ` *  Name of the desired constant.` |
|        - |  7117 | ` * Return` |
|        - |  7118 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  7119 | ` */` |
|       14 |  7120 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7121 |  |
|        - |  7122 | `	const char *zName;` |
|       16 |  7123 | `	int nLen = 0;` |
|       16 |  7124 | `	int res = 0;` |
|       16 |  7125 | `	if( nArg < 1 ){` |
|        - |  7126 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  7127 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  7128 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7129 | `		return SXRET_OK;` |
|        - |  7130 | `	}` |
|        - |  7131 | `	/* Extract constant name */` |
|       16 |  7132 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7133 | `	/* Perform the lookup */` |
|       16 |  7134 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7135 | `		/* Already defined */` |
|       10 |  7136 | `		res = 1;` |
|        4 |  7137 | `	}` |
|       16 |  7138 | `	ph7_result_bool(pCtx,res);` |
|       16 |  7139 | `	return SXRET_OK;` |
|        9 |  7140 |  |
|        - |  7141 | `/*` |
|        - |  7142 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  7143 | ` * below.` |
|        - |  7144 | ` */` |
|        8 |  7145 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  7146 |  |
|       10 |  7147 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7148 | `	/* Expand constant value */` |
|       10 |  7149 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7150 |  |
|        - |  7151 | `/*` |
|        - |  7152 | ` * bool define(string $constant_name,expression value)` |
|        - |  7153 | ` *  Defines a named constant at runtime.` |
|        - |  7154 | ` * Parameter:` |
|        - |  7155 | ` *  $constant_name` |
|        - |  7156 | ` *   The name of the constant` |
|        - |  7157 | ` *  $value` |
|        - |  7158 | ` *   Constant value` |
|        - |  7159 | ` * Return:` |
|        - |  7160 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7161 | ` */` |
|       10 |  7162 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7163 |  |
|        - |  7164 | `	const char *zName;  /* Constant name */` |
|        - |  7165 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7166 | `	int nLen = 0;       /* Name length */` |
|        - |  7167 | `	sxi32 rc;` |
|       12 |  7168 | `	if( nArg < 2 ){` |
|        - |  7169 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7170 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7171 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7172 | `		return SXRET_OK;` |
|        - |  7173 | `	}` |
|       12 |  7174 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7175 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7176 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7177 | `		return SXRET_OK;` |
|        - |  7178 | `	}` |
|        - |  7179 | `	/* Extract constant name */` |
|       12 |  7180 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7181 | `	if( nLen < 1 ){` |
|      ! 0 |  7182 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7183 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7184 | `		return SXRET_OK;` |
|        - |  7185 | `	}` |
|        - |  7186 | `	/* Duplicate constant value */` |
|       12 |  7187 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7188 | `	if( pValue == 0 ){` |
|      ! 0 |  7189 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7190 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7191 | `		return SXRET_OK;` |
|        - |  7192 | `	}` |
|        - |  7193 | `	/* Initialize the memory object */` |
|       12 |  7194 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7195 | `	/* Register the constant */` |
|       12 |  7196 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7197 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7198 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7199 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7200 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7201 | `		return SXRET_OK;` |
|        - |  7202 | `	}` |
|        - |  7203 | `	/* Duplicate constant value */` |
|       12 |  7204 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7205 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7206 | `		/* Lower case the constant name */` |
|      ! 0 |  7207 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7208 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7209 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7210 | `				/* UTF-8 stream */` |
|      ! 0 |  7211 | `				zCur++;` |
|      ! 0 |  7212 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  7213 | `					zCur++;` |
|      ! 0 |  7214 | `				}` |
|      ! 0 |  7215 | `				continue;` |
|        - |  7216 | `			}` |
|      ! 0 |  7217 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  7218 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  7219 | `				zCur[0] = (char)c;` |
|      ! 0 |  7220 | `			}` |
|      ! 0 |  7221 | `			zCur++;` |
|      ! 0 |  7222 | `		}` |
|        - |  7223 | `		/* Finally,register the constant */` |
|      ! 0 |  7224 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  7225 | `	}` |
|        - |  7226 | `	/* All done,return TRUE */` |
|       12 |  7227 | `	ph7_result_bool(pCtx,1);` |
|       12 |  7228 | `	return SXRET_OK;` |
|        7 |  7229 |  |
|        - |  7230 | `/*` |
|        - |  7231 | ` * value constant(string $name)` |
|        - |  7232 | ` *  Returns the value of a constant` |
|        - |  7233 | ` * Parameter` |
|        - |  7234 | ` *  $name` |
|        - |  7235 | ` *    Name of the constant.` |
|        - |  7236 | ` * Return` |
|        - |  7237 | ` *  Constant value or NULL if not defined.` |
|        - |  7238 | ` */` |
|        8 |  7239 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7240 |  |
|        - |  7241 | `	SyHashEntry *pEntry;` |
|        - |  7242 | `	ph7_constant *pCons;` |
|        - |  7243 | `	const char *zName; /* Constant name */` |
|        - |  7244 | `	ph7_value sVal;    /* Constant value */` |
|        - |  7245 | `	int nLen;` |
|       10 |  7246 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  7247 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  7248 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  7249 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7250 | `		return SXRET_OK;` |
|        - |  7251 | `	}` |
|        - |  7252 | `	/* Extract the constant name */` |
|       10 |  7253 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7254 | `	/* Perform the query */` |
|       10 |  7255 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  7256 | `	if( pEntry == 0 ){` |
|        3 |  7257 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  7258 | `		ph7_result_null(pCtx);` |
|        3 |  7259 | `		return SXRET_OK;` |
|        - |  7260 | `	}` |
|        8 |  7261 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  7262 | `	/* Point to the structure that describe the constant */` |
|        8 |  7263 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  7264 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  7265 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  7266 | `	/* Return that value */` |
|        8 |  7267 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  7268 | `	/* Cleanup */` |
|        8 |  7269 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  7270 | `	return SXRET_OK;` |
|        6 |  7271 |  |
|        - |  7272 | `/*` |
|        - |  7273 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  7274 | ` * defined below.` |
|        - |  7275 | ` */` |
|      416 |  7276 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7277 |  |
|      417 |  7278 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  7279 | `	ph7_value sName;` |
|        - |  7280 | `	sxi32 rc;` |
|        - |  7281 | `	/* Prepare the constant name for insertion */` |
|      417 |  7282 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      417 |  7283 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  7284 | `	/* Perform the insertion */` |
|      417 |  7285 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      417 |  7286 | `	PH7_MemObjRelease(&sName);` |
|      417 |  7287 | `	return rc;` |
|        1 |  7288 |  |
|        - |  7289 | `/*` |
|        - |  7290 | ` * array get_defined_constants(void)` |
|        - |  7291 | ` *  Returns an associative array with the names of all defined` |
|        - |  7292 | ` *  constants.` |
|        - |  7293 | ` * Parameters` |
|        - |  7294 | ` *  NONE.` |
|        - |  7295 | ` * Returns` |
|        - |  7296 | ` *  Returns the names of all the constants currently defined.` |
|        - |  7297 | ` */` |
|        2 |  7298 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7299 |  |
|        - |  7300 | `	ph7_value *pArray;` |
|        - |  7301 | `	/* Create the array first*/` |
|        3 |  7302 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7303 | `	if( pArray == 0 ){` |
|      ! 0 |  7304 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7305 | `		SXUNUSED(apArg);` |
|        - |  7306 | `		/* Return NULL */` |
|      ! 0 |  7307 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7308 | `		return SXRET_OK;` |
|        - |  7309 | `	}` |
|        - |  7310 | `	/* Fill the array with the defined constants */` |
|        3 |  7311 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  7312 | `	/* Return the created array */` |
|        3 |  7313 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7314 | `	return SXRET_OK;` |
|        2 |  7315 |  |
|        - |  7316 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  7317 | `/*` |
|        - |  7318 | ` * Section:` |
|        - |  7319 | ` *  Random numbers/string generators.` |
|        - |  7320 | ` * Status:` |
|        - |  7321 | ` *    Stable.` |
|        - |  7322 | ` */` |
|        - |  7323 | `/*` |
|        - |  7324 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  7325 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  7326 | ` * used by te SQLite3 library.` |
|        - |  7327 | ` */` |
|     2145 |  7328 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  7329 |  |
|        - |  7330 | `	sxu32 iNum;` |
|     2147 |  7331 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2147 |  7332 | `	return iNum;` |
|        2 |  7333 |  |
|        - |  7334 | `/*` |
|        - |  7335 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  7336 | ` * Note that the generated string is NOT null terminated.` |
|        - |  7337 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  7338 | ` * by te SQLite3 library.` |
|        - |  7339 | ` */` |
|    67196 |  7340 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  7341 |  |
|        - |  7342 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  7343 | `	int i;` |
|        - |  7344 | `	/* Generate a binary string first */` |
|    67198 |  7345 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  7346 | `	/* Turn the binary string into english based alphabet */` |
|   739326 |  7347 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   672130 |  7348 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   336066 |  7349 | `	 }` |
|    67198 |  7350 |  |
|        - |  7351 | `/*` |
|        - |  7352 | ` * int rand()` |
|        - |  7353 | ` * int mt_rand()` |
|        - |  7354 | ` * int rand(int $min,int $max)` |
|        - |  7355 | ` * int mt_rand(int $min,int $max)` |
|        - |  7356 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  7357 | ` * Parameter` |
|        - |  7358 | ` *  $min` |
|        - |  7359 | ` *    The lowest value to return (default: 0)` |
|        - |  7360 | ` *  $max` |
|        - |  7361 | ` *   The highest value to return (default: getrandmax())` |
|        - |  7362 | ` * Return` |
|        - |  7363 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  7364 | ` * Note:` |
|        - |  7365 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7366 | ` *  by te SQLite3 library.` |
|        - |  7367 | ` */` |
|       20 |  7368 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7369 |  |
|        - |  7370 | `	sxu32 iNum;` |
|        - |  7371 | `	/* Generate the random number */` |
|       21 |  7372 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  7373 | `	if( nArg > 1 ){` |
|        - |  7374 | `		sxu32 iMin,iMax;` |
|        3 |  7375 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  7376 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  7377 | `		if( iMin < iMax ){` |
|        3 |  7378 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  7379 | `			if( iDiv > 0 ){` |
|        3 |  7380 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  7381 | `			}` |
|        1 |  7382 | `		}else if(iMax > 0 ){` |
|      ! 0 |  7383 | `			iNum %= iMax;` |
|      ! 0 |  7384 | `		}` |
|        1 |  7385 | `	}` |
|        - |  7386 | `	/* Return the number */` |
|       21 |  7387 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  7388 | `	return SXRET_OK;` |
|        1 |  7389 |  |
|        - |  7390 | `/*` |
|        - |  7391 | ` * int getrandmax(void)` |
|        - |  7392 | ` * int mt_getrandmax(void)` |
|        - |  7393 | ` * int rc4_getrandmax(void)` |
|        - |  7394 | ` *   Show largest possible random value` |
|        - |  7395 | ` * Return` |
|        - |  7396 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  7397 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  7398 | ` * Note:` |
|        - |  7399 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7400 | ` *  by te SQLite3 library.` |
|        - |  7401 | ` */` |
|        4 |  7402 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7403 |  |
|        2 |  7404 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  7405 | `	SXUNUSED(apArg);` |
|        5 |  7406 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  7407 | `	return SXRET_OK;` |
|        1 |  7408 |  |
|        - |  7409 | `/*` |
|        - |  7410 | ` * string rand_str()` |
|        - |  7411 | ` * string rand_str(int $len)` |
|        - |  7412 | ` *  Generate a random string (English alphabet).` |
|        - |  7413 | ` * Parameter` |
|        - |  7414 | ` *  $len` |
|        - |  7415 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  7416 | ` * Return` |
|        - |  7417 | ` *   A pseudo random string.` |
|        - |  7418 | ` * Note:` |
|        - |  7419 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7420 | ` *  by te SQLite3 library.` |
|        - |  7421 | ` *  This function is a symisc extension.` |
|        - |  7422 | ` */` |
|      120 |  7423 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7424 |  |
|        - |  7425 | `	char zString[1024];` |
|      122 |  7426 | `	int iLen = 0x10;` |
|      122 |  7427 | `	if( nArg > 0 ){` |
|        - |  7428 | `		/* Get the desired length */` |
|      122 |  7429 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  7430 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  7431 | `			/* Default length */` |
|        3 |  7432 | `			iLen = 0x10;` |
|        1 |  7433 | `		}` |
|       60 |  7434 | `	}` |
|        - |  7435 | `	/* Generate the random string */` |
|      122 |  7436 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  7437 | `	/* Return the generated string */` |
|      122 |  7438 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  7439 | `	return SXRET_OK;` |
|        2 |  7440 |  |
|        - |  7441 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  7442 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  7443 | `/* Unique ID private data */` |
|        - |  7444 | `struct unique_id_data` |
|        - |  7445 |  |
|        - |  7446 | `	ph7_context *pCtx; /* Call context */` |
|        - |  7447 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  7448 | `};` |
|        - |  7449 | `/*` |
|        - |  7450 | ` * Binary to hex consumer callback.` |
|        - |  7451 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  7452 | ` * defined below.` |
|        - |  7453 | ` */` |
|      192 |  7454 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  7455 |  |
|      193 |  7456 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  7457 | `	sxu32 nBuflen;` |
|        - |  7458 | `	/* Extract result buffer length */` |
|      193 |  7459 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  7460 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  7461 | `			/*` |
|        - |  7462 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  7463 | `			 * string will be 13 characters long` |
|        - |  7464 | `			 */` |
|       25 |  7465 | `		return SXERR_ABORT;` |
|        - |  7466 | `	}` |
|      169 |  7467 | `	if( nBuflen > 22 ){` |
|      ! 0 |  7468 | `		return SXERR_ABORT;` |
|        - |  7469 | `	}` |
|        - |  7470 | `	/* Safely Consume the hex stream */` |
|      169 |  7471 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  7472 | `	return SXRET_OK;` |
|       97 |  7473 |  |
|        - |  7474 | `/*` |
|        - |  7475 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  7476 | ` *  Generate a unique ID` |
|        - |  7477 | ` * Parameter` |
|        - |  7478 | ` * $prefix` |
|        - |  7479 | ` *  Append this prefix to the generated unique ID.` |
|        - |  7480 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  7481 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  7482 | ` * $more_entropy` |
|        - |  7483 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  7484 | ` *  that the result will be unique.` |
|        - |  7485 | ` * Return` |
|        - |  7486 | ` *  Returns the unique identifier, as a string.` |
|        - |  7487 | ` */` |
|       24 |  7488 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7489 |  |
|        - |  7490 | `	struct unique_id_data sUniq;` |
|        - |  7491 | `	unsigned char zDigest[20];` |
|       25 |  7492 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7493 | `	const char *zPrefix;` |
|        - |  7494 | `	SHA1Context sCtx;` |
|        - |  7495 | `	char zRandom[7];` |
|        - |  7496 | `	int nPrefix;` |
|        - |  7497 | `	int entropy;` |
|        - |  7498 | `	/* Generate a random string first */` |
|       25 |  7499 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  7500 | `	/* Initialize fields */` |
|       25 |  7501 | `	zPrefix = 0;` |
|       25 |  7502 | `	nPrefix = 0;` |
|       25 |  7503 | `	entropy = 0;` |
|       25 |  7504 | `	if( nArg > 0 ){` |
|        - |  7505 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  7506 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  7507 | `		if( nArg > 1 ){` |
|      ! 0 |  7508 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  7509 | `		}` |
|      ! 0 |  7510 | `	}` |
|       25 |  7511 | `	SHA1Init(&sCtx);` |
|        - |  7512 | `	/* Generate the random ID */` |
|       25 |  7513 | `	if( nPrefix > 0 ){` |
|      ! 0 |  7514 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  7515 | `	}` |
|        - |  7516 | `	/* Append the random ID */` |
|       25 |  7517 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  7518 | `	/* Append the random string */` |
|       25 |  7519 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  7520 | `	/* Increment the number */` |
|       25 |  7521 | `	pVm->unique_id++;` |
|       25 |  7522 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  7523 | `	/* Hexify the digest */` |
|       25 |  7524 | `	sUniq.pCtx = pCtx;` |
|       25 |  7525 | `	sUniq.entropy = entropy;` |
|       25 |  7526 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  7527 | `	/* All done */` |
|       25 |  7528 | `	return PH7_OK;` |
|        1 |  7529 |  |
|        - |  7530 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  7531 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  7532 | `/*` |
|        - |  7533 | ` * Section:` |
|        - |  7534 | ` *  Language construct implementation as foreign functions.` |
|        - |  7535 | ` * Status:` |
|        - |  7536 | ` *    Stable.` |
|        - |  7537 | ` */` |
|        - |  7538 | `/*` |
|        - |  7539 | ` * void echo($string...)` |
|        - |  7540 | ` *  Output one or more messages.` |
|        - |  7541 | ` * Parameters` |
|        - |  7542 | ` *  $string` |
|        - |  7543 | ` *   Message to output.` |
|        - |  7544 | ` * Return` |
|        - |  7545 | ` *  NULL.` |
|        - |  7546 | ` */` |
|      ! 0 |  7547 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7548 |  |
|        - |  7549 | `	const char *zData;` |
|      ! 0 |  7550 | `	int nDataLen = 0;` |
|        - |  7551 | `	ph7_vm *pVm;` |
|        - |  7552 | `	int i,rc;` |
|        - |  7553 | `	/* Point to the target VM */` |
|      ! 0 |  7554 | `	pVm = pCtx->pVm;` |
|        - |  7555 | `	/* Output */` |
|      ! 0 |  7556 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  7557 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  7558 | `		if( nDataLen > 0 ){` |
|      ! 0 |  7559 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  7560 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7561 | `				/* Increment output length */` |
|      ! 0 |  7562 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  7563 | `			}` |
|      ! 0 |  7564 | `			if( rc == SXERR_ABORT ){` |
|        - |  7565 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7566 | `				return PH7_ABORT;` |
|        - |  7567 | `			}` |
|      ! 0 |  7568 | `		}` |
|      ! 0 |  7569 | `	}` |
|      ! 0 |  7570 | `	return SXRET_OK;` |
|      ! 0 |  7571 |  |
|        - |  7572 | `/*` |
|        - |  7573 | ` * int print($string...)` |
|        - |  7574 | ` *  Output one or more messages.` |
|        - |  7575 | ` * Parameters` |
|        - |  7576 | ` *  $string` |
|        - |  7577 | ` *   Message to output.` |
|        - |  7578 | ` * Return` |
|        - |  7579 | ` *  1 always.` |
|        - |  7580 | ` */` |
|        2 |  7581 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7582 |  |
|        - |  7583 | `	const char *zData;` |
|        3 |  7584 | `	int nDataLen = 0;` |
|        - |  7585 | `	ph7_vm *pVm;` |
|        - |  7586 | `	int i,rc;` |
|        - |  7587 | `	/* Point to the target VM */` |
|        3 |  7588 | `	pVm = pCtx->pVm;` |
|        - |  7589 | `	/* Output */` |
|        5 |  7590 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  7591 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  7592 | `		if( nDataLen > 0 ){` |
|        3 |  7593 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  7594 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7595 | `				/* Increment output length */` |
|        3 |  7596 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  7597 | `			}` |
|        3 |  7598 | `			if( rc == SXERR_ABORT ){` |
|        - |  7599 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7600 | `				return PH7_ABORT;` |
|        - |  7601 | `			}` |
|        1 |  7602 | `		}` |
|        2 |  7603 | `	}` |
|        - |  7604 | `	/* Return 1 */` |
|        3 |  7605 | `	ph7_result_int(pCtx,1);` |
|        3 |  7606 | `	return SXRET_OK;` |
|        2 |  7607 |  |
|        - |  7608 | `/*` |
|        - |  7609 | ` * void exit(string $msg)` |
|        - |  7610 | ` * void exit(int $status)` |
|        - |  7611 | ` * void die(string $ms)` |
|        - |  7612 | ` * void die(int $status)` |
|        - |  7613 | ` *   Output a message and terminate program execution.` |
|        - |  7614 | ` * Parameter` |
|        - |  7615 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  7616 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  7617 | ` *  and not printed` |
|        - |  7618 | ` * Return` |
|        - |  7619 | ` *  NULL` |
|        - |  7620 | ` */` |
|      ! 0 |  7621 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7622 |  |
|      ! 0 |  7623 | `	if( nArg > 0 ){` |
|      ! 0 |  7624 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  7625 | `			const char *zData;` |
|      ! 0 |  7626 | `			int iLen = 0;` |
|        - |  7627 | `			/* Print exit message */` |
|      ! 0 |  7628 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  7629 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  7630 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  7631 | `			sxi32 iExitStatus;` |
|        - |  7632 | `			/* Record exit status code */` |
|      ! 0 |  7633 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  7634 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  7635 | `		}` |
|      ! 0 |  7636 | `	}` |
|        - |  7637 | `	/* Check if we are in an included file */` |
|      ! 0 |  7638 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  7639 | `		/* Exit the entire process */` |
|      ! 0 |  7640 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  7641 | `	}` |
|        - |  7642 | `	/* Abort processing immediately */` |
|      ! 0 |  7643 | `	return PH7_ABORT;` |
|      ! 0 |  7644 |  |
|        - |  7645 | `/*` |
|        - |  7646 | ` * bool isset($var,...)` |
|        - |  7647 | ` *  Finds out whether a variable is set.` |
|        - |  7648 | ` * Parameters` |
|        - |  7649 | ` *  One or more variable to check.` |
|        - |  7650 | ` * Return` |
|        - |  7651 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  7652 | ` */` |
|    69338 |  7653 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7654 |  |
|        - |  7655 | `	ph7_value *pObj;` |
|    69340 |  7656 | `	int res = 0;` |
|        - |  7657 | `	int i;` |
|    69340 |  7658 | `	if( nArg < 1 ){` |
|        - |  7659 | `		/* Missing arguments,return false */` |
|      ! 0 |  7660 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  7661 | `		return SXRET_OK;` |
|        - |  7662 | `	}` |
|        - |  7663 | `	/* Iterate over available arguments */` |
|    91688 |  7664 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    69340 |  7665 | `		pObj = apArg[i];` |
|    69340 |  7666 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    46496 |  7667 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7668 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  7669 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  7670 | `			}` |
|    23247 |  7671 | `		}` |
|    69340 |  7672 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    69340 |  7673 | `		if( !res ){` |
|        - |  7674 | `			/* Variable not set,return FALSE */` |
|    46992 |  7675 | `			ph7_result_bool(pCtx,0);` |
|    46992 |  7676 | `			return SXRET_OK;` |
|        - |  7677 | `		}` |
|    11176 |  7678 | `	}` |
|        - |  7679 | `	/* All given variable are set,return TRUE */` |
|    22350 |  7680 | `	ph7_result_bool(pCtx,1);` |
|    22350 |  7681 | `	return SXRET_OK;` |
|    34671 |  7682 |  |
|        - |  7683 | `/*` |
|        - |  7684 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  7685 | ` * frame,the reference table and discard it's contents.` |
|        - |  7686 | ` * This function never fail and always return SXRET_OK.` |
|        - |  7687 | ` */` |
|  2953934 |  7688 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  7689 |  |
|        - |  7690 | `	ph7_value *pObj;` |
|        - |  7691 | `	VmRefObj *pRef;` |
|  2953936 |  7692 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2953936 |  7693 | `	if( pObj ){` |
|        - |  7694 | `		/* Release the object */` |
|  2953936 |  7695 | `		PH7_MemObjRelease(pObj);` |
|  1476967 |  7696 | `	}` |
|        - |  7697 | `	/* Remove old reference links */` |
|  2953936 |  7698 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2953936 |  7699 | `	if( pRef ){` |
|  2953916 |  7700 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  7701 | `		/* Unlink from the reference table */` |
|  2953916 |  7702 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2953916 |  7703 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  7704 | `			VmSlot sFree;` |
|        - |  7705 | `			/* Restore to the free list */` |
|  2953910 |  7706 | `			sFree.nIdx = nObjIdx;` |
|  2953910 |  7707 | `			sFree.pUserData = 0;` |
|  2953910 |  7708 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1476954 |  7709 | `		}` |
|  1476957 |  7710 | `	}` |
|  2953936 |  7711 | `	return SXRET_OK;` |
|        2 |  7712 |  |
|        - |  7713 | `/*` |
|        - |  7714 | ` * void unset($var,...)` |
|        - |  7715 | ` *   Unset one or more given variable.` |
|        - |  7716 | ` * Parameters` |
|        - |  7717 | ` *  One or more variable to unset.` |
|        - |  7718 | ` * Return` |
|        - |  7719 | ` *  Nothing.` |
|        - |  7720 | ` */` |
|     3258 |  7721 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7722 |  |
|        - |  7723 | `	ph7_value *pObj;` |
|        - |  7724 | `	ph7_vm *pVm;` |
|        - |  7725 | `	int i;` |
|        - |  7726 | `	/* Point to the target VM */` |
|     3260 |  7727 | `	pVm = pCtx->pVm;` |
|        - |  7728 | `	/* Iterate and unset */` |
|     9662 |  7729 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6404 |  7730 | `		pObj = apArg[i];` |
|     6404 |  7731 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      868 |  7732 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7733 | `				/* Throw an error */` |
|      ! 0 |  7734 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  7735 | `			}` |
|      435 |  7736 | `		}else{` |
|     5537 |  7737 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  7738 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     5537 |  7739 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     5531 |  7740 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2765 |  7741 | `			}` |
|        - |  7742 | `		}` |
|     3203 |  7743 | `	}` |
|     3260 |  7744 | `	return SXRET_OK;` |
|        2 |  7745 |  |
|        - |  7746 | `/*` |
|        - |  7747 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  7748 | ` */` |
|      110 |  7749 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7750 |  |
|      111 |  7751 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  7752 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  7753 | `	ph7_value *pObj;` |
|        - |  7754 | `	sxu32 nIdx;` |
|        - |  7755 | `	/* Extract the memory object */` |
|      111 |  7756 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  7757 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  7758 | `	if( pObj ){` |
|      111 |  7759 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  7760 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  7761 | `				SyString sName;` |
|        - |  7762 | `				ph7_value sKey;` |
|        - |  7763 | `				/* Perform the insertion */` |
|      109 |  7764 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  7765 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  7766 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  7767 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  7768 | `			}` |
|       54 |  7769 | `		}` |
|       55 |  7770 | `	}` |
|      111 |  7771 | `	return SXRET_OK;` |
|        1 |  7772 |  |
|        - |  7773 | `/*` |
|        - |  7774 | ` * array get_defined_vars(void)` |
|        - |  7775 | ` *  Returns an array of all defined variables.` |
|        - |  7776 | ` * Parameter` |
|        - |  7777 | ` *  None` |
|        - |  7778 | ` * Return` |
|        - |  7779 | ` *  An array with all the variables defined in the current scope.` |
|        - |  7780 | ` */` |
|        2 |  7781 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7782 |  |
|        3 |  7783 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7784 | `	ph7_value *pArray;` |
|        - |  7785 | `	/* Create a new array */` |
|        3 |  7786 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7787 | ` 	if( pArray == 0 ){` |
|      ! 0 |  7788 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7789 | `		SXUNUSED(apArg);` |
|        - |  7790 | `		/* Return NULL */` |
|      ! 0 |  7791 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7792 | `		return SXRET_OK;` |
|        - |  7793 | `	}` |
|        - |  7794 | `	/* Superglobals first */` |
|        3 |  7795 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  7796 | `	/* Then variable defined in the current frame */` |
|        3 |  7797 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  7798 | `	/* Finally,return the created array */` |
|        3 |  7799 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7800 | `	return SXRET_OK;` |
|        2 |  7801 |  |
|        - |  7802 | `/*` |
|        - |  7803 | ` * bool gettype($var)` |
|        - |  7804 | ` *  Get the type of a variable` |
|        - |  7805 | ` * Parameters` |
|        - |  7806 | ` *   $var` |
|        - |  7807 | ` *    The variable being type checked.` |
|        - |  7808 | ` * Return` |
|        - |  7809 | ` *   String representation of the given variable type.` |
|        - |  7810 | ` */` |
|       32 |  7811 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7812 |  |
|       34 |  7813 | `	const char *zType = "Empty";` |
|       34 |  7814 | `	if( nArg > 0 ){` |
|       34 |  7815 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  7816 | `	}` |
|        - |  7817 | `	/* Return the variable type */` |
|       34 |  7818 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  7819 | `	return SXRET_OK;` |
|        2 |  7820 |  |
|        - |  7821 | `/*` |
|        - |  7822 | ` * string get_resource_type(resource $handle)` |
|        - |  7823 | ` *  This function gets the type of the given resource.` |
|        - |  7824 | ` * Parameters` |
|        - |  7825 | ` *  $handle` |
|        - |  7826 | ` *  The evaluated resource handle.` |
|        - |  7827 | ` * Return` |
|        - |  7828 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  7829 | ` *  representing its type. If the type is not identified by this function` |
|        - |  7830 | ` *  the return value will be the string Unknown.` |
|        - |  7831 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  7832 | ` *  is not a resource.` |
|        - |  7833 | ` */` |
|        2 |  7834 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7835 |  |
|        3 |  7836 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  7837 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  7838 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7839 | `		return PH7_OK;` |
|        - |  7840 | `	}` |
|        3 |  7841 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  7842 | `	return SXRET_OK;` |
|        2 |  7843 |  |
|        - |  7844 | `/*` |
|        - |  7845 | ` * void var_dump(expression,....)` |
|        - |  7846 | ` *   var_dump � Dumps information about a variable` |
|        - |  7847 | ` * Parameters` |
|        - |  7848 | ` *   One or more expression to dump.` |
|        - |  7849 | ` * Returns` |
|        - |  7850 | ` *  Nothing.` |
|        - |  7851 | ` */` |
|      218 |  7852 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7853 |  |
|        - |  7854 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  7855 | `	int i;` |
|      220 |  7856 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  7857 | `	/* Dump one or more expressions */` |
|      444 |  7858 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  7859 | `		ph7_value *pObj = apArg[i];` |
|        - |  7860 | `		/* Reset the working buffer */` |
|      226 |  7861 | `		SyBlobReset(&sDump);` |
|        - |  7862 | `		/* Dump the given expression */` |
|      226 |  7863 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  7864 | `		/* Output */` |
|      226 |  7865 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  7866 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  7867 | `		}` |
|      114 |  7868 | `	}` |
|        - |  7869 | `	/* Release the working buffer */` |
|      220 |  7870 | `	SyBlobRelease(&sDump);` |
|      220 |  7871 | `	return SXRET_OK;` |
|        2 |  7872 |  |
|        - |  7873 | `/*` |
|        - |  7874 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  7875 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  7876 | ` * Parameters` |
|        - |  7877 | ` *   expression: Expression to dump` |
|        - |  7878 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  7879 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  7880 | ` *            print_r() will return the information rather than print it.` |
|        - |  7881 | ` * Return` |
|        - |  7882 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  7883 | ` *  Otherwise, the return value is TRUE.` |
|        - |  7884 | ` */` |
|       16 |  7885 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7886 |  |
|       17 |  7887 | `	int ret_string = 0;` |
|        - |  7888 | `	SyBlob sDump;` |
|       17 |  7889 | `	if( nArg < 1 ){` |
|        - |  7890 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  7891 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7892 | `		return SXRET_OK;` |
|        - |  7893 | `	}` |
|       17 |  7894 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  7895 | `	if ( nArg > 1 ){` |
|        - |  7896 | `		/* Where to redirect output */` |
|       11 |  7897 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  7898 | `	}` |
|        - |  7899 | `	/* Generate dump */` |
|       17 |  7900 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  7901 | `	if( !ret_string ){` |
|        - |  7902 | `		/* Output dump */` |
|        7 |  7903 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7904 | `		/* Return true */` |
|        7 |  7905 | `		ph7_result_bool(pCtx,1);` |
|        4 |  7906 | `	}else{` |
|        - |  7907 | `		/* Generated dump as return value */` |
|       11 |  7908 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7909 | `	}` |
|        - |  7910 | `	/* Release the working buffer */` |
|       17 |  7911 | `	SyBlobRelease(&sDump);` |
|       17 |  7912 | `	return SXRET_OK;` |
|        9 |  7913 |  |
|        - |  7914 | `/*` |
|        - |  7915 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  7916 | ` * Same job as print_r. (see coment above)` |
|        - |  7917 | ` */` |
|        2 |  7918 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7919 |  |
|        3 |  7920 | `	int ret_string = 0;` |
|        - |  7921 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  7922 | `	if( nArg < 1 ){` |
|        - |  7923 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  7924 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7925 | `		return SXRET_OK;` |
|        - |  7926 | `	}` |
|        3 |  7927 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  7928 | `	if ( nArg > 1 ){` |
|        - |  7929 | `		/* Where to redirect output */` |
|        3 |  7930 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  7931 | `	}` |
|        - |  7932 | `	/* Generate dump */` |
|        3 |  7933 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  7934 | `	if( !ret_string ){` |
|        - |  7935 | `		/* Output dump */` |
|      ! 0 |  7936 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7937 | `		/* Return NULL */` |
|      ! 0 |  7938 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7939 | `	}else{` |
|        - |  7940 | `		/* Generated dump as return value */` |
|        3 |  7941 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7942 | `	}` |
|        - |  7943 | `	/* Release the working buffer */` |
|        3 |  7944 | `	SyBlobRelease(&sDump);` |
|        3 |  7945 | `	return SXRET_OK;` |
|        2 |  7946 |  |
|        - |  7947 | `/*` |
|        - |  7948 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  7949 | ` *  Set/get the various assert flags.` |
|        - |  7950 | ` * Parameter` |
|        - |  7951 | ` * $what` |
|        - |  7952 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  7953 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  7954 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  7955 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  7956 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  7957 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  7958 | ` * $value` |
|        - |  7959 | ` *   An optional new value for the option.` |
|        - |  7960 | ` * Return` |
|        - |  7961 | ` *  Old setting on success or FALSE on failure.` |
|        - |  7962 | ` */` |
|       30 |  7963 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7964 |  |
|       32 |  7965 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7966 | `	int iOption;` |
|        - |  7967 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       32 |  7968 | `	if( nArg < 1 ){` |
|        3 |  7969 | `		return PH7_VmThrowException(pCtx,` |
|        - |  7970 | `			"ArgumentCountError",` |
|        - |  7971 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  7972 | `			);` |
|        - |  7973 | `	}` |
|        - |  7974 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       28 |  7975 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       30 |  7976 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  7977 | `		return PH7_VmThrowException(pCtx,` |
|        - |  7978 | `			"TypeError",` |
|        - |  7979 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  7980 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  7981 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  7982 | `			);` |
|        - |  7983 | `	}` |
|       30 |  7984 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  7985 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  7986 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  7987 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       30 |  7988 | `	switch( iOption ){` |
|        6 |  7989 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  7990 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       14 |  7991 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       14 |  7992 | `		if( nArg > 1 ){` |
|        5 |  7993 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  7994 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  7995 | `			}else{` |
|        3 |  7996 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  7997 | `			}` |
|        2 |  7998 | `		}` |
|       14 |  7999 | `		break;` |
|        1 |  8000 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  8001 | `		/* Return old callback or null */` |
|        3 |  8002 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  8003 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  8004 | `		}else{` |
|        3 |  8005 | `			ph7_result_null(pCtx);` |
|        - |  8006 | `		}` |
|        3 |  8007 | `		if( nArg > 1 ){` |
|      ! 0 |  8008 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  8009 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  8010 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  8011 | `			}else{` |
|      ! 0 |  8012 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  8013 | `			}` |
|      ! 0 |  8014 | `		}` |
|        3 |  8015 | `		break;` |
|        5 |  8016 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  8017 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  8018 | `		if( nArg > 1 ){` |
|        5 |  8019 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  8020 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  8021 | `			}else{` |
|        3 |  8022 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  8023 | `			}` |
|        2 |  8024 | `		}` |
|       11 |  8025 | `		break;` |
|      ! 0 |  8026 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  8027 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  8028 | `		break;` |
|        1 |  8029 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  8030 | `		ph7_result_int(pCtx, 1);` |
|        3 |  8031 | `		break;` |
|      ! 0 |  8032 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  8033 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  8034 | `		break;` |
|        1 |  8035 | `	default:` |
|        - |  8036 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  8037 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8038 | `			"ValueError",` |
|        - |  8039 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  8040 | `			);` |
|        - |  8041 | `	}` |
|       28 |  8042 | `	return PH7_OK;` |
|       17 |  8043 |  |
|        - |  8044 | `/*` |
|        - |  8045 | ` * bool assert(mixed $assertion)` |
|        - |  8046 | ` *  Checks if assertion is FALSE.` |
|        - |  8047 | ` * Parameter` |
|        - |  8048 | ` *  $assertion` |
|        - |  8049 | ` *    The assertion to test.` |
|        - |  8050 | ` * Return` |
|        - |  8051 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  8052 | ` */` |
|       26 |  8053 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8054 |  |
|       28 |  8055 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8056 | `	int iFlags,iResult;` |
|        - |  8057 | `	const char *zDesc;` |
|        - |  8058 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       28 |  8059 | `	if( nArg < 1 ){` |
|        3 |  8060 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8061 | `			"ArgumentCountError",` |
|        - |  8062 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  8063 | `			);` |
|        - |  8064 | `	}` |
|       26 |  8065 | `	iFlags = pVm->iAssertFlags;` |
|       26 |  8066 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  8067 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  8068 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  8069 | `		return PH7_OK;` |
|        - |  8070 | `	}` |
|        - |  8071 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       26 |  8072 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       26 |  8073 | `	if( !iResult ){` |
|        - |  8074 | `		/* Assertion failed */` |
|        - |  8075 | `		/* Extract optional description */` |
|       13 |  8076 | `		zDesc = 0;` |
|       13 |  8077 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  8078 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  8079 | `		}` |
|       13 |  8080 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  8081 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  8082 | `			ph7_value sFile,sLine;` |
|        - |  8083 | `			ph7_value *apCbArg[3];` |
|        - |  8084 | `			SyString *pFile;` |
|        - |  8085 | `			/* Extract the processed script */` |
|      ! 0 |  8086 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  8087 | `			if( pFile == 0 ){` |
|      ! 0 |  8088 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  8089 | `			}` |
|        - |  8090 | `			/* Invoke the callback */` |
|      ! 0 |  8091 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  8092 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  8093 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  8094 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  8095 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  8096 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  8097 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  8098 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  8099 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  8100 | `		}` |
|       13 |  8101 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  8102 | `			/* Abort VM execution immediately */` |
|      ! 0 |  8103 | `			return PH7_ABORT;` |
|        - |  8104 | `		}` |
|        - |  8105 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  8106 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  8107 | `			return PH7_VmThrowException(pCtx,` |
|        - |  8108 | `				"AssertionError",` |
|        - |  8109 | `				"%s",` |
|        1 |  8110 | `				zDesc` |
|        - |  8111 | `				);` |
|      ! 0 |  8112 | `		}else{` |
|       11 |  8113 | `			return PH7_VmThrowException(pCtx,` |
|        - |  8114 | `				"AssertionError",` |
|        - |  8115 | `				"assert(false)"` |
|        - |  8116 | `				);` |
|        - |  8117 | `		}` |
|        - |  8118 | `	}` |
|        - |  8119 | `	/* Assertion passed */` |
|       14 |  8120 | `	ph7_result_bool(pCtx,1);` |
|       14 |  8121 | `	return PH7_OK;` |
|       15 |  8122 |  |
|        - |  8123 | `/*` |
|        - |  8124 | ` * Section:` |
|        - |  8125 | ` *  Error reporting functions.` |
|        - |  8126 | ` * Status:` |
|        - |  8127 | ` *    Stable.` |
|        - |  8128 | ` */` |
|        - |  8129 | `/*` |
|        - |  8130 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  8131 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  8132 | ` * Parameters` |
|        - |  8133 | ` *  $error_msg` |
|        - |  8134 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  8135 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  8136 | ` * $error_type` |
|        - |  8137 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  8138 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  8139 | ` * Return` |
|        - |  8140 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  8141 | ` */` |
|       12 |  8142 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8143 |  |
|       14 |  8144 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  8145 | `	int rc = PH7_OK;` |
|       14 |  8146 | `	if( nArg > 0 ){` |
|        - |  8147 | `		const char *zErr;` |
|        - |  8148 | `		int nLen;` |
|        - |  8149 | `		/* Extract the error message */` |
|       12 |  8150 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8151 | `		if( nArg > 1 ){` |
|        - |  8152 | `			/* Extract the error type */` |
|       12 |  8153 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  8154 | `			switch( nErr ){` |
|        1 |  8155 | `			case 1:   /* E_ERROR */` |
|        - |  8156 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  8157 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  8158 | `			case 256: /* E_USER_ERROR */` |
|        3 |  8159 | `				nErr = PH7_CTX_ERR;` |
|        3 |  8160 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  8161 | `				break;` |
|        1 |  8162 | `			case 2:   /* E_WARNING */` |
|        - |  8163 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  8164 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  8165 | `			case 512: /* E_USER_WARNING */` |
|        3 |  8166 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  8167 | `				break;` |
|        3 |  8168 | `			default:` |
|        8 |  8169 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  8170 | `				break;` |
|        - |  8171 | `			}` |
|        5 |  8172 | `		}` |
|        - |  8173 | `		/* Report error */` |
|       12 |  8174 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  8175 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  8176 | `			return rc;` |
|        - |  8177 | `		}` |
|        - |  8178 | `		/* Return true */` |
|       12 |  8179 | `		ph7_result_bool(pCtx,1);` |
|        7 |  8180 | `	}else{` |
|        - |  8181 | `		/* Missing arguments,return FALSE */` |
|        3 |  8182 | `		ph7_result_bool(pCtx,0);` |
|        - |  8183 | `	}` |
|       14 |  8184 | `	return rc;` |
|        8 |  8185 |  |
|        - |  8186 | `/*` |
|        - |  8187 | ` * int error_reporting([int $level])` |
|        - |  8188 | ` *  Sets which PHP errors are reported.` |
|        - |  8189 | ` * Parameters` |
|        - |  8190 | ` *  $level` |
|        - |  8191 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  8192 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  8193 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  8194 | ` *   levels will not always behave as expected.` |
|        - |  8195 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  8196 | ` *   in the predefined constants.` |
|        - |  8197 | ` * Return` |
|        - |  8198 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  8199 | ` *   parameter is given.` |
|        - |  8200 | ` */` |
|       40 |  8201 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8202 |  |
|       42 |  8203 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8204 | `	int nOld;` |
|        - |  8205 | `	/* Extract the old reporting level */` |
|       42 |  8206 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       42 |  8207 | `	if( nArg > 0 ){` |
|        - |  8208 | `		int nNew;` |
|        - |  8209 | `		/* Extract the desired error reporting level */` |
|       34 |  8210 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       34 |  8211 | `		if( !nNew ){` |
|        - |  8212 | `			/* Do not report errors at all */` |
|        5 |  8213 | `			pVm->bErrReport = 0;` |
|        3 |  8214 | `		}else{` |
|        - |  8215 | `			/* Report all errors */` |
|       30 |  8216 | `			pVm->bErrReport = 1;` |
|        - |  8217 | `		}` |
|       16 |  8218 | `	}` |
|        - |  8219 | `	/* Return the old level */` |
|       42 |  8220 | `	ph7_result_int(pCtx,nOld);` |
|       42 |  8221 | `	return PH7_OK;` |
|        2 |  8222 |  |
|        - |  8223 | `/*` |
|        - |  8224 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  8225 | ` *  Send an error message somewhere.` |
|        - |  8226 | ` * Parameter` |
|        - |  8227 | ` *  $message` |
|        - |  8228 | ` *   The error message that should be logged.` |
|        - |  8229 | ` *  $message_type` |
|        - |  8230 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  8231 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  8232 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  8233 | ` *       This is the default option.` |
|        - |  8234 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  8235 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  8236 | ` *    2  No longer an option.` |
|        - |  8237 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  8238 | ` *       to the end of the message string.` |
|        - |  8239 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  8240 | ` *  $destination` |
|        - |  8241 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  8242 | ` *  $extra_headers` |
|        - |  8243 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  8244 | ` * Return` |
|        - |  8245 | ` *  TRUE on success or FALSE on failure.` |
|        - |  8246 | ` * NOTE:` |
|        - |  8247 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  8248 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  8249 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  8250 | ` *  Otherwise this function is no-op.` |
|        - |  8251 | ` */` |
|        4 |  8252 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8253 |  |
|        - |  8254 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  8255 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  8256 | `	int iType = 0;` |
|        5 |  8257 | `	if( nArg < 1 ){` |
|        - |  8258 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  8259 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8260 | `		return PH7_OK;` |
|        - |  8261 | `	}` |
|        5 |  8262 | `	if( pVm->xErrLog  ){` |
|        - |  8263 | `		/* Invoke the user callback */` |
|      ! 0 |  8264 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  8265 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  8266 | `		if( nArg > 1 ){` |
|      ! 0 |  8267 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  8268 | `			if( nArg > 2 ){` |
|      ! 0 |  8269 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  8270 | `				if( nArg > 3 ){` |
|      ! 0 |  8271 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  8272 | `				}` |
|      ! 0 |  8273 | `			}` |
|      ! 0 |  8274 | `		}` |
|      ! 0 |  8275 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  8276 | `	}` |
|        - |  8277 | `	/* Retun TRUE */` |
|        5 |  8278 | `	ph7_result_bool(pCtx,1);` |
|        5 |  8279 | `	return PH7_OK;` |
|        3 |  8280 |  |
|        - |  8281 | `/*` |
|        - |  8282 | ` * bool restore_exception_handler(void)` |
|        - |  8283 | ` *  Restores the previously defined exception handler function.` |
|        - |  8284 | ` * Parameter` |
|        - |  8285 | ` *  None` |
|        - |  8286 | ` * Return` |
|        - |  8287 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  8288 | ` */` |
|        4 |  8289 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8290 |  |
|        5 |  8291 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8292 | `	ph7_value *pOld,*pNew;` |
|        - |  8293 | `	/* Point to the old and the new handler */` |
|        5 |  8294 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  8295 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  8296 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8297 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8298 | `		SXUNUSED(apArg);` |
|        - |  8299 | `		/* No installed handler,return FALSE */` |
|        5 |  8300 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8301 | `		return PH7_OK;` |
|        - |  8302 | `	}` |
|        - |  8303 | `	/* Copy the old handler */` |
|      ! 0 |  8304 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8305 | `	PH7_MemObjRelease(pOld);` |
|        - |  8306 | `	/* Return TRUE */` |
|      ! 0 |  8307 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8308 | `	return PH7_OK;` |
|        3 |  8309 |  |
|        - |  8310 | `/*` |
|        - |  8311 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  8312 | ` *  Sets a user-defined exception handler function.` |
|        - |  8313 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  8314 | ` * NOTE` |
|        - |  8315 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  8316 | ` *  the satndard PHP engine.` |
|        - |  8317 | ` * Parameters` |
|        - |  8318 | ` *  $exception_handler` |
|        - |  8319 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  8320 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  8321 | ` *   that was thrown.` |
|        - |  8322 | ` *  Note:` |
|        - |  8323 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8324 | ` * Return` |
|        - |  8325 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  8326 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8327 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8328 | ` */` |
|        4 |  8329 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8330 |  |
|        6 |  8331 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8332 | `	ph7_value *pOld,*pNew;` |
|        - |  8333 | `	/* Point to the old and the new handler */` |
|        6 |  8334 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  8335 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  8336 | `	/* Return the old handler */` |
|        6 |  8337 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  8338 | `	if( nArg > 0 ){` |
|        6 |  8339 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8340 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  8341 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  8342 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  8343 | `		}else{` |
|        6 |  8344 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8345 | `			/* Install the new handler */` |
|        6 |  8346 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8347 | `		}` |
|        2 |  8348 | `	}` |
|        6 |  8349 | `	return PH7_OK;` |
|        2 |  8350 |  |
|        - |  8351 | `/*` |
|        - |  8352 | ` * bool restore_error_handler(void)` |
|        - |  8353 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8354 | ` * Parameters:` |
|        - |  8355 | ` *  None.` |
|        - |  8356 | ` * Return` |
|        - |  8357 | ` *  Always TRUE.` |
|        - |  8358 | ` */` |
|        4 |  8359 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8360 |  |
|        5 |  8361 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8362 | `	ph7_value *pOld,*pNew;` |
|        - |  8363 | `	/* Point to the old and the new handler */` |
|        5 |  8364 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  8365 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  8366 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8367 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8368 | `		SXUNUSED(apArg);` |
|        - |  8369 | `		/* No installed callback,return FALSE */` |
|        5 |  8370 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8371 | `		return PH7_OK;` |
|        - |  8372 | `	}` |
|        - |  8373 | `	/* Copy the old callback */` |
|      ! 0 |  8374 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8375 | `	PH7_MemObjRelease(pOld);` |
|        - |  8376 | `	/* Return TRUE */` |
|      ! 0 |  8377 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8378 | `	return PH7_OK;` |
|        3 |  8379 |  |
|        - |  8380 | `/*` |
|        - |  8381 | ` * value set_error_handler(callable $error_handler)` |
|        - |  8382 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8383 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8384 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8385 | ` *  Sets a user-defined error handler function.` |
|        - |  8386 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  8387 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  8388 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  8389 | ` *  conditions (using trigger_error()).` |
|        - |  8390 | ` * Parameters` |
|        - |  8391 | ` *  $error_handler` |
|        - |  8392 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  8393 | ` *   describing the error.` |
|        - |  8394 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  8395 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  8396 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  8397 | ` *   The function can be shown as:` |
|        - |  8398 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  8399 | ` *     errno` |
|        - |  8400 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  8401 | ` *   errstr` |
|        - |  8402 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  8403 | ` *   errfile` |
|        - |  8404 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  8405 | ` *     was raised in, as a string.` |
|        - |  8406 | ` *  Note:` |
|        - |  8407 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8408 | ` * Return` |
|        - |  8409 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  8410 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8411 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8412 | ` */` |
|     8722 |  8413 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8414 |  |
|     8724 |  8415 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8416 | `	ph7_value *pOld,*pNew;` |
|        - |  8417 | `	/* Point to the old and the new handler */` |
|     8724 |  8418 | `	pOld = &pVm->aErrCB[0];` |
|     8724 |  8419 | `	pNew = &pVm->aErrCB[1];` |
|        - |  8420 | `	/* Return the old handler */` |
|     8724 |  8421 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8724 |  8422 | `	if( nArg > 0 ){` |
|     8724 |  8423 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8424 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4361 |  8425 | `			PH7_MemObjRelease(pNew);` |
|     4361 |  8426 | `			ph7_result_bool(pCtx,1);` |
|     2181 |  8427 | `		}else{` |
|     4364 |  8428 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8429 | `			/* Install the new handler */` |
|     4364 |  8430 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8431 | `		}` |
|     4361 |  8432 | `	}` |
|     8724 |  8433 | `	return PH7_OK;` |
|        2 |  8434 |  |
|        - |  8435 | `/*` |
|        - |  8436 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  8437 | ` *  Generates a backtrace.` |
|        - |  8438 | ` * Paramaeter` |
|        - |  8439 | ` *  $options` |
|        - |  8440 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  8441 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  8442 | ` *   all the function/method arguments, to save memory.` |
|        - |  8443 | ` * $limit` |
|        - |  8444 | ` *   (Not Used)` |
|        - |  8445 | ` * Return` |
|        - |  8446 | ` *  An array.The possible returned elements are as follows:` |
|        - |  8447 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  8448 | ` *          Name        Type      Description` |
|        - |  8449 | ` *          ------      ------     -----------` |
|        - |  8450 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  8451 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  8452 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  8453 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  8454 | ` *          object      object    The current object.` |
|        - |  8455 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  8456 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  8457 | ` */` |
|      492 |  8458 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8459 |  |
|      494 |  8460 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8461 | `	ph7_value *pArray;` |
|        - |  8462 | `	ph7_class *pClass;` |
|        - |  8463 | `	ph7_value *pValue;` |
|        - |  8464 | `	SyString *pFile;` |
|        - |  8465 | `	/* Create a new array */` |
|      494 |  8466 | `	pArray = ph7_context_new_array(pCtx);` |
|      494 |  8467 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      494 |  8468 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8469 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8470 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  8471 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8472 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8473 | `		SXUNUSED(apArg);` |
|      ! 0 |  8474 | `		return PH7_OK;` |
|        - |  8475 | `	}` |
|        - |  8476 | `	/* Dump running function name and it's arguments  */` |
|      494 |  8477 | `	if( pVm->pFrame->pParent ){` |
|      494 |  8478 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8479 | `		ph7_vm_func *pFunc;` |
|        - |  8480 | `		ph7_value *pArg;` |
|      494 |  8481 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8482 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  8483 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  8484 | `		}` |
|      494 |  8485 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      494 |  8486 | `		if( pFrame->pParent && pFunc ){` |
|      494 |  8487 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      494 |  8488 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      494 |  8489 | `			ph7_value_reset_string_cursor(pValue);` |
|      246 |  8490 | `		}` |
|        - |  8491 | `		/* Function arguments */` |
|      494 |  8492 | `		pArg = ph7_context_new_array(pCtx);` |
|      494 |  8493 | `		if( pArg  ){` |
|        - |  8494 | `			ph7_value *pObj;` |
|        - |  8495 | `			VmSlot *aSlot;` |
|        - |  8496 | `			sxu32 n;` |
|        - |  8497 | `			/* Start filling the array with the given arguments */` |
|      494 |  8498 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     1962 |  8499 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1470 |  8500 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1470 |  8501 | `				if( pObj ){` |
|     1470 |  8502 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      734 |  8503 | `				}` |
|      736 |  8504 | `			}` |
|        - |  8505 | `			/* Save the array */` |
|      494 |  8506 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      246 |  8507 | `		}` |
|      246 |  8508 | `	}` |
|      494 |  8509 | `	ph7_value_int(pValue,1);` |
|        - |  8510 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  8511 | `	 * line numbers at run-time. )` |
|        - |  8512 | `	 */` |
|      494 |  8513 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  8514 | `	/* Current processed script */` |
|      494 |  8515 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      494 |  8516 | `	if( pFile ){` |
|      494 |  8517 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      494 |  8518 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      494 |  8519 | `		ph7_value_reset_string_cursor(pValue);` |
|      246 |  8520 | `	}` |
|        - |  8521 | `	/* Top class */` |
|      494 |  8522 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      494 |  8523 | `	if( pClass ){` |
|      490 |  8524 | `		ph7_value_reset_string_cursor(pValue);` |
|      490 |  8525 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      490 |  8526 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      244 |  8527 | `	}` |
|        - |  8528 | `	/* Return the freshly created array */` |
|      494 |  8529 | `	ph7_result_value(pCtx,pArray);` |
|        - |  8530 | `	/*` |
|        - |  8531 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  8532 | `	 * as soon we return from this function.` |
|        - |  8533 | `	 */` |
|      494 |  8534 | `	return PH7_OK;` |
|      248 |  8535 |  |
|        - |  8536 | `/*` |
|        - |  8537 | ` * Generate a small backtrace.` |
|        - |  8538 | ` * Store the generated dump in the given BLOB` |
|        - |  8539 | ` */` |
|        4 |  8540 | `static int VmMiniBacktrace(` |
|        - |  8541 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8542 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  8543 | `	)` |
|        1 |  8544 |  |
|        5 |  8545 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8546 | `	ph7_vm_func *pFunc;` |
|        - |  8547 | `	ph7_class *pClass;` |
|        - |  8548 | `	SyString *pFile;` |
|        - |  8549 | `	/* Called function */` |
|        5 |  8550 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8551 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  8552 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  8553 | `	}` |
|        5 |  8554 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  8555 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8556 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  8557 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  8558 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  8559 | `	}else{` |
|      ! 0 |  8560 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  8561 | `	}` |
|        5 |  8562 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  8563 | `	/* Current processed script */` |
|        5 |  8564 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  8565 | `	if( pFile ){` |
|        5 |  8566 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8567 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  8568 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  8569 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  8570 | `	}` |
|        - |  8571 | `	/* Top class */` |
|        5 |  8572 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  8573 | `	if( pClass ){` |
|      ! 0 |  8574 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  8575 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  8576 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  8577 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  8578 | `	}` |
|        5 |  8579 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  8580 | `	/* All done */` |
|        5 |  8581 | `	return SXRET_OK;` |
|        1 |  8582 |  |
|        - |  8583 | `/*` |
|        - |  8584 | ` * void debug_print_backtrace()` |
|        - |  8585 | ` *  Prints a backtrace` |
|        - |  8586 | ` * Parameters` |
|        - |  8587 | ` * None` |
|        - |  8588 | ` * Return` |
|        - |  8589 | ` * NULL` |
|        - |  8590 | ` */` |
|        2 |  8591 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8592 |  |
|        3 |  8593 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8594 | `	SyBlob sDump;` |
|        3 |  8595 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8596 | `	/* Generate the backtrace */` |
|        3 |  8597 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8598 | `	/* Output backtrace */` |
|        3 |  8599 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8600 | `	/* All done,cleanup */` |
|        3 |  8601 | `	SyBlobRelease(&sDump);` |
|        1 |  8602 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8603 | `	SXUNUSED(apArg);` |
|        3 |  8604 | `	return PH7_OK;` |
|        1 |  8605 |  |
|        - |  8606 | `/*` |
|        - |  8607 | ` * string debug_string_backtrace()` |
|        - |  8608 | ` *  Generate a backtrace` |
|        - |  8609 | ` * Parameters` |
|        - |  8610 | ` * None` |
|        - |  8611 | ` * Return` |
|        - |  8612 | ` *  A mini backtrace().` |
|        - |  8613 | ` * Note that this is a symisc extension.` |
|        - |  8614 | ` */` |
|        2 |  8615 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8616 |  |
|        3 |  8617 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8618 | `	SyBlob sDump;` |
|        3 |  8619 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8620 | `	/* Generate the backtrace */` |
|        3 |  8621 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8622 | `	/* Return the backtrace */` |
|        3 |  8623 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  8624 | `	/* All done,cleanup */` |
|        3 |  8625 | `	SyBlobRelease(&sDump);` |
|        1 |  8626 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8627 | `	SXUNUSED(apArg);` |
|        3 |  8628 | `	return PH7_OK;` |
|        1 |  8629 |  |
|        - |  8630 | `/*` |
|        - |  8631 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  8632 | ` * exception is triggered.` |
|        - |  8633 | ` */` |
|      472 |  8634 | `static sxi32 VmUncaughtException(` |
|        - |  8635 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8636 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8637 | `	)` |
|        1 |  8638 |  |
|        - |  8639 | `	ph7_value *apArg[2],sArg;` |
|      473 |  8640 | `	int nArg = 1;` |
|        - |  8641 | `	sxi32 rc;` |
|      473 |  8642 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  8643 | `		/* Nesting limit reached */` |
|      ! 0 |  8644 | `		return SXRET_OK;` |
|        - |  8645 | `	}` |
|        - |  8646 | `	/* Call any exception handler if available */` |
|      473 |  8647 | `	PH7_MemObjInit(pVm,&sArg);` |
|      473 |  8648 | `	if( pThis ){` |
|        - |  8649 | `		/* Load the exception instance */` |
|      473 |  8650 | `		sArg.x.pOther = pThis;` |
|      473 |  8651 | `		pThis->iRef++;` |
|      473 |  8652 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      237 |  8653 | `	}else{` |
|      ! 0 |  8654 | `		nArg = 0;` |
|        - |  8655 | `	}` |
|      473 |  8656 | `	apArg[0] = &sArg;` |
|        - |  8657 | `	/* Call the exception handler if available */` |
|      473 |  8658 | `	pVm->nExceptDepth++;` |
|      473 |  8659 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      473 |  8660 | `	pVm->nExceptDepth--;` |
|      473 |  8661 | `	if( rc != SXRET_OK ){` |
|        - |  8662 | `		SyBlob sMsgBuf;` |
|      471 |  8663 | `		const char *zClass = "Exception";` |
|      471 |  8664 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  8665 | `		const char *zMsg;` |
|        - |  8666 | `		sxu32 nMsg;` |
|        - |  8667 | `		const char *zFuncName;` |
|        - |  8668 | `		int nFuncLen;` |
|      471 |  8669 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      471 |  8670 | `		if( pThis ){` |
|        - |  8671 | `			ph7_class_method *pGetMessage;` |
|        - |  8672 | `			ph7_value sMsg;` |
|        - |  8673 | `			const char *zTmp;` |
|        - |  8674 | `			int nTmp;` |
|      471 |  8675 | `			zClass = pThis->pClass->sName.zString;` |
|      471 |  8676 | `			nClass = pThis->pClass->sName.nByte;` |
|      471 |  8677 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      471 |  8678 | `			if( pGetMessage ){` |
|      471 |  8679 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      471 |  8680 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      471 |  8681 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      471 |  8682 | `					if( zTmp && nTmp > 0 ){` |
|      471 |  8683 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      235 |  8684 | `					}` |
|      235 |  8685 | `				}` |
|      471 |  8686 | `				PH7_MemObjRelease(&sMsg);` |
|      235 |  8687 | `			}` |
|      235 |  8688 | `		}` |
|      471 |  8689 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  8690 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  8691 | `		}` |
|      471 |  8692 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      471 |  8693 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      471 |  8694 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      471 |  8695 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      471 |  8696 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  8697 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      471 |  8698 | `		rc = SXERR_ABORT;` |
|      235 |  8699 | `	}` |
|      473 |  8700 | `	PH7_MemObjRelease(&sArg);` |
|      473 |  8701 | `	return rc;` |
|      237 |  8702 |  |
|        - |  8703 | `/*` |
|        - |  8704 | ` * Throw an user exception.` |
|        - |  8705 | ` */` |
|      490 |  8706 | `static sxi32 VmThrowException(` |
|        - |  8707 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  8708 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8709 | `	)` |
|        2 |  8710 |  |
|        - |  8711 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  8712 | `	ph7_exception **apException;` |
|        - |  8713 | `	ph7_exception *pException;` |
|        - |  8714 | `	/* Point to the stack of loaded exceptions */` |
|      492 |  8715 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      492 |  8716 | `	pException = 0;` |
|      492 |  8717 | `	pCatch = 0;` |
|      492 |  8718 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8719 | `		ph7_exception_block *aCatch;` |
|        - |  8720 | `		ph7_class *pClass;` |
|        - |  8721 | `		sxu32 j;` |
|        - |  8722 | `		/* Locate the appropriate block to execute */` |
|       20 |  8723 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       20 |  8724 | `		(void)SySetPop(&pVm->aException);` |
|       20 |  8725 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       20 |  8726 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       20 |  8727 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  8728 | `			/* Extract the target class */` |
|       20 |  8729 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       20 |  8730 | `			if( pClass == 0 ){` |
|        - |  8731 | `				/* No such class */` |
|      ! 0 |  8732 | `				continue;` |
|        - |  8733 | `			}` |
|       20 |  8734 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  8735 | `				/* Catch block found,break immeditaley */` |
|       20 |  8736 | `				pCatch = &aCatch[j];` |
|       20 |  8737 | `				break;` |
|        - |  8738 | `			}` |
|      ! 0 |  8739 | `		}` |
|        9 |  8740 | `	}` |
|        - |  8741 | `	/* Execute the cached block if available */` |
|      492 |  8742 | `	if( pCatch == 0 ){` |
|        - |  8743 | `		sxi32 rc;` |
|      473 |  8744 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      473 |  8745 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  8746 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  8747 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8748 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  8749 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  8750 | `			}` |
|      ! 0 |  8751 | `			if( pException->pFrame == pFrame ){` |
|        - |  8752 | `				/* Tell the upper layer that the exception was caught */` |
|      ! 0 |  8753 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  8754 | `			}` |
|      ! 0 |  8755 | `		}` |
|      473 |  8756 | `		return rc;` |
|      ! 0 |  8757 | `	}else{` |
|       20 |  8758 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8759 | `		sxi32 rc;` |
|       30 |  8760 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8761 | `			/* Safely ignore the exception frame */` |
|       12 |  8762 | `			pFrame = pFrame->pParent;` |
|        2 |  8763 | `		}` |
|       20 |  8764 | `		if( pException->pFrame == pFrame ){` |
|        - |  8765 | `			/* Tell the upper layer that the exception was caught */` |
|       12 |  8766 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        5 |  8767 | `		}` |
|        - |  8768 | `		/* Create a private frame first */` |
|       20 |  8769 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       20 |  8770 | `		if( rc == SXRET_OK ){` |
|        - |  8771 | `			/* Mark as catch frame */` |
|       20 |  8772 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       20 |  8773 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       20 |  8774 | `			if( pObj ){` |
|        - |  8775 | `				/* Install the exception instance */` |
|       20 |  8776 | `				pThis->iRef++; /* Increment reference count */` |
|       20 |  8777 | `				pObj->x.pOther = pThis;` |
|       20 |  8778 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        9 |  8779 | `			}` |
|        - |  8780 | `			/* Exceute the block */` |
|       20 |  8781 | `			VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  8782 | `			/* Leave the frame */` |
|       20 |  8783 | `			VmLeaveFrame(&(*pVm));` |
|        9 |  8784 | `		}` |
|        - |  8785 | `	}` |
|        - |  8786 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  8787 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  8788 | `	 */` |
|       20 |  8789 | `	return SXRET_OK;` |
|      247 |  8790 |  |
|        - |  8791 | `/*` |
|        - |  8792 | ` * Section:` |
|        - |  8793 | ` *  Version,Credits and Copyright related functions.` |
|        - |  8794 | ` * Status:` |
|        - |  8795 | ` *    Stable.` |
|        - |  8796 | ` */` |
|        - |  8797 | `/*` |
|        - |  8798 | ` * string ph7version(void)` |
|        - |  8799 | ` *  Returns the running version of the PH7 version.` |
|        - |  8800 | ` * Parameters` |
|        - |  8801 | ` *  None` |
|        - |  8802 | ` * Return` |
|        - |  8803 | ` * Current PH7 version.` |
|        - |  8804 | ` */` |
|        2 |  8805 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8806 |  |
|        1 |  8807 | `	SXUNUSED(nArg);` |
|        1 |  8808 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  8809 | `	/* Current engine version */` |
|        3 |  8810 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  8811 | `	return PH7_OK;` |
|        1 |  8812 |  |
|        - |  8813 | `/*` |
|        - |  8814 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  8815 | ` */` |
|        - |  8816 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  8817 | ` "<html><head>"\` |
|        - |  8818 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  8819 | ` "<style type=\"text/css\">"\` |
|        - |  8820 | ` "div {"\` |
|        - |  8821 | `     "border: 1px solid #cccccc;"\` |
|        - |  8822 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  8823 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  8824 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  8825 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  8826 | `     "-webkit-border-radius: 10px;"\` |
|        - |  8827 | `     "-o-border-radius: 10px;"\` |
|        - |  8828 | `     "border-radius: 10px;"\` |
|        - |  8829 | `     "padding-left: 2em;"\` |
|        - |  8830 | `     "background-color: white;"\` |
|        - |  8831 | `     "margin-left: auto;"\` |
|        - |  8832 | `     "font-family: verdana;"\` |
|        - |  8833 | `     "padding-right: 2em;"\` |
|        - |  8834 | `     "margin-right: auto;"\` |
|        - |  8835 | `     "}"\` |
|        - |  8836 | `     "body {"\` |
|        - |  8837 | `     "padding: 0.2em;"\` |
|        - |  8838 | `     "font-style: normal;"\` |
|        - |  8839 | `     "font-size: medium;"\` |
|        - |  8840 | `     "background-color: #f2f2f2;"\` |
|        - |  8841 | `     "}"\` |
|        - |  8842 | `     "hr {"\` |
|        - |  8843 | `     "border-style: solid none none;"\` |
|        - |  8844 | `     "border-width: 1px medium medium;"\` |
|        - |  8845 | `     "border-top: 1px solid #cccccc;"\` |
|        - |  8846 | `     "height: 1px;"\` |
|        - |  8847 | `     "}"\` |
|        - |  8848 | `     "a {"\` |
|        - |  8849 | `     "color: #3366cc;"\` |
|        - |  8850 | `     "text-decoration: none;"\` |
|        - |  8851 | `     "}"\` |
|        - |  8852 | `     "a:hover {"\` |
|        - |  8853 | `     "color: #999999;"\` |
|        - |  8854 | `     "}"\` |
|        - |  8855 | `     "a:active {"\` |
|        - |  8856 | `     "color: #663399;"\` |
|        - |  8857 | `     "}"\` |
|        - |  8858 | `     "h1 {"\` |
|        - |  8859 | `     "margin: 0;"\` |
|        - |  8860 | `     "padding: 0;"\` |
|        - |  8861 | `     "font-family: Verdana;"\` |
|        - |  8862 | `     "font-weight: bold;"\` |
|        - |  8863 | `     "font-style: normal;"\` |
|        - |  8864 | `     "font-size: medium;"\` |
|        - |  8865 | `     "text-transform: capitalize;"\` |
|        - |  8866 | `     "color: #0a328c;"\` |
|        - |  8867 | `     "}"\` |
|        - |  8868 | `     "p {"\` |
|        - |  8869 | `     "margin: 0 auto;"\` |
|        - |  8870 | `     "font-size: medium;"\` |
|        - |  8871 | `     "font-style: normal;"\` |
|        - |  8872 | `     "font-family: verdana;"\` |
|        - |  8873 | `     "}"\` |
|        - |  8874 | `"</style></head><body>"\` |
|        - |  8875 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - |  8876 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - |  8877 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - |  8878 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - |  8879 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - |  8880 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - |  8881 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - |  8882 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - |  8883 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - |  8884 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - |  8885 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - |  8886 |  |
|        - |  8887 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8888 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - |  8889 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - |  8890 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - |  8891 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8892 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - |  8893 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  8894 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - |  8895 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  8896 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - |  8897 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8898 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - |  8899 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - |  8900 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - |  8901 |  |
|        - |  8902 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - |  8903 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - |  8904 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - |  8905 | `"&nbsp;*<br>"\` |
|        - |  8906 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - |  8907 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - |  8908 | `"&nbsp;* are met:<br>"\` |
|        - |  8909 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - |  8910 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - |  8911 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - |  8912 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - |  8913 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - |  8914 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - |  8915 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - |  8916 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - |  8917 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - |  8918 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - |  8919 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - |  8920 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - |  8921 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - |  8922 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - |  8923 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - |  8924 | `"&nbsp;*<br>"\` |
|        - |  8925 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - |  8926 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - |  8927 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - |  8928 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - |  8929 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - |  8930 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - |  8931 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - |  8932 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - |  8933 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - |  8934 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - |  8935 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - |  8936 | `"&nbsp;*/<br>"\` |
|        - |  8937 | `"</span></small></small></p>"\` |
|        - |  8938 | `"</div></body></html>"` |
|        - |  8939 | `/*` |
|        - |  8940 | ` * bool ph7credits(void)` |
|        - |  8941 | ` * bool ph7info(void)` |
|        - |  8942 | ` * bool ph7copyright(void)` |
|        - |  8943 | ` *  Prints out the credits for PH7 engine` |
|        - |  8944 | ` * Parameters` |
|        - |  8945 | ` *  None` |
|        - |  8946 | ` * Return` |
|        - |  8947 | ` *  Always TRUE` |
|        - |  8948 | ` */` |
|        2 |  8949 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8950 |  |
|        3 |  8951 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - |  8952 | `	/* Expand the HTML page above*/` |
|        3 |  8953 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 |  8954 | `	ph7_context_output_format(` |
|        1 |  8955 | `		pCtx,` |
|        - |  8956 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 |  8957 | `		ph7_lib_version(),   /* Engine version */` |
|        1 |  8958 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 |  8959 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 |  8960 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 |  8961 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 |  8962 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - |  8963 | `#ifdef __WINNT__` |
|        - |  8964 | `		"Windows NT"` |
|        - |  8965 | `#elif defined(__UNIXES__)` |
|        - |  8966 | `		"UNIX-Like"` |
|        - |  8967 | `#else` |
|        - |  8968 | `		"Other OS"` |
|        - |  8969 | `#endif` |
|        - |  8970 | `		);` |
|        3 |  8971 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 |  8972 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8973 | `	SXUNUSED(apArg);` |
|        - |  8974 | `	/* Return TRUE */` |
|        - |  8975 | `	//ph7_result_bool(pCtx,1);` |
|        3 |  8976 | `	return PH7_OK;` |
|        1 |  8977 |  |
|        - |  8978 | `/*` |
|        - |  8979 | ` * Section:` |
|        - |  8980 | ` *    URL related routines.` |
|        - |  8981 | ` * Status:` |
|        - |  8982 | ` *    Stable.` |
|        - |  8983 | ` */` |
|        - |  8984 | `/*` |
|        - |  8985 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - |  8986 | ` *  Parse a URL and return its fields.` |
|        - |  8987 | ` * Parameters` |
|        - |  8988 | ` *  $url` |
|        - |  8989 | ` *   The URL to parse.` |
|        - |  8990 | ` * $component` |
|        - |  8991 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - |  8992 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - |  8993 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - |  8994 | ` *  in which case the return value will be an integer).` |
|        - |  8995 | ` * Return` |
|        - |  8996 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - |  8997 | ` *  At least one element will be present within the array. Potential keys within` |
|        - |  8998 | ` *  this array are:` |
|        - |  8999 | ` *   scheme - e.g. http` |
|        - |  9000 | ` *   host` |
|        - |  9001 | ` *   port` |
|        - |  9002 | ` *   user` |
|        - |  9003 | ` *   pass` |
|        - |  9004 | ` *   path` |
|        - |  9005 | ` *   query - after the question mark ?` |
|        - |  9006 | ` *   fragment - after the hashmark #` |
|        - |  9007 | ` * Note:` |
|        - |  9008 | ` *  FALSE is returned on failure.` |
|        - |  9009 | ` *  This function work with relative URL unlike the one shipped` |
|        - |  9010 | ` *  with the standard PHP engine.` |
|        - |  9011 | ` */` |
|       28 |  9012 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9013 |  |
|        - |  9014 | `	const char *zStr; /* Input string */` |
|        - |  9015 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - |  9016 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - |  9017 | `	int nLen;` |
|        - |  9018 | `	sxi32 rc;` |
|       29 |  9019 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  9020 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  9021 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9022 | `		return PH7_OK;` |
|        - |  9023 | `	}` |
|        - |  9024 | `	/* Extract the given URI */` |
|       29 |  9025 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 |  9026 | `	if( nLen < 1 ){` |
|        - |  9027 | `		/* Nothing to process,return FALSE */` |
|        3 |  9028 | `		ph7_result_bool(pCtx,0);` |
|        3 |  9029 | `		return PH7_OK;` |
|        - |  9030 | `	}` |
|        - |  9031 | `	/* Get a parse */` |
|       27 |  9032 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 |  9033 | `	if( rc != SXRET_OK ){` |
|        - |  9034 | `		/* Malformed input,return FALSE */` |
|      ! 0 |  9035 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9036 | `		return PH7_OK;` |
|        - |  9037 | `	}` |
|       27 |  9038 | `	if( nArg > 1 ){` |
|      ! 0 |  9039 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - |  9040 | `		/* Refer to constant.c for constants values */` |
|      ! 0 |  9041 | `		switch(nComponent){` |
|      ! 0 |  9042 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 |  9043 | `			pComp = &sURI.sScheme;` |
|      ! 0 |  9044 | `			if( pComp->nByte < 1 ){` |
|        - |  9045 | `				/* No available value,return NULL */` |
|      ! 0 |  9046 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9047 | `			}else{` |
|      ! 0 |  9048 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9049 | `			}` |
|      ! 0 |  9050 | `			break;` |
|      ! 0 |  9051 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 |  9052 | `			pComp = &sURI.sHost;` |
|      ! 0 |  9053 | `			if( pComp->nByte < 1 ){` |
|        - |  9054 | `				/* No available value,return NULL */` |
|      ! 0 |  9055 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9056 | `			}else{` |
|      ! 0 |  9057 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9058 | `			}` |
|      ! 0 |  9059 | `			break;` |
|      ! 0 |  9060 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 |  9061 | `			pComp = &sURI.sPort;` |
|      ! 0 |  9062 | `			if( pComp->nByte < 1 ){` |
|        - |  9063 | `				/* No available value,return NULL */` |
|      ! 0 |  9064 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9065 | `			}else{` |
|      ! 0 |  9066 | `				int iPort = 0;` |
|        - |  9067 | `				/* Cast the value to integer */` |
|      ! 0 |  9068 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 |  9069 | `				ph7_result_int(pCtx,iPort);` |
|        - |  9070 | `			}` |
|      ! 0 |  9071 | `			break;` |
|      ! 0 |  9072 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 |  9073 | `			pComp = &sURI.sUser;` |
|      ! 0 |  9074 | `			if( pComp->nByte < 1 ){` |
|        - |  9075 | `				/* No available value,return NULL */` |
|      ! 0 |  9076 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9077 | `			}else{` |
|      ! 0 |  9078 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9079 | `			}` |
|      ! 0 |  9080 | `			break;` |
|      ! 0 |  9081 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 |  9082 | `			pComp = &sURI.sPass;` |
|      ! 0 |  9083 | `			if( pComp->nByte < 1 ){` |
|        - |  9084 | `				/* No available value,return NULL */` |
|      ! 0 |  9085 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9086 | `			}else{` |
|      ! 0 |  9087 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9088 | `			}` |
|      ! 0 |  9089 | `			break;` |
|      ! 0 |  9090 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 |  9091 | `			pComp = &sURI.sQuery;` |
|      ! 0 |  9092 | `			if( pComp->nByte < 1 ){` |
|        - |  9093 | `				/* No available value,return NULL */` |
|      ! 0 |  9094 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9095 | `			}else{` |
|      ! 0 |  9096 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9097 | `			}` |
|      ! 0 |  9098 | `			break;` |
|      ! 0 |  9099 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 |  9100 | `			pComp = &sURI.sFragment;` |
|      ! 0 |  9101 | `			if( pComp->nByte < 1 ){` |
|        - |  9102 | `				/* No available value,return NULL */` |
|      ! 0 |  9103 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9104 | `			}else{` |
|      ! 0 |  9105 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9106 | `			}` |
|      ! 0 |  9107 | `			break;` |
|      ! 0 |  9108 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 |  9109 | `			pComp = &sURI.sPath;` |
|      ! 0 |  9110 | `			if( pComp->nByte < 1 ){` |
|        - |  9111 | `				/* No available value,return NULL */` |
|      ! 0 |  9112 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9113 | `			}else{` |
|      ! 0 |  9114 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9115 | `			}` |
|      ! 0 |  9116 | `			break;` |
|      ! 0 |  9117 | `		default:` |
|        - |  9118 | `			/* No such entry,return NULL */` |
|      ! 0 |  9119 | `			ph7_result_null(pCtx);` |
|      ! 0 |  9120 | `			break;` |
|        - |  9121 | `		}` |
|      ! 0 |  9122 | `	}else{` |
|        - |  9123 | `		ph7_value *pArray,*pValue;` |
|        - |  9124 | `		/* Return an associative array */` |
|       27 |  9125 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 |  9126 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 |  9127 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9128 | `			/* Out of memory */` |
|      ! 0 |  9129 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9130 | `			/* Return false */` |
|      ! 0 |  9131 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |  9132 | `			return PH7_OK;` |
|        - |  9133 | `		}` |
|        - |  9134 | `		/* Fill the array */` |
|       27 |  9135 | `		pComp = &sURI.sScheme;` |
|       27 |  9136 | `		if( pComp->nByte > 0 ){` |
|       19 |  9137 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 |  9138 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 |  9139 | `		}` |
|        - |  9140 | `		/* Reset the string cursor */` |
|       27 |  9141 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9142 | `		pComp = &sURI.sHost;` |
|       27 |  9143 | `		if( pComp->nByte > 0 ){` |
|       25 |  9144 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 |  9145 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 |  9146 | `		}` |
|        - |  9147 | `		/* Reset the string cursor */` |
|       27 |  9148 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9149 | `		pComp = &sURI.sPort;` |
|       27 |  9150 | `		if( pComp->nByte > 0 ){` |
|       11 |  9151 | `			int iPort = 0;/* cc warning */` |
|        - |  9152 | `			/* Convert to integer */` |
|       11 |  9153 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 |  9154 | `			ph7_value_int(pValue,iPort);` |
|       11 |  9155 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 |  9156 | `		}` |
|        - |  9157 | `		/* Reset the string cursor */` |
|       27 |  9158 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9159 | `		pComp = &sURI.sUser;` |
|       27 |  9160 | `		if( pComp->nByte > 0 ){` |
|        7 |  9161 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9162 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 |  9163 | `		}` |
|        - |  9164 | `		/* Reset the string cursor */` |
|       27 |  9165 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9166 | `		pComp = &sURI.sPass;` |
|       27 |  9167 | `		if( pComp->nByte > 0 ){` |
|        7 |  9168 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9169 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 |  9170 | `		}` |
|        - |  9171 | `		/* Reset the string cursor */` |
|       27 |  9172 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9173 | `		pComp = &sURI.sPath;` |
|       27 |  9174 | `		if( pComp->nByte > 0 ){` |
|       17 |  9175 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 |  9176 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 |  9177 | `		}` |
|        - |  9178 | `		/* Reset the string cursor */` |
|       27 |  9179 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9180 | `		pComp = &sURI.sQuery;` |
|       27 |  9181 | `		if( pComp->nByte > 0 ){` |
|        5 |  9182 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9183 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 |  9184 | `		}` |
|        - |  9185 | `		/* Reset the string cursor */` |
|       27 |  9186 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9187 | `		pComp = &sURI.sFragment;` |
|       27 |  9188 | `		if( pComp->nByte > 0 ){` |
|        5 |  9189 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9190 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 |  9191 | `		}` |
|        - |  9192 | `		/* Return the created array */` |
|       27 |  9193 | `		ph7_result_value(pCtx,pArray);` |
|        - |  9194 | `		/* NOTE:` |
|        - |  9195 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - |  9196 | `		 * automatically as soon we return from this function.` |
|        - |  9197 | `		 */` |
|        - |  9198 | `	}` |
|        - |  9199 | `	/* All done */` |
|       27 |  9200 | `	return PH7_OK;` |
|       15 |  9201 |  |
|        - |  9202 | `/*` |
|        - |  9203 | ` * Section:` |
|        - |  9204 | ` *   Array related routines.` |
|        - |  9205 | ` * Status:` |
|        - |  9206 | ` *    Stable.` |
|        - |  9207 | ` * Note 2012-5-21 01:04:15:` |
|        - |  9208 | ` *  Array related functions that need access to the underlying` |
|        - |  9209 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - |  9210 | ` */` |
|        - |  9211 | `/*` |
|        - |  9212 | ` * The [compact()] function store it's state information in an instance` |
|        - |  9213 | ` * of the following structure.` |
|        - |  9214 | ` */` |
|        - |  9215 | `struct compact_data` |
|        - |  9216 |  |
|        - |  9217 | `	ph7_value *pArray;  /* Target array */` |
|        - |  9218 | `	int nRecCount;      /* Recursion count */` |
|        - |  9219 | `};` |
|        - |  9220 | `/*` |
|        - |  9221 | ` * Walker callback for the [compact()] function defined below.` |
|        - |  9222 | ` */` |
|      ! 0 |  9223 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 |  9224 |  |
|      ! 0 |  9225 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 |  9226 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 |  9227 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9228 | `	/* Act according to the hashmap value */` |
|      ! 0 |  9229 | `	if( ph7_value_is_string(pValue) ){` |
|        - |  9230 | `		SyString sVar;` |
|      ! 0 |  9231 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 |  9232 | `		if( sVar.nByte > 0 ){` |
|        - |  9233 | `			/* Query the current frame */` |
|      ! 0 |  9234 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - |  9235 | `			/* ^` |
|        - |  9236 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - |  9237 | `			 */` |
|      ! 0 |  9238 | `			if( pKey ){` |
|        - |  9239 | `				/* Perform the insertion */` |
|      ! 0 |  9240 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 |  9241 | `			}` |
|      ! 0 |  9242 | `		}` |
|      ! 0 |  9243 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - |  9244 | `		int rc;` |
|        - |  9245 | `		/* Recursively traverse this array */` |
|      ! 0 |  9246 | `		pData->nRecCount++;` |
|      ! 0 |  9247 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 |  9248 | `		pData->nRecCount--;` |
|      ! 0 |  9249 | `		return rc;` |
|        - |  9250 | `	}` |
|      ! 0 |  9251 | `	return SXRET_OK;` |
|      ! 0 |  9252 |  |
|        - |  9253 | `/*` |
|        - |  9254 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - |  9255 | ` *  Create array containing variables and their values.` |
|        - |  9256 | ` *  For each of these, compact() looks for a variable with that name` |
|        - |  9257 | ` *  in the current symbol table and adds it to the output array such` |
|        - |  9258 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - |  9259 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - |  9260 | ` *  Any strings that are not set will simply be skipped.` |
|        - |  9261 | ` * Parameters` |
|        - |  9262 | ` *  $varname` |
|        - |  9263 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - |  9264 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - |  9265 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - |  9266 | ` *   it recursively.` |
|        - |  9267 | ` * Return` |
|        - |  9268 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - |  9269 | ` */` |
|        2 |  9270 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9271 |  |
|        - |  9272 | `	ph7_value *pArray,*pObj;` |
|        3 |  9273 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9274 | `	const char *zName;` |
|        - |  9275 | `	SyString sVar;` |
|        - |  9276 | `	int i,nLen;` |
|        3 |  9277 | `	if( nArg < 1 ){` |
|        - |  9278 | `		/* Missing arguments,return NULL */` |
|      ! 0 |  9279 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9280 | `		return PH7_OK;` |
|        - |  9281 | `	}` |
|        - |  9282 | `	/* Create the array */` |
|        3 |  9283 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9284 | `	if( pArray == 0 ){` |
|        - |  9285 | `		/* Out of memory */` |
|      ! 0 |  9286 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9287 | `		/* Return NULL */` |
|      ! 0 |  9288 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9289 | `		return PH7_OK;` |
|        - |  9290 | `	}` |
|        - |  9291 | `	/* Perform the requested operation */` |
|        7 |  9292 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 |  9293 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 |  9294 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - |  9295 | `				struct compact_data sData;` |
|      ! 0 |  9296 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - |  9297 | `				/* Recursively walk the array */` |
|      ! 0 |  9298 | `				sData.nRecCount = 0;` |
|      ! 0 |  9299 | `				sData.pArray = pArray;` |
|      ! 0 |  9300 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 |  9301 | `			}` |
|      ! 0 |  9302 | `		}else{` |
|        - |  9303 | `			/* Extract variable name */` |
|        5 |  9304 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 |  9305 | `			if( nLen > 0 ){` |
|        5 |  9306 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - |  9307 | `				/* Check if the variable is available in the current frame */` |
|        5 |  9308 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 |  9309 | `				if( pObj ){` |
|        5 |  9310 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 |  9311 | `				}` |
|        2 |  9312 | `			}` |
|        - |  9313 | `		}` |
|        3 |  9314 | `	}` |
|        - |  9315 | `	/* Return the array */` |
|        3 |  9316 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9317 | `	return PH7_OK;` |
|        2 |  9318 |  |
|        - |  9319 | `/*` |
|        - |  9320 | ` * The [extract()] function store it's state information in an instance` |
|        - |  9321 | ` * of the following structure.` |
|        - |  9322 | ` */` |
|        - |  9323 | `typedef struct extract_aux_data extract_aux_data;` |
|        - |  9324 | `struct extract_aux_data` |
|        - |  9325 |  |
|        - |  9326 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - |  9327 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - |  9328 | `	const char *zPrefix;  /* Prefix name */` |
|        - |  9329 | `	int Prefixlen;        /* Prefix  length */` |
|        - |  9330 | `	int iFlags;           /* Control flags */` |
|        - |  9331 | `	char zWorker[1024];   /* Working buffer */` |
|        - |  9332 | `};` |
|        - |  9333 | `/* Forward declaration */` |
|        - |  9334 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - |  9335 | `/*` |
|        - |  9336 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - |  9337 | ` *   Import variables into the current symbol table from an array.` |
|        - |  9338 | ` * Parameters` |
|        - |  9339 | ` * $var_array` |
|        - |  9340 | ` *  An associative array. This function treats keys as variable names and values` |
|        - |  9341 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - |  9342 | ` *  table, subject to extract_type and prefix parameters.` |
|        - |  9343 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - |  9344 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - |  9345 | ` * $extract_type` |
|        - |  9346 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - |  9347 | ` *  It can be one of the following values:` |
|        - |  9348 | ` *   EXTR_OVERWRITE` |
|        - |  9349 | ` *       If there is a collision, overwrite the existing variable.` |
|        - |  9350 | ` *   EXTR_SKIP` |
|        - |  9351 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - |  9352 | ` *   EXTR_PREFIX_SAME` |
|        - |  9353 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - |  9354 | ` *   EXTR_PREFIX_ALL` |
|        - |  9355 | ` *       Prefix all variable names with prefix.` |
|        - |  9356 | ` *   EXTR_PREFIX_INVALID` |
|        - |  9357 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - |  9358 | ` *   EXTR_IF_EXISTS` |
|        - |  9359 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - |  9360 | ` *       otherwise do nothing.` |
|        - |  9361 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - |  9362 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - |  9363 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - |  9364 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - |  9365 | ` *      the current symbol table.` |
|        - |  9366 | ` * $prefix` |
|        - |  9367 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - |  9368 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - |  9369 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - |  9370 | ` *  underscore character.` |
|        - |  9371 | ` * Return` |
|        - |  9372 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - |  9373 | ` */` |
|        4 |  9374 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9375 |  |
|        - |  9376 | `	extract_aux_data sAux;` |
|        - |  9377 | `	ph7_hashmap *pMap;` |
|        5 |  9378 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - |  9379 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 |  9380 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9381 | `		return PH7_OK;` |
|        - |  9382 | `	}` |
|        - |  9383 | `	/* Point to the target hashmap */` |
|        5 |  9384 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 |  9385 | `	if( pMap->nEntry < 1 ){` |
|        - |  9386 | `		/* Empty map,return  0 */` |
|      ! 0 |  9387 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9388 | `		return PH7_OK;` |
|        - |  9389 | `	}` |
|        - |  9390 | `	/* Prepare the aux data */` |
|        5 |  9391 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 |  9392 | `	if( nArg > 1 ){` |
|        3 |  9393 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 |  9394 | `		if( nArg > 2 ){` |
|      ! 0 |  9395 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 |  9396 | `		}` |
|        1 |  9397 | `	}` |
|        5 |  9398 | `	sAux.pVm = pCtx->pVm;` |
|        - |  9399 | `	/* Invoke the worker callback */` |
|        5 |  9400 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - |  9401 | `	/* Number of variables successfully imported */` |
|        5 |  9402 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 |  9403 | `	return PH7_OK;` |
|        3 |  9404 |  |
|        - |  9405 | `/*` |
|        - |  9406 | ` * Worker callback for the [extract()] function defined` |
|        - |  9407 | ` * below.` |
|        - |  9408 | ` */` |
|        8 |  9409 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9410 |  |
|        9 |  9411 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 |  9412 | `	int iFlags = pAux->iFlags;` |
|        9 |  9413 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9414 | `	ph7_value *pObj;` |
|        - |  9415 | `	SyString sVar;` |
|        9 |  9416 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 |  9417 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 |  9418 | `	}` |
|        - |  9419 | `	/* Perform a string cast */` |
|        9 |  9420 | `	PH7_MemObjToString(pKey);` |
|        9 |  9421 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9422 | `		/* Unavailable variable name */` |
|      ! 0 |  9423 | `		return SXRET_OK;` |
|        - |  9424 | `	}` |
|        9 |  9425 | `	sVar.nByte = 0; /* cc warning */` |
|        9 |  9426 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 |  9427 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9428 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9429 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9430 | `			);` |
|      ! 0 |  9431 | `	}else{` |
|       13 |  9432 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 |  9433 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9434 | `	}` |
|        9 |  9435 | `	sVar.zString = pAux->zWorker;` |
|        - |  9436 | `	/* Try to extract the variable */` |
|        9 |  9437 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 |  9438 | `	if( pObj ){` |
|        - |  9439 | `		/* Collision */` |
|        5 |  9440 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 |  9441 | `			return SXRET_OK;` |
|        - |  9442 | `		}` |
|        5 |  9443 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 |  9444 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - |  9445 | `				/* Already prefixed */` |
|      ! 0 |  9446 | `				return SXRET_OK;` |
|        - |  9447 | `			}` |
|      ! 0 |  9448 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9449 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9450 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9451 | `				);` |
|      ! 0 |  9452 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 |  9453 | `		}` |
|        3 |  9454 | `	}else{` |
|        - |  9455 | `		/* Create the variable */` |
|        5 |  9456 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - |  9457 | `	}` |
|        9 |  9458 | `	if( pObj ){` |
|        - |  9459 | `		/* Overwrite the old value */` |
|        9 |  9460 | `		PH7_MemObjStore(pValue,pObj);` |
|        - |  9461 | `		/* Increment counter */` |
|        9 |  9462 | `		pAux->iCount++;` |
|        4 |  9463 | `	}` |
|        9 |  9464 | `	return SXRET_OK;` |
|        5 |  9465 |  |
|        - |  9466 | `/*` |
|        - |  9467 | ` * Worker callback for the [import_request_variables()] function` |
|        - |  9468 | ` * defined below.` |
|        - |  9469 | ` */` |
|        2 |  9470 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9471 |  |
|        3 |  9472 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 |  9473 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9474 | `	ph7_value *pObj;` |
|        - |  9475 | `	SyString sVar;` |
|        - |  9476 | `	/* Perform a string cast */` |
|        3 |  9477 | `	PH7_MemObjToString(pKey);` |
|        3 |  9478 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9479 | `		/* Unavailable variable name */` |
|      ! 0 |  9480 | `		return SXRET_OK;` |
|        - |  9481 | `	}` |
|        3 |  9482 | `	sVar.nByte = 0; /* cc warning */` |
|        3 |  9483 | `	if( pAux->Prefixlen > 0 ){` |
|        4 |  9484 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 |  9485 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 |  9486 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9487 | `			);` |
|        2 |  9488 | `	}else{` |
|      ! 0 |  9489 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 |  9490 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9491 | `	}` |
|        3 |  9492 | `	sVar.zString = pAux->zWorker;` |
|        - |  9493 | `	/* Extract the variable */` |
|        3 |  9494 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 |  9495 | `	if( pObj ){` |
|        3 |  9496 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 |  9497 | `	}` |
|        3 |  9498 | `	return SXRET_OK;` |
|        2 |  9499 |  |
|        - |  9500 | `/*` |
|        - |  9501 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - |  9502 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - |  9503 | ` * Parameters` |
|        - |  9504 | ` * $types` |
|        - |  9505 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - |  9506 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - |  9507 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - |  9508 | ` *  POST includes the POST uploaded file information.` |
|        - |  9509 | ` *  Note:` |
|        - |  9510 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - |  9511 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - |  9512 | ` * $prefix` |
|        - |  9513 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - |  9514 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - |  9515 | ` *  variable named $pref_userid.` |
|        - |  9516 | ` * Return` |
|        - |  9517 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9518 | ` */` |
|        2 |  9519 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9520 |  |
|        - |  9521 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - |  9522 | `	extract_aux_data sAux;` |
|        - |  9523 | `	int nLen,nPrefixLen;` |
|        - |  9524 | `	ph7_value *pSuper;` |
|        - |  9525 | `	ph7_vm *pVm;` |
|        - |  9526 | `	/* By default import only $_GET variables  */` |
|        3 |  9527 | `	zImport = "G";` |
|        3 |  9528 | `	nLen = (int)sizeof(char);` |
|        3 |  9529 | `	zPrefix = 0;` |
|        3 |  9530 | `	nPrefixLen = 0;` |
|        3 |  9531 | `	if( nArg > 0 ){` |
|        3 |  9532 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 |  9533 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 |  9534 | `		}` |
|        3 |  9535 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9536 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 |  9537 | `		}` |
|        1 |  9538 | `	}` |
|        - |  9539 | `	/* Point to the underlying VM */` |
|        3 |  9540 | `	pVm = pCtx->pVm;` |
|        - |  9541 | `	/* Initialize the aux data */` |
|        3 |  9542 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 |  9543 | `	sAux.zPrefix = zPrefix;` |
|        3 |  9544 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 |  9545 | `	sAux.pVm = pVm;` |
|        - |  9546 | `	/* Extract */` |
|        3 |  9547 | `	zEnd = &zImport[nLen];` |
|        5 |  9548 | `	while( zImport < zEnd ){` |
|        3 |  9549 | `		int c = zImport[0];` |
|        3 |  9550 | `		pSuper = 0;` |
|        3 |  9551 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - |  9552 | `			/* Import $_GET variables */` |
|        3 |  9553 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 |  9554 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - |  9555 | `			/* Import $_POST variables */` |
|      ! 0 |  9556 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 |  9557 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - |  9558 | `			/* Import $_COOKIE variables */` |
|      ! 0 |  9559 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 |  9560 | `		}` |
|        3 |  9561 | `		if( pSuper ){` |
|        - |  9562 | `			/* Iterate throw array entries */` |
|        3 |  9563 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 |  9564 | `		}` |
|        - |  9565 | `		/* Advance the cursor */` |
|        3 |  9566 | `		zImport++;` |
|        1 |  9567 | `	}` |
|        - |  9568 | `	/* All done,return TRUE*/` |
|        3 |  9569 | `	ph7_result_bool(pCtx,0);` |
|        3 |  9570 | `	return PH7_OK;` |
|        1 |  9571 |  |
|        - |  9572 | `/*` |
|        - |  9573 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |  9574 | ` * Refer to the eval() language construct implementation for more` |
|        - |  9575 | ` * information.` |
|        - |  9576 | ` */` |
|     9852 |  9577 | `static sxi32 VmEvalChunk(` |
|        - |  9578 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |  9579 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |  9580 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |  9581 | `	int iFlags,         /* Compile flag */` |
|        - |  9582 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |  9583 | `	)` |
|        2 |  9584 |  |
|        - |  9585 | `	SySet *pByteCode,aByteCode;` |
|     9854 |  9586 | `	ProcConsumer xErr = 0;` |
|     9854 |  9587 | `	void *pErrData = 0;` |
|        - |  9588 | `	/* Initialize bytecode container */` |
|     9854 |  9589 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     9854 |  9590 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |  9591 | `	/* Reset the code generator */` |
|     9854 |  9592 | `	if( bTrueReturn ){` |
|        - |  9593 | `		/* Included file,log compile-time errors */` |
|     7535 |  9594 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7535 |  9595 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3767 |  9596 | `	}` |
|     9854 |  9597 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |  9598 | `	/* Swap bytecode container */` |
|     9854 |  9599 | `	pByteCode = pVm->pByteContainer;` |
|     9854 |  9600 | `	pVm->pByteContainer = &aByteCode;` |
|        - |  9601 | `	/* Compile the chunk */` |
|     9854 |  9602 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    14780 |  9603 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |  9604 | `		/* Compilation error,return false */` |
|        3 |  9605 | `		if( pCtx ){` |
|        3 |  9606 | `			ph7_result_bool(pCtx,0);` |
|        1 |  9607 | `		}` |
|        2 |  9608 | `	}else{` |
|        - |  9609 | `		/* Mount any newly defined classes */` |
|        - |  9610 | `		SyHashEntry *pEntry;` |
|        - |  9611 | `		ph7_class *pClass;` |
|        - |  9612 | `		ph7_value sResult; /* Return value */` |
|        - |  9613 | `		sxi32 rc;` |
|     9852 |  9614 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   272997 |  9615 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   258222 |  9616 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  9617 | `			/* Only mount classes that haven't been mounted yet */` |
|   258222 |  9618 | `			if( !pClass->bMounted ){` |
|    59740 |  9619 | `				rc = VmMountUserClass(pVm,pClass);` |
|    59740 |  9620 | `				if( rc != SXRET_OK ){` |
|        - |  9621 | `					/* Mount failure (likely memory error) */` |
|      ! 0 |  9622 | `					if( pCtx ){` |
|      ! 0 |  9623 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 |  9624 | `					}` |
|      ! 0 |  9625 | `					goto Cleanup;` |
|        - |  9626 | `				}` |
|    29869 |  9627 | `			}` |
|        2 |  9628 | `		}` |
|     9852 |  9629 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  9630 | `			/* Out of memory */` |
|      ! 0 |  9631 | `			if( pCtx ){` |
|      ! 0 |  9632 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  9633 | `			}` |
|      ! 0 |  9634 | `			goto Cleanup;` |
|        - |  9635 | `		}` |
|     9852 |  9636 | `		if( bTrueReturn ){` |
|        - |  9637 | `			/* Assume a boolean true return value */` |
|     7535 |  9638 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3768 |  9639 | `		}else{` |
|        - |  9640 | `			/* Assume a null return value */` |
|     2318 |  9641 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  9642 | `		}` |
|        - |  9643 | `		/* Execute the compiled chunk */` |
|     9852 |  9644 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|     9852 |  9645 | `		if( pCtx ){` |
|        - |  9646 | `			/* Set the execution result */` |
|     7548 |  9647 | `			ph7_result_value(pCtx,&sResult);` |
|     3773 |  9648 | `		}` |
|     9852 |  9649 | `		PH7_MemObjRelease(&sResult);` |
|        - |  9650 | `	}` |
|     4926 |  9651 | `Cleanup:` |
|        - |  9652 | `	/* Cleanup the mess left behind */` |
|     9854 |  9653 | `	pVm->pByteContainer = pByteCode;` |
|     9854 |  9654 | `	SySetRelease(&aByteCode);` |
|     9854 |  9655 | `	return SXRET_OK;` |
|        2 |  9656 |  |
|        - |  9657 | `/*` |
|        - |  9658 | ` * value eval(string $code)` |
|        - |  9659 | ` *   Evaluate a string as PHP code.` |
|        - |  9660 | ` * Parameter` |
|        - |  9661 | ` *  code: PHP code to evaluate.` |
|        - |  9662 | ` * Return` |
|        - |  9663 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  9664 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  9665 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  9666 | ` */` |
|       16 |  9667 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9668 |  |
|        - |  9669 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 |  9670 | `	if( nArg < 1 ){` |
|        - |  9671 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9672 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9673 | `		return SXRET_OK;` |
|        - |  9674 | `	}` |
|        - |  9675 | `	/* Chunk to evaluate */` |
|       18 |  9676 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 |  9677 | `	if( sChunk.nByte < 1 ){` |
|        - |  9678 | `		/* Empty string,return NULL */` |
|        3 |  9679 | `		ph7_result_null(pCtx);` |
|        3 |  9680 | `		return SXRET_OK;` |
|        - |  9681 | `	}` |
|        - |  9682 | `	/* Eval the chunk */` |
|       16 |  9683 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 |  9684 | `	return SXRET_OK;` |
|       10 |  9685 |  |
|        - |  9686 | `/*` |
|        - |  9687 | ` * Check if a file path is already included.` |
|        - |  9688 | ` */` |
|    15064 |  9689 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 |  9690 |  |
|        - |  9691 | `	SyString *aEntries;` |
|        - |  9692 | `	sxu32 n;` |
|    15065 |  9693 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  9694 | `	/* Perform a linear search */` |
| 56720651 |  9695 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 56705593 |  9696 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  9697 | `			/* Already included */` |
|        7 |  9698 | `			return TRUE;` |
|        - |  9699 | `		}` |
| 28352794 |  9700 | `	}` |
|    15059 |  9701 | `	return FALSE;` |
|     7533 |  9702 |  |
|        - |  9703 | `/*` |
|        - |  9704 | ` * Push a file path in the appropriate VM container.` |
|        - |  9705 | ` */` |
|    17360 |  9706 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 |  9707 |  |
|        - |  9708 | `	SyString sPath;` |
|        - |  9709 | `	char *zDup;` |
|        - |  9710 | `#ifdef __WINNT__` |
|        - |  9711 | `	char *zCur;` |
|        - |  9712 | `#endif` |
|        - |  9713 | `	sxi32 rc;` |
|    17362 |  9714 | `	if( nLen < 0 ){` |
|     2298 |  9715 | `		nLen = SyStrlen(zPath);` |
|     1148 |  9716 | `	}` |
|        - |  9717 | `	/* Duplicate the file path first */` |
|    17362 |  9718 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    17362 |  9719 | `	if( zDup == 0 ){` |
|      ! 0 |  9720 | `		return SXERR_MEM;` |
|        - |  9721 | `	}` |
|        - |  9722 | `#ifdef __WINNT__` |
|        - |  9723 | `	/* Normalize path on windows` |
|        - |  9724 | `	 * Example:` |
|        - |  9725 | `	 *    Path/To/File.php` |
|        - |  9726 | `	 * becomes` |
|        - |  9727 | `	 *   path\to\file.php` |
|        - |  9728 | `	 */` |
|        2 |  9729 | `	zCur = zDup;` |
|        2 |  9730 | `	while( zCur[0] != 0 ){` |
|        2 |  9731 | `		if( zCur[0] == '/' ){` |
|        2 |  9732 | `			zCur[0] = '\\';` |
|        2 |  9733 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 |  9734 | `			int c = SyToLower(zCur[0]);` |
|        1 |  9735 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - |  9736 | `		}` |
|        2 |  9737 | `		zCur++;` |
|        2 |  9738 | `	}` |
|        - |  9739 | `#endif` |
|        - |  9740 | `	/* Install the file path */` |
|    17362 |  9741 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    17362 |  9742 | `	if( !bMain ){` |
|    15065 |  9743 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  9744 | `			/* Already included */` |
|        7 |  9745 | `			*pNew = 0;` |
|        4 |  9746 | `		}else{` |
|        - |  9747 | `			/* Insert in the corresponding container */` |
|    15059 |  9748 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    15059 |  9749 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9750 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  9751 | `				return rc;` |
|        - |  9752 | `			}` |
|    15059 |  9753 | `			*pNew = 1;` |
|        - |  9754 | `		}` |
|     7532 |  9755 | `	}` |
|    17362 |  9756 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    17362 |  9757 | `	return SXRET_OK;` |
|     8682 |  9758 |  |
|        - |  9759 | `/*` |
|        - |  9760 | ` * Compile and Execute a PHP script at run-time.` |
|        - |  9761 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - |  9762 | ` * indicates failure.` |
|        - |  9763 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - |  9764 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - |  9765 | ` * operations.` |
|        - |  9766 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - |  9767 | ` * this function is a no-op.` |
|        - |  9768 | ` * Refer to the implementation of the include(),include_once() language` |
|        - |  9769 | ` * constructs for more information.` |
|        - |  9770 | ` */` |
|     7540 |  9771 | `static sxi32 VmExecIncludedFile(` |
|        - |  9772 | `	 ph7_context *pCtx, /* Call Context */` |
|        - |  9773 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - |  9774 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - |  9775 | `	 )` |
|        2 |  9776 |  |
|        - |  9777 | `	sxi32 rc;` |
|        - |  9778 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9779 | `	const ph7_io_stream *pStream;` |
|        - |  9780 | `	SyBlob sContents;` |
|        - |  9781 | `	void *pHandle;` |
|        - |  9782 | `	ph7_vm *pVm;` |
|        - |  9783 | `	int isNew;` |
|        - |  9784 | `	/* Initialize fields */` |
|     7542 |  9785 | `	pVm = pCtx->pVm;` |
|     7542 |  9786 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7542 |  9787 | `	isNew = 0;` |
|        - |  9788 | `	/* Extract the associated stream */` |
|     7542 |  9789 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - |  9790 | `	/*` |
|        - |  9791 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - |  9792 | `	 * in a read-only mode.` |
|        - |  9793 | `	 */` |
|     7542 |  9794 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7542 |  9795 | `	if( pHandle == 0 ){` |
|        3 |  9796 | `		return SXERR_IO;` |
|        - |  9797 | `	}` |
|     7539 |  9798 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7539 |  9799 | `	if( IncludeOnce && !isNew ){` |
|        - |  9800 | `		/* Already included */` |
|        5 |  9801 | `		rc = SXERR_EXISTS;` |
|        3 |  9802 | `	}else{` |
|        - |  9803 | `		/* Read the whole file contents */` |
|     7535 |  9804 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7535 |  9805 | `		if( rc == SXRET_OK ){` |
|        - |  9806 | `			SyString sScript;` |
|        - |  9807 | `			/* Compile and execute the script */` |
|     7535 |  9808 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7535 |  9809 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3767 |  9810 | `		}` |
|        - |  9811 | `	}` |
|        - |  9812 | `	/* Pop from the set of included file */` |
|     7539 |  9813 | `	(void)SySetPop(&pVm->aFiles);` |
|        - |  9814 | `	/* Close the handle */` |
|     7539 |  9815 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - |  9816 | `	/* Release the working buffer */` |
|     7539 |  9817 | `	SyBlobRelease(&sContents);` |
|        - |  9818 | `#else` |
|        - |  9819 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - |  9820 | `	SXUNUSED(pPath);` |
|        - |  9821 | `	SXUNUSED(IncludeOnce);` |
|        - |  9822 | `	rc = SXERR_IO;` |
|        - |  9823 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7539 |  9824 | `	return rc;` |
|     3772 |  9825 |  |
|        - |  9826 | `/*` |
|        - |  9827 | ` * string get_include_path(void)` |
|        - |  9828 | ` *  Gets the current include_path configuration option.` |
|        - |  9829 | ` * Parameter` |
|        - |  9830 | ` *  None` |
|        - |  9831 | ` * Return` |
|        - |  9832 | ` *  Included paths as a string` |
|        - |  9833 | ` */` |
|        2 |  9834 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9835 |  |
|        3 |  9836 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9837 | `	SyString *aEntry;` |
|        - |  9838 | `	int dir_sep;` |
|        - |  9839 | `	sxu32 n;` |
|        - |  9840 | `#ifdef __WINNT__` |
|        1 |  9841 | `	dir_sep = ';';` |
|        - |  9842 | `#else` |
|        - |  9843 | `	/* Assume UNIX path separator */` |
|        2 |  9844 | `	dir_sep = ':';` |
|        - |  9845 | `#endif` |
|        1 |  9846 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9847 | `	SXUNUSED(apArg);` |
|        - |  9848 | `	/* Point to the list of import paths */` |
|        3 |  9849 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 |  9850 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 |  9851 | `		SyString *pEntry = &aEntry[n];` |
|        3 |  9852 | `		if( n > 0 ){` |
|        - |  9853 | `			/* Append dir seprator */` |
|      ! 0 |  9854 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 |  9855 | `		}` |
|        - |  9856 | `		/* Append path */` |
|        3 |  9857 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 |  9858 | `	}` |
|        3 |  9859 | `	return PH7_OK;` |
|        1 |  9860 |  |
|        - |  9861 | `/*` |
|        - |  9862 | ` * string get_get_included_files(void)` |
|        - |  9863 | ` *  Gets the current include_path configuration option.` |
|        - |  9864 | ` * Parameter` |
|        - |  9865 | ` *  None` |
|        - |  9866 | ` * Return` |
|        - |  9867 | ` *  Included paths as a string` |
|        - |  9868 | ` */` |
|        2 |  9869 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9870 |  |
|        3 |  9871 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - |  9872 | `	ph7_value *pArray,*pWorker;` |
|        - |  9873 | `	SyString *pEntry;` |
|        - |  9874 | `	int c,d;` |
|        - |  9875 | `	/* Create an array and a working value */` |
|        3 |  9876 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 |  9877 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 |  9878 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - |  9879 | `		/* Out of memory,return null */` |
|      ! 0 |  9880 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9881 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9882 | `		SXUNUSED(apArg);` |
|      ! 0 |  9883 | `		return PH7_OK;` |
|        - |  9884 | `	}` |
|        3 |  9885 | `	c = d = '/';` |
|        - |  9886 | `#ifdef __WINNT__` |
|        1 |  9887 | `	d = '\\';` |
|        - |  9888 | `#endif` |
|        - |  9889 | `	/* Iterate throw entries */` |
|        3 |  9890 | `	SySetResetCursor(pFiles);` |
|     3691 |  9891 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - |  9892 | `		const char *zBase,*zEnd;` |
|        - |  9893 | `		int iLen;` |
|        - |  9894 | `		/* reset the string cursor */` |
|     3689 |  9895 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - |  9896 | `		/* Extract base name */` |
|     3689 |  9897 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - |  9898 | `		/* Ignore trailing '/' */` |
|     5533 |  9899 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 |  9900 | `			zEnd--;` |
|      ! 0 |  9901 | `		}` |
|     3689 |  9902 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   113825 |  9903 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   108293 |  9904 | `			zEnd--;` |
|        1 |  9905 | `		}` |
|     3689 |  9906 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3689 |  9907 | `		zEnd = &pEntry->zString[iLen];` |
|        - |  9908 | `		/* Copy entry name */` |
|     3689 |  9909 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - |  9910 | `		/* Perform the insertion */` |
|     3689 |  9911 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 |  9912 | `	}` |
|        - |  9913 | `	/* All done,return the created array */` |
|        3 |  9914 | `	ph7_result_value(pCtx,pArray);` |
|        - |  9915 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - |  9916 | `	 * by the engine as soon we return from this foreign` |
|        - |  9917 | `	 * function.` |
|        - |  9918 | `	 */` |
|        3 |  9919 | `	return PH7_OK;` |
|        2 |  9920 |  |
|        - |  9921 | `/*` |
|        - |  9922 | ` * include:` |
|        - |  9923 | ` * According to the PHP reference manual.` |
|        - |  9924 | ` *  The include() function includes and evaluates the specified file.` |
|        - |  9925 | ` *  Files are included based on the file path given or, if none is given` |
|        - |  9926 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - |  9927 | ` *  include() will finally check in the calling script's own directory` |
|        - |  9928 | ` *  and the current working directory before failing. The include()` |
|        - |  9929 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - |  9930 | ` *  behavior from require(), which will emit a fatal error.` |
|        - |  9931 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - |  9932 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - |  9933 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - |  9934 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - |  9935 | ` *  directory to find the requested file.` |
|        - |  9936 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - |  9937 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - |  9938 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - |  9939 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - |  9940 | ` */` |
|     7528 |  9941 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9942 |  |
|        - |  9943 | `	SyString sFile;` |
|        - |  9944 | `	sxi32 rc;` |
|     7530 |  9945 | `	if( nArg < 1 ){` |
|        - |  9946 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9947 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9948 | `		return SXRET_OK;` |
|        - |  9949 | `	}` |
|        - |  9950 | `	/* File to include */` |
|     7530 |  9951 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7530 |  9952 | `	if( sFile.nByte < 1 ){` |
|        - |  9953 | `		/* Empty string,return NULL */` |
|      ! 0 |  9954 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9955 | `		return SXRET_OK;` |
|        - |  9956 | `	}` |
|        - |  9957 | `	/* Open,compile and execute the desired script */` |
|     7530 |  9958 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7530 |  9959 | `	if( rc != SXRET_OK ){` |
|        - |  9960 | `		/* Emit a warning and return false */` |
|        3 |  9961 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 |  9962 | `		ph7_result_bool(pCtx,0);` |
|        1 |  9963 | `	}` |
|     7530 |  9964 | `	return SXRET_OK;` |
|     3766 |  9965 |  |
|        - |  9966 | `/*` |
|        - |  9967 | ` * include_once:` |
|        - |  9968 | ` *  According to the PHP reference manual.` |
|        - |  9969 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - |  9970 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - |  9971 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - |  9972 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - |  9973 | ` *   just once.` |
|        - |  9974 | ` */` |
|        4 |  9975 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9976 |  |
|        - |  9977 | `	SyString sFile;` |
|        - |  9978 | `	sxi32 rc;` |
|        5 |  9979 | `	if( nArg < 1 ){` |
|        - |  9980 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9981 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9982 | `		return SXRET_OK;` |
|        - |  9983 | `	}` |
|        - |  9984 | `	/* File to include */` |
|        5 |  9985 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 |  9986 | `	if( sFile.nByte < 1 ){` |
|        - |  9987 | `		/* Empty string,return NULL */` |
|      ! 0 |  9988 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9989 | `		return SXRET_OK;` |
|        - |  9990 | `	}` |
|        - |  9991 | `	/* Open,compile and execute the desired script */` |
|        5 |  9992 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 |  9993 | `	if( rc == SXERR_EXISTS ){` |
|        - |  9994 | `		/* File already included,return TRUE */` |
|        3 |  9995 | `		ph7_result_bool(pCtx,1);` |
|        3 |  9996 | `		return SXRET_OK;` |
|        - |  9997 | `	}` |
|        3 |  9998 | `	if( rc != SXRET_OK ){` |
|        - |  9999 | `		/* Emit a warning and return false */` |
|      ! 0 | 10000 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10001 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10002 | ` 	}` |
|        3 | 10003 | `	return SXRET_OK;` |
|        3 | 10004 |  |
|        - | 10005 | `/*` |
|        - | 10006 | ` * require.` |
|        - | 10007 | ` *  According to the PHP reference manual.` |
|        - | 10008 | ` *   require() is identical to include() except upon failure it will` |
|        - | 10009 | ` *   also produce a fatal level error.` |
|        - | 10010 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 10011 | ` *   emits a warning  which allows the script to continue.` |
|        - | 10012 | ` */` |
|        4 | 10013 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10014 |  |
|        - | 10015 | `	SyString sFile;` |
|        - | 10016 | `	sxi32 rc;` |
|        5 | 10017 | `	if( nArg < 1 ){` |
|        - | 10018 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10019 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10020 | `		return SXRET_OK;` |
|        - | 10021 | `	}` |
|        - | 10022 | `	/* File to include */` |
|        5 | 10023 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10024 | `	if( sFile.nByte < 1 ){` |
|        - | 10025 | `		/* Empty string,return NULL */` |
|      ! 0 | 10026 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10027 | `		return SXRET_OK;` |
|        - | 10028 | `	}` |
|        - | 10029 | `	/* Open,compile and execute the desired script */` |
|        5 | 10030 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 10031 | `	if( rc != SXRET_OK ){` |
|        - | 10032 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10033 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10034 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10035 | `		return PH7_ABORT;` |
|        - | 10036 | `	}` |
|        5 | 10037 | `	return SXRET_OK;` |
|        3 | 10038 |  |
|        - | 10039 | `/*` |
|        - | 10040 | ` * require_once:` |
|        - | 10041 | ` *  According to the PHP reference manual.` |
|        - | 10042 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 10043 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 10044 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 10045 | ` *   and how it differs from its non _once siblings.` |
|        - | 10046 | ` */` |
|        4 | 10047 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10048 |  |
|        - | 10049 | `	SyString sFile;` |
|        - | 10050 | `	sxi32 rc;` |
|        5 | 10051 | `	if( nArg < 1 ){` |
|        - | 10052 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10053 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10054 | `		return SXRET_OK;` |
|        - | 10055 | `	}` |
|        - | 10056 | `	/* File to include */` |
|        5 | 10057 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10058 | `	if( sFile.nByte < 1 ){` |
|        - | 10059 | `		/* Empty string,return NULL */` |
|      ! 0 | 10060 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10061 | `		return SXRET_OK;` |
|        - | 10062 | `	}` |
|        - | 10063 | `	/* Open,compile and execute the desired script */` |
|        5 | 10064 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 10065 | `	if( rc == SXERR_EXISTS ){` |
|        - | 10066 | `		/* File already included,return TRUE */` |
|        3 | 10067 | `		ph7_result_bool(pCtx,1);` |
|        3 | 10068 | `		return SXRET_OK;` |
|        - | 10069 | `	}` |
|        3 | 10070 | `	if( rc != SXRET_OK ){` |
|        - | 10071 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10072 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10073 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10074 | `		return PH7_ABORT;` |
|        - | 10075 | `	}` |
|        3 | 10076 | `	return SXRET_OK;` |
|        3 | 10077 |  |
|        - | 10078 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 10079 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 10080 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 10081 | `/* Table of built-in VM functions. */` |
|        - | 10082 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 10083 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 10084 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 10085 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 10086 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 10087 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 10088 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 10089 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 10090 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 10091 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 10092 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 10093 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 10094 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 10095 | `	    /* Constants management */` |
|        - | 10096 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 10097 | `	{ "define",   vm_builtin_define               },` |
|        - | 10098 | `	{ "constant", vm_builtin_constant             },` |
|        - | 10099 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 10100 | `	   /* Class/Object functions */` |
|        - | 10101 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 10102 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 10103 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 10104 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 10105 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 10106 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 10107 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 10108 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 10109 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 10110 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 10111 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 10112 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 10113 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 10114 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 10115 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 10116 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 10117 | `	   /* Random numbers/strings generators */` |
|        - | 10118 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 10119 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 10120 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 10121 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 10122 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 10123 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10124 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 10125 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 10126 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 10127 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10128 | `	   /* Language constructs functions */` |
|        - | 10129 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 10130 | `	{ "print", vm_builtin_print                   },` |
|        - | 10131 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 10132 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 10133 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 10134 | `	  /* Variable handling functions */` |
|        - | 10135 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 10136 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 10137 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 10138 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 10139 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 10140 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 10141 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 10142 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 10143 | `	  /* Ouput control functions */` |
|        - | 10144 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 10145 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 10146 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 10147 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 10148 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 10149 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 10150 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 10151 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 10152 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 10153 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 10154 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 10155 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 10156 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 10157 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 10158 | `	  /* Assertion functions */` |
|        - | 10159 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 10160 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 10161 | `	  /* Error reporting functions */` |
|        - | 10162 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 10163 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 10164 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 10165 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 10166 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 10167 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 10168 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 10169 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 10170 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 10171 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 10172 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 10173 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 10174 | `	  /* Release info */` |
|        - | 10175 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 10176 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 10177 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 10178 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 10179 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 10180 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 10181 | `	  /* hashmap */` |
|        - | 10182 | `	{"compact",          vm_builtin_compact       },` |
|        - | 10183 | `	{"extract",          vm_builtin_extract       },` |
|        - | 10184 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 10185 | `	  /* URL related function */` |
|        - | 10186 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 10187 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 10188 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10189 | `	   /* XML processing functions */` |
|        - | 10190 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 10191 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 10192 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 10193 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 10194 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 10195 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 10196 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 10197 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 10198 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 10199 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 10200 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 10201 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 10202 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 10203 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 10204 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 10205 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 10206 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 10207 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 10208 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 10209 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 10210 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 10211 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10212 | `	   /* UTF-8 encoding/decoding */` |
|        - | 10213 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 10214 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 10215 | `	   /* Command line processing */` |
|        - | 10216 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 10217 | `	   /* JSON encoding/decoding */` |
|        - | 10218 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 10219 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 10220 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 10221 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 10222 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 10223 | `	   /* Files/URI inclusion facility */` |
|        - | 10224 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 10225 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 10226 | `	{ "include",      vm_builtin_include          },` |
|        - | 10227 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 10228 | `	{ "require",      vm_builtin_require          },` |
|        - | 10229 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 10230 | `};` |
|        - | 10231 | `/*` |
|        - | 10232 | ` * Register the built-in VM functions defined above.` |
|        - | 10233 | ` */` |
|     2072 | 10234 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 10235 |  |
|        - | 10236 | `	sxi32 rc;` |
|        - | 10237 | `	sxu32 n;` |
|   259002 | 10238 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 10239 | `		/* Note that these special functions have access` |
|        - | 10240 | `		 * to the underlying virtual machine as their` |
|        - | 10241 | `		 * private data.` |
|        - | 10242 | `		 */` |
|   256930 | 10243 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   256930 | 10244 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 10245 | `			return rc;` |
|        - | 10246 | `		}` |
|   128466 | 10247 | `	}` |
|     2074 | 10248 | `	return SXRET_OK;` |
|     1038 | 10249 |  |
|        - | 10250 | `/*` |
|        - | 10251 | ` * Check if the given name refer to an installed class.` |
|        - | 10252 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 10253 | ` */` |
|    15062 | 10254 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 10255 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 10256 | `	const char *zName,  /* Name of the target class */` |
|        - | 10257 | `	sxu32 nByte,        /* zName length */` |
|        - | 10258 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 10259 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 10260 | `						 */` |
|        - | 10261 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 10262 | `	)` |
|        2 | 10263 |  |
|        - | 10264 | `	SyHashEntry *pEntry;` |
|        - | 10265 | `	ph7_class *pClass;` |
|        - | 10266 | `	SyString sName;` |
|     7531 | 10267 | `	SXUNUSED(iNest);` |
|        - | 10268 | `	/* Namespace-aware class lookup */` |
|    15064 | 10269 | `	SyStringInitFromBuf(&sName,zName,nByte);` |
|    15064 | 10270 | `	pEntry = VmNsAwareHashLookup(pVm,&pVm->hClass,&sName);` |
|    15064 | 10271 | `	if( pEntry == 0 ){` |
|      ! 0 | 10272 | `		return 0;` |
|        - | 10273 | `	}` |
|    15064 | 10274 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    15064 | 10275 | `	if( !iLoadable ){` |
|    14008 | 10276 | `		return pClass;` |
|        - | 10277 | `	}` |
|        - | 10278 | `	/* Filter for loadable classes (skip interfaces/abstract) */` |
|     1058 | 10279 | `	while(pClass){` |
|     1058 | 10280 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) == 0 ){` |
|     1058 | 10281 | `			return pClass;` |
|        - | 10282 | `		}` |
|      ! 0 | 10283 | `		pClass = pClass->pNextName;` |
|      ! 0 | 10284 | `	}` |
|      ! 0 | 10285 | `	return 0;` |
|     7533 | 10286 |  |
|        - | 10287 | `/*` |
|        - | 10288 | ` * Reference Table Implementation` |
|        - | 10289 | ` * Status: stable <chm@symisc.net>` |
|        - | 10290 | ` * Intro` |
|        - | 10291 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 10292 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 10293 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 10294 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 10295 | ` *  Refer to the official for more information on this powerful` |
|        - | 10296 | ` *  extension.` |
|        - | 10297 | ` */` |
|        - | 10298 | `/*` |
|        - | 10299 | ` * Allocate a new reference entry.` |
|        - | 10300 | ` */` |
|  2983494 | 10301 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 10302 |  |
|        - | 10303 | `	VmRefObj *pRef;` |
|        - | 10304 | `	/* Allocate a new instance */` |
|  2983496 | 10305 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  2983496 | 10306 | `	if( pRef == 0 ){` |
|      ! 0 | 10307 | `		return 0;` |
|        - | 10308 | `	}` |
|        - | 10309 | `	/* Zero the structure */` |
|  2983496 | 10310 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 10311 | `	/* Initialize fields */` |
|  2983496 | 10312 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  2983496 | 10313 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  2983496 | 10314 | `	pRef->nIdx = nIdx;` |
|  2983496 | 10315 | `	return pRef;` |
|  1491749 | 10316 |  |
|        - | 10317 | `/*` |
|        - | 10318 | ` * Default hash function used by the reference table` |
|        - | 10319 | ` * for lookup/insertion operations.` |
|        - | 10320 | ` */` |
| 16562885 | 10321 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 10322 |  |
|        - | 10323 | `	/* Calculate the hash based on the memory object index */` |
| 16562887 | 10324 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 10325 |  |
|        - | 10326 | `/*` |
|        - | 10327 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 10328 | ` * in the reference table.` |
|        - | 10329 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 10330 | ` * otherwise.` |
|        - | 10331 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10332 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10333 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10334 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10335 | ` * Refer to the official for more information on this powerful` |
|        - | 10336 | ` * extension.` |
|        - | 10337 | ` */` |
|  8909210 | 10338 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 10339 |  |
|        - | 10340 | `	VmRefObj *pRef;` |
|        - | 10341 | `	sxu32 nBucket;` |
|        - | 10342 | `	/* Point to the appropriate bucket */` |
|  8909212 | 10343 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 10344 | `	/* Perform the lookup */` |
|  8909212 | 10345 | `	pRef = pVm->apRefObj[nBucket];` |
| 18803750 | 10346 | `	for(;;){` |
| 37601777 | 10347 | `		if( pRef == 0 ){` |
|  3056658 | 10348 | `			break;` |
|        - | 10349 | `		}` |
| 34545121 | 10350 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 10351 | `			/* Entry found */` |
|  5852556 | 10352 | `			return pRef;` |
|        - | 10353 | `		}` |
|        - | 10354 | `		/* Point to the next entry */` |
| 28692567 | 10355 | `		pRef = pRef->pNextCollide;` |
|        2 | 10356 | `	}` |
|        - | 10357 | `	/* No such entry,return NULL */` |
|  3056658 | 10358 | `	return 0;` |
|  4454607 | 10359 |  |
|        - | 10360 | `/*` |
|        - | 10361 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10362 | ` *` |
|        - | 10363 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10364 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10365 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10366 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10367 | ` * Refer to the official for more information on this powerful` |
|        - | 10368 | ` * extension.` |
|        - | 10369 | ` */` |
|  2983494 | 10370 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10371 |  |
|        - | 10372 | `	sxu32 nBucket;` |
|  2983496 | 10373 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 10374 | `		VmRefObj **apNew;` |
|        - | 10375 | `		sxu32 nNew;` |
|        - | 10376 | `		/* Allocate a larger table */` |
|     3258 | 10377 | `		nNew = pVm->nRefSize << 1;` |
|     3258 | 10378 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     3258 | 10379 | `		if( apNew ){` |
|     3258 | 10380 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 10381 | `			sxu32 n;` |
|        - | 10382 | `			/* Zero the structure */` |
|     3258 | 10383 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 10384 | `			/* Rehash all referenced entries */` |
|  2832808 | 10385 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 10386 | `				/* Remove old collision links */` |
|  2829552 | 10387 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 10388 | `				/* Point to the appropriate bucket */` |
|  2829552 | 10389 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 10390 | `				/* Insert the entry  */` |
|  2829552 | 10391 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2829552 | 10392 | `				if( apNew[nBucket] ){` |
|  2298896 | 10393 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 10394 | `				}` |
|  2829552 | 10395 | `				apNew[nBucket] = pEntry;` |
|        - | 10396 | `				/* Point to the next entry */` |
|  2829552 | 10397 | `				pEntry = pEntry->pNext;` |
|  1414777 | 10398 | `			}` |
|        - | 10399 | `			/* Release the old table */` |
|     3258 | 10400 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 10401 | `			/* Install the new one */` |
|     3258 | 10402 | `			pVm->apRefObj = apNew;` |
|     3258 | 10403 | `			pVm->nRefSize = nNew;` |
|     1628 | 10404 | `		}` |
|     1628 | 10405 | `	}` |
|        - | 10406 | `	/* Point to the appropriate bucket */` |
|  2983496 | 10407 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 10408 | `	/* Insert the entry */` |
|  2983496 | 10409 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  2983496 | 10410 | `	if( pVm->apRefObj[nBucket] ){` |
|  2476506 | 10411 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1238255 | 10412 | `	}` |
|  2983496 | 10413 | `	pVm->apRefObj[nBucket] = pRef;` |
|  2983496 | 10414 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  2983496 | 10415 | `	pVm->nRefUsed++;` |
|  2983496 | 10416 | `	return SXRET_OK;` |
|        2 | 10417 |  |
|        - | 10418 | `/*` |
|        - | 10419 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 10420 | ` * the reference table.` |
|        - | 10421 | ` * This function is invoked when the user perform an unset` |
|        - | 10422 | ` * call [i.e: unset($var); ].` |
|        - | 10423 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10424 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10425 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10426 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10427 | ` * Refer to the official for more information on this powerful` |
|        - | 10428 | ` * extension.` |
|        - | 10429 | ` */` |
|  2953914 | 10430 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10431 |  |
|        - | 10432 | `	ph7_hashmap_node **apNode;` |
|        - | 10433 | `	SyHashEntry **apEntry;` |
|        - | 10434 | `	sxu32 n;` |
|        - | 10435 | `	/* Point to the reference table */` |
|  2953916 | 10436 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2953916 | 10437 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 10438 | `	/* Unlink the entry from the reference table */` |
|  3032040 | 10439 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    78126 | 10440 | `		if( apEntry[n] ){` |
|    78076 | 10441 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    39037 | 10442 | `		}` |
|    39064 | 10443 | `	}` |
|  5831606 | 10444 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2877692 | 10445 | `		if( apNode[n] ){` |
|     5635 | 10446 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2817 | 10447 | `		}` |
|  1438847 | 10448 | `	}` |
|  2953916 | 10449 | `	if( pRef->pPrevCollide ){` |
|  1113285 | 10450 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   556888 | 10451 | `	}else{` |
|  1840633 | 10452 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 10453 | `	}` |
|  2953916 | 10454 | `	if( pRef->pNextCollide ){` |
|  1665122 | 10455 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   832563 | 10456 | `	}` |
|  2953916 | 10457 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 10458 | `	/* Release the node */` |
|  2953916 | 10459 | `	SySetRelease(&pRef->aReference);` |
|  2953916 | 10460 | `	SySetRelease(&pRef->aArrEntries);` |
|  2953916 | 10461 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2953916 | 10462 | `	pVm->nRefUsed--;` |
|  2953916 | 10463 | `	return SXRET_OK;` |
|        2 | 10464 |  |
|        - | 10465 | `/*` |
|        - | 10466 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10467 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10468 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10469 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10470 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10471 | ` * Refer to the official for more information on this powerful` |
|        - | 10472 | ` * extension.` |
|        - | 10473 | ` */` |
|  3010018 | 10474 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 10475 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10476 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10477 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10478 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 10479 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 10480 | `	)` |
|        2 | 10481 |  |
|  3010020 | 10482 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10483 | `	VmRefObj *pRef;` |
|        - | 10484 | `	/* Check if the referenced object already exists */` |
|  3010020 | 10485 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3010020 | 10486 | `	if( pRef == 0 ){` |
|        - | 10487 | `		/* Create a new entry */` |
|  2983496 | 10488 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  2983496 | 10489 | `		if( pRef == 0 ){` |
|      ! 0 | 10490 | `			return SXERR_MEM;` |
|        - | 10491 | `		}` |
|  2983496 | 10492 | `		pRef->iFlags = iFlags;` |
|        - | 10493 | `		/* Install the entry */` |
|  2983496 | 10494 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1491747 | 10495 | `	}` |
|  3010096 | 10496 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 10497 | `		/* Safely ignore the exception frame */` |
|       78 | 10498 | `		pFrame = pFrame->pParent;` |
|        2 | 10499 | `	}` |
|  3010020 | 10500 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 10501 | `		VmSlot sRef;` |
|        - | 10502 | `		/* Local frame,record referenced entry so that it can` |
|        - | 10503 | `		 * be deleted when we leave this frame.` |
|        - | 10504 | `		 */` |
|    73198 | 10505 | `		sRef.nIdx = nIdx;` |
|    73198 | 10506 | `		sRef.pUserData = pEntry;` |
|    73198 | 10507 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 10508 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 10509 | `		}` |
|    36598 | 10510 | `	}` |
|  3010020 | 10511 | `	if( pEntry ){` |
|        - | 10512 | `		/* Address of the hash-entry */` |
|    99532 | 10513 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    49765 | 10514 | `	}` |
|  3010020 | 10515 | `	if( pMapEntry ){` |
|        - | 10516 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2905726 | 10517 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1452862 | 10518 | `	}` |
|  3010020 | 10519 | `	return SXRET_OK;` |
|  1505011 | 10520 |  |
|        - | 10521 | `/*` |
|        - | 10522 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 10523 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10524 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10525 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10526 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10527 | ` * Refer to the official for more information on this powerful` |
|        - | 10528 | ` * extension.` |
|        - | 10529 | ` */` |
|  2945258 | 10530 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 10531 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10532 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10533 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10534 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 10535 | `	)` |
|        2 | 10536 |  |
|        - | 10537 | `	VmRefObj *pRef;` |
|        - | 10538 | `	sxu32 n;` |
|        - | 10539 | `	/* Check if the referenced object already exists */` |
|  2945260 | 10540 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2945260 | 10541 | `	if( pRef == 0 ){` |
|        - | 10542 | `		/* Not such entry */` |
|    73144 | 10543 | `		return SXERR_NOTFOUND;` |
|        - | 10544 | `	}` |
|        - | 10545 | `	/* Remove the desired entry */` |
|  2872118 | 10546 | `	if( pEntry ){` |
|        - | 10547 | `		SyHashEntry **apEntry;` |
|       56 | 10548 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 10549 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 10550 | `			if( apEntry[n] == pEntry ){` |
|        - | 10551 | `				/* Nullify the entry */` |
|       56 | 10552 | `				apEntry[n] = 0;` |
|        - | 10553 | `				/*` |
|        - | 10554 | `				 * NOTE:` |
|        - | 10555 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 10556 | `				 * we avoid wasting spaces.` |
|        - | 10557 | `				 */` |
|       27 | 10558 | `			}` |
|       79 | 10559 | `		}` |
|       27 | 10560 | `	}` |
|  2872118 | 10561 | `	if( pMapEntry ){` |
|        - | 10562 | `		ph7_hashmap_node **apNode;` |
|  2872064 | 10563 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5744214 | 10564 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2872152 | 10565 | `			if( apNode[n] == pMapEntry ){` |
|        - | 10566 | `				/* nullify the entry */` |
|  2872064 | 10567 | `				apNode[n] = 0;` |
|  1436031 | 10568 | `			}` |
|  1436077 | 10569 | `		}` |
|  1436031 | 10570 | `	}` |
|  2872118 | 10571 | `	return SXRET_OK;` |
|  1472631 | 10572 |  |
|        - | 10573 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 10574 | `/*` |
|        - | 10575 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 10576 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 10577 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 10578 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 10579 | ` * For more information on how to register IO stream devices,please` |
|        - | 10580 | ` * refer to the official documentation.` |
|        - | 10581 | ` */` |
|    22718 | 10582 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 10583 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 10584 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 10585 | `	int nByte              /* *pzDevice length*/` |
|        - | 10586 | `	)` |
|        2 | 10587 |  |
|        - | 10588 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 10589 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 10590 | `	SyString sDev,sCur;` |
|        - | 10591 | `	sxu32 n,nEntry;` |
|        - | 10592 | `	int rc;` |
|        - | 10593 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    22720 | 10594 | `	zNext = zCur = zIn = *pzDevice;` |
|    22720 | 10595 | `	zEnd = &zIn[nByte];` |
|  1454688 | 10596 | `	while( zIn < zEnd ){` |
|  1431972 | 10597 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 10598 | `			/* Got one */` |
|        3 | 10599 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 10600 | `			break;` |
|        - | 10601 | `		}` |
|        - | 10602 | `		/* Advance the cursor */` |
|  1431970 | 10603 | `		zIn++;` |
|        2 | 10604 | `	}` |
|    22720 | 10605 | `	if( zIn >= zEnd ){` |
|        - | 10606 | `		/* No such scheme,return the default stream */` |
|    22718 | 10607 | `		return pVm->pDefStream;` |
|        - | 10608 | `	}` |
|        3 | 10609 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 10610 | `	/* Remove leading and trailing white spaces */` |
|        3 | 10611 | `	SyStringFullTrim(&sDev);` |
|        - | 10612 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 10613 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 10614 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 10615 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 10616 | `		pStream = apStream[n];` |
|        3 | 10617 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 10618 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 10619 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 10620 | `		if( rc == 0 ){` |
|        - | 10621 | `			/* Stream device found */` |
|        3 | 10622 | `			*pzDevice = zNext;` |
|        3 | 10623 | `			return pStream;` |
|        - | 10624 | `		}` |
|      ! 0 | 10625 | `	}` |
|        - | 10626 | `	/* No such stream,return NULL */` |
|      ! 0 | 10627 | `	return 0;` |
|    11361 | 10628 |  |
|        - | 10629 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 10630 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 10631 |  |
