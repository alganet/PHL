# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4461/5954 lines (74.92%)

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
|        - |    34 | `/*` |
|        - |    35 | ` * Each active virtual machine frame is represented by an instance` |
|        - |    36 | ` * of the following structure.` |
|        - |    37 | ` * VM Frame hold local variables and other stuff related to function call.` |
|        - |    38 | ` */` |
|        - |    39 | `struct VmFrame` |
|        - |    40 |  |
|        - |    41 | `	VmFrame *pParent; /* Parent frame or NULL if global scope */` |
|        - |    42 | `	void *pUserData;  /* Upper layer private data associated with this frame */` |
|        - |    43 | `	ph7_class_instance *pThis; /* Current class instance [i.e: the '$this' variable].NULL otherwise */` |
|        - |    44 | `	SySet sLocal;     /* Local variables container (VmSlot instance) */` |
|        - |    45 | `	ph7_vm *pVm;      /* VM that own this frame */` |
|        - |    46 | `	SyHash hVar;      /* Variable hashtable for fast lookup */` |
|        - |    47 | `	SySet sArg;       /* Function arguments container */` |
|        - |    48 | `	SySet sRef;       /* Local reference table (VmSlot instance) */` |
|        - |    49 | `	sxi32 iFlags;     /* Frame configuration flags (See below)*/` |
|        - |    50 | `	sxu32 iExceptionJump; /* Exception jump destination */` |
|        - |    51 | `};` |
|        - |    52 | `#define VM_FRAME_EXCEPTION  0x01 /* Special Exception frame */` |
|        - |    53 | `#define VM_FRAME_THROW      0x02 /* An exception was thrown */` |
|        - |    54 | `#define VM_FRAME_CATCH      0x04 /* Catch frame */` |
|        - |    55 | `/*` |
|        - |    56 | ` * When a user defined variable is released (via manual unset($x) or garbage collected)` |
|        - |    57 | ` * memory object index is stored in an instance of the following structure and put` |
|        - |    58 | ` * in the free object table so that it can be reused again without allocating` |
|        - |    59 | ` * a new memory object.` |
|        - |    60 | ` */` |
|        - |    61 | `typedef struct VmSlot VmSlot;` |
|        - |    62 | `struct VmSlot` |
|        - |    63 |  |
|        - |    64 | `	sxu32 nIdx;      /* Index in pVm->aMemObj[] */` |
|        - |    65 | `	void *pUserData; /* Upper-layer private data */` |
|        - |    66 | `};` |
|        - |    67 | `/*` |
|        - |    68 | ` * An entry in the reference table is represented by an instance of the` |
|        - |    69 | ` * follwoing table.` |
|        - |    70 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - |    71 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - |    72 | ` * the reference implementation is consistent,solid and it's` |
|        - |    73 | ` * behavior resemble the C++ reference mechanism.` |
|        - |    74 | ` * Refer to the official for more information on this powerful` |
|        - |    75 | ` * extension.` |
|        - |    76 | ` */` |
|        - |    77 | `struct VmRefObj` |
|        - |    78 |  |
|        - |    79 | `	SySet aReference;  /* Table of references to this memory object */` |
|        - |    80 | `	SySet aArrEntries; /* Foreign hashmap entries [i.e: array(&$a) ] */` |
|        - |    81 | `	sxu32 nIdx;        /* Referenced object index */` |
|        - |    82 | `	sxi32 iFlags;      /* Configuration flags */` |
|        - |    83 | `	VmRefObj *pNextCollide,*pPrevCollide; /* Collision link */` |
|        - |    84 | `	VmRefObj *pNext,*pPrev;               /* List of all referenced objects */` |
|        - |    85 | `};` |
|        - |    86 | `#define VM_REF_IDX_KEEP  0x001 /* Do not restore the memory object to the free list */` |
|        - |    87 | `/*` |
|        - |    88 | ` * Output control buffer entry.` |
|        - |    89 | ` * Refer to the implementation of [ob_start()] for more information.` |
|        - |    90 | ` */` |
|        - |    91 | `typedef struct VmObEntry VmObEntry;` |
|        - |    92 | `struct VmObEntry` |
|        - |    93 |  |
|        - |    94 | `	ph7_value sCallback; /* User defined callback */` |
|        - |    95 | `	SyBlob sOB;          /* Output buffer consumer */` |
|        - |    96 | `};` |
|        - |    97 | `/*` |
|        - |    98 | ` * Each installed shutdown callback (registered using [register_shutdown_function()] )` |
|        - |    99 | ` * is stored in an instance of the following structure.` |
|        - |   100 | ` * Refer to the implementation of [register_shutdown_function(()] for more information.` |
|        - |   101 | ` */` |
|        - |   102 | `typedef struct VmShutdownCB VmShutdownCB;` |
|        - |   103 | `struct VmShutdownCB` |
|        - |   104 |  |
|        - |   105 | `	ph7_value sCallback; /* Shutdown callback */` |
|        - |   106 | `	ph7_value aArg[10];   /* Callback arguments (10 maximum arguments) */` |
|        - |   107 | `	int nArg;             /* Total number of given arguments */` |
|        - |   108 | `};` |
|        - |   109 | `/* Uncaught exception code value */` |
|        - |   110 | `#define PH7_EXCEPTION -255` |
|        - |   111 |  |
|        - |   112 | `/*` |
|        - |   113 | ` * Return TRUE if either operand is a NaN real value.` |
|        - |   114 | ` */` |
|   748036 |   115 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   116 |  |
|   748038 |   117 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       23 |   118 | `		return TRUE;` |
|        - |   119 | `	}` |
|   748016 |   120 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |   121 | `		return TRUE;` |
|        - |   122 | `	}` |
|   748008 |   123 | `	return FALSE;` |
|   374042 |   124 |  |
|        - |   125 | `/* SyhttpUri, SyhttpHeader and HTTP method/protocol defines moved to ph7int.h */` |
|        - |   126 | `/*` |
|        - |   127 | ` * Register a constant and it's associated expansion callback so that` |
|        - |   128 | ` * it can be expanded from the target PHP program.` |
|        - |   129 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|        - |   130 | ` * simple and work as follows:` |
|        - |   131 | ` * Each registered constant have a C procedure associated with it.` |
|        - |   132 | ` * This procedure known as the constant expansion callback is responsible` |
|        - |   133 | ` * of expanding the invoked constant to the desired value,for example:` |
|        - |   134 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|        - |   135 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|        - |   136 | ` * (Windows,Linux,...) and so on.` |
|        - |   137 | ` * Please refer to the official documentation for additional information.` |
|        - |   138 | ` */` |
|   337762 |   139 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |   140 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   141 | `	const SyString *pName,  /* Constant name */` |
|        - |   142 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   143 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |   144 | `	)` |
|        2 |   145 |  |
|        - |   146 | `	ph7_constant *pCons;` |
|        - |   147 | `	SyHashEntry *pEntry;` |
|        - |   148 | `	char *zDupName;` |
|        - |   149 | `	sxi32 rc;` |
|   337764 |   150 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   337764 |   151 | `	if( pEntry ){` |
|        - |   152 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   153 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   154 | `		pCons->xExpand = xExpand;` |
|        6 |   155 | `		pCons->pUserData = pUserData;` |
|        6 |   156 | `		return SXRET_OK;` |
|        - |   157 | `	}` |
|        - |   158 | `	/* Allocate a new constant instance */` |
|   337760 |   159 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   337760 |   160 | `	if( pCons == 0 ){` |
|      ! 0 |   161 | `		return 0;` |
|        - |   162 | `	}` |
|        - |   163 | `	/* Duplicate constant name */` |
|   337760 |   164 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   337760 |   165 | `	if( zDupName == 0 ){` |
|      ! 0 |   166 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   167 | `		return 0;` |
|        - |   168 | `	}` |
|        - |   169 | `	/* Install the constant */` |
|   337760 |   170 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   337760 |   171 | `	pCons->xExpand = xExpand;` |
|   337760 |   172 | `	pCons->pUserData = pUserData;` |
|   337760 |   173 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   337760 |   174 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   175 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   176 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   177 | `		return rc;` |
|        - |   178 | `	}` |
|        - |   179 | `	/* All done,constant can be invoked from PHP code */` |
|   337760 |   180 | `	return SXRET_OK;` |
|   168883 |   181 |  |
|        - |   182 | `/*` |
|        - |   183 | ` * Allocate a new foreign function instance.` |
|        - |   184 | ` * This function return SXRET_OK on success. Any other` |
|        - |   185 | ` * return value indicates failure.` |
|        - |   186 | ` * Please refer to the official documentation for an introduction to` |
|        - |   187 | ` * the foreign function mechanism.` |
|        - |   188 | ` */` |
|   727320 |   189 | `static sxi32 PH7_NewForeignFunction(` |
|        - |   190 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   191 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   192 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   193 | `	void *pUserData,          /* Foreign function private data */` |
|        - |   194 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |   195 | `	)` |
|        2 |   196 |  |
|        - |   197 | `	ph7_user_func *pFunc;` |
|        - |   198 | `	char *zDup;` |
|        - |   199 | `	/* Allocate a new user function */` |
|   727322 |   200 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   727322 |   201 | `	if( pFunc == 0 ){` |
|      ! 0 |   202 | `		return SXERR_MEM;` |
|        - |   203 | `	}` |
|        - |   204 | `	/* Duplicate function name */` |
|   727322 |   205 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   727322 |   206 | `	if( zDup == 0 ){` |
|      ! 0 |   207 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   208 | `		return SXERR_MEM;` |
|        - |   209 | `	}` |
|        - |   210 | `	/* Zero the structure */` |
|   727322 |   211 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   212 | `	/* Initialize structure fields */` |
|   727322 |   213 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   727322 |   214 | `	pFunc->pVm   = pVm;` |
|   727322 |   215 | `	pFunc->xFunc = xFunc;` |
|   727322 |   216 | `	pFunc->pUserData = pUserData;` |
|   727322 |   217 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   218 | `	/* Write a pointer to the new function */` |
|   727322 |   219 | `	*ppOut = pFunc;` |
|   727322 |   220 | `	return SXRET_OK;` |
|   363662 |   221 |  |
|        - |   222 | `/*` |
|        - |   223 | ` * Install a foreign function and it's associated callback so that` |
|        - |   224 | ` * it can be invoked from the target PHP code.` |
|        - |   225 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   226 | ` * return value indicates failure.` |
|        - |   227 | ` * Please refer to the official documentation for an introduction to` |
|        - |   228 | ` * the foreign function mechanism.` |
|        - |   229 | ` */` |
|   728992 |   230 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |   231 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   232 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   233 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   234 | `	void *pUserData           /* Foreign function private data */` |
|        - |   235 | `	)` |
|        2 |   236 |  |
|        - |   237 | `	ph7_user_func *pFunc;` |
|        - |   238 | `	SyHashEntry *pEntry;` |
|        - |   239 | `	sxi32 rc;` |
|        - |   240 | `	/* Overwrite any previously registered function with the same name */` |
|   728994 |   241 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   728994 |   242 | `	if( pEntry ){` |
|     1674 |   243 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     1674 |   244 | `		pFunc->pUserData = pUserData;` |
|     1674 |   245 | `		pFunc->xFunc = xFunc;` |
|     1674 |   246 | `		SySetReset(&pFunc->aAux);` |
|     1674 |   247 | `		return SXRET_OK;` |
|        - |   248 | `	}` |
|        - |   249 | `	/* Create a new user function */` |
|   727322 |   250 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   727322 |   251 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   252 | `		return rc;` |
|        - |   253 | `	}` |
|        - |   254 | `	/* Install the function in the corresponding hashtable */` |
|   727322 |   255 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   727322 |   256 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   257 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   258 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   259 | `		return rc;` |
|        - |   260 | `	}` |
|        - |   261 | `	/* User function successfully installed */` |
|   727322 |   262 | `	return SXRET_OK;` |
|   364498 |   263 |  |
|        - |   264 | `/*` |
|        - |   265 | ` * Initialize a VM function.` |
|        - |   266 | ` */` |
|    80890 |   267 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   268 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   269 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   270 | `	const char *zName,  /* Function name */` |
|        - |   271 | `	sxu32 nByte,        /* zName length */` |
|        - |   272 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   273 | `	void *pUserData     /* Function private data */` |
|        - |   274 | `	)` |
|        2 |   275 |  |
|        - |   276 | `	/* Zero the structure */` |
|    80892 |   277 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   278 | `	/* Initialize structure fields */` |
|        - |   279 | `	/* Arguments container */` |
|    80892 |   280 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   281 | `	/* Static variable container */` |
|    80892 |   282 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   283 | `	/* Bytecode container */` |
|    80892 |   284 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   285 | `    /* Preallocate some instruction slots */` |
|    80892 |   286 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   287 | `	/* Closure environment */` |
|    80892 |   288 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|    80892 |   289 | `	pFunc->iFlags = iFlags;` |
|    80892 |   290 | `	pFunc->pUserData = pUserData;` |
|    80892 |   291 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|    80892 |   292 | `	return SXRET_OK;` |
|        2 |   293 |  |
|        - |   294 | `/*` |
|        - |   295 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   296 | ` */` |
|   258348 |   297 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   298 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   299 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   300 | `	SyString *pName     /* Function name */` |
|        - |   301 | `	)` |
|        2 |   302 |  |
|        - |   303 | `	SyHashEntry *pEntry;` |
|        - |   304 | `	sxi32 rc;` |
|   258350 |   305 | `	if( pName == 0 ){` |
|        - |   306 | `		/* Use the built-in name */` |
|    25286 |   307 | `		pName = &pFunc->sName;` |
|    12642 |   308 | `	}` |
|        - |   309 | `	/* Check for duplicates (functions with the same name) first */` |
|   258350 |   310 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   258350 |   311 | `	if( pEntry ){` |
|   192946 |   312 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   192946 |   313 | `		if( pLink != pFunc ){` |
|        - |   314 | `			/* Link */` |
|      179 |   315 | `			pFunc->pNextName = pLink;` |
|      179 |   316 | `			pEntry->pUserData = pFunc;` |
|       89 |   317 | `		}` |
|   192946 |   318 | `		return SXRET_OK;` |
|        - |   319 | `	}` |
|        - |   320 | `	/* First time seen */` |
|    65406 |   321 | `	pFunc->pNextName = 0;` |
|    65406 |   322 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    65406 |   323 | `	return rc;` |
|   129176 |   324 |  |
|        - |   325 | `/*` |
|        - |   326 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   327 | ` */` |
|    21268 |   328 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   329 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   330 | `	ph7_class *pClass /* Target Class */` |
|        - |   331 | `	)` |
|        2 |   332 |  |
|    21270 |   333 | `	SyString *pName = &pClass->sName;` |
|        - |   334 | `	SyHashEntry *pEntry;` |
|        - |   335 | `	sxi32 rc;` |
|        - |   336 | `	/* Check for duplicates */` |
|    21270 |   337 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    21270 |   338 | `	if( pEntry ){` |
|       31 |   339 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   340 | `		/* Link entry with the same name */` |
|       31 |   341 | `		pClass->pNextName = pLink;` |
|       31 |   342 | `		pEntry->pUserData = pClass;` |
|       31 |   343 | `		return SXRET_OK;` |
|        - |   344 | `	}` |
|    21240 |   345 | `	pClass->pNextName = 0;` |
|        - |   346 | `	/* Perform a simple hashtable insertion */` |
|    21240 |   347 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    21240 |   348 | `	return rc;` |
|    10636 |   349 |  |
|        - |   350 | `/*` |
|        - |   351 | ` * Instruction builder interface.` |
|        - |   352 | ` */` |
|  2017712 |   353 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   354 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   355 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   356 | `	sxi32 iP1,    /* First operand */` |
|        - |   357 | `	sxu32 iP2,    /* Second operand */` |
|        - |   358 | `	void *p3,     /* Third operand */` |
|        - |   359 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   360 | `	)` |
|        2 |   361 |  |
|        - |   362 | `	VmInstr sInstr;` |
|        - |   363 | `	sxi32 rc;` |
|        - |   364 | `	/* Fill the VM instruction */` |
|  2017714 |   365 | `	sInstr.iOp = (sxu8)iOp;` |
|  2017714 |   366 | `	sInstr.iP1 = iP1;` |
|  2017714 |   367 | `	sInstr.iP2 = iP2;` |
|  2017714 |   368 | `	sInstr.p3  = p3;` |
|  2017714 |   369 | `	if( pIndex ){` |
|        - |   370 | `		/* Instruction index in the bytecode array */` |
|   122832 |   371 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    61415 |   372 | `	}` |
|        - |   373 | `	/* Finally,record the instruction */` |
|  2017714 |   374 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  2017714 |   375 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   376 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   377 | `		/* Fall throw */` |
|      ! 0 |   378 | `	}` |
|  2017714 |   379 | `	return rc;` |
|        2 |   380 |  |
|        - |   381 | `/*` |
|        - |   382 | ` * Swap the current bytecode container with the given one.` |
|        - |   383 | ` */` |
|   196656 |   384 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   385 |  |
|   196658 |   386 | `	if( pContainer == 0 ){` |
|        - |   387 | `		/* Point to the default container */` |
|      ! 0 |   388 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   389 | `	}else{` |
|        - |   390 | `		/* Change container */` |
|   196658 |   391 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   392 | `	}` |
|   196658 |   393 | `	return SXRET_OK;` |
|        2 |   394 |  |
|        - |   395 | `/*` |
|        - |   396 | ` * Return the current bytecode container.` |
|        - |   397 | ` */` |
|    98328 |   398 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   399 |  |
|    98330 |   400 | `	return pVm->pByteContainer;` |
|        2 |   401 |  |
|        - |   402 | `/*` |
|        - |   403 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   404 | ` */` |
|   120844 |   405 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   406 |  |
|        - |   407 | `	VmInstr *pInstr;` |
|   120846 |   408 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   120846 |   409 | `	return pInstr;` |
|        2 |   410 |  |
|        - |   411 | `/*` |
|        - |   412 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   413 | ` */` |
|   586254 |   414 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   415 |  |
|   586256 |   416 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   417 |  |
|        - |   418 | `/*` |
|        - |   419 | ` * Pop the last VM instruction.` |
|        - |   420 | ` */` |
|   117524 |   421 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   422 |  |
|   117526 |   423 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   424 |  |
|        - |   425 | `/*` |
|        - |   426 | ` * Peek the last VM instruction.` |
|        - |   427 | ` */` |
|   313146 |   428 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   429 |  |
|   313148 |   430 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   431 |  |
|     7866 |   432 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   433 |  |
|        - |   434 | `	VmInstr *aInstr;` |
|        - |   435 | `	sxu32 n;` |
|     7868 |   436 | `	n = SySetUsed(pVm->pByteContainer);` |
|     7868 |   437 | `	if( n < 2 ){` |
|      ! 0 |   438 | `		return 0;` |
|        - |   439 | `	}` |
|     7868 |   440 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|     7868 |   441 | `	return &aInstr[n - 2];` |
|     3935 |   442 |  |
|        - |   443 | `/*` |
|        - |   444 | ` * Allocate a new virtual machine frame.` |
|        - |   445 | ` */` |
|    12606 |   446 | `static VmFrame * VmNewFrame(` |
|        - |   447 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   448 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   449 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   450 | `	)` |
|        2 |   451 |  |
|        - |   452 | `	VmFrame *pFrame;` |
|        - |   453 | `	/* Allocate a new vm frame */` |
|    12608 |   454 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    12608 |   455 | `	if( pFrame == 0 ){` |
|      ! 0 |   456 | `		return 0;` |
|        - |   457 | `	}` |
|        - |   458 | `	/* Zero the structure */` |
|    12608 |   459 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   460 | `	/* Initialize frame fields */` |
|    12608 |   461 | `	pFrame->pUserData = pUserData;` |
|    12608 |   462 | `	pFrame->pThis = pThis;` |
|    12608 |   463 | `	pFrame->pVm = pVm;` |
|    12608 |   464 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    12608 |   465 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    12608 |   466 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    12608 |   467 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    12608 |   468 | `	return pFrame;` |
|     6305 |   469 |  |
|        - |   470 | `/*` |
|        - |   471 | ` * Enter a VM frame.` |
|        - |   472 | ` */` |
|    12606 |   473 | `static sxi32 VmEnterFrame(` |
|        - |   474 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   475 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   476 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   477 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   478 | `	)` |
|        2 |   479 |  |
|        - |   480 | `	VmFrame *pFrame;` |
|        - |   481 | `	/* Allocate a new frame */` |
|    12608 |   482 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    12608 |   483 | `	if( pFrame == 0 ){` |
|      ! 0 |   484 | `		return SXERR_MEM;` |
|        - |   485 | `	}` |
|        - |   486 | `	/* Link to the list of active VM frame */` |
|    12608 |   487 | `	pFrame->pParent = pVm->pFrame;` |
|    12608 |   488 | `	pVm->pFrame = pFrame;` |
|    12608 |   489 | `	if( ppFrame ){` |
|        - |   490 | `		/* Write a pointer to the new VM frame */` |
|    10698 |   491 | `		*ppFrame = pFrame;` |
|     5348 |   492 | `	}` |
|    12608 |   493 | `	return SXRET_OK;` |
|     6305 |   494 |  |
|        - |   495 | `/*` |
|        - |   496 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   497 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   498 | ` * information.` |
|        - |   499 | ` */` |
|       48 |   500 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        1 |   501 |  |
|        - |   502 | `	VmFrame *pTarget,*pFrame;` |
|       49 |   503 | `	SyHashEntry *pEntry = 0;` |
|        - |   504 | `	sxi32 rc;` |
|        - |   505 | `	/* Point to the upper frame */` |
|       49 |   506 | `	pFrame = pVm->pFrame;` |
|       49 |   507 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |   508 | `		/* Safely ignore the exception frame */` |
|      ! 0 |   509 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   510 | `	}` |
|       49 |   511 | `	pTarget = pFrame;` |
|       49 |   512 | `	pFrame = pTarget->pParent;` |
|       63 |   513 | `	while( pFrame ){` |
|       63 |   514 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   515 | `			/* Query the current frame */` |
|       49 |   516 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       49 |   517 | `			if( pEntry ){` |
|        - |   518 | `				/* Variable found */` |
|       49 |   519 | `				break;` |
|        - |   520 | `			}` |
|      ! 0 |   521 | `		}` |
|        - |   522 | `		/* Point to the upper frame */` |
|       15 |   523 | `		pFrame = pFrame->pParent;` |
|        1 |   524 | `	}` |
|       49 |   525 | `	if( pEntry == 0 ){` |
|        - |   526 | `		/* Inexistant variable */` |
|      ! 0 |   527 | `		return SXERR_NOTFOUND;` |
|        - |   528 | `	}` |
|        - |   529 | `	/* Link to the current frame */` |
|       49 |   530 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       49 |   531 | `	if( rc == SXRET_OK ){` |
|        - |   532 | `		sxu32 nIdx;` |
|       49 |   533 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       49 |   534 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       24 |   535 | `	}` |
|       49 |   536 | `	return rc;` |
|       25 |   537 |  |
|        - |   538 | `/*` |
|        - |   539 | ` * Leave the top-most active frame.` |
|        - |   540 | ` */` |
|    10694 |   541 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   542 |  |
|    10696 |   543 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    10696 |   544 | `	if( pCurFrame ){` |
|        - |   545 | `		/* Unlink from the list of active VM frame */` |
|    10696 |   546 | `		pVm->pFrame = pCurFrame->pParent;` |
|    10696 |   547 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   548 | `			VmSlot  *aSlot;` |
|        - |   549 | `			sxu32 n;` |
|        - |   550 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    10678 |   551 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    77702 |   552 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   553 | `				/* Unset the local variable */` |
|    67026 |   554 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    33514 |   555 | `			}` |
|        - |   556 | `			/* Remove local reference */` |
|    10678 |   557 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    77754 |   558 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    67078 |   559 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    33540 |   560 | `			}` |
|     5338 |   561 | `		}` |
|        - |   562 | `		/* Release internal containers */` |
|    10696 |   563 | `		SyHashRelease(&pCurFrame->hVar);` |
|    10696 |   564 | `		SySetRelease(&pCurFrame->sArg);` |
|    10696 |   565 | `		SySetRelease(&pCurFrame->sLocal);` |
|    10696 |   566 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   567 | `		/* Release the whole structure */` |
|    10696 |   568 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     5347 |   569 | `	}` |
|    10696 |   570 |  |
|        - |   571 | `/*` |
|        - |   572 | ` * Compare two functions signature and return the comparison result.` |
|        - |   573 | ` */` |
|      818 |   574 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   575 |  |
|      819 |   576 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      819 |   577 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      819 |   578 | `	const char *zSin = pSecond->zString;` |
|      819 |   579 | `	const char *zFin = pFirst->zString;` |
|      819 |   580 | `	const char *zPtr = zFin;` |
|      409 |   581 | `	for(;;){` |
|      819 |   582 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      410 |   583 | `			break;` |
|        - |   584 | `		}` |
|      ! 0 |   585 | `		if( zFin[0] != zSin[0] ){` |
|        - |   586 | `			/* mismatch */` |
|      ! 0 |   587 | `			break;` |
|        - |   588 | `		}` |
|      ! 0 |   589 | `		zFin++;` |
|      ! 0 |   590 | `		zSin++;` |
|      ! 0 |   591 | `	}` |
|      819 |   592 | `	return (int)(zFin-zPtr);` |
|        1 |   593 |  |
|        - |   594 | `/*` |
|        - |   595 | ` * Select the appropriate VM function for the current call context.` |
|        - |   596 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   597 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   598 | ` * Refer to the official documentation for more information.` |
|        - |   599 | ` */` |
|      122 |   600 | `static ph7_vm_func * VmOverload(` |
|        - |   601 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   602 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   603 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   604 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   605 | `	)` |
|        1 |   606 |  |
|        - |   607 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   608 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   609 | `	ph7_vm_func *pLink;` |
|        - |   610 | `	SyString sArgSig;` |
|        - |   611 | `	SyBlob sSig;` |
|        - |   612 |  |
|      123 |   613 | `	pLink = pList;` |
|      123 |   614 | `	i = 0;` |
|        - |   615 | `	/* Put functions expecting the same number of passed arguments */` |
|     1031 |   616 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|      969 |   617 | `		if( pLink == 0 ){` |
|       61 |   618 | `			break;` |
|        - |   619 | `		}` |
|      909 |   620 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   621 | `			/* Candidate for overloading */` |
|      863 |   622 | `			apSet[i++] = pLink;` |
|      431 |   623 | `		}` |
|        - |   624 | `		/* Point to the next entry */` |
|      909 |   625 | `		pLink = pLink->pNextName;` |
|        1 |   626 | `	}` |
|      123 |   627 | `	if( i < 1 ){` |
|        - |   628 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   629 | `		return pList;` |
|        - |   630 | `	}` |
|      123 |   631 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   632 | `		/* Return the only candidate */` |
|       21 |   633 | `		return apSet[0];` |
|        - |   634 | `	}` |
|        - |   635 | `	/* Calculate function signature */` |
|      103 |   636 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      355 |   637 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      253 |   638 | `		int c = 'n'; /* null */` |
|      253 |   639 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   640 | `			/* Hashmap */` |
|       45 |   641 | `			c = 'h';` |
|      231 |   642 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   643 | `			/* bool */` |
|      ! 0 |   644 | `			c = 'b';` |
|      209 |   645 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   646 | `			/* int */` |
|        5 |   647 | `			c = 'i';` |
|      207 |   648 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   649 | `			/* String */` |
|      105 |   650 | `			c = 's';` |
|      153 |   651 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   652 | `			/* Float */` |
|      ! 0 |   653 | `			c = 'f';` |
|      101 |   654 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   655 | `			/* Class instance */` |
|      ! 0 |   656 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|      ! 0 |   657 | `			SyString *pName = &pClass->sName;` |
|      ! 0 |   658 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|      ! 0 |   659 | `			c = -1;` |
|      ! 0 |   660 | `		}` |
|      253 |   661 | `		if( c > 0 ){` |
|      253 |   662 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      126 |   663 | `		}` |
|      127 |   664 | `	}` |
|      103 |   665 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      103 |   666 | `	iTarget = 0;` |
|      103 |   667 | `	iMax = -1;` |
|        - |   668 | `	/* Select the appropriate function */` |
|      921 |   669 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   670 | `		/* Compare the two signatures */` |
|      819 |   671 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      819 |   672 | `		if( iCur > iMax ){` |
|      103 |   673 | `			iMax = iCur;` |
|      103 |   674 | `			iTarget = j;` |
|       51 |   675 | `		}` |
|      410 |   676 | `	}` |
|      103 |   677 | `	SyBlobRelease(&sSig);` |
|        - |   678 | `	/* Appropriate function for the current call context */` |
|      103 |   679 | `	return apSet[iTarget];` |
|       62 |   680 |  |
|        - |   681 | `/* Forward declaration */` |
|        - |   682 | `static sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult);` |
|        - |   683 | `static sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...);` |
|        - |   684 | `/*` |
|        - |   685 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   686 | ` * it can be instanciated from the executed PHP script.` |
|        - |   687 | ` */` |
|    71332 |   688 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   689 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   690 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   691 | `	)` |
|        2 |   692 |  |
|        - |   693 | `	ph7_class_method *pMeth;` |
|        - |   694 | `	ph7_class_attr *pAttr;` |
|        - |   695 | `	SyHashEntry *pEntry;` |
|        - |   696 | `	sxi32 rc;` |
|        - |   697 | `	/* Reset the loop cursor */` |
|    71334 |   698 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   699 | `	/* Process only static and constant attribute */` |
|   251756 |   700 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   701 | `		/* Extract the current attribute */` |
|   144758 |   702 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   144758 |   703 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   704 | `			ph7_value *pMemObj;` |
|        - |   705 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1290 |   706 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1290 |   707 | `			if( pMemObj == 0 ){` |
|      ! 0 |   708 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   709 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   710 | `					&pClass->sName,&pAttr->sName` |
|        - |   711 | `					);` |
|      ! 0 |   712 | `				return SXERR_MEM;` |
|        - |   713 | `			}` |
|     1290 |   714 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   715 | `				/* Initialize attribute default value (any complex expression) */` |
|     1290 |   716 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      644 |   717 | `			}` |
|        - |   718 | `			/* Record attribute index */` |
|     1290 |   719 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   720 | `			/* Install static attribute in the reference table */` |
|     1290 |   721 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      644 |   722 | `		}` |
|        2 |   723 | `	}` |
|        - |   724 | `	/* Install class methods */` |
|    71334 |   725 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |   726 | `		/* Do not mount interface methods since they are signatures only.` |
|        - |   727 | `		 */` |
|    42444 |   728 | `		return SXRET_OK;` |
|        - |   729 | `	}` |
|        - |   730 | `	/* Create constructor alias if not yet done */` |
|    28892 |   731 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   732 | `		/* User constructor with the same base class name */` |
|      206 |   733 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      206 |   734 | `		if( pEntry ){` |
|      ! 0 |   735 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   736 | `			/* Create the alias */` |
|      ! 0 |   737 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   738 | `		}` |
|      102 |   739 | `	}` |
|        - |   740 | `	/* Install the methods now */` |
|    28892 |   741 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   276407 |   742 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   233072 |   743 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   233072 |   744 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   233066 |   745 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   233066 |   746 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   747 | `				return rc;` |
|        - |   748 | `			}` |
|   116532 |   749 | `		}` |
|        2 |   750 | `	}` |
|        - |   751 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    28892 |   752 | `	pClass->bMounted = TRUE;` |
|    28892 |   753 | `	return SXRET_OK;` |
|    35668 |   754 |  |
|        - |   755 | `/*` |
|        - |   756 | ` * Allocate a private frame for attributes of the given` |
|        - |   757 | ` * class instance (Object in the PHP jargon).` |
|        - |   758 | ` */` |
|      916 |   759 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   760 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   761 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   762 | `	)` |
|        2 |   763 |  |
|      918 |   764 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   765 | `	ph7_class_attr *pAttr;` |
|        - |   766 | `	SyHashEntry *pEntry;` |
|        - |   767 | `	sxi32 rc;` |
|        - |   768 | `	/* Install class attribute in the private frame associated with this instance */` |
|      918 |   769 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     3704 |   770 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   771 | `		VmClassAttr *pVmAttr;` |
|        - |   772 | `		/* Extract the current attribute */` |
|     2788 |   773 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     2788 |   774 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     2788 |   775 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   776 | `			return SXERR_MEM;` |
|        - |   777 | `		}` |
|     2788 |   778 | `		pVmAttr->pAttr = pAttr;` |
|     2788 |   779 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   780 | `			ph7_value *pMemObj;` |
|        - |   781 | `			/* Reserve a memory object for this attribute */` |
|     2782 |   782 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     2782 |   783 | `			if( pMemObj == 0 ){` |
|      ! 0 |   784 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   785 | `				return SXERR_MEM;` |
|        - |   786 | `			}` |
|     2782 |   787 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     2782 |   788 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   789 | `				/* Initialize attribute default value (any complex expression) */` |
|      904 |   790 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      451 |   791 | `			}` |
|     2782 |   792 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     2782 |   793 | `			if( rc != SXRET_OK ){` |
|        - |   794 | `				VmSlot sSlot;` |
|        - |   795 | `				/* Restore memory object */` |
|      ! 0 |   796 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   797 | `				sSlot.pUserData = 0;` |
|      ! 0 |   798 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   799 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   800 | `				return SXERR_MEM;` |
|        - |   801 | `			}` |
|        - |   802 | `			/* Install attribute in the reference table */` |
|     2782 |   803 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1392 |   804 | `		}else{` |
|        - |   805 | `			/* Install static/constant attribute */` |
|        8 |   806 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   807 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   808 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   809 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   810 | `				return SXERR_MEM;` |
|        - |   811 | `			}` |
|        - |   812 | `		}` |
|        2 |   813 | `	}` |
|      918 |   814 | `	return SXRET_OK;` |
|      460 |   815 |  |
|        - |   816 | `/* Forward declaration */` |
|        - |   817 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   818 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   819 | `/*` |
|        - |   820 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   821 | ` */` |
|        - |   822 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   823 | `/*` |
|        - |   824 | ` * Reserve a constant memory object.` |
|        - |   825 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   826 | ` */` |
|   233306 |   827 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   828 |  |
|        - |   829 | `	ph7_value *pObj;` |
|        - |   830 | `	sxi32 rc;` |
|   233308 |   831 | `	if( pIndex ){` |
|        - |   832 | `		/* Object index in the object table */` |
|   227578 |   833 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   113788 |   834 | `	}` |
|        - |   835 | `	/* Reserve a slot for the new object */` |
|   233308 |   836 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   233308 |   837 | `	if( rc != SXRET_OK ){` |
|        - |   838 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   839 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   840 | `		 */` |
|      ! 0 |   841 | `		return 0;` |
|        - |   842 | `	}` |
|   233308 |   843 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   233308 |   844 | `	return pObj;` |
|   116655 |   845 |  |
|        - |   846 | `/*` |
|        - |   847 | ` * Reserve a memory object.` |
|        - |   848 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   849 | ` */` |
|  2129624 |   850 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   851 |  |
|        - |   852 | `	ph7_value *pObj;` |
|        - |   853 | `	sxi32 rc;` |
|  2129626 |   854 | `	if( pIndex ){` |
|        - |   855 | `		/* Object index in the object table */` |
|  2129626 |   856 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1064812 |   857 | `	}` |
|        - |   858 | `	/* Reserve a slot for the new object */` |
|  2129626 |   859 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2129626 |   860 | `	if( rc != SXRET_OK ){` |
|        - |   861 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   862 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   863 | `		 */` |
|      ! 0 |   864 | `		return 0;` |
|        - |   865 | `	}` |
|  2129626 |   866 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2129626 |   867 | `	return pObj;` |
|  1064814 |   868 |  |
|        - |   869 | `/* Forward declaration */` |
|        - |   870 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   871 | `/*` |
|        - |   872 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   873 | ` * directly as foreign functions.` |
|        - |   874 | ` */` |
|        - |   875 | `#define PH7_BUILTIN_LIB \` |
|        - |   876 | `	"class Exception { "\` |
|        - |   877 | `    "protected $message = 'Unknown exception';"\` |
|        - |   878 | `    "protected $code = 0;"\` |
|        - |   879 | `    "protected $file;"\` |
|        - |   880 | `    "protected $line;"\` |
|        - |   881 | `    "protected $trace;"\` |
|        - |   882 | `    "protected $previous;"\` |
|        - |   883 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   884 | `	"   if( isset($message) ){"\` |
|        - |   885 | `	"	  $this->message = $message;"\` |
|        - |   886 | `	"   }"\` |
|        - |   887 | `	"   $this->code = $code;"\` |
|        - |   888 | `	"   $this->file = __FILE__;"\` |
|        - |   889 | `	"   $this->line = __LINE__;"\` |
|        - |   890 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   891 | `	"   if( isset($previous) ){"\` |
|        - |   892 | `	"     $this->previous = $previous;"\` |
|        - |   893 | `	"   }"\` |
|        - |   894 | `	"}"\` |
|        - |   895 | `	"public function getMessage(){"\` |
|        - |   896 | `	"   return $this->message;"\` |
|        - |   897 | `	"}"\` |
|        - |   898 | `	" public function getCode(){"\` |
|        - |   899 | `	"  return $this->code;"\` |
|        - |   900 | `	"}"\` |
|        - |   901 | `	"public function getFile(){"\` |
|        - |   902 | `	"  return $this->file;"\` |
|        - |   903 | `	"}"\` |
|        - |   904 | `	"public function getLine(){"\` |
|        - |   905 | `	"  return $this->line;"\` |
|        - |   906 | `	"}"\` |
|        - |   907 | `	"public function getTrace(){"\` |
|        - |   908 | `	"   return $this->trace;"\` |
|        - |   909 | `	"}"\` |
|        - |   910 | `	"public function getTraceAsString(){"\` |
|        - |   911 | `	"  return debug_string_backtrace();"\` |
|        - |   912 | `	"}"\` |
|        - |   913 | `	"public function getPrevious(){"\` |
|        - |   914 | `	"    return $this->previous;"\` |
|        - |   915 | `	"}"\` |
|        - |   916 | `	"public function __toString(){"\` |
|        - |   917 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   918 | `    "}"\` |
|        - |   919 | `	"}"\` |
|        - |   920 | `	"class Error extends Exception { }"\` |
|        - |   921 | `	"class TypeError extends Error { }"\` |
|        - |   922 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   923 | `	"class ValueError extends Error { }"\` |
|        - |   924 | `	"class ErrorException extends Exception { "\` |
|        - |   925 | `	"protected $severity;"\` |
|        - |   926 | `	"public function __construct(string $message = null,"\` |
|        - |   927 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   928 | `	"   if( isset($message) ){"\` |
|        - |   929 | `	"	  $this->message = $message;"\` |
|        - |   930 | `	"   }"\` |
|        - |   931 | `	"   $this->severity = $severity;"\` |
|        - |   932 | `	"   $this->code = $code;"\` |
|        - |   933 | `	"   $this->file = $filename;"\` |
|        - |   934 | `	"   $this->line = $lineno;"\` |
|        - |   935 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   936 | `	"   if( isset($previous) ){"\` |
|        - |   937 | `	"     $this->previous = $previous;"\` |
|        - |   938 | `	"   }"\` |
|        - |   939 | `	"}"\` |
|        - |   940 | `	"public function getSeverity(){"\` |
|        - |   941 | `	"   return $this->severity;"\` |
|        - |   942 | `    "}"\` |
|        - |   943 | `	"}"\` |
|        - |   944 | `	"interface Iterator {"\` |
|        - |   945 | `	"public function current();"\` |
|        - |   946 | `	"public function key();"\` |
|        - |   947 | `	"public function next();"\` |
|        - |   948 | `	"public function rewind();"\` |
|        - |   949 | `	"public function valid();"\` |
|        - |   950 | `	"}"\` |
|        - |   951 | `	"interface IteratorAggregate {"\` |
|        - |   952 | `	"public function getIterator();"\` |
|        - |   953 | `	"}"\` |
|        - |   954 | `	"interface Serializable {"\` |
|        - |   955 | `	"public function serialize();"\` |
|        - |   956 | `	"public function unserialize(string $serialized);"\` |
|        - |   957 | `	"}"\` |
|        - |   958 | `	"/* Directory releated IO */"\` |
|        - |   959 | `	"class Directory {"\` |
|        - |   960 | `	"public $handle = null;"\` |
|        - |   961 | `	"public $path  = null;"\` |
|        - |   962 | `	"public function __construct(string $path)"\` |
|        - |   963 | `	"{"\` |
|        - |   964 | `	"   $this->handle = opendir($path);"\` |
|        - |   965 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   966 | `	"      $this->path = $path;"\` |
|        - |   967 | `	"   }"\` |
|        - |   968 | `	"}"\` |
|        - |   969 | `	"public function __destruct()"\` |
|        - |   970 | `	"{"\` |
|        - |   971 | `	"  if( $this->handle != null ){"\` |
|        - |   972 | `	"       closedir($this->handle);"\` |
|        - |   973 | `	"  }"\` |
|        - |   974 | `	"}"\` |
|        - |   975 | `	"public function read()"\` |
|        - |   976 | `	"{"\` |
|        - |   977 | `	"    return readdir($this->handle);"\` |
|        - |   978 | `	"}"\` |
|        - |   979 | `	"public function rewind()"\` |
|        - |   980 | `	"{"\` |
|        - |   981 | `	"    rewinddir($this->handle);"\` |
|        - |   982 | `	"}"\` |
|        - |   983 | `	"public function close()"\` |
|        - |   984 | `	"{"\` |
|        - |   985 | `	"    closedir($this->handle);"\` |
|        - |   986 | `	"    $this->handle = null;"\` |
|        - |   987 | `	"}"\` |
|        - |   988 | `	"}"\` |
|        - |   989 | `	"class stdClass{"\` |
|        - |   990 | `	"  public $value;"\` |
|        - |   991 | `	" /* Magic methods */"\` |
|        - |   992 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |   993 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |   994 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |   995 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |   996 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |   997 | `	"}"\` |
|        - |   998 | `	"function dir(string $path){"\` |
|        - |   999 | `	"   return new Directory($path);"\` |
|        - |  1000 | `	"}"\` |
|        - |  1001 | `	"function Dir(string $path){"\` |
|        - |  1002 | `	"   return new Directory($path);"\` |
|        - |  1003 | `	"}"\` |
|        - |  1004 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1005 | `    "{"\` |
|        - |  1006 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1007 | `	"  $aDir = array();"\` |
|        - |  1008 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1009 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1010 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1011 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1012 | `	"   }"\` |
|        - |  1013 | `	"  closedir($pHandle);"\` |
|        - |  1014 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1015 | `	"      rsort($aDir);"\` |
|        - |  1016 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1017 | `	"      sort($aDir);"\` |
|        - |  1018 | `	"  }"\` |
|        - |  1019 | `	"  return $aDir;"\` |
|        - |  1020 | `	"}"\` |
|        - |  1021 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1022 | `	"/* Open the target directory */"\` |
|        - |  1023 | `	"$zDir = dirname($pattern);"\` |
|        - |  1024 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1025 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1026 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1027 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1028 | `	"	return FALSE;"\` |
|        - |  1029 | `	"}"\` |
|        - |  1030 | `	"$pattern = basename($pattern);"\` |
|        - |  1031 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1032 | `	"/* Loop throw available entries */"\` |
|        - |  1033 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1034 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1035 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1036 | `	"	if( $rc ){"\` |
|        - |  1037 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1038 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1039 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1040 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1041 | `	"		  }"\` |
|        - |  1042 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1043 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1044 | `	"		 continue;"\` |
|        - |  1045 | `	"	   }"\` |
|        - |  1046 | `	"	   /* Add the entry */"\` |
|        - |  1047 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1048 | `	"	}"\` |
|        - |  1049 | `	" }"\` |
|        - |  1050 | `	"/* Close the handle */"\` |
|        - |  1051 | `	"closedir($pHandle);"\` |
|        - |  1052 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1053 | `	"  /* Sort the array */"\` |
|        - |  1054 | `	"  sort($pArray);"\` |
|        - |  1055 | `	"}"\` |
|        - |  1056 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1057 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1058 | `	"  $pArray[] = $pattern;"\` |
|        - |  1059 | `	"}"\` |
|        - |  1060 | `	"/* Return the created array */"\` |
|        - |  1061 | `	"return $pArray;"\` |
|        - |  1062 | `   "}"\` |
|        - |  1063 | `   "/* Creates a temporary file */"\` |
|        - |  1064 | `   "function tmpfile(){"\` |
|        - |  1065 | `   "  /* Extract the temp directory */"\` |
|        - |  1066 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1067 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1068 | `   "    /* Use the current dir */"\` |
|        - |  1069 | `   "    $zTempDir = '.';"\` |
|        - |  1070 | `   "  }"\` |
|        - |  1071 | `   "  /* Create the file */"\` |
|        - |  1072 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1073 | `   "  return $pHandle;"\` |
|        - |  1074 | `   "}"\` |
|        - |  1075 | `   "/* Creates a temporary filename */"\` |
|        - |  1076 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1077 | `   "{"\` |
|        - |  1078 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1079 | `   "}"\` |
|        - |  1080 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1081 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1082 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1083 | `   "/* Copy arguments */"\` |
|        - |  1084 | `   "$nArgs = func_num_args();"\` |
|        - |  1085 | `   "$pNew = array();"\` |
|        - |  1086 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1087 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1088 | `    "}"\` |
|        - |  1089 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1090 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1091 | `	"/* Erase */"\` |
|        - |  1092 | `	"array_erase($pArray);"\` |
|        - |  1093 | `	"/* Unshift */"\` |
|        - |  1094 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1095 | `	"return sizeof($pArray);"\` |
|        - |  1096 | `    "}"\` |
|        - |  1097 | `	"function array_merge_recursive($array1, $array2){"\` |
|        - |  1098 | `	"if( func_num_args() < 1 ){ return NULL; }"\` |
|        - |  1099 | `    "$arrays = func_get_args();"\` |
|        - |  1100 | `    "$narrays = count($arrays);"\` |
|        - |  1101 | `    "$ret = $arrays[0];"\` |
|        - |  1102 | `    "for ($i = 1; $i < $narrays; $i++) {"\` |
|        - |  1103 | `	 " if( array_same($ret,$arrays[$i]) ){ /* Same instance */continue;}"\` |
|        - |  1104 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1105 | `     "  if (((string) $key) === ((string) intval($key))) {"\` |
|        - |  1106 | `     "   $ret[] = $value;"\` |
|        - |  1107 | `     "  }else{"\` |
|        - |  1108 | `     "  if (is_array($value) && isset($ret[$key]) ) {"\` |
|        - |  1109 | `     "   $ret[$key] = array_merge_recursive($ret[$key], $value);"\` |
|        - |  1110 | `     " }else {"\` |
|        - |  1111 | `     "   $ret[$key] = $value;"\` |
|        - |  1112 | `     "  }"\` |
|        - |  1113 | `     " }"\` |
|        - |  1114 | `     " }"\` |
|        - |  1115 | `	 "}"\` |
|        - |  1116 | `	 " return $ret;"\` |
|        - |  1117 | `    "}"\` |
|        - |  1118 | `	"function max(){"\` |
|        - |  1119 | `    "  $pArgs = func_get_args();"\` |
|        - |  1120 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1121 | `	"  return null;"\` |
|        - |  1122 | `    " }"\` |
|        - |  1123 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1124 | `    " $pArg = $pArgs[0];"\` |
|        - |  1125 | `	" if( !is_array($pArg) ){"\` |
|        - |  1126 | `	"   return $pArg; "\` |
|        - |  1127 | `	" }"\` |
|        - |  1128 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1129 | `	"   return null;"\` |
|        - |  1130 | `	" }"\` |
|        - |  1131 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1132 | `	" reset($pArg);"\` |
|        - |  1133 | `	" $max = current($pArg);"\` |
|        - |  1134 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1135 | `	"   if( $val > $max ){"\` |
|        - |  1136 | `	"     $max = $val;"\` |
|        - |  1137 | `    " }"\` |
|        - |  1138 | `	" }"\` |
|        - |  1139 | `	" return $max;"\` |
|        - |  1140 | `    " }"\` |
|        - |  1141 | `    " $max = $pArgs[0];"\` |
|        - |  1142 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1143 | `    " $val = $pArgs[$i];"\` |
|        - |  1144 | `	"if( $val > $max ){"\` |
|        - |  1145 | `	" $max = $val;"\` |
|        - |  1146 | `	"}"\` |
|        - |  1147 | `    " }"\` |
|        - |  1148 | `	" return $max;"\` |
|        - |  1149 | `    "}"\` |
|        - |  1150 | `	"function min(){"\` |
|        - |  1151 | `    "  $pArgs = func_get_args();"\` |
|        - |  1152 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1153 | `	"  return null;"\` |
|        - |  1154 | `    " }"\` |
|        - |  1155 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1156 | `    " $pArg = $pArgs[0];"\` |
|        - |  1157 | `	" if( !is_array($pArg) ){"\` |
|        - |  1158 | `	"   return $pArg; "\` |
|        - |  1159 | `	" }"\` |
|        - |  1160 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1161 | `	"   return null;"\` |
|        - |  1162 | `	" }"\` |
|        - |  1163 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1164 | `	" reset($pArg);"\` |
|        - |  1165 | `	" $min = current($pArg);"\` |
|        - |  1166 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1167 | `	"   if( $val < $min ){"\` |
|        - |  1168 | `	"     $min = $val;"\` |
|        - |  1169 | `    " }"\` |
|        - |  1170 | `	" }"\` |
|        - |  1171 | `	" return $min;"\` |
|        - |  1172 | `    " }"\` |
|        - |  1173 | `    " $min = $pArgs[0];"\` |
|        - |  1174 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1175 | `    " $val = $pArgs[$i];"\` |
|        - |  1176 | `	"if( $val < $min ){"\` |
|        - |  1177 | `	" $min = $val;"\` |
|        - |  1178 | `	" }"\` |
|        - |  1179 | `    " }"\` |
|        - |  1180 | `	" return $min;"\` |
|        - |  1181 | `	"}"\` |
|        - |  1182 | `	"function fileowner(string $file){"\` |
|        - |  1183 | `    " $a = stat($file);"\` |
|        - |  1184 | `	" if( !is_array($a) ){"\` |
|        - |  1185 | `	"	return false;"\` |
|        - |  1186 | `	" }"\` |
|        - |  1187 | `	" return $a['uid'];"\` |
|        - |  1188 | `    "}"\` |
|        - |  1189 | `    "function filegroup(string $file){"\` |
|        - |  1190 | `	" $a = stat($file);"\` |
|        - |  1191 | `	" if( !is_array($a) ){"\` |
|        - |  1192 | `	"	return false;"\` |
|        - |  1193 | `	" }"\` |
|        - |  1194 | `	" return $a['gid'];"\` |
|        - |  1195 | `    "}"\` |
|        - |  1196 | `	 "function fileinode(string $file){"\` |
|        - |  1197 | `	" $a = stat($file);"\` |
|        - |  1198 | `	" if( !is_array($a) ){"\` |
|        - |  1199 | `	"	return false;"\` |
|        - |  1200 | `	" }"\` |
|        - |  1201 | `	" return $a['ino'];"\` |
|        - |  1202 | `    "}"` |
|        - |  1203 |  |
|        - |  1204 | `/*` |
|        - |  1205 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1206 | ` * start compiling the target PHP program.` |
|        - |  1207 | ` */` |
|     1910 |  1208 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1209 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1210 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1211 | `	 )` |
|        2 |  1212 |  |
|        - |  1213 | `	SyString sBuiltin;` |
|        - |  1214 | `	ph7_value *pObj;` |
|        - |  1215 | `	sxi32 rc;` |
|        - |  1216 | `	/* Zero the structure */` |
|     1912 |  1217 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1218 | `	/* Initialize VM fields */` |
|     1912 |  1219 | `	pVm->pEngine = &(*pEngine);` |
|     1912 |  1220 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1221 | `	/* Instructions containers */` |
|     1912 |  1222 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     1912 |  1223 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     1912 |  1224 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1225 | `	/* Object containers */` |
|     1912 |  1226 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1912 |  1227 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1228 | `	/* Virtual machine internal containers */` |
|     1912 |  1229 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     1912 |  1230 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     1912 |  1231 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     1912 |  1232 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1912 |  1233 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     1912 |  1234 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     1912 |  1235 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     1912 |  1236 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     1912 |  1237 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     1912 |  1238 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     1912 |  1239 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     1912 |  1240 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     1912 |  1241 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     1912 |  1242 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     1912 |  1243 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|        - |  1244 | `	/* Configuration containers */` |
|     1912 |  1245 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     1912 |  1246 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     1912 |  1247 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     1912 |  1248 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     1912 |  1249 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1250 | `	/* Error callbacks containers */` |
|     1912 |  1251 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     1912 |  1252 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     1912 |  1253 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     1912 |  1254 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     1912 |  1255 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1256 | `	/* Set a default recursion limit */` |
|        - |  1257 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     1912 |  1258 | `	pVm->nMaxDepth = 32;` |
|        - |  1259 | `#else` |
|        - |  1260 | `	pVm->nMaxDepth = 16;` |
|        - |  1261 | `#endif` |
|        - |  1262 | `	/* Default assertion flags */` |
|     1912 |  1263 | `	pVm->iAssertFlags = PH7_ASSERT_WARNING; /* Issue a warning for each failed assertion */` |
|        - |  1264 | `	/* JSON return status */` |
|     1912 |  1265 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1266 | `	/* PRNG context */` |
|     1912 |  1267 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1268 | `	/* Install the null constant */` |
|     1912 |  1269 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1912 |  1270 | `	if( pObj == 0 ){` |
|      ! 0 |  1271 | `		rc = SXERR_MEM;` |
|      ! 0 |  1272 | `		goto Err;` |
|        - |  1273 | `	}` |
|     1912 |  1274 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1275 | `	/* Install the boolean TRUE constant */` |
|     1912 |  1276 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1912 |  1277 | `	if( pObj == 0 ){` |
|      ! 0 |  1278 | `		rc = SXERR_MEM;` |
|      ! 0 |  1279 | `		goto Err;` |
|        - |  1280 | `	}` |
|     1912 |  1281 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1282 | `	/* Install the boolean FALSE constant */` |
|     1912 |  1283 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1912 |  1284 | `	if( pObj == 0 ){` |
|      ! 0 |  1285 | `		rc = SXERR_MEM;` |
|      ! 0 |  1286 | `		goto Err;` |
|        - |  1287 | `	}` |
|     1912 |  1288 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1289 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1290 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1291 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     1912 |  1292 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     1912 |  1293 | `	if( pObj == 0 ){` |
|      ! 0 |  1294 | `		rc = SXERR_MEM;` |
|      ! 0 |  1295 | `		goto Err;` |
|        - |  1296 | `	}` |
|     1912 |  1297 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1298 | `	/* Create the global frame */` |
|     1912 |  1299 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     1912 |  1300 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1301 | `		goto Err;` |
|        - |  1302 | `	}` |
|        - |  1303 | `	/* Initialize the code generator */` |
|     1912 |  1304 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1912 |  1305 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1306 | `		goto Err;` |
|        - |  1307 | `	}` |
|        - |  1308 | `	/* VM correctly initialized,set the magic number */` |
|     1912 |  1309 | `	pVm->nMagic = PH7_VM_INIT;` |
|     1912 |  1310 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1311 | `	/* Compile the built-in library */` |
|     1912 |  1312 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1313 | `	/* Reset the code generator */` |
|     1912 |  1314 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1912 |  1315 | `	return SXRET_OK;` |
|      ! 0 |  1316 | `Err:` |
|      ! 0 |  1317 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1318 | `	return rc;` |
|      957 |  1319 |  |
|        - |  1320 | `/*` |
|        - |  1321 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1322 | ` * routine which store the output in an internal blob.` |
|        - |  1323 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1324 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1325 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1326 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1327 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1328 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1329 | ` * to finish executing and extracting the output.` |
|        - |  1330 | ` */` |
|      ! 0 |  1331 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1332 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1333 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1334 | `	void *pUserData     /* User private data */` |
|        - |  1335 | `	)` |
|      ! 0 |  1336 |  |
|        - |  1337 | `	 sxi32 rc;` |
|        - |  1338 | `	 /* Store the output in an internal BLOB */` |
|      ! 0 |  1339 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|      ! 0 |  1340 | `	 return rc;` |
|      ! 0 |  1341 |  |
|        - |  1342 | `#define VM_STACK_GUARD 16` |
|        - |  1343 | `/*` |
|        - |  1344 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1345 | ` * our compiled PHP program.` |
|        - |  1346 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1347 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1348 | ` */` |
|    26958 |  1349 | `static ph7_value * VmNewOperandStack(` |
|        - |  1350 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1351 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1352 | `	)` |
|        2 |  1353 |  |
|        - |  1354 | `	ph7_value *pStack;` |
|        - |  1355 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1356 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1357 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1358 | `  ** on the maximum stack depth required.` |
|        - |  1359 | `  **` |
|        - |  1360 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1361 | `  */` |
|    26960 |  1362 | `	nInstr += VM_STACK_GUARD;` |
|    26960 |  1363 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    26960 |  1364 | `	if( pStack == 0 ){` |
|      ! 0 |  1365 | `		return 0;` |
|        - |  1366 | `	}` |
|        - |  1367 | `	/* Initialize the operand stack */` |
|  1720202 |  1368 | `	while( nInstr > 0 ){` |
|  1693244 |  1369 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1693244 |  1370 | `		--nInstr;` |
|        2 |  1371 | `	}` |
|        - |  1372 | `	/* Ready for bytecode execution */` |
|    26960 |  1373 | `	return pStack;` |
|    13481 |  1374 |  |
|        - |  1375 | `/* Forward declaration */` |
|        - |  1376 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1377 | `static int VmInstanceOf(ph7_class *pThis,ph7_class *pClass);` |
|        - |  1378 | `static int VmClassMemberAccess(ph7_vm *pVm,ph7_class *pClass,const SyString *pAttrName,sxi32 iProtection,int bLog);` |
|        - |  1379 | `/*` |
|        - |  1380 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1381 | ` * This routine gets called by the PH7 engine after` |
|        - |  1382 | ` * successful compilation of the target PHP program.` |
|        - |  1383 | ` */` |
|     1672 |  1384 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1385 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1386 | `	)` |
|        2 |  1387 |  |
|        - |  1388 | `	SyHashEntry *pEntry;` |
|        - |  1389 | `	sxi32 rc;` |
|     1674 |  1390 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1391 | `		/* Initialize your VM first */` |
|      ! 0 |  1392 | `		return SXERR_CORRUPT;` |
|        - |  1393 | `	}` |
|        - |  1394 | `	/* Mark the VM ready for byte-code execution */` |
|     1674 |  1395 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1396 | `	/* Release the code generator now we have compiled our program */` |
|     1674 |  1397 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1398 | `	/* Emit the DONE instruction */` |
|     1674 |  1399 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     1674 |  1400 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1401 | `		return SXERR_MEM;` |
|        - |  1402 | `	}` |
|        - |  1403 | `	/* Script return value */` |
|     1674 |  1404 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1405 | `	/* Allocate a new operand stack */` |
|     1674 |  1406 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     1674 |  1407 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1408 | `		return SXERR_MEM;` |
|        - |  1409 | `	}` |
|        - |  1410 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1411 | `	 * private data. */` |
|     1674 |  1412 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     1674 |  1413 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1414 | `	/* Allocate the reference table */` |
|     1674 |  1415 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     1674 |  1416 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     1674 |  1417 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1418 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1419 | `		return SXERR_MEM;` |
|        - |  1420 | `	}` |
|        - |  1421 | `	/* Zero the reference table */` |
|     1674 |  1422 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1423 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     1674 |  1424 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     1674 |  1425 | `	if( rc != SXRET_OK ){` |
|        - |  1426 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1427 | `		return rc;` |
|        - |  1428 | `	}` |
|        - |  1429 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     1674 |  1430 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     1674 |  1431 | `	if( rc != SXRET_OK ){` |
|        - |  1432 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1433 | `		return rc;` |
|        - |  1434 | `	}` |
|        - |  1435 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     1674 |  1436 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1437 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     1674 |  1438 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1439 | `	/* Initialize and install static and constants class attributes */` |
|     1674 |  1440 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    20096 |  1441 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    18424 |  1442 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    18424 |  1443 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1444 | `			return rc;` |
|        - |  1445 | `		}` |
|        2 |  1446 | `	}` |
|        - |  1447 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     1674 |  1448 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1449 | `	/* VM is ready for bytecode execution */` |
|     1674 |  1450 | `	return SXRET_OK;` |
|      838 |  1451 |  |
|        - |  1452 | `/*` |
|        - |  1453 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1454 | ` */` |
|      ! 0 |  1455 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1456 |  |
|      ! 0 |  1457 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1458 | `		return SXERR_CORRUPT;` |
|        - |  1459 | `	}` |
|        - |  1460 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1461 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1462 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1463 | `	/* Set the ready flag */` |
|      ! 0 |  1464 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1465 | `	return SXRET_OK;` |
|      ! 0 |  1466 |  |
|        - |  1467 | `/*` |
|        - |  1468 | ` * Release a Virtual Machine.` |
|        - |  1469 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1470 | ` */` |
|     1664 |  1471 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1472 |  |
|        - |  1473 | `	/* Set the stale magic number */` |
|     1666 |  1474 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1475 | `	/* Release the private memory subsystem */` |
|     1666 |  1476 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     1666 |  1477 | `	return SXRET_OK;` |
|        2 |  1478 |  |
|        - |  1479 | `/*` |
|        - |  1480 | ` * Initialize a foreign function call context.` |
|        - |  1481 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1482 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1483 | ` * functions.` |
|        - |  1484 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1485 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1486 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1487 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1488 | ` */` |
|   527968 |  1489 | `static sxi32 VmInitCallContext(` |
|        - |  1490 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1491 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1492 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1493 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1494 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1495 | `	)` |
|        2 |  1496 |  |
|   527970 |  1497 | `	pOut->pFunc = pFunc;` |
|   527970 |  1498 | `	pOut->pVm   = pVm;` |
|   527970 |  1499 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   527970 |  1500 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1501 | `	/* Assume a null return value */` |
|   527970 |  1502 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   527970 |  1503 | `	pOut->pRet = pRet;` |
|   527970 |  1504 | `	pOut->iFlags = iFlags;` |
|   527970 |  1505 | `	return SXRET_OK;` |
|        2 |  1506 |  |
|        - |  1507 | `/*` |
|        - |  1508 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1509 | ` * left behind.` |
|        - |  1510 | ` */` |
|   527968 |  1511 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1512 |  |
|        - |  1513 | `	sxu32 n;` |
|   527970 |  1514 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6164 |  1515 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    17420 |  1516 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    11258 |  1517 | `			if( apObj[n] == 0 ){` |
|        - |  1518 | `				/* Already released */` |
|      250 |  1519 | `				continue;` |
|        - |  1520 | `			}` |
|    11010 |  1521 | `			PH7_MemObjRelease(apObj[n]);` |
|    11010 |  1522 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     5506 |  1523 | `		}` |
|     6164 |  1524 | `		SySetRelease(&pCtx->sVar);` |
|     3081 |  1525 | `	}` |
|   527970 |  1526 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1527 | `		ph7_aux_data *aAux;` |
|        - |  1528 | `		void *pChunk;` |
|        - |  1529 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1530 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1531 | `		 */` |
|        9 |  1532 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1533 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1534 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1535 | `			/* Release the chunk */` |
|       25 |  1536 | `			if( pChunk ){` |
|       25 |  1537 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1538 | `			}` |
|       13 |  1539 | `		}` |
|        9 |  1540 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1541 | `	}` |
|   527970 |  1542 |  |
|        - |  1543 | `/*` |
|        - |  1544 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1545 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1546 | ` */` |
|      248 |  1547 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1548 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1549 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1550 | `	)` |
|        2 |  1551 |  |
|      250 |  1552 | `	if( pValue == 0 ){` |
|        - |  1553 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1554 | `		return;` |
|        - |  1555 | `	}` |
|      250 |  1556 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      250 |  1557 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1558 | `		sxu32 n;` |
|      936 |  1559 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      936 |  1560 | `			if( apObj[n] == pValue ){` |
|      250 |  1561 | `				PH7_MemObjRelease(pValue);` |
|      250 |  1562 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1563 | `				/* Mark as released */` |
|      250 |  1564 | `				apObj[n] = 0;` |
|      250 |  1565 | `				break;` |
|        - |  1566 | `			}` |
|      345 |  1567 | `		}` |
|      124 |  1568 | `	}` |
|      126 |  1569 |  |
|        - |  1570 | `/*` |
|        - |  1571 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1572 | ` */` |
|  3141328 |  1573 | `static void VmPopOperand(` |
|        - |  1574 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1575 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1576 | `	)` |
|        2 |  1577 |  |
|  3141330 |  1578 | `	ph7_value *pTos = *ppTos;` |
|  6654662 |  1579 | `	while( nPop > 0 ){` |
|  3513334 |  1580 | `		PH7_MemObjRelease(pTos);` |
|  3513334 |  1581 | `		pTos--;` |
|  3513334 |  1582 | `		nPop--;` |
|        2 |  1583 | `	}` |
|        - |  1584 | `	/* Top of the stack */` |
|  3141330 |  1585 | `	*ppTos = pTos;` |
|  3141330 |  1586 |  |
|        - |  1587 | `/*` |
|        - |  1588 | ` * Reserve a memory object.` |
|        - |  1589 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1590 | ` */` |
|  2940358 |  1591 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1592 |  |
|  2940360 |  1593 | `	ph7_value *pObj = 0;` |
|        - |  1594 | `	VmSlot *pSlot;` |
|        - |  1595 | `	sxu32 nIdx;` |
|        - |  1596 | `	/* Check for a free slot */` |
|  2940360 |  1597 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  2940360 |  1598 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  2940360 |  1599 | `	if( pSlot ){` |
|   810736 |  1600 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   810736 |  1601 | `		nIdx = pSlot->nIdx;` |
|   405367 |  1602 | `	}` |
|  2940360 |  1603 | `	if( pObj == 0 ){` |
|        - |  1604 | `		/* Reserve a new memory object */` |
|  2129626 |  1605 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2129626 |  1606 | `		if( pObj == 0 ){` |
|      ! 0 |  1607 | `			return 0;` |
|        - |  1608 | `		}` |
|  1064812 |  1609 | `	}` |
|        - |  1610 | `	/* Set a null default value */` |
|  2940360 |  1611 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  2940360 |  1612 | `	pObj->nIdx = nIdx;` |
|  2940360 |  1613 | `	return pObj;` |
|  1470181 |  1614 |  |
|        - |  1615 | `/*` |
|        - |  1616 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1617 | ` */` |
|    22116 |  1618 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1619 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1620 | `	const char *zKey,  /* Entry key */` |
|        - |  1621 | `	sxu32 nByte,       /* Key length */` |
|        - |  1622 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1623 | `	)` |
|        2 |  1624 |  |
|        - |  1625 | `	ph7_value sKey;` |
|        - |  1626 | `	sxi32 rc;` |
|    22118 |  1627 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    22118 |  1628 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1629 | `	/* Perform the insertion */` |
|    22118 |  1630 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    22118 |  1631 | `	PH7_MemObjRelease(&sKey);` |
|    22118 |  1632 | `	return rc;` |
|        2 |  1633 |  |
|        - |  1634 | `/*` |
|        - |  1635 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1636 | ` * Return a pointer to the variable value on success.` |
|        - |  1637 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1638 | ` */` |
|  2957242 |  1639 | `static ph7_value * VmExtractMemObj(` |
|        - |  1640 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1641 | `	const SyString *pName, /* Variable name */` |
|        - |  1642 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1643 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1644 | `	)` |
|        2 |  1645 |  |
|  2957244 |  1646 | `	int bNullify = FALSE;` |
|        - |  1647 | `	SyHashEntry *pEntry;` |
|        - |  1648 | `	VmFrame *pFrame;` |
|        - |  1649 | `	ph7_value *pObj;` |
|        - |  1650 | `	sxu32 nIdx;` |
|        - |  1651 | `	sxi32 rc;` |
|        - |  1652 | `	/* Point to the top active frame */` |
|  2957244 |  1653 | `	pFrame = pVm->pFrame;` |
|  3006596 |  1654 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1655 | `		/* Safely ignore the exception frame */` |
|    49353 |  1656 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        1 |  1657 | `	}` |
|        - |  1658 | `	/* Perform the lookup */` |
|  2957244 |  1659 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1660 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1661 | `		pName = &sAnnon;` |
|        - |  1662 | `		/* Always nullify the object */` |
|      ! 0 |  1663 | `		bNullify = TRUE;` |
|      ! 0 |  1664 | `		bDup = FALSE;` |
|      ! 0 |  1665 | `	}` |
|        - |  1666 | `	/* Check the superglobals table first */` |
|  2957244 |  1667 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  2957244 |  1668 | `	if( pEntry == 0 ){` |
|        - |  1669 | `		/* Query the top active frame */` |
|  2957208 |  1670 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  2957208 |  1671 | `		if( pEntry == 0 ){` |
|    72972 |  1672 | `			char *zName = (char *)pName->zString;` |
|        - |  1673 | `			VmSlot sLocal;` |
|    72972 |  1674 | `			if( !bCreate ){` |
|        - |  1675 | `				/* Do not create the variable,return NULL instead */` |
|      576 |  1676 | `				return 0;` |
|        - |  1677 | `			}` |
|        - |  1678 | `			/* No such variable,automatically create a new one and install` |
|        - |  1679 | `			 * it in the current frame.` |
|        - |  1680 | `			 */` |
|    72398 |  1681 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    72398 |  1682 | `			if( pObj == 0 ){` |
|      ! 0 |  1683 | `				return 0;` |
|        - |  1684 | `			}` |
|    72398 |  1685 | `			nIdx = pObj->nIdx;` |
|    72398 |  1686 | `			if( bDup ){` |
|        - |  1687 | `				/* Duplicate name */` |
|      132 |  1688 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      132 |  1689 | `				if( zName == 0 ){` |
|      ! 0 |  1690 | `					return 0;` |
|        - |  1691 | `				}` |
|       65 |  1692 | `			}` |
|        - |  1693 | `			/* Link to the top active VM frame */` |
|    72398 |  1694 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    72398 |  1695 | `			if( rc != SXRET_OK ){` |
|        - |  1696 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1697 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1698 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1699 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1700 | `				return 0;` |
|        - |  1701 | `			}` |
|    72398 |  1702 | `			if( pFrame->pParent != 0 ){` |
|        - |  1703 | `				/* Local variable */` |
|    67026 |  1704 | `				sLocal.nIdx = nIdx;` |
|    67026 |  1705 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    33514 |  1706 | `			}else{` |
|        - |  1707 | `				/* Register in the $GLOBALS array */` |
|     5374 |  1708 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1709 | `			}` |
|        - |  1710 | `			/* Install in the reference table */` |
|    72398 |  1711 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1712 | `			/* Save object index */` |
|    72398 |  1713 | `			pObj->nIdx = nIdx;` |
|    36200 |  1714 | `		}else{` |
|        - |  1715 | `			/* Extract variable contents */` |
|  2884238 |  1716 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  2884238 |  1717 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2884238 |  1718 | `			if( bNullify && pObj ){` |
|      ! 0 |  1719 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1720 | `			}` |
|        - |  1721 | `		}` |
|  1478428 |  1722 | `	}else{` |
|        - |  1723 | `		/* Superglobal */` |
|       38 |  1724 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1725 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1726 | `	}` |
|  2956670 |  1727 | `	return pObj;` |
|  1478733 |  1728 |  |
|        - |  1729 | `/*` |
|        - |  1730 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1731 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1732 | ` */` |
|     1698 |  1733 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1734 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1735 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1736 | `	sxu32 nByte        /* zName length */` |
|        - |  1737 | `	)` |
|        2 |  1738 |  |
|        - |  1739 | `	SyHashEntry *pEntry;` |
|        - |  1740 | `	ph7_value *pValue;` |
|        - |  1741 | `	sxu32 nIdx;` |
|        - |  1742 | `	/* Query the superglobal table */` |
|     1700 |  1743 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     1700 |  1744 | `	if( pEntry == 0 ){` |
|        - |  1745 | `		/* No such entry */` |
|      ! 0 |  1746 | `		return 0;` |
|        - |  1747 | `	}` |
|        - |  1748 | `	/* Extract the superglobal index in the global object pool */` |
|     1700 |  1749 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1750 | `	/* Extract the variable value  */` |
|     1700 |  1751 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     1700 |  1752 | `	return pValue;` |
|      851 |  1753 |  |
|        - |  1754 | `/*` |
|        - |  1755 | ` * Perform a raw hashmap insertion.` |
|        - |  1756 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1757 | ` */` |
|     1696 |  1758 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1759 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1760 | `	const char *zKey,   /* Entry key */` |
|        - |  1761 | `	int nKeylen,        /* zKey length*/` |
|        - |  1762 | `	const char *zData,  /* Entry data */` |
|        - |  1763 | `	int nLen            /* zData length */` |
|        - |  1764 | `	)` |
|        2 |  1765 |  |
|        - |  1766 | `	ph7_value sKey,sValue;` |
|        - |  1767 | `	sxi32 rc;` |
|     1698 |  1768 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     1698 |  1769 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     1698 |  1770 | `	if( zKey ){` |
|     1676 |  1771 | `		if( nKeylen < 0 ){` |
|     1676 |  1772 | `			nKeylen = (int)SyStrlen(zKey);` |
|      837 |  1773 | `		}` |
|     1676 |  1774 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|      837 |  1775 | `	}` |
|     1698 |  1776 | `	if( zData ){` |
|     1698 |  1777 | `		if( nLen < 0 ){` |
|        - |  1778 | `			/* Compute length automatically */` |
|      ! 0 |  1779 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1780 | `		}` |
|     1698 |  1781 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|      848 |  1782 | `	}` |
|        - |  1783 | `	/* Perform the insertion */` |
|     1698 |  1784 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     1698 |  1785 | `	PH7_MemObjRelease(&sKey);` |
|     1698 |  1786 | `	PH7_MemObjRelease(&sValue);` |
|     1698 |  1787 | `	return rc;` |
|        2 |  1788 |  |
|        - |  1789 | `/*` |
|        - |  1790 | ` * Configure a working virtual machine instance.` |
|        - |  1791 | ` *` |
|        - |  1792 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1793 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1794 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1795 | ` * The second argument to this function is an integer configuration option` |
|        - |  1796 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1797 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1798 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1799 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1800 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1801 | ` */` |
|    26776 |  1802 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1803 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1804 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1805 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1806 | `	)` |
|        2 |  1807 |  |
|    26778 |  1808 | `	sxi32 rc = SXRET_OK;` |
|    26778 |  1809 | `	switch(nOp){` |
|      836 |  1810 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     1674 |  1811 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     1674 |  1812 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1813 | `		/* VM output consumer callback */` |
|        - |  1814 | `#ifdef UNTRUST` |
|        - |  1815 | `		if( xConsumer == 0 ){` |
|        - |  1816 | `			rc = SXERR_CORRUPT;` |
|        - |  1817 | `			break;` |
|        - |  1818 | `		}` |
|        - |  1819 | `#endif` |
|        - |  1820 | `		/* Install the output consumer */` |
|     1674 |  1821 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     1674 |  1822 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     1674 |  1823 | `		break;` |
|        - |  1824 | `							   }` |
|      836 |  1825 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1826 | `		/* Import path */` |
|        - |  1827 | `		  const char *zPath;` |
|        - |  1828 | `		  SyString sPath;` |
|     1674 |  1829 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1830 | `#if defined(UNTRUST)` |
|        - |  1831 | `		  if( zPath == 0 ){` |
|        - |  1832 | `			  rc = SXERR_EMPTY;` |
|        - |  1833 | `			  break;` |
|        - |  1834 | `		  }` |
|        - |  1835 | `#endif` |
|     1674 |  1836 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1837 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1838 | `#ifdef __WINNT__` |
|        2 |  1839 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1840 | `#endif` |
|     3346 |  1841 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1842 | `		  /* Remove leading and trailing white spaces */` |
|     1674 |  1843 | `		  SyStringFullTrim(&sPath);` |
|     1674 |  1844 | `		  if( sPath.nByte > 0 ){` |
|        - |  1845 | `			  /* Store the path in the corresponding conatiner */` |
|     1674 |  1846 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      836 |  1847 | `		  }` |
|     1674 |  1848 | `		  break;` |
|        - |  1849 | `									 }` |
|      836 |  1850 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1851 | `		/* Run-Time Error report */` |
|     1674 |  1852 | `		pVm->bErrReport = 1;` |
|     1674 |  1853 | `		break;` |
|      ! 0 |  1854 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1855 | `		/* Recursion depth */` |
|      ! 0 |  1856 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1857 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1858 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1859 | `		}` |
|      ! 0 |  1860 | `		break;` |
|        - |  1861 | `									   }` |
|      ! 0 |  1862 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1863 | `		/* VM output length in bytes */` |
|      ! 0 |  1864 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1865 | `#ifdef UNTRUST` |
|        - |  1866 | `		if( pOut == 0 ){` |
|        - |  1867 | `			rc = SXERR_CORRUPT;` |
|        - |  1868 | `			break;` |
|        - |  1869 | `		}` |
|        - |  1870 | `#endif` |
|      ! 0 |  1871 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1872 | `		break;` |
|        - |  1873 | `							   }` |
|        - |  1874 |  |
|     8360 |  1875 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1876 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1877 | `		/* Create a new superglobal/global variable */` |
|    16722 |  1878 | `		const char *zName = va_arg(ap,const char *);` |
|    16722 |  1879 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1880 | `		SyHashEntry *pEntry;` |
|        - |  1881 | `		ph7_value *pObj;` |
|        - |  1882 | `		sxu32 nByte;` |
|        - |  1883 | `		sxu32 nIdx;` |
|        - |  1884 | `#ifdef UNTRUST` |
|        - |  1885 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1886 | `			rc = SXERR_CORRUPT;` |
|        - |  1887 | `			break;` |
|        - |  1888 | `		}` |
|        - |  1889 | `#endif` |
|    16722 |  1890 | `		nByte = SyStrlen(zName);` |
|    16722 |  1891 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1892 | `			/* Check if the superglobal is already installed */` |
|    16722 |  1893 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     8362 |  1894 | `		}else{` |
|        - |  1895 | `			/* Query the top active VM frame */` |
|      ! 0 |  1896 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1897 | `		}` |
|    16722 |  1898 | `		if( pEntry ){` |
|        - |  1899 | `			/* Variable already installed */` |
|      ! 0 |  1900 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1901 | `			/* Extract contents */` |
|      ! 0 |  1902 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  1903 | `			if( pObj ){` |
|        - |  1904 | `				/* Overwrite old contents */` |
|      ! 0 |  1905 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  1906 | `			}` |
|      ! 0 |  1907 | `		}else{` |
|        - |  1908 | `			/* Install a new variable */` |
|    16722 |  1909 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    16722 |  1910 | `			if( pObj == 0 ){` |
|      ! 0 |  1911 | `				rc = SXERR_MEM;` |
|      ! 0 |  1912 | `				break;` |
|        - |  1913 | `			}` |
|    16722 |  1914 | `			nIdx = pObj->nIdx;` |
|        - |  1915 | `			/* Copy value */` |
|    16722 |  1916 | `			PH7_MemObjStore(pValue,pObj);` |
|    16722 |  1917 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1918 | `				/* Install the superglobal */` |
|    16722 |  1919 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     8362 |  1920 | `			}else{` |
|        - |  1921 | `				/* Install in the current frame */` |
|      ! 0 |  1922 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1923 | `			}` |
|    16722 |  1924 | `			if( rc == SXRET_OK ){` |
|        - |  1925 | `				SyHashEntry *pRef;` |
|    16722 |  1926 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    16722 |  1927 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     8362 |  1928 | `				}else{` |
|      ! 0 |  1929 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1930 | `				}` |
|        - |  1931 | `				/* Install in the reference table */` |
|    16722 |  1932 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    16722 |  1933 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1934 | `					/* Register in the $GLOBALS array */` |
|    16722 |  1935 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     8360 |  1936 | `				}` |
|     8360 |  1937 | `			}` |
|        - |  1938 | `		}` |
|    16722 |  1939 | `		break;` |
|        - |  1940 | `									}` |
|      837 |  1941 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1942 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1943 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1944 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1945 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1946 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1947 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     1676 |  1948 | `		const char *zKey   = va_arg(ap,const char *);` |
|     1676 |  1949 | `		const char *zValue = va_arg(ap,const char *);` |
|     1676 |  1950 | `		int nLen = va_arg(ap,int);` |
|        - |  1951 | `		ph7_hashmap *pMap;` |
|        - |  1952 | `		ph7_value *pValue;` |
|     1676 |  1953 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1954 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1955 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     1675 |  1956 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  1957 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  1958 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     1674 |  1959 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  1960 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  1961 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     1674 |  1962 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  1963 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  1964 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     1674 |  1965 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  1966 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  1967 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     1674 |  1968 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  1969 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  1970 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  1971 | `		}else{` |
|        - |  1972 | `			/* Extract the $_SERVER superglobal */` |
|     1674 |  1973 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  1974 | `		}` |
|     1676 |  1975 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1976 | `			/* No such entry */` |
|      ! 0 |  1977 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1978 | `			break;` |
|        - |  1979 | `		}` |
|        - |  1980 | `		/* Point to the hashmap */` |
|     1676 |  1981 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1982 | `		/* Perform the insertion */` |
|     1676 |  1983 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     1676 |  1984 | `		break;` |
|        - |  1985 | `								   }` |
|       11 |  1986 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  1987 | `		/* Script arguments */` |
|       24 |  1988 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  1989 | `		ph7_hashmap *pMap;` |
|        - |  1990 | `		ph7_value *pValue;` |
|        - |  1991 | `		sxu32 n;` |
|       24 |  1992 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  1993 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  1994 | `			break;` |
|        - |  1995 | `		}` |
|        - |  1996 | `		/* Extract the $argv array */` |
|       24 |  1997 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  1998 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1999 | `			/* No such entry */` |
|      ! 0 |  2000 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2001 | `			break;` |
|        - |  2002 | `		}` |
|        - |  2003 | `		/* Point to the hashmap */` |
|       24 |  2004 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2005 | `		/* Perform the insertion */` |
|       24 |  2006 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2007 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2008 | `		if( rc == SXRET_OK ){` |
|       24 |  2009 | `			if( pMap->nEntry > 1 ){` |
|        - |  2010 | `				/* Append space separator first */` |
|       18 |  2011 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2012 | `			}` |
|       24 |  2013 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2014 | `		}` |
|       24 |  2015 | `		break;` |
|        - |  2016 | `								  }` |
|      ! 0 |  2017 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2018 | `		/* error_log() consumer */` |
|      ! 0 |  2019 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2020 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2021 | `		break;` |
|        - |  2022 | `										}` |
|      ! 0 |  2023 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2024 | `		/* Script return value */` |
|      ! 0 |  2025 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2026 | `#ifdef UNTRUST` |
|        - |  2027 | `		if( ppValue == 0 ){` |
|        - |  2028 | `			rc = SXERR_CORRUPT;` |
|        - |  2029 | `			break;` |
|        - |  2030 | `		}` |
|        - |  2031 | `#endif` |
|      ! 0 |  2032 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2033 | `		break;` |
|        - |  2034 | `								   }` |
|     1672 |  2035 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2036 | `		/* Register an IO stream device */` |
|     3346 |  2037 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2038 | `		/* Make sure we are dealing with a valid IO stream */` |
|     5016 |  2039 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     3346 |  2040 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2041 | `				/* Invalid stream */` |
|      ! 0 |  2042 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2043 | `				break;` |
|        - |  2044 | `		}` |
|     3346 |  2045 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2046 | `			/* Make the 'file://' stream the defaut stream device */` |
|     1674 |  2047 | `			pVm->pDefStream = pStream;` |
|      836 |  2048 | `		}` |
|        - |  2049 | `		/* Insert in the appropriate container */` |
|     3346 |  2050 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     3346 |  2051 | `		break;` |
|        - |  2052 | `								  }` |
|      ! 0 |  2053 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2054 | `		/* Point to the VM internal output consumer buffer */` |
|      ! 0 |  2055 | `		const void **ppOut = va_arg(ap,const void **);` |
|      ! 0 |  2056 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2057 | `#ifdef UNTRUST` |
|        - |  2058 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2059 | `			rc = SXERR_CORRUPT;` |
|        - |  2060 | `			break;` |
|        - |  2061 | `		}` |
|        - |  2062 | `#endif` |
|      ! 0 |  2063 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|      ! 0 |  2064 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|      ! 0 |  2065 | `		break;` |
|        - |  2066 | `									   }` |
|      ! 0 |  2067 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2068 | `		/* Raw HTTP request*/` |
|      ! 0 |  2069 | `		const char *zRequest = va_arg(ap,const char *);` |
|      ! 0 |  2070 | `		int nByte = va_arg(ap,int);` |
|      ! 0 |  2071 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2072 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2073 | `			break;` |
|        - |  2074 | `		}` |
|      ! 0 |  2075 | `		if( nByte < 0 ){` |
|        - |  2076 | `			/* Compute length automatically */` |
|      ! 0 |  2077 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2078 | `		}` |
|        - |  2079 | `		/* Process the request */` |
|      ! 0 |  2080 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|      ! 0 |  2081 | `		break;` |
|        - |  2082 | `									}` |
|      ! 0 |  2083 | `	default:` |
|        - |  2084 | `		/* Unknown configuration option */` |
|      ! 0 |  2085 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2086 | `		break;` |
|        - |  2087 | `	}` |
|    26778 |  2088 | `	return rc;` |
|        2 |  2089 |  |
|        - |  2090 | `/* Forward declaration */` |
|        - |  2091 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2092 | `/*` |
|        - |  2093 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2094 | ` * format.` |
|        - |  2095 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2096 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2097 | ` * (STDOUT).` |
|        - |  2098 | ` */` |
|        2 |  2099 | `static sxi32 VmByteCodeDump(` |
|        - |  2100 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2101 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2102 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2103 | `	)` |
|        1 |  2104 |  |
|        - |  2105 | `	static const char zDump[] = {` |
|        - |  2106 | `		"====================================================\n"` |
|        - |  2107 | `		"PH7 VM Dump\n"` |
|        - |  2108 | `		"====================================================\n"` |
|        - |  2109 | `	};` |
|        - |  2110 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2111 | `	sxi32 rc = SXRET_OK;` |
|        - |  2112 | `	sxu32 n;` |
|        - |  2113 | `	/* Point to the PH7 instructions */` |
|        3 |  2114 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2115 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2116 | `	n = 0;` |
|        3 |  2117 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2118 | `	/* Dump instructions */` |
|        6 |  2119 | `	for(;;){` |
|       13 |  2120 | `		if( pInstr >= pEnd ){` |
|        - |  2121 | `			/* No more instructions */` |
|        3 |  2122 | `			break;` |
|        - |  2123 | `		}` |
|        - |  2124 | `		/* Format and call the consumer callback */` |
|       16 |  2125 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       10 |  2126 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       10 |  2127 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       11 |  2128 | `		if( rc != SXRET_OK ){` |
|        - |  2129 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2130 | `			return rc;` |
|        - |  2131 | `		}` |
|       11 |  2132 | `		++n;` |
|       11 |  2133 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2134 | `	}` |
|        3 |  2135 | `	return rc;` |
|        2 |  2136 |  |
|        - |  2137 | `/* Forward declaration */` |
|        - |  2138 | `static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData);` |
|        - |  2139 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2140 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2141 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2142 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2143 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2144 | `/*` |
|        - |  2145 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2146 | ` * consumer callback.` |
|        - |  2147 | ` */` |
|      436 |  2148 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2149 |  |
|      437 |  2150 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      437 |  2151 | `	sxi32 rc = SXRET_OK;` |
|        - |  2152 | `	/* Append a new line */` |
|        - |  2153 | `#ifdef __WINNT__` |
|        1 |  2154 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2155 | `#else` |
|      436 |  2156 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2157 | `#endif` |
|        - |  2158 | `	/* Invoke the output consumer callback */` |
|      437 |  2159 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      437 |  2160 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  2161 | `		/* Increment output length */` |
|      437 |  2162 | `		pVm->nOutputLen += SyBlobLength(pMsg);` |
|      218 |  2163 | `	}` |
|      437 |  2164 | `	return rc;` |
|        1 |  2165 |  |
|        - |  2166 | `/*` |
|        - |  2167 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2168 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2169 | ` * information.` |
|        - |  2170 | ` */` |
|      138 |  2171 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2172 |  |
|      140 |  2173 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2174 | `		ph7_value apArg[4];` |
|        - |  2175 | `		ph7_value *apArgPtr[4];` |
|        - |  2176 | `		ph7_value sResult;` |
|        - |  2177 | `		SyString sErr;` |
|        - |  2178 | `		/* Prepare arguments */` |
|       61 |  2179 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2180 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2181 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2182 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2183 | `		if( pFile ){` |
|       61 |  2184 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2185 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2186 | `		}else{` |
|      ! 0 |  2187 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2188 | `		}` |
|       61 |  2189 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2190 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2191 | `		/* Set up pointer array */` |
|       61 |  2192 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2193 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2194 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2195 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2196 | `		/* Call the handler */` |
|       61 |  2197 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2198 | `		/* Check return value */` |
|       61 |  2199 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2200 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2201 | `		}` |
|        - |  2202 | `		/* Release */` |
|       61 |  2203 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2204 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2205 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2206 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2207 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2208 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2209 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2210 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2211 | `	}` |
|        - |  2212 | `	/* No handler, always call error handler */` |
|       79 |  2213 | `	return TRUE;` |
|       71 |  2214 |  |
|      102 |  2215 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2216 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2217 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2218 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2219 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2220 | `	)` |
|        2 |  2221 |  |
|      104 |  2222 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2223 | `	SyString *pFile;` |
|        - |  2224 | `	char *zErr;` |
|      104 |  2225 | `	sxi32 rc = SXRET_OK;` |
|      104 |  2226 | `	if( !pVm->bErrReport ){` |
|        - |  2227 | `		/* Don't bother reporting errors */` |
|        3 |  2228 | `		return SXRET_OK;` |
|        - |  2229 | `	}` |
|        - |  2230 | `	/* Reset the working buffer */` |
|      102 |  2231 | `	SyBlobReset(pWorker);` |
|        - |  2232 | `	/* Peek the processed file if available */` |
|      102 |  2233 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      102 |  2234 | `	if( pFile ){` |
|        - |  2235 | `		/* Append file name */` |
|      102 |  2236 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|      102 |  2237 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       50 |  2238 | `	}` |
|        - |  2239 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2240 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2241 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2242 | `	 * E_DEPRECATED). */` |
|      102 |  2243 | `	zErr = "Error:  ";` |
|      102 |  2244 | `	switch(iErr){` |
|       21 |  2245 | `	case PH7_CTX_WARNING:` |
|       44 |  2246 | `		zErr = "Warning:  ";` |
|       44 |  2247 | `		break;` |
|        6 |  2248 | `	case PH7_CTX_NOTICE:` |
|       14 |  2249 | `		zErr = "Notice:  ";` |
|       12 |  2250 | `		break;` |
|       23 |  2251 | `	default:` |
|        - |  2252 | `		/* keep iErr unchanged */` |
|       46 |  2253 | `		break;` |
|        - |  2254 | `	}` |
|      102 |  2255 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|      102 |  2256 | `	if( pFuncName ){` |
|        - |  2257 | `		/* Append function name first */` |
|       29 |  2258 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       29 |  2259 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       14 |  2260 | `	}` |
|      102 |  2261 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2262 | `	/* Check for user error handler.  compute length of C string */` |
|      102 |  2263 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       53 |  2264 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       26 |  2265 | `	}` |
|      102 |  2266 | `	return rc;` |
|       53 |  2267 |  |
|        - |  2268 | `/*` |
|        - |  2269 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2270 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2271 | ` * information.` |
|        - |  2272 | ` */` |
|       38 |  2273 | `static sxi32 VmThrowErrorAp(` |
|        - |  2274 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2275 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2276 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2277 | `	const char *zFormat, /* Format message */` |
|        - |  2278 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2279 | `	)` |
|        2 |  2280 |  |
|       40 |  2281 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2282 | `	SyBlob sMsg;` |
|        - |  2283 | `	SyString *pFile;` |
|        - |  2284 | `	char *zErr;` |
|       40 |  2285 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2286 | `	if( !pVm->bErrReport ){` |
|        - |  2287 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2288 | `		return SXRET_OK;` |
|        - |  2289 | `	}` |
|        - |  2290 | `	/* Reset the working buffer */` |
|       40 |  2291 | `	SyBlobReset(pWorker);` |
|        - |  2292 | `	/* Peek the processed file if available */` |
|       40 |  2293 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2294 | `	if( pFile ){` |
|        - |  2295 | `		/* Append file name */` |
|       40 |  2296 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2297 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2298 | `	}` |
|        - |  2299 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2300 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2301 | `	 * the correct errno value. */` |
|       40 |  2302 | `	zErr = "Error:  ";` |
|       40 |  2303 | `	switch(iErr){` |
|        4 |  2304 | `	case PH7_CTX_WARNING:` |
|        9 |  2305 | `		zErr = "Warning:  ";` |
|        9 |  2306 | `		break;` |
|        3 |  2307 | `	case PH7_CTX_NOTICE:` |
|        7 |  2308 | `		zErr = "Notice:  ";` |
|        6 |  2309 | `		break;` |
|       12 |  2310 | `	default:` |
|        - |  2311 | `		/* do not change iErr */` |
|       24 |  2312 | `		break;` |
|        - |  2313 | `	}` |
|       40 |  2314 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2315 | `	if( pFuncName ){` |
|        - |  2316 | `		/* Append function name first */` |
|       26 |  2317 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2318 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2319 | `	}` |
|        - |  2320 | `	/* Format the raw message */` |
|       40 |  2321 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2322 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2323 | `	/* Check if a user error handler is installed */` |
|       40 |  2324 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2325 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2326 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2327 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2328 | `	}` |
|       40 |  2329 | `	SyBlobRelease(&sMsg);` |
|       40 |  2330 | `	return rc;` |
|       21 |  2331 |  |
|        - |  2332 | `/*` |
|        - |  2333 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2334 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2335 | ` * information.` |
|        - |  2336 | ` * ------------------------------------` |
|        - |  2337 | ` * Simple boring wrapper function.` |
|        - |  2338 | ` * ------------------------------------` |
|        - |  2339 | ` */` |
|       14 |  2340 | `static sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2341 |  |
|        - |  2342 | `	va_list ap;` |
|        - |  2343 | `	sxi32 rc;` |
|       15 |  2344 | `	va_start(ap,zFormat);` |
|       15 |  2345 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2346 | `	va_end(ap);` |
|       15 |  2347 | `	return rc;` |
|        1 |  2348 |  |
|        - |  2349 | `/*` |
|        - |  2350 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2351 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2352 | ` * information.` |
|        - |  2353 | ` * ------------------------------------` |
|        - |  2354 | ` * Simple boring wrapper function.` |
|        - |  2355 | ` * ------------------------------------` |
|        - |  2356 | ` */` |
|       24 |  2357 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2358 |  |
|        - |  2359 | `	sxi32 rc;` |
|       26 |  2360 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2361 | `	return rc;` |
|        2 |  2362 |  |
|        - |  2363 | `/*` |
|        - |  2364 | ` * Resolve function context from the current frame.` |
|        - |  2365 | ` */` |
|      712 |  2366 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2367 |  |
|        - |  2368 | `	VmFrame *pFrame;` |
|        - |  2369 | `	ph7_vm_func *pFunc;` |
|      713 |  2370 | `	*pzFuncName = 0;` |
|      713 |  2371 | `	*pnFuncLen = 0;` |
|      713 |  2372 | `	pFrame = pVm->pFrame;` |
|      713 |  2373 | `	if( pFrame == 0 ){` |
|      ! 0 |  2374 | `		return;` |
|        - |  2375 | `	}` |
|      713 |  2376 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  2377 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  2378 | `	}` |
|      713 |  2379 | `	if( pFrame->pParent == 0 ){` |
|      709 |  2380 | `		return;` |
|        - |  2381 | `	}` |
|        5 |  2382 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  2383 | `	if( pFunc == 0 ){` |
|      ! 0 |  2384 | `		return;` |
|        - |  2385 | `	}` |
|        5 |  2386 | `	*pzFuncName = pFunc->sName.zString;` |
|        5 |  2387 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      357 |  2388 |  |
|        - |  2389 | `/*` |
|        - |  2390 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2391 | ` */` |
|      358 |  2392 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2393 |  |
|        - |  2394 | `	SyBlob sOut;` |
|        - |  2395 | `	SyString *pFile;` |
|      359 |  2396 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2397 | `		return PH7_OK;` |
|        - |  2398 | `	}` |
|      359 |  2399 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2400 | `		zClass = "Exception";` |
|      ! 0 |  2401 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2402 | `	}` |
|      359 |  2403 | `	if( zMsg == 0 ){` |
|      ! 0 |  2404 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2405 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2406 | `	}` |
|      359 |  2407 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      355 |  2408 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      177 |  2409 | `	}` |
|      359 |  2410 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      359 |  2411 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      359 |  2412 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      359 |  2413 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      359 |  2414 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      359 |  2415 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      359 |  2416 | `	if( pFile ){` |
|      359 |  2417 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      359 |  2418 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      359 |  2419 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      179 |  2420 | `	}` |
|      359 |  2421 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      359 |  2422 | `	if( pFile ){` |
|      359 |  2423 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      359 |  2424 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      359 |  2425 | `		if( zFuncName && nFuncLen > 0 ){` |
|        5 |  2426 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        3 |  2427 | `		}else{` |
|      355 |  2428 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2429 | `		}` |
|      179 |  2430 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2431 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2432 | `	}else{` |
|      ! 0 |  2433 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2434 | `	}` |
|      359 |  2435 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      359 |  2436 | `	if( pFile ){` |
|      359 |  2437 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      359 |  2438 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      359 |  2439 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      359 |  2440 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      179 |  2441 | `	}` |
|      359 |  2442 | `	VmCallErrorHandler(pVm,&sOut);` |
|      359 |  2443 | `	SyBlobRelease(&sOut);` |
|      359 |  2444 | `	return PH7_ABORT;` |
|      180 |  2445 |  |
|        - |  2446 | `/*` |
|        - |  2447 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2448 | ` */` |
|      354 |  2449 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2450 |  |
|        - |  2451 | `	ph7_vm *pVm;` |
|        - |  2452 | `	ph7_class *pClass;` |
|        - |  2453 | `	ph7_class_instance *pThis;` |
|        - |  2454 | `	ph7_class_method *pCons;` |
|        - |  2455 | `	ph7_value sArg;` |
|        - |  2456 | `	ph7_value *apArg[1];` |
|        - |  2457 | `	SyBlob sMsg;` |
|        - |  2458 | `	SyString sMsgStr;` |
|        - |  2459 | `	VmFrame *pFrame;` |
|        - |  2460 | `	va_list ap;` |
|        - |  2461 | `	sxi32 rc;` |
|        - |  2462 |  |
|      356 |  2463 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2464 | `		return PH7_ABORT;` |
|        - |  2465 | `	}` |
|      356 |  2466 | `	pVm = pCtx->pVm;` |
|      356 |  2467 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2468 | `		zClass = "Error";` |
|      ! 0 |  2469 | `	}` |
|      356 |  2470 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      356 |  2471 | `	if( pClass == 0 ){` |
|      ! 0 |  2472 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2473 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2474 | `			zClass` |
|        - |  2475 | `			);` |
|        - |  2476 | `	}` |
|      356 |  2477 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      356 |  2478 | `	if( pThis == 0 ){` |
|      ! 0 |  2479 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2480 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2481 | `			);` |
|        - |  2482 | `	}` |
|        - |  2483 |  |
|      356 |  2484 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      356 |  2485 | `	va_start(ap,zFormat);` |
|      356 |  2486 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      356 |  2487 | `	va_end(ap);` |
|        - |  2488 |  |
|      356 |  2489 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      356 |  2490 | `	if( pCons ){` |
|      356 |  2491 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      356 |  2492 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      356 |  2493 | `		apArg[0] = &sArg;` |
|      356 |  2494 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      356 |  2495 | `		PH7_MemObjRelease(&sArg);` |
|      177 |  2496 | `	}` |
|      356 |  2497 | `	SyBlobRelease(&sMsg);` |
|        - |  2498 |  |
|      356 |  2499 | `	pFrame = pVm->pFrame;` |
|      356 |  2500 | `	if( pFrame ){` |
|      358 |  2501 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        3 |  2502 | `			pFrame = pFrame->pParent;` |
|        1 |  2503 | `		}` |
|      356 |  2504 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      177 |  2505 | `	}` |
|      356 |  2506 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      356 |  2507 | `	PH7_ClassInstanceUnref(pThis);` |
|      356 |  2508 | `	if( rc == SXERR_ABORT ){` |
|      353 |  2509 | `		return PH7_ABORT;` |
|        - |  2510 | `	}` |
|        3 |  2511 | `	return PH7_EXCEPTION;` |
|      179 |  2512 |  |
|        - |  2513 | `/*` |
|        - |  2514 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2515 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2516 | ` */` |
|      ! 0 |  2517 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2518 |  |
|        - |  2519 | `	ph7_vm *pVm;` |
|        - |  2520 | `	SyBlob sMsg;` |
|      ! 0 |  2521 | `	const char *zFuncName = 0;` |
|      ! 0 |  2522 | `	int nFuncLen = 0;` |
|        - |  2523 | `	va_list ap;` |
|        - |  2524 | `	sxi32 rc;` |
|        - |  2525 |  |
|      ! 0 |  2526 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2527 | `		return PH7_OK;` |
|        - |  2528 | `	}` |
|      ! 0 |  2529 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2530 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2531 | `		zClass = "Error";` |
|      ! 0 |  2532 | `	}` |
|        - |  2533 |  |
|      ! 0 |  2534 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2535 |  |
|      ! 0 |  2536 | `	va_start(ap,zFormat);` |
|      ! 0 |  2537 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2538 | `	va_end(ap);` |
|        - |  2539 |  |
|      ! 0 |  2540 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2541 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2542 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2543 | `	}` |
|      ! 0 |  2544 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2545 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2546 | `	}` |
|      ! 0 |  2547 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2548 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2549 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2550 | `	return rc;` |
|      ! 0 |  2551 |  |
|        - |  2552 | `/*` |
|        - |  2553 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2554 | ` *` |
|        - |  2555 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2556 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2557 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2558 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2559 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2560 | ` * then the program execution is halted.` |
|        - |  2561 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2562 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2563 | ` * or to reset the VM to it's initial state.` |
|        - |  2564 | ` */` |
|    26958 |  2565 | `static sxi32 VmByteCodeExec(` |
|        - |  2566 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2567 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2568 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2569 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2570 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2571 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2572 | `	int is_callback      /* TRUE if we are executing a callback */` |
|        - |  2573 | `	)` |
|        2 |  2574 |  |
|        - |  2575 | `	VmInstr *pInstr;` |
|        - |  2576 | `	ph7_value *pTos;` |
|        - |  2577 | `	SySet aArg;` |
|        - |  2578 | `	sxi32 pc;` |
|        - |  2579 | `	sxi32 rc;` |
|        - |  2580 | `	/* Argument container */` |
|    26960 |  2581 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    26960 |  2582 | `	if( nTos < 0 ){` |
|    25610 |  2583 | `		pTos = &pStack[-1];` |
|    12806 |  2584 | `	}else{` |
|     1352 |  2585 | `		pTos = &pStack[nTos];` |
|        - |  2586 | `	}` |
|    26960 |  2587 | `	pc = 0;` |
|        - |  2588 | `	/* Execute as much as we can */` |
|  4710173 |  2589 | `	for(;;){` |
|        - |  2590 | `		/* Fetch the instruction to execute */` |
|  9419644 |  2591 | `		pInstr = &aInstr[pc];` |
|  9419644 |  2592 | `		rc = SXRET_OK;` |
|        - |  2593 | `/*` |
|        - |  2594 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2595 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2596 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2597 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2598 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2599 | ` */` |
|  9419644 |  2600 | `		switch(pInstr->iOp){` |
|        - |  2601 | `/*` |
|        - |  2602 | ` * DONE: P1 * *` |
|        - |  2603 | ` *` |
|        - |  2604 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2605 | ` * and return immediately.` |
|        - |  2606 | ` */` |
|    13291 |  2607 | `case PH7_OP_DONE:` |
|    26584 |  2608 | `	if( pInstr->iP1 ){` |
|        - |  2609 | `#ifdef UNTRUST` |
|        - |  2610 | `		if( pTos < pStack ){` |
|        - |  2611 | `			goto Abort;` |
|        - |  2612 | `		}` |
|        - |  2613 | `#endif` |
|    15128 |  2614 | `		if( pLastRef ){` |
|     9900 |  2615 | `			*pLastRef = pTos->nIdx;` |
|     4949 |  2616 | `		}` |
|    15128 |  2617 | `		if( pResult ){` |
|        - |  2618 | `			/* Execution result */` |
|    14452 |  2619 | `			PH7_MemObjStore(pTos,pResult);` |
|     7225 |  2620 | `		}` |
|    15128 |  2621 | `		VmPopOperand(&pTos,1);` |
|    19021 |  2622 | `	}else if( pLastRef ){` |
|        - |  2623 | `		/* Nothing referenced */` |
|      762 |  2624 | `		*pLastRef = SXU32_HIGH;` |
|      380 |  2625 | `	}` |
|    26584 |  2626 | `	goto Done;` |
|        - |  2627 | `/*` |
|        - |  2628 | ` * HALT: P1 * *` |
|        - |  2629 | ` *` |
|        - |  2630 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2631 | ` * and abort immediately.` |
|        - |  2632 | ` */` |
|        4 |  2633 | `case PH7_OP_HALT:` |
|        9 |  2634 | `	if( pInstr->iP1 ){` |
|        - |  2635 | `#ifdef UNTRUST` |
|        - |  2636 | `		if( pTos < pStack ){` |
|        - |  2637 | `			goto Abort;` |
|        - |  2638 | `		}` |
|        - |  2639 | `#endif` |
|        9 |  2640 | `		if( pLastRef ){` |
|      ! 0 |  2641 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2642 | `		}` |
|        9 |  2643 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2644 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2645 | `				/* Output the exit message */` |
|        7 |  2646 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2647 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2648 | `				if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  2649 | `					/* Increment output length */` |
|        5 |  2650 | `					pVm->nOutputLen += SyBlobLength(&pTos->sBlob);` |
|        2 |  2651 | `				}` |
|        3 |  2652 | `			}` |
|        7 |  2653 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2654 | `			/* Record exit status */` |
|        5 |  2655 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2656 | `		}` |
|        9 |  2657 | `		VmPopOperand(&pTos,1);` |
|        4 |  2658 | `	}else if( pLastRef ){` |
|        - |  2659 | `		/* Nothing referenced */` |
|      ! 0 |  2660 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2661 | `	}` |
|        - |  2662 | `	/* Check if we're in an included file context */` |
|        9 |  2663 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2664 | `		/* Terminate the entire process */` |
|        9 |  2665 | `		exit(pVm->iExitStatus);` |
|        - |  2666 | `	}` |
|      ! 0 |  2667 | `	goto Abort;` |
|        - |  2668 | `/*` |
|        - |  2669 | ` * JMP: * P2 *` |
|        - |  2670 | ` *` |
|        - |  2671 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2672 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2673 | ` */` |
|   206992 |  2674 | `case PH7_OP_JMP:` |
|   414030 |  2675 | `	pc = pInstr->iP2 - 1;` |
|   414030 |  2676 | `	break;` |
|        - |  2677 | `/*` |
|        - |  2678 | ` * JZ: P1 P2 *` |
|        - |  2679 | ` *` |
|        - |  2680 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2681 | ` * entry in the stack if P1 is zero.` |
|        - |  2682 | ` */` |
|   478032 |  2683 | `case PH7_OP_JZ:` |
|        - |  2684 | `#ifdef UNTRUST` |
|        - |  2685 | `	if( pTos < pStack ){` |
|        - |  2686 | `		goto Abort;` |
|        - |  2687 | `	}` |
|        - |  2688 | `#endif` |
|        - |  2689 | `	/* Get a boolean value */` |
|   956154 |  2690 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       77 |  2691 | `		PH7_MemObjToBool(pTos);` |
|       38 |  2692 | `	}` |
|   956154 |  2693 | `	if( !pTos->x.iVal ){` |
|        - |  2694 | `		/* Take the jump */` |
|   458792 |  2695 | `		pc = pInstr->iP2 - 1;` |
|   229395 |  2696 | `	}` |
|   956154 |  2697 | `	if( !pInstr->iP1 ){` |
|   752614 |  2698 | `		VmPopOperand(&pTos,1);` |
|   376328 |  2699 | `	}` |
|   956154 |  2700 | `	break;` |
|        - |  2701 | `/*` |
|        - |  2702 | ` * JNZ: P1 P2 *` |
|        - |  2703 | ` *` |
|        - |  2704 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2705 | ` * entry in the stack if P1 is zero.` |
|        - |  2706 | ` */` |
|    51022 |  2707 | `case PH7_OP_JNZ:` |
|        - |  2708 | `#ifdef UNTRUST` |
|        - |  2709 | `	if( pTos < pStack ){` |
|        - |  2710 | `		goto Abort;` |
|        - |  2711 | `	}` |
|        - |  2712 | `#endif` |
|        - |  2713 | `	/* Get a boolean value */` |
|   102046 |  2714 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2715 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2716 | `	}` |
|   102046 |  2717 | `	if( pTos->x.iVal ){` |
|        - |  2718 | `		/* Take the jump */` |
|     3938 |  2719 | `		pc = pInstr->iP2 - 1;` |
|     1968 |  2720 | `	}` |
|   102046 |  2721 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2722 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2723 | `	}` |
|   102046 |  2724 | `	break;` |
|        - |  2725 | `/*` |
|        - |  2726 | ` * NOOP: * * *` |
|        - |  2727 | ` *` |
|        - |  2728 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2729 | ` * destination.` |
|        - |  2730 | ` */` |
|      ! 0 |  2731 | `case PH7_OP_NOOP:` |
|      ! 0 |  2732 | `	break;` |
|        - |  2733 | `/*` |
|        - |  2734 | ` * POP: P1 * *` |
|        - |  2735 | ` *` |
|        - |  2736 | ` * Pop P1 elements from the operand stack.` |
|        - |  2737 | ` */` |
|   368584 |  2738 | `case PH7_OP_POP: {` |
|   737214 |  2739 | `	sxi32 n = pInstr->iP1;` |
|   737214 |  2740 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2741 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2742 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2743 | `	}` |
|   737214 |  2744 | `	VmPopOperand(&pTos,n);` |
|   737214 |  2745 | `	break;` |
|        - |  2746 | `				 }` |
|        - |  2747 | `/*` |
|        - |  2748 | ` * CVT_INT: * * *` |
|        - |  2749 | ` *` |
|        - |  2750 | ` * Force the top of the stack to be an integer.` |
|        - |  2751 | ` */` |
|       35 |  2752 | `case PH7_OP_CVT_INT:` |
|        - |  2753 | `#ifdef UNTRUST` |
|        - |  2754 | `	if( pTos < pStack ){` |
|        - |  2755 | `		goto Abort;` |
|        - |  2756 | `	}` |
|        - |  2757 | `#endif` |
|       72 |  2758 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2759 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2760 | `	}` |
|        - |  2761 | `	/* Invalidate any prior representation */` |
|       72 |  2762 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2763 | `	break;` |
|        - |  2764 | `/*` |
|        - |  2765 | ` * CVT_REAL: * * *` |
|        - |  2766 | ` *` |
|        - |  2767 | ` * Force the top of the stack to be a real.` |
|        - |  2768 | ` */` |
|        4 |  2769 | `case PH7_OP_CVT_REAL:` |
|        - |  2770 | `#ifdef UNTRUST` |
|        - |  2771 | `	if( pTos < pStack ){` |
|        - |  2772 | `		goto Abort;` |
|        - |  2773 | `	}` |
|        - |  2774 | `#endif` |
|        9 |  2775 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2776 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2777 | `	}` |
|        - |  2778 | `	/* Invalidate any prior representation */` |
|        9 |  2779 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2780 | `	break;` |
|        - |  2781 | `/*` |
|        - |  2782 | ` * CVT_STR: * * *` |
|        - |  2783 | ` *` |
|        - |  2784 | ` * Force the top of the stack to be a string.` |
|        - |  2785 | ` */` |
|      136 |  2786 | `case PH7_OP_CVT_STR:` |
|        - |  2787 | `#ifdef UNTRUST` |
|        - |  2788 | `	if( pTos < pStack ){` |
|        - |  2789 | `		goto Abort;` |
|        - |  2790 | `	}` |
|        - |  2791 | `#endif` |
|      274 |  2792 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      274 |  2793 | `		PH7_MemObjToString(pTos);` |
|      136 |  2794 | `	}` |
|      274 |  2795 | `	break;` |
|        - |  2796 | `/*` |
|        - |  2797 | ` * CVT_BOOL: * * *` |
|        - |  2798 | ` *` |
|        - |  2799 | ` * Force the top of the stack to be a boolean.` |
|        - |  2800 | ` */` |
|        5 |  2801 | `case PH7_OP_CVT_BOOL:` |
|        - |  2802 | `#ifdef UNTRUST` |
|        - |  2803 | `	if( pTos < pStack ){` |
|        - |  2804 | `		goto Abort;` |
|        - |  2805 | `	}` |
|        - |  2806 | `#endif` |
|       11 |  2807 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2808 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2809 | `	}` |
|       11 |  2810 | `	break;` |
|        - |  2811 | `/*` |
|        - |  2812 | ` * CVT_NULL: * * *` |
|        - |  2813 | ` *` |
|        - |  2814 | ` * Nullify the top of the stack.` |
|        - |  2815 | ` */` |
|        3 |  2816 | `case PH7_OP_CVT_NULL:` |
|        - |  2817 | `#ifdef UNTRUST` |
|        - |  2818 | `	if( pTos < pStack ){` |
|        - |  2819 | `		goto Abort;` |
|        - |  2820 | `	}` |
|        - |  2821 | `#endif` |
|        7 |  2822 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2823 | `	break;` |
|        - |  2824 | `/*` |
|        - |  2825 | ` * CVT_NUMC: * * *` |
|        - |  2826 | ` *` |
|        - |  2827 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2828 | ` */` |
|      ! 0 |  2829 | `case PH7_OP_CVT_NUMC:` |
|        - |  2830 | `#ifdef UNTRUST` |
|        - |  2831 | `	if( pTos < pStack ){` |
|        - |  2832 | `		goto Abort;` |
|        - |  2833 | `	}` |
|        - |  2834 | `#endif` |
|        - |  2835 | `	/* Force a numeric cast */` |
|      ! 0 |  2836 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2837 | `	break;` |
|        - |  2838 | `/*` |
|        - |  2839 | ` * CVT_ARRAY: * * *` |
|        - |  2840 | ` *` |
|        - |  2841 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2842 | ` */` |
|       10 |  2843 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2844 | `#ifdef UNTRUST` |
|        - |  2845 | `	if( pTos < pStack ){` |
|        - |  2846 | `		goto Abort;` |
|        - |  2847 | `	}` |
|        - |  2848 | `#endif` |
|        - |  2849 | `	/* Force a hashmap cast */` |
|       21 |  2850 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  2851 | `	if( rc != SXRET_OK ){` |
|        - |  2852 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  2853 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  2854 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  2855 | `	}` |
|       21 |  2856 | `	break;` |
|        - |  2857 | `/*` |
|        - |  2858 | ` * CVT_OBJ: * * *` |
|        - |  2859 | ` *` |
|        - |  2860 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  2861 | ` */` |
|        8 |  2862 | `case PH7_OP_CVT_OBJ:` |
|        - |  2863 | `#ifdef UNTRUST` |
|        - |  2864 | `	if( pTos < pStack ){` |
|        - |  2865 | `		goto Abort;` |
|        - |  2866 | `	}` |
|        - |  2867 | `#endif` |
|       17 |  2868 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  2869 | `		/* Force a 'stdClass()' cast */` |
|       17 |  2870 | `		PH7_MemObjToObject(pTos);` |
|        8 |  2871 | `	}` |
|       17 |  2872 | `	break;` |
|        - |  2873 | `/*` |
|        - |  2874 | ` * ERR_CTRL * * *` |
|        - |  2875 | ` *` |
|        - |  2876 | ` * Error control operator.` |
|        - |  2877 | ` */` |
|    11428 |  2878 | `case PH7_OP_ERR_CTRL:` |
|        - |  2879 | `	/*` |
|        - |  2880 | `	 * TICKET 1433-038:` |
|        - |  2881 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2882 | `	 * use the public API,to control error output.` |
|        - |  2883 | `	 */` |
|    22856 |  2884 | `	break;` |
|        - |  2885 | `/*` |
|        - |  2886 | ` * IS_A * * *` |
|        - |  2887 | ` *` |
|        - |  2888 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  2889 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  2890 | ` * holding a class name or an object).` |
|        - |  2891 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  2892 | ` */` |
|       11 |  2893 | `case PH7_OP_IS_A:{` |
|       23 |  2894 | `	ph7_value *pNos = &pTos[-1];` |
|       23 |  2895 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  2896 | `#ifdef UNTRUST` |
|        - |  2897 | `	if( pNos < pStack ){` |
|        - |  2898 | `		goto Abort;` |
|        - |  2899 | `	}` |
|        - |  2900 | `#endif` |
|       23 |  2901 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       21 |  2902 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       21 |  2903 | `		ph7_class *pClass = 0;` |
|        - |  2904 | `		/* Extract the target class */` |
|       21 |  2905 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  2906 | `			/* Instance already loaded */` |
|      ! 0 |  2907 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       21 |  2908 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2909 | `			/* Perform the query */` |
|       31 |  2910 | `			pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|       20 |  2911 | `				SyBlobLength(&pTos->sBlob),FALSE,0);` |
|       10 |  2912 | `		}` |
|       21 |  2913 | `		if( pClass ){` |
|        - |  2914 | `			/* Perform the query */` |
|       21 |  2915 | `			iRes = VmInstanceOf(pThis->pClass,pClass);` |
|       10 |  2916 | `		}` |
|       10 |  2917 | `	}` |
|        - |  2918 | `	/* Push result */` |
|       23 |  2919 | `	VmPopOperand(&pTos,1);` |
|       23 |  2920 | `	PH7_MemObjRelease(pTos);` |
|       23 |  2921 | `	pTos->x.iVal = iRes;` |
|       23 |  2922 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       23 |  2923 | `	break;` |
|        - |  2924 | `				 }` |
|        - |  2925 |  |
|        - |  2926 | `/*` |
|        - |  2927 | ` * LOADC P1 P2 *` |
|        - |  2928 | ` *` |
|        - |  2929 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  2930 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  2931 | ` */` |
|   756005 |  2932 | `case PH7_OP_LOADC: {` |
|        - |  2933 | `	ph7_value *pObj;` |
|        - |  2934 | `	/* Reserve a room */` |
|  1512056 |  2935 | `	pTos++;` |
|  1512056 |  2936 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1512056 |  2937 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2938 | `			SyHashEntry *pEntry;` |
|        - |  2939 | `			/* Candidate for expansion via user defined callbacks */` |
|    17292 |  2940 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    17292 |  2941 | `			if( pEntry ){` |
|    14210 |  2942 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2943 | `				/* Set a NULL default value */` |
|    14210 |  2944 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    14210 |  2945 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2946 | `				/* Invoke the callback and deal with the expanded value */` |
|    14210 |  2947 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  2948 | `				/* Mark as constant */` |
|    14210 |  2949 | `				pTos->nIdx = SXU32_HIGH;` |
|    14210 |  2950 | `				break;` |
|        - |  2951 | `			}` |
|     1541 |  2952 | `		}` |
|  1497848 |  2953 | `		PH7_MemObjLoad(pObj,pTos);` |
|   748947 |  2954 | `	}else{` |
|        - |  2955 | `		/* Set a NULL value */` |
|      ! 0 |  2956 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2957 | `	}` |
|        - |  2958 | `	/* Mark as constant */` |
|  1497848 |  2959 | `	pTos->nIdx = SXU32_HIGH;` |
|  1497848 |  2960 | `	break;` |
|        - |  2961 | `				  }` |
|        - |  2962 | `/*` |
|        - |  2963 | ` * LOAD: P1 * P3` |
|        - |  2964 | ` *` |
|        - |  2965 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  2966 | ` * from the P3 operand.` |
|        - |  2967 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  2968 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  2969 | ` */` |
|  1301715 |  2970 | `case PH7_OP_LOAD:{` |
|        - |  2971 | `	ph7_value *pObj;` |
|        - |  2972 | `	SyString sName;` |
|  2603652 |  2973 | `	if( pInstr->p3 == 0 ){` |
|        - |  2974 | `		/* Take the variable name from the top of the stack */` |
|        - |  2975 | `#ifdef UNTRUST` |
|        - |  2976 | `		if( pTos < pStack ){` |
|        - |  2977 | `			goto Abort;` |
|        - |  2978 | `		}` |
|        - |  2979 | `#endif` |
|        - |  2980 | `		/* Force a string cast */` |
|       19 |  2981 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2982 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  2983 | `		}` |
|       19 |  2984 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  2985 | `	}else{` |
|  2603634 |  2986 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  2987 | `		/* Reserve a room for the target object */` |
|  2603634 |  2988 | `		pTos++;` |
|        - |  2989 | `	}` |
|        - |  2990 | `	/* Extract the requested memory object */` |
|  2603652 |  2991 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2603652 |  2992 | `	if( pObj == 0 ){` |
|      568 |  2993 | `		if( pInstr->iP1 ){` |
|        - |  2994 | `			/* Variable not found,load NULL */` |
|      568 |  2995 | `			if( !pInstr->p3 ){` |
|      ! 0 |  2996 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  2997 | `			}else{` |
|      568 |  2998 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2999 | `			}` |
|      568 |  3000 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1302000 |  3001 | `			break;` |
|      ! 0 |  3002 | `		}else{` |
|        - |  3003 | `			/* Fatal error */` |
|      ! 0 |  3004 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3005 | `			goto Abort;` |
|        - |  3006 | `		}` |
|        - |  3007 | `	}` |
|        - |  3008 | `	/* Load variable contents */` |
|  2603086 |  3009 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2603086 |  3010 | `	pTos->nIdx = pObj->nIdx;` |
|  2603086 |  3011 | `	break;` |
|        - |  3012 | `				   }` |
|        - |  3013 | `/*` |
|        - |  3014 | ` * LOAD_MAP P1 * *` |
|        - |  3015 | ` *` |
|        - |  3016 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3017 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3018 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3019 | ` */` |
|    16446 |  3020 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3021 | `	ph7_hashmap *pMap;` |
|        - |  3022 | `	/* Allocate a new hashmap instance */` |
|    32894 |  3023 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    32894 |  3024 | `	if( pMap == 0 ){` |
|      ! 0 |  3025 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3026 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3027 | `		goto Abort;` |
|        - |  3028 | `	}` |
|    32894 |  3029 | `	if( pInstr->iP1 > 0 ){` |
|     1934 |  3030 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3031 | `		/* Perform the insertion */` |
|     5898 |  3032 | `		while( pEntry < pTos ){` |
|     3966 |  3033 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3034 | `				/* Insertion by reference */` |
|      142 |  3035 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3036 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3037 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3038 | `					);` |
|       48 |  3039 | `			}else{` |
|        - |  3040 | `				/* Standard insertion */` |
|     5807 |  3041 | `				PH7_HashmapInsert(pMap,` |
|     3870 |  3042 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     1935 |  3043 | `					&pEntry[1]` |
|        - |  3044 | `				);` |
|        - |  3045 | `			}` |
|        - |  3046 | `			/* Next pair on the stack */` |
|     3966 |  3047 | `			pEntry += 2;` |
|        2 |  3048 | `		}` |
|        - |  3049 | `		/* Pop P1 elements */` |
|     1934 |  3050 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|      966 |  3051 | `	}` |
|        - |  3052 | `	/* Push the hashmap */` |
|    32894 |  3053 | `	pTos++;` |
|    32894 |  3054 | `	pTos->nIdx = SXU32_HIGH;` |
|    32894 |  3055 | `	pTos->x.pOther = pMap;` |
|    32894 |  3056 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    32894 |  3057 | `	break;` |
|        - |  3058 | `					  }` |
|        - |  3059 | `/*` |
|        - |  3060 | ` * LOAD_LIST: P1 * *` |
|        - |  3061 | ` *` |
|        - |  3062 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3063 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3064 | ` * Caveats:` |
|        - |  3065 | ` *  This implementation support only a single nesting level.` |
|        - |  3066 | ` */` |
|       17 |  3067 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3068 | `	ph7_value *pEntry;` |
|       35 |  3069 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3070 | `		/* Empty list,break immediately */` |
|      ! 0 |  3071 | `		break;` |
|        - |  3072 | `	}` |
|       35 |  3073 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3074 | `#ifdef UNTRUST` |
|        - |  3075 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3076 | `		goto Abort;` |
|        - |  3077 | `	}` |
|        - |  3078 | `#endif` |
|       35 |  3079 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       31 |  3080 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3081 | `		ph7_hashmap_node *pNode;` |
|        - |  3082 | `		ph7_value sKey,*pObj;` |
|        - |  3083 | `		/* Start Copying */` |
|       31 |  3084 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|       99 |  3085 | `		while( pEntry <= pTos ){` |
|       69 |  3086 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       65 |  3087 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       65 |  3088 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       65 |  3089 | `					if( rc == SXRET_OK ){` |
|        - |  3090 | `						/* Store node value */` |
|       65 |  3091 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       33 |  3092 | `					}else{` |
|        - |  3093 | `						/* Nullify the variable */` |
|      ! 0 |  3094 | `						PH7_MemObjRelease(pObj);` |
|        - |  3095 | `					}` |
|       32 |  3096 | `				}` |
|       32 |  3097 | `			}` |
|       69 |  3098 | `			sKey.x.iVal++; /* Next numeric index */` |
|       69 |  3099 | `			pEntry++;` |
|        1 |  3100 | `		}` |
|       15 |  3101 | `	}` |
|       35 |  3102 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       35 |  3103 | `	break;` |
|        - |  3104 | `					   }` |
|        - |  3105 | `/*` |
|        - |  3106 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3107 | ` *` |
|        - |  3108 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3109 | ` * from the stack.` |
|        - |  3110 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3111 | ` * instead.` |
|        - |  3112 | ` */` |
|   213799 |  3113 | `case PH7_OP_LOAD_IDX: {` |
|   427644 |  3114 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   427644 |  3115 | `	ph7_hashmap *pMap = 0;` |
|        - |  3116 | `	ph7_value *pIdx;` |
|   427644 |  3117 | `	pIdx = 0;` |
|   427644 |  3118 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3119 | `		if( !pInstr->iP2){` |
|        - |  3120 | `			/* No available index,load NULL */` |
|      ! 0 |  3121 | `			if( pTos >= pStack ){` |
|      ! 0 |  3122 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3123 | `			}else{` |
|        - |  3124 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3125 | `				pTos++;` |
|      ! 0 |  3126 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3127 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3128 | `			}` |
|        - |  3129 | `			/* Emit a notice */` |
|      ! 0 |  3130 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3131 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3132 | `			break;` |
|        - |  3133 | `		}` |
|      ! 0 |  3134 | `	}else{` |
|   427644 |  3135 | `		pIdx = pTos;` |
|   427644 |  3136 | `		pTos--;` |
|        - |  3137 | `	}` |
|   427644 |  3138 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3139 | `		/* String access */` |
|   346124 |  3140 | `		if( pIdx ){` |
|        - |  3141 | `			sxu32 nOfft;` |
|   346124 |  3142 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3143 | `				/* Force an int cast */` |
|      ! 0 |  3144 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3145 | `			}` |
|   346124 |  3146 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   346124 |  3147 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3148 | `				/* Invalid offset,load null */` |
|      ! 0 |  3149 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3150 | `			}else{` |
|   346124 |  3151 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   346124 |  3152 | `				int c = zData[nOfft];` |
|   346124 |  3153 | `				PH7_MemObjRelease(pTos);` |
|   346124 |  3154 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   346124 |  3155 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3156 | `			}` |
|   173085 |  3157 | `		}else{` |
|        - |  3158 | `			/* No available index,load NULL */` |
|      ! 0 |  3159 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3160 | `		}` |
|   346124 |  3161 | `		break;` |
|        - |  3162 | `	}` |
|    81522 |  3163 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3164 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3165 | `			ph7_value *pObj;` |
|      ! 0 |  3166 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3167 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3168 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3169 | `			}` |
|      ! 0 |  3170 | `		}` |
|      ! 0 |  3171 | `	}` |
|    81522 |  3172 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    81522 |  3173 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3174 | `		/* Point to the hashmap */` |
|    81522 |  3175 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    81522 |  3176 | `		if( pIdx ){` |
|        - |  3177 | `			/* Load the desired entry */` |
|    81522 |  3178 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    40760 |  3179 | `		}` |
|    81522 |  3180 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3181 | `			/* Create a new empty entry */` |
|      ! 0 |  3182 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3183 | `			if( rc == SXRET_OK ){` |
|        - |  3184 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3185 | `				pNode = pMap->pLast;` |
|      ! 0 |  3186 | `			}` |
|      ! 0 |  3187 | `		}` |
|    40760 |  3188 | `	}` |
|    81522 |  3189 | `	if( pIdx ){` |
|    81522 |  3190 | `		PH7_MemObjRelease(pIdx);` |
|    40760 |  3191 | `	}` |
|    81522 |  3192 | `	if( rc == SXRET_OK ){` |
|        - |  3193 | `		/* Load entry contents */` |
|    37674 |  3194 | `		if( pMap->iRef < 2 ){` |
|        - |  3195 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3196 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3197 | `			 */` |
|        7 |  3198 | `			pTos->nIdx = SXU32_HIGH;` |
|        7 |  3199 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|        4 |  3200 | `		}else{` |
|    37668 |  3201 | `			pTos->nIdx = pNode->nValIdx;` |
|    37668 |  3202 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    37668 |  3203 | `			PH7_HashmapUnref(pMap);` |
|        - |  3204 | `		}` |
|    18838 |  3205 | `	}else{` |
|        - |  3206 | `		/* No such entry,load NULL */` |
|    43850 |  3207 | `		PH7_MemObjRelease(pTos);` |
|    43850 |  3208 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3209 | `	}` |
|    81522 |  3210 | `	break;` |
|        - |  3211 | `					  }` |
|        - |  3212 | `/*` |
|        - |  3213 | ` * LOAD_CLOSURE * * P3` |
|        - |  3214 | ` *` |
|        - |  3215 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3216 | ` * name in the stack.` |
|        - |  3217 | ` */` |
|        2 |  3218 | `case PH7_OP_LOAD_CLOSURE:{` |
|        5 |  3219 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        5 |  3220 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3221 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3222 | `		ph7_vm_func *pClosure;` |
|        - |  3223 | `		char *zName;` |
|        - |  3224 | `		sxu32 mLen;` |
|        - |  3225 | `		sxu32 n;` |
|        - |  3226 | `		/* Create a new VM function */` |
|        5 |  3227 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3228 | `		/* Generate an unique closure name */` |
|        5 |  3229 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        5 |  3230 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3231 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3232 | `			goto Abort;` |
|        - |  3233 | `		}` |
|        5 |  3234 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        5 |  3235 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3236 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3237 | `		}` |
|        - |  3238 | `		/* Zero the stucture */` |
|        5 |  3239 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3240 | `		/* Perform a structure assignment on read-only items */` |
|        5 |  3241 | `		pClosure->aArgs = pFunc->aArgs;` |
|        5 |  3242 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        5 |  3243 | `		pClosure->aStatic = pFunc->aStatic;` |
|        5 |  3244 | `		pClosure->iFlags = pFunc->iFlags;` |
|        5 |  3245 | `		pClosure->pUserData = pFunc->pUserData;` |
|        5 |  3246 | `		pClosure->sSignature = pFunc->sSignature;` |
|        5 |  3247 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3248 | `		/* Register the closure */` |
|        5 |  3249 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3250 | `		/* Set up closure environment */` |
|        5 |  3251 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        5 |  3252 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       13 |  3253 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3254 | `			ph7_value *pValue;` |
|        9 |  3255 | `			pEnv = &aEnv[n];` |
|        9 |  3256 | `			sEnv.sName  = pEnv->sName;` |
|        9 |  3257 | `			sEnv.iFlags = pEnv->iFlags;` |
|        9 |  3258 | `			sEnv.nIdx = SXU32_HIGH;` |
|        9 |  3259 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|        9 |  3260 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3261 | `				/* Pass by reference */` |
|      ! 0 |  3262 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3263 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3264 | `					);` |
|      ! 0 |  3265 | `			}` |
|        - |  3266 | `			/* Standard pass by value */` |
|        9 |  3267 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|        9 |  3268 | `			if( pValue ){` |
|        - |  3269 | `				/* Copy imported value */` |
|        5 |  3270 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        2 |  3271 | `			}` |
|        - |  3272 | `			/* Insert the imported variable */` |
|        9 |  3273 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        5 |  3274 | `		}` |
|        - |  3275 | `		/* Finally,load the closure name on the stack */` |
|        5 |  3276 | `		pTos++;` |
|        5 |  3277 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        2 |  3278 | `	}` |
|        5 |  3279 | `	break;` |
|        - |  3280 | `						 }` |
|        - |  3281 | `/*` |
|        - |  3282 | ` * STORE * P2 P3` |
|        - |  3283 | ` *` |
|        - |  3284 | ` * Perform a store (Assignment) operation.` |
|        - |  3285 | ` */` |
|   100230 |  3286 | `case PH7_OP_STORE: {` |
|        - |  3287 | `	ph7_value *pObj;` |
|        - |  3288 | `	SyString sName;` |
|        - |  3289 | `#ifdef UNTRUST` |
|        - |  3290 | `	if( pTos < pStack ){` |
|        - |  3291 | `		goto Abort;` |
|        - |  3292 | `	}` |
|        - |  3293 | `#endif` |
|   200462 |  3294 | `	if( pInstr->iP2 ){` |
|        - |  3295 | `		sxu32 nIdx;` |
|        - |  3296 | `		/* Member store operation */` |
|     2258 |  3297 | `		nIdx = pTos->nIdx;` |
|     2258 |  3298 | `		VmPopOperand(&pTos,1);` |
|     2258 |  3299 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3300 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3301 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3302 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3303 | `		}else{` |
|        - |  3304 | `			/* Point to the desired memory object */` |
|     2254 |  3305 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2254 |  3306 | `			if( pObj ){` |
|        - |  3307 | `				/* Perform the store operation */` |
|     2254 |  3308 | `				PH7_MemObjStore(pTos,pObj);` |
|     1126 |  3309 | `			}` |
|        - |  3310 | `		}` |
|   101360 |  3311 | `		break;` |
|   198206 |  3312 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3313 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3314 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3315 | `			/* Force a string cast */` |
|      ! 0 |  3316 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3317 | `		}` |
|        7 |  3318 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3319 | `		pTos--;` |
|        - |  3320 | `#ifdef UNTRUST` |
|        - |  3321 | `		if( pTos < pStack  ){` |
|        - |  3322 | `			goto Abort;` |
|        - |  3323 | `		}` |
|        - |  3324 | `#endif` |
|        4 |  3325 | `	}else{` |
|   198200 |  3326 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3327 | `	}` |
|        - |  3328 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   198206 |  3329 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   198206 |  3330 | `	if( pObj == 0 ){` |
|      ! 0 |  3331 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3332 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3333 | `		goto Abort;` |
|        - |  3334 | `	}` |
|   198206 |  3335 | `	if( !pInstr->p3 ){` |
|        7 |  3336 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3337 | `	}` |
|        - |  3338 | `	/* Perform the store operation */` |
|   198206 |  3339 | `	PH7_MemObjStore(pTos,pObj);` |
|   198206 |  3340 | `	break;` |
|        - |  3341 | `				   }` |
|        - |  3342 | `/*` |
|        - |  3343 | ` * STORE_IDX:   P1 * P3` |
|        - |  3344 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3345 | ` *` |
|        - |  3346 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3347 | ` */` |
|    75936 |  3348 | `case PH7_OP_STORE_IDX:` |
|        - |  3349 | `case PH7_OP_STORE_IDX_REF: {` |
|   151874 |  3350 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3351 | `	ph7_value *pKey;` |
|        - |  3352 | `	sxu32 nIdx;` |
|   151874 |  3353 | `	if( pInstr->iP1 ){` |
|        - |  3354 | `		/* Key is next on stack */` |
|    54948 |  3355 | `		pKey = pTos;` |
|    54948 |  3356 | `		pTos--;` |
|    27475 |  3357 | `	}else{` |
|    96928 |  3358 | `		pKey = 0;` |
|        - |  3359 | `	}` |
|   151874 |  3360 | `	nIdx = pTos->nIdx;` |
|   151874 |  3361 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3362 | `		/* Hashmap already loaded */` |
|   151822 |  3363 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   151822 |  3364 | `		if( pMap->iRef < 2 ){` |
|        - |  3365 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3366 | `			pMap->iRef = 2;` |
|      ! 0 |  3367 | `		}` |
|    75912 |  3368 | `	}else{` |
|        - |  3369 | `		ph7_value *pObj;` |
|       53 |  3370 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3371 | `		if( pObj == 0 ){` |
|      ! 0 |  3372 | `			if( pKey ){` |
|      ! 0 |  3373 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3374 | `			}` |
|      ! 0 |  3375 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3376 | `			break;` |
|        - |  3377 | `		}` |
|        - |  3378 | `		/* Phase#1: Load the array */` |
|       53 |  3379 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3380 | `			VmPopOperand(&pTos,1);` |
|       53 |  3381 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3382 | `				/* Force a string cast */` |
|      ! 0 |  3383 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3384 | `			}` |
|       53 |  3385 | `			if( pKey == 0 ){` |
|        - |  3386 | `				/* Append string */` |
|        3 |  3387 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3388 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3389 | `				}` |
|        2 |  3390 | `			}else{` |
|        - |  3391 | `				sxu32 nOfft;` |
|       51 |  3392 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3393 | `					/* Force an int cast */` |
|       51 |  3394 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3395 | `				}` |
|       51 |  3396 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3397 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3398 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3399 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3400 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3401 | `				}else{` |
|      ! 0 |  3402 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3403 | `						/* Perform an append operation */` |
|      ! 0 |  3404 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3405 | `					}` |
|        - |  3406 | `				}` |
|        - |  3407 | `			}` |
|       53 |  3408 | `			if( pKey ){` |
|       51 |  3409 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3410 | `			}` |
|       53 |  3411 | `			break;` |
|      ! 0 |  3412 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3413 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3414 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3415 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3416 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3417 | `				goto Abort;` |
|        - |  3418 | `			}` |
|      ! 0 |  3419 | `		}` |
|      ! 0 |  3420 | `		pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - |  3421 | `	}` |
|   151822 |  3422 | `	VmPopOperand(&pTos,1);` |
|        - |  3423 | `	/* Phase#2: Perform the insertion */` |
|   151822 |  3424 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3425 | `		/* Insertion by reference */` |
|       15 |  3426 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3427 | `	}else{` |
|   151808 |  3428 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3429 | `	}` |
|   151822 |  3430 | `	if( pKey ){` |
|    54898 |  3431 | `		PH7_MemObjRelease(pKey);` |
|    27448 |  3432 | `	}` |
|   151822 |  3433 | `	break;` |
|        - |  3434 | `					   }` |
|        - |  3435 | `/*` |
|        - |  3436 | ` * INCR: P1 * *` |
|        - |  3437 | ` *` |
|        - |  3438 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3439 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3440 | ` * the stack and increment after that.` |
|        - |  3441 | ` */` |
|   155751 |  3442 | `case PH7_OP_INCR:` |
|        - |  3443 | `#ifdef UNTRUST` |
|        - |  3444 | `	if( pTos < pStack ){` |
|        - |  3445 | `		goto Abort;` |
|        - |  3446 | `	}` |
|        - |  3447 | `#endif` |
|   311548 |  3448 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   311548 |  3449 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3450 | `			ph7_value *pObj;` |
|   311548 |  3451 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3452 | `				/* Force a numeric cast */` |
|   311548 |  3453 | `				PH7_MemObjToNumeric(pObj);` |
|   311548 |  3454 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3455 | `					pObj->rVal++;` |
|        - |  3456 | `					/* Try to get an integer representation */` |
|      ! 0 |  3457 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3458 | `				}else{` |
|   311548 |  3459 | `					pObj->x.iVal++;` |
|   311548 |  3460 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3461 | `				}` |
|   311548 |  3462 | `				if( pInstr->iP1 ){` |
|        - |  3463 | `					/* Pre-icrement */` |
|       71 |  3464 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3465 | `				}` |
|   155795 |  3466 | `			}` |
|   155797 |  3467 | `		}else{` |
|      ! 0 |  3468 | `			if( pInstr->iP1 ){` |
|        - |  3469 | `				/* Force a numeric cast */` |
|      ! 0 |  3470 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3471 | `				/* Pre-increment */` |
|      ! 0 |  3472 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3473 | `					pTos->rVal++;` |
|        - |  3474 | `					/* Try to get an integer representation */` |
|      ! 0 |  3475 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3476 | `				}else{` |
|      ! 0 |  3477 | `					pTos->x.iVal++;` |
|      ! 0 |  3478 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3479 | `				}` |
|      ! 0 |  3480 | `			}` |
|        - |  3481 | `		}` |
|   155795 |  3482 | `	}` |
|   311548 |  3483 | `	break;` |
|        - |  3484 | `/*` |
|        - |  3485 | ` * DECR: P1 * *` |
|        - |  3486 | ` *` |
|        - |  3487 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3488 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3489 | ` * and decrement after that.` |
|        - |  3490 | ` */` |
|        2 |  3491 | `case PH7_OP_DECR:` |
|        - |  3492 | `#ifdef UNTRUST` |
|        - |  3493 | `	if( pTos < pStack ){` |
|        - |  3494 | `		goto Abort;` |
|        - |  3495 | `	}` |
|        - |  3496 | `#endif` |
|        5 |  3497 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3498 | `		/* Force a numeric cast */` |
|        5 |  3499 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3500 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3501 | `			ph7_value *pObj;` |
|        5 |  3502 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3503 | `				/* Force a numeric cast */` |
|        5 |  3504 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3505 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3506 | `					pObj->rVal--;` |
|        - |  3507 | `					/* Try to get an integer representation */` |
|      ! 0 |  3508 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3509 | `				}else{` |
|        5 |  3510 | `					pObj->x.iVal--;` |
|        5 |  3511 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3512 | `				}` |
|        5 |  3513 | `				if( pInstr->iP1 ){` |
|        - |  3514 | `					/* Pre-icrement */` |
|      ! 0 |  3515 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3516 | `				}` |
|        2 |  3517 | `			}` |
|        3 |  3518 | `		}else{` |
|      ! 0 |  3519 | `			if( pInstr->iP1 ){` |
|        - |  3520 | `				/* Pre-increment */` |
|      ! 0 |  3521 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3522 | `					pTos->rVal--;` |
|        - |  3523 | `					/* Try to get an integer representation */` |
|      ! 0 |  3524 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3525 | `				}else{` |
|      ! 0 |  3526 | `					pTos->x.iVal--;` |
|      ! 0 |  3527 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3528 | `				}` |
|      ! 0 |  3529 | `			}` |
|        - |  3530 | `		}` |
|        2 |  3531 | `	}` |
|        5 |  3532 | `	break;` |
|        - |  3533 | `/*` |
|        - |  3534 | ` * UMINUS: * * *` |
|        - |  3535 | ` *` |
|        - |  3536 | ` * Perform a unary minus operation.` |
|        - |  3537 | ` */` |
|    21293 |  3538 | `case PH7_OP_UMINUS:` |
|        - |  3539 | `#ifdef UNTRUST` |
|        - |  3540 | `	if( pTos < pStack ){` |
|        - |  3541 | `		goto Abort;` |
|        - |  3542 | `	}` |
|        - |  3543 | `#endif` |
|        - |  3544 | `	/* Force a numeric (integer,real or both) cast */` |
|    42588 |  3545 | `	PH7_MemObjToNumeric(pTos);` |
|    42588 |  3546 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       25 |  3547 | `		pTos->rVal = -pTos->rVal;` |
|       12 |  3548 | `	}` |
|    42588 |  3549 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    42564 |  3550 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    21281 |  3551 | `	}` |
|    42588 |  3552 | `	break;` |
|        - |  3553 | `/*` |
|        - |  3554 | ` * UPLUS: * * *` |
|        - |  3555 | ` *` |
|        - |  3556 | ` * Perform a unary plus operation.` |
|        - |  3557 | ` */` |
|       16 |  3558 | `case PH7_OP_UPLUS:` |
|        - |  3559 | `#ifdef UNTRUST` |
|        - |  3560 | `	if( pTos < pStack ){` |
|        - |  3561 | `		goto Abort;` |
|        - |  3562 | `	}` |
|        - |  3563 | `#endif` |
|        - |  3564 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3565 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3566 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3567 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3568 | `	}` |
|       33 |  3569 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3570 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3571 | `	}` |
|       33 |  3572 | `	break;` |
|        - |  3573 | `/*` |
|        - |  3574 | ` * OP_LNOT: * * *` |
|        - |  3575 | ` *` |
|        - |  3576 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3577 | ` * with its complement.` |
|        - |  3578 | ` */` |
|    46513 |  3579 | `case PH7_OP_LNOT:` |
|        - |  3580 | `#ifdef UNTRUST` |
|        - |  3581 | `	if( pTos < pStack ){` |
|        - |  3582 | `		goto Abort;` |
|        - |  3583 | `	}` |
|        - |  3584 | `#endif` |
|        - |  3585 | `	/* Force a boolean cast */` |
|    93072 |  3586 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3587 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3588 | `	}` |
|    93072 |  3589 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    93072 |  3590 | `	break;` |
|        - |  3591 | `/*` |
|        - |  3592 | ` * OP_BITNOT: * * *` |
|        - |  3593 | ` *` |
|        - |  3594 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3595 | ` * with its ones-complement.` |
|        - |  3596 | ` */` |
|        3 |  3597 | `case PH7_OP_BITNOT:` |
|        - |  3598 | `#ifdef UNTRUST` |
|        - |  3599 | `	if( pTos < pStack ){` |
|        - |  3600 | `		goto Abort;` |
|        - |  3601 | `	}` |
|        - |  3602 | `#endif` |
|        - |  3603 | `	/* Force an integer cast */` |
|        7 |  3604 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3605 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3606 | `	}` |
|        7 |  3607 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|        7 |  3608 | `	break;` |
|        - |  3609 | `/* OP_MUL * * *` |
|        - |  3610 | ` * OP_MUL_STORE * * *` |
|        - |  3611 | ` *` |
|        - |  3612 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3613 | ` * and push the result back onto the stack.` |
|        - |  3614 | ` */` |
|     1234 |  3615 | `case PH7_OP_MUL:` |
|        - |  3616 | `case PH7_OP_MUL_STORE: {` |
|     2470 |  3617 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3618 | `	/* Force the operand to be numeric */` |
|        - |  3619 | `#ifdef UNTRUST` |
|        - |  3620 | `	if( pNos < pStack ){` |
|        - |  3621 | `		goto Abort;` |
|        - |  3622 | `	}` |
|        - |  3623 | `#endif` |
|     2470 |  3624 | `	PH7_MemObjToNumeric(pTos);` |
|     2470 |  3625 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3626 | `	/* Perform the requested operation */` |
|     2470 |  3627 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3628 | `		/* Floating point arithemic */` |
|        - |  3629 | `		ph7_real a,b,r;` |
|       17 |  3630 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3631 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3632 | `		}` |
|       17 |  3633 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3634 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3635 | `		}` |
|       17 |  3636 | `		a = pNos->rVal;` |
|       17 |  3637 | `		b = pTos->rVal;` |
|       17 |  3638 | `		r = a * b;` |
|        - |  3639 | `		/* Push the result */` |
|       17 |  3640 | `		pNos->rVal = r;` |
|       17 |  3641 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3642 | `		/* Try to get an integer representation */` |
|       17 |  3643 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3644 | `	}else{` |
|        - |  3645 | `		/* Integer arithmetic */` |
|        - |  3646 | `		sxi64 a,b,r;` |
|     2454 |  3647 | `		a = pNos->x.iVal;` |
|     2454 |  3648 | `		b = pTos->x.iVal;` |
|     2454 |  3649 | `		r = a * b;` |
|        - |  3650 | `		/* Push the result */` |
|     2454 |  3651 | `		pNos->x.iVal = r;` |
|     2454 |  3652 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3653 | `	}` |
|     2470 |  3654 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3655 | `		ph7_value *pObj;` |
|       19 |  3656 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3657 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  3658 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  3659 | `			PH7_MemObjStore(pNos,pObj);` |
|        9 |  3660 | `		}` |
|        9 |  3661 | `	}` |
|     2470 |  3662 | `	VmPopOperand(&pTos,1);` |
|     2470 |  3663 | `	break;` |
|        - |  3664 | `				 }` |
|        - |  3665 | `/* OP_ADD * * *` |
|        - |  3666 | ` *` |
|        - |  3667 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3668 | ` * and push the result back onto the stack.` |
|        - |  3669 | ` */` |
|      425 |  3670 | `case PH7_OP_ADD:{` |
|      852 |  3671 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3672 | `#ifdef UNTRUST` |
|        - |  3673 | `	if( pNos < pStack ){` |
|        - |  3674 | `		goto Abort;` |
|        - |  3675 | `	}` |
|        - |  3676 | `#endif` |
|        - |  3677 | `	/* Perform the addition */` |
|      852 |  3678 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      852 |  3679 | `	VmPopOperand(&pTos,1);` |
|      852 |  3680 | `	break;` |
|        - |  3681 | `				}` |
|        - |  3682 | `/*` |
|        - |  3683 | ` * OP_ADD_STORE * * *` |
|        - |  3684 | ` *` |
|        - |  3685 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3686 | ` * and push the result back onto the stack.` |
|        - |  3687 | ` */` |
|      481 |  3688 | `case PH7_OP_ADD_STORE:{` |
|      963 |  3689 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3690 | `	ph7_value *pObj;` |
|        - |  3691 | `	sxu32 nIdx;` |
|        - |  3692 | `#ifdef UNTRUST` |
|        - |  3693 | `	if( pNos < pStack ){` |
|        - |  3694 | `		goto Abort;` |
|        - |  3695 | `	}` |
|        - |  3696 | `#endif` |
|        - |  3697 | `	/* Perform the addition */` |
|      963 |  3698 | `	nIdx = pTos->nIdx;` |
|      963 |  3699 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3700 | `	/* Peform the store operation */` |
|      963 |  3701 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3702 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      963 |  3703 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      963 |  3704 | `		PH7_MemObjStore(pTos,pObj);` |
|      481 |  3705 | `	}` |
|        - |  3706 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      963 |  3707 | `	PH7_MemObjStore(pTos,pNos);` |
|      963 |  3708 | `	VmPopOperand(&pTos,1);` |
|      963 |  3709 | `	break;` |
|        - |  3710 | `				}` |
|        - |  3711 | `/* OP_SUB * * *` |
|        - |  3712 | ` *` |
|        - |  3713 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3714 | ` * first (what was next on the stack) from the second (the` |
|        - |  3715 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3716 | ` */` |
|      294 |  3717 | `case PH7_OP_SUB: {` |
|      589 |  3718 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3719 | `#ifdef UNTRUST` |
|        - |  3720 | `	if( pNos < pStack ){` |
|        - |  3721 | `		goto Abort;` |
|        - |  3722 | `	}` |
|        - |  3723 | `#endif` |
|      589 |  3724 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3725 | `		/* Floating point arithemic */` |
|        - |  3726 | `		ph7_real a,b,r;` |
|       95 |  3727 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3728 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3729 | `		}` |
|       95 |  3730 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3731 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3732 | `		}` |
|       95 |  3733 | `		a = pNos->rVal;` |
|       95 |  3734 | `		b = pTos->rVal;` |
|       95 |  3735 | `		r = a - b;` |
|        - |  3736 | `		/* Push the result */` |
|       95 |  3737 | `		pNos->rVal = r;` |
|       95 |  3738 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3739 | `		/* Try to get an integer representation */` |
|       95 |  3740 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  3741 | `	}else{` |
|        - |  3742 | `		/* Integer arithmetic */` |
|        - |  3743 | `		sxi64 a,b,r;` |
|      495 |  3744 | `		a = pNos->x.iVal;` |
|      495 |  3745 | `		b = pTos->x.iVal;` |
|      495 |  3746 | `		r = a - b;` |
|        - |  3747 | `		/* Push the result */` |
|      495 |  3748 | `		pNos->x.iVal = r;` |
|      495 |  3749 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3750 | `	}` |
|      589 |  3751 | `	VmPopOperand(&pTos,1);` |
|      589 |  3752 | `	break;` |
|        - |  3753 | `				 }` |
|        - |  3754 | `/* OP_SUB_STORE * * *` |
|        - |  3755 | ` *` |
|        - |  3756 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3757 | ` * first (what was next on the stack) from the second (the` |
|        - |  3758 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3759 | ` */` |
|        1 |  3760 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3761 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3762 | `	ph7_value *pObj;` |
|        - |  3763 | `#ifdef UNTRUST` |
|        - |  3764 | `	if( pNos < pStack ){` |
|        - |  3765 | `		goto Abort;` |
|        - |  3766 | `	}` |
|        - |  3767 | `#endif` |
|        3 |  3768 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3769 | `		/* Floating point arithemic */` |
|        - |  3770 | `		ph7_real a,b,r;` |
|      ! 0 |  3771 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3772 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3773 | `		}` |
|      ! 0 |  3774 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3775 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3776 | `		}` |
|      ! 0 |  3777 | `		a = pTos->rVal;` |
|      ! 0 |  3778 | `		b = pNos->rVal;` |
|      ! 0 |  3779 | `		r = a - b;` |
|        - |  3780 | `		/* Push the result */` |
|      ! 0 |  3781 | `		pNos->rVal = r;` |
|      ! 0 |  3782 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3783 | `		/* Try to get an integer representation */` |
|      ! 0 |  3784 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3785 | `	}else{` |
|        - |  3786 | `		/* Integer arithmetic */` |
|        - |  3787 | `		sxi64 a,b,r;` |
|        3 |  3788 | `		a = pTos->x.iVal;` |
|        3 |  3789 | `		b = pNos->x.iVal;` |
|        3 |  3790 | `		r = a - b;` |
|        - |  3791 | `		/* Push the result */` |
|        3 |  3792 | `		pNos->x.iVal = r;` |
|        3 |  3793 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3794 | `	}` |
|        3 |  3795 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3796 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3797 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3798 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3799 | `	}` |
|        3 |  3800 | `	VmPopOperand(&pTos,1);` |
|        3 |  3801 | `	break;` |
|        - |  3802 | `				 }` |
|        - |  3803 |  |
|        - |  3804 | `/*` |
|        - |  3805 | ` * OP_MOD * * *` |
|        - |  3806 | ` *` |
|        - |  3807 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3808 | ` * first (what was next on the stack) from the second (the` |
|        - |  3809 | ` * top of the stack) and push the remainder after division` |
|        - |  3810 | ` * onto the stack.` |
|        - |  3811 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3812 | ` */` |
|      296 |  3813 | `case PH7_OP_MOD:{` |
|      594 |  3814 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3815 | `	sxi64 a,b,r;` |
|        - |  3816 | `#ifdef UNTRUST` |
|        - |  3817 | `	if( pNos < pStack ){` |
|        - |  3818 | `		goto Abort;` |
|        - |  3819 | `	}` |
|        - |  3820 | `#endif` |
|        - |  3821 | `	/* Force the operands to be integer */` |
|      594 |  3822 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3823 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3824 | `	}` |
|      594 |  3825 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3826 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3827 | `	}` |
|        - |  3828 | `	/* Perform the requested operation */` |
|      594 |  3829 | `	a = pNos->x.iVal;` |
|      594 |  3830 | `	b = pTos->x.iVal;` |
|      594 |  3831 | `	if( b == 0 ){` |
|        3 |  3832 | `		r = 0;` |
|        3 |  3833 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3834 | `		/* goto Abort; */` |
|        2 |  3835 | `	}else{` |
|      591 |  3836 | `		r = a%b;` |
|        - |  3837 | `	}` |
|        - |  3838 | `	/* Push the result */` |
|      594 |  3839 | `	pNos->x.iVal = r;` |
|      594 |  3840 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      594 |  3841 | `	VmPopOperand(&pTos,1);` |
|      594 |  3842 | `	break;` |
|        - |  3843 | `				}` |
|        - |  3844 | `/*` |
|        - |  3845 | ` * OP_MOD_STORE * * *` |
|        - |  3846 | ` *` |
|        - |  3847 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3848 | ` * first (what was next on the stack) from the second (the` |
|        - |  3849 | ` * top of the stack) and push the remainder after division` |
|        - |  3850 | ` * onto the stack.` |
|        - |  3851 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3852 | ` */` |
|        1 |  3853 | `case PH7_OP_MOD_STORE: {` |
|        3 |  3854 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3855 | `	ph7_value *pObj;` |
|        - |  3856 | `	sxi64 a,b,r;` |
|        - |  3857 | `#ifdef UNTRUST` |
|        - |  3858 | `	if( pNos < pStack ){` |
|        - |  3859 | `		goto Abort;` |
|        - |  3860 | `	}` |
|        - |  3861 | `#endif` |
|        - |  3862 | `	/* Force the operands to be integer */` |
|        3 |  3863 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3864 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3865 | `	}` |
|        3 |  3866 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3867 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3868 | `	}` |
|        - |  3869 | `	/* Perform the requested operation */` |
|        3 |  3870 | `	a = pTos->x.iVal;` |
|        3 |  3871 | `	b = pNos->x.iVal;` |
|        3 |  3872 | `	if( b == 0 ){` |
|      ! 0 |  3873 | `		r = 0;` |
|      ! 0 |  3874 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3875 | `		/* goto Abort; */` |
|      ! 0 |  3876 | `	}else{` |
|        3 |  3877 | `		r = a%b;` |
|        - |  3878 | `	}` |
|        - |  3879 | `	/* Push the result */` |
|        3 |  3880 | `	pNos->x.iVal = r;` |
|        3 |  3881 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  3882 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3883 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3884 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3885 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3886 | `	}` |
|        3 |  3887 | `	VmPopOperand(&pTos,1);` |
|        3 |  3888 | `	break;` |
|        - |  3889 | `				}` |
|        - |  3890 | `/*` |
|        - |  3891 | ` * OP_DIV * * *` |
|        - |  3892 | ` *` |
|        - |  3893 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3894 | ` * first (what was next on the stack) from the second (the` |
|        - |  3895 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3896 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3897 | ` */` |
|       28 |  3898 | `case PH7_OP_DIV:{` |
|       58 |  3899 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3900 | `	ph7_real a,b,r;` |
|        - |  3901 | `#ifdef UNTRUST` |
|        - |  3902 | `	if( pNos < pStack ){` |
|        - |  3903 | `		goto Abort;` |
|        - |  3904 | `	}` |
|        - |  3905 | `#endif` |
|        - |  3906 | `	/* Force the operands to be real */` |
|       58 |  3907 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  3908 | `		PH7_MemObjToReal(pTos);` |
|       26 |  3909 | `	}` |
|       58 |  3910 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  3911 | `		PH7_MemObjToReal(pNos);` |
|        9 |  3912 | `	}` |
|        - |  3913 | `	/* Perform the requested operation */` |
|       58 |  3914 | `	a = pNos->rVal;` |
|       58 |  3915 | `	b = pTos->rVal;` |
|       58 |  3916 | `	if( b == 0 ){` |
|        - |  3917 | `		/* Division by zero */` |
|        3 |  3918 | `		pNos->rVal = 0;` |
|        3 |  3919 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  3920 | `		/* goto Abort; */` |
|        2 |  3921 | `	}else{` |
|       55 |  3922 | `		r = a/b;` |
|        - |  3923 | `		/* Push the result */` |
|       55 |  3924 | `		pNos->rVal = r;` |
|       55 |  3925 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3926 | `		/* Try to get an integer representation */` |
|       55 |  3927 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3928 | `	}` |
|       58 |  3929 | `	VmPopOperand(&pTos,1);` |
|       58 |  3930 | `	break;` |
|        - |  3931 | `				}` |
|        - |  3932 | `/*` |
|        - |  3933 | ` * OP_DIV_STORE * * *` |
|        - |  3934 | ` *` |
|        - |  3935 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3936 | ` * first (what was next on the stack) from the second (the` |
|        - |  3937 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3938 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3939 | ` */` |
|        1 |  3940 | `case PH7_OP_DIV_STORE:{` |
|        3 |  3941 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3942 | `	ph7_value *pObj;` |
|        - |  3943 | `	ph7_real a,b,r;` |
|        - |  3944 | `#ifdef UNTRUST` |
|        - |  3945 | `	if( pNos < pStack ){` |
|        - |  3946 | `		goto Abort;` |
|        - |  3947 | `	}` |
|        - |  3948 | `#endif` |
|        - |  3949 | `	/* Force the operands to be real */` |
|        3 |  3950 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3951 | `		PH7_MemObjToReal(pTos);` |
|        1 |  3952 | `	}` |
|        3 |  3953 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3954 | `		PH7_MemObjToReal(pNos);` |
|        1 |  3955 | `	}` |
|        - |  3956 | `	/* Perform the requested operation */` |
|        3 |  3957 | `	a = pTos->rVal;` |
|        3 |  3958 | `	b = pNos->rVal;` |
|        3 |  3959 | `	if( b == 0 ){` |
|        - |  3960 | `		/* Division by zero */` |
|      ! 0 |  3961 | `		r = 0;` |
|      ! 0 |  3962 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  3963 | `		/* goto Abort; */` |
|      ! 0 |  3964 | `	}else{` |
|        3 |  3965 | `		r = a/b;` |
|        - |  3966 | `		/* Push the result */` |
|        3 |  3967 | `		pNos->rVal = r;` |
|        3 |  3968 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3969 | `		/* Try to get an integer representation */` |
|        3 |  3970 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3971 | `	}` |
|        3 |  3972 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3973 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3974 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3975 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3976 | `	}` |
|        3 |  3977 | `	VmPopOperand(&pTos,1);` |
|        3 |  3978 | `	break;` |
|        - |  3979 | `				}` |
|        - |  3980 | `/* OP_BAND * * *` |
|        - |  3981 | ` *` |
|        - |  3982 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3983 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  3984 | ` * two elements.` |
|        - |  3985 | `*/` |
|        - |  3986 | `/* OP_BOR * * *` |
|        - |  3987 | ` *` |
|        - |  3988 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3989 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  3990 | ` * two elements.` |
|        - |  3991 | ` */` |
|        - |  3992 | `/* OP_BXOR * * *` |
|        - |  3993 | ` *` |
|        - |  3994 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3995 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  3996 | ` * two elements.` |
|        - |  3997 | ` */` |
|       19 |  3998 | `case PH7_OP_BAND:` |
|        - |  3999 | `case PH7_OP_BOR:` |
|        - |  4000 | `case PH7_OP_BXOR:{` |
|       39 |  4001 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4002 | `	sxi64 a,b,r;` |
|        - |  4003 | `#ifdef UNTRUST` |
|        - |  4004 | `	if( pNos < pStack ){` |
|        - |  4005 | `		goto Abort;` |
|        - |  4006 | `	}` |
|        - |  4007 | `#endif` |
|        - |  4008 | `	/* Force the operands to be integer */` |
|       39 |  4009 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4010 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4011 | `	}` |
|       39 |  4012 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4013 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4014 | `	}` |
|        - |  4015 | `	/* Perform the requested operation */` |
|       39 |  4016 | `	a = pNos->x.iVal;` |
|       39 |  4017 | `	b = pTos->x.iVal;` |
|       39 |  4018 | `	switch(pInstr->iOp){` |
|        6 |  4019 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4020 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4021 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4022 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        7 |  4023 | `	case PH7_OP_BAND_STORE:` |
|        7 |  4024 | `	case PH7_OP_BAND:` |
|       15 |  4025 | `	default:          r = a&b; break;` |
|        - |  4026 | `	}` |
|        - |  4027 | `	/* Push the result */` |
|       39 |  4028 | `	pNos->x.iVal = r;` |
|       39 |  4029 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       39 |  4030 | `	VmPopOperand(&pTos,1);` |
|       39 |  4031 | `	break;` |
|        - |  4032 | `				 }` |
|        - |  4033 | `/* OP_BAND_STORE * * *` |
|        - |  4034 | ` *` |
|        - |  4035 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4036 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4037 | ` * two elements.` |
|        - |  4038 | `*/` |
|        - |  4039 | `/* OP_BOR_STORE * * *` |
|        - |  4040 | ` *` |
|        - |  4041 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4042 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4043 | ` * two elements.` |
|        - |  4044 | ` */` |
|        - |  4045 | `/* OP_BXOR_STORE * * *` |
|        - |  4046 | ` *` |
|        - |  4047 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4048 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4049 | ` * two elements.` |
|        - |  4050 | ` */` |
|        7 |  4051 | `case PH7_OP_BAND_STORE:` |
|        - |  4052 | `case PH7_OP_BOR_STORE:` |
|        - |  4053 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4054 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4055 | `	ph7_value *pObj;` |
|        - |  4056 | `	sxi64 a,b,r;` |
|        - |  4057 | `#ifdef UNTRUST` |
|        - |  4058 | `	if( pNos < pStack ){` |
|        - |  4059 | `		goto Abort;` |
|        - |  4060 | `	}` |
|        - |  4061 | `#endif` |
|        - |  4062 | `	/* Force the operands to be integer */` |
|       15 |  4063 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4064 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4065 | `	}` |
|       15 |  4066 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4067 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4068 | `	}` |
|        - |  4069 | `	/* Perform the requested operation */` |
|       15 |  4070 | `	a = pTos->x.iVal;` |
|       15 |  4071 | `	b = pNos->x.iVal;` |
|       15 |  4072 | `	switch(pInstr->iOp){` |
|        2 |  4073 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4074 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4075 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4076 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4077 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4078 | `	case PH7_OP_BAND:` |
|        5 |  4079 | `	default:          r = a&b; break;` |
|        - |  4080 | `	}` |
|        - |  4081 | `	/* Push the result */` |
|       15 |  4082 | `	pNos->x.iVal = r;` |
|       15 |  4083 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4084 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4085 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4086 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4087 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4088 | `	}` |
|       15 |  4089 | `	VmPopOperand(&pTos,1);` |
|       15 |  4090 | `	break;` |
|        - |  4091 | `				 }` |
|        - |  4092 | `/* OP_SHL * * *` |
|        - |  4093 | ` *` |
|        - |  4094 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4095 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4096 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4097 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4098 | ` */` |
|        - |  4099 | `/* OP_SHR * * *` |
|        - |  4100 | ` *` |
|        - |  4101 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4102 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4103 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4104 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4105 | ` */` |
|        9 |  4106 | `case PH7_OP_SHL:` |
|        - |  4107 | `case PH7_OP_SHR: {` |
|       19 |  4108 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4109 | `	sxi64 a,r;` |
|        - |  4110 | `	sxi32 b;` |
|        - |  4111 | `#ifdef UNTRUST` |
|        - |  4112 | `	if( pNos < pStack ){` |
|        - |  4113 | `		goto Abort;` |
|        - |  4114 | `	}` |
|        - |  4115 | `#endif` |
|        - |  4116 | `	/* Force the operands to be integer */` |
|       19 |  4117 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4118 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4119 | `	}` |
|       19 |  4120 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4121 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4122 | `	}` |
|        - |  4123 | `	/* Perform the requested operation */` |
|       19 |  4124 | `	a = pNos->x.iVal;` |
|       19 |  4125 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4126 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4127 | `		r = a << b;` |
|        6 |  4128 | `	}else{` |
|        9 |  4129 | `		r = a >> b;` |
|        - |  4130 | `	}` |
|        - |  4131 | `	/* Push the result */` |
|       19 |  4132 | `	pNos->x.iVal = r;` |
|       19 |  4133 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4134 | `	VmPopOperand(&pTos,1);` |
|       19 |  4135 | `	break;` |
|        - |  4136 | `				 }` |
|        - |  4137 | `/*  OP_SHL_STORE * * *` |
|        - |  4138 | ` *` |
|        - |  4139 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4140 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4141 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4142 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4143 | ` */` |
|        - |  4144 | `/* OP_SHR_STORE * * *` |
|        - |  4145 | ` *` |
|        - |  4146 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4147 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4148 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4149 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4150 | ` */` |
|        7 |  4151 | `case PH7_OP_SHL_STORE:` |
|        - |  4152 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4153 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4154 | `	ph7_value *pObj;` |
|        - |  4155 | `	sxi64 a,r;` |
|        - |  4156 | `	sxi32 b;` |
|        - |  4157 | `#ifdef UNTRUST` |
|        - |  4158 | `	if( pNos < pStack ){` |
|        - |  4159 | `		goto Abort;` |
|        - |  4160 | `	}` |
|        - |  4161 | `#endif` |
|        - |  4162 | `	/* Force the operands to be integer */` |
|       15 |  4163 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4164 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4165 | `	}` |
|       15 |  4166 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4167 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4168 | `	}` |
|        - |  4169 | `	/* Perform the requested operation */` |
|       15 |  4170 | `	a = pTos->x.iVal;` |
|       15 |  4171 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4172 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4173 | `		r = a << b;` |
|        4 |  4174 | `	}else{` |
|        9 |  4175 | `		r = a >> b;` |
|        - |  4176 | `	}` |
|        - |  4177 | `	/* Push the result */` |
|       15 |  4178 | `	pNos->x.iVal = r;` |
|       15 |  4179 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4180 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4181 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4182 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4183 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4184 | `	}` |
|       15 |  4185 | `	VmPopOperand(&pTos,1);` |
|       15 |  4186 | `	break;` |
|        - |  4187 | `				 }` |
|        - |  4188 | `/* CAT:  P1 * *` |
|        - |  4189 | ` *` |
|        - |  4190 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4191 | ` * back.` |
|        - |  4192 | ` */` |
|    57423 |  4193 | `case PH7_OP_CAT:{` |
|        - |  4194 | `	ph7_value *pNos,*pCur;` |
|   114848 |  4195 | `	if( pInstr->iP1 < 1 ){` |
|    88076 |  4196 | `		pNos = &pTos[-1];` |
|    44039 |  4197 | `	}else{` |
|    26774 |  4198 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4199 | `	}` |
|        - |  4200 | `#ifdef UNTRUST` |
|        - |  4201 | `	if( pNos < pStack ){` |
|        - |  4202 | `		goto Abort;` |
|        - |  4203 | `	}` |
|        - |  4204 | `#endif` |
|        - |  4205 | `	/* Force a string cast */` |
|   114848 |  4206 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      884 |  4207 | `		PH7_MemObjToString(pNos);` |
|      441 |  4208 | `	}` |
|   114848 |  4209 | `	pCur = &pNos[1];` |
|   231314 |  4210 | `	while( pCur <= pTos ){` |
|   116468 |  4211 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50364 |  4212 | `			PH7_MemObjToString(pCur);` |
|    25181 |  4213 | `		}` |
|        - |  4214 | `		/* Perform the concatenation */` |
|   116468 |  4215 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   116430 |  4216 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    58214 |  4217 | `		}` |
|   116468 |  4218 | `		SyBlobRelease(&pCur->sBlob);` |
|   116468 |  4219 | `		pCur++;` |
|        2 |  4220 | `	}` |
|   114848 |  4221 | `	pTos = pNos;` |
|   114848 |  4222 | `	break;` |
|        - |  4223 | `				}` |
|        - |  4224 | `/*  CAT_STORE: * * *` |
|        - |  4225 | ` *` |
|        - |  4226 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4227 | ` * back.` |
|        - |  4228 | ` */` |
|     2404 |  4229 | `case PH7_OP_CAT_STORE:{` |
|     4810 |  4230 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4231 | `	ph7_value *pObj;` |
|        - |  4232 | `#ifdef UNTRUST` |
|        - |  4233 | `	if( pNos < pStack ){` |
|        - |  4234 | `		goto Abort;` |
|        - |  4235 | `	}` |
|        - |  4236 | `#endif` |
|     4810 |  4237 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4238 | `		/* Force a string cast */` |
|      ! 0 |  4239 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4240 | `	}` |
|     4810 |  4241 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4242 | `		/* Force a string cast */` |
|      ! 0 |  4243 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4244 | `	}` |
|        - |  4245 | `	/* Perform the concatenation (Reverse order) */` |
|     4810 |  4246 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     4810 |  4247 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     2404 |  4248 | `	}` |
|        - |  4249 | `	/* Perform the store operation */` |
|     4810 |  4250 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4251 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     4810 |  4252 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     4810 |  4253 | `		PH7_MemObjStore(pTos,pObj);` |
|     2404 |  4254 | `	}` |
|     4810 |  4255 | `	PH7_MemObjStore(pTos,pNos);` |
|     4810 |  4256 | `	VmPopOperand(&pTos,1);` |
|     4810 |  4257 | `	break;` |
|        - |  4258 | `				}` |
|        - |  4259 | `/* OP_AND: * * *` |
|        - |  4260 | ` *` |
|        - |  4261 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4262 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4263 | ` * stack.` |
|        - |  4264 | ` */` |
|        - |  4265 | `/* OP_OR: * * *` |
|        - |  4266 | ` *` |
|        - |  4267 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4268 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4269 | ` * stack.` |
|        - |  4270 | ` */` |
|    99146 |  4271 | `case PH7_OP_LAND:` |
|        - |  4272 | `case PH7_OP_LOR: {` |
|   198338 |  4273 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4274 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4275 | `#ifdef UNTRUST` |
|        - |  4276 | `	if( pNos < pStack ){` |
|        - |  4277 | `		goto Abort;` |
|        - |  4278 | `	}` |
|        - |  4279 | `#endif` |
|        - |  4280 | `	/* Force a boolean cast */` |
|   198338 |  4281 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4282 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4283 | `	}` |
|   198338 |  4284 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4285 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4286 | `	}` |
|   198338 |  4287 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   198338 |  4288 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   198338 |  4289 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4290 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|   100230 |  4291 | `		v1 = and_logic[v1*3+v2];` |
|    50138 |  4292 | `	}else{` |
|        - |  4293 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|    98110 |  4294 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4295 | `	}` |
|   198338 |  4296 | `	if( v1 == 2 ){` |
|      ! 0 |  4297 | `		v1 = 1;` |
|      ! 0 |  4298 | `	}` |
|   198338 |  4299 | `	VmPopOperand(&pTos,1);` |
|   198338 |  4300 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   198338 |  4301 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   198338 |  4302 | `	break;` |
|        - |  4303 | `				 }` |
|        - |  4304 | `/* OP_LXOR: * * *` |
|        - |  4305 | ` *` |
|        - |  4306 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4307 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4308 | ` * stack.` |
|        - |  4309 | ` * According to the PHP language reference manual:` |
|        - |  4310 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4311 | ` *  TRUE,but not both.` |
|        - |  4312 | ` */` |
|        5 |  4313 | `case PH7_OP_LXOR:{` |
|       11 |  4314 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4315 | `	sxi32 v = 0;` |
|        - |  4316 | `#ifdef UNTRUST` |
|        - |  4317 | `	if( pNos < pStack ){` |
|        - |  4318 | `		goto Abort;` |
|        - |  4319 | `	}` |
|        - |  4320 | `#endif` |
|        - |  4321 | `	/* Force a boolean cast */` |
|       11 |  4322 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4323 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4324 | `	}` |
|       11 |  4325 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4326 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4327 | `	}` |
|       11 |  4328 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4329 | `		v = 1;` |
|        3 |  4330 | `	}` |
|       11 |  4331 | `	VmPopOperand(&pTos,1);` |
|       11 |  4332 | `	pTos->x.iVal = v;` |
|       11 |  4333 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4334 | `	break;` |
|        - |  4335 | `				 }` |
|        - |  4336 | `/* OP_EQ P1 P2 P3` |
|        - |  4337 | ` *` |
|        - |  4338 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4339 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4340 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4341 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4342 | ` */` |
|        - |  4343 | `/* OP_NEQ P1 P2 P3` |
|        - |  4344 | ` *` |
|        - |  4345 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4346 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4347 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4348 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4349 | ` */` |
|     3597 |  4350 | `case PH7_OP_EQ:` |
|        - |  4351 | `case PH7_OP_NEQ: {` |
|     7196 |  4352 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4353 | `	/* Perform the comparison and act accordingly */` |
|        - |  4354 | `#ifdef UNTRUST` |
|        - |  4355 | `	if( pNos < pStack ){` |
|        - |  4356 | `		goto Abort;` |
|        - |  4357 | `	}` |
|        - |  4358 | `#endif` |
|     7196 |  4359 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7196 |  4360 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       11 |  4361 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7191 |  4362 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7160 |  4363 | `		rc = rc == 0;` |
|     3581 |  4364 | `	}else{` |
|       28 |  4365 | `		rc = rc != 0;` |
|        - |  4366 | `	}` |
|     7196 |  4367 | `	VmPopOperand(&pTos,1);` |
|     7196 |  4368 | `	if( !pInstr->iP2 ){` |
|        - |  4369 | `		/* Push comparison result without taking the jump */` |
|     7196 |  4370 | `		PH7_MemObjRelease(pTos);` |
|     7196 |  4371 | `		pTos->x.iVal = rc;` |
|        - |  4372 | `		/* Invalidate any prior representation */` |
|     7196 |  4373 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3599 |  4374 | `	}else{` |
|      ! 0 |  4375 | `		if( rc ){` |
|        - |  4376 | `			/* Jump to the desired location */` |
|      ! 0 |  4377 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4378 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4379 | `		}` |
|        - |  4380 | `	}` |
|     7196 |  4381 | `	break;` |
|        - |  4382 | `				 }` |
|        - |  4383 | `/* OP_TEQ P1 P2 *` |
|        - |  4384 | ` *` |
|        - |  4385 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4386 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4387 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4388 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4389 | ` */` |
|   119636 |  4390 | `case PH7_OP_TEQ: {` |
|   239274 |  4391 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4392 | `	/* Perform the comparison and act accordingly */` |
|        - |  4393 | `#ifdef UNTRUST` |
|        - |  4394 | `	if( pNos < pStack ){` |
|        - |  4395 | `		goto Abort;` |
|        - |  4396 | `	}` |
|        - |  4397 | `#endif` |
|   239274 |  4398 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   239274 |  4399 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4400 | `		rc = 0;` |
|        2 |  4401 | `	}else{` |
|   239272 |  4402 | `		rc = rc == 0;` |
|        - |  4403 | `	}` |
|   239274 |  4404 | `	VmPopOperand(&pTos,1);` |
|   239274 |  4405 | `	if( !pInstr->iP2 ){` |
|        - |  4406 | `		/* Push comparison result without taking the jump */` |
|   239274 |  4407 | `		PH7_MemObjRelease(pTos);` |
|   239274 |  4408 | `		pTos->x.iVal = rc;` |
|        - |  4409 | `		/* Invalidate any prior representation */` |
|   239274 |  4410 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   119638 |  4411 | `	}else{` |
|      ! 0 |  4412 | `		if( rc ){` |
|        - |  4413 | `			/* Jump to the desired location */` |
|      ! 0 |  4414 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4415 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4416 | `		}` |
|        - |  4417 | `	}` |
|   239274 |  4418 | `	break;` |
|        - |  4419 | `				 }` |
|        - |  4420 | `/* OP_TNE P1 P2 *` |
|        - |  4421 | ` *` |
|        - |  4422 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4423 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4424 | ` * instruction.` |
|        - |  4425 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4426 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4427 | ` *` |
|        - |  4428 | ` */` |
|    95057 |  4429 | `case PH7_OP_TNE: {` |
|   190116 |  4430 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4431 | `	/* Perform the comparison and act accordingly */` |
|        - |  4432 | `#ifdef UNTRUST` |
|        - |  4433 | `	if( pNos < pStack ){` |
|        - |  4434 | `		goto Abort;` |
|        - |  4435 | `	}` |
|        - |  4436 | `#endif` |
|   190116 |  4437 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   190116 |  4438 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4439 | `		rc = 1;` |
|        2 |  4440 | `	}else{` |
|   190114 |  4441 | `		rc = rc != 0;` |
|        - |  4442 | `	}` |
|   190116 |  4443 | `	VmPopOperand(&pTos,1);` |
|   190116 |  4444 | `	if( !pInstr->iP2 ){` |
|        - |  4445 | `		/* Push comparison result without taking the jump */` |
|   190116 |  4446 | `		PH7_MemObjRelease(pTos);` |
|   190116 |  4447 | `		pTos->x.iVal = rc;` |
|        - |  4448 | `		/* Invalidate any prior representation */` |
|   190116 |  4449 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    95059 |  4450 | `	}else{` |
|      ! 0 |  4451 | `		if( rc ){` |
|        - |  4452 | `			/* Jump to the desired location */` |
|      ! 0 |  4453 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4454 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4455 | `		}` |
|        - |  4456 | `	}` |
|   190116 |  4457 | `	break;` |
|        - |  4458 | `				 }` |
|        - |  4459 | `/* OP_LT P1 P2 P3` |
|        - |  4460 | ` *` |
|        - |  4461 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4462 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4463 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4464 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4465 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4466 | ` *` |
|        - |  4467 | ` */` |
|        - |  4468 | `/* OP_LE P1 P2 P3` |
|        - |  4469 | ` *` |
|        - |  4470 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4471 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4472 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4473 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4474 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4475 | ` *` |
|        - |  4476 | ` */` |
|   108969 |  4477 | `case PH7_OP_LT:` |
|        - |  4478 | `case PH7_OP_LE: {` |
|   217984 |  4479 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4480 | `	/* Perform the comparison and act accordingly */` |
|        - |  4481 | `#ifdef UNTRUST` |
|        - |  4482 | `	if( pNos < pStack ){` |
|        - |  4483 | `		goto Abort;` |
|        - |  4484 | `	}` |
|        - |  4485 | `#endif` |
|   217984 |  4486 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   217984 |  4487 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4488 | `		rc = 0;` |
|   217980 |  4489 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      395 |  4490 | `		rc = rc < 1;` |
|      198 |  4491 | `	}else{` |
|   217582 |  4492 | `		rc = rc < 0;` |
|        - |  4493 | `	}` |
|   217984 |  4494 | `	VmPopOperand(&pTos,1);` |
|   217984 |  4495 | `	if( !pInstr->iP2 ){` |
|        - |  4496 | `		/* Push comparison result without taking the jump */` |
|   217984 |  4497 | `		PH7_MemObjRelease(pTos);` |
|   217984 |  4498 | `		pTos->x.iVal = rc;` |
|        - |  4499 | `		/* Invalidate any prior representation */` |
|   217984 |  4500 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   109015 |  4501 | `	}else{` |
|      ! 0 |  4502 | `		if( rc ){` |
|        - |  4503 | `			/* Jump to the desired location */` |
|      ! 0 |  4504 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4505 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4506 | `		}` |
|        - |  4507 | `	}` |
|   217984 |  4508 | `	break;` |
|        - |  4509 | `				}` |
|        - |  4510 | `/* OP_GT P1 P2 P3` |
|        - |  4511 | ` *` |
|        - |  4512 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4513 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4514 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4515 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4516 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4517 | ` *` |
|        - |  4518 | ` */` |
|        - |  4519 | `/* OP_GE P1 P2 P3` |
|        - |  4520 | ` *` |
|        - |  4521 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4522 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4523 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4524 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4525 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4526 | ` *` |
|        - |  4527 | ` */` |
|    46737 |  4528 | `case PH7_OP_GT:` |
|        - |  4529 | `case PH7_OP_GE: {` |
|    93476 |  4530 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4531 | `	/* Perform the comparison and act accordingly */` |
|        - |  4532 | `#ifdef UNTRUST` |
|        - |  4533 | `	if( pNos < pStack ){` |
|        - |  4534 | `		goto Abort;` |
|        - |  4535 | `	}` |
|        - |  4536 | `#endif` |
|    93476 |  4537 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    93476 |  4538 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4539 | `		rc = 0;` |
|    93472 |  4540 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    93320 |  4541 | `		rc = rc >= 0;` |
|    46661 |  4542 | `	}else{` |
|      150 |  4543 | `		rc = rc > 0;` |
|        - |  4544 | `	}` |
|    93476 |  4545 | `	VmPopOperand(&pTos,1);` |
|    93476 |  4546 | `	if( !pInstr->iP2 ){` |
|        - |  4547 | `		/* Push comparison result without taking the jump */` |
|    93476 |  4548 | `		PH7_MemObjRelease(pTos);` |
|    93476 |  4549 | `		pTos->x.iVal = rc;` |
|        - |  4550 | `		/* Invalidate any prior representation */` |
|    93476 |  4551 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    46739 |  4552 | `	}else{` |
|      ! 0 |  4553 | `		if( rc ){` |
|        - |  4554 | `			/* Jump to the desired location */` |
|      ! 0 |  4555 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4556 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4557 | `		}` |
|        - |  4558 | `	}` |
|    93476 |  4559 | `	break;` |
|        - |  4560 | `				}` |
|        - |  4561 | `/* OP_SEQ P1 P2 *` |
|        - |  4562 | ` * Strict string comparison.` |
|        - |  4563 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4564 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4565 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4566 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4567 | ` * use PH7_OP_EQ.` |
|        - |  4568 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4569 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4570 | ` */` |
|        - |  4571 | `/* OP_SNE P1 P2 *` |
|        - |  4572 | ` * Strict string comparison.` |
|        - |  4573 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4574 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4575 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4576 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4577 | ` * use PH7_OP_EQ.` |
|        - |  4578 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4579 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4580 | ` */` |
|       18 |  4581 | `case PH7_OP_SEQ:` |
|        - |  4582 | `case PH7_OP_SNE: {` |
|       38 |  4583 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4584 | `	SyString s1,s2;` |
|        - |  4585 | `	/* Perform the comparison and act accordingly */` |
|        - |  4586 | `#ifdef UNTRUST` |
|        - |  4587 | `	if( pNos < pStack ){` |
|        - |  4588 | `		goto Abort;` |
|        - |  4589 | `	}` |
|        - |  4590 | `#endif` |
|        - |  4591 | `	/* Force a string cast */` |
|       38 |  4592 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4593 | `		PH7_MemObjToString(pTos);` |
|        2 |  4594 | `	}` |
|       38 |  4595 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4596 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4597 | `	}` |
|       38 |  4598 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4599 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4600 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4601 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4602 | `		rc = rc != 0;` |
|      ! 0 |  4603 | `	}else{` |
|       38 |  4604 | `		rc = rc == 0;` |
|        - |  4605 | `	}` |
|       38 |  4606 | `	VmPopOperand(&pTos,1);` |
|       38 |  4607 | `	if( !pInstr->iP2 ){` |
|        - |  4608 | `		/* Push comparison result without taking the jump */` |
|       38 |  4609 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4610 | `		pTos->x.iVal = rc;` |
|        - |  4611 | `		/* Invalidate any prior representation */` |
|       38 |  4612 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4613 | `	}else{` |
|      ! 0 |  4614 | `		if( rc ){` |
|        - |  4615 | `			/* Jump to the desired location */` |
|      ! 0 |  4616 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4617 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4618 | `		}` |
|        - |  4619 | `	}` |
|       38 |  4620 | `	break;` |
|        - |  4621 | `				 }` |
|        - |  4622 | `/*` |
|        - |  4623 | ` * OP_LOAD_REF * * *` |
|        - |  4624 | ` * Push the index of a referenced object on the stack.` |
|        - |  4625 | ` */` |
|       57 |  4626 | `case PH7_OP_LOAD_REF: {` |
|        - |  4627 | `	sxu32 nIdx;` |
|        - |  4628 | `#ifdef UNTRUST` |
|        - |  4629 | `	if( pTos < pStack ){` |
|        - |  4630 | `		goto Abort;` |
|        - |  4631 | `	}` |
|        - |  4632 | `#endif` |
|        - |  4633 | `	/* Extract memory object index */` |
|      115 |  4634 | `	nIdx = pTos->nIdx;` |
|      115 |  4635 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4636 | `		/* Nullify the object */` |
|       95 |  4637 | `		PH7_MemObjRelease(pTos);` |
|        - |  4638 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4639 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4640 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4641 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4642 | `	}` |
|      115 |  4643 | `	break;` |
|        - |  4644 | `					  }` |
|        - |  4645 | `/*` |
|        - |  4646 | ` * OP_STORE_REF * * P3` |
|        - |  4647 | ` * Perform an assignment operation by reference.` |
|        - |  4648 | ` */` |
|       14 |  4649 | ` case PH7_OP_STORE_REF: {` |
|       30 |  4650 | `	 SyString sName = { 0 , 0 };` |
|        - |  4651 | `	 VmFrame *pFrameLocal;` |
|        - |  4652 | `	SyHashEntry *pEntry;` |
|        - |  4653 | `	sxu32 nIdx;` |
|        - |  4654 | `#ifdef UNTRUST` |
|        - |  4655 | `	if( pTos < pStack ){` |
|        - |  4656 | `		goto Abort;` |
|        - |  4657 | `	}` |
|        - |  4658 | `#endif` |
|       30 |  4659 | `	if( pInstr->p3 == 0 ){` |
|        - |  4660 | `		char *zName;` |
|        - |  4661 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4662 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4663 | `			/* Force a string cast */` |
|      ! 0 |  4664 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4665 | `		}` |
|      ! 0 |  4666 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4667 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4668 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4669 | `			if( zName ){` |
|      ! 0 |  4670 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4671 | `			}` |
|      ! 0 |  4672 | `		}` |
|      ! 0 |  4673 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4674 | `		pTos--;` |
|      ! 0 |  4675 | `	}else{` |
|       30 |  4676 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4677 | `	}` |
|       30 |  4678 | `	nIdx = pTos->nIdx;` |
|       30 |  4679 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4680 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4681 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4682 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4683 | `		}else{` |
|        - |  4684 | `			ph7_value *pObj;` |
|        - |  4685 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4686 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4687 | `			if( pObj == 0 ){` |
|      ! 0 |  4688 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4689 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4690 | `				goto Abort;` |
|        - |  4691 | `			}` |
|        - |  4692 | `			/* Perform the store operation */` |
|      ! 0 |  4693 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4694 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4695 | `		}` |
|       30 |  4696 | `	}else if( sName.nByte > 0){` |
|       30 |  4697 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4698 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4699 | `		}else{` |
|       30 |  4700 | `			pFrameLocal = pVm->pFrame;` |
|       50 |  4701 | `			while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4702 | `				/* Safely ignore the exception frame */` |
|       21 |  4703 | `				pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4704 | `			}` |
|        - |  4705 | `			/* Query the local frame */` |
|       30 |  4706 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       30 |  4707 | `			if( pEntry ){` |
|      ! 0 |  4708 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4709 | `			}else{` |
|       30 |  4710 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       30 |  4711 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4712 | `					/* Insert in the $GLOBALS array */` |
|       26 |  4713 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       12 |  4714 | `				}` |
|       30 |  4715 | `				if( rc == SXRET_OK ){` |
|       30 |  4716 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       14 |  4717 | `				}` |
|        - |  4718 | `			}` |
|        - |  4719 | `		}` |
|       14 |  4720 | `	}` |
|       30 |  4721 | `	break;` |
|        - |  4722 | `				 }` |
|        - |  4723 | `/*` |
|        - |  4724 | ` * OP_UPLINK P1 * *` |
|        - |  4725 | ` * Link a variable to the top active VM frame.` |
|        - |  4726 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4727 | ` */` |
|       23 |  4728 | `case PH7_OP_UPLINK: {` |
|       47 |  4729 | `	if( pVm->pFrame->pParent ){` |
|       47 |  4730 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4731 | `		SyString sName;` |
|        - |  4732 | `		/* Perform the link */` |
|       95 |  4733 | `		while( pLink <= pTos ){` |
|       49 |  4734 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4735 | `				/* Force a string cast */` |
|      ! 0 |  4736 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4737 | `			}` |
|       49 |  4738 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       49 |  4739 | `			if( sName.nByte > 0 ){` |
|       49 |  4740 | `				VmFrameLink(&(*pVm),&sName);` |
|       24 |  4741 | `			}` |
|       49 |  4742 | `			pLink++;` |
|        1 |  4743 | `		}` |
|       23 |  4744 | `	}` |
|       47 |  4745 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       47 |  4746 | `	break;` |
|        - |  4747 | `					}` |
|        - |  4748 | `/*` |
|        - |  4749 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4750 | ` * Push an exception in the corresponding container so that` |
|        - |  4751 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4752 | ` */` |
|       10 |  4753 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       22 |  4754 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4755 | `	VmFrame *pFrameLocal;` |
|       22 |  4756 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4757 | `	/* Create the exception frame */` |
|       22 |  4758 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       22 |  4759 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4760 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4761 | `		goto Abort;` |
|        - |  4762 | `	}` |
|        - |  4763 | `	/* Mark the special frame */` |
|       22 |  4764 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       22 |  4765 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4766 | `	/* Point to the frame that trigger the exception */` |
|       22 |  4767 | `	pFrameLocal = pFrameLocal->pParent;` |
|       34 |  4768 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|       13 |  4769 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4770 | `	}` |
|       22 |  4771 | `	pException->pFrame = pFrameLocal;` |
|       22 |  4772 | `	break;` |
|        - |  4773 | `							}` |
|        - |  4774 | `/*` |
|        - |  4775 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4776 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4777 | ` */` |
|        9 |  4778 | `case PH7_OP_POP_EXCEPTION: {` |
|       20 |  4779 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       20 |  4780 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4781 | `		ph7_exception **apException;` |
|        - |  4782 | `		/* Pop the loaded exception */` |
|        7 |  4783 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        7 |  4784 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|        7 |  4785 | `			(void)SySetPop(&pVm->aException);` |
|        3 |  4786 | `		}` |
|        3 |  4787 | `	}` |
|       20 |  4788 | `	pException->pFrame = 0;` |
|        - |  4789 | `	/* Leave the exception frame */` |
|       20 |  4790 | `	VmLeaveFrame(&(*pVm));` |
|       20 |  4791 | `	break;` |
|        - |  4792 | `							}` |
|        - |  4793 |  |
|        - |  4794 | `/*` |
|        - |  4795 | ` * OP_THROW * P2 *` |
|        - |  4796 | ` * Throw an user exception.` |
|        - |  4797 | ` */` |
|       10 |  4798 | `case PH7_OP_THROW: {` |
|       22 |  4799 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       22 |  4800 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4801 | `#ifdef UNTRUST` |
|        - |  4802 | `	if( pTos < pStack ){` |
|        - |  4803 | `		goto Abort;` |
|        - |  4804 | `	}` |
|        - |  4805 | `#endif` |
|       28 |  4806 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4807 | `		/* Safely ignore the exception frame */` |
|        8 |  4808 | `		pFrameLocal = pFrameLocal->pParent;` |
|        2 |  4809 | `	}` |
|        - |  4810 | `	/* Tell the upper layer that an exception was thrown */` |
|       22 |  4811 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       22 |  4812 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       22 |  4813 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4814 | `		ph7_class *pException;` |
|        - |  4815 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4816 | `		 */` |
|       22 |  4817 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       22 |  4818 | `		if( pException == 0 \|\| !VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4819 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4820 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4821 | `			if( rc == SXERR_ABORT ){` |
|        - |  4822 | `				/* Abort processing immediately */` |
|      ! 0 |  4823 | `				goto Abort;` |
|        - |  4824 | `			}` |
|      ! 0 |  4825 | `		}else{` |
|        - |  4826 | `			/* Throw the exception */` |
|       22 |  4827 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       22 |  4828 | `			if( rc == SXERR_ABORT ){` |
|        - |  4829 | `				/* Abort processing immediately */` |
|        7 |  4830 | `				goto Abort;` |
|        - |  4831 | `			}` |
|        - |  4832 | `		}` |
|        9 |  4833 | `	}else{` |
|        - |  4834 | `		/* Expecting a class instance */` |
|      ! 0 |  4835 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  4836 | `		if( rc == SXERR_ABORT ){` |
|        - |  4837 | `			/* Abort processing immediately */` |
|      ! 0 |  4838 | `			goto Abort;` |
|        - |  4839 | `		}` |
|        - |  4840 | `	}` |
|        - |  4841 | `	/* Pop the top entry */` |
|       16 |  4842 | `	VmPopOperand(&pTos,1);` |
|        - |  4843 | `	/* Perform an unconditional jump */` |
|       16 |  4844 | `	pc = nJump - 1;` |
|       16 |  4845 | `	break;` |
|        - |  4846 | `				   }` |
|        - |  4847 | `/*` |
|        - |  4848 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  4849 | ` * Prepare a foreach step.` |
|        - |  4850 | ` */` |
|     4385 |  4851 | `case PH7_OP_FOREACH_INIT: {` |
|     8772 |  4852 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4853 | `	void *pName;` |
|        - |  4854 | `#ifdef UNTRUST` |
|        - |  4855 | `	if( pTos < pStack ){` |
|        - |  4856 | `		goto Abort;` |
|        - |  4857 | `	}` |
|        - |  4858 | `#endif` |
|     8772 |  4859 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4860 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  4861 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4862 | `			/* Force a string cast */` |
|      ! 0 |  4863 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4864 | `		}` |
|        - |  4865 | `		/* Duplicate name */` |
|      ! 0 |  4866 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4867 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4868 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4869 | `		}` |
|      ! 0 |  4870 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4871 | `	}` |
|     8772 |  4872 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  4873 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4874 | `			/* Force a string cast */` |
|      ! 0 |  4875 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4876 | `		}` |
|        - |  4877 | `		/* Duplicate name */` |
|      ! 0 |  4878 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4879 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4880 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4881 | `		}` |
|      ! 0 |  4882 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4883 | `	}` |
|        - |  4884 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     8772 |  4885 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4886 | `		/* Jump out of the loop */` |
|      ! 0 |  4887 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4888 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4889 | `		}` |
|      ! 0 |  4890 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4891 | `	}else{` |
|        - |  4892 | `		ph7_foreach_step *pStep;` |
|     8772 |  4893 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     8772 |  4894 | `		if( pStep == 0 ){` |
|      ! 0 |  4895 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4896 | `			/* Jump out of the loop */` |
|      ! 0 |  4897 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4898 | `		}else{` |
|        - |  4899 | `			/* Zero the structure */` |
|     8772 |  4900 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4901 | `			/* Prepare the step */` |
|     8772 |  4902 | `			pStep->iFlags = pInfo->iFlags;` |
|     8772 |  4903 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     8764 |  4904 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4905 | `				/* Reset the internal loop cursor */` |
|     8764 |  4906 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4907 | `				/* Mark the step */` |
|     8764 |  4908 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     8764 |  4909 | `				pStep->xIter.pMap = pMap;` |
|     8764 |  4910 | `				pMap->iRef++;` |
|     4383 |  4911 | `			}else{` |
|        9 |  4912 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4913 | `				/* Reset the loop cursor */` |
|        9 |  4914 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|        - |  4915 | `				/* Mark the step */` |
|        9 |  4916 | `				pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  4917 | `				pStep->xIter.pThis = pThis;` |
|        9 |  4918 | `				pThis->iRef++;` |
|        - |  4919 | `			}` |
|        - |  4920 | `		}` |
|     8772 |  4921 | `		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  4922 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  4923 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  4924 | `			/* Jump out of the loop */` |
|      ! 0 |  4925 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4926 | `		}` |
|        - |  4927 | `	}` |
|     8772 |  4928 | `	VmPopOperand(&pTos,1);` |
|     8772 |  4929 | `	break;` |
|        - |  4930 | `						  }` |
|        - |  4931 | `/*` |
|        - |  4932 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  4933 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  4934 | ` */` |
|    70974 |  4935 | `case PH7_OP_FOREACH_STEP: {` |
|   141950 |  4936 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4937 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  4938 | `	ph7_value *pValue;` |
|        - |  4939 | `	VmFrame *pFrameLocal;` |
|        - |  4940 | `	/* Peek the last step */` |
|   141950 |  4941 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   141950 |  4942 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   141950 |  4943 | `	pFrameLocal = pVm->pFrame;` |
|   146982 |  4944 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4945 | `		/* Safely ignore the exception frame */` |
|     5033 |  4946 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4947 | `	}` |
|   141950 |  4948 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   141926 |  4949 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  4950 | `		ph7_hashmap_node *pNode;` |
|        - |  4951 | `		/* Extract the current node value */` |
|   141926 |  4952 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   141926 |  4953 | `		if( pNode == 0 ){` |
|        - |  4954 | `			/* No more entry to process */` |
|     8764 |  4955 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     8764 |  4956 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4957 | `				/* Break the reference with the last element */` |
|        5 |  4958 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  4959 | `			}` |
|        - |  4960 | `			/* Automatically reset the loop cursor */` |
|     8764 |  4961 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4962 | `			/* Cleanup the mess left behind */` |
|     8764 |  4963 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     8764 |  4964 | `			SySetPop(&pInfo->aStep);` |
|     8764 |  4965 | `			PH7_HashmapUnref(pMap);` |
|     4383 |  4966 | `		}else{` |
|   133164 |  4967 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      259 |  4968 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      259 |  4969 | `				if( pKey ){` |
|      259 |  4970 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      129 |  4971 | `				}` |
|      129 |  4972 | `			}` |
|   133164 |  4973 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4974 | `				SyHashEntry *pEntry;` |
|        - |  4975 | `				/* Pass by reference */` |
|       13 |  4976 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       13 |  4977 | `				if( pEntry ){` |
|       13 |  4978 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        7 |  4979 | `				}else{` |
|      ! 0 |  4980 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  4981 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  4982 | `				}` |
|        7 |  4983 | `			}else{` |
|        - |  4984 | `				/* Make a copy of the entry value */` |
|   133152 |  4985 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   133152 |  4986 | `				if( pValue ){` |
|   133152 |  4987 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    66575 |  4988 | `				}` |
|        - |  4989 | `			}` |
|        - |  4990 | `		}` |
|    70964 |  4991 | `	}else{` |
|       25 |  4992 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  4993 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  4994 | `		SyHashEntry *pEntry;` |
|        - |  4995 | `		/* Point to the next attribute */` |
|       29 |  4996 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  4997 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  4998 | `			/* Check access permission */` |
|       31 |  4999 | `			if( VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5000 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5001 | `					break; /* Access is granted */` |
|        - |  5002 | `			}` |
|        1 |  5003 | `		}` |
|       25 |  5004 | `		if( pEntry == 0 ){` |
|        - |  5005 | `			/* Clean up the mess left behind */` |
|        9 |  5006 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5007 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5008 | `				/* Break the reference with the last element */` |
|        3 |  5009 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5010 | `			}` |
|        9 |  5011 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5012 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5013 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5014 | `		}else{` |
|       17 |  5015 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5016 | `			ph7_value *pAttrValue;` |
|       17 |  5017 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5018 | `				/* Fill with the current attribute name */` |
|       17 |  5019 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5020 | `				if( pKey ){` |
|       17 |  5021 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5022 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5023 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5024 | `				}` |
|        8 |  5025 | `			}` |
|        - |  5026 | `			/* Extract attribute value */` |
|       17 |  5027 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5028 | `			if( pAttrValue ){` |
|       17 |  5029 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5030 | `					/* Pass by reference */` |
|        3 |  5031 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5032 | `					if( pEntry ){` |
|        3 |  5033 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5034 | `					}else{` |
|      ! 0 |  5035 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5036 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5037 | `					}` |
|        2 |  5038 | `				}else{` |
|        - |  5039 | `					/* Make a copy of the attribute value */` |
|       15 |  5040 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5041 | `					if( pValue ){` |
|       15 |  5042 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5043 | `					}` |
|        - |  5044 | `				}` |
|        8 |  5045 | `			}` |
|        - |  5046 | `		}` |
|        - |  5047 | `	}` |
|   141950 |  5048 | `	break;` |
|        - |  5049 | `						  }` |
|        - |  5050 | `/*` |
|        - |  5051 | ` * OP_MEMBER P1 P2` |
|        - |  5052 | ` * Load class attribute/method on the stack.` |
|        - |  5053 | ` */` |
|     1488 |  5054 | `case PH7_OP_MEMBER: {` |
|        - |  5055 | `	ph7_class_instance *pThis;` |
|        - |  5056 | `	ph7_value *pNos;` |
|        - |  5057 | `	SyString sName;` |
|     2978 |  5058 | `	if( !pInstr->iP1 ){` |
|     2920 |  5059 | `		pNos = &pTos[-1];` |
|        - |  5060 | `#ifdef UNTRUST` |
|        - |  5061 | `		if( pNos < pStack ){` |
|        - |  5062 | `			goto Abort;` |
|        - |  5063 | `		}` |
|        - |  5064 | `#endif` |
|     2920 |  5065 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5066 | `			ph7_class *pClass;` |
|        - |  5067 | `			/* Class already instantiated */` |
|     2920 |  5068 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5069 | `			/* Point to the instantiated class */` |
|     2920 |  5070 | `			pClass = pThis->pClass;` |
|        - |  5071 | `			/* Extract attribute name first */` |
|     2920 |  5072 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     2920 |  5073 | `			if( pInstr->iP2 ){` |
|        - |  5074 | `				/* Method call */` |
|      120 |  5075 | `				ph7_class_method *pMeth = 0;` |
|      120 |  5076 | `				if( sName.nByte > 0 ){` |
|        - |  5077 | `					/* Extract the target method */` |
|      120 |  5078 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       59 |  5079 | `				}` |
|      120 |  5080 | `				if( pMeth == 0 ){` |
|      ! 0 |  5081 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5082 | `						&pClass->sName,&sName` |
|        - |  5083 | `						);` |
|        - |  5084 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5085 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5086 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5087 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5088 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5089 | `				}else{` |
|        - |  5090 | `					/* Push method name on the stack */` |
|      120 |  5091 | `					PH7_MemObjRelease(pTos);` |
|      120 |  5092 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      120 |  5093 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5094 | `				}` |
|      120 |  5095 | `				pTos->nIdx = SXU32_HIGH;` |
|       61 |  5096 | `			}else{` |
|        - |  5097 | `				/* Attribute access */` |
|     2802 |  5098 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5099 | `				SyHashEntry *pEntry;` |
|        - |  5100 | `				/* Extract the target attribute */` |
|     2802 |  5101 | `				if( sName.nByte > 0 ){` |
|     2802 |  5102 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     2802 |  5103 | `					if( pEntry ){` |
|        - |  5104 | `						/* Point to the attribute value */` |
|     2800 |  5105 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1399 |  5106 | `					}` |
|     1400 |  5107 | `				}` |
|     2802 |  5108 | `				if( pObjAttr == 0 ){` |
|        - |  5109 | `					/* No such attribute,load null */` |
|        4 |  5110 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5111 | `						&pClass->sName,&sName);` |
|        - |  5112 | `					/* Call the __get magic method if available */` |
|        3 |  5113 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5114 | `				}` |
|     2802 |  5115 | `				VmPopOperand(&pTos,1);` |
|        - |  5116 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5117 | `				 * This is due to the following case:` |
|        - |  5118 | `				 *     (new TestClass())->foo;` |
|        - |  5119 | `				 */` |
|     2802 |  5120 | `				pThis->iRef++;` |
|     2802 |  5121 | `				PH7_MemObjRelease(pTos);` |
|     2802 |  5122 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     2802 |  5123 | `				if( pObjAttr ){` |
|     2800 |  5124 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5125 | `					/* Check attribute access */` |
|     2800 |  5126 | `					if( VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5127 | `						/* Load attribute */` |
|     2800 |  5128 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     2800 |  5129 | `						if( pValue ){` |
|     2800 |  5130 | `							if( pThis->iRef < 2 ){` |
|        - |  5131 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5132 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5133 | `								 */` |
|        3 |  5134 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5135 | `							}else{` |
|        - |  5136 | `								/* Simple load */` |
|     2798 |  5137 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5138 | `							}` |
|     2800 |  5139 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     2798 |  5140 | `								if( pThis->iRef > 1 ){` |
|        - |  5141 | `									/* Load attribute index */` |
|     2796 |  5142 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1397 |  5143 | `								}` |
|     1398 |  5144 | `							}` |
|     1399 |  5145 | `						}` |
|     1399 |  5146 | `					}` |
|     1399 |  5147 | `				}` |
|        - |  5148 | `				/* Safely unreference the object */` |
|     2802 |  5149 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5150 | `			}` |
|     1461 |  5151 | `		}else{` |
|      ! 0 |  5152 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5153 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5154 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5155 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5156 | `		}` |
|     1461 |  5157 | `	}else{` |
|        - |  5158 | `		/* Static member access using class name */` |
|       59 |  5159 | `		pNos = pTos;` |
|       59 |  5160 | `		pThis = 0;` |
|       59 |  5161 | `		if( !pInstr->p3 ){` |
|       57 |  5162 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       57 |  5163 | `			pNos--;` |
|        - |  5164 | `#ifdef UNTRUST` |
|        - |  5165 | `			if( pNos < pStack ){` |
|        - |  5166 | `				goto Abort;` |
|        - |  5167 | `			}` |
|        - |  5168 | `#endif` |
|       29 |  5169 | `		}else{` |
|        - |  5170 | `			/* Attribute name already computed */` |
|        3 |  5171 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5172 | `		}` |
|       59 |  5173 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|       59 |  5174 | `			ph7_class *pClass = 0;` |
|       59 |  5175 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5176 | `				/* Class already instantiated */` |
|      ! 0 |  5177 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5178 | `				pClass = pThis->pClass;` |
|      ! 0 |  5179 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5180 | `			}else{` |
|        - |  5181 | `				/* Try to extract the target class */` |
|       59 |  5182 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       88 |  5183 | `					pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pNos->sBlob),` |
|       29 |  5184 | `						SyBlobLength(&pNos->sBlob),FALSE,0);` |
|       29 |  5185 | `				}` |
|        - |  5186 | `			}` |
|       59 |  5187 | `			if( pClass == 0 ){` |
|        - |  5188 | `				/* Undefined class */` |
|      ! 0 |  5189 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5190 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5191 | `					);` |
|      ! 0 |  5192 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5193 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5194 | `				}` |
|      ! 0 |  5195 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5196 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5197 | `			}else{` |
|       59 |  5198 | `				if( pInstr->iP2 ){` |
|        - |  5199 | `					/* Method call */` |
|       25 |  5200 | `					ph7_class_method *pMeth = 0;` |
|       25 |  5201 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5202 | `						/* Extract the target method */` |
|       25 |  5203 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       12 |  5204 | `					}` |
|       25 |  5205 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5206 | `						if( pMeth ){` |
|      ! 0 |  5207 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5208 | `								&pClass->sName,&sName` |
|        - |  5209 | `								);` |
|      ! 0 |  5210 | `						}else{` |
|      ! 0 |  5211 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5212 | `								&pClass->sName,&sName` |
|        - |  5213 | `								);` |
|        - |  5214 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5215 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5216 | `						}` |
|        - |  5217 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5218 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5219 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5220 | `						}` |
|      ! 0 |  5221 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5222 | `					}else{` |
|        - |  5223 | `						/* Push method name on the stack */` |
|       25 |  5224 | `						PH7_MemObjRelease(pTos);` |
|       25 |  5225 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       25 |  5226 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5227 | `					}` |
|       25 |  5228 | `					pTos->nIdx = SXU32_HIGH;` |
|       13 |  5229 | `				}else{` |
|        - |  5230 | `					/* Attribute access */` |
|       35 |  5231 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5232 | `					/* Check for special ::class pseudo-constant */` |
|       49 |  5233 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       28 |  5234 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5235 | `						/* ::class returns the fully qualified class name */` |
|        - |  5236 | `						/* Pop the attribute name from the stack */` |
|       27 |  5237 | `						if( !pInstr->p3 ){` |
|       27 |  5238 | `							VmPopOperand(&pTos,1);` |
|       13 |  5239 | `						}` |
|       27 |  5240 | `						PH7_MemObjRelease(pTos);` |
|        - |  5241 | `						/* Load the class name */` |
|       27 |  5242 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       27 |  5243 | `						pTos->nIdx = SXU32_HIGH;` |
|       14 |  5244 | `					}else{` |
|        - |  5245 | `						/* Extract the target attribute */` |
|        9 |  5246 | `						if( sName.nByte > 0 ){` |
|        9 |  5247 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        4 |  5248 | `						}` |
|        9 |  5249 | `						if( pAttr == 0 ){` |
|        - |  5250 | `							/* No such attribute,load null */` |
|      ! 0 |  5251 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5252 | `								&pClass->sName,&sName);` |
|        - |  5253 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5254 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5255 | `						}` |
|        - |  5256 | `						/* Pop the attribute name from the stack */` |
|        9 |  5257 | `						if( !pInstr->p3 ){` |
|        7 |  5258 | `							VmPopOperand(&pTos,1);` |
|        3 |  5259 | `						}` |
|        9 |  5260 | `						PH7_MemObjRelease(pTos);` |
|        9 |  5261 | `						pTos->nIdx = SXU32_HIGH;` |
|        9 |  5262 | `						if( pAttr ){` |
|        9 |  5263 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5264 | `								/* Access to a non static attribute */` |
|      ! 0 |  5265 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5266 | `									&pClass->sName,&pAttr->sName` |
|        - |  5267 | `									);` |
|      ! 0 |  5268 | `							}else{` |
|        - |  5269 | `								ph7_value *pValue;` |
|        - |  5270 | `								/* Check if the access to the attribute is allowed */` |
|        9 |  5271 | `								if( VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5272 | `									/* Load the desired attribute */` |
|        9 |  5273 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|        9 |  5274 | `									if( pValue ){` |
|        9 |  5275 | `										PH7_MemObjLoad(pValue,pTos);` |
|        9 |  5276 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5277 | `											/* Load index number */` |
|        3 |  5278 | `											pTos->nIdx = pAttr->nIdx;` |
|        1 |  5279 | `										}` |
|        4 |  5280 | `									}` |
|        4 |  5281 | `								}` |
|        - |  5282 | `							}` |
|        4 |  5283 | `						}` |
|        - |  5284 | `					}` |
|        - |  5285 | `				}` |
|       59 |  5286 | `				if( pThis ){` |
|        - |  5287 | `					/* Safely unreference the object */` |
|      ! 0 |  5288 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5289 | `				}` |
|        - |  5290 | `			}` |
|       30 |  5291 | `		}else{` |
|        - |  5292 | `			/* Pop operands */` |
|      ! 0 |  5293 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5294 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5295 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5296 | `			}` |
|      ! 0 |  5297 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5298 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5299 | `		}` |
|        - |  5300 | `	}` |
|     2978 |  5301 | `	break;` |
|        - |  5302 | `					}` |
|        - |  5303 | `/*` |
|        - |  5304 | ` * OP_NEW P1 * * *` |
|        - |  5305 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5306 | ` */` |
|      252 |  5307 | `case PH7_OP_NEW: {` |
|      506 |  5308 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      506 |  5309 | `	ph7_class *pClass = 0;` |
|        - |  5310 | `	ph7_class_instance *pNew;` |
|      506 |  5311 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5312 | `		/* Try to extract the desired class */` |
|      758 |  5313 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      504 |  5314 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      252 |  5315 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5316 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5317 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5318 | `	}` |
|      506 |  5319 | `	if( pClass == 0 ){` |
|        - |  5320 | `		/* No such class */` |
|      ! 0 |  5321 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined,PH7 is loading NULL",` |
|      ! 0 |  5322 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5323 | `			);` |
|      ! 0 |  5324 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5325 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5326 | `			/* Pop given arguments */` |
|      ! 0 |  5327 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5328 | `		}` |
|      ! 0 |  5329 | `	}else{` |
|        - |  5330 | `		ph7_class_method *pCons;` |
|        - |  5331 | `		/* Create a new class instance */` |
|      506 |  5332 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      506 |  5333 | `		if( pNew == 0 ){` |
|      ! 0 |  5334 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5335 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5336 | `				&pClass->sName` |
|        - |  5337 | `			);` |
|      ! 0 |  5338 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5339 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5340 | `				/* Pop given arguments */` |
|      ! 0 |  5341 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5342 | `			}` |
|      ! 0 |  5343 | `			break;` |
|        - |  5344 | `		}` |
|        - |  5345 | `		/* Check if a constructor is available */` |
|      506 |  5346 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      506 |  5347 | `		if( pCons == 0 ){` |
|      450 |  5348 | `			SyString *pName = &pClass->sName;` |
|        - |  5349 | `			/* Check for a constructor with the same base class name */` |
|      450 |  5350 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      224 |  5351 | `		}` |
|      506 |  5352 | `		if( pCons ){` |
|        - |  5353 | `			/* Call the class constructor */` |
|       58 |  5354 | `			SySetReset(&aArg);` |
|      104 |  5355 | `			while( pArg < pTos ){` |
|       48 |  5356 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       48 |  5357 | `				pArg++;` |
|        2 |  5358 | `			}` |
|       58 |  5359 | `			if( pVm->bErrReport ){` |
|        - |  5360 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5361 | `				sxu32 n;` |
|       15 |  5362 | `				n = SySetUsed(&aArg);` |
|        - |  5363 | `				/* Emit a notice for missing arguments */` |
|       39 |  5364 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       25 |  5365 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       25 |  5366 | `					if( pFuncArg ){` |
|       25 |  5367 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5368 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5369 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5370 | `						}` |
|       12 |  5371 | `					}` |
|       25 |  5372 | `					n++;` |
|        1 |  5373 | `				}` |
|        7 |  5374 | `			}` |
|       58 |  5375 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5376 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       58 |  5377 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5378 | `				pNew->iRef = 1;` |
|      ! 0 |  5379 | `			}` |
|       28 |  5380 | `		}` |
|      506 |  5381 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5382 | `			/* Pop given arguments */` |
|       42 |  5383 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       20 |  5384 | `		}` |
|      506 |  5385 | `		PH7_MemObjRelease(pTos);` |
|      506 |  5386 | `		pTos->x.pOther = pNew;` |
|      506 |  5387 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5388 | `	}` |
|      506 |  5389 | `	break;` |
|        - |  5390 | `				 }` |
|        - |  5391 | `/*` |
|        - |  5392 | ` * OP_CLONE * * *` |
|        - |  5393 | ` * Perfome a clone operation.` |
|        - |  5394 | ` */` |
|       23 |  5395 | `case PH7_OP_CLONE: {` |
|        - |  5396 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5397 | `#ifdef UNTRUST` |
|        - |  5398 | `	if( pTos < pStack ){` |
|        - |  5399 | `		goto Abort;` |
|        - |  5400 | `	}` |
|        - |  5401 | `#endif` |
|        - |  5402 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5403 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5404 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5405 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5406 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5407 | `		break;` |
|        - |  5408 | `	}` |
|        - |  5409 | `	/* Point to the source */` |
|       44 |  5410 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5411 | `	/* Perform the clone operation */` |
|       44 |  5412 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5413 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5414 | `	if( pClone == 0 ){` |
|      ! 0 |  5415 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5416 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5417 | `	}else{` |
|        - |  5418 | `		/* Load the cloned object */` |
|       44 |  5419 | `		pTos->x.pOther = pClone;` |
|       44 |  5420 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5421 | `	}` |
|       44 |  5422 | `	break;` |
|        - |  5423 | `				   }` |
|        - |  5424 | `/*` |
|        - |  5425 | ` * OP_SWITCH * * P3` |
|        - |  5426 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5427 | ` */` |
|       18 |  5428 | `case PH7_OP_SWITCH: {` |
|       38 |  5429 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5430 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5431 | `	ph7_value sValue,sCaseValue;` |
|        - |  5432 | `	sxu32 n,nEntry;` |
|        - |  5433 | `#ifdef UNTRUST` |
|        - |  5434 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5435 | `		goto Abort;` |
|        - |  5436 | `	}` |
|        - |  5437 | `#endif` |
|        - |  5438 | `	/* Point to the case table  */` |
|       38 |  5439 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5440 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5441 | `	/* Select the appropriate case block to execute */` |
|       38 |  5442 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5443 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5444 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5445 | `		pCase = &aCase[n];` |
|       92 |  5446 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5447 | `		/* Execute the case expression first */` |
|       92 |  5448 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5449 | `		/* Compare the two expression */` |
|       92 |  5450 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5451 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5452 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5453 | `		if( rc == 0 ){` |
|        - |  5454 | `			/* Value match,jump to this block */` |
|       38 |  5455 | `			pc = pCase->nStart - 1;` |
|       38 |  5456 | `			break;` |
|        - |  5457 | `		}` |
|       29 |  5458 | `	}` |
|       38 |  5459 | `	VmPopOperand(&pTos,1);` |
|       38 |  5460 | `	if( n >= nEntry ){` |
|        - |  5461 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5462 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5463 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5464 | `		}else{` |
|        - |  5465 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5466 | `			pc = pSwitch->nOut - 1;` |
|        - |  5467 | `		}` |
|      ! 0 |  5468 | `	}` |
|       38 |  5469 | `	break;` |
|        - |  5470 | `					}` |
|        - |  5471 | `/*` |
|        - |  5472 | ` * OP_CALL P1 * *` |
|        - |  5473 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5474 | ` *  function on the stack.` |
|        - |  5475 | ` */` |
|   269297 |  5476 | `case PH7_OP_CALL: {` |
|   538640 |  5477 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5478 | `	SyHashEntry *pEntry;` |
|        - |  5479 | `	SyString sName;` |
|        - |  5480 | `	/* Extract function name */` |
|   538640 |  5481 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5482 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5483 | `			ph7_value sResult;` |
|      ! 0 |  5484 | `			SySetReset(&aArg);` |
|      ! 0 |  5485 | `			while( pArg < pTos ){` |
|      ! 0 |  5486 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5487 | `				pArg++;` |
|      ! 0 |  5488 | `			}` |
|      ! 0 |  5489 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5490 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5491 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5492 | `			SySetReset(&aArg);` |
|        - |  5493 | `			/* Pop given arguments */` |
|      ! 0 |  5494 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5495 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5496 | `			}` |
|        - |  5497 | `			/* Copy result */` |
|      ! 0 |  5498 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5499 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5500 | `		}else{` |
|        3 |  5501 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5502 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5503 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5504 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5505 | `			}else{` |
|        - |  5506 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5507 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5508 | `			}` |
|        - |  5509 | `			/* Pop given arguments */` |
|        3 |  5510 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5511 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5512 | `			}` |
|        - |  5513 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5514 | `			PH7_MemObjRelease(pTos);` |
|        - |  5515 | `		}` |
|   269118 |  5516 | `		break;` |
|        - |  5517 | `	}` |
|   538638 |  5518 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5519 | `	/* Check for a compiled function first */` |
|   538638 |  5520 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|   538638 |  5521 | `	if( pEntry ){` |
|        - |  5522 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5523 | `		ph7_class_instance *pThis;` |
|        - |  5524 | `		ph7_value *pFrameStack;` |
|        - |  5525 | `		ph7_vm_func *pVmFunc;` |
|        - |  5526 | `		ph7_class *pSelf;` |
|        - |  5527 | `		VmFrame *pFrame;` |
|        - |  5528 | `		ph7_value *pObj;` |
|        - |  5529 | `		VmSlot sArg;` |
|        - |  5530 | `		sxu32 n;` |
|        - |  5531 | `		/* initialize fields */` |
|    10666 |  5532 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    10666 |  5533 | `		pThis = 0;` |
|    10666 |  5534 | `		pSelf = 0;` |
|    10666 |  5535 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5536 | `			ph7_class_method *pMeth;` |
|        - |  5537 | `			/* Class method call */` |
|     1062 |  5538 | `			ph7_value *pTarget = &pTos[-1];` |
|     1062 |  5539 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5540 | `				/* Extract the 'this' pointer */` |
|     1062 |  5541 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5542 | `					/* Instance already loaded */` |
|     1032 |  5543 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1032 |  5544 | `					pThis->iRef++;` |
|     1032 |  5545 | `					pSelf = pThis->pClass;` |
|      515 |  5546 | `				}` |
|     1062 |  5547 | `				if( pSelf == 0 ){` |
|       31 |  5548 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5549 | `						/* "Late Static Binding" class name */` |
|       37 |  5550 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       12 |  5551 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       12 |  5552 | `					}` |
|       31 |  5553 | `					if( pSelf == 0 ){` |
|        7 |  5554 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 |  5555 | `					}` |
|       15 |  5556 | `				}` |
|     1062 |  5557 | `				if( pThis == 0  ){` |
|       31 |  5558 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       33 |  5559 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5560 | `						/* Safely ignore the exception frame */` |
|        3 |  5561 | `						pFrameLocal = pFrameLocal->pParent;` |
|        1 |  5562 | `					}` |
|       31 |  5563 | `					if( pFrameLocal->pParent ){` |
|        - |  5564 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5565 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5566 | `						if( pThis ){` |
|       13 |  5567 | `							pThis->iRef++;` |
|        6 |  5568 | `						}` |
|        9 |  5569 | `					}` |
|       15 |  5570 | `				}` |
|     1062 |  5571 | `				VmPopOperand(&pTos,1);` |
|     1062 |  5572 | `				PH7_MemObjRelease(pTos);` |
|        - |  5573 | `				/* Synchronize pointers */` |
|     1062 |  5574 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5575 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5576 | `				 * user have already computed the random generated unique class method name` |
|        - |  5577 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5578 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5579 | `				 */` |
|     1062 |  5580 | `				while( pArg < pStack ){` |
|      ! 0 |  5581 | `					pArg++;` |
|      ! 0 |  5582 | `				}` |
|     1062 |  5583 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5584 | `					/* Check if the call is allowed */` |
|     1062 |  5585 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1062 |  5586 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        5 |  5587 | `						if( !VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5588 | `							/* Pop given arguments */` |
|      ! 0 |  5589 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5590 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5591 | `							}` |
|        - |  5592 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5593 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5594 | `							break;` |
|        - |  5595 | `						}` |
|        2 |  5596 | `					}` |
|      530 |  5597 | `				}` |
|      530 |  5598 | `			}` |
|      530 |  5599 | `		}` |
|        - |  5600 | `		/* Check The recursion limit */` |
|    10666 |  5601 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5602 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5603 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5604 | `				&pVmFunc->sName);` |
|        - |  5605 | `			/* Pop given arguments */` |
|        3 |  5606 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5607 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5608 | `			}` |
|        - |  5609 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5610 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5611 | `			break;` |
|        - |  5612 | `		}` |
|    10664 |  5613 | `		if( pVmFunc->pNextName ){` |
|        - |  5614 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      123 |  5615 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       61 |  5616 | `		}` |
|        - |  5617 | `		/* Extract the formal argument set */` |
|    10664 |  5618 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5619 | `		/* Create a new VM frame  */` |
|    10664 |  5620 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    10664 |  5621 | `		if( rc != SXRET_OK ){` |
|        - |  5622 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5623 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5624 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5625 | `				&pVmFunc->sName);` |
|        - |  5626 | `			/* Pop given arguments */` |
|      ! 0 |  5627 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5628 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5629 | `			}` |
|        - |  5630 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5631 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5632 | `			break;` |
|        - |  5633 | `		}` |
|    10664 |  5634 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5635 | `			/* Install the '$this' variable */` |
|        - |  5636 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1042 |  5637 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1042 |  5638 | `			if( pObj ){` |
|        - |  5639 | `				/* Reflect the change */` |
|     1042 |  5640 | `				pObj->x.pOther = pThis;` |
|     1042 |  5641 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      520 |  5642 | `			}` |
|      520 |  5643 | `		}` |
|    10664 |  5644 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5645 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5646 | `			/* Install static variables */` |
|      ! 0 |  5647 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5648 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5649 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5650 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5651 | `					/* Initialize the static variables */` |
|      ! 0 |  5652 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5653 | `					if( pObj ){` |
|        - |  5654 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5655 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5656 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5657 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5658 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5659 | `						}` |
|      ! 0 |  5660 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5661 | `					}else{` |
|      ! 0 |  5662 | `						continue;` |
|        - |  5663 | `					}` |
|      ! 0 |  5664 | `				}` |
|        - |  5665 | `				/* Install in the current frame */` |
|      ! 0 |  5666 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5667 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5668 | `			}` |
|      ! 0 |  5669 | `		}` |
|        - |  5670 | `		/* Push arguments in the local frame */` |
|    10664 |  5671 | `		n = 0;` |
|    29994 |  5672 | `		while( pArg < pTos ){` |
|    19332 |  5673 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    19214 |  5674 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5675 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5676 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5677 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5678 | `						goto Abort;` |
|        - |  5679 | `					}` |
|      ! 0 |  5680 | `				}` |
|        - |  5681 | `				/* Make sure the given arguments are of the correct type */` |
|    19214 |  5682 | `				if( aFormalArg[n].nType > 0 ){` |
|     1066 |  5683 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5684 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5685 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5686 | `						ph7_class *pClass;` |
|        - |  5687 | `						/* Try to extract the desired class */` |
|      ! 0 |  5688 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5689 | `						if( pClass ){` |
|      ! 0 |  5690 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5691 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5692 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5693 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5694 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5695 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5696 | `								}` |
|      ! 0 |  5697 | `							}else{` |
|        - |  5698 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5699 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5700 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5701 | `								if( ! VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5702 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5703 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5704 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5705 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5706 | `								}` |
|        - |  5707 | `							}` |
|      ! 0 |  5708 | `						}` |
|     1066 |  5709 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5710 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5711 | `						/* Cast to the desired type */` |
|      ! 0 |  5712 | `						xCast(pArg);` |
|      ! 0 |  5713 | `					}` |
|      532 |  5714 | `				}` |
|    19214 |  5715 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5716 | `					/* Pass by reference */` |
|       48 |  5717 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5718 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5719 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5720 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5721 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5722 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5723 | `						}` |
|        - |  5724 | `						/* Switch to pass by value */` |
|      ! 0 |  5725 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5726 | `					}else{` |
|        - |  5727 | `						SyHashEntry *pRefEntry;` |
|        - |  5728 | `						/* Install the referenced variable in the private function frame */` |
|       48 |  5729 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       48 |  5730 | `						if( pRefEntry == 0 ){` |
|       71 |  5731 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       46 |  5732 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       48 |  5733 | `							sArg.nIdx = pArg->nIdx;` |
|       48 |  5734 | `							sArg.pUserData = 0;` |
|       48 |  5735 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  5736 | `						}` |
|       48 |  5737 | `						pObj = 0;` |
|        - |  5738 | `					}` |
|       25 |  5739 | `				}else{` |
|        - |  5740 | `					/* Pass by value,make a copy of the given argument */` |
|    19168 |  5741 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5742 | `				}` |
|     9608 |  5743 | `			}else{` |
|        - |  5744 | `				char zName[32];` |
|        - |  5745 | `				SyString sArgName;` |
|        - |  5746 | `				/* Set a dummy name */` |
|      120 |  5747 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      120 |  5748 | `				sArgName.zString = zName;` |
|        - |  5749 | `				/* Annonymous argument */` |
|      120 |  5750 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5751 | `			}` |
|    19332 |  5752 | `			if( pObj ){` |
|    19286 |  5753 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5754 | `				/* Insert argument index  */` |
|    19286 |  5755 | `				sArg.nIdx = pObj->nIdx;` |
|    19286 |  5756 | `				sArg.pUserData = 0;` |
|    19286 |  5757 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|     9642 |  5758 | `			}` |
|    19332 |  5759 | `			PH7_MemObjRelease(pArg);` |
|    19332 |  5760 | `			pArg++;` |
|    19332 |  5761 | `			++n;` |
|        2 |  5762 | `		}` |
|        - |  5763 | `		/* Set up closure environment */` |
|    10664 |  5764 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5765 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  5766 | `			ph7_value *pValue;` |
|        - |  5767 | `			sxu32 iEnv;` |
|        9 |  5768 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  5769 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  5770 | `				pEnv = &aEnv[iEnv];` |
|       17 |  5771 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  5772 | `					/* Do not install null value */` |
|        9 |  5773 | `					continue;` |
|        - |  5774 | `				}` |
|        9 |  5775 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  5776 | `				if( pValue == 0 ){` |
|      ! 0 |  5777 | `					continue;` |
|        - |  5778 | `				}` |
|        - |  5779 | `				/* Invalidate any prior representation */` |
|        9 |  5780 | `				PH7_MemObjRelease(pValue);` |
|        - |  5781 | `				/* Duplicate bound variable value */` |
|        9 |  5782 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  5783 | `			}` |
|        4 |  5784 | `		}` |
|        - |  5785 | `		/* Process default values */` |
|    12252 |  5786 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1590 |  5787 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1580 |  5788 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1580 |  5789 | `				if( pObj ){` |
|        - |  5790 | `					/* Evaluate the default value and extract it's result */` |
|     1580 |  5791 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1580 |  5792 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5793 | `						goto Abort;` |
|        - |  5794 | `					}` |
|        - |  5795 | `					/* Insert argument index */` |
|     1580 |  5796 | `					sArg.nIdx = pObj->nIdx;` |
|     1580 |  5797 | `					sArg.pUserData = 0;` |
|     1580 |  5798 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  5799 | `					/* Make sure the default argument is of the correct type */` |
|     1580 |  5800 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5801 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5802 | `						/* Cast to the desired type */` |
|      ! 0 |  5803 | `						xCast(pObj);` |
|      ! 0 |  5804 | `					}` |
|      789 |  5805 | `				}` |
|      789 |  5806 | `			}` |
|     1590 |  5807 | `			++n;` |
|        2 |  5808 | `		}` |
|        - |  5809 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  5810 | `		 * does not return anything.` |
|        - |  5811 | `		 */` |
|    10664 |  5812 | `		PH7_MemObjRelease(pTos);` |
|    10664 |  5813 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  5814 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    10664 |  5815 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    10664 |  5816 | `		if( pFrameStack == 0 ){` |
|        - |  5817 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5818 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5819 | `				&pVmFunc->sName);` |
|      ! 0 |  5820 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5821 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5822 | `			}` |
|      ! 0 |  5823 | `			break;` |
|        - |  5824 | `		}` |
|    10664 |  5825 | `		if( pSelf ){` |
|        - |  5826 | `			/* Push class name */` |
|     1060 |  5827 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      529 |  5828 | `		}` |
|        - |  5829 | `		/* Increment nesting level */` |
|    10664 |  5830 | `		pVm->nRecursionDepth++;` |
|        - |  5831 | `		/* Execute function body */` |
|    10664 |  5832 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  5833 | `		/* Decrement nesting level */` |
|    10664 |  5834 | `		pVm->nRecursionDepth--;` |
|    10664 |  5835 | `		if( pSelf ){` |
|        - |  5836 | `			/* Pop class name */` |
|     1060 |  5837 | `			(void)SySetPop(&pVm->aSelf);` |
|      529 |  5838 | `		}` |
|        - |  5839 | `		/* Cleanup the mess left behind */` |
|    10664 |  5840 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  5841 | `			/* Return by reference,reflect that */` |
|        9 |  5842 | `			if( n != SXU32_HIGH ){` |
|        9 |  5843 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  5844 | `				sxu32 i;` |
|        - |  5845 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  5846 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  5847 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  5848 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  5849 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5850 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5851 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  5852 | `								&pVmFunc->sName);` |
|      ! 0 |  5853 | `						}` |
|      ! 0 |  5854 | `						n = SXU32_HIGH;` |
|      ! 0 |  5855 | `						break;` |
|        - |  5856 | `					}` |
|        3 |  5857 | `				}` |
|        5 |  5858 | `			}else{` |
|      ! 0 |  5859 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5860 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5861 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  5862 | `						&pVmFunc->sName);` |
|      ! 0 |  5863 | `				}` |
|        - |  5864 | `			}` |
|        9 |  5865 | `			pTos->nIdx = n;` |
|        4 |  5866 | `		}` |
|        - |  5867 | `		/* Cleanup the mess left behind */` |
|    10664 |  5868 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  5869 | `			/* An exception was throw in this frame */` |
|        7 |  5870 | `			pFrame = pFrame->pParent;` |
|        7 |  5871 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  5872 | `				/* Pop the resutlt */` |
|        5 |  5873 | `				VmPopOperand(&pTos,1);` |
|        - |  5874 | `				/* Jump to this destination */` |
|        5 |  5875 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  5876 | `				rc = PH7_OK;` |
|        3 |  5877 | `			}else{` |
|        3 |  5878 | `				if( pFrame->pParent ){` |
|        3 |  5879 | `					rc = PH7_EXCEPTION;` |
|        2 |  5880 | `				}else{` |
|        - |  5881 | `					/* Continue normal execution */` |
|      ! 0 |  5882 | `					rc = PH7_OK;` |
|        - |  5883 | `				}` |
|        - |  5884 | `			}` |
|        3 |  5885 | `		}` |
|        - |  5886 | `		/* Free the operand stack */` |
|    10664 |  5887 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  5888 | `		/* Leave the frame */` |
|    10664 |  5889 | `		VmLeaveFrame(&(*pVm));` |
|    10664 |  5890 | `		if( rc == PH7_ABORT ){` |
|        - |  5891 | `			/* Abort processing immeditaley */` |
|        5 |  5892 | `			goto Abort;` |
|    10660 |  5893 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5894 | `			goto Exception;` |
|        - |  5895 | `		}` |
|     5330 |  5896 | `	}else{` |
|        - |  5897 | `		ph7_user_func *pFunc;` |
|        - |  5898 | `		ph7_context sCtx;` |
|        - |  5899 | `		ph7_value sRet;` |
|        - |  5900 | `		/* Look for an installed foreign function */` |
|   527974 |  5901 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   527974 |  5902 | `		if( pEntry == 0 ){` |
|        - |  5903 | `			/* Call to undefined function */` |
|        5 |  5904 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  5905 | `			/* Pop given arguments */` |
|        5 |  5906 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5907 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5908 | `			}` |
|        - |  5909 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  5910 | `			PH7_MemObjRelease(pTos);` |
|        5 |  5911 | `			break;` |
|        - |  5912 | `		}` |
|   527970 |  5913 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  5914 | `		/* Start collecting function arguments */` |
|   527970 |  5915 | `		SySetReset(&aArg);` |
|  1405090 |  5916 | `		while( pArg < pTos ){` |
|   877122 |  5917 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   877122 |  5918 | `			pArg++;` |
|        2 |  5919 | `		}` |
|        - |  5920 | `		/* Assume a null return value */` |
|   527970 |  5921 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  5922 | `		/* Init the call context */` |
|   527970 |  5923 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  5924 | `		/* Call the foreign function */` |
|   527970 |  5925 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5926 | `		/* Release the call context */` |
|   527970 |  5927 | `		VmReleaseCallContext(&sCtx);` |
|   527970 |  5928 | `		if( rc == PH7_ABORT ){` |
|      355 |  5929 | `			goto Abort;` |
|   527616 |  5930 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5931 | `			goto Exception;` |
|        - |  5932 | `		}` |
|   527614 |  5933 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5934 | `			/* Pop function name and arguments */` |
|   510622 |  5935 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   255332 |  5936 | `		}` |
|        - |  5937 | `		/* Save foreign function return value */` |
|   527614 |  5938 | `		PH7_MemObjStore(&sRet,pTos);` |
|   527614 |  5939 | `		PH7_MemObjRelease(&sRet);` |
|        - |  5940 | `	}` |
|   538270 |  5941 | `	break;` |
|        - |  5942 | `				  }` |
|        - |  5943 | `/*` |
|        - |  5944 | ` * OP_CONSUME: P1 * *` |
|        - |  5945 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  5946 | ` */` |
|     9821 |  5947 | `case PH7_OP_CONSUME: {` |
|    19644 |  5948 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    19644 |  5949 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  5950 |  |
|    19644 |  5951 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    19644 |  5952 | `	pCur = pOut;` |
|        - |  5953 | `	/* Start the consume process  */` |
|    39286 |  5954 | `	while( pOut <= pTos ){` |
|        - |  5955 | `		/* Force a string cast */` |
|    19644 |  5956 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      158 |  5957 | `			PH7_MemObjToString(pOut);` |
|       78 |  5958 | `		}` |
|    19644 |  5959 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  5960 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  5961 | `			/* Invoke the output consumer callback */` |
|    10498 |  5962 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    10498 |  5963 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  5964 | `				/* Increment output length */` |
|     4148 |  5965 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     2073 |  5966 | `			}` |
|    10498 |  5967 | `			SyBlobRelease(&pOut->sBlob);` |
|    10498 |  5968 | `			if( rc == SXERR_ABORT ){` |
|        - |  5969 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  5970 | `				goto Abort;` |
|        - |  5971 | `			}` |
|     5248 |  5972 | `		}` |
|    19644 |  5973 | `		pOut++;` |
|        2 |  5974 | `	}` |
|    19644 |  5975 | `	pTos = &pCur[-1];` |
|    19642 |  5976 | `	break;` |
|        - |  5977 | `					 }` |
|        - |  5978 |  |
|        - |  5979 | `		} /* Switch() */` |
|  9392686 |  5980 | `		pc++; /* Next instruction in the stream */` |
|        2 |  5981 | `	} /* For(;;) */` |
|    13291 |  5982 | `Done:` |
|    26584 |  5983 | `	SySetRelease(&aArg);` |
|    26584 |  5984 | `	return SXRET_OK;` |
|      182 |  5985 | `Abort:` |
|      365 |  5986 | `	SySetRelease(&aArg);` |
|     1271 |  5987 | `	while( pTos >= pStack ){` |
|      907 |  5988 | `		PH7_MemObjRelease(pTos);` |
|      907 |  5989 | `		pTos--;` |
|        1 |  5990 | `	}` |
|      365 |  5991 | `	return PH7_ABORT;` |
|        2 |  5992 | `Exception:` |
|        5 |  5993 | `	SySetRelease(&aArg);` |
|        9 |  5994 | `	while( pTos >= pStack ){` |
|        5 |  5995 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5996 | `		pTos--;` |
|        1 |  5997 | `	}` |
|        5 |  5998 | `	return PH7_EXCEPTION;` |
|    13477 |  5999 |  |
|        - |  6000 | `/*` |
|        - |  6001 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6002 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6003 | ` * See block-comment on that function for additional information.` |
|        - |  6004 | ` */` |
|    13274 |  6005 | `static sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6006 |  |
|        - |  6007 | `	ph7_value *pStack;` |
|        - |  6008 | `	sxi32 rc;` |
|        - |  6009 | `	/* Allocate a new operand stack */` |
|    13276 |  6010 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    13276 |  6011 | `	if( pStack == 0 ){` |
|      ! 0 |  6012 | `		return SXERR_MEM;` |
|        - |  6013 | `	}` |
|        - |  6014 | `	/* Execute the program */` |
|    13276 |  6015 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  6016 | `	/* Free the operand stack */` |
|    13276 |  6017 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6018 | `	/* Execution result */` |
|    13276 |  6019 | `	return rc;` |
|     6639 |  6020 |  |
|        - |  6021 | `/*` |
|        - |  6022 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6023 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6024 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6025 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6026 | ` * execution ends.` |
|        - |  6027 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6028 | ` * additional information.` |
|        - |  6029 | ` */` |
|     1664 |  6030 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6031 |  |
|        - |  6032 | `	VmShutdownCB *pEntry;` |
|        - |  6033 | `	ph7_value *apArg[10];` |
|        - |  6034 | `	sxu32 n,nEntry;` |
|        - |  6035 | `	int i;` |
|        - |  6036 | `	/* Point to the stack of registered callbacks */` |
|     1666 |  6037 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    18306 |  6038 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    16642 |  6039 | `		apArg[i] = 0;` |
|     8322 |  6040 | `	}` |
|     1668 |  6041 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6042 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6043 | `		if( pEntry ){` |
|        - |  6044 | `			/* Prepare callback arguments if any */` |
|        3 |  6045 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6046 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6047 | `					break;` |
|        - |  6048 | `				}` |
|      ! 0 |  6049 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6050 | `			}` |
|        - |  6051 | `			/* Invoke the callback */` |
|        3 |  6052 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6053 | `			/*` |
|        - |  6054 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6055 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6056 | `			 */` |
|        3 |  6057 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6058 | `			if( pEntry ){` |
|        3 |  6059 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6060 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6061 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6062 | `				}` |
|        1 |  6063 | `			}` |
|        1 |  6064 | `		}` |
|        2 |  6065 | `	}` |
|     1666 |  6066 | `	SySetReset(&pVm->aShutdown);` |
|     1666 |  6067 |  |
|        - |  6068 | `/*` |
|        - |  6069 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6070 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6071 | ` * See block-comment on that function for additional information.` |
|        - |  6072 | ` */` |
|     1672 |  6073 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6074 |  |
|        - |  6075 | `	/* Make sure we are ready to execute this program */` |
|     1674 |  6076 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6077 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6078 | `	}` |
|        - |  6079 | `	/* Set the execution magic number  */` |
|     1674 |  6080 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6081 | `	/* Execute the program */` |
|     1674 |  6082 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6083 | `	/* Invoke any shutdown callbacks */` |
|     1670 |  6084 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6085 | `	/*` |
|        - |  6086 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6087 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6088 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6089 | `	 */` |
|     1670 |  6090 | `	return SXRET_OK;` |
|      838 |  6091 |  |
|        - |  6092 | `/*` |
|        - |  6093 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6094 | ` * the desired message.` |
|        - |  6095 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6096 | ` * in 'api.c' for additional information.` |
|        - |  6097 | ` */` |
|      352 |  6098 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6099 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6100 | `	SyString *pString /* Message to output */` |
|        - |  6101 | `	)` |
|        2 |  6102 |  |
|      354 |  6103 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      354 |  6104 | `	sxi32 rc = SXRET_OK;` |
|        - |  6105 | `	/* Call the output consumer */` |
|      354 |  6106 | `	if( pString->nByte > 0 ){` |
|      354 |  6107 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      354 |  6108 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6109 | `			/* Increment output length */` |
|       17 |  6110 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6111 | `		}` |
|      176 |  6112 | `	}` |
|      354 |  6113 | `	return rc;` |
|        2 |  6114 |  |
|        - |  6115 | `/*` |
|        - |  6116 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6117 | ` * callback to consume the formatted message.` |
|        - |  6118 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6119 | ` * in 'api.c' for additional information.` |
|        - |  6120 | ` */` |
|        2 |  6121 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6122 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6123 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6124 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6125 | `	)` |
|        1 |  6126 |  |
|        3 |  6127 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6128 | `	sxi32 rc = SXRET_OK;` |
|        - |  6129 | `	SyBlob sWorker;` |
|        - |  6130 | `	/* Format the message and call the output consumer */` |
|        3 |  6131 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6132 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6133 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6134 | `		/* Consume the formatted message */` |
|        3 |  6135 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6136 | `	}` |
|        3 |  6137 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6138 | `		/* Increment output length */` |
|      ! 0 |  6139 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6140 | `	}` |
|        - |  6141 | `	/* Release the working buffer */` |
|        3 |  6142 | `	SyBlobRelease(&sWorker);` |
|        3 |  6143 | `	return rc;` |
|        1 |  6144 |  |
|        - |  6145 | `/*` |
|        - |  6146 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6147 | ` * This function never fail and always return a pointer` |
|        - |  6148 | ` * to a null terminated string.` |
|        - |  6149 | ` */` |
|       10 |  6150 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6151 |  |
|       11 |  6152 | `	const char *zOp = "Unknown     ";` |
|       11 |  6153 | `	switch(nOp){` |
|        3 |  6154 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6155 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6156 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6157 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6158 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6159 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6160 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6161 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6162 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6163 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6164 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6165 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6166 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6167 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6168 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6169 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6170 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6171 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6172 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6173 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6174 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6175 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6176 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6177 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6178 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6179 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6180 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6181 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6182 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6183 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6184 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6185 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6186 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6187 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6188 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6189 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6190 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6191 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6192 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6193 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6194 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6195 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6196 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6197 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6198 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6199 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6200 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6201 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6202 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6203 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6204 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6205 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6206 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6207 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6208 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6209 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6210 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6211 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6212 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6213 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6214 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6215 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6216 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6217 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6218 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6219 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6220 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6221 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6222 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6223 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6224 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6225 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6226 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6227 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6228 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6229 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6230 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6231 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6232 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6233 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6234 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6235 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6236 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6237 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6238 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6239 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6240 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6241 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6242 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6243 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6244 | `	default:` |
|      ! 0 |  6245 | `		break;` |
|        - |  6246 | `	}` |
|       11 |  6247 | `	return zOp;` |
|        1 |  6248 |  |
|        - |  6249 | `/*` |
|        - |  6250 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6251 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6252 | ` * is responsible of consuming the generated dump.` |
|        - |  6253 | ` */` |
|        2 |  6254 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6255 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6256 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6257 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6258 | `	)` |
|        1 |  6259 |  |
|        - |  6260 | `	sxi32 rc;` |
|        3 |  6261 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6262 | `	return rc;` |
|        1 |  6263 |  |
|        - |  6264 | `/*` |
|        - |  6265 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6266 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6267 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6268 | ` * in 'compile.c' for additional information.` |
|        - |  6269 | ` */` |
|        8 |  6270 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6271 |  |
|        9 |  6272 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6273 | `	/* Evaluate and expand constant value */` |
|        9 |  6274 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6275 |  |
|        - |  6276 | `/*` |
|        - |  6277 | ` * Section:` |
|        - |  6278 | ` *  Function handling functions.` |
|        - |  6279 | ` * Status:` |
|        - |  6280 | ` *    Stable.` |
|        - |  6281 | ` */` |
|        - |  6282 | `/*` |
|        - |  6283 | ` * int func_num_args(void)` |
|        - |  6284 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6285 | ` * Parameters` |
|        - |  6286 | ` *   None.` |
|        - |  6287 | ` * Return` |
|        - |  6288 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6289 | ` *  or -1 if called from the globe scope.` |
|        - |  6290 | ` */` |
|      868 |  6291 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6292 |  |
|        - |  6293 | `	VmFrame *pFrame;` |
|        - |  6294 | `	ph7_vm *pVm;` |
|        - |  6295 | `	/* Point to the target VM */` |
|      870 |  6296 | `	pVm = pCtx->pVm;` |
|        - |  6297 | `	/* Current frame */` |
|      870 |  6298 | `	pFrame = pVm->pFrame;` |
|      870 |  6299 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6300 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6301 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6302 | `	}` |
|      870 |  6303 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6304 | `		SXUNUSED(nArg);` |
|      ! 0 |  6305 | `		SXUNUSED(apArg);` |
|        - |  6306 | `		/* Global frame,return -1 */` |
|      ! 0 |  6307 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6308 | `		return SXRET_OK;` |
|        - |  6309 | `	}` |
|        - |  6310 | `	/* Total number of arguments passed to the enclosing function */` |
|      870 |  6311 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      870 |  6312 | `	ph7_result_int(pCtx,nArg);` |
|      870 |  6313 | `	return SXRET_OK;` |
|      436 |  6314 |  |
|        - |  6315 | `/*` |
|        - |  6316 | ` * value func_get_arg(int $arg_num)` |
|        - |  6317 | ` *   Return an item from the argument list.` |
|        - |  6318 | ` * Parameters` |
|        - |  6319 | ` *  Argument number(index start from zero).` |
|        - |  6320 | ` * Return` |
|        - |  6321 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6322 | ` */` |
|       22 |  6323 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6324 |  |
|       24 |  6325 | `	ph7_value *pObj = 0;` |
|       24 |  6326 | `	VmSlot *pSlot = 0;` |
|        - |  6327 | `	VmFrame *pFrame;` |
|        - |  6328 | `	ph7_vm *pVm;` |
|        - |  6329 | `	/* Point to the target VM */` |
|       24 |  6330 | `	pVm = pCtx->pVm;` |
|        - |  6331 | `	/* Current frame */` |
|       24 |  6332 | `	pFrame = pVm->pFrame;` |
|       24 |  6333 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6334 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6335 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6336 | `	}` |
|       24 |  6337 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6338 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6339 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6340 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6341 | `		return SXRET_OK;` |
|        - |  6342 | `	}` |
|        - |  6343 | `	/* Extract the desired index */` |
|       21 |  6344 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  6345 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6346 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6347 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6348 | `		return SXRET_OK;` |
|        - |  6349 | `	}` |
|        - |  6350 | `	/* Extract the desired argument */` |
|       21 |  6351 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  6352 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6353 | `			/* Return the desired argument */` |
|       21 |  6354 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  6355 | `		}else{` |
|        - |  6356 | `			/* No such argument,return false */` |
|      ! 0 |  6357 | `			ph7_result_bool(pCtx,0);` |
|        - |  6358 | `		}` |
|       11 |  6359 | `	}else{` |
|        - |  6360 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6361 | `		ph7_result_bool(pCtx,0);` |
|        - |  6362 | `	}` |
|       21 |  6363 | `	return SXRET_OK;` |
|       13 |  6364 |  |
|        - |  6365 | `/*` |
|        - |  6366 | ` * array func_get_args_byref(void)` |
|        - |  6367 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6368 | ` * Parameters` |
|        - |  6369 | ` *  None.` |
|        - |  6370 | ` * Return` |
|        - |  6371 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6372 | ` *  member of the current user-defined function's argument list.` |
|        - |  6373 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6374 | ` * NOTE:` |
|        - |  6375 | ` *  Arguments are returned to the array by reference.` |
|        - |  6376 | ` */` |
|        2 |  6377 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6378 |  |
|        - |  6379 | `	ph7_value *pArray;` |
|        - |  6380 | `	VmFrame *pFrame;` |
|        - |  6381 | `	VmSlot *aSlot;` |
|        - |  6382 | `	sxu32 n;` |
|        - |  6383 | `	/* Point to the current frame */` |
|        3 |  6384 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6385 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6386 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6387 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6388 | `	}` |
|        3 |  6389 | `	if( pFrame->pParent == 0 ){` |
|        - |  6390 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6391 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6392 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6393 | `		return SXRET_OK;` |
|        - |  6394 | `	}` |
|        - |  6395 | `	/* Create a new array */` |
|        3 |  6396 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6397 | `	if( pArray == 0 ){` |
|      ! 0 |  6398 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6399 | `		SXUNUSED(apArg);` |
|      ! 0 |  6400 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6401 | `		return SXRET_OK;` |
|        - |  6402 | `	}` |
|        - |  6403 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6404 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6405 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6406 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6407 | `	}` |
|        - |  6408 | `	/* Return the freshly created array */` |
|        3 |  6409 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6410 | `	return SXRET_OK;` |
|        2 |  6411 |  |
|        - |  6412 | `/*` |
|        - |  6413 | ` * array func_get_args(void)` |
|        - |  6414 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6415 | ` * Parameters` |
|        - |  6416 | ` *  None.` |
|        - |  6417 | ` * Return` |
|        - |  6418 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6419 | ` *  member of the current user-defined function's argument list.` |
|        - |  6420 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6421 | ` */` |
|       46 |  6422 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6423 |  |
|       47 |  6424 | `	ph7_value *pObj = 0;` |
|        - |  6425 | `	ph7_value *pArray;` |
|        - |  6426 | `	VmFrame *pFrame;` |
|        - |  6427 | `	VmSlot *aSlot;` |
|        - |  6428 | `	sxu32 n;` |
|        - |  6429 | `	/* Point to the current frame */` |
|       47 |  6430 | `	pFrame = pCtx->pVm->pFrame;` |
|       47 |  6431 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6432 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6433 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6434 | `	}` |
|       47 |  6435 | `	if( pFrame->pParent == 0 ){` |
|        - |  6436 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6437 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6438 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6439 | `		return SXRET_OK;` |
|        - |  6440 | `	}` |
|        - |  6441 | `	/* Create a new array */` |
|       47 |  6442 | `	pArray = ph7_context_new_array(pCtx);` |
|       47 |  6443 | `	if( pArray == 0 ){` |
|      ! 0 |  6444 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6445 | `		SXUNUSED(apArg);` |
|      ! 0 |  6446 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6447 | `		return SXRET_OK;` |
|        - |  6448 | `	}` |
|        - |  6449 | `	/* Start filling the array with the given arguments */` |
|       47 |  6450 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      143 |  6451 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|       97 |  6452 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|       97 |  6453 | `		if( pObj ){` |
|       97 |  6454 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       48 |  6455 | `		}` |
|       49 |  6456 | `	}` |
|        - |  6457 | `	/* Return the freshly created array */` |
|       47 |  6458 | `	ph7_result_value(pCtx,pArray);` |
|       47 |  6459 | `	return SXRET_OK;` |
|       24 |  6460 |  |
|        - |  6461 | `/*` |
|        - |  6462 | ` * bool function_exists(string $name)` |
|        - |  6463 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6464 | ` * Parameters` |
|        - |  6465 | ` *  The name of the desired function.` |
|        - |  6466 | ` * Return` |
|        - |  6467 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6468 | ` */` |
|     1666 |  6469 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6470 |  |
|        - |  6471 | `	const char *zName;` |
|        - |  6472 | `	ph7_vm *pVm;` |
|        - |  6473 | `	int nLen;` |
|        - |  6474 | `	int res;` |
|     1668 |  6475 | `	if( nArg < 1 ){` |
|        - |  6476 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6477 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6478 | `		return SXRET_OK;` |
|        - |  6479 | `	}` |
|        - |  6480 | `	/* Point to the target VM */` |
|     1668 |  6481 | `	pVm = pCtx->pVm;` |
|        - |  6482 | `	/* Extract the function name */` |
|     1668 |  6483 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6484 | `	/* Assume the function is not defined */` |
|     1668 |  6485 | `	res = 0;` |
|        - |  6486 | `	/* Perform the lookup */` |
|     2499 |  6487 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1662 |  6488 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6489 | `			/* Function is defined */` |
|      212 |  6490 | `			res = 1;` |
|      105 |  6491 | `	}` |
|     1668 |  6492 | `	ph7_result_bool(pCtx,res);` |
|     1668 |  6493 | `	return SXRET_OK;` |
|      835 |  6494 |  |
|        - |  6495 | `/* Forward declaration */` |
|        - |  6496 | `static ph7_class * VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg);` |
|        - |  6497 | `/*` |
|        - |  6498 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6499 | ` * [i.e: Whether it is callable or not].` |
|        - |  6500 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6501 | ` */` |
|    15836 |  6502 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6503 |  |
|    15838 |  6504 | `	int res = 0;` |
|    15838 |  6505 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6506 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6507 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6508 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6509 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6510 | `		if( pMethod && CallInvoke ){` |
|        - |  6511 | `			ph7_value sResult;` |
|        - |  6512 | `			sxi32 rc;` |
|        - |  6513 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6514 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6515 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6516 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6517 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6518 | `			}` |
|      ! 0 |  6519 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6520 | `		}` |
|    15838 |  6521 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  6522 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       20 |  6523 | `		if( pMap->nEntry == 2 ){` |
|        - |  6524 | `			ph7_class *pClass;` |
|        - |  6525 | `			ph7_value *pV;` |
|        - |  6526 | `			/* Extract the target class */` |
|        7 |  6527 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|        7 |  6528 | `			if( pV ){` |
|        7 |  6529 | `				pClass = VmExtractClassFromValue(pVm,pV);` |
|        7 |  6530 | `				if( pClass ){` |
|        - |  6531 | `					ph7_class_method *pMethod;` |
|        - |  6532 | `					/* Extract the target method */` |
|        7 |  6533 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|        7 |  6534 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6535 | `						/* Perform the lookup */` |
|        7 |  6536 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|        7 |  6537 | `						if( pMethod ){` |
|        - |  6538 | `							/* Method is callable */` |
|        5 |  6539 | `							res = 1;` |
|        2 |  6540 | `						}` |
|        3 |  6541 | `					}` |
|        3 |  6542 | `				}` |
|        3 |  6543 | `			}` |
|        5 |  6544 | `		}` |
|    15829 |  6545 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6546 | `		const char *zName;` |
|        - |  6547 | `		int nLen;` |
|        - |  6548 | `		/* Extract the name */` |
|     4658 |  6549 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6550 | `		/* Perform the lookup */` |
|     4671 |  6551 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       26 |  6552 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6553 | `				/* Function is callable */` |
|     4644 |  6554 | `				res = 1;` |
|     2321 |  6555 | `		}` |
|     2328 |  6556 | `	}` |
|    15838 |  6557 | `	return res;` |
|        2 |  6558 |  |
|        - |  6559 | `/*` |
|        - |  6560 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6561 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6562 | ` * Parameters` |
|        - |  6563 | ` * $name` |
|        - |  6564 | ` *    The callback function to check` |
|        - |  6565 | ` * $syntax_only` |
|        - |  6566 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6567 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6568 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6569 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6570 | ` *    a string.` |
|        - |  6571 | ` * Return` |
|        - |  6572 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6573 | ` */` |
|       14 |  6574 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6575 |  |
|        - |  6576 | `	ph7_vm *pVm;` |
|        - |  6577 | `	int res;` |
|       15 |  6578 | `	if( nArg < 1 ){` |
|        - |  6579 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6580 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6581 | `		return SXRET_OK;` |
|        - |  6582 | `	}` |
|        - |  6583 | `	/* Point to the target VM */` |
|       15 |  6584 | `	pVm = pCtx->pVm;` |
|        - |  6585 | `	/* Perform the requested operation */` |
|       15 |  6586 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6587 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6588 | `	return SXRET_OK;` |
|        8 |  6589 |  |
|        - |  6590 | `/*` |
|        - |  6591 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6592 | ` * defined below.` |
|        - |  6593 | ` */` |
|     1074 |  6594 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6595 |  |
|     1075 |  6596 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6597 | `	ph7_value sName;` |
|        - |  6598 | `	sxi32 rc;` |
|        - |  6599 | `	/* Prepare the function name for insertion */` |
|     1075 |  6600 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1075 |  6601 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6602 | `	/* Perform the insertion */` |
|     1075 |  6603 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1075 |  6604 | `	PH7_MemObjRelease(&sName);` |
|     1075 |  6605 | `	return rc;` |
|        1 |  6606 |  |
|        - |  6607 | `/*` |
|        - |  6608 | ` * array get_defined_functions(void)` |
|        - |  6609 | ` *  Returns an array of all defined functions.` |
|        - |  6610 | ` * Parameter` |
|        - |  6611 | ` *  None.` |
|        - |  6612 | ` * Return` |
|        - |  6613 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6614 | ` *  both built-in (internal) and user-defined.` |
|        - |  6615 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6616 | ` *  defined ones using $arr["user"].` |
|        - |  6617 | ` * Note:` |
|        - |  6618 | ` *  NULL is returned on failure.` |
|        - |  6619 | ` */` |
|        2 |  6620 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6621 |  |
|        - |  6622 | `	ph7_value *pArray,*pEntry;` |
|        - |  6623 | `	/* NOTE:` |
|        - |  6624 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6625 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6626 | `	 */` |
|        3 |  6627 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6628 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6629 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6630 | `		SXUNUSED(apArg);` |
|        - |  6631 | `		/* Return NULL */` |
|      ! 0 |  6632 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6633 | `		return SXRET_OK;` |
|        - |  6634 | `	}` |
|        3 |  6635 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6636 | `	if( pEntry == 0 ){` |
|        - |  6637 | `		/* Return NULL */` |
|      ! 0 |  6638 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6639 | `		return SXRET_OK;` |
|        - |  6640 | `	}` |
|        - |  6641 | `	/* Fill with the appropriate information */` |
|        3 |  6642 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6643 | `	/* Create the 'internal' index */` |
|        3 |  6644 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6645 | `	/* Create the user-func array */` |
|        3 |  6646 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6647 | `	if( pEntry == 0 ){` |
|        - |  6648 | `		/* Return NULL */` |
|      ! 0 |  6649 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6650 | `		return SXRET_OK;` |
|        - |  6651 | `	}` |
|        - |  6652 | `	/* Fill with the appropriate information */` |
|        3 |  6653 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6654 | `	/* Create the 'user' index */` |
|        3 |  6655 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6656 | `	/* Return the multi-dimensional array */` |
|        3 |  6657 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6658 | `	return SXRET_OK;` |
|        2 |  6659 |  |
|        - |  6660 | `/*` |
|        - |  6661 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6662 | ` *  Register a function for execution on shutdown.` |
|        - |  6663 | ` * Note` |
|        - |  6664 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6665 | ` *  be called in the same order as they were registered.` |
|        - |  6666 | ` * Parameters` |
|        - |  6667 | ` *  $callback` |
|        - |  6668 | ` *   The shutdown callback to register.` |
|        - |  6669 | ` * $param` |
|        - |  6670 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6671 | ` * Return` |
|        - |  6672 | ` *  Nothing.` |
|        - |  6673 | ` */` |
|        2 |  6674 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6675 |  |
|        - |  6676 | `	VmShutdownCB sEntry;` |
|        - |  6677 | `	int i,j;` |
|        3 |  6678 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6679 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6680 | `		return PH7_OK;` |
|        - |  6681 | `	}` |
|        - |  6682 | `	/* Zero the Entry */` |
|        3 |  6683 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6684 | `	/* Initialize fields */` |
|        3 |  6685 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6686 | `	/* Save the callback name for later invocation name */` |
|        3 |  6687 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6688 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6689 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6690 | `	}` |
|        - |  6691 | `	/* Copy arguments */` |
|        3 |  6692 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6693 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6694 | `			/* Limit reached */` |
|      ! 0 |  6695 | `			break;` |
|        - |  6696 | `		}` |
|      ! 0 |  6697 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6698 | `	}` |
|        3 |  6699 | `	sEntry.nArg = j;` |
|        - |  6700 | `	/* Install the callback */` |
|        3 |  6701 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6702 | `	return PH7_OK;` |
|        2 |  6703 |  |
|        - |  6704 | `/*` |
|        - |  6705 | ` * Section:` |
|        - |  6706 | ` *  Class handling functions.` |
|        - |  6707 | ` * Status:` |
|        - |  6708 | ` *    Stable.` |
|        - |  6709 | ` */` |
|        - |  6710 | `/*` |
|        - |  6711 | ` * Extract the top active class. NULL is returned` |
|        - |  6712 | ` * if the class stack is empty.` |
|        - |  6713 | ` */` |
|      400 |  6714 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6715 |  |
|      402 |  6716 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6717 | `	ph7_class **apClass;` |
|      402 |  6718 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6719 | `		/* Empty stack,return NULL */` |
|       15 |  6720 | `		return 0;` |
|        - |  6721 | `	}` |
|        - |  6722 | `	/* Peek the last entry */` |
|      388 |  6723 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      388 |  6724 | `	return apClass[pSet->nUsed - 1];` |
|      202 |  6725 |  |
|        - |  6726 | `/*` |
|        - |  6727 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  6728 | ` *   Get the class that declared the currently executing method.` |
|        - |  6729 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  6730 | ` *` |
|        - |  6731 | ` * Parameters` |
|        - |  6732 | ` *   pVm: Target VM` |
|        - |  6733 | ` *` |
|        - |  6734 | ` * Return` |
|        - |  6735 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  6736 | ` *   - Not executing within a class method` |
|        - |  6737 | ` *` |
|        - |  6738 | ` * Note` |
|        - |  6739 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  6740 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  6741 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  6742 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  6743 | ` *   declaring class.` |
|        - |  6744 | ` */` |
|       18 |  6745 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        1 |  6746 |  |
|       19 |  6747 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  6748 | `	ph7_vm_func *pVmFunc;` |
|        - |  6749 |  |
|        - |  6750 | `	/* Skip exception frames to find the actual method frame */` |
|       19 |  6751 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  6752 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6753 | `	}` |
|        - |  6754 |  |
|        - |  6755 | `	/* Check if we're in a method context */` |
|       19 |  6756 | `	if( pFrame->pParent ){` |
|       15 |  6757 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       15 |  6758 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  6759 | `			/* Return the declaring class */` |
|       15 |  6760 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  6761 | `		}` |
|      ! 0 |  6762 | `	}` |
|        - |  6763 |  |
|        5 |  6764 | `	return 0;` |
|       10 |  6765 |  |
|        - |  6766 |  |
|        - |  6767 | `/*` |
|        - |  6768 | ` * string get_class ([ object $object = NULL ] )` |
|        - |  6769 | ` *   Returns the name of the class of an object` |
|        - |  6770 | ` * Parameters` |
|        - |  6771 | ` *  object` |
|        - |  6772 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|        - |  6773 | ` * Return` |
|        - |  6774 | ` *  The name of the class of which object is an instance.` |
|        - |  6775 | ` *  Returns FALSE if object is not an object.` |
|        - |  6776 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|        - |  6777 | ` */` |
|       18 |  6778 | `static int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6779 |  |
|        - |  6780 | `	ph7_class *pClass;` |
|        - |  6781 | `	SyString *pName;` |
|       20 |  6782 | `	if( nArg < 1 ){` |
|        - |  6783 | `		/* Check if we are inside a class */` |
|      ! 0 |  6784 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|      ! 0 |  6785 | `		if( pClass ){` |
|        - |  6786 | `			/* Point to the class name */` |
|      ! 0 |  6787 | `			pName = &pClass->sName;` |
|      ! 0 |  6788 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|      ! 0 |  6789 | `		}else{` |
|        - |  6790 | `			/* Not inside class,return FALSE */` |
|      ! 0 |  6791 | `			ph7_result_bool(pCtx,0);` |
|        - |  6792 | `		}` |
|      ! 0 |  6793 | `	}else{` |
|        - |  6794 | `		/* Extract the target class */` |
|       20 |  6795 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       20 |  6796 | `		if( pClass ){` |
|       18 |  6797 | `			pName = &pClass->sName;` |
|        - |  6798 | `			/* Return the class name */` |
|       18 |  6799 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|       10 |  6800 | `		}else{` |
|        - |  6801 | `			/* Not a class instance,return FALSE */` |
|        3 |  6802 | `			ph7_result_bool(pCtx,0);` |
|        - |  6803 | `		}` |
|        - |  6804 | `	}` |
|       20 |  6805 | `	return PH7_OK;` |
|        2 |  6806 |  |
|        - |  6807 | `/*` |
|        - |  6808 | ` * string get_parent_class([object $object = NULL ] )` |
|        - |  6809 | ` *   Returns the name of the parent class of an object` |
|        - |  6810 | ` * Parameters` |
|        - |  6811 | ` *  object` |
|        - |  6812 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|        - |  6813 | ` * Return` |
|        - |  6814 | ` *  The name of the parent class of which object is an instance.` |
|        - |  6815 | ` *  Returns FALSE if object is not an object or if the object does` |
|        - |  6816 | ` *  not have a parent.` |
|        - |  6817 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|        - |  6818 | ` */` |
|        8 |  6819 | `static int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6820 |  |
|        - |  6821 | `	ph7_class *pClass;` |
|        - |  6822 | `	SyString *pName;` |
|        9 |  6823 | `	if( nArg < 1 ){` |
|        - |  6824 | `		/* Check if we are inside a class [i.e: a method call]*/` |
|        3 |  6825 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|        3 |  6826 | `		if( pClass && pClass->pBase ){` |
|        - |  6827 | `			/* Point to the class name */` |
|        3 |  6828 | `			pName = &pClass->pBase->sName;` |
|        3 |  6829 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        2 |  6830 | `		}else{` |
|        - |  6831 | `			/* Not inside class,return FALSE */` |
|      ! 0 |  6832 | `			ph7_result_bool(pCtx,0);` |
|        - |  6833 | `		}` |
|        2 |  6834 | `	}else{` |
|        - |  6835 | `		/* Extract the target class */` |
|        7 |  6836 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        7 |  6837 | `		if( pClass ){` |
|        7 |  6838 | `			if( pClass->pBase ){` |
|        5 |  6839 | `				pName = &pClass->pBase->sName;` |
|        - |  6840 | `				/* Return the parent class name */` |
|        5 |  6841 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        3 |  6842 | `			}else{` |
|        - |  6843 | `				/* Object does not have a parent class */` |
|        3 |  6844 | `				ph7_result_bool(pCtx,0);` |
|        - |  6845 | `			}` |
|        4 |  6846 | `		}else{` |
|        - |  6847 | `			/* Not a class instance,return FALSE */` |
|      ! 0 |  6848 | `			ph7_result_bool(pCtx,0);` |
|        - |  6849 | `		}` |
|        - |  6850 | `	}` |
|        9 |  6851 | `	return PH7_OK;` |
|        1 |  6852 |  |
|        - |  6853 | `/*` |
|        - |  6854 | ` * string get_called_class(void)` |
|        - |  6855 | ` *   Gets the name of the class the static method is called in.` |
|        - |  6856 | ` * Parameters` |
|        - |  6857 | ` *  None.` |
|        - |  6858 | ` * Return` |
|        - |  6859 | ` *  Returns the class name. Returns FALSE if called from outside a class.` |
|        - |  6860 | ` */` |
|        4 |  6861 | `static int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6862 |  |
|        - |  6863 | `	ph7_class *pClass;` |
|        - |  6864 | `	/* Check if we are inside a class [i.e: a method call] */` |
|        5 |  6865 | `	pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|        5 |  6866 | `	if( pClass ){` |
|        - |  6867 | `		SyString *pName;` |
|        - |  6868 | `		/* Point to the class name */` |
|        5 |  6869 | `		pName = &pClass->sName;` |
|        5 |  6870 | `		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        3 |  6871 | `	}else{` |
|      ! 0 |  6872 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6873 | `		SXUNUSED(apArg);` |
|        - |  6874 | `		/* Not inside class,return FALSE */` |
|      ! 0 |  6875 | `		ph7_result_bool(pCtx,0);` |
|        - |  6876 | `	}` |
|        5 |  6877 | `	return PH7_OK;` |
|        1 |  6878 |  |
|        - |  6879 | `/*` |
|        - |  6880 | ` * Extract a ph7_class from the given ph7_value.` |
|        - |  6881 | ` * The given value must be of type object [i.e: class instance] or` |
|        - |  6882 | ` * string which hold the class name.` |
|        - |  6883 | ` */` |
|       78 |  6884 | `static ph7_class * VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|        2 |  6885 |  |
|       80 |  6886 | `	ph7_class *pClass = 0;` |
|       80 |  6887 | `	if( ph7_value_is_object(pArg) ){` |
|        - |  6888 | `		/* Class instance already loaded,no need to perform a lookup */` |
|       44 |  6889 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|       59 |  6890 | `	}else if( ph7_value_is_string(pArg) ){` |
|        - |  6891 | `		const char *zClass;` |
|        - |  6892 | `		int nLen;` |
|        - |  6893 | `		/* Extract class name */` |
|       35 |  6894 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|       35 |  6895 | `		if( nLen > 0 ){` |
|        - |  6896 | `			SyHashEntry *pEntry;` |
|        - |  6897 | `			/* Perform a lookup */` |
|       35 |  6898 | `			pEntry = SyHashGet(&pVm->hClass,(const void *)zClass,(sxu32)nLen);` |
|       35 |  6899 | `			if( pEntry ){` |
|        - |  6900 | `				/* Point to the desired class */` |
|       31 |  6901 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|       15 |  6902 | `			}` |
|       17 |  6903 | `		}` |
|       17 |  6904 | `	}` |
|       80 |  6905 | `	return pClass;` |
|        2 |  6906 |  |
|        - |  6907 | `/*` |
|        - |  6908 | ` * bool property_exists(mixed $class,string $property)` |
|        - |  6909 | ` *   Checks if the object or class has a property.` |
|        - |  6910 | ` * Parameters` |
|        - |  6911 | ` *  class` |
|        - |  6912 | ` *   The class name or an object of the class to test for` |
|        - |  6913 | ` * property` |
|        - |  6914 | ` *  The name of the property` |
|        - |  6915 | ` * Return` |
|        - |  6916 | ` *   Returns TRUE if the property exists,FALSE otherwise.` |
|        - |  6917 | ` */` |
|       12 |  6918 | `static int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6919 |  |
|       13 |  6920 | `	int res = 0; /* Assume attribute does not exists */` |
|       13 |  6921 | `	if( nArg > 1 ){` |
|        - |  6922 | `		ph7_class *pClass;` |
|       13 |  6923 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       13 |  6924 | `		if( pClass ){` |
|        - |  6925 | `			const char *zName;` |
|        - |  6926 | `			int nLen;` |
|        - |  6927 | `			/* Extract attribute name */` |
|       13 |  6928 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|       13 |  6929 | `			if( nLen > 0 ){` |
|        - |  6930 | `				/* Perform the lookup in the attribute and method table */` |
|       12 |  6931 | `				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0` |
|        8 |  6932 | `					\|\| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6933 | `						/* property exists,flag that */` |
|       11 |  6934 | `						res = 1;` |
|        5 |  6935 | `				}` |
|        6 |  6936 | `			}` |
|        6 |  6937 | `		}` |
|        6 |  6938 | `	}` |
|       13 |  6939 | `	ph7_result_bool(pCtx,res);` |
|       13 |  6940 | `	return PH7_OK;` |
|        1 |  6941 |  |
|        - |  6942 | `/*` |
|        - |  6943 | ` * bool method_exists(mixed $class,string $method)` |
|        - |  6944 | ` *   Checks if the given method is a class member.` |
|        - |  6945 | ` * Parameters` |
|        - |  6946 | ` *  class` |
|        - |  6947 | ` *   The class name or an object of the class to test for` |
|        - |  6948 | ` * property` |
|        - |  6949 | ` *  The name of the method` |
|        - |  6950 | ` * Return` |
|        - |  6951 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|        - |  6952 | ` */` |
|        4 |  6953 | `static int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6954 |  |
|        5 |  6955 | `	int res = 0; /* Assume method does not exists */` |
|        5 |  6956 | `	if( nArg > 1 ){` |
|        - |  6957 | `		ph7_class *pClass;` |
|        5 |  6958 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        5 |  6959 | `		if( pClass ){` |
|        - |  6960 | `			const char *zName;` |
|        - |  6961 | `			int nLen;` |
|        - |  6962 | `			/* Extract method name */` |
|        5 |  6963 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|        5 |  6964 | `			if( nLen > 0 ){` |
|        - |  6965 | `				/* Perform the lookup in the method table */` |
|        5 |  6966 | `				if( SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6967 | `					/* method exists,flag that */` |
|        3 |  6968 | `					res = 1;` |
|        1 |  6969 | `				}` |
|        2 |  6970 | `			}` |
|        2 |  6971 | `		}` |
|        2 |  6972 | `	}` |
|        5 |  6973 | `	ph7_result_bool(pCtx,res);` |
|        5 |  6974 | `	return PH7_OK;` |
|        1 |  6975 |  |
|        - |  6976 | `/*` |
|        - |  6977 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|        - |  6978 | ` *   Checks if the class has been defined.` |
|        - |  6979 | ` * Parameters` |
|        - |  6980 | ` *  class_name` |
|        - |  6981 | ` *   The class name. The name is matched in a case-sensitive manner` |
|        - |  6982 | ` *   unlinke the standard PHP engine.` |
|        - |  6983 | ` *  autoload` |
|        - |  6984 | ` *   Whether or not to call __autoload by default.` |
|        - |  6985 | ` * Return` |
|        - |  6986 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|        - |  6987 | ` */` |
|       12 |  6988 | `static int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6989 |  |
|       14 |  6990 | `	int res = 0; /* Assume class does not exists */` |
|       14 |  6991 | `	if( nArg > 0 ){` |
|        - |  6992 | `		const char *zName;` |
|        - |  6993 | `		int nLen;` |
|        - |  6994 | `		/* Extract given name */` |
|       14 |  6995 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6996 | `		/* Perform a hashlookup */` |
|       14 |  6997 | `		if( nLen > 0 && SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6998 | `			/* class is available */` |
|       10 |  6999 | `			res = 1;` |
|        4 |  7000 | `		}` |
|        6 |  7001 | `	}` |
|       14 |  7002 | `	ph7_result_bool(pCtx,res);` |
|       14 |  7003 | `	return PH7_OK;` |
|        2 |  7004 |  |
|        - |  7005 | `/*` |
|        - |  7006 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|        - |  7007 | ` *   Checks if the interface has been defined.` |
|        - |  7008 | ` * Parameters` |
|        - |  7009 | ` *  class_name` |
|        - |  7010 | ` *   The class name. The name is matched in a case-sensitive manner` |
|        - |  7011 | ` *   unlinke the standard PHP engine.` |
|        - |  7012 | ` *  autoload` |
|        - |  7013 | ` *   Whether or not to call __autoload by default.` |
|        - |  7014 | ` * Return` |
|        - |  7015 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|        - |  7016 | ` */` |
|        6 |  7017 | `static int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7018 |  |
|        7 |  7019 | `	int res = 0; /* Assume class does not exists */` |
|        7 |  7020 | `	if( nArg > 0 ){` |
|        7 |  7021 | `		SyHashEntry *pEntry = 0;` |
|        - |  7022 | `		const char *zName;` |
|        - |  7023 | `		int nLen;` |
|        - |  7024 | `		/* Extract given name */` |
|        7 |  7025 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7026 | `		/* Perform a hashlookup */` |
|        7 |  7027 | `		if( nLen > 0 ){` |
|        7 |  7028 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|        3 |  7029 | `		}` |
|        7 |  7030 | `		if( pEntry ){` |
|        5 |  7031 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        5 |  7032 | `			while( pClass ){` |
|        5 |  7033 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |  7034 | `					/* interface is available */` |
|        5 |  7035 | `					res = 1;` |
|        5 |  7036 | `					break;` |
|        - |  7037 | `				}` |
|        - |  7038 | `				/* Next with the same name */` |
|      ! 0 |  7039 | `				pClass = pClass->pNextName;` |
|      ! 0 |  7040 | `			}` |
|        2 |  7041 | `		}` |
|        3 |  7042 | `	}` |
|        7 |  7043 | `	ph7_result_bool(pCtx,res);` |
|        7 |  7044 | `	return PH7_OK;` |
|        1 |  7045 |  |
|        - |  7046 | `/*` |
|        - |  7047 | ` * bool class_alias([string $original[,string $alias ]])` |
|        - |  7048 | ` *   Creates an alias for a class.` |
|        - |  7049 | ` * Parameters` |
|        - |  7050 | ` *  original` |
|        - |  7051 | ` *    The original class.` |
|        - |  7052 | ` *  alias` |
|        - |  7053 | ` *   The alias name for the class.` |
|        - |  7054 | ` * Return` |
|        - |  7055 | ` *   Returns TRUE on success or FALSE on failure.` |
|        - |  7056 | ` */` |
|        2 |  7057 | `static int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7058 |  |
|        - |  7059 | `	const char *zOld,*zNew;` |
|        - |  7060 | `	int nOldLen,nNewLen;` |
|        - |  7061 | `	SyHashEntry *pEntry;` |
|        - |  7062 | `	ph7_class *pClass;` |
|        - |  7063 | `	char *zDup;` |
|        - |  7064 | `	sxi32 rc;` |
|        3 |  7065 | `	if( nArg < 2 ){` |
|        - |  7066 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  7067 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7068 | `		return PH7_OK;` |
|        - |  7069 | `	}` |
|        - |  7070 | `	/* Extract old class name */` |
|        3 |  7071 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|        - |  7072 | `	/* Extract alias name */` |
|        3 |  7073 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|        3 |  7074 | `	if( nNewLen < 1 ){` |
|        - |  7075 | `		/* Invalid alias name,return FALSE */` |
|      ! 0 |  7076 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7077 | `		return PH7_OK;` |
|        - |  7078 | `	}` |
|        - |  7079 | `	/* Perform a hash lookup */` |
|        3 |  7080 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,(sxu32)nOldLen);` |
|        3 |  7081 | `	if( pEntry ==  0 ){` |
|        - |  7082 | `		/* No such class,return FALSE */` |
|      ! 0 |  7083 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7084 | `		return PH7_OK;` |
|        - |  7085 | `	}` |
|        - |  7086 | `	/* Point to the class */` |
|        3 |  7087 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7088 | `	/* Duplicate alias name */` |
|        3 |  7089 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,(sxu32)nNewLen);` |
|        3 |  7090 | `	if( zDup == 0 ){` |
|        - |  7091 | `		/* Out of memory,return FALSE */` |
|      ! 0 |  7092 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7093 | `		return PH7_OK;` |
|        - |  7094 | `	}` |
|        - |  7095 | `	/* Create the alias */` |
|        3 |  7096 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,(sxu32)nNewLen,pClass);` |
|        3 |  7097 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7098 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|      ! 0 |  7099 | `	}` |
|        3 |  7100 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|        3 |  7101 | `	return PH7_OK;` |
|        2 |  7102 |  |
|        - |  7103 | `/*` |
|        - |  7104 | ` * array get_declared_classes(void)` |
|        - |  7105 | ` *   Returns an array with the name of the defined classes` |
|        - |  7106 | ` * Parameters` |
|        - |  7107 | ` *  None` |
|        - |  7108 | ` * Return` |
|        - |  7109 | ` *   Returns an array of the names of the declared classes` |
|        - |  7110 | ` *   in the current script.` |
|        - |  7111 | ` * Note:` |
|        - |  7112 | ` *   NULL is returned on failure.` |
|        - |  7113 | ` */` |
|        2 |  7114 | `static int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7115 |  |
|        - |  7116 | `	ph7_value *pName,*pArray;` |
|        - |  7117 | `	SyHashEntry *pEntry;` |
|        - |  7118 | `	/* Create a new array first */` |
|        3 |  7119 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7120 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7121 | `	if( pArray == 0 \|\| pName == 0){` |
|      ! 0 |  7122 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7123 | `		SXUNUSED(apArg);` |
|        - |  7124 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7125 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7126 | `		return PH7_OK;` |
|        - |  7127 | `	}` |
|        - |  7128 | `	/* Fill the array with the defined classes */` |
|        3 |  7129 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|       52 |  7130 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|       49 |  7131 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7132 | `		/* Do not register classes defined as interfaces */` |
|       49 |  7133 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       43 |  7134 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|        - |  7135 | `			/* insert class name */` |
|       43 |  7136 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7137 | `			/* Reset the cursor */` |
|       43 |  7138 | `			ph7_value_reset_string_cursor(pName);` |
|       21 |  7139 | `		}` |
|        1 |  7140 | `	}` |
|        - |  7141 | `	/* Return the created array */` |
|        3 |  7142 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7143 | `	return PH7_OK;` |
|        2 |  7144 |  |
|        - |  7145 | `/*` |
|        - |  7146 | ` * array get_declared_interfaces(void)` |
|        - |  7147 | ` *   Returns an array with the name of the defined interfaces` |
|        - |  7148 | ` * Parameters` |
|        - |  7149 | ` *  None` |
|        - |  7150 | ` * Return` |
|        - |  7151 | ` *   Returns an array of the names of the declared interfaces` |
|        - |  7152 | ` *   in the current script.` |
|        - |  7153 | ` * Note:` |
|        - |  7154 | ` *   NULL is returned on failure.` |
|        - |  7155 | ` */` |
|        2 |  7156 | `static int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7157 |  |
|        - |  7158 | `	ph7_value *pName,*pArray;` |
|        - |  7159 | `	SyHashEntry *pEntry;` |
|        - |  7160 | `	/* Create a new array first */` |
|        3 |  7161 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7162 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7163 | `	if( pArray == 0 \|\| pName == 0 ){` |
|      ! 0 |  7164 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7165 | `		SXUNUSED(apArg);` |
|        - |  7166 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7167 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7168 | `		return PH7_OK;` |
|        - |  7169 | `	}` |
|        - |  7170 | `	/* Fill the array with the defined classes */` |
|        3 |  7171 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|       54 |  7172 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|       51 |  7173 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7174 | `		/* Register classes defined as interfaces only */` |
|       51 |  7175 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        9 |  7176 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|        - |  7177 | `			/* insert interface name */` |
|        9 |  7178 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7179 | `			/* Reset the cursor */` |
|        9 |  7180 | `			ph7_value_reset_string_cursor(pName);` |
|        4 |  7181 | `		}` |
|        1 |  7182 | `	}` |
|        - |  7183 | `	/* Return the created array */` |
|        3 |  7184 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7185 | `	return PH7_OK;` |
|        2 |  7186 |  |
|        - |  7187 | `/*` |
|        - |  7188 | ` * array get_class_methods(string/object $class_name)` |
|        - |  7189 | ` *   Returns an array with the name of the class methods` |
|        - |  7190 | ` * Parameters` |
|        - |  7191 | ` *  class_name` |
|        - |  7192 | ` *  The class name or class instance` |
|        - |  7193 | ` * Return` |
|        - |  7194 | ` *  Returns an array of method names defined for the class specified by class_name.` |
|        - |  7195 | ` *  In case of an error, it returns NULL.` |
|        - |  7196 | ` * Note:` |
|        - |  7197 | ` *   NULL is returned on failure.` |
|        - |  7198 | ` */` |
|        6 |  7199 | `static int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7200 |  |
|        - |  7201 | `	ph7_value *pName,*pArray;` |
|        - |  7202 | `	SyHashEntry *pEntry;` |
|        - |  7203 | `	ph7_class *pClass;` |
|        - |  7204 | `	/* Extract the target class first */` |
|        7 |  7205 | `	pClass = 0;` |
|        7 |  7206 | `	if( nArg > 0 ){` |
|        7 |  7207 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        3 |  7208 | `	}` |
|        7 |  7209 | `	if( pClass == 0 ){` |
|        - |  7210 | `		/* No such class,return NULL */` |
|        3 |  7211 | `		ph7_result_null(pCtx);` |
|        3 |  7212 | `		return PH7_OK;` |
|        - |  7213 | `	}` |
|        - |  7214 | `	/* Create a new array  */` |
|        5 |  7215 | `	pArray = ph7_context_new_array(pCtx);` |
|        5 |  7216 | `	pName = ph7_context_new_scalar(pCtx);` |
|        5 |  7217 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7218 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7219 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7220 | `		return PH7_OK;` |
|        - |  7221 | `	}` |
|        - |  7222 | `	/* Fill the array with the defined methods */` |
|        5 |  7223 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|       17 |  7224 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       13 |  7225 | `		ph7_class_method *pMethod = (ph7_class_method *)pEntry->pUserData;` |
|        - |  7226 | `		/* Insert method name */` |
|       13 |  7227 | `		ph7_value_string(pName,SyStringData(&pMethod->sFunc.sName),(int)SyStringLength(&pMethod->sFunc.sName));` |
|       13 |  7228 | `		ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7229 | `		/* Reset the cursor */` |
|       13 |  7230 | `		ph7_value_reset_string_cursor(pName);` |
|        1 |  7231 | `	}` |
|        - |  7232 | `	/* Return the created array */` |
|        5 |  7233 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7234 | `	/*` |
|        - |  7235 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7236 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7237 | `	 */` |
|        5 |  7238 | `	return PH7_OK;` |
|        4 |  7239 |  |
|        - |  7240 | `/*` |
|        - |  7241 | ` * This function return TRUE(1) if the given class attribute stored` |
|        - |  7242 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|        - |  7243 | ` * from the current scope.Otherwise FALSE is returned.` |
|        - |  7244 | ` */` |
|     2840 |  7245 | `static int VmClassMemberAccess(` |
|        - |  7246 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7247 | `	ph7_class *pClass,         /* Target Class */` |
|        - |  7248 | `	const SyString *pAttrName, /* Attribute name */` |
|        - |  7249 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|        - |  7250 | `	int bLog                   /* TRUE to log forbidden access. */` |
|        - |  7251 | `	)` |
|        2 |  7252 |  |
|     2842 |  7253 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     2278 |  7254 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  7255 | `		ph7_vm_func *pVmFunc;` |
|     2282 |  7256 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|        - |  7257 | `			/* Safely ignore the exception frame */` |
|        5 |  7258 | `			pFrame = pFrame->pParent;` |
|        1 |  7259 | `		}` |
|     2278 |  7260 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     2278 |  7261 | `		if( pVmFunc == 0 \|\| (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|        9 |  7262 | `			goto dis; /* Access is forbidden */` |
|        - |  7263 | `		}` |
|     2270 |  7264 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - |  7265 | `			/* Must be the same instance */` |
|        7 |  7266 | `			if( (ph7_class *)pVmFunc->pUserData != pClass ){` |
|      ! 0 |  7267 | `				goto dis; /* Access is forbidden */` |
|        - |  7268 | `			}` |
|        4 |  7269 | `		}else{` |
|        - |  7270 | `			/* Protected */` |
|     2264 |  7271 | `			ph7_class *pBase = (ph7_class *)pVmFunc->pUserData;` |
|        - |  7272 | `			/* Must be a derived class */` |
|     2264 |  7273 | `			if( !VmInstanceOf(pClass,pBase) ){` |
|      ! 0 |  7274 | `				goto dis; /* Access is forbidden */` |
|        - |  7275 | `			}` |
|        - |  7276 | `		}` |
|     1134 |  7277 | `	}` |
|     2834 |  7278 | `	return 1; /* Access is granted */` |
|        4 |  7279 | `dis:` |
|        9 |  7280 | `	if( bLog ){` |
|      ! 0 |  7281 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7282 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|      ! 0 |  7283 | `			&pClass->sName,pAttrName);` |
|      ! 0 |  7284 | `	}` |
|        9 |  7285 | `	return 0; /* Access is forbidden */` |
|     1422 |  7286 |  |
|        - |  7287 | `/*` |
|        - |  7288 | ` * array get_class_vars(string/object $class_name)` |
|        - |  7289 | ` *   Get the default properties of the class` |
|        - |  7290 | ` * Parameters` |
|        - |  7291 | ` *  class_name` |
|        - |  7292 | ` *   The class name or class instance` |
|        - |  7293 | ` * Return` |
|        - |  7294 | ` *  Returns an associative array of declared properties visible from the current scope` |
|        - |  7295 | ` *  with their default value. The resulting array elements are in the form` |
|        - |  7296 | ` *  of varname => value.` |
|        - |  7297 | ` * Note:` |
|        - |  7298 | ` *   NULL is returned on failure.` |
|        - |  7299 | ` */` |
|        2 |  7300 | `static int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7301 |  |
|        - |  7302 | `	ph7_value *pName,*pArray,sValue;` |
|        - |  7303 | `	SyHashEntry *pEntry;` |
|        - |  7304 | `	ph7_class *pClass;` |
|        - |  7305 | `	/* Extract the target class first */` |
|        3 |  7306 | `	pClass = 0;` |
|        3 |  7307 | `	if( nArg > 0 ){` |
|        3 |  7308 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        1 |  7309 | `	}` |
|        3 |  7310 | `	if( pClass == 0 ){` |
|        - |  7311 | `		/* No such class,return NULL */` |
|      ! 0 |  7312 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7313 | `		return PH7_OK;` |
|        - |  7314 | `	}` |
|        - |  7315 | `	/* Create a new array  */` |
|        3 |  7316 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7317 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7318 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|        3 |  7319 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7320 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7321 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7322 | `		return PH7_OK;` |
|        - |  7323 | `	}` |
|        - |  7324 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|        3 |  7325 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        8 |  7326 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        5 |  7327 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|        - |  7328 | `		/* Check if the access is allowed */` |
|        5 |  7329 | `		if( VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        5 |  7330 | `			SyString *pAttrName = &pAttr->sName;` |
|        5 |  7331 | `			ph7_value *pValue = 0;` |
|        5 |  7332 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |  7333 | `				/* Extract static attribute value which is always computed */` |
|        5 |  7334 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|        3 |  7335 | `			}else{` |
|      ! 0 |  7336 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      ! 0 |  7337 | `					PH7_MemObjRelease(&sValue);` |
|        - |  7338 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|      ! 0 |  7339 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue);` |
|      ! 0 |  7340 | `					pValue = &sValue;` |
|      ! 0 |  7341 | `				}` |
|        - |  7342 | `			}` |
|        - |  7343 | `			/* Fill in the array */` |
|        5 |  7344 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|        5 |  7345 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|        - |  7346 | `			/* Reset the cursor */` |
|        5 |  7347 | `			ph7_value_reset_string_cursor(pName);` |
|        2 |  7348 | `		}` |
|        1 |  7349 | `	}` |
|        3 |  7350 | `	PH7_MemObjRelease(&sValue);` |
|        - |  7351 | `	/* Return the created array */` |
|        3 |  7352 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7353 | `	/*` |
|        - |  7354 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7355 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7356 | `	 */` |
|        3 |  7357 | `	return PH7_OK;` |
|        2 |  7358 |  |
|        - |  7359 | `/*` |
|        - |  7360 | ` * array get_object_vars(object $this)` |
|        - |  7361 | ` *   Gets the properties of the given object` |
|        - |  7362 | ` * Parameters` |
|        - |  7363 | ` *  this` |
|        - |  7364 | ` *   A class instance` |
|        - |  7365 | ` * Return` |
|        - |  7366 | ` *  Returns an associative array of defined object accessible non-static properties` |
|        - |  7367 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|        - |  7368 | ` *  it will be returned with a NULL value.` |
|        - |  7369 | ` * Note:` |
|        - |  7370 | ` *   NULL is returned on failure.` |
|        - |  7371 | ` */` |
|        2 |  7372 | `static int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7373 |  |
|        3 |  7374 | `	ph7_class_instance *pThis = 0;` |
|        - |  7375 | `	ph7_value *pName,*pArray;` |
|        - |  7376 | `	SyHashEntry *pEntry;` |
|        3 |  7377 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|        - |  7378 | `		/* Extract the target instance */` |
|        3 |  7379 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        1 |  7380 | `	}` |
|        3 |  7381 | `	if( pThis == 0 ){` |
|        - |  7382 | `		/* No such instance,return NULL */` |
|      ! 0 |  7383 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7384 | `		return PH7_OK;` |
|        - |  7385 | `	}` |
|        - |  7386 | `	/* Create a new array  */` |
|        3 |  7387 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7388 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7389 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7390 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7391 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7392 | `		return PH7_OK;` |
|        - |  7393 | `	}` |
|        - |  7394 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|        3 |  7395 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  7396 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        7 |  7397 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  7398 | `		SyString *pAttrName;` |
|        7 |  7399 | `		if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - |  7400 | `			/* Only non-static/constant attributes are extracted */` |
|      ! 0 |  7401 | `			continue;` |
|        - |  7402 | `		}` |
|        7 |  7403 | `		pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  7404 | `		/* Check if the access is allowed */` |
|        7 |  7405 | `		if( VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|        3 |  7406 | `			ph7_value *pValue = 0;` |
|        - |  7407 | `			/* Extract attribute */` |
|        3 |  7408 | `			pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|        3 |  7409 | `			if( pValue ){` |
|        - |  7410 | `				/* Insert attribute name in the array */` |
|        3 |  7411 | `				ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|        3 |  7412 | `				ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|        1 |  7413 | `			}` |
|        - |  7414 | `			/* Reset the cursor */` |
|        3 |  7415 | `			ph7_value_reset_string_cursor(pName);` |
|        1 |  7416 | `		}` |
|        1 |  7417 | `	}` |
|        - |  7418 | `	/* Return the created array */` |
|        3 |  7419 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7420 | `	/*` |
|        - |  7421 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7422 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7423 | `	 */` |
|        3 |  7424 | `	return PH7_OK;` |
|        2 |  7425 |  |
|        - |  7426 | `/*` |
|        - |  7427 | ` * This function returns TRUE if the given class is an implemented` |
|        - |  7428 | ` * interface.Otherwise FALSE is returned.` |
|        - |  7429 | ` */` |
|     5148 |  7430 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|        2 |  7431 |  |
|        - |  7432 | `	ph7_class **apInterface;` |
|        - |  7433 | `	sxu32 n;` |
|     5150 |  7434 | `	if( SySetUsed(pSet) < 1 ){` |
|        - |  7435 | `		/* Empty interface container */` |
|     5148 |  7436 | `		return FALSE;` |
|        - |  7437 | `	}` |
|        - |  7438 | `	/* Point to the set of implemented interfaces */` |
|        3 |  7439 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|        - |  7440 | `	/* Perform the lookup */` |
|        3 |  7441 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
|        3 |  7442 | `		if( apInterface[n] == pClass ){` |
|        3 |  7443 | `			return TRUE;` |
|        - |  7444 | `		}` |
|      ! 0 |  7445 | `	}` |
|      ! 0 |  7446 | `	return FALSE;` |
|     2576 |  7447 |  |
|        - |  7448 | `/*` |
|        - |  7449 | ` * This function returns TRUE if the given class (first argument)` |
|        - |  7450 | ` * is an instance of the main class (second argument).` |
|        - |  7451 | ` * Otherwise FALSE is returned.` |
|        - |  7452 | ` */` |
|     2318 |  7453 | `static int VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|        2 |  7454 |  |
|        - |  7455 | `	ph7_class *pParent;` |
|        - |  7456 | `	sxi32 rc;` |
|     2320 |  7457 | `	if( pThis == pClass ){` |
|        - |  7458 | `		/* Instance of the same class */` |
|      140 |  7459 | `		return TRUE;` |
|        - |  7460 | `	}` |
|        - |  7461 | `	/* Check implemented interfaces */` |
|     2182 |  7462 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
|     2182 |  7463 | `	if( rc ){` |
|        3 |  7464 | `		return TRUE;` |
|        - |  7465 | `	}` |
|        - |  7466 | `	/* Check parent classes */` |
|     2180 |  7467 | `	pParent = pThis->pBase;` |
|     5148 |  7468 | `	while( pParent ){` |
|     5146 |  7469 | `		if( pParent == pClass ){` |
|        - |  7470 | `			/* Same instance */` |
|     2178 |  7471 | `			return TRUE;` |
|        - |  7472 | `		}` |
|        - |  7473 | `		/* Check the implemented interfaces */` |
|     2970 |  7474 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|     2970 |  7475 | `		if( rc ){` |
|      ! 0 |  7476 | `			return TRUE;` |
|        - |  7477 | `		}` |
|        - |  7478 | `		/* Point to the parent class */` |
|     2970 |  7479 | `		pParent = pParent->pBase;` |
|        2 |  7480 | `	}` |
|        - |  7481 | `	/* Not an instance of the the given class */` |
|        3 |  7482 | `	return FALSE;` |
|     1161 |  7483 |  |
|        - |  7484 | `/*` |
|        - |  7485 | ` * This function returns TRUE if the given class (first argument)` |
|        - |  7486 | ` * is a subclass of the main class (second argument).` |
|        - |  7487 | ` * Otherwise FALSE is returned.` |
|        - |  7488 | ` */` |
|        4 |  7489 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|        1 |  7490 |  |
|        5 |  7491 | `	SySet *pInterface = &pClass->aInterface;` |
|        - |  7492 | `	SyHashEntry *pEntry;` |
|        - |  7493 | `	SyString *pName;` |
|        - |  7494 | `	sxi32 rc;` |
|        5 |  7495 | `	while( pClass ){` |
|        5 |  7496 | `		pName = &pClass->sName;` |
|        - |  7497 | `		/* Query the derived hashtable */` |
|        5 |  7498 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|        5 |  7499 | `		if( pEntry ){` |
|        5 |  7500 | `			return TRUE;` |
|        - |  7501 | `		}` |
|      ! 0 |  7502 | `		pClass = pClass->pBase;` |
|      ! 0 |  7503 | `	}` |
|      ! 0 |  7504 | `	rc = VmQueryInterfaceSet(pBase,pInterface);` |
|      ! 0 |  7505 | `	if( rc ){` |
|      ! 0 |  7506 | `		return TRUE;` |
|        - |  7507 | `	}` |
|        - |  7508 | `	/* Not a subclass */` |
|      ! 0 |  7509 | `	return FALSE;` |
|        3 |  7510 |  |
|        - |  7511 | `/*` |
|        - |  7512 | ` * bool is_a(object $object,string $class_name)` |
|        - |  7513 | ` *   Checks if the object is of this class or has this class as one of its parents.` |
|        - |  7514 | ` * Parameters` |
|        - |  7515 | ` *  object` |
|        - |  7516 | ` *   The tested object` |
|        - |  7517 | ` * class_name` |
|        - |  7518 | ` *  The class name` |
|        - |  7519 | ` * Return` |
|        - |  7520 | ` *   Returns TRUE if the object is of this class or has this class as one of its` |
|        - |  7521 | ` *   parents, FALSE otherwise.` |
|        - |  7522 | ` */` |
|        2 |  7523 | `static int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7524 |  |
|        3 |  7525 | `	int res = 0; /* Assume FALSE by default */` |
|        3 |  7526 | `	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){` |
|        3 |  7527 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7528 | `		ph7_class *pClass;` |
|        - |  7529 | `		/* Extract the given class */` |
|        3 |  7530 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|        3 |  7531 | `		if( pClass ){` |
|        - |  7532 | `			/* Perform the query */` |
|        3 |  7533 | `			res = VmInstanceOf(pThis->pClass,pClass);` |
|        1 |  7534 | `		}` |
|        1 |  7535 | `	}` |
|        - |  7536 | `	/* Query result */` |
|        3 |  7537 | `	ph7_result_bool(pCtx,res);` |
|        3 |  7538 | `	return PH7_OK;` |
|        1 |  7539 |  |
|        - |  7540 | `/*` |
|        - |  7541 | ` * bool is_subclass_of(object/string $object,object/string $class_name)` |
|        - |  7542 | ` *   Checks if the object has this class as one of its parents.` |
|        - |  7543 | ` * Parameters` |
|        - |  7544 | ` *  object` |
|        - |  7545 | ` *   The tested object` |
|        - |  7546 | ` * class_name` |
|        - |  7547 | ` *  The class name` |
|        - |  7548 | ` * Return` |
|        - |  7549 | ` *  This function returns TRUE if the object , belongs to a class` |
|        - |  7550 | ` *  which is a subclass of class_name, FALSE otherwise.` |
|        - |  7551 | ` */` |
|        6 |  7552 | `static int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7553 |  |
|        7 |  7554 | `	int res = 0; /* Assume FALSE by default */` |
|        7 |  7555 | `	if( nArg > 1 ){` |
|        - |  7556 | `		ph7_class *pClass,*pMain;` |
|        - |  7557 | `		/* Extract the given classes */` |
|        7 |  7558 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        7 |  7559 | `		pMain = VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|        7 |  7560 | `		if( pClass && pMain ){` |
|        - |  7561 | `			/* Perform the query */` |
|        5 |  7562 | `			res = VmSubclassOf(pClass,pMain);` |
|        2 |  7563 | `		}` |
|        3 |  7564 | `	}` |
|        - |  7565 | `	/* Query result */` |
|        7 |  7566 | `	ph7_result_bool(pCtx,res);` |
|        7 |  7567 | `	return PH7_OK;` |
|        1 |  7568 |  |
|        - |  7569 | `/*` |
|        - |  7570 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  7571 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  7572 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  7573 | ` * return value indicates failure.` |
|        - |  7574 | ` */` |
|      918 |  7575 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  7576 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7577 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  7578 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  7579 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  7580 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  7581 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  7582 | `	)` |
|        2 |  7583 |  |
|        - |  7584 | `	ph7_value *aStack;` |
|        - |  7585 | `	VmInstr aInstr[2];` |
|        - |  7586 | `	int iCursor;` |
|        - |  7587 | `	int i;` |
|        - |  7588 | `	/* Create a new operand stack */` |
|      920 |  7589 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|      920 |  7590 | `	if( aStack == 0 ){` |
|      ! 0 |  7591 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7592 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  7593 | `		return SXERR_MEM;` |
|        - |  7594 | `	}` |
|        - |  7595 | `	/* Fill the operand stack with the given arguments */` |
|     1350 |  7596 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      432 |  7597 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7598 | `		/*` |
|        - |  7599 | `		 * Symisc eXtension:` |
|        - |  7600 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7601 | `		 */` |
|      432 |  7602 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      217 |  7603 | `	}` |
|      920 |  7604 | `	iCursor = nArg + 1;` |
|      920 |  7605 | `	if( pThis ){` |
|        - |  7606 | `		/*` |
|        - |  7607 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  7608 | `		 */` |
|      914 |  7609 | `		pThis->iRef++; /* Increment reference count */` |
|      914 |  7610 | `		aStack[i].x.pOther = pThis;` |
|      914 |  7611 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      456 |  7612 | `	}` |
|      920 |  7613 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|      920 |  7614 | `	i++;` |
|        - |  7615 | `	/* Push method name */` |
|      920 |  7616 | `	SyBlobReset(&aStack[i].sBlob);` |
|      920 |  7617 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|      920 |  7618 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|      920 |  7619 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  7620 | `	/* Emit the CALL istruction */` |
|      920 |  7621 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      920 |  7622 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      920 |  7623 | `	aInstr[0].iP2 = 0;` |
|      920 |  7624 | `	aInstr[0].p3  = 0;` |
|        - |  7625 | `	/* Emit the DONE instruction */` |
|      920 |  7626 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      920 |  7627 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|      920 |  7628 | `	aInstr[1].iP2 = 0;` |
|      920 |  7629 | `	aInstr[1].p3  = 0;` |
|        - |  7630 | `	/* Execute the method body (if available) */` |
|      920 |  7631 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  7632 | `	/* Clean up the mess left behind */` |
|      920 |  7633 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      920 |  7634 | `	return PH7_OK;` |
|      461 |  7635 |  |
|        - |  7636 | `/*` |
|        - |  7637 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  7638 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  7639 | ` * in the apArg[] array.` |
|        - |  7640 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7641 | ` * return value indicates failure.` |
|        - |  7642 | ` */` |
|      800 |  7643 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  7644 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7645 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7646 | `	int nArg,          /* Total number of given arguments */` |
|        - |  7647 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  7648 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  7649 | `	)` |
|        2 |  7650 |  |
|        - |  7651 | `	ph7_value *aStack;` |
|        - |  7652 | `	VmInstr aInstr[2];` |
|        - |  7653 | `	int i;` |
|      802 |  7654 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  7655 | `		/* Don't bother processing,it's invalid anyway */` |
|      359 |  7656 | `		if( pResult ){` |
|        - |  7657 | `			/* Assume a null return value */` |
|      ! 0 |  7658 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7659 | `		}` |
|      359 |  7660 | `		return SXERR_INVALID;` |
|        - |  7661 | `	}` |
|      444 |  7662 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7663 | `		/* Class method */` |
|       11 |  7664 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  7665 | `		ph7_class_method *pMethod = 0;` |
|       11 |  7666 | `		ph7_class_instance *pThis = 0;` |
|       11 |  7667 | `		ph7_class *pClass = 0;` |
|        - |  7668 | `		ph7_value *pValue;` |
|        - |  7669 | `		sxi32 rc;` |
|       11 |  7670 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  7671 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  7672 | `			if( pResult ){` |
|        - |  7673 | `				/* Assume a null return value */` |
|      ! 0 |  7674 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7675 | `			}` |
|      ! 0 |  7676 | `			return SXRET_OK;` |
|        - |  7677 | `		}` |
|        - |  7678 | `		/* Extract the class name or an instance of it */` |
|       11 |  7679 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  7680 | `		if( pValue ){` |
|       11 |  7681 | `			pClass = VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  7682 | `		}` |
|       11 |  7683 | `		if( pClass == 0 ){` |
|        - |  7684 | `			/* No such class,return NULL */` |
|      ! 0 |  7685 | `			if( pResult ){` |
|      ! 0 |  7686 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7687 | `			}` |
|      ! 0 |  7688 | `			return SXRET_OK;` |
|        - |  7689 | `		}` |
|       11 |  7690 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  7691 | `			/* Point to the class instance */` |
|        5 |  7692 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  7693 | `		}` |
|        - |  7694 | `		/* Try to extract the method */` |
|       11 |  7695 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  7696 | `		if( pValue ){` |
|       11 |  7697 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  7698 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  7699 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  7700 | `			}` |
|        5 |  7701 | `		}` |
|       11 |  7702 | `		if( pMethod == 0 ){` |
|        - |  7703 | `			/* No such method,return NULL */` |
|      ! 0 |  7704 | `			if( pResult ){` |
|      ! 0 |  7705 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7706 | `			}` |
|      ! 0 |  7707 | `			return SXRET_OK;` |
|        - |  7708 | `		}` |
|        - |  7709 | `		/* Call the class method */` |
|       11 |  7710 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  7711 | `		return rc;` |
|        - |  7712 | `	}` |
|        - |  7713 | `	/* Create a new operand stack */` |
|      434 |  7714 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      434 |  7715 | `	if( aStack == 0 ){` |
|      ! 0 |  7716 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7717 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  7718 | `		if( pResult ){` |
|        - |  7719 | `			/* Assume a null return value */` |
|      ! 0 |  7720 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7721 | `		}` |
|      ! 0 |  7722 | `		return SXERR_MEM;` |
|        - |  7723 | `	}` |
|        - |  7724 | `	/* Fill the operand stack with the given arguments */` |
|     1428 |  7725 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      996 |  7726 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7727 | `		/*` |
|        - |  7728 | `		 * Symisc eXtension:` |
|        - |  7729 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7730 | `		 */` |
|      996 |  7731 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      499 |  7732 | `	}` |
|        - |  7733 | `	/* Push the function name */` |
|      434 |  7734 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      434 |  7735 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7736 | `	/* Emit the CALL istruction */` |
|      434 |  7737 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      434 |  7738 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      434 |  7739 | `	aInstr[0].iP2 = 0;` |
|      434 |  7740 | `	aInstr[0].p3  = 0;` |
|        - |  7741 | `	/* Emit the DONE instruction */` |
|      434 |  7742 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      434 |  7743 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      434 |  7744 | `	aInstr[1].iP2 = 0;` |
|      434 |  7745 | `	aInstr[1].p3  = 0;` |
|        - |  7746 | `	/* Execute the function body (if available) */` |
|      434 |  7747 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  7748 | `	/* Clean up the mess left behind */` |
|      434 |  7749 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      434 |  7750 | `	return PH7_OK;` |
|      402 |  7751 |  |
|        - |  7752 | `/*` |
|        - |  7753 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  7754 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  7755 | ` * parameter.` |
|        - |  7756 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7757 | ` * return value indicates failure.` |
|        - |  7758 | ` */` |
|      236 |  7759 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  7760 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7761 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7762 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  7763 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  7764 | `	)` |
|        1 |  7765 |  |
|        - |  7766 | `	ph7_value *pArg;` |
|        - |  7767 | `	SySet aArg;` |
|        - |  7768 | `	va_list ap;` |
|        - |  7769 | `	sxi32 rc;` |
|      237 |  7770 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7771 | `	/* Copy arguments one after one */` |
|      237 |  7772 | `	va_start(ap,pResult);` |
|      393 |  7773 | `	for(;;){` |
|      787 |  7774 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  7775 | `		if( pArg == 0 ){` |
|      237 |  7776 | `			break;` |
|        - |  7777 | `		}` |
|      551 |  7778 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  7779 | `	}` |
|        - |  7780 | `	/* Call the core routine */` |
|      237 |  7781 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  7782 | `	/* Cleanup */` |
|      237 |  7783 | `	SySetRelease(&aArg);` |
|      237 |  7784 | `	return rc;` |
|        1 |  7785 |  |
|        - |  7786 | `/*` |
|        - |  7787 | ` * value call_user_func(callable $callback[,value $parameter[, value $... ]])` |
|        - |  7788 | ` *  Call the callback given by the first parameter.` |
|        - |  7789 | ` * Parameter` |
|        - |  7790 | ` *  $callback` |
|        - |  7791 | ` *   The callable to be called.` |
|        - |  7792 | ` *  ...` |
|        - |  7793 | ` *    Zero or more parameters to be passed to the callback.` |
|        - |  7794 | ` * Return` |
|        - |  7795 | ` *  Th return value of the callback, or FALSE on error.` |
|        - |  7796 | ` */` |
|       14 |  7797 | `static int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7798 |  |
|        - |  7799 | `	ph7_value sResult; /* Store callback return value here */` |
|        - |  7800 | `	sxi32 rc;` |
|       15 |  7801 | `	if( nArg < 1 ){` |
|        - |  7802 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  7803 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7804 | `		return PH7_OK;` |
|        - |  7805 | `	}` |
|       15 |  7806 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|       15 |  7807 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7808 | `	/* Try to invoke the callback */` |
|       15 |  7809 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|       15 |  7810 | `	if( rc != SXRET_OK ){` |
|        - |  7811 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|      ! 0 |  7812 | `		ph7_result_bool(pCtx,0); /* return false */` |
|      ! 0 |  7813 | `	}else{` |
|        - |  7814 | `		/* Callback result */` |
|       15 |  7815 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        - |  7816 | `	}` |
|       15 |  7817 | `	PH7_MemObjRelease(&sResult);` |
|       15 |  7818 | `	return PH7_OK;` |
|        8 |  7819 |  |
|        - |  7820 | `/*` |
|        - |  7821 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|        - |  7822 | ` *  Call a callback with an array of parameters.` |
|        - |  7823 | ` * Parameter` |
|        - |  7824 | ` *  $callback` |
|        - |  7825 | ` *   The callable to be called.` |
|        - |  7826 | ` * $param_arr` |
|        - |  7827 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|        - |  7828 | ` * Return` |
|        - |  7829 | ` *  Returns the return value of the callback, or FALSE on error.` |
|        - |  7830 | ` */` |
|       10 |  7831 | `static int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7832 |  |
|        - |  7833 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|        - |  7834 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|        - |  7835 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|        - |  7836 | `	SySet aArg;               /* Arguments containers */` |
|        - |  7837 | `	sxi32 rc;` |
|        - |  7838 | `	sxu32 n;` |
|       11 |  7839 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|        - |  7840 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  7841 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7842 | `		return PH7_OK;` |
|        - |  7843 | `	}` |
|       11 |  7844 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|       11 |  7845 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7846 | `	/* Initialize the arguments container */` |
|       11 |  7847 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7848 | `	/* Turn hashmap entries into callback arguments */` |
|       11 |  7849 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       11 |  7850 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|       23 |  7851 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        - |  7852 | `		/* Extract node value */` |
|       13 |  7853 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|       13 |  7854 | `			SySetPut(&aArg,(const void *)&pValue);` |
|        6 |  7855 | `		}` |
|        - |  7856 | `		/* Point to the next entry */` |
|       13 |  7857 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        7 |  7858 | `	}` |
|        - |  7859 | `	/* Try to invoke the callback */` |
|       11 |  7860 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|       11 |  7861 | `	if( rc != SXRET_OK ){` |
|        - |  7862 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|      ! 0 |  7863 | `		ph7_result_bool(pCtx,0); /* return false */` |
|      ! 0 |  7864 | `	}else{` |
|        - |  7865 | `		/* Callback result */` |
|       11 |  7866 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        - |  7867 | `	}` |
|        - |  7868 | `	/* Cleanup the mess left behind */` |
|       11 |  7869 | `	PH7_MemObjRelease(&sResult);` |
|       11 |  7870 | `	SySetRelease(&aArg);` |
|       11 |  7871 | `	return PH7_OK;` |
|        6 |  7872 |  |
|        - |  7873 | `/*` |
|        - |  7874 | ` * bool defined(string $name)` |
|        - |  7875 | ` *  Checks whether a given named constant exists.` |
|        - |  7876 | ` * Parameter:` |
|        - |  7877 | ` *  Name of the desired constant.` |
|        - |  7878 | ` * Return` |
|        - |  7879 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  7880 | ` */` |
|       14 |  7881 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7882 |  |
|        - |  7883 | `	const char *zName;` |
|       16 |  7884 | `	int nLen = 0;` |
|       16 |  7885 | `	int res = 0;` |
|       16 |  7886 | `	if( nArg < 1 ){` |
|        - |  7887 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  7888 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  7889 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7890 | `		return SXRET_OK;` |
|        - |  7891 | `	}` |
|        - |  7892 | `	/* Extract constant name */` |
|       16 |  7893 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7894 | `	/* Perform the lookup */` |
|       16 |  7895 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7896 | `		/* Already defined */` |
|       10 |  7897 | `		res = 1;` |
|        4 |  7898 | `	}` |
|       16 |  7899 | `	ph7_result_bool(pCtx,res);` |
|       16 |  7900 | `	return SXRET_OK;` |
|        9 |  7901 |  |
|        - |  7902 | `/*` |
|        - |  7903 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  7904 | ` * below.` |
|        - |  7905 | ` */` |
|        8 |  7906 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  7907 |  |
|       10 |  7908 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7909 | `	/* Expand constant value */` |
|       10 |  7910 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7911 |  |
|        - |  7912 | `/*` |
|        - |  7913 | ` * bool define(string $constant_name,expression value)` |
|        - |  7914 | ` *  Defines a named constant at runtime.` |
|        - |  7915 | ` * Parameter:` |
|        - |  7916 | ` *  $constant_name` |
|        - |  7917 | ` *   The name of the constant` |
|        - |  7918 | ` *  $value` |
|        - |  7919 | ` *   Constant value` |
|        - |  7920 | ` * Return:` |
|        - |  7921 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7922 | ` */` |
|       10 |  7923 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7924 |  |
|        - |  7925 | `	const char *zName;  /* Constant name */` |
|        - |  7926 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7927 | `	int nLen = 0;       /* Name length */` |
|        - |  7928 | `	sxi32 rc;` |
|       12 |  7929 | `	if( nArg < 2 ){` |
|        - |  7930 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7931 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7932 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7933 | `		return SXRET_OK;` |
|        - |  7934 | `	}` |
|       12 |  7935 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7936 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7937 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7938 | `		return SXRET_OK;` |
|        - |  7939 | `	}` |
|        - |  7940 | `	/* Extract constant name */` |
|       12 |  7941 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7942 | `	if( nLen < 1 ){` |
|      ! 0 |  7943 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7944 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7945 | `		return SXRET_OK;` |
|        - |  7946 | `	}` |
|        - |  7947 | `	/* Duplicate constant value */` |
|       12 |  7948 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7949 | `	if( pValue == 0 ){` |
|      ! 0 |  7950 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7951 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7952 | `		return SXRET_OK;` |
|        - |  7953 | `	}` |
|        - |  7954 | `	/* Initialize the memory object */` |
|       12 |  7955 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7956 | `	/* Register the constant */` |
|       12 |  7957 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7958 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7959 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7960 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7961 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7962 | `		return SXRET_OK;` |
|        - |  7963 | `	}` |
|        - |  7964 | `	/* Duplicate constant value */` |
|       12 |  7965 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7966 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7967 | `		/* Lower case the constant name */` |
|      ! 0 |  7968 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7969 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7970 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7971 | `				/* UTF-8 stream */` |
|      ! 0 |  7972 | `				zCur++;` |
|      ! 0 |  7973 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  7974 | `					zCur++;` |
|      ! 0 |  7975 | `				}` |
|      ! 0 |  7976 | `				continue;` |
|        - |  7977 | `			}` |
|      ! 0 |  7978 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  7979 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  7980 | `				zCur[0] = (char)c;` |
|      ! 0 |  7981 | `			}` |
|      ! 0 |  7982 | `			zCur++;` |
|      ! 0 |  7983 | `		}` |
|        - |  7984 | `		/* Finally,register the constant */` |
|      ! 0 |  7985 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  7986 | `	}` |
|        - |  7987 | `	/* All done,return TRUE */` |
|       12 |  7988 | `	ph7_result_bool(pCtx,1);` |
|       12 |  7989 | `	return SXRET_OK;` |
|        7 |  7990 |  |
|        - |  7991 | `/*` |
|        - |  7992 | ` * value constant(string $name)` |
|        - |  7993 | ` *  Returns the value of a constant` |
|        - |  7994 | ` * Parameter` |
|        - |  7995 | ` *  $name` |
|        - |  7996 | ` *    Name of the constant.` |
|        - |  7997 | ` * Return` |
|        - |  7998 | ` *  Constant value or NULL if not defined.` |
|        - |  7999 | ` */` |
|        8 |  8000 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8001 |  |
|        - |  8002 | `	SyHashEntry *pEntry;` |
|        - |  8003 | `	ph7_constant *pCons;` |
|        - |  8004 | `	const char *zName; /* Constant name */` |
|        - |  8005 | `	ph7_value sVal;    /* Constant value */` |
|        - |  8006 | `	int nLen;` |
|       10 |  8007 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  8008 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  8009 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  8010 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8011 | `		return SXRET_OK;` |
|        - |  8012 | `	}` |
|        - |  8013 | `	/* Extract the constant name */` |
|       10 |  8014 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8015 | `	/* Perform the query */` |
|       10 |  8016 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  8017 | `	if( pEntry == 0 ){` |
|        3 |  8018 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  8019 | `		ph7_result_null(pCtx);` |
|        3 |  8020 | `		return SXRET_OK;` |
|        - |  8021 | `	}` |
|        8 |  8022 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  8023 | `	/* Point to the structure that describe the constant */` |
|        8 |  8024 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  8025 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  8026 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  8027 | `	/* Return that value */` |
|        8 |  8028 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  8029 | `	/* Cleanup */` |
|        8 |  8030 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  8031 | `	return SXRET_OK;` |
|        6 |  8032 |  |
|        - |  8033 | `/*` |
|        - |  8034 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  8035 | ` * defined below.` |
|        - |  8036 | ` */` |
|      414 |  8037 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8038 |  |
|      415 |  8039 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8040 | `	ph7_value sName;` |
|        - |  8041 | `	sxi32 rc;` |
|        - |  8042 | `	/* Prepare the constant name for insertion */` |
|      415 |  8043 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      415 |  8044 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8045 | `	/* Perform the insertion */` |
|      415 |  8046 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      415 |  8047 | `	PH7_MemObjRelease(&sName);` |
|      415 |  8048 | `	return rc;` |
|        1 |  8049 |  |
|        - |  8050 | `/*` |
|        - |  8051 | ` * array get_defined_constants(void)` |
|        - |  8052 | ` *  Returns an associative array with the names of all defined` |
|        - |  8053 | ` *  constants.` |
|        - |  8054 | ` * Parameters` |
|        - |  8055 | ` *  NONE.` |
|        - |  8056 | ` * Returns` |
|        - |  8057 | ` *  Returns the names of all the constants currently defined.` |
|        - |  8058 | ` */` |
|        2 |  8059 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8060 |  |
|        - |  8061 | `	ph7_value *pArray;` |
|        - |  8062 | `	/* Create the array first*/` |
|        3 |  8063 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8064 | `	if( pArray == 0 ){` |
|      ! 0 |  8065 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8066 | `		SXUNUSED(apArg);` |
|        - |  8067 | `		/* Return NULL */` |
|      ! 0 |  8068 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8069 | `		return SXRET_OK;` |
|        - |  8070 | `	}` |
|        - |  8071 | `	/* Fill the array with the defined constants */` |
|        3 |  8072 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  8073 | `	/* Return the created array */` |
|        3 |  8074 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8075 | `	return SXRET_OK;` |
|        2 |  8076 |  |
|        - |  8077 | `/*` |
|        - |  8078 | ` * Section:` |
|        - |  8079 | ` *  Output Control (OB) functions.` |
|        - |  8080 | ` * Status:` |
|        - |  8081 | ` *    Stable.` |
|        - |  8082 | ` */` |
|        - |  8083 | `/* Forward declaration */` |
|        - |  8084 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry);` |
|        - |  8085 | `/*` |
|        - |  8086 | ` * void ob_clean(void)` |
|        - |  8087 | ` *  This function discards the contents of the output buffer.` |
|        - |  8088 | ` *  This function does not destroy the output buffer like ob_end_clean() does.` |
|        - |  8089 | ` * Parameter` |
|        - |  8090 | ` *  None` |
|        - |  8091 | ` * Return` |
|        - |  8092 | ` *  No value is returned.` |
|        - |  8093 | ` */` |
|        2 |  8094 | `static int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8095 |  |
|        3 |  8096 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8097 | `	VmObEntry *pOb;` |
|        1 |  8098 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8099 | `	SXUNUSED(apArg);` |
|        - |  8100 | `	/* Peek the top most OB */` |
|        3 |  8101 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8102 | `	if( pOb ){` |
|        3 |  8103 | `		SyBlobRelease(&pOb->sOB);` |
|        1 |  8104 | `	}` |
|        3 |  8105 | `	return PH7_OK;` |
|        1 |  8106 |  |
|        - |  8107 | `/*` |
|        - |  8108 | ` * bool ob_end_clean(void)` |
|        - |  8109 | ` *  Clean (erase) the output buffer and turn off output buffering` |
|        - |  8110 | ` *  This function discards the contents of the topmost output buffer and turns` |
|        - |  8111 | ` *  off this output buffering. If you want to further process the buffer's contents` |
|        - |  8112 | ` *  you have to call ob_get_contents() before ob_end_clean() as the buffer contents` |
|        - |  8113 | ` *  are discarded when ob_end_clean() is called.` |
|        - |  8114 | ` * Parameter` |
|        - |  8115 | ` *  None` |
|        - |  8116 | ` * Return` |
|        - |  8117 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first that you called` |
|        - |  8118 | ` *  the function without an active buffer or that for some reason a buffer could not be deleted` |
|        - |  8119 | ` * (possible for special buffer)` |
|        - |  8120 | ` */` |
|     3140 |  8121 | `static int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8122 |  |
|     3142 |  8123 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8124 | `	VmObEntry *pOb;` |
|        - |  8125 | `	/* Pop the top most OB */` |
|     3142 |  8126 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     3142 |  8127 | `	if( pOb == 0){` |
|        - |  8128 | `		/* No such OB,return FALSE */` |
|      ! 0 |  8129 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8130 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8131 | `		SXUNUSED(apArg);` |
|      ! 0 |  8132 | `	}else{` |
|        - |  8133 | `		/* Release */` |
|     3142 |  8134 | `		VmObRestore(pVm,pOb);` |
|        - |  8135 | `		/* Return true */` |
|     3142 |  8136 | `		ph7_result_bool(pCtx,1);` |
|        - |  8137 | `	}` |
|     3142 |  8138 | `	return PH7_OK;` |
|        2 |  8139 |  |
|        - |  8140 | `/*` |
|        - |  8141 | ` * string ob_get_contents(void)` |
|        - |  8142 | ` *  Gets the contents of the output buffer without clearing it.` |
|        - |  8143 | ` * Parameter` |
|        - |  8144 | ` *  None` |
|        - |  8145 | ` * Return` |
|        - |  8146 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|        - |  8147 | ` */` |
|        6 |  8148 | `static int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8149 |  |
|        7 |  8150 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8151 | `	VmObEntry *pOb;` |
|        - |  8152 | `	/* Peek the top most OB */` |
|        7 |  8153 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        7 |  8154 | `	if( pOb == 0 ){` |
|        - |  8155 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8156 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8157 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8158 | `		SXUNUSED(apArg);` |
|      ! 0 |  8159 | `	}else{` |
|        - |  8160 | `		/* Return contents */` |
|        7 |  8161 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB));` |
|        - |  8162 | `	}` |
|        7 |  8163 | `	return PH7_OK;` |
|        1 |  8164 |  |
|        - |  8165 | `/*` |
|        - |  8166 | ` * string ob_get_clean(void)` |
|        - |  8167 | ` * string ob_get_flush(void)` |
|        - |  8168 | ` *  Get current buffer contents and delete current output buffer.` |
|        - |  8169 | ` * Parameter` |
|        - |  8170 | ` *  None` |
|        - |  8171 | ` * Return` |
|        - |  8172 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|        - |  8173 | ` */` |
|     4346 |  8174 | `static int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8175 |  |
|     4348 |  8176 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8177 | `	VmObEntry *pOb;` |
|        - |  8178 | `	/* Pop the top most OB */` |
|     4348 |  8179 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     4348 |  8180 | `	if( pOb == 0 ){` |
|        - |  8181 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8182 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8183 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8184 | `		SXUNUSED(apArg);` |
|      ! 0 |  8185 | `	}else{` |
|        - |  8186 | `		/* Return contents */` |
|     4348 |  8187 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB)); /* Will make it's own copy */` |
|        - |  8188 | `		/* Release */` |
|     4348 |  8189 | `		VmObRestore(pVm,pOb);` |
|        - |  8190 | `	}` |
|     4348 |  8191 | `	return PH7_OK;` |
|        2 |  8192 |  |
|        - |  8193 | `/*` |
|        - |  8194 | ` * int ob_get_length(void)` |
|        - |  8195 | ` *  Return the length of the output buffer.` |
|        - |  8196 | ` * Parameter` |
|        - |  8197 | ` *  None` |
|        - |  8198 | ` * Return` |
|        - |  8199 | ` *  Returns the length of the output buffer contents or FALSE if no buffering is active.` |
|        - |  8200 | ` */` |
|        2 |  8201 | `static int vm_builtin_ob_get_length(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8202 |  |
|        3 |  8203 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8204 | `	VmObEntry *pOb;` |
|        - |  8205 | `	/* Peek the top most OB */` |
|        3 |  8206 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8207 | `	if( pOb == 0 ){` |
|        - |  8208 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8209 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8210 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8211 | `		SXUNUSED(apArg);` |
|      ! 0 |  8212 | `	}else{` |
|        - |  8213 | `		/* Return OB length */` |
|        3 |  8214 | `		ph7_result_int64(pCtx,(ph7_int64)SyBlobLength(&pOb->sOB));` |
|        - |  8215 | `	}` |
|        3 |  8216 | `	return PH7_OK;` |
|        1 |  8217 |  |
|        - |  8218 | `/*` |
|        - |  8219 | ` * int ob_get_level(void)` |
|        - |  8220 | ` *  Returns the nesting level of the output buffering mechanism.` |
|        - |  8221 | ` * Parameter` |
|        - |  8222 | ` *  None` |
|        - |  8223 | ` * Return` |
|        - |  8224 | ` *  Returns the level of nested output buffering handlers or zero if output buffering is not active.` |
|        - |  8225 | ` */` |
|        6 |  8226 | `static int vm_builtin_ob_get_level(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8227 |  |
|        7 |  8228 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8229 | `	int iNest;` |
|        3 |  8230 | `	SXUNUSED(nArg); /* cc warning */` |
|        3 |  8231 | `	SXUNUSED(apArg);` |
|        - |  8232 | `	/* Nesting level */` |
|        7 |  8233 | `	iNest = (int)SySetUsed(&pVm->aOB);` |
|        - |  8234 | `	/* Return the nesting value */` |
|        7 |  8235 | `	ph7_result_int(pCtx,iNest);` |
|        7 |  8236 | `	return PH7_OK;` |
|        1 |  8237 |  |
|        - |  8238 | `/*` |
|        - |  8239 | ` * Output Buffer(OB) default VM consumer routine.All VM output is now redirected` |
|        - |  8240 | ` * to a stackable internal buffer,until the user call [ob_get_clean(),ob_end_clean(),...].` |
|        - |  8241 | ` * Refer to the implementation of [ob_start()] for more information.` |
|        - |  8242 | ` */` |
|     6690 |  8243 | `static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData)` |
|        2 |  8244 |  |
|     6692 |  8245 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|        - |  8246 | `	VmObEntry *pEntry;` |
|        - |  8247 | `	ph7_value sResult;` |
|        - |  8248 | `	/* Peek the top most entry */` |
|     6692 |  8249 | `	pEntry = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|     6692 |  8250 | `	if( pEntry == 0 ){` |
|        - |  8251 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8252 | `		return PH7_OK;` |
|        - |  8253 | `	}` |
|     6692 |  8254 | `	PH7_MemObjInit(pVm,&sResult);` |
|     6692 |  8255 | `	if( ph7_value_is_callable(&pEntry->sCallback) && pVm->nObDepth < 15 ){` |
|        - |  8256 | `		ph7_value sArg,*apArg[2];` |
|        - |  8257 | `		/* Fill the first argument */` |
|      ! 0 |  8258 | `		PH7_MemObjInitFromString(pVm,&sArg,0);` |
|      ! 0 |  8259 | `		PH7_MemObjStringAppend(&sArg,(const char *)pData,nDataLen);` |
|      ! 0 |  8260 | `		apArg[0] = &sArg;` |
|        - |  8261 | `		/* Call the 'filter' callback */` |
|      ! 0 |  8262 | `		pVm->nObDepth++;` |
|      ! 0 |  8263 | `		PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult);` |
|      ! 0 |  8264 | `		pVm->nObDepth--;` |
|      ! 0 |  8265 | `		if( sResult.iFlags & MEMOBJ_STRING ){` |
|        - |  8266 | `			/* Extract the function result */` |
|      ! 0 |  8267 | `			pData = SyBlobData(&sResult.sBlob);` |
|      ! 0 |  8268 | `			nDataLen = SyBlobLength(&sResult.sBlob);` |
|      ! 0 |  8269 | `		}` |
|      ! 0 |  8270 | `		PH7_MemObjRelease(&sArg);` |
|      ! 0 |  8271 | `	}` |
|     6692 |  8272 | `	if( nDataLen > 0 ){` |
|        - |  8273 | `		/* Redirect the VM output to the internal buffer */` |
|     6692 |  8274 | `		SyBlobAppend(&pEntry->sOB,pData,nDataLen);` |
|     3345 |  8275 | `	}` |
|        - |  8276 | `	/* Release */` |
|     6692 |  8277 | `	PH7_MemObjRelease(&sResult);` |
|     6692 |  8278 | `	return PH7_OK;` |
|     3347 |  8279 |  |
|        - |  8280 | `/*` |
|        - |  8281 | ` * Restore the default consumer.` |
|        - |  8282 | ` * Refer to the implementation of [ob_end_clean()] for more` |
|        - |  8283 | ` * information.` |
|        - |  8284 | ` */` |
|     7488 |  8285 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry)` |
|        2 |  8286 |  |
|     7490 |  8287 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     7490 |  8288 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|        - |  8289 | `		/* No more stackable OB */` |
|     7472 |  8290 | `		pCons->xConsumer = pCons->xDef;` |
|     7472 |  8291 | `		pCons->pUserData = pCons->pDefData;` |
|     3735 |  8292 | `	}` |
|        - |  8293 | `	/* Release OB data */` |
|     7490 |  8294 | `	PH7_MemObjRelease(&pEntry->sCallback);` |
|     7490 |  8295 | `	SyBlobRelease(&pEntry->sOB);` |
|     7490 |  8296 |  |
|        - |  8297 | `/*` |
|        - |  8298 | ` * bool ob_start([ callback $output_callback] )` |
|        - |  8299 | ` * This function will turn output buffering on. While output buffering is active no output` |
|        - |  8300 | ` *  is sent from the script (other than headers), instead the output is stored in an internal` |
|        - |  8301 | ` *  buffer.` |
|        - |  8302 | ` * Parameter` |
|        - |  8303 | ` *  $output_callback` |
|        - |  8304 | ` *   An optional output_callback function may be specified. This function takes a string` |
|        - |  8305 | ` *   as a parameter and should return a string. The function will be called when the output` |
|        - |  8306 | ` *   buffer is flushed (sent) or cleaned (with ob_flush(), ob_clean() or similar function)` |
|        - |  8307 | ` *   or when the output buffer is flushed to the browser at the end of the request.` |
|        - |  8308 | ` *   When output_callback is called, it will receive the contents of the output buffer` |
|        - |  8309 | ` *   as its parameter and is expected to return a new output buffer as a result, which will` |
|        - |  8310 | ` *   be sent to the browser. If the output_callback is not a callable function, this function` |
|        - |  8311 | ` *   will return FALSE.` |
|        - |  8312 | ` *   If the callback function has two parameters, the second parameter is filled with` |
|        - |  8313 | ` *   a bit-field consisting of PHP_OUTPUT_HANDLER_START, PHP_OUTPUT_HANDLER_CONT` |
|        - |  8314 | ` *   and PHP_OUTPUT_HANDLER_END.` |
|        - |  8315 | ` *   If output_callback returns FALSE original input is sent to the browser.` |
|        - |  8316 | ` *   The output_callback parameter may be bypassed by passing a NULL value.` |
|        - |  8317 | ` * Return` |
|        - |  8318 | ` *   Returns TRUE on success or FALSE on failure.` |
|        - |  8319 | ` */` |
|     7488 |  8320 | `static int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8321 |  |
|     7490 |  8322 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8323 | `	VmObEntry sOb;` |
|        - |  8324 | `	sxi32 rc;` |
|        - |  8325 | `	/* Initialize the OB entry */` |
|     7490 |  8326 | `	PH7_MemObjInit(pCtx->pVm,&sOb.sCallback);` |
|     7490 |  8327 | `	SyBlobInit(&sOb.sOB,&pVm->sAllocator);` |
|     7490 |  8328 | `	if( nArg > 0 && (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) ){` |
|        - |  8329 | `		/* Save the callback name for later invocation */` |
|      ! 0 |  8330 | `		PH7_MemObjStore(apArg[0],&sOb.sCallback);` |
|      ! 0 |  8331 | `	}` |
|        - |  8332 | `	/* Push in the stack */` |
|     7490 |  8333 | `	rc = SySetPut(&pVm->aOB,(const void *)&sOb);` |
|     7490 |  8334 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8335 | `		PH7_MemObjRelease(&sOb.sCallback);` |
|      ! 0 |  8336 | `	}else{` |
|     7490 |  8337 | `		ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        - |  8338 | `		/* Substitute the default VM consumer */` |
|     7490 |  8339 | `		if( pCons->xConsumer != VmObConsumer ){` |
|     7472 |  8340 | `			pCons->xDef = pCons->xConsumer;` |
|     7472 |  8341 | `			pCons->pDefData = pCons->pUserData;` |
|        - |  8342 | `			/* Install the new consumer */` |
|     7472 |  8343 | `			pCons->xConsumer = VmObConsumer;` |
|     7472 |  8344 | `			pCons->pUserData = pVm;` |
|     3735 |  8345 | `		}` |
|        - |  8346 | `	}` |
|     7490 |  8347 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     7490 |  8348 | `	return PH7_OK;` |
|        2 |  8349 |  |
|        - |  8350 | `/*` |
|        - |  8351 | ` * Flush Output buffer to the default VM output consumer.` |
|        - |  8352 | ` * Refer to the implementation of [ob_flush()] for more` |
|        - |  8353 | ` * information.` |
|        - |  8354 | ` */` |
|        4 |  8355 | `static sxi32 VmObFlush(ph7_vm *pVm,VmObEntry *pEntry,int bRelease)` |
|        1 |  8356 |  |
|        5 |  8357 | `	SyBlob *pBlob = &pEntry->sOB;` |
|        - |  8358 | `	sxi32 rc;` |
|        - |  8359 | `	/* Flush contents */` |
|        5 |  8360 | `	rc = PH7_OK;` |
|        5 |  8361 | `	if( SyBlobLength(pBlob) > 0 ){` |
|        - |  8362 | `		/* Call the VM output consumer */` |
|        5 |  8363 | `		rc = pVm->sVmConsumer.xDef(SyBlobData(pBlob),SyBlobLength(pBlob),pVm->sVmConsumer.pDefData);` |
|        - |  8364 | `		/* Increment VM output counter */` |
|        5 |  8365 | `		pVm->nOutputLen += SyBlobLength(pBlob);` |
|        5 |  8366 | `		if( rc != PH7_ABORT ){` |
|        5 |  8367 | `			rc = PH7_OK;` |
|        2 |  8368 | `		}` |
|        2 |  8369 | `	}` |
|        5 |  8370 | `	if( bRelease ){` |
|        3 |  8371 | `		VmObRestore(&(*pVm),pEntry);` |
|        2 |  8372 | `	}else{` |
|        - |  8373 | `		/* Reset the blob */` |
|        3 |  8374 | `		SyBlobReset(pBlob);` |
|        - |  8375 | `	}` |
|        5 |  8376 | `	return rc;` |
|        1 |  8377 |  |
|        - |  8378 | `/*` |
|        - |  8379 | ` * void ob_flush(void)` |
|        - |  8380 | ` * void flush(void)` |
|        - |  8381 | ` *  Flush (send) the output buffer.` |
|        - |  8382 | ` * Parameter` |
|        - |  8383 | ` *  None` |
|        - |  8384 | ` * Return` |
|        - |  8385 | ` *  No return value.` |
|        - |  8386 | ` */` |
|        2 |  8387 | `static int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8388 |  |
|        3 |  8389 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8390 | `	VmObEntry *pOb;` |
|        - |  8391 | `	sxi32 rc;` |
|        - |  8392 | `	/* Peek the top most OB entry */` |
|        3 |  8393 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8394 | `	if( pOb == 0 ){` |
|        - |  8395 | `		/* Empty stack,return immediately */` |
|      ! 0 |  8396 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8397 | `		SXUNUSED(apArg);` |
|      ! 0 |  8398 | `		return PH7_OK;` |
|        - |  8399 | `	}` |
|        - |  8400 | `	/* Flush contents */` |
|        3 |  8401 | `	rc = VmObFlush(pVm,pOb,FALSE);` |
|        3 |  8402 | `	return rc;` |
|        2 |  8403 |  |
|        - |  8404 | `/*` |
|        - |  8405 | ` * bool ob_end_flush(void)` |
|        - |  8406 | ` *  Flush (send) the output buffer and turn off output buffering.` |
|        - |  8407 | ` * Parameter` |
|        - |  8408 | ` *  None` |
|        - |  8409 | ` * Return` |
|        - |  8410 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first` |
|        - |  8411 | ` *  that you called the function without an active buffer or that for some reason` |
|        - |  8412 | ` *  a buffer could not be deleted (possible for special buffer).` |
|        - |  8413 | ` */` |
|        2 |  8414 | `static int vm_builtin_ob_end_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8415 |  |
|        3 |  8416 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8417 | `	VmObEntry *pOb;` |
|        - |  8418 | `	sxi32 rc;` |
|        - |  8419 | `	/* Pop the top most OB entry */` |
|        3 |  8420 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|        3 |  8421 | `	if( pOb == 0 ){` |
|        - |  8422 | `		/* Empty stack,return FALSE */` |
|      ! 0 |  8423 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8424 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8425 | `		SXUNUSED(apArg);` |
|      ! 0 |  8426 | `		return PH7_OK;` |
|        - |  8427 | `	}` |
|        - |  8428 | `	/* Flush contents */` |
|        3 |  8429 | `	rc = VmObFlush(pVm,pOb,TRUE);` |
|        - |  8430 | `	/* Return true */` |
|        3 |  8431 | `	ph7_result_bool(pCtx,1);` |
|        3 |  8432 | `	return rc;` |
|        2 |  8433 |  |
|        - |  8434 | `/*` |
|        - |  8435 | ` * void ob_implicit_flush([int $flag = true ])` |
|        - |  8436 | ` *  ob_implicit_flush() will turn implicit flushing on or off.` |
|        - |  8437 | ` *  Implicit flushing will result in a flush operation after every` |
|        - |  8438 | ` *  output call, so that explicit calls to flush() will no longer be needed.` |
|        - |  8439 | ` * Parameter` |
|        - |  8440 | ` *  $flag` |
|        - |  8441 | ` *   TRUE to turn implicit flushing on, FALSE otherwise.` |
|        - |  8442 | ` * Return` |
|        - |  8443 | ` *   Nothing` |
|        - |  8444 | ` */` |
|        4 |  8445 | `static int vm_builtin_ob_implicit_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8446 |  |
|        - |  8447 | `	/* NOTE: As of this version,this function is a no-op.` |
|        - |  8448 | `	 * PH7 is smart enough to flush it's internal buffer when appropriate.` |
|        - |  8449 | `	 */` |
|        2 |  8450 | `	SXUNUSED(pCtx);` |
|        2 |  8451 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8452 | `	SXUNUSED(apArg);` |
|        5 |  8453 | `	return PH7_OK;` |
|        1 |  8454 |  |
|        - |  8455 | `/*` |
|        - |  8456 | ` * array ob_list_handlers(void)` |
|        - |  8457 | ` *  Lists all output handlers in use.` |
|        - |  8458 | ` * Parameter` |
|        - |  8459 | ` *  None` |
|        - |  8460 | ` * Return` |
|        - |  8461 | ` *  This will return an array with the output handlers in use (if any).` |
|        - |  8462 | ` */` |
|        2 |  8463 | `static int vm_builtin_ob_list_handlers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8464 |  |
|        3 |  8465 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8466 | `	ph7_value *pArray;` |
|        - |  8467 | `	VmObEntry *aEntry;` |
|        - |  8468 | `	ph7_value sVal;` |
|        - |  8469 | `	sxu32 n;` |
|        3 |  8470 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|        - |  8471 | `		/* Empty stack,return null */` |
|      ! 0 |  8472 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8473 | `		return PH7_OK;` |
|        - |  8474 | `	}` |
|        - |  8475 | `	/* Create a new array */` |
|        3 |  8476 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8477 | `	if( pArray == 0 ){` |
|        - |  8478 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8479 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8480 | `		SXUNUSED(apArg);` |
|      ! 0 |  8481 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8482 | `		return PH7_OK;` |
|        - |  8483 | `	}` |
|        3 |  8484 | `	PH7_MemObjInit(pVm,&sVal);` |
|        - |  8485 | `	/* Point to the installed OB entries */` |
|        3 |  8486 | `	aEntry = (VmObEntry *)SySetBasePtr(&pVm->aOB);` |
|        - |  8487 | `	/* Perform the requested operation */` |
|        5 |  8488 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; n++ ){` |
|        3 |  8489 | `		VmObEntry *pEntry = &aEntry[n];` |
|        - |  8490 | `		/* Extract handler name */` |
|        3 |  8491 | `		SyBlobReset(&sVal.sBlob);` |
|        3 |  8492 | `		if( pEntry->sCallback.iFlags & MEMOBJ_STRING ){` |
|        - |  8493 | `			/* Callback,dup it's name */` |
|      ! 0 |  8494 | `			SyBlobDup(&pEntry->sCallback.sBlob,&sVal.sBlob);` |
|        3 |  8495 | `		}else if( pEntry->sCallback.iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  8496 | `			SyBlobAppend(&sVal.sBlob,"Class Method",sizeof("Class Method")-1);` |
|      ! 0 |  8497 | `		}else{` |
|        3 |  8498 | `			SyBlobAppend(&sVal.sBlob,"default output handler",sizeof("default output handler")-1);` |
|        - |  8499 | `		}` |
|        3 |  8500 | `		sVal.iFlags = MEMOBJ_STRING;` |
|        - |  8501 | `		/* Perform the insertion */` |
|        3 |  8502 | `		ph7_array_add_elem(pArray,0/* Automatic index assign */,&sVal /* Will make it's own copy */);` |
|        2 |  8503 | `	}` |
|        3 |  8504 | `	PH7_MemObjRelease(&sVal);` |
|        - |  8505 | `	/* Return the freshly created array */` |
|        3 |  8506 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8507 | `	return PH7_OK;` |
|        2 |  8508 |  |
|        - |  8509 | `/*` |
|        - |  8510 | ` * Section:` |
|        - |  8511 | ` *  Random numbers/string generators.` |
|        - |  8512 | ` * Status:` |
|        - |  8513 | ` *    Stable.` |
|        - |  8514 | ` */` |
|        - |  8515 | `/*` |
|        - |  8516 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  8517 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  8518 | ` * used by te SQLite3 library.` |
|        - |  8519 | ` */` |
|     1744 |  8520 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  8521 |  |
|        - |  8522 | `	sxu32 iNum;` |
|     1746 |  8523 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     1746 |  8524 | `	return iNum;` |
|        2 |  8525 |  |
|        - |  8526 | `/*` |
|        - |  8527 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  8528 | ` * Note that the generated string is NOT null terminated.` |
|        - |  8529 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  8530 | ` * by te SQLite3 library.` |
|        - |  8531 | ` */` |
|    55744 |  8532 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  8533 |  |
|        - |  8534 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  8535 | `	int i;` |
|        - |  8536 | `	/* Generate a binary string first */` |
|    55746 |  8537 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  8538 | `	/* Turn the binary string into english based alphabet */` |
|   613354 |  8539 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   557610 |  8540 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   278806 |  8541 | `	 }` |
|    55746 |  8542 |  |
|        - |  8543 | `/*` |
|        - |  8544 | ` * int rand()` |
|        - |  8545 | ` * int mt_rand()` |
|        - |  8546 | ` * int rand(int $min,int $max)` |
|        - |  8547 | ` * int mt_rand(int $min,int $max)` |
|        - |  8548 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  8549 | ` * Parameter` |
|        - |  8550 | ` *  $min` |
|        - |  8551 | ` *    The lowest value to return (default: 0)` |
|        - |  8552 | ` *  $max` |
|        - |  8553 | ` *   The highest value to return (default: getrandmax())` |
|        - |  8554 | ` * Return` |
|        - |  8555 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  8556 | ` * Note:` |
|        - |  8557 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8558 | ` *  by te SQLite3 library.` |
|        - |  8559 | ` */` |
|       20 |  8560 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8561 |  |
|        - |  8562 | `	sxu32 iNum;` |
|        - |  8563 | `	/* Generate the random number */` |
|       21 |  8564 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  8565 | `	if( nArg > 1 ){` |
|        - |  8566 | `		sxu32 iMin,iMax;` |
|        3 |  8567 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  8568 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  8569 | `		if( iMin < iMax ){` |
|        3 |  8570 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  8571 | `			if( iDiv > 0 ){` |
|        3 |  8572 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  8573 | `			}` |
|        1 |  8574 | `		}else if(iMax > 0 ){` |
|      ! 0 |  8575 | `			iNum %= iMax;` |
|      ! 0 |  8576 | `		}` |
|        1 |  8577 | `	}` |
|        - |  8578 | `	/* Return the number */` |
|       21 |  8579 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  8580 | `	return SXRET_OK;` |
|        1 |  8581 |  |
|        - |  8582 | `/*` |
|        - |  8583 | ` * int getrandmax(void)` |
|        - |  8584 | ` * int mt_getrandmax(void)` |
|        - |  8585 | ` * int rc4_getrandmax(void)` |
|        - |  8586 | ` *   Show largest possible random value` |
|        - |  8587 | ` * Return` |
|        - |  8588 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  8589 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  8590 | ` * Note:` |
|        - |  8591 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8592 | ` *  by te SQLite3 library.` |
|        - |  8593 | ` */` |
|        4 |  8594 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8595 |  |
|        2 |  8596 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8597 | `	SXUNUSED(apArg);` |
|        5 |  8598 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  8599 | `	return SXRET_OK;` |
|        1 |  8600 |  |
|        - |  8601 | `/*` |
|        - |  8602 | ` * string rand_str()` |
|        - |  8603 | ` * string rand_str(int $len)` |
|        - |  8604 | ` *  Generate a random string (English alphabet).` |
|        - |  8605 | ` * Parameter` |
|        - |  8606 | ` *  $len` |
|        - |  8607 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  8608 | ` * Return` |
|        - |  8609 | ` *   A pseudo random string.` |
|        - |  8610 | ` * Note:` |
|        - |  8611 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8612 | ` *  by te SQLite3 library.` |
|        - |  8613 | ` *  This function is a symisc extension.` |
|        - |  8614 | ` */` |
|      120 |  8615 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8616 |  |
|        - |  8617 | `	char zString[1024];` |
|      122 |  8618 | `	int iLen = 0x10;` |
|      122 |  8619 | `	if( nArg > 0 ){` |
|        - |  8620 | `		/* Get the desired length */` |
|      122 |  8621 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  8622 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  8623 | `			/* Default length */` |
|        3 |  8624 | `			iLen = 0x10;` |
|        1 |  8625 | `		}` |
|       60 |  8626 | `	}` |
|        - |  8627 | `	/* Generate the random string */` |
|      122 |  8628 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  8629 | `	/* Return the generated string */` |
|      122 |  8630 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  8631 | `	return SXRET_OK;` |
|        2 |  8632 |  |
|        - |  8633 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  8634 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  8635 | `/* Unique ID private data */` |
|        - |  8636 | `struct unique_id_data` |
|        - |  8637 |  |
|        - |  8638 | `	ph7_context *pCtx; /* Call context */` |
|        - |  8639 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  8640 | `};` |
|        - |  8641 | `/*` |
|        - |  8642 | ` * Binary to hex consumer callback.` |
|        - |  8643 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  8644 | ` * defined below.` |
|        - |  8645 | ` */` |
|      192 |  8646 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  8647 |  |
|      193 |  8648 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  8649 | `	sxu32 nBuflen;` |
|        - |  8650 | `	/* Extract result buffer length */` |
|      193 |  8651 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  8652 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  8653 | `			/*` |
|        - |  8654 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  8655 | `			 * string will be 13 characters long` |
|        - |  8656 | `			 */` |
|       25 |  8657 | `		return SXERR_ABORT;` |
|        - |  8658 | `	}` |
|      169 |  8659 | `	if( nBuflen > 22 ){` |
|      ! 0 |  8660 | `		return SXERR_ABORT;` |
|        - |  8661 | `	}` |
|        - |  8662 | `	/* Safely Consume the hex stream */` |
|      169 |  8663 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  8664 | `	return SXRET_OK;` |
|       97 |  8665 |  |
|        - |  8666 | `/*` |
|        - |  8667 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  8668 | ` *  Generate a unique ID` |
|        - |  8669 | ` * Parameter` |
|        - |  8670 | ` * $prefix` |
|        - |  8671 | ` *  Append this prefix to the generated unique ID.` |
|        - |  8672 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  8673 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  8674 | ` * $more_entropy` |
|        - |  8675 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  8676 | ` *  that the result will be unique.` |
|        - |  8677 | ` * Return` |
|        - |  8678 | ` *  Returns the unique identifier, as a string.` |
|        - |  8679 | ` */` |
|       24 |  8680 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8681 |  |
|        - |  8682 | `	struct unique_id_data sUniq;` |
|        - |  8683 | `	unsigned char zDigest[20];` |
|       25 |  8684 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8685 | `	const char *zPrefix;` |
|        - |  8686 | `	SHA1Context sCtx;` |
|        - |  8687 | `	char zRandom[7];` |
|        - |  8688 | `	int nPrefix;` |
|        - |  8689 | `	int entropy;` |
|        - |  8690 | `	/* Generate a random string first */` |
|       25 |  8691 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  8692 | `	/* Initialize fields */` |
|       25 |  8693 | `	zPrefix = 0;` |
|       25 |  8694 | `	nPrefix = 0;` |
|       25 |  8695 | `	entropy = 0;` |
|       25 |  8696 | `	if( nArg > 0 ){` |
|        - |  8697 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  8698 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  8699 | `		if( nArg > 1 ){` |
|      ! 0 |  8700 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  8701 | `		}` |
|      ! 0 |  8702 | `	}` |
|       25 |  8703 | `	SHA1Init(&sCtx);` |
|        - |  8704 | `	/* Generate the random ID */` |
|       25 |  8705 | `	if( nPrefix > 0 ){` |
|      ! 0 |  8706 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  8707 | `	}` |
|        - |  8708 | `	/* Append the random ID */` |
|       25 |  8709 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  8710 | `	/* Append the random string */` |
|       25 |  8711 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  8712 | `	/* Increment the number */` |
|       25 |  8713 | `	pVm->unique_id++;` |
|       25 |  8714 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  8715 | `	/* Hexify the digest */` |
|       25 |  8716 | `	sUniq.pCtx = pCtx;` |
|       25 |  8717 | `	sUniq.entropy = entropy;` |
|       25 |  8718 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  8719 | `	/* All done */` |
|       25 |  8720 | `	return PH7_OK;` |
|        1 |  8721 |  |
|        - |  8722 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  8723 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  8724 | `/*` |
|        - |  8725 | ` * Section:` |
|        - |  8726 | ` *  Language construct implementation as foreign functions.` |
|        - |  8727 | ` * Status:` |
|        - |  8728 | ` *    Stable.` |
|        - |  8729 | ` */` |
|        - |  8730 | `/*` |
|        - |  8731 | ` * void echo($string...)` |
|        - |  8732 | ` *  Output one or more messages.` |
|        - |  8733 | ` * Parameters` |
|        - |  8734 | ` *  $string` |
|        - |  8735 | ` *   Message to output.` |
|        - |  8736 | ` * Return` |
|        - |  8737 | ` *  NULL.` |
|        - |  8738 | ` */` |
|      ! 0 |  8739 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  8740 |  |
|        - |  8741 | `	const char *zData;` |
|      ! 0 |  8742 | `	int nDataLen = 0;` |
|        - |  8743 | `	ph7_vm *pVm;` |
|        - |  8744 | `	int i,rc;` |
|        - |  8745 | `	/* Point to the target VM */` |
|      ! 0 |  8746 | `	pVm = pCtx->pVm;` |
|        - |  8747 | `	/* Output */` |
|      ! 0 |  8748 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  8749 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  8750 | `		if( nDataLen > 0 ){` |
|      ! 0 |  8751 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  8752 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  8753 | `				/* Increment output length */` |
|      ! 0 |  8754 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  8755 | `			}` |
|      ! 0 |  8756 | `			if( rc == SXERR_ABORT ){` |
|        - |  8757 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  8758 | `				return PH7_ABORT;` |
|        - |  8759 | `			}` |
|      ! 0 |  8760 | `		}` |
|      ! 0 |  8761 | `	}` |
|      ! 0 |  8762 | `	return SXRET_OK;` |
|      ! 0 |  8763 |  |
|        - |  8764 | `/*` |
|        - |  8765 | ` * int print($string...)` |
|        - |  8766 | ` *  Output one or more messages.` |
|        - |  8767 | ` * Parameters` |
|        - |  8768 | ` *  $string` |
|        - |  8769 | ` *   Message to output.` |
|        - |  8770 | ` * Return` |
|        - |  8771 | ` *  1 always.` |
|        - |  8772 | ` */` |
|        2 |  8773 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8774 |  |
|        - |  8775 | `	const char *zData;` |
|        3 |  8776 | `	int nDataLen = 0;` |
|        - |  8777 | `	ph7_vm *pVm;` |
|        - |  8778 | `	int i,rc;` |
|        - |  8779 | `	/* Point to the target VM */` |
|        3 |  8780 | `	pVm = pCtx->pVm;` |
|        - |  8781 | `	/* Output */` |
|        5 |  8782 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  8783 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  8784 | `		if( nDataLen > 0 ){` |
|        3 |  8785 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  8786 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  8787 | `				/* Increment output length */` |
|        3 |  8788 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  8789 | `			}` |
|        3 |  8790 | `			if( rc == SXERR_ABORT ){` |
|        - |  8791 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  8792 | `				return PH7_ABORT;` |
|        - |  8793 | `			}` |
|        1 |  8794 | `		}` |
|        2 |  8795 | `	}` |
|        - |  8796 | `	/* Return 1 */` |
|        3 |  8797 | `	ph7_result_int(pCtx,1);` |
|        3 |  8798 | `	return SXRET_OK;` |
|        2 |  8799 |  |
|        - |  8800 | `/*` |
|        - |  8801 | ` * void exit(string $msg)` |
|        - |  8802 | ` * void exit(int $status)` |
|        - |  8803 | ` * void die(string $ms)` |
|        - |  8804 | ` * void die(int $status)` |
|        - |  8805 | ` *   Output a message and terminate program execution.` |
|        - |  8806 | ` * Parameter` |
|        - |  8807 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  8808 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  8809 | ` *  and not printed` |
|        - |  8810 | ` * Return` |
|        - |  8811 | ` *  NULL` |
|        - |  8812 | ` */` |
|      ! 0 |  8813 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  8814 |  |
|      ! 0 |  8815 | `	if( nArg > 0 ){` |
|      ! 0 |  8816 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  8817 | `			const char *zData;` |
|      ! 0 |  8818 | `			int iLen = 0;` |
|        - |  8819 | `			/* Print exit message */` |
|      ! 0 |  8820 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  8821 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  8822 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  8823 | `			sxi32 iExitStatus;` |
|        - |  8824 | `			/* Record exit status code */` |
|      ! 0 |  8825 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  8826 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  8827 | `		}` |
|      ! 0 |  8828 | `	}` |
|        - |  8829 | `	/* Check if we are in an included file */` |
|      ! 0 |  8830 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  8831 | `		/* Exit the entire process */` |
|      ! 0 |  8832 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  8833 | `	}` |
|        - |  8834 | `	/* Abort processing immediately */` |
|      ! 0 |  8835 | `	return PH7_ABORT;` |
|      ! 0 |  8836 |  |
|        - |  8837 | `/*` |
|        - |  8838 | ` * bool isset($var,...)` |
|        - |  8839 | ` *  Finds out whether a variable is set.` |
|        - |  8840 | ` * Parameters` |
|        - |  8841 | ` *  One or more variable to check.` |
|        - |  8842 | ` * Return` |
|        - |  8843 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  8844 | ` */` |
|    65266 |  8845 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8846 |  |
|        - |  8847 | `	ph7_value *pObj;` |
|    65268 |  8848 | `	int res = 0;` |
|        - |  8849 | `	int i;` |
|    65268 |  8850 | `	if( nArg < 1 ){` |
|        - |  8851 | `		/* Missing arguments,return false */` |
|      ! 0 |  8852 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  8853 | `		return SXRET_OK;` |
|        - |  8854 | `	}` |
|        - |  8855 | `	/* Iterate over available arguments */` |
|    86580 |  8856 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    65268 |  8857 | `		pObj = apArg[i];` |
|    65268 |  8858 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    43576 |  8859 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  8860 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  8861 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  8862 | `			}` |
|    21787 |  8863 | `		}` |
|    65268 |  8864 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    65268 |  8865 | `		if( !res ){` |
|        - |  8866 | `			/* Variable not set,return FALSE */` |
|    43956 |  8867 | `			ph7_result_bool(pCtx,0);` |
|    43956 |  8868 | `			return SXRET_OK;` |
|        - |  8869 | `		}` |
|    10658 |  8870 | `	}` |
|        - |  8871 | `	/* All given variable are set,return TRUE */` |
|    21314 |  8872 | `	ph7_result_bool(pCtx,1);` |
|    21314 |  8873 | `	return SXRET_OK;` |
|    32635 |  8874 |  |
|        - |  8875 | `/*` |
|        - |  8876 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  8877 | ` * frame,the reference table and discard it's contents.` |
|        - |  8878 | ` * This function never fail and always return SXRET_OK.` |
|        - |  8879 | ` */` |
|  2914064 |  8880 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  8881 |  |
|        - |  8882 | `	ph7_value *pObj;` |
|        - |  8883 | `	VmRefObj *pRef;` |
|  2914066 |  8884 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2914066 |  8885 | `	if( pObj ){` |
|        - |  8886 | `		/* Release the object */` |
|  2914066 |  8887 | `		PH7_MemObjRelease(pObj);` |
|  1457032 |  8888 | `	}` |
|        - |  8889 | `	/* Remove old reference links */` |
|  2914066 |  8890 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2914066 |  8891 | `	if( pRef ){` |
|  2914046 |  8892 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  8893 | `		/* Unlink from the reference table */` |
|  2914046 |  8894 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2914046 |  8895 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  8896 | `			VmSlot sFree;` |
|        - |  8897 | `			/* Restore to the free list */` |
|  2914040 |  8898 | `			sFree.nIdx = nObjIdx;` |
|  2914040 |  8899 | `			sFree.pUserData = 0;` |
|  2914040 |  8900 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1457019 |  8901 | `		}` |
|  1457022 |  8902 | `	}` |
|  2914066 |  8903 | `	return SXRET_OK;` |
|        2 |  8904 |  |
|        - |  8905 | `/*` |
|        - |  8906 | ` * void unset($var,...)` |
|        - |  8907 | ` *   Unset one or more given variable.` |
|        - |  8908 | ` * Parameters` |
|        - |  8909 | ` *  One or more variable to unset.` |
|        - |  8910 | ` * Return` |
|        - |  8911 | ` *  Nothing.` |
|        - |  8912 | ` */` |
|     3164 |  8913 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8914 |  |
|        - |  8915 | `	ph7_value *pObj;` |
|        - |  8916 | `	ph7_vm *pVm;` |
|        - |  8917 | `	int i;` |
|        - |  8918 | `	/* Point to the target VM */` |
|     3166 |  8919 | `	pVm = pCtx->pVm;` |
|        - |  8920 | `	/* Iterate and unset */` |
|     9472 |  8921 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6308 |  8922 | `		pObj = apArg[i];` |
|     6308 |  8923 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      812 |  8924 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  8925 | `				/* Throw an error */` |
|      ! 0 |  8926 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  8927 | `			}` |
|      407 |  8928 | `		}else{` |
|     5497 |  8929 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  8930 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     5497 |  8931 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     5491 |  8932 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2745 |  8933 | `			}` |
|        - |  8934 | `		}` |
|     3155 |  8935 | `	}` |
|     3166 |  8936 | `	return SXRET_OK;` |
|        2 |  8937 |  |
|        - |  8938 | `/*` |
|        - |  8939 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  8940 | ` */` |
|      110 |  8941 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8942 |  |
|      111 |  8943 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  8944 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  8945 | `	ph7_value *pObj;` |
|        - |  8946 | `	sxu32 nIdx;` |
|        - |  8947 | `	/* Extract the memory object */` |
|      111 |  8948 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  8949 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  8950 | `	if( pObj ){` |
|      111 |  8951 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  8952 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  8953 | `				SyString sName;` |
|        - |  8954 | `				ph7_value sKey;` |
|        - |  8955 | `				/* Perform the insertion */` |
|      109 |  8956 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  8957 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  8958 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  8959 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  8960 | `			}` |
|       54 |  8961 | `		}` |
|       55 |  8962 | `	}` |
|      111 |  8963 | `	return SXRET_OK;` |
|        1 |  8964 |  |
|        - |  8965 | `/*` |
|        - |  8966 | ` * array get_defined_vars(void)` |
|        - |  8967 | ` *  Returns an array of all defined variables.` |
|        - |  8968 | ` * Parameter` |
|        - |  8969 | ` *  None` |
|        - |  8970 | ` * Return` |
|        - |  8971 | ` *  An array with all the variables defined in the current scope.` |
|        - |  8972 | ` */` |
|        2 |  8973 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8974 |  |
|        3 |  8975 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8976 | `	ph7_value *pArray;` |
|        - |  8977 | `	/* Create a new array */` |
|        3 |  8978 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8979 | ` 	if( pArray == 0 ){` |
|      ! 0 |  8980 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8981 | `		SXUNUSED(apArg);` |
|        - |  8982 | `		/* Return NULL */` |
|      ! 0 |  8983 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8984 | `		return SXRET_OK;` |
|        - |  8985 | `	}` |
|        - |  8986 | `	/* Superglobals first */` |
|        3 |  8987 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  8988 | `	/* Then variable defined in the current frame */` |
|        3 |  8989 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  8990 | `	/* Finally,return the created array */` |
|        3 |  8991 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8992 | `	return SXRET_OK;` |
|        2 |  8993 |  |
|        - |  8994 | `/*` |
|        - |  8995 | ` * bool gettype($var)` |
|        - |  8996 | ` *  Get the type of a variable` |
|        - |  8997 | ` * Parameters` |
|        - |  8998 | ` *   $var` |
|        - |  8999 | ` *    The variable being type checked.` |
|        - |  9000 | ` * Return` |
|        - |  9001 | ` *   String representation of the given variable type.` |
|        - |  9002 | ` */` |
|       30 |  9003 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9004 |  |
|       32 |  9005 | `	const char *zType = "Empty";` |
|       32 |  9006 | `	if( nArg > 0 ){` |
|       32 |  9007 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       15 |  9008 | `	}` |
|        - |  9009 | `	/* Return the variable type */` |
|       32 |  9010 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       32 |  9011 | `	return SXRET_OK;` |
|        2 |  9012 |  |
|        - |  9013 | `/*` |
|        - |  9014 | ` * string get_resource_type(resource $handle)` |
|        - |  9015 | ` *  This function gets the type of the given resource.` |
|        - |  9016 | ` * Parameters` |
|        - |  9017 | ` *  $handle` |
|        - |  9018 | ` *  The evaluated resource handle.` |
|        - |  9019 | ` * Return` |
|        - |  9020 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  9021 | ` *  representing its type. If the type is not identified by this function` |
|        - |  9022 | ` *  the return value will be the string Unknown.` |
|        - |  9023 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  9024 | ` *  is not a resource.` |
|        - |  9025 | ` */` |
|        2 |  9026 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9027 |  |
|        3 |  9028 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  9029 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  9030 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9031 | `		return PH7_OK;` |
|        - |  9032 | `	}` |
|        3 |  9033 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  9034 | `	return SXRET_OK;` |
|        2 |  9035 |  |
|        - |  9036 | `/*` |
|        - |  9037 | ` * void var_dump(expression,....)` |
|        - |  9038 | ` *   var_dump � Dumps information about a variable` |
|        - |  9039 | ` * Parameters` |
|        - |  9040 | ` *   One or more expression to dump.` |
|        - |  9041 | ` * Returns` |
|        - |  9042 | ` *  Nothing.` |
|        - |  9043 | ` */` |
|      220 |  9044 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9045 |  |
|        - |  9046 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  9047 | `	int i;` |
|      222 |  9048 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  9049 | `	/* Dump one or more expressions */` |
|      448 |  9050 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      228 |  9051 | `		ph7_value *pObj = apArg[i];` |
|        - |  9052 | `		/* Reset the working buffer */` |
|      228 |  9053 | `		SyBlobReset(&sDump);` |
|        - |  9054 | `		/* Dump the given expression */` |
|      228 |  9055 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  9056 | `		/* Output */` |
|      228 |  9057 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      228 |  9058 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      113 |  9059 | `		}` |
|      115 |  9060 | `	}` |
|        - |  9061 | `	/* Release the working buffer */` |
|      222 |  9062 | `	SyBlobRelease(&sDump);` |
|      222 |  9063 | `	return SXRET_OK;` |
|        2 |  9064 |  |
|        - |  9065 | `/*` |
|        - |  9066 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  9067 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  9068 | ` * Parameters` |
|        - |  9069 | ` *   expression: Expression to dump` |
|        - |  9070 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  9071 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  9072 | ` *            print_r() will return the information rather than print it.` |
|        - |  9073 | ` * Return` |
|        - |  9074 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  9075 | ` *  Otherwise, the return value is TRUE.` |
|        - |  9076 | ` */` |
|       16 |  9077 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9078 |  |
|       17 |  9079 | `	int ret_string = 0;` |
|        - |  9080 | `	SyBlob sDump;` |
|       17 |  9081 | `	if( nArg < 1 ){` |
|        - |  9082 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9083 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9084 | `		return SXRET_OK;` |
|        - |  9085 | `	}` |
|       17 |  9086 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  9087 | `	if ( nArg > 1 ){` |
|        - |  9088 | `		/* Where to redirect output */` |
|       11 |  9089 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  9090 | `	}` |
|        - |  9091 | `	/* Generate dump */` |
|       17 |  9092 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  9093 | `	if( !ret_string ){` |
|        - |  9094 | `		/* Output dump */` |
|        7 |  9095 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9096 | `		/* Return true */` |
|        7 |  9097 | `		ph7_result_bool(pCtx,1);` |
|        4 |  9098 | `	}else{` |
|        - |  9099 | `		/* Generated dump as return value */` |
|       11 |  9100 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9101 | `	}` |
|        - |  9102 | `	/* Release the working buffer */` |
|       17 |  9103 | `	SyBlobRelease(&sDump);` |
|       17 |  9104 | `	return SXRET_OK;` |
|        9 |  9105 |  |
|        - |  9106 | `/*` |
|        - |  9107 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9108 | ` * Same job as print_r. (see coment above)` |
|        - |  9109 | ` */` |
|        2 |  9110 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9111 |  |
|        3 |  9112 | `	int ret_string = 0;` |
|        - |  9113 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9114 | `	if( nArg < 1 ){` |
|        - |  9115 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9116 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9117 | `		return SXRET_OK;` |
|        - |  9118 | `	}` |
|        3 |  9119 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9120 | `	if ( nArg > 1 ){` |
|        - |  9121 | `		/* Where to redirect output */` |
|        3 |  9122 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9123 | `	}` |
|        - |  9124 | `	/* Generate dump */` |
|        3 |  9125 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9126 | `	if( !ret_string ){` |
|        - |  9127 | `		/* Output dump */` |
|      ! 0 |  9128 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9129 | `		/* Return NULL */` |
|      ! 0 |  9130 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9131 | `	}else{` |
|        - |  9132 | `		/* Generated dump as return value */` |
|        3 |  9133 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9134 | `	}` |
|        - |  9135 | `	/* Release the working buffer */` |
|        3 |  9136 | `	SyBlobRelease(&sDump);` |
|        3 |  9137 | `	return SXRET_OK;` |
|        2 |  9138 |  |
|        - |  9139 | `/*` |
|        - |  9140 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9141 | ` *  Set/get the various assert flags.` |
|        - |  9142 | ` * Parameter` |
|        - |  9143 | ` * $what` |
|        - |  9144 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9145 | ` *   ASSERT_WARNING         Issue a warning for each failed assertion` |
|        - |  9146 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9147 | ` *   ASSERT_QUIET_EVAL      Not used` |
|        - |  9148 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9149 | ` * $value` |
|        - |  9150 | ` *   An optional new value for the option.` |
|        - |  9151 | ` * Return` |
|        - |  9152 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9153 | ` */` |
|        8 |  9154 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9155 |  |
|        9 |  9156 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9157 | `	int iOld,iNew,iValue;` |
|        9 |  9158 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|        - |  9159 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  9160 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9161 | `		return PH7_OK;` |
|        - |  9162 | `	}` |
|        - |  9163 | `	/* Save old assertion flags */` |
|        9 |  9164 | `	iOld = pVm->iAssertFlags;` |
|        - |  9165 | `	/* Extract the new flags */` |
|        9 |  9166 | `	iNew = ph7_value_to_int(apArg[0]);` |
|        9 |  9167 | `	if( iNew == PH7_ASSERT_DISABLE ){` |
|        7 |  9168 | `		pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        7 |  9169 | `		if( nArg > 1 ){` |
|        5 |  9170 | `			iValue = !ph7_value_to_bool(apArg[1]);` |
|        5 |  9171 | `			if( iValue ){` |
|        - |  9172 | `				/* Disable assertion */` |
|        3 |  9173 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        1 |  9174 | `			}` |
|        3 |  9175 | `		}` |
|        6 |  9176 | `	}else if( iNew == PH7_ASSERT_WARNING ){` |
|      ! 0 |  9177 | `		pVm->iAssertFlags &= ~PH7_ASSERT_WARNING;` |
|      ! 0 |  9178 | `		if( nArg > 1 ){` |
|      ! 0 |  9179 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9180 | `			if( iValue ){` |
|        - |  9181 | `				/* Issue a warning for each failed assertion */` |
|      ! 0 |  9182 | `				pVm->iAssertFlags \|= PH7_ASSERT_WARNING;` |
|      ! 0 |  9183 | `			}` |
|      ! 0 |  9184 | `		}` |
|        3 |  9185 | `	}else if( iNew == PH7_ASSERT_BAIL ){` |
|        3 |  9186 | `		pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        3 |  9187 | `		if( nArg > 1 ){` |
|        3 |  9188 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|        3 |  9189 | `			if( iValue ){` |
|        - |  9190 | `				/* Terminate execution on failed assertions */` |
|        3 |  9191 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        1 |  9192 | `			}` |
|        2 |  9193 | `		}` |
|        1 |  9194 | `	}else if( iNew == PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  9195 | `		pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9196 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|        - |  9197 | `			/* Callback to call on failed assertions */` |
|      ! 0 |  9198 | `			PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  9199 | `			pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9200 | `		}` |
|      ! 0 |  9201 | `	}` |
|        - |  9202 | `	/* Return the old flags */` |
|        9 |  9203 | `	ph7_result_int(pCtx,iOld);` |
|        9 |  9204 | `	return PH7_OK;` |
|        5 |  9205 |  |
|        - |  9206 | `/*` |
|        - |  9207 | ` * bool assert(mixed $assertion)` |
|        - |  9208 | ` *  Checks if assertion is FALSE.` |
|        - |  9209 | ` * Parameter` |
|        - |  9210 | ` *  $assertion` |
|        - |  9211 | ` *    The assertion to test.` |
|        - |  9212 | ` * Return` |
|        - |  9213 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9214 | ` */` |
|       14 |  9215 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9216 |  |
|       15 |  9217 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9218 | `	ph7_value *pAssert;` |
|        - |  9219 | `	int iFlags,iResult;` |
|       15 |  9220 | `	if( nArg < 1 ){` |
|        - |  9221 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  9222 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9223 | `		return PH7_OK;` |
|        - |  9224 | `	}` |
|       15 |  9225 | `	iFlags = pVm->iAssertFlags;` |
|       15 |  9226 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  9227 | `		/* Assertion is disabled,return FALSE */` |
|      ! 0 |  9228 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9229 | `		return PH7_OK;` |
|        - |  9230 | `	}` |
|       15 |  9231 | `	pAssert = apArg[0];` |
|       15 |  9232 | `	iResult = 1; /* cc warning */` |
|       15 |  9233 | `	if( pAssert->iFlags & MEMOBJ_STRING ){` |
|        - |  9234 | `		SyString sChunk;` |
|        7 |  9235 | `		SyStringInitFromBuf(&sChunk,SyBlobData(&pAssert->sBlob),SyBlobLength(&pAssert->sBlob));` |
|        7 |  9236 | `		if( sChunk.nByte > 0 ){` |
|        5 |  9237 | `			VmEvalChunk(pVm,pCtx,&sChunk,PH7_PHP_ONLY\|PH7_PHP_EXPR,FALSE);` |
|        - |  9238 | `			/* Extract evaluation result */` |
|        5 |  9239 | `			iResult = ph7_value_to_bool(pCtx->pRet);` |
|        3 |  9240 | `		}else{` |
|        3 |  9241 | `			iResult = 0;` |
|        - |  9242 | `		}` |
|        4 |  9243 | `	}else{` |
|        - |  9244 | `		/* Perform a boolean cast */` |
|        9 |  9245 | `		iResult = ph7_value_to_bool(apArg[0]);` |
|        - |  9246 | `	}` |
|       15 |  9247 | `	if( !iResult ){` |
|        - |  9248 | `		/* Assertion failed */` |
|        9 |  9249 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  9250 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  9251 | `			ph7_value sFile,sLine;` |
|        - |  9252 | `			ph7_value *apCbArg[3];` |
|        - |  9253 | `			SyString *pFile;` |
|        - |  9254 | `			/* Extract the processed script */` |
|      ! 0 |  9255 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  9256 | `			if( pFile == 0 ){` |
|      ! 0 |  9257 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  9258 | `			}` |
|        - |  9259 | `			/* Invoke the callback */` |
|      ! 0 |  9260 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  9261 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  9262 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  9263 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  9264 | `			apCbArg[2] = pAssert;` |
|      ! 0 |  9265 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  9266 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  9267 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  9268 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  9269 | `		}` |
|        9 |  9270 | `		if( iFlags & PH7_ASSERT_WARNING ){` |
|        - |  9271 | `			/* Emit a warning */` |
|        9 |  9272 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Assertion failed");` |
|        4 |  9273 | `		}` |
|        9 |  9274 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  9275 | `			/* Abort VM execution immediately */` |
|        3 |  9276 | `			return PH7_ABORT;` |
|        - |  9277 | `		}` |
|        3 |  9278 | `	}` |
|        - |  9279 | `	/* Assertion result */` |
|       13 |  9280 | `	ph7_result_bool(pCtx,iResult);` |
|       13 |  9281 | `	return PH7_OK;` |
|        8 |  9282 |  |
|        - |  9283 | `/*` |
|        - |  9284 | ` * Section:` |
|        - |  9285 | ` *  Error reporting functions.` |
|        - |  9286 | ` * Status:` |
|        - |  9287 | ` *    Stable.` |
|        - |  9288 | ` */` |
|        - |  9289 | `/*` |
|        - |  9290 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  9291 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  9292 | ` * Parameters` |
|        - |  9293 | ` *  $error_msg` |
|        - |  9294 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  9295 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  9296 | ` * $error_type` |
|        - |  9297 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  9298 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  9299 | ` * Return` |
|        - |  9300 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  9301 | ` */` |
|       12 |  9302 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9303 |  |
|       14 |  9304 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  9305 | `	int rc = PH7_OK;` |
|       14 |  9306 | `	if( nArg > 0 ){` |
|        - |  9307 | `		const char *zErr;` |
|        - |  9308 | `		int nLen;` |
|        - |  9309 | `		/* Extract the error message */` |
|       12 |  9310 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  9311 | `		if( nArg > 1 ){` |
|        - |  9312 | `			/* Extract the error type */` |
|       12 |  9313 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  9314 | `			switch( nErr ){` |
|        1 |  9315 | `			case 1:   /* E_ERROR */` |
|        - |  9316 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  9317 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  9318 | `			case 256: /* E_USER_ERROR */` |
|        3 |  9319 | `				nErr = PH7_CTX_ERR;` |
|        3 |  9320 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  9321 | `				break;` |
|        1 |  9322 | `			case 2:   /* E_WARNING */` |
|        - |  9323 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  9324 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  9325 | `			case 512: /* E_USER_WARNING */` |
|        3 |  9326 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  9327 | `				break;` |
|        3 |  9328 | `			default:` |
|        8 |  9329 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  9330 | `				break;` |
|        - |  9331 | `			}` |
|        5 |  9332 | `		}` |
|        - |  9333 | `		/* Report error */` |
|       12 |  9334 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  9335 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  9336 | `			return rc;` |
|        - |  9337 | `		}` |
|        - |  9338 | `		/* Return true */` |
|       12 |  9339 | `		ph7_result_bool(pCtx,1);` |
|        7 |  9340 | `	}else{` |
|        - |  9341 | `		/* Missing arguments,return FALSE */` |
|        3 |  9342 | `		ph7_result_bool(pCtx,0);` |
|        - |  9343 | `	}` |
|       14 |  9344 | `	return rc;` |
|        8 |  9345 |  |
|        - |  9346 | `/*` |
|        - |  9347 | ` * int error_reporting([int $level])` |
|        - |  9348 | ` *  Sets which PHP errors are reported.` |
|        - |  9349 | ` * Parameters` |
|        - |  9350 | ` *  $level` |
|        - |  9351 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  9352 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  9353 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  9354 | ` *   levels will not always behave as expected.` |
|        - |  9355 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  9356 | ` *   in the predefined constants.` |
|        - |  9357 | ` * Return` |
|        - |  9358 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  9359 | ` *   parameter is given.` |
|        - |  9360 | ` */` |
|       18 |  9361 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9362 |  |
|       19 |  9363 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9364 | `	int nOld;` |
|        - |  9365 | `	/* Extract the old reporting level */` |
|       19 |  9366 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       19 |  9367 | `	if( nArg > 0 ){` |
|        - |  9368 | `		int nNew;` |
|        - |  9369 | `		/* Extract the desired error reporting level */` |
|       11 |  9370 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       11 |  9371 | `		if( !nNew ){` |
|        - |  9372 | `			/* Do not report errors at all */` |
|        5 |  9373 | `			pVm->bErrReport = 0;` |
|        3 |  9374 | `		}else{` |
|        - |  9375 | `			/* Report all errors */` |
|        7 |  9376 | `			pVm->bErrReport = 1;` |
|        - |  9377 | `		}` |
|        5 |  9378 | `	}` |
|        - |  9379 | `	/* Return the old level */` |
|       19 |  9380 | `	ph7_result_int(pCtx,nOld);` |
|       19 |  9381 | `	return PH7_OK;` |
|        1 |  9382 |  |
|        - |  9383 | `/*` |
|        - |  9384 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  9385 | ` *  Send an error message somewhere.` |
|        - |  9386 | ` * Parameter` |
|        - |  9387 | ` *  $message` |
|        - |  9388 | ` *   The error message that should be logged.` |
|        - |  9389 | ` *  $message_type` |
|        - |  9390 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  9391 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  9392 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  9393 | ` *       This is the default option.` |
|        - |  9394 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  9395 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  9396 | ` *    2  No longer an option.` |
|        - |  9397 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  9398 | ` *       to the end of the message string.` |
|        - |  9399 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  9400 | ` *  $destination` |
|        - |  9401 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  9402 | ` *  $extra_headers` |
|        - |  9403 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  9404 | ` * Return` |
|        - |  9405 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9406 | ` * NOTE:` |
|        - |  9407 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  9408 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  9409 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  9410 | ` *  Otherwise this function is no-op.` |
|        - |  9411 | ` */` |
|        4 |  9412 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9413 |  |
|        - |  9414 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  9415 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  9416 | `	int iType = 0;` |
|        5 |  9417 | `	if( nArg < 1 ){` |
|        - |  9418 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  9419 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9420 | `		return PH7_OK;` |
|        - |  9421 | `	}` |
|        5 |  9422 | `	if( pVm->xErrLog  ){` |
|        - |  9423 | `		/* Invoke the user callback */` |
|      ! 0 |  9424 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  9425 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  9426 | `		if( nArg > 1 ){` |
|      ! 0 |  9427 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  9428 | `			if( nArg > 2 ){` |
|      ! 0 |  9429 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  9430 | `				if( nArg > 3 ){` |
|      ! 0 |  9431 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  9432 | `				}` |
|      ! 0 |  9433 | `			}` |
|      ! 0 |  9434 | `		}` |
|      ! 0 |  9435 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  9436 | `	}` |
|        - |  9437 | `	/* Retun TRUE */` |
|        5 |  9438 | `	ph7_result_bool(pCtx,1);` |
|        5 |  9439 | `	return PH7_OK;` |
|        3 |  9440 |  |
|        - |  9441 | `/*` |
|        - |  9442 | ` * bool restore_exception_handler(void)` |
|        - |  9443 | ` *  Restores the previously defined exception handler function.` |
|        - |  9444 | ` * Parameter` |
|        - |  9445 | ` *  None` |
|        - |  9446 | ` * Return` |
|        - |  9447 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  9448 | ` */` |
|        4 |  9449 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9450 |  |
|        5 |  9451 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9452 | `	ph7_value *pOld,*pNew;` |
|        - |  9453 | `	/* Point to the old and the new handler */` |
|        5 |  9454 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  9455 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  9456 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9457 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9458 | `		SXUNUSED(apArg);` |
|        - |  9459 | `		/* No installed handler,return FALSE */` |
|        5 |  9460 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9461 | `		return PH7_OK;` |
|        - |  9462 | `	}` |
|        - |  9463 | `	/* Copy the old handler */` |
|      ! 0 |  9464 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9465 | `	PH7_MemObjRelease(pOld);` |
|        - |  9466 | `	/* Return TRUE */` |
|      ! 0 |  9467 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9468 | `	return PH7_OK;` |
|        3 |  9469 |  |
|        - |  9470 | `/*` |
|        - |  9471 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  9472 | ` *  Sets a user-defined exception handler function.` |
|        - |  9473 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  9474 | ` * NOTE` |
|        - |  9475 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  9476 | ` *  the satndard PHP engine.` |
|        - |  9477 | ` * Parameters` |
|        - |  9478 | ` *  $exception_handler` |
|        - |  9479 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  9480 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  9481 | ` *   that was thrown.` |
|        - |  9482 | ` *  Note:` |
|        - |  9483 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9484 | ` * Return` |
|        - |  9485 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  9486 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9487 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9488 | ` */` |
|        4 |  9489 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9490 |  |
|        6 |  9491 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9492 | `	ph7_value *pOld,*pNew;` |
|        - |  9493 | `	/* Point to the old and the new handler */` |
|        6 |  9494 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  9495 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  9496 | `	/* Return the old handler */` |
|        6 |  9497 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  9498 | `	if( nArg > 0 ){` |
|        6 |  9499 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9500 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  9501 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  9502 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  9503 | `		}else{` |
|        6 |  9504 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9505 | `			/* Install the new handler */` |
|        6 |  9506 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9507 | `		}` |
|        2 |  9508 | `	}` |
|        6 |  9509 | `	return PH7_OK;` |
|        2 |  9510 |  |
|        - |  9511 | `/*` |
|        - |  9512 | ` * bool restore_error_handler(void)` |
|        - |  9513 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9514 | ` * Parameters:` |
|        - |  9515 | ` *  None.` |
|        - |  9516 | ` * Return` |
|        - |  9517 | ` *  Always TRUE.` |
|        - |  9518 | ` */` |
|        4 |  9519 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9520 |  |
|        5 |  9521 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9522 | `	ph7_value *pOld,*pNew;` |
|        - |  9523 | `	/* Point to the old and the new handler */` |
|        5 |  9524 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  9525 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  9526 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9527 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9528 | `		SXUNUSED(apArg);` |
|        - |  9529 | `		/* No installed callback,return FALSE */` |
|        5 |  9530 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9531 | `		return PH7_OK;` |
|        - |  9532 | `	}` |
|        - |  9533 | `	/* Copy the old callback */` |
|      ! 0 |  9534 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9535 | `	PH7_MemObjRelease(pOld);` |
|        - |  9536 | `	/* Return TRUE */` |
|      ! 0 |  9537 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9538 | `	return PH7_OK;` |
|        3 |  9539 |  |
|        - |  9540 | `/*` |
|        - |  9541 | ` * value set_error_handler(callable $error_handler)` |
|        - |  9542 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9543 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9544 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9545 | ` *  Sets a user-defined error handler function.` |
|        - |  9546 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  9547 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  9548 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  9549 | ` *  conditions (using trigger_error()).` |
|        - |  9550 | ` * Parameters` |
|        - |  9551 | ` *  $error_handler` |
|        - |  9552 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  9553 | ` *   describing the error.` |
|        - |  9554 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  9555 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  9556 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  9557 | ` *   The function can be shown as:` |
|        - |  9558 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  9559 | ` *     errno` |
|        - |  9560 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  9561 | ` *   errstr` |
|        - |  9562 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  9563 | ` *   errfile` |
|        - |  9564 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  9565 | ` *     was raised in, as a string.` |
|        - |  9566 | ` *  Note:` |
|        - |  9567 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9568 | ` * Return` |
|        - |  9569 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  9570 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9571 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9572 | ` */` |
|     8670 |  9573 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9574 |  |
|     8672 |  9575 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9576 | `	ph7_value *pOld,*pNew;` |
|        - |  9577 | `	/* Point to the old and the new handler */` |
|     8672 |  9578 | `	pOld = &pVm->aErrCB[0];` |
|     8672 |  9579 | `	pNew = &pVm->aErrCB[1];` |
|        - |  9580 | `	/* Return the old handler */` |
|     8672 |  9581 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8672 |  9582 | `	if( nArg > 0 ){` |
|     8672 |  9583 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9584 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4335 |  9585 | `			PH7_MemObjRelease(pNew);` |
|     4335 |  9586 | `			ph7_result_bool(pCtx,1);` |
|     2168 |  9587 | `		}else{` |
|     4338 |  9588 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9589 | `			/* Install the new handler */` |
|     4338 |  9590 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9591 | `		}` |
|     4335 |  9592 | `	}` |
|     8672 |  9593 | `	return PH7_OK;` |
|        2 |  9594 |  |
|        - |  9595 | `/*` |
|        - |  9596 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  9597 | ` *  Generates a backtrace.` |
|        - |  9598 | ` * Paramaeter` |
|        - |  9599 | ` *  $options` |
|        - |  9600 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  9601 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  9602 | ` *   all the function/method arguments, to save memory.` |
|        - |  9603 | ` * $limit` |
|        - |  9604 | ` *   (Not Used)` |
|        - |  9605 | ` * Return` |
|        - |  9606 | ` *  An array.The possible returned elements are as follows:` |
|        - |  9607 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  9608 | ` *          Name        Type      Description` |
|        - |  9609 | ` *          ------      ------     -----------` |
|        - |  9610 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  9611 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  9612 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  9613 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  9614 | ` *          object      object    The current object.` |
|        - |  9615 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  9616 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  9617 | ` */` |
|      376 |  9618 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9619 |  |
|      378 |  9620 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9621 | `	ph7_value *pArray;` |
|        - |  9622 | `	ph7_class *pClass;` |
|        - |  9623 | `	ph7_value *pValue;` |
|        - |  9624 | `	SyString *pFile;` |
|        - |  9625 | `	/* Create a new array */` |
|      378 |  9626 | `	pArray = ph7_context_new_array(pCtx);` |
|      378 |  9627 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      378 |  9628 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9629 | `		/* Out of memory,return NULL */` |
|      ! 0 |  9630 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  9631 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9632 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9633 | `		SXUNUSED(apArg);` |
|      ! 0 |  9634 | `		return PH7_OK;` |
|        - |  9635 | `	}` |
|        - |  9636 | `	/* Dump running function name and it's arguments  */` |
|      378 |  9637 | `	if( pVm->pFrame->pParent ){` |
|      378 |  9638 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  9639 | `		ph7_vm_func *pFunc;` |
|        - |  9640 | `		ph7_value *pArg;` |
|      378 |  9641 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9642 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  9643 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  9644 | `		}` |
|      378 |  9645 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      378 |  9646 | `		if( pFrame->pParent && pFunc ){` |
|      378 |  9647 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      378 |  9648 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      378 |  9649 | `			ph7_value_reset_string_cursor(pValue);` |
|      188 |  9650 | `		}` |
|        - |  9651 | `		/* Function arguments */` |
|      378 |  9652 | `		pArg = ph7_context_new_array(pCtx);` |
|      378 |  9653 | `		if( pArg  ){` |
|        - |  9654 | `			ph7_value *pObj;` |
|        - |  9655 | `			VmSlot *aSlot;` |
|        - |  9656 | `			sxu32 n;` |
|        - |  9657 | `			/* Start filling the array with the given arguments */` |
|      378 |  9658 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     1498 |  9659 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1122 |  9660 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1122 |  9661 | `				if( pObj ){` |
|     1122 |  9662 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      560 |  9663 | `				}` |
|      562 |  9664 | `			}` |
|        - |  9665 | `			/* Save the array */` |
|      378 |  9666 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      188 |  9667 | `		}` |
|      188 |  9668 | `	}` |
|      378 |  9669 | `	ph7_value_int(pValue,1);` |
|        - |  9670 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  9671 | `	 * line numbers at run-time. )` |
|        - |  9672 | `	 */` |
|      378 |  9673 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  9674 | `	/* Current processed script */` |
|      378 |  9675 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      378 |  9676 | `	if( pFile ){` |
|      378 |  9677 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      378 |  9678 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      378 |  9679 | `		ph7_value_reset_string_cursor(pValue);` |
|      188 |  9680 | `	}` |
|        - |  9681 | `	/* Top class */` |
|      378 |  9682 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      378 |  9683 | `	if( pClass ){` |
|      374 |  9684 | `		ph7_value_reset_string_cursor(pValue);` |
|      374 |  9685 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      374 |  9686 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      186 |  9687 | `	}` |
|        - |  9688 | `	/* Return the freshly created array */` |
|      378 |  9689 | `	ph7_result_value(pCtx,pArray);` |
|        - |  9690 | `	/*` |
|        - |  9691 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  9692 | `	 * as soon we return from this function.` |
|        - |  9693 | `	 */` |
|      378 |  9694 | `	return PH7_OK;` |
|      190 |  9695 |  |
|        - |  9696 | `/*` |
|        - |  9697 | ` * Generate a small backtrace.` |
|        - |  9698 | ` * Store the generated dump in the given BLOB` |
|        - |  9699 | ` */` |
|        4 |  9700 | `static int VmMiniBacktrace(` |
|        - |  9701 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  9702 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  9703 | `	)` |
|        1 |  9704 |  |
|        5 |  9705 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  9706 | `	ph7_vm_func *pFunc;` |
|        - |  9707 | `	ph7_class *pClass;` |
|        - |  9708 | `	SyString *pFile;` |
|        - |  9709 | `	/* Called function */` |
|        5 |  9710 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9711 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  9712 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  9713 | `	}` |
|        5 |  9714 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  9715 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  9716 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  9717 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  9718 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  9719 | `	}else{` |
|      ! 0 |  9720 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  9721 | `	}` |
|        5 |  9722 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  9723 | `	/* Current processed script */` |
|        5 |  9724 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  9725 | `	if( pFile ){` |
|        5 |  9726 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  9727 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  9728 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  9729 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  9730 | `	}` |
|        - |  9731 | `	/* Top class */` |
|        5 |  9732 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  9733 | `	if( pClass ){` |
|      ! 0 |  9734 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  9735 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  9736 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  9737 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  9738 | `	}` |
|        5 |  9739 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  9740 | `	/* All done */` |
|        5 |  9741 | `	return SXRET_OK;` |
|        1 |  9742 |  |
|        - |  9743 | `/*` |
|        - |  9744 | ` * void debug_print_backtrace()` |
|        - |  9745 | ` *  Prints a backtrace` |
|        - |  9746 | ` * Parameters` |
|        - |  9747 | ` * None` |
|        - |  9748 | ` * Return` |
|        - |  9749 | ` * NULL` |
|        - |  9750 | ` */` |
|        2 |  9751 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9752 |  |
|        3 |  9753 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9754 | `	SyBlob sDump;` |
|        3 |  9755 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  9756 | `	/* Generate the backtrace */` |
|        3 |  9757 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  9758 | `	/* Output backtrace */` |
|        3 |  9759 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9760 | `	/* All done,cleanup */` |
|        3 |  9761 | `	SyBlobRelease(&sDump);` |
|        1 |  9762 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9763 | `	SXUNUSED(apArg);` |
|        3 |  9764 | `	return PH7_OK;` |
|        1 |  9765 |  |
|        - |  9766 | `/*` |
|        - |  9767 | ` * string debug_string_backtrace()` |
|        - |  9768 | ` *  Generate a backtrace` |
|        - |  9769 | ` * Parameters` |
|        - |  9770 | ` * None` |
|        - |  9771 | ` * Return` |
|        - |  9772 | ` *  A mini backtrace().` |
|        - |  9773 | ` * Note that this is a symisc extension.` |
|        - |  9774 | ` */` |
|        2 |  9775 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9776 |  |
|        3 |  9777 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9778 | `	SyBlob sDump;` |
|        3 |  9779 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  9780 | `	/* Generate the backtrace */` |
|        3 |  9781 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  9782 | `	/* Return the backtrace */` |
|        3 |  9783 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  9784 | `	/* All done,cleanup */` |
|        3 |  9785 | `	SyBlobRelease(&sDump);` |
|        1 |  9786 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9787 | `	SXUNUSED(apArg);` |
|        3 |  9788 | `	return PH7_OK;` |
|        1 |  9789 |  |
|        - |  9790 | `/*` |
|        - |  9791 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  9792 | ` * exception is triggered.` |
|        - |  9793 | ` */` |
|      360 |  9794 | `static sxi32 VmUncaughtException(` |
|        - |  9795 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  9796 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  9797 | `	)` |
|        1 |  9798 |  |
|        - |  9799 | `	ph7_value *apArg[2],sArg;` |
|      361 |  9800 | `	int nArg = 1;` |
|        - |  9801 | `	sxi32 rc;` |
|      361 |  9802 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  9803 | `		/* Nesting limit reached */` |
|      ! 0 |  9804 | `		return SXRET_OK;` |
|        - |  9805 | `	}` |
|        - |  9806 | `	/* Call any exception handler if available */` |
|      361 |  9807 | `	PH7_MemObjInit(pVm,&sArg);` |
|      361 |  9808 | `	if( pThis ){` |
|        - |  9809 | `		/* Load the exception instance */` |
|      361 |  9810 | `		sArg.x.pOther = pThis;` |
|      361 |  9811 | `		pThis->iRef++;` |
|      361 |  9812 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      181 |  9813 | `	}else{` |
|      ! 0 |  9814 | `		nArg = 0;` |
|        - |  9815 | `	}` |
|      361 |  9816 | `	apArg[0] = &sArg;` |
|        - |  9817 | `	/* Call the exception handler if available */` |
|      361 |  9818 | `	pVm->nExceptDepth++;` |
|      361 |  9819 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      361 |  9820 | `	pVm->nExceptDepth--;` |
|      361 |  9821 | `	if( rc != SXRET_OK ){` |
|        - |  9822 | `		SyBlob sMsgBuf;` |
|      359 |  9823 | `		const char *zClass = "Exception";` |
|      359 |  9824 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  9825 | `		const char *zMsg;` |
|        - |  9826 | `		sxu32 nMsg;` |
|        - |  9827 | `		const char *zFuncName;` |
|        - |  9828 | `		int nFuncLen;` |
|      359 |  9829 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      359 |  9830 | `		if( pThis ){` |
|        - |  9831 | `			ph7_class_method *pGetMessage;` |
|        - |  9832 | `			ph7_value sMsg;` |
|        - |  9833 | `			const char *zTmp;` |
|        - |  9834 | `			int nTmp;` |
|      359 |  9835 | `			zClass = pThis->pClass->sName.zString;` |
|      359 |  9836 | `			nClass = pThis->pClass->sName.nByte;` |
|      359 |  9837 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      359 |  9838 | `			if( pGetMessage ){` |
|      359 |  9839 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      359 |  9840 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      359 |  9841 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      359 |  9842 | `					if( zTmp && nTmp > 0 ){` |
|      359 |  9843 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      179 |  9844 | `					}` |
|      179 |  9845 | `				}` |
|      359 |  9846 | `				PH7_MemObjRelease(&sMsg);` |
|      179 |  9847 | `			}` |
|      179 |  9848 | `		}` |
|      359 |  9849 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  9850 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  9851 | `		}` |
|      359 |  9852 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      359 |  9853 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      359 |  9854 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      359 |  9855 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      359 |  9856 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  9857 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      359 |  9858 | `		rc = SXERR_ABORT;` |
|      179 |  9859 | `	}` |
|      361 |  9860 | `	PH7_MemObjRelease(&sArg);` |
|      361 |  9861 | `	return rc;` |
|      181 |  9862 |  |
|        - |  9863 | `/*` |
|        - |  9864 | ` * Throw an user exception.` |
|        - |  9865 | ` */` |
|      374 |  9866 | `static sxi32 VmThrowException(` |
|        - |  9867 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  9868 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  9869 | `	)` |
|        2 |  9870 |  |
|        - |  9871 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  9872 | `	ph7_exception **apException;` |
|        - |  9873 | `	ph7_exception *pException;` |
|        - |  9874 | `	/* Point to the stack of loaded exceptions */` |
|      376 |  9875 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      376 |  9876 | `	pException = 0;` |
|      376 |  9877 | `	pCatch = 0;` |
|      376 |  9878 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  9879 | `		ph7_exception_block *aCatch;` |
|        - |  9880 | `		ph7_class *pClass;` |
|        - |  9881 | `		sxu32 j;` |
|        - |  9882 | `		/* Locate the appropriate block to execute */` |
|       16 |  9883 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       16 |  9884 | `		(void)SySetPop(&pVm->aException);` |
|       16 |  9885 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       16 |  9886 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       16 |  9887 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  9888 | `			/* Extract the target class */` |
|       16 |  9889 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       16 |  9890 | `			if( pClass == 0 ){` |
|        - |  9891 | `				/* No such class */` |
|      ! 0 |  9892 | `				continue;` |
|        - |  9893 | `			}` |
|       16 |  9894 | `			if( VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  9895 | `				/* Catch block found,break immeditaley */` |
|       16 |  9896 | `				pCatch = &aCatch[j];` |
|       16 |  9897 | `				break;` |
|        - |  9898 | `			}` |
|      ! 0 |  9899 | `		}` |
|        7 |  9900 | `	}` |
|        - |  9901 | `	/* Execute the cached block if available */` |
|      376 |  9902 | `	if( pCatch == 0 ){` |
|        - |  9903 | `		sxi32 rc;` |
|      361 |  9904 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      361 |  9905 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  9906 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  9907 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9908 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  9909 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  9910 | `			}` |
|      ! 0 |  9911 | `			if( pException->pFrame == pFrame ){` |
|        - |  9912 | `				/* Tell the upper layer that the exception was caught */` |
|      ! 0 |  9913 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  9914 | `			}` |
|      ! 0 |  9915 | `		}` |
|      361 |  9916 | `		return rc;` |
|      ! 0 |  9917 | `	}else{` |
|       16 |  9918 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  9919 | `		sxi32 rc;` |
|       24 |  9920 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9921 | `			/* Safely ignore the exception frame */` |
|       10 |  9922 | `			pFrame = pFrame->pParent;` |
|        2 |  9923 | `		}` |
|       16 |  9924 | `		if( pException->pFrame == pFrame ){` |
|        - |  9925 | `			/* Tell the upper layer that the exception was caught */` |
|        8 |  9926 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        3 |  9927 | `		}` |
|        - |  9928 | `		/* Create a private frame first */` |
|       16 |  9929 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       16 |  9930 | `		if( rc == SXRET_OK ){` |
|        - |  9931 | `			/* Mark as catch frame */` |
|       16 |  9932 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       16 |  9933 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       16 |  9934 | `			if( pObj ){` |
|        - |  9935 | `				/* Install the exception instance */` |
|       16 |  9936 | `				pThis->iRef++; /* Increment reference count */` |
|       16 |  9937 | `				pObj->x.pOther = pThis;` |
|       16 |  9938 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        7 |  9939 | `			}` |
|        - |  9940 | `			/* Exceute the block */` |
|       16 |  9941 | `			VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  9942 | `			/* Leave the frame */` |
|       16 |  9943 | `			VmLeaveFrame(&(*pVm));` |
|        7 |  9944 | `		}` |
|        - |  9945 | `	}` |
|        - |  9946 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  9947 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  9948 | `	 */` |
|       16 |  9949 | `	return SXRET_OK;` |
|      189 |  9950 |  |
|        - |  9951 | `/*` |
|        - |  9952 | ` * Section:` |
|        - |  9953 | ` *  Version,Credits and Copyright related functions.` |
|        - |  9954 | ` * Status:` |
|        - |  9955 | ` *    Stable.` |
|        - |  9956 | ` */` |
|        - |  9957 | `/*` |
|        - |  9958 | ` * string ph7version(void)` |
|        - |  9959 | ` *  Returns the running version of the PH7 version.` |
|        - |  9960 | ` * Parameters` |
|        - |  9961 | ` *  None` |
|        - |  9962 | ` * Return` |
|        - |  9963 | ` * Current PH7 version.` |
|        - |  9964 | ` */` |
|        2 |  9965 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9966 |  |
|        1 |  9967 | `	SXUNUSED(nArg);` |
|        1 |  9968 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  9969 | `	/* Current engine version */` |
|        3 |  9970 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  9971 | `	return PH7_OK;` |
|        1 |  9972 |  |
|        - |  9973 | `/*` |
|        - |  9974 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  9975 | ` */` |
|        - |  9976 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  9977 | ` "<html><head>"\` |
|        - |  9978 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  9979 | ` "<style type=\"text/css\">"\` |
|        - |  9980 | ` "div {"\` |
|        - |  9981 | `     "border: 1px solid #cccccc;"\` |
|        - |  9982 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  9983 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  9984 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  9985 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  9986 | `     "-webkit-border-radius: 10px;"\` |
|        - |  9987 | `     "-o-border-radius: 10px;"\` |
|        - |  9988 | `     "border-radius: 10px;"\` |
|        - |  9989 | `     "padding-left: 2em;"\` |
|        - |  9990 | `     "background-color: white;"\` |
|        - |  9991 | `     "margin-left: auto;"\` |
|        - |  9992 | `     "font-family: verdana;"\` |
|        - |  9993 | `     "padding-right: 2em;"\` |
|        - |  9994 | `     "margin-right: auto;"\` |
|        - |  9995 | `     "}"\` |
|        - |  9996 | `     "body {"\` |
|        - |  9997 | `     "padding: 0.2em;"\` |
|        - |  9998 | `     "font-style: normal;"\` |
|        - |  9999 | `     "font-size: medium;"\` |
|        - | 10000 | `     "background-color: #f2f2f2;"\` |
|        - | 10001 | `     "}"\` |
|        - | 10002 | `     "hr {"\` |
|        - | 10003 | `     "border-style: solid none none;"\` |
|        - | 10004 | `     "border-width: 1px medium medium;"\` |
|        - | 10005 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 10006 | `     "height: 1px;"\` |
|        - | 10007 | `     "}"\` |
|        - | 10008 | `     "a {"\` |
|        - | 10009 | `     "color: #3366cc;"\` |
|        - | 10010 | `     "text-decoration: none;"\` |
|        - | 10011 | `     "}"\` |
|        - | 10012 | `     "a:hover {"\` |
|        - | 10013 | `     "color: #999999;"\` |
|        - | 10014 | `     "}"\` |
|        - | 10015 | `     "a:active {"\` |
|        - | 10016 | `     "color: #663399;"\` |
|        - | 10017 | `     "}"\` |
|        - | 10018 | `     "h1 {"\` |
|        - | 10019 | `     "margin: 0;"\` |
|        - | 10020 | `     "padding: 0;"\` |
|        - | 10021 | `     "font-family: Verdana;"\` |
|        - | 10022 | `     "font-weight: bold;"\` |
|        - | 10023 | `     "font-style: normal;"\` |
|        - | 10024 | `     "font-size: medium;"\` |
|        - | 10025 | `     "text-transform: capitalize;"\` |
|        - | 10026 | `     "color: #0a328c;"\` |
|        - | 10027 | `     "}"\` |
|        - | 10028 | `     "p {"\` |
|        - | 10029 | `     "margin: 0 auto;"\` |
|        - | 10030 | `     "font-size: medium;"\` |
|        - | 10031 | `     "font-style: normal;"\` |
|        - | 10032 | `     "font-family: verdana;"\` |
|        - | 10033 | `     "}"\` |
|        - | 10034 | `"</style></head><body>"\` |
|        - | 10035 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10036 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 10037 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 10038 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 10039 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 10040 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 10041 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 10042 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 10043 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 10044 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 10045 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 10046 |  |
|        - | 10047 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10048 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 10049 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 10050 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 10051 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10052 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 10053 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10054 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 10055 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10056 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 10057 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10058 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 10059 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 10060 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 10061 |  |
|        - | 10062 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 10063 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 10064 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 10065 | `"&nbsp;*<br>"\` |
|        - | 10066 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 10067 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 10068 | `"&nbsp;* are met:<br>"\` |
|        - | 10069 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 10070 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 10071 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 10072 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 10073 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 10074 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 10075 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 10076 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 10077 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 10078 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 10079 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 10080 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 10081 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 10082 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 10083 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 10084 | `"&nbsp;*<br>"\` |
|        - | 10085 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 10086 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 10087 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 10088 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 10089 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 10090 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 10091 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 10092 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 10093 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 10094 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 10095 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 10096 | `"&nbsp;*/<br>"\` |
|        - | 10097 | `"</span></small></small></p>"\` |
|        - | 10098 | `"</div></body></html>"` |
|        - | 10099 | `/*` |
|        - | 10100 | ` * bool ph7credits(void)` |
|        - | 10101 | ` * bool ph7info(void)` |
|        - | 10102 | ` * bool ph7copyright(void)` |
|        - | 10103 | ` *  Prints out the credits for PH7 engine` |
|        - | 10104 | ` * Parameters` |
|        - | 10105 | ` *  None` |
|        - | 10106 | ` * Return` |
|        - | 10107 | ` *  Always TRUE` |
|        - | 10108 | ` */` |
|        2 | 10109 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10110 |  |
|        3 | 10111 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 10112 | `	/* Expand the HTML page above*/` |
|        3 | 10113 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 10114 | `	ph7_context_output_format(` |
|        1 | 10115 | `		pCtx,` |
|        - | 10116 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 10117 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 10118 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 10119 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 10120 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 10121 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 10122 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 10123 | `#ifdef __WINNT__` |
|        - | 10124 | `		"Windows NT"` |
|        - | 10125 | `#elif defined(__UNIXES__)` |
|        - | 10126 | `		"UNIX-Like"` |
|        - | 10127 | `#else` |
|        - | 10128 | `		"Other OS"` |
|        - | 10129 | `#endif` |
|        - | 10130 | `		);` |
|        3 | 10131 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 10132 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10133 | `	SXUNUSED(apArg);` |
|        - | 10134 | `	/* Return TRUE */` |
|        - | 10135 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 10136 | `	return PH7_OK;` |
|        1 | 10137 |  |
|        - | 10138 | `/*` |
|        - | 10139 | ` * Section:` |
|        - | 10140 | ` *    URL related routines.` |
|        - | 10141 | ` * Status:` |
|        - | 10142 | ` *    Stable.` |
|        - | 10143 | ` */` |
|        - | 10144 | `/*` |
|        - | 10145 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 10146 | ` *  Parse a URL and return its fields.` |
|        - | 10147 | ` * Parameters` |
|        - | 10148 | ` *  $url` |
|        - | 10149 | ` *   The URL to parse.` |
|        - | 10150 | ` * $component` |
|        - | 10151 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 10152 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 10153 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 10154 | ` *  in which case the return value will be an integer).` |
|        - | 10155 | ` * Return` |
|        - | 10156 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 10157 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 10158 | ` *  this array are:` |
|        - | 10159 | ` *   scheme - e.g. http` |
|        - | 10160 | ` *   host` |
|        - | 10161 | ` *   port` |
|        - | 10162 | ` *   user` |
|        - | 10163 | ` *   pass` |
|        - | 10164 | ` *   path` |
|        - | 10165 | ` *   query - after the question mark ?` |
|        - | 10166 | ` *   fragment - after the hashmark #` |
|        - | 10167 | ` * Note:` |
|        - | 10168 | ` *  FALSE is returned on failure.` |
|        - | 10169 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 10170 | ` *  with the standard PHP engine.` |
|        - | 10171 | ` */` |
|       28 | 10172 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10173 |  |
|        - | 10174 | `	const char *zStr; /* Input string */` |
|        - | 10175 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 10176 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 10177 | `	int nLen;` |
|        - | 10178 | `	sxi32 rc;` |
|       29 | 10179 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10180 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 10181 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10182 | `		return PH7_OK;` |
|        - | 10183 | `	}` |
|        - | 10184 | `	/* Extract the given URI */` |
|       29 | 10185 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 10186 | `	if( nLen < 1 ){` |
|        - | 10187 | `		/* Nothing to process,return FALSE */` |
|        3 | 10188 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10189 | `		return PH7_OK;` |
|        - | 10190 | `	}` |
|        - | 10191 | `	/* Get a parse */` |
|       27 | 10192 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 10193 | `	if( rc != SXRET_OK ){` |
|        - | 10194 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 10195 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10196 | `		return PH7_OK;` |
|        - | 10197 | `	}` |
|       27 | 10198 | `	if( nArg > 1 ){` |
|      ! 0 | 10199 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 10200 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 10201 | `		switch(nComponent){` |
|      ! 0 | 10202 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 10203 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 10204 | `			if( pComp->nByte < 1 ){` |
|        - | 10205 | `				/* No available value,return NULL */` |
|      ! 0 | 10206 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10207 | `			}else{` |
|      ! 0 | 10208 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10209 | `			}` |
|      ! 0 | 10210 | `			break;` |
|      ! 0 | 10211 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 10212 | `			pComp = &sURI.sHost;` |
|      ! 0 | 10213 | `			if( pComp->nByte < 1 ){` |
|        - | 10214 | `				/* No available value,return NULL */` |
|      ! 0 | 10215 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10216 | `			}else{` |
|      ! 0 | 10217 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10218 | `			}` |
|      ! 0 | 10219 | `			break;` |
|      ! 0 | 10220 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 10221 | `			pComp = &sURI.sPort;` |
|      ! 0 | 10222 | `			if( pComp->nByte < 1 ){` |
|        - | 10223 | `				/* No available value,return NULL */` |
|      ! 0 | 10224 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10225 | `			}else{` |
|      ! 0 | 10226 | `				int iPort = 0;` |
|        - | 10227 | `				/* Cast the value to integer */` |
|      ! 0 | 10228 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 10229 | `				ph7_result_int(pCtx,iPort);` |
|        - | 10230 | `			}` |
|      ! 0 | 10231 | `			break;` |
|      ! 0 | 10232 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 10233 | `			pComp = &sURI.sUser;` |
|      ! 0 | 10234 | `			if( pComp->nByte < 1 ){` |
|        - | 10235 | `				/* No available value,return NULL */` |
|      ! 0 | 10236 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10237 | `			}else{` |
|      ! 0 | 10238 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10239 | `			}` |
|      ! 0 | 10240 | `			break;` |
|      ! 0 | 10241 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 10242 | `			pComp = &sURI.sPass;` |
|      ! 0 | 10243 | `			if( pComp->nByte < 1 ){` |
|        - | 10244 | `				/* No available value,return NULL */` |
|      ! 0 | 10245 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10246 | `			}else{` |
|      ! 0 | 10247 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10248 | `			}` |
|      ! 0 | 10249 | `			break;` |
|      ! 0 | 10250 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 10251 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 10252 | `			if( pComp->nByte < 1 ){` |
|        - | 10253 | `				/* No available value,return NULL */` |
|      ! 0 | 10254 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10255 | `			}else{` |
|      ! 0 | 10256 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10257 | `			}` |
|      ! 0 | 10258 | `			break;` |
|      ! 0 | 10259 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 10260 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 10261 | `			if( pComp->nByte < 1 ){` |
|        - | 10262 | `				/* No available value,return NULL */` |
|      ! 0 | 10263 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10264 | `			}else{` |
|      ! 0 | 10265 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10266 | `			}` |
|      ! 0 | 10267 | `			break;` |
|      ! 0 | 10268 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 10269 | `			pComp = &sURI.sPath;` |
|      ! 0 | 10270 | `			if( pComp->nByte < 1 ){` |
|        - | 10271 | `				/* No available value,return NULL */` |
|      ! 0 | 10272 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10273 | `			}else{` |
|      ! 0 | 10274 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10275 | `			}` |
|      ! 0 | 10276 | `			break;` |
|      ! 0 | 10277 | `		default:` |
|        - | 10278 | `			/* No such entry,return NULL */` |
|      ! 0 | 10279 | `			ph7_result_null(pCtx);` |
|      ! 0 | 10280 | `			break;` |
|        - | 10281 | `		}` |
|      ! 0 | 10282 | `	}else{` |
|        - | 10283 | `		ph7_value *pArray,*pValue;` |
|        - | 10284 | `		/* Return an associative array */` |
|       27 | 10285 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 10286 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 10287 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10288 | `			/* Out of memory */` |
|      ! 0 | 10289 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10290 | `			/* Return false */` |
|      ! 0 | 10291 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 10292 | `			return PH7_OK;` |
|        - | 10293 | `		}` |
|        - | 10294 | `		/* Fill the array */` |
|       27 | 10295 | `		pComp = &sURI.sScheme;` |
|       27 | 10296 | `		if( pComp->nByte > 0 ){` |
|       19 | 10297 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 10298 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 10299 | `		}` |
|        - | 10300 | `		/* Reset the string cursor */` |
|       27 | 10301 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10302 | `		pComp = &sURI.sHost;` |
|       27 | 10303 | `		if( pComp->nByte > 0 ){` |
|       25 | 10304 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 10305 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 10306 | `		}` |
|        - | 10307 | `		/* Reset the string cursor */` |
|       27 | 10308 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10309 | `		pComp = &sURI.sPort;` |
|       27 | 10310 | `		if( pComp->nByte > 0 ){` |
|       11 | 10311 | `			int iPort = 0;/* cc warning */` |
|        - | 10312 | `			/* Convert to integer */` |
|       11 | 10313 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 10314 | `			ph7_value_int(pValue,iPort);` |
|       11 | 10315 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 10316 | `		}` |
|        - | 10317 | `		/* Reset the string cursor */` |
|       27 | 10318 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10319 | `		pComp = &sURI.sUser;` |
|       27 | 10320 | `		if( pComp->nByte > 0 ){` |
|        7 | 10321 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10322 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 10323 | `		}` |
|        - | 10324 | `		/* Reset the string cursor */` |
|       27 | 10325 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10326 | `		pComp = &sURI.sPass;` |
|       27 | 10327 | `		if( pComp->nByte > 0 ){` |
|        7 | 10328 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10329 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 10330 | `		}` |
|        - | 10331 | `		/* Reset the string cursor */` |
|       27 | 10332 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10333 | `		pComp = &sURI.sPath;` |
|       27 | 10334 | `		if( pComp->nByte > 0 ){` |
|       17 | 10335 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 10336 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 10337 | `		}` |
|        - | 10338 | `		/* Reset the string cursor */` |
|       27 | 10339 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10340 | `		pComp = &sURI.sQuery;` |
|       27 | 10341 | `		if( pComp->nByte > 0 ){` |
|        5 | 10342 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10343 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 10344 | `		}` |
|        - | 10345 | `		/* Reset the string cursor */` |
|       27 | 10346 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10347 | `		pComp = &sURI.sFragment;` |
|       27 | 10348 | `		if( pComp->nByte > 0 ){` |
|        5 | 10349 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10350 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 10351 | `		}` |
|        - | 10352 | `		/* Return the created array */` |
|       27 | 10353 | `		ph7_result_value(pCtx,pArray);` |
|        - | 10354 | `		/* NOTE:` |
|        - | 10355 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 10356 | `		 * automatically as soon we return from this function.` |
|        - | 10357 | `		 */` |
|        - | 10358 | `	}` |
|        - | 10359 | `	/* All done */` |
|       27 | 10360 | `	return PH7_OK;` |
|       15 | 10361 |  |
|        - | 10362 | `/*` |
|        - | 10363 | ` * Section:` |
|        - | 10364 | ` *   Array related routines.` |
|        - | 10365 | ` * Status:` |
|        - | 10366 | ` *    Stable.` |
|        - | 10367 | ` * Note 2012-5-21 01:04:15:` |
|        - | 10368 | ` *  Array related functions that need access to the underlying` |
|        - | 10369 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 10370 | ` */` |
|        - | 10371 | `/*` |
|        - | 10372 | ` * The [compact()] function store it's state information in an instance` |
|        - | 10373 | ` * of the following structure.` |
|        - | 10374 | ` */` |
|        - | 10375 | `struct compact_data` |
|        - | 10376 |  |
|        - | 10377 | `	ph7_value *pArray;  /* Target array */` |
|        - | 10378 | `	int nRecCount;      /* Recursion count */` |
|        - | 10379 | `};` |
|        - | 10380 | `/*` |
|        - | 10381 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 10382 | ` */` |
|      ! 0 | 10383 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 10384 |  |
|      ! 0 | 10385 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 10386 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 10387 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 10388 | `	/* Act according to the hashmap value */` |
|      ! 0 | 10389 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 10390 | `		SyString sVar;` |
|      ! 0 | 10391 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 10392 | `		if( sVar.nByte > 0 ){` |
|        - | 10393 | `			/* Query the current frame */` |
|      ! 0 | 10394 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 10395 | `			/* ^` |
|        - | 10396 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 10397 | `			 */` |
|      ! 0 | 10398 | `			if( pKey ){` |
|        - | 10399 | `				/* Perform the insertion */` |
|      ! 0 | 10400 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 10401 | `			}` |
|      ! 0 | 10402 | `		}` |
|      ! 0 | 10403 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 10404 | `		int rc;` |
|        - | 10405 | `		/* Recursively traverse this array */` |
|      ! 0 | 10406 | `		pData->nRecCount++;` |
|      ! 0 | 10407 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 10408 | `		pData->nRecCount--;` |
|      ! 0 | 10409 | `		return rc;` |
|        - | 10410 | `	}` |
|      ! 0 | 10411 | `	return SXRET_OK;` |
|      ! 0 | 10412 |  |
|        - | 10413 | `/*` |
|        - | 10414 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 10415 | ` *  Create array containing variables and their values.` |
|        - | 10416 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 10417 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 10418 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 10419 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 10420 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 10421 | ` * Parameters` |
|        - | 10422 | ` *  $varname` |
|        - | 10423 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 10424 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 10425 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 10426 | ` *   it recursively.` |
|        - | 10427 | ` * Return` |
|        - | 10428 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 10429 | ` */` |
|        2 | 10430 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10431 |  |
|        - | 10432 | `	ph7_value *pArray,*pObj;` |
|        3 | 10433 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10434 | `	const char *zName;` |
|        - | 10435 | `	SyString sVar;` |
|        - | 10436 | `	int i,nLen;` |
|        3 | 10437 | `	if( nArg < 1 ){` |
|        - | 10438 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 10439 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10440 | `		return PH7_OK;` |
|        - | 10441 | `	}` |
|        - | 10442 | `	/* Create the array */` |
|        3 | 10443 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10444 | `	if( pArray == 0 ){` |
|        - | 10445 | `		/* Out of memory */` |
|      ! 0 | 10446 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10447 | `		/* Return NULL */` |
|      ! 0 | 10448 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10449 | `		return PH7_OK;` |
|        - | 10450 | `	}` |
|        - | 10451 | `	/* Perform the requested operation */` |
|        7 | 10452 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 10453 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 10454 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 10455 | `				struct compact_data sData;` |
|      ! 0 | 10456 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 10457 | `				/* Recursively walk the array */` |
|      ! 0 | 10458 | `				sData.nRecCount = 0;` |
|      ! 0 | 10459 | `				sData.pArray = pArray;` |
|      ! 0 | 10460 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 10461 | `			}` |
|      ! 0 | 10462 | `		}else{` |
|        - | 10463 | `			/* Extract variable name */` |
|        5 | 10464 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 10465 | `			if( nLen > 0 ){` |
|        5 | 10466 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 10467 | `				/* Check if the variable is available in the current frame */` |
|        5 | 10468 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 10469 | `				if( pObj ){` |
|        5 | 10470 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 10471 | `				}` |
|        2 | 10472 | `			}` |
|        - | 10473 | `		}` |
|        3 | 10474 | `	}` |
|        - | 10475 | `	/* Return the array */` |
|        3 | 10476 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10477 | `	return PH7_OK;` |
|        2 | 10478 |  |
|        - | 10479 | `/*` |
|        - | 10480 | ` * The [extract()] function store it's state information in an instance` |
|        - | 10481 | ` * of the following structure.` |
|        - | 10482 | ` */` |
|        - | 10483 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 10484 | `struct extract_aux_data` |
|        - | 10485 |  |
|        - | 10486 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 10487 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 10488 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 10489 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 10490 | `	int iFlags;           /* Control flags */` |
|        - | 10491 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 10492 | `};` |
|        - | 10493 | `/* Forward declaration */` |
|        - | 10494 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 10495 | `/*` |
|        - | 10496 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 10497 | ` *   Import variables into the current symbol table from an array.` |
|        - | 10498 | ` * Parameters` |
|        - | 10499 | ` * $var_array` |
|        - | 10500 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 10501 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 10502 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 10503 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 10504 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 10505 | ` * $extract_type` |
|        - | 10506 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 10507 | ` *  It can be one of the following values:` |
|        - | 10508 | ` *   EXTR_OVERWRITE` |
|        - | 10509 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 10510 | ` *   EXTR_SKIP` |
|        - | 10511 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 10512 | ` *   EXTR_PREFIX_SAME` |
|        - | 10513 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 10514 | ` *   EXTR_PREFIX_ALL` |
|        - | 10515 | ` *       Prefix all variable names with prefix.` |
|        - | 10516 | ` *   EXTR_PREFIX_INVALID` |
|        - | 10517 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 10518 | ` *   EXTR_IF_EXISTS` |
|        - | 10519 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 10520 | ` *       otherwise do nothing.` |
|        - | 10521 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 10522 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 10523 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 10524 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 10525 | ` *      the current symbol table.` |
|        - | 10526 | ` * $prefix` |
|        - | 10527 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 10528 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 10529 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 10530 | ` *  underscore character.` |
|        - | 10531 | ` * Return` |
|        - | 10532 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 10533 | ` */` |
|        4 | 10534 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10535 |  |
|        - | 10536 | `	extract_aux_data sAux;` |
|        - | 10537 | `	ph7_hashmap *pMap;` |
|        5 | 10538 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 10539 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 10540 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 10541 | `		return PH7_OK;` |
|        - | 10542 | `	}` |
|        - | 10543 | `	/* Point to the target hashmap */` |
|        5 | 10544 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 10545 | `	if( pMap->nEntry < 1 ){` |
|        - | 10546 | `		/* Empty map,return  0 */` |
|      ! 0 | 10547 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 10548 | `		return PH7_OK;` |
|        - | 10549 | `	}` |
|        - | 10550 | `	/* Prepare the aux data */` |
|        5 | 10551 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 10552 | `	if( nArg > 1 ){` |
|        3 | 10553 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 10554 | `		if( nArg > 2 ){` |
|      ! 0 | 10555 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 10556 | `		}` |
|        1 | 10557 | `	}` |
|        5 | 10558 | `	sAux.pVm = pCtx->pVm;` |
|        - | 10559 | `	/* Invoke the worker callback */` |
|        5 | 10560 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 10561 | `	/* Number of variables successfully imported */` |
|        5 | 10562 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 10563 | `	return PH7_OK;` |
|        3 | 10564 |  |
|        - | 10565 | `/*` |
|        - | 10566 | ` * Worker callback for the [extract()] function defined` |
|        - | 10567 | ` * below.` |
|        - | 10568 | ` */` |
|        8 | 10569 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 10570 |  |
|        9 | 10571 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 10572 | `	int iFlags = pAux->iFlags;` |
|        9 | 10573 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 10574 | `	ph7_value *pObj;` |
|        - | 10575 | `	SyString sVar;` |
|        9 | 10576 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 10577 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 10578 | `	}` |
|        - | 10579 | `	/* Perform a string cast */` |
|        9 | 10580 | `	PH7_MemObjToString(pKey);` |
|        9 | 10581 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 10582 | `		/* Unavailable variable name */` |
|      ! 0 | 10583 | `		return SXRET_OK;` |
|        - | 10584 | `	}` |
|        9 | 10585 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 10586 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 10587 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 10588 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 10589 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10590 | `			);` |
|      ! 0 | 10591 | `	}else{` |
|       13 | 10592 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 10593 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 10594 | `	}` |
|        9 | 10595 | `	sVar.zString = pAux->zWorker;` |
|        - | 10596 | `	/* Try to extract the variable */` |
|        9 | 10597 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 10598 | `	if( pObj ){` |
|        - | 10599 | `		/* Collision */` |
|        5 | 10600 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 10601 | `			return SXRET_OK;` |
|        - | 10602 | `		}` |
|        5 | 10603 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 10604 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 10605 | `				/* Already prefixed */` |
|      ! 0 | 10606 | `				return SXRET_OK;` |
|        - | 10607 | `			}` |
|      ! 0 | 10608 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 10609 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 10610 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10611 | `				);` |
|      ! 0 | 10612 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 10613 | `		}` |
|        3 | 10614 | `	}else{` |
|        - | 10615 | `		/* Create the variable */` |
|        5 | 10616 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 10617 | `	}` |
|        9 | 10618 | `	if( pObj ){` |
|        - | 10619 | `		/* Overwrite the old value */` |
|        9 | 10620 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 10621 | `		/* Increment counter */` |
|        9 | 10622 | `		pAux->iCount++;` |
|        4 | 10623 | `	}` |
|        9 | 10624 | `	return SXRET_OK;` |
|        5 | 10625 |  |
|        - | 10626 | `/*` |
|        - | 10627 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 10628 | ` * defined below.` |
|        - | 10629 | ` */` |
|        2 | 10630 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 10631 |  |
|        3 | 10632 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 10633 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 10634 | `	ph7_value *pObj;` |
|        - | 10635 | `	SyString sVar;` |
|        - | 10636 | `	/* Perform a string cast */` |
|        3 | 10637 | `	PH7_MemObjToString(pKey);` |
|        3 | 10638 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 10639 | `		/* Unavailable variable name */` |
|      ! 0 | 10640 | `		return SXRET_OK;` |
|        - | 10641 | `	}` |
|        3 | 10642 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 10643 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 10644 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 10645 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 10646 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10647 | `			);` |
|        2 | 10648 | `	}else{` |
|      ! 0 | 10649 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 10650 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 10651 | `	}` |
|        3 | 10652 | `	sVar.zString = pAux->zWorker;` |
|        - | 10653 | `	/* Extract the variable */` |
|        3 | 10654 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 10655 | `	if( pObj ){` |
|        3 | 10656 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 10657 | `	}` |
|        3 | 10658 | `	return SXRET_OK;` |
|        2 | 10659 |  |
|        - | 10660 | `/*` |
|        - | 10661 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 10662 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 10663 | ` * Parameters` |
|        - | 10664 | ` * $types` |
|        - | 10665 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 10666 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 10667 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 10668 | ` *  POST includes the POST uploaded file information.` |
|        - | 10669 | ` *  Note:` |
|        - | 10670 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 10671 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 10672 | ` * $prefix` |
|        - | 10673 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 10674 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 10675 | ` *  variable named $pref_userid.` |
|        - | 10676 | ` * Return` |
|        - | 10677 | ` *  TRUE on success or FALSE on failure.` |
|        - | 10678 | ` */` |
|        2 | 10679 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10680 |  |
|        - | 10681 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 10682 | `	extract_aux_data sAux;` |
|        - | 10683 | `	int nLen,nPrefixLen;` |
|        - | 10684 | `	ph7_value *pSuper;` |
|        - | 10685 | `	ph7_vm *pVm;` |
|        - | 10686 | `	/* By default import only $_GET variables  */` |
|        3 | 10687 | `	zImport = "G";` |
|        3 | 10688 | `	nLen = (int)sizeof(char);` |
|        3 | 10689 | `	zPrefix = 0;` |
|        3 | 10690 | `	nPrefixLen = 0;` |
|        3 | 10691 | `	if( nArg > 0 ){` |
|        3 | 10692 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 10693 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 10694 | `		}` |
|        3 | 10695 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 10696 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 10697 | `		}` |
|        1 | 10698 | `	}` |
|        - | 10699 | `	/* Point to the underlying VM */` |
|        3 | 10700 | `	pVm = pCtx->pVm;` |
|        - | 10701 | `	/* Initialize the aux data */` |
|        3 | 10702 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 10703 | `	sAux.zPrefix = zPrefix;` |
|        3 | 10704 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 10705 | `	sAux.pVm = pVm;` |
|        - | 10706 | `	/* Extract */` |
|        3 | 10707 | `	zEnd = &zImport[nLen];` |
|        5 | 10708 | `	while( zImport < zEnd ){` |
|        3 | 10709 | `		int c = zImport[0];` |
|        3 | 10710 | `		pSuper = 0;` |
|        3 | 10711 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 10712 | `			/* Import $_GET variables */` |
|        3 | 10713 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 10714 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 10715 | `			/* Import $_POST variables */` |
|      ! 0 | 10716 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 10717 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 10718 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 10719 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 10720 | `		}` |
|        3 | 10721 | `		if( pSuper ){` |
|        - | 10722 | `			/* Iterate throw array entries */` |
|        3 | 10723 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 10724 | `		}` |
|        - | 10725 | `		/* Advance the cursor */` |
|        3 | 10726 | `		zImport++;` |
|        1 | 10727 | `	}` |
|        - | 10728 | `	/* All done,return TRUE*/` |
|        3 | 10729 | `	ph7_result_bool(pCtx,0);` |
|        3 | 10730 | `	return PH7_OK;` |
|        1 | 10731 |  |
|        - | 10732 | `/*` |
|        - | 10733 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 10734 | ` * Refer to the eval() language construct implementation for more` |
|        - | 10735 | ` * information.` |
|        - | 10736 | ` */` |
|     9396 | 10737 | `static sxi32 VmEvalChunk(` |
|        - | 10738 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 10739 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 10740 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 10741 | `	int iFlags,         /* Compile flag */` |
|        - | 10742 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 10743 | `	)` |
|        2 | 10744 |  |
|        - | 10745 | `	SySet *pByteCode,aByteCode;` |
|     9398 | 10746 | `	ProcConsumer xErr = 0;` |
|     9398 | 10747 | `	void *pErrData = 0;` |
|        - | 10748 | `	/* Initialize bytecode container */` |
|     9398 | 10749 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     9398 | 10750 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 10751 | `	/* Reset the code generator */` |
|     9398 | 10752 | `	if( bTrueReturn ){` |
|        - | 10753 | `		/* Included file,log compile-time errors */` |
|     7469 | 10754 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7469 | 10755 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3734 | 10756 | `	}` |
|     9398 | 10757 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 10758 | `	/* Swap bytecode container */` |
|     9398 | 10759 | `	pByteCode = pVm->pByteContainer;` |
|     9398 | 10760 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 10761 | `	/* Compile the chunk */` |
|     9398 | 10762 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    14096 | 10763 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 10764 | `		/* Compilation error,return false */` |
|        3 | 10765 | `		if( pCtx ){` |
|        3 | 10766 | `			ph7_result_bool(pCtx,0);` |
|        1 | 10767 | `		}` |
|        2 | 10768 | `	}else{` |
|        - | 10769 | `		/* Mount any newly defined classes */` |
|        - | 10770 | `		SyHashEntry *pEntry;` |
|        - | 10771 | `		ph7_class *pClass;` |
|        - | 10772 | `		ph7_value sResult; /* Return value */` |
|        - | 10773 | `		sxi32 rc;` |
|     9396 | 10774 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   257271 | 10775 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   243180 | 10776 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 10777 | `			/* Only mount classes that haven't been mounted yet */` |
|   243180 | 10778 | `			if( !pClass->bMounted ){` |
|    52912 | 10779 | `				rc = VmMountUserClass(pVm,pClass);` |
|    52912 | 10780 | `				if( rc != SXRET_OK ){` |
|        - | 10781 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 10782 | `					if( pCtx ){` |
|      ! 0 | 10783 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 10784 | `					}` |
|      ! 0 | 10785 | `					goto Cleanup;` |
|        - | 10786 | `				}` |
|    26455 | 10787 | `			}` |
|        2 | 10788 | `		}` |
|     9396 | 10789 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 10790 | `			/* Out of memory */` |
|      ! 0 | 10791 | `			if( pCtx ){` |
|      ! 0 | 10792 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 10793 | `			}` |
|      ! 0 | 10794 | `			goto Cleanup;` |
|        - | 10795 | `		}` |
|     9396 | 10796 | `		if( bTrueReturn ){` |
|        - | 10797 | `			/* Assume a boolean true return value */` |
|     7469 | 10798 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3735 | 10799 | `		}else{` |
|        - | 10800 | `			/* Assume a null return value */` |
|     1928 | 10801 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 10802 | `		}` |
|        - | 10803 | `		/* Execute the compiled chunk */` |
|     9396 | 10804 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|     9396 | 10805 | `		if( pCtx ){` |
|        - | 10806 | `			/* Set the execution result */` |
|     7486 | 10807 | `			ph7_result_value(pCtx,&sResult);` |
|     3742 | 10808 | `		}` |
|     9396 | 10809 | `		PH7_MemObjRelease(&sResult);` |
|        - | 10810 | `	}` |
|     4698 | 10811 | `Cleanup:` |
|        - | 10812 | `	/* Cleanup the mess left behind */` |
|     9398 | 10813 | `	pVm->pByteContainer = pByteCode;` |
|     9398 | 10814 | `	SySetRelease(&aByteCode);` |
|     9398 | 10815 | `	return SXRET_OK;` |
|        2 | 10816 |  |
|        - | 10817 | `/*` |
|        - | 10818 | ` * value eval(string $code)` |
|        - | 10819 | ` *   Evaluate a string as PHP code.` |
|        - | 10820 | ` * Parameter` |
|        - | 10821 | ` *  code: PHP code to evaluate.` |
|        - | 10822 | ` * Return` |
|        - | 10823 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 10824 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 10825 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 10826 | ` */` |
|       16 | 10827 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10828 |  |
|        - | 10829 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 | 10830 | `	if( nArg < 1 ){` |
|        - | 10831 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10832 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10833 | `		return SXRET_OK;` |
|        - | 10834 | `	}` |
|        - | 10835 | `	/* Chunk to evaluate */` |
|       18 | 10836 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 | 10837 | `	if( sChunk.nByte < 1 ){` |
|        - | 10838 | `		/* Empty string,return NULL */` |
|        3 | 10839 | `		ph7_result_null(pCtx);` |
|        3 | 10840 | `		return SXRET_OK;` |
|        - | 10841 | `	}` |
|        - | 10842 | `	/* Eval the chunk */` |
|       16 | 10843 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 | 10844 | `	return SXRET_OK;` |
|       10 | 10845 |  |
|        - | 10846 | `/*` |
|        - | 10847 | ` * Check if a file path is already included.` |
|        - | 10848 | ` */` |
|    14932 | 10849 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 | 10850 |  |
|        - | 10851 | `	SyString *aEntries;` |
|        - | 10852 | `	sxu32 n;` |
|    14933 | 10853 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 10854 | `	/* Perform a linear search */` |
| 55730729 | 10855 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 55715803 | 10856 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 10857 | `			/* Already included */` |
|        7 | 10858 | `			return TRUE;` |
|        - | 10859 | `		}` |
| 27857899 | 10860 | `	}` |
|    14927 | 10861 | `	return FALSE;` |
|     7467 | 10862 |  |
|        - | 10863 | `/*` |
|        - | 10864 | ` * Push a file path in the appropriate VM container.` |
|        - | 10865 | ` */` |
|    16834 | 10866 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 10867 |  |
|        - | 10868 | `	SyString sPath;` |
|        - | 10869 | `	char *zDup;` |
|        - | 10870 | `#ifdef __WINNT__` |
|        - | 10871 | `	char *zCur;` |
|        - | 10872 | `#endif` |
|        - | 10873 | `	sxi32 rc;` |
|    16836 | 10874 | `	if( nLen < 0 ){` |
|     1904 | 10875 | `		nLen = SyStrlen(zPath);` |
|      951 | 10876 | `	}` |
|        - | 10877 | `	/* Duplicate the file path first */` |
|    16836 | 10878 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    16836 | 10879 | `	if( zDup == 0 ){` |
|      ! 0 | 10880 | `		return SXERR_MEM;` |
|        - | 10881 | `	}` |
|        - | 10882 | `#ifdef __WINNT__` |
|        - | 10883 | `	/* Normalize path on windows` |
|        - | 10884 | `	 * Example:` |
|        - | 10885 | `	 *    Path/To/File.php` |
|        - | 10886 | `	 * becomes` |
|        - | 10887 | `	 *   path\to\file.php` |
|        - | 10888 | `	 */` |
|        2 | 10889 | `	zCur = zDup;` |
|        2 | 10890 | `	while( zCur[0] != 0 ){` |
|        2 | 10891 | `		if( zCur[0] == '/' ){` |
|        2 | 10892 | `			zCur[0] = '\\';` |
|        2 | 10893 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 10894 | `			int c = SyToLower(zCur[0]);` |
|        1 | 10895 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 10896 | `		}` |
|        2 | 10897 | `		zCur++;` |
|        2 | 10898 | `	}` |
|        - | 10899 | `#endif` |
|        - | 10900 | `	/* Install the file path */` |
|    16836 | 10901 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    16836 | 10902 | `	if( !bMain ){` |
|    14933 | 10903 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 10904 | `			/* Already included */` |
|        7 | 10905 | `			*pNew = 0;` |
|        4 | 10906 | `		}else{` |
|        - | 10907 | `			/* Insert in the corresponding container */` |
|    14927 | 10908 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    14927 | 10909 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10910 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 10911 | `				return rc;` |
|        - | 10912 | `			}` |
|    14927 | 10913 | `			*pNew = 1;` |
|        - | 10914 | `		}` |
|     7466 | 10915 | `	}` |
|    16836 | 10916 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    16836 | 10917 | `	return SXRET_OK;` |
|     8419 | 10918 |  |
|        - | 10919 | `/*` |
|        - | 10920 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 10921 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 10922 | ` * indicates failure.` |
|        - | 10923 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 10924 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 10925 | ` * operations.` |
|        - | 10926 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 10927 | ` * this function is a no-op.` |
|        - | 10928 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 10929 | ` * constructs for more information.` |
|        - | 10930 | ` */` |
|     7474 | 10931 | `static sxi32 VmExecIncludedFile(` |
|        - | 10932 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 10933 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 10934 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 10935 | `	 )` |
|        2 | 10936 |  |
|        - | 10937 | `	sxi32 rc;` |
|        - | 10938 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10939 | `	const ph7_io_stream *pStream;` |
|        - | 10940 | `	SyBlob sContents;` |
|        - | 10941 | `	void *pHandle;` |
|        - | 10942 | `	ph7_vm *pVm;` |
|        - | 10943 | `	int isNew;` |
|        - | 10944 | `	/* Initialize fields */` |
|     7476 | 10945 | `	pVm = pCtx->pVm;` |
|     7476 | 10946 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7476 | 10947 | `	isNew = 0;` |
|        - | 10948 | `	/* Extract the associated stream */` |
|     7476 | 10949 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 10950 | `	/*` |
|        - | 10951 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 10952 | `	 * in a read-only mode.` |
|        - | 10953 | `	 */` |
|     7476 | 10954 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7476 | 10955 | `	if( pHandle == 0 ){` |
|        3 | 10956 | `		return SXERR_IO;` |
|        - | 10957 | `	}` |
|     7473 | 10958 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7473 | 10959 | `	if( IncludeOnce && !isNew ){` |
|        - | 10960 | `		/* Already included */` |
|        5 | 10961 | `		rc = SXERR_EXISTS;` |
|        3 | 10962 | `	}else{` |
|        - | 10963 | `		/* Read the whole file contents */` |
|     7469 | 10964 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7469 | 10965 | `		if( rc == SXRET_OK ){` |
|        - | 10966 | `			SyString sScript;` |
|        - | 10967 | `			/* Compile and execute the script */` |
|     7469 | 10968 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7469 | 10969 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3734 | 10970 | `		}` |
|        - | 10971 | `	}` |
|        - | 10972 | `	/* Pop from the set of included file */` |
|     7473 | 10973 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 10974 | `	/* Close the handle */` |
|     7473 | 10975 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 10976 | `	/* Release the working buffer */` |
|     7473 | 10977 | `	SyBlobRelease(&sContents);` |
|        - | 10978 | `#else` |
|        - | 10979 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 10980 | `	SXUNUSED(pPath);` |
|        - | 10981 | `	SXUNUSED(IncludeOnce);` |
|        - | 10982 | `	rc = SXERR_IO;` |
|        - | 10983 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7473 | 10984 | `	return rc;` |
|     3739 | 10985 |  |
|        - | 10986 | `/*` |
|        - | 10987 | ` * string get_include_path(void)` |
|        - | 10988 | ` *  Gets the current include_path configuration option.` |
|        - | 10989 | ` * Parameter` |
|        - | 10990 | ` *  None` |
|        - | 10991 | ` * Return` |
|        - | 10992 | ` *  Included paths as a string` |
|        - | 10993 | ` */` |
|        2 | 10994 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10995 |  |
|        3 | 10996 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10997 | `	SyString *aEntry;` |
|        - | 10998 | `	int dir_sep;` |
|        - | 10999 | `	sxu32 n;` |
|        - | 11000 | `#ifdef __WINNT__` |
|        1 | 11001 | `	dir_sep = ';';` |
|        - | 11002 | `#else` |
|        - | 11003 | `	/* Assume UNIX path separator */` |
|        2 | 11004 | `	dir_sep = ':';` |
|        - | 11005 | `#endif` |
|        1 | 11006 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11007 | `	SXUNUSED(apArg);` |
|        - | 11008 | `	/* Point to the list of import paths */` |
|        3 | 11009 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11010 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11011 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11012 | `		if( n > 0 ){` |
|        - | 11013 | `			/* Append dir seprator */` |
|      ! 0 | 11014 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11015 | `		}` |
|        - | 11016 | `		/* Append path */` |
|        3 | 11017 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11018 | `	}` |
|        3 | 11019 | `	return PH7_OK;` |
|        1 | 11020 |  |
|        - | 11021 | `/*` |
|        - | 11022 | ` * string get_get_included_files(void)` |
|        - | 11023 | ` *  Gets the current include_path configuration option.` |
|        - | 11024 | ` * Parameter` |
|        - | 11025 | ` *  None` |
|        - | 11026 | ` * Return` |
|        - | 11027 | ` *  Included paths as a string` |
|        - | 11028 | ` */` |
|        2 | 11029 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11030 |  |
|        3 | 11031 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 11032 | `	ph7_value *pArray,*pWorker;` |
|        - | 11033 | `	SyString *pEntry;` |
|        - | 11034 | `	int c,d;` |
|        - | 11035 | `	/* Create an array and a working value */` |
|        3 | 11036 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 11037 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11038 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 11039 | `		/* Out of memory,return null */` |
|      ! 0 | 11040 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11041 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11042 | `		SXUNUSED(apArg);` |
|      ! 0 | 11043 | `		return PH7_OK;` |
|        - | 11044 | `	}` |
|        3 | 11045 | `	c = d = '/';` |
|        - | 11046 | `#ifdef __WINNT__` |
|        1 | 11047 | `	d = '\\';` |
|        - | 11048 | `#endif` |
|        - | 11049 | `	/* Iterate throw entries */` |
|        3 | 11050 | `	SySetResetCursor(pFiles);` |
|     3627 | 11051 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 11052 | `		const char *zBase,*zEnd;` |
|        - | 11053 | `		int iLen;` |
|        - | 11054 | `		/* reset the string cursor */` |
|     3625 | 11055 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 11056 | `		/* Extract base name */` |
|     3625 | 11057 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 11058 | `		/* Ignore trailing '/' */` |
|     5437 | 11059 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 11060 | `			zEnd--;` |
|      ! 0 | 11061 | `		}` |
|     3625 | 11062 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   111273 | 11063 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   105837 | 11064 | `			zEnd--;` |
|        1 | 11065 | `		}` |
|     3625 | 11066 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3625 | 11067 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 11068 | `		/* Copy entry name */` |
|     3625 | 11069 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 11070 | `		/* Perform the insertion */` |
|     3625 | 11071 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 11072 | `	}` |
|        - | 11073 | `	/* All done,return the created array */` |
|        3 | 11074 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11075 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 11076 | `	 * by the engine as soon we return from this foreign` |
|        - | 11077 | `	 * function.` |
|        - | 11078 | `	 */` |
|        3 | 11079 | `	return PH7_OK;` |
|        2 | 11080 |  |
|        - | 11081 | `/*` |
|        - | 11082 | ` * include:` |
|        - | 11083 | ` * According to the PHP reference manual.` |
|        - | 11084 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 11085 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 11086 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 11087 | ` *  include() will finally check in the calling script's own directory` |
|        - | 11088 | ` *  and the current working directory before failing. The include()` |
|        - | 11089 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 11090 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 11091 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 11092 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 11093 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 11094 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 11095 | ` *  directory to find the requested file.` |
|        - | 11096 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 11097 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 11098 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 11099 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 11100 | ` */` |
|     7462 | 11101 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11102 |  |
|        - | 11103 | `	SyString sFile;` |
|        - | 11104 | `	sxi32 rc;` |
|     7464 | 11105 | `	if( nArg < 1 ){` |
|        - | 11106 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11107 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11108 | `		return SXRET_OK;` |
|        - | 11109 | `	}` |
|        - | 11110 | `	/* File to include */` |
|     7464 | 11111 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7464 | 11112 | `	if( sFile.nByte < 1 ){` |
|        - | 11113 | `		/* Empty string,return NULL */` |
|      ! 0 | 11114 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11115 | `		return SXRET_OK;` |
|        - | 11116 | `	}` |
|        - | 11117 | `	/* Open,compile and execute the desired script */` |
|     7464 | 11118 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7464 | 11119 | `	if( rc != SXRET_OK ){` |
|        - | 11120 | `		/* Emit a warning and return false */` |
|        3 | 11121 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 11122 | `		ph7_result_bool(pCtx,0);` |
|        1 | 11123 | `	}` |
|     7464 | 11124 | `	return SXRET_OK;` |
|     3733 | 11125 |  |
|        - | 11126 | `/*` |
|        - | 11127 | ` * include_once:` |
|        - | 11128 | ` *  According to the PHP reference manual.` |
|        - | 11129 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 11130 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 11131 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 11132 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 11133 | ` *   just once.` |
|        - | 11134 | ` */` |
|        4 | 11135 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11136 |  |
|        - | 11137 | `	SyString sFile;` |
|        - | 11138 | `	sxi32 rc;` |
|        5 | 11139 | `	if( nArg < 1 ){` |
|        - | 11140 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11141 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11142 | `		return SXRET_OK;` |
|        - | 11143 | `	}` |
|        - | 11144 | `	/* File to include */` |
|        5 | 11145 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11146 | `	if( sFile.nByte < 1 ){` |
|        - | 11147 | `		/* Empty string,return NULL */` |
|      ! 0 | 11148 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11149 | `		return SXRET_OK;` |
|        - | 11150 | `	}` |
|        - | 11151 | `	/* Open,compile and execute the desired script */` |
|        5 | 11152 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11153 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11154 | `		/* File already included,return TRUE */` |
|        3 | 11155 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11156 | `		return SXRET_OK;` |
|        - | 11157 | `	}` |
|        3 | 11158 | `	if( rc != SXRET_OK ){` |
|        - | 11159 | `		/* Emit a warning and return false */` |
|      ! 0 | 11160 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11161 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11162 | ` 	}` |
|        3 | 11163 | `	return SXRET_OK;` |
|        3 | 11164 |  |
|        - | 11165 | `/*` |
|        - | 11166 | ` * require.` |
|        - | 11167 | ` *  According to the PHP reference manual.` |
|        - | 11168 | ` *   require() is identical to include() except upon failure it will` |
|        - | 11169 | ` *   also produce a fatal level error.` |
|        - | 11170 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 11171 | ` *   emits a warning  which allows the script to continue.` |
|        - | 11172 | ` */` |
|        4 | 11173 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11174 |  |
|        - | 11175 | `	SyString sFile;` |
|        - | 11176 | `	sxi32 rc;` |
|        5 | 11177 | `	if( nArg < 1 ){` |
|        - | 11178 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11179 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11180 | `		return SXRET_OK;` |
|        - | 11181 | `	}` |
|        - | 11182 | `	/* File to include */` |
|        5 | 11183 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11184 | `	if( sFile.nByte < 1 ){` |
|        - | 11185 | `		/* Empty string,return NULL */` |
|      ! 0 | 11186 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11187 | `		return SXRET_OK;` |
|        - | 11188 | `	}` |
|        - | 11189 | `	/* Open,compile and execute the desired script */` |
|        5 | 11190 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 11191 | `	if( rc != SXRET_OK ){` |
|        - | 11192 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11193 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11194 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11195 | `		return PH7_ABORT;` |
|        - | 11196 | `	}` |
|        5 | 11197 | `	return SXRET_OK;` |
|        3 | 11198 |  |
|        - | 11199 | `/*` |
|        - | 11200 | ` * require_once:` |
|        - | 11201 | ` *  According to the PHP reference manual.` |
|        - | 11202 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 11203 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 11204 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 11205 | ` *   and how it differs from its non _once siblings.` |
|        - | 11206 | ` */` |
|        4 | 11207 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11208 |  |
|        - | 11209 | `	SyString sFile;` |
|        - | 11210 | `	sxi32 rc;` |
|        5 | 11211 | `	if( nArg < 1 ){` |
|        - | 11212 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11213 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11214 | `		return SXRET_OK;` |
|        - | 11215 | `	}` |
|        - | 11216 | `	/* File to include */` |
|        5 | 11217 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11218 | `	if( sFile.nByte < 1 ){` |
|        - | 11219 | `		/* Empty string,return NULL */` |
|      ! 0 | 11220 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11221 | `		return SXRET_OK;` |
|        - | 11222 | `	}` |
|        - | 11223 | `	/* Open,compile and execute the desired script */` |
|        5 | 11224 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11225 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11226 | `		/* File already included,return TRUE */` |
|        3 | 11227 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11228 | `		return SXRET_OK;` |
|        - | 11229 | `	}` |
|        3 | 11230 | `	if( rc != SXRET_OK ){` |
|        - | 11231 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11232 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11233 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11234 | `		return PH7_ABORT;` |
|        - | 11235 | `	}` |
|        3 | 11236 | `	return SXRET_OK;` |
|        3 | 11237 |  |
|        - | 11238 | `/*` |
|        - | 11239 | ` * Section:` |
|        - | 11240 | ` *  Command line arguments processing.` |
|        - | 11241 | ` * Status:` |
|        - | 11242 | ` *    Stable.` |
|        - | 11243 | ` */` |
|        - | 11244 | `/*` |
|        - | 11245 | ` * Check if a short option argument [i.e: -c] is available in the command` |
|        - | 11246 | ` * line string. Return a pointer to the start of the stream on success.` |
|        - | 11247 | ` * NULL otherwise.` |
|        - | 11248 | ` */` |
|        6 | 11249 | `static const char * VmFindShortOpt(int c,const char *zIn,const char *zEnd)` |
|        1 | 11250 |  |
|      319 | 11251 | `	while( zIn < zEnd ){` |
|      313 | 11252 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == c ){` |
|        - | 11253 | `			/* Got one */` |
|      ! 0 | 11254 | `			return &zIn[1];` |
|        - | 11255 | `		}` |
|        - | 11256 | `		/* Advance the cursor */` |
|      313 | 11257 | `		zIn++;` |
|        1 | 11258 | `	}` |
|        - | 11259 | `	/* No such option */` |
|        7 | 11260 | `	return 0;` |
|        4 | 11261 |  |
|        - | 11262 | `/*` |
|        - | 11263 | ` * Check if a long option argument [i.e: --opt] is available in the command` |
|        - | 11264 | ` * line string. Return a pointer to the start of the stream on success.` |
|        - | 11265 | ` * NULL otherwise.` |
|        - | 11266 | ` */` |
|      ! 0 | 11267 | `static const char * VmFindLongOpt(const char *zLong,int nByte,const char *zIn,const char *zEnd)` |
|      ! 0 | 11268 |  |
|        - | 11269 | `	const char *zOpt;` |
|      ! 0 | 11270 | `	while( zIn < zEnd ){` |
|      ! 0 | 11271 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == '-' ){` |
|      ! 0 | 11272 | `			zIn += 2;` |
|      ! 0 | 11273 | `			zOpt = zIn;` |
|      ! 0 | 11274 | `			while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 11275 | `				if( zIn[0] == '=' /* --opt=val */){` |
|      ! 0 | 11276 | `					break;` |
|        - | 11277 | `				}` |
|      ! 0 | 11278 | `				zIn++;` |
|      ! 0 | 11279 | `			}` |
|        - | 11280 | `			/* Test */` |
|      ! 0 | 11281 | `			if( (int)(zIn-zOpt) == nByte && SyMemcmp(zOpt,zLong,nByte) == 0 ){` |
|        - | 11282 | `				/* Got one,return it's value */` |
|      ! 0 | 11283 | `				return zIn;` |
|        - | 11284 | `			}` |
|        - | 11285 |  |
|      ! 0 | 11286 | `		}else{` |
|      ! 0 | 11287 | `			zIn++;` |
|        - | 11288 | `		}` |
|      ! 0 | 11289 | `	}` |
|        - | 11290 | `	/* No such option */` |
|      ! 0 | 11291 | `	return 0;` |
|      ! 0 | 11292 |  |
|        - | 11293 | `/*` |
|        - | 11294 | ` * Long option [i.e: --opt] arguments private data structure.` |
|        - | 11295 | ` */` |
|        - | 11296 | `struct getopt_long_opt` |
|        - | 11297 |  |
|        - | 11298 | `	const char *zArgIn,*zArgEnd; /* Command line arguments */` |
|        - | 11299 | `	ph7_value *pWorker;  /* Worker variable*/` |
|        - | 11300 | `	ph7_value *pArray;   /* getopt() return value */` |
|        - | 11301 | `	ph7_context *pCtx;   /* Call Context */` |
|        - | 11302 | `};` |
|        - | 11303 | `/* Forward declaration */` |
|        - | 11304 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11305 | `/*` |
|        - | 11306 | ` * Extract short or long argument option values.` |
|        - | 11307 | ` */` |
|      ! 0 | 11308 | `static void VmExtractOptArgValue(` |
|        - | 11309 | `	ph7_value *pArray,  /* getopt() return value */` |
|        - | 11310 | `	ph7_value *pWorker, /* Worker variable */` |
|        - | 11311 | `	const char *zArg,   /* Argument stream */` |
|        - | 11312 | `	const char *zArgEnd,/* End of the argument stream  */` |
|        - | 11313 | `	int need_val,       /* TRUE to fetch option argument */` |
|        - | 11314 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11315 | `	const char *zName   /* Option name */)` |
|      ! 0 | 11316 |  |
|      ! 0 | 11317 | `	ph7_value_bool(pWorker,0);` |
|      ! 0 | 11318 | `	if( !need_val ){` |
|        - | 11319 | `		/*` |
|        - | 11320 | `		 * Option does not need arguments.` |
|        - | 11321 | `		 * Insert the option name and a boolean FALSE.` |
|        - | 11322 | `		 */` |
|      ! 0 | 11323 | `		ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11324 | `	}else{` |
|        - | 11325 | `		const char *zCur;` |
|        - | 11326 | `		/* Extract option argument */` |
|      ! 0 | 11327 | `		zArg++;` |
|      ! 0 | 11328 | `		if( zArg < zArgEnd && zArg[0] == '=' ){` |
|      ! 0 | 11329 | `			zArg++;` |
|      ! 0 | 11330 | `		}` |
|      ! 0 | 11331 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11332 | `			zArg++;` |
|      ! 0 | 11333 | `		}` |
|      ! 0 | 11334 | `		if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|        - | 11335 | `			/*` |
|        - | 11336 | `			 * Argument not found.` |
|        - | 11337 | `			 * Insert the option name and a boolean FALSE.` |
|        - | 11338 | `			 */` |
|      ! 0 | 11339 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11340 | `			return;` |
|        - | 11341 | `		}` |
|        - | 11342 | `		/* Delimit the value */` |
|      ! 0 | 11343 | `		zCur = zArg;` |
|      ! 0 | 11344 | `		if( zArg[0] == '\'' \|\| zArg[0] == '"' ){` |
|      ! 0 | 11345 | `			int d = zArg[0];` |
|        - | 11346 | `			/* Delimt the argument */` |
|      ! 0 | 11347 | `			zArg++;` |
|      ! 0 | 11348 | `			zCur = zArg;` |
|      ! 0 | 11349 | `			while( zArg < zArgEnd ){` |
|      ! 0 | 11350 | `				if( zArg[0] == d && zArg[-1] != '\\' ){` |
|        - | 11351 | `					/* Delimiter found,exit the loop  */` |
|      ! 0 | 11352 | `					break;` |
|        - | 11353 | `				}` |
|      ! 0 | 11354 | `				zArg++;` |
|      ! 0 | 11355 | `			}` |
|        - | 11356 | `			/* Save the value */` |
|      ! 0 | 11357 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|      ! 0 | 11358 | `			if( zArg < zArgEnd ){ zArg++; }` |
|      ! 0 | 11359 | `		}else{` |
|      ! 0 | 11360 | `			while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|      ! 0 | 11361 | `				zArg++;` |
|      ! 0 | 11362 | `			}` |
|        - | 11363 | `			/* Save the value */` |
|      ! 0 | 11364 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|        - | 11365 | `		}` |
|        - | 11366 | `		/*` |
|        - | 11367 | `		 * Check if we are dealing with multiple values.` |
|        - | 11368 | `		 * If so,create an array to hold them,rather than a scalar variable.` |
|        - | 11369 | `		 */` |
|      ! 0 | 11370 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11371 | `			zArg++;` |
|      ! 0 | 11372 | `		}` |
|      ! 0 | 11373 | `		if( zArg < zArgEnd && zArg[0] != '-' ){` |
|        - | 11374 | `			ph7_value *pOptArg; /* Array of option arguments */` |
|      ! 0 | 11375 | `			pOptArg = ph7_context_new_array(pCtx);` |
|      ! 0 | 11376 | `			if( pOptArg == 0 ){` |
|      ! 0 | 11377 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11378 | `			}else{` |
|        - | 11379 | `				/* Insert the first value */` |
|      ! 0 | 11380 | `				ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11381 | `				for(;;){` |
|      ! 0 | 11382 | `					if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|        - | 11383 | `						/* No more value */` |
|      ! 0 | 11384 | `						break;` |
|        - | 11385 | `					}` |
|        - | 11386 | `					/* Delimit the value */` |
|      ! 0 | 11387 | `					zCur = zArg;` |
|      ! 0 | 11388 | `					if( zArg < zArgEnd && zArg[0] == '\\' ){` |
|      ! 0 | 11389 | `						zArg++;` |
|      ! 0 | 11390 | `						zCur = zArg;` |
|      ! 0 | 11391 | `					}` |
|      ! 0 | 11392 | `					while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|      ! 0 | 11393 | `						zArg++;` |
|      ! 0 | 11394 | `					}` |
|        - | 11395 | `					/* Reset the string cursor */` |
|      ! 0 | 11396 | `					ph7_value_reset_string_cursor(pWorker);` |
|        - | 11397 | `					/* Save the value */` |
|      ! 0 | 11398 | `					ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|        - | 11399 | `					/* Insert */` |
|      ! 0 | 11400 | `					ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|        - | 11401 | `					/* Jump trailing white spaces */` |
|      ! 0 | 11402 | `					while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11403 | `						zArg++;` |
|      ! 0 | 11404 | `					}` |
|      ! 0 | 11405 | `				}` |
|        - | 11406 | `				/* Insert the option arg array */` |
|      ! 0 | 11407 | `				ph7_array_add_strkey_elem(pArray,(const char *)zName,pOptArg); /* Will make it's own copy */` |
|        - | 11408 | `				/* Safely release */` |
|      ! 0 | 11409 | `				ph7_context_release_value(pCtx,pOptArg);` |
|        - | 11410 | `			}` |
|      ! 0 | 11411 | `		}else{` |
|        - | 11412 | `			/* Single value */` |
|      ! 0 | 11413 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|        - | 11414 | `		}` |
|        - | 11415 | `	}` |
|      ! 0 | 11416 |  |
|        - | 11417 | `/*` |
|        - | 11418 | ` * array getopt(string $options[,array $longopts ])` |
|        - | 11419 | ` *   Gets options from the command line argument list.` |
|        - | 11420 | ` * Parameters` |
|        - | 11421 | ` *  $options` |
|        - | 11422 | ` *   Each character in this string will be used as option characters` |
|        - | 11423 | ` *   and matched against options passed to the script starting with` |
|        - | 11424 | ` *   a single hyphen (-). For example, an option string "x" recognizes` |
|        - | 11425 | ` *   an option -x. Only a-z, A-Z and 0-9 are allowed.` |
|        - | 11426 | ` *  $longopts` |
|        - | 11427 | ` *   An array of options. Each element in this array will be used as option` |
|        - | 11428 | ` *   strings and matched against options passed to the script starting with` |
|        - | 11429 | ` *   two hyphens (--). For example, an longopts element "opt" recognizes an` |
|        - | 11430 | ` *   option --opt.` |
|        - | 11431 | ` * Return` |
|        - | 11432 | ` *  This function will return an array of option / argument pairs or FALSE` |
|        - | 11433 | ` *  on failure.` |
|        - | 11434 | ` */` |
|        2 | 11435 | `static int vm_builtin_getopt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11436 |  |
|        - | 11437 | `	const char *zIn,*zEnd,*zArg,*zArgIn,*zArgEnd;` |
|        - | 11438 | `	struct getopt_long_opt sLong;` |
|        - | 11439 | `	ph7_value *pArray,*pWorker;` |
|        - | 11440 | `	SyBlob *pArg;` |
|        - | 11441 | `	int nByte;` |
|        3 | 11442 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11443 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 11444 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Missing/Invalid option arguments");` |
|      ! 0 | 11445 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11446 | `		return PH7_OK;` |
|        - | 11447 | `	}` |
|        - | 11448 | `	/* Extract option arguments */` |
|        3 | 11449 | `	zIn  = ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 11450 | `	zEnd = &zIn[nByte];` |
|        - | 11451 | `	/* Point to the string representation of the $argv[] array */` |
|        3 | 11452 | `	pArg = &pCtx->pVm->sArgv;` |
|        - | 11453 | `	/* Create a new empty array and a worker variable */` |
|        3 | 11454 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11455 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11456 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|      ! 0 | 11457 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11458 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11459 | `		return PH7_OK;` |
|        - | 11460 | `	}` |
|        3 | 11461 | `	if( SyBlobLength(pArg) < 1 ){` |
|        - | 11462 | `		/* Empty command line,return the empty array*/` |
|      ! 0 | 11463 | `		ph7_result_value(pCtx,pArray);` |
|        - | 11464 | `		/* Everything will be released automatically when we return` |
|        - | 11465 | `		 * from this function.` |
|        - | 11466 | `		 */` |
|      ! 0 | 11467 | `		return PH7_OK;` |
|        - | 11468 | `	}` |
|        3 | 11469 | `	zArgIn = (const char *)SyBlobData(pArg);` |
|        3 | 11470 | `	zArgEnd = &zArgIn[SyBlobLength(pArg)];` |
|        - | 11471 | `	/* Fill the long option structure */` |
|        3 | 11472 | `	sLong.pArray = pArray;` |
|        3 | 11473 | `	sLong.pWorker = pWorker;` |
|        3 | 11474 | `	sLong.zArgIn =  zArgIn;` |
|        3 | 11475 | `	sLong.zArgEnd = zArgEnd;` |
|        3 | 11476 | `	sLong.pCtx = pCtx;` |
|        - | 11477 | `	/* Start processing */` |
|        9 | 11478 | `	while( zIn < zEnd ){` |
|        7 | 11479 | `		int c = zIn[0];` |
|        7 | 11480 | `		int need_val = 0;` |
|        - | 11481 | `		/* Advance the stream cursor */` |
|        7 | 11482 | `		zIn++;` |
|        - | 11483 | `		/* Ignore non-alphanum characters */` |
|        7 | 11484 | `		if( !SyisAlphaNum(c) ){` |
|      ! 0 | 11485 | `			continue;` |
|        - | 11486 | `		}` |
|        7 | 11487 | `		if( zIn < zEnd && zIn[0] == ':' ){` |
|        5 | 11488 | `			zIn++;` |
|        5 | 11489 | `			need_val = 1;` |
|        5 | 11490 | `			if( zIn < zEnd && zIn[0] == ':' ){` |
|      ! 0 | 11491 | `				zIn++;` |
|      ! 0 | 11492 | `			}` |
|        2 | 11493 | `		}` |
|        - | 11494 | `		/* Find option */` |
|        7 | 11495 | `		zArg = VmFindShortOpt(c,zArgIn,zArgEnd);` |
|        7 | 11496 | `		if( zArg == 0 ){` |
|        - | 11497 | `			/* No such option */` |
|        7 | 11498 | `			continue;` |
|        - | 11499 | `		}` |
|        - | 11500 | `		/* Extract option argument value */` |
|      ! 0 | 11501 | `		VmExtractOptArgValue(pArray,pWorker,zArg,zArgEnd,need_val,pCtx,(const char *)&c);` |
|      ! 0 | 11502 | `	}` |
|        3 | 11503 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) && ph7_array_count(apArg[1]) > 0 ){` |
|        - | 11504 | `		/* Process long options */` |
|      ! 0 | 11505 | `		ph7_array_walk(apArg[1],VmProcessLongOpt,&sLong);` |
|      ! 0 | 11506 | `	}` |
|        - | 11507 | `	/* Return the option array */` |
|        3 | 11508 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11509 | `	/*` |
|        - | 11510 | `	 * Don't worry about freeing memory, everything will be released` |
|        - | 11511 | `	 * automatically as soon we return from this foreign function.` |
|        - | 11512 | `	 */` |
|        3 | 11513 | `	return PH7_OK;` |
|        2 | 11514 |  |
|        - | 11515 | `/*` |
|        - | 11516 | ` * Array walker callback used for processing long options values.` |
|        - | 11517 | ` */` |
|      ! 0 | 11518 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11519 |  |
|      ! 0 | 11520 | `	struct getopt_long_opt *pOpt = (struct getopt_long_opt *)pUserData;` |
|        - | 11521 | `	const char *zArg,*zOpt,*zEnd;` |
|      ! 0 | 11522 | `	int need_value = 0;` |
|        - | 11523 | `	int nByte;` |
|        - | 11524 | `	/* Value must be of type string */` |
|      ! 0 | 11525 | `	if( !ph7_value_is_string(pValue) ){` |
|        - | 11526 | `		/* Simply ignore */` |
|      ! 0 | 11527 | `		return PH7_OK;` |
|        - | 11528 | `	}` |
|      ! 0 | 11529 | `	zOpt = ph7_value_to_string(pValue,&nByte);` |
|      ! 0 | 11530 | `	if( nByte < 1 ){` |
|        - | 11531 | `		/* Empty string,ignore */` |
|      ! 0 | 11532 | `		return PH7_OK;` |
|        - | 11533 | `	}` |
|      ! 0 | 11534 | `	zEnd = &zOpt[nByte - 1];` |
|      ! 0 | 11535 | `	if( zEnd[0] == ':' ){` |
|        - | 11536 | `		char *zTerm;` |
|        - | 11537 | `		/* Try to extract a value */` |
|      ! 0 | 11538 | `		need_value = 1;` |
|      ! 0 | 11539 | `		while( zEnd >= zOpt && zEnd[0] == ':' ){` |
|      ! 0 | 11540 | `			zEnd--;` |
|      ! 0 | 11541 | `		}` |
|      ! 0 | 11542 | `		if( zOpt >= zEnd ){` |
|        - | 11543 | `			/* Empty string,ignore */` |
|      ! 0 | 11544 | `			SXUNUSED(pKey);` |
|      ! 0 | 11545 | `			return PH7_OK;` |
|        - | 11546 | `		}` |
|      ! 0 | 11547 | `		zEnd++;` |
|      ! 0 | 11548 | `		zTerm = (char *)zEnd;` |
|      ! 0 | 11549 | `		zTerm[0] = 0;` |
|      ! 0 | 11550 | `	}else{` |
|      ! 0 | 11551 | `		zEnd = &zOpt[nByte];` |
|        - | 11552 | `	}` |
|        - | 11553 | `	/* Find the option */` |
|      ! 0 | 11554 | `	zArg = VmFindLongOpt(zOpt,(int)(zEnd-zOpt),pOpt->zArgIn,pOpt->zArgEnd);` |
|      ! 0 | 11555 | `	if( zArg == 0 ){` |
|        - | 11556 | `		/* No such option,return immediately */` |
|      ! 0 | 11557 | `		return PH7_OK;` |
|        - | 11558 | `	}` |
|        - | 11559 | `	/* Try to extract a value */` |
|      ! 0 | 11560 | `	VmExtractOptArgValue(pOpt->pArray,pOpt->pWorker,zArg,pOpt->zArgEnd,need_value,pOpt->pCtx,zOpt);` |
|      ! 0 | 11561 | `	return PH7_OK;` |
|      ! 0 | 11562 |  |
|        - | 11563 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 11564 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 11565 | `/* Table of built-in VM functions. */` |
|        - | 11566 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 11567 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 11568 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 11569 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 11570 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 11571 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 11572 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 11573 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 11574 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 11575 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 11576 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 11577 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 11578 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 11579 | `	    /* Constants management */` |
|        - | 11580 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 11581 | `	{ "define",   vm_builtin_define               },` |
|        - | 11582 | `	{ "constant", vm_builtin_constant             },` |
|        - | 11583 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 11584 | `	   /* Class/Object functions */` |
|        - | 11585 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 11586 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 11587 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 11588 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 11589 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 11590 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 11591 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 11592 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 11593 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 11594 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 11595 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 11596 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 11597 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 11598 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 11599 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 11600 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 11601 | `	   /* Random numbers/strings generators */` |
|        - | 11602 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 11603 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 11604 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 11605 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 11606 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 11607 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11608 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 11609 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 11610 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 11611 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 11612 | `	   /* Language constructs functions */` |
|        - | 11613 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 11614 | `	{ "print", vm_builtin_print                   },` |
|        - | 11615 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 11616 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 11617 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 11618 | `	  /* Variable handling functions */` |
|        - | 11619 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 11620 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 11621 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 11622 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 11623 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 11624 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 11625 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 11626 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 11627 | `	  /* Ouput control functions */` |
|        - | 11628 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 11629 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 11630 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 11631 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 11632 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 11633 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 11634 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 11635 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 11636 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 11637 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 11638 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 11639 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 11640 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 11641 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 11642 | `	  /* Assertion functions */` |
|        - | 11643 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 11644 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 11645 | `	  /* Error reporting functions */` |
|        - | 11646 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 11647 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 11648 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 11649 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 11650 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 11651 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 11652 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 11653 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 11654 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 11655 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 11656 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 11657 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 11658 | `	  /* Release info */` |
|        - | 11659 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 11660 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 11661 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 11662 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 11663 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 11664 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 11665 | `	  /* hashmap */` |
|        - | 11666 | `	{"compact",          vm_builtin_compact       },` |
|        - | 11667 | `	{"extract",          vm_builtin_extract       },` |
|        - | 11668 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 11669 | `	  /* URL related function */` |
|        - | 11670 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 11671 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 11672 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11673 | `	   /* XML processing functions */` |
|        - | 11674 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 11675 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 11676 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 11677 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 11678 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 11679 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 11680 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 11681 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 11682 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 11683 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 11684 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 11685 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 11686 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 11687 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 11688 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 11689 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 11690 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 11691 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 11692 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 11693 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 11694 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 11695 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 11696 | `	   /* UTF-8 encoding/decoding */` |
|        - | 11697 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 11698 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 11699 | `	   /* Command line processing */` |
|        - | 11700 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 11701 | `	   /* JSON encoding/decoding */` |
|        - | 11702 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 11703 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 11704 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 11705 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 11706 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 11707 | `	   /* Files/URI inclusion facility */` |
|        - | 11708 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 11709 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 11710 | `	{ "include",      vm_builtin_include          },` |
|        - | 11711 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 11712 | `	{ "require",      vm_builtin_require          },` |
|        - | 11713 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 11714 | `};` |
|        - | 11715 | `/*` |
|        - | 11716 | ` * Register the built-in VM functions defined above.` |
|        - | 11717 | ` */` |
|     1672 | 11718 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 11719 |  |
|        - | 11720 | `	sxi32 rc;` |
|        - | 11721 | `	sxu32 n;` |
|   209002 | 11722 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 11723 | `		/* Note that these special functions have access` |
|        - | 11724 | `		 * to the underlying virtual machine as their` |
|        - | 11725 | `		 * private data.` |
|        - | 11726 | `		 */` |
|   207330 | 11727 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   207330 | 11728 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11729 | `			return rc;` |
|        - | 11730 | `		}` |
|   103666 | 11731 | `	}` |
|     1674 | 11732 | `	return SXRET_OK;` |
|      838 | 11733 |  |
|        - | 11734 | `/*` |
|        - | 11735 | ` * Check if the given name refer to an installed class.` |
|        - | 11736 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 11737 | ` */` |
|    10604 | 11738 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 11739 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 11740 | `	const char *zName,  /* Name of the target class */` |
|        - | 11741 | `	sxu32 nByte,        /* zName length */` |
|        - | 11742 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 11743 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 11744 | `						 */` |
|        - | 11745 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 11746 | `	)` |
|        2 | 11747 |  |
|        - | 11748 | `	SyHashEntry *pEntry;` |
|        - | 11749 | `	ph7_class *pClass;` |
|     5302 | 11750 | `		SXUNUSED(iNest);` |
|        - | 11751 | `	/* Perform a hash lookup */` |
|    10606 | 11752 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|        - | 11753 |  |
|    10606 | 11754 | `	if( pEntry == 0 ){` |
|        - | 11755 | `		/* No such entry,return NULL */` |
|      ! 0 | 11756 | `		return 0;` |
|        - | 11757 | `	}` |
|    10606 | 11758 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    10606 | 11759 | `	if( !iLoadable ){` |
|        - | 11760 | `		/* Return the first class seen */` |
|     9714 | 11761 | `		return pClass;` |
|      ! 0 | 11762 | `	}else{` |
|        - | 11763 | `		/* Check the collision list */` |
|      894 | 11764 | `		while(pClass){` |
|      894 | 11765 | `			if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) == 0 ){` |
|        - | 11766 | `				/* Class is loadable */` |
|      894 | 11767 | `				return pClass;` |
|        - | 11768 | `			}` |
|        - | 11769 | `			/* Point to the next entry */` |
|      ! 0 | 11770 | `			pClass = pClass->pNextName;` |
|      ! 0 | 11771 | `		}` |
|        - | 11772 | `	}` |
|        - | 11773 | `	/* No such loadable class */` |
|      ! 0 | 11774 | `	return 0;` |
|     5304 | 11775 |  |
|        - | 11776 | `/*` |
|        - | 11777 | ` * Reference Table Implementation` |
|        - | 11778 | ` * Status: stable <chm@symisc.net>` |
|        - | 11779 | ` * Intro` |
|        - | 11780 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 11781 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 11782 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 11783 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 11784 | ` *  Refer to the official for more information on this powerful` |
|        - | 11785 | ` *  extension.` |
|        - | 11786 | ` */` |
|        - | 11787 | `/*` |
|        - | 11788 | ` * Allocate a new reference entry.` |
|        - | 11789 | ` */` |
|  2938686 | 11790 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 11791 |  |
|        - | 11792 | `	VmRefObj *pRef;` |
|        - | 11793 | `	/* Allocate a new instance */` |
|  2938688 | 11794 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  2938688 | 11795 | `	if( pRef == 0 ){` |
|      ! 0 | 11796 | `		return 0;` |
|        - | 11797 | `	}` |
|        - | 11798 | `	/* Zero the structure */` |
|  2938688 | 11799 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 11800 | `	/* Initialize fields */` |
|  2938688 | 11801 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  2938688 | 11802 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  2938688 | 11803 | `	pRef->nIdx = nIdx;` |
|  2938688 | 11804 | `	return pRef;` |
|  1469345 | 11805 |  |
|        - | 11806 | `/*` |
|        - | 11807 | ` * Default hash function used by the reference table` |
|        - | 11808 | ` * for lookup/insertion operations.` |
|        - | 11809 | ` */` |
| 16369942 | 11810 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 11811 |  |
|        - | 11812 | `	/* Calculate the hash based on the memory object index */` |
| 16369944 | 11813 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 11814 |  |
|        - | 11815 | `/*` |
|        - | 11816 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 11817 | ` * in the reference table.` |
|        - | 11818 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 11819 | ` * otherwise.` |
|        - | 11820 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 11821 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 11822 | ` * the reference implementation is consistent,solid and it's` |
|        - | 11823 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 11824 | ` * Refer to the official for more information on this powerful` |
|        - | 11825 | ` * extension.` |
|        - | 11826 | ` */` |
|  8781172 | 11827 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 11828 |  |
|        - | 11829 | `	VmRefObj *pRef;` |
|        - | 11830 | `	sxu32 nBucket;` |
|        - | 11831 | `	/* Point to the appropriate bucket */` |
|  8781174 | 11832 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 11833 | `	/* Perform the lookup */` |
|  8781174 | 11834 | `	pRef = pVm->apRefObj[nBucket];` |
| 18519249 | 11835 | `	for(;;){` |
| 37040129 | 11836 | `		if( pRef == 0 ){` |
|  3005734 | 11837 | `			break;` |
|        - | 11838 | `		}` |
| 34034397 | 11839 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 11840 | `			/* Entry found */` |
|  5775442 | 11841 | `			return pRef;` |
|        - | 11842 | `		}` |
|        - | 11843 | `		/* Point to the next entry */` |
| 28258957 | 11844 | `		pRef = pRef->pNextCollide;` |
|        2 | 11845 | `	}` |
|        - | 11846 | `	/* No such entry,return NULL */` |
|  3005734 | 11847 | `	return 0;` |
|  4390588 | 11848 |  |
|        - | 11849 | `/*` |
|        - | 11850 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 11851 | ` *` |
|        - | 11852 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 11853 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 11854 | ` * the reference implementation is consistent,solid and it's` |
|        - | 11855 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 11856 | ` * Refer to the official for more information on this powerful` |
|        - | 11857 | ` * extension.` |
|        - | 11858 | ` */` |
|  2938686 | 11859 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 11860 |  |
|        - | 11861 | `	sxu32 nBucket;` |
|  2938688 | 11862 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 11863 | `		VmRefObj **apNew;` |
|        - | 11864 | `		sxu32 nNew;` |
|        - | 11865 | `		/* Allocate a larger table */` |
|     2572 | 11866 | `		nNew = pVm->nRefSize << 1;` |
|     2572 | 11867 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     2572 | 11868 | `		if( apNew ){` |
|     2572 | 11869 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 11870 | `			sxu32 n;` |
|        - | 11871 | `			/* Zero the structure */` |
|     2572 | 11872 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 11873 | `			/* Rehash all referenced entries */` |
|  2825344 | 11874 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 11875 | `				/* Remove old collision links */` |
|  2822774 | 11876 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 11877 | `				/* Point to the appropriate bucket */` |
|  2822774 | 11878 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 11879 | `				/* Insert the entry  */` |
|  2822774 | 11880 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2822774 | 11881 | `				if( apNew[nBucket] ){` |
|  2298896 | 11882 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 11883 | `				}` |
|  2822774 | 11884 | `				apNew[nBucket] = pEntry;` |
|        - | 11885 | `				/* Point to the next entry */` |
|  2822774 | 11886 | `				pEntry = pEntry->pNext;` |
|  1411388 | 11887 | `			}` |
|        - | 11888 | `			/* Release the old table */` |
|     2572 | 11889 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 11890 | `			/* Install the new one */` |
|     2572 | 11891 | `			pVm->apRefObj = apNew;` |
|     2572 | 11892 | `			pVm->nRefSize = nNew;` |
|     1285 | 11893 | `		}` |
|     1285 | 11894 | `	}` |
|        - | 11895 | `	/* Point to the appropriate bucket */` |
|  2938688 | 11896 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 11897 | `	/* Insert the entry */` |
|  2938688 | 11898 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  2938688 | 11899 | `	if( pVm->apRefObj[nBucket] ){` |
|  2431871 | 11900 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1216413 | 11901 | `	}` |
|  2938688 | 11902 | `	pVm->apRefObj[nBucket] = pRef;` |
|  2938688 | 11903 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  2938688 | 11904 | `	pVm->nRefUsed++;` |
|  2938688 | 11905 | `	return SXRET_OK;` |
|        2 | 11906 |  |
|        - | 11907 | `/*` |
|        - | 11908 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 11909 | ` * the reference table.` |
|        - | 11910 | ` * This function is invoked when the user perform an unset` |
|        - | 11911 | ` * call [i.e: unset($var); ].` |
|        - | 11912 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 11913 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 11914 | ` * the reference implementation is consistent,solid and it's` |
|        - | 11915 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 11916 | ` * Refer to the official for more information on this powerful` |
|        - | 11917 | ` * extension.` |
|        - | 11918 | ` */` |
|  2914044 | 11919 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 11920 |  |
|        - | 11921 | `	ph7_hashmap_node **apNode;` |
|        - | 11922 | `	SyHashEntry **apEntry;` |
|        - | 11923 | `	sxu32 n;` |
|        - | 11924 | `	/* Point to the reference table */` |
|  2914046 | 11925 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2914046 | 11926 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 11927 | `	/* Unlink the entry from the reference table */` |
|  2986014 | 11928 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    71970 | 11929 | `		if( apEntry[n] ){` |
|    71920 | 11930 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    35959 | 11931 | `		}` |
|    35986 | 11932 | `	}` |
|  5758678 | 11933 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2844634 | 11934 | `		if( apNode[n] ){` |
|     5595 | 11935 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2797 | 11936 | `		}` |
|  1422318 | 11937 | `	}` |
|  2914046 | 11938 | `	if( pRef->pPrevCollide ){` |
|  1086734 | 11939 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   543348 | 11940 | `	}else{` |
|  1827314 | 11941 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 11942 | `	}` |
|  2914046 | 11943 | `	if( pRef->pNextCollide ){` |
|  1626659 | 11944 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   813838 | 11945 | `	}` |
|  2914046 | 11946 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 11947 | `	/* Release the node */` |
|  2914046 | 11948 | `	SySetRelease(&pRef->aReference);` |
|  2914046 | 11949 | `	SySetRelease(&pRef->aArrEntries);` |
|  2914046 | 11950 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2914046 | 11951 | `	pVm->nRefUsed--;` |
|  2914046 | 11952 | `	return SXRET_OK;` |
|        2 | 11953 |  |
|        - | 11954 | `/*` |
|        - | 11955 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 11956 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 11957 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 11958 | ` * the reference implementation is consistent,solid and it's` |
|        - | 11959 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 11960 | ` * Refer to the official for more information on this powerful` |
|        - | 11961 | ` * extension.` |
|        - | 11962 | ` */` |
|  2960988 | 11963 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 11964 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 11965 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 11966 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 11967 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 11968 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 11969 | `	)` |
|        2 | 11970 |  |
|  2960990 | 11971 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 11972 | `	VmRefObj *pRef;` |
|        - | 11973 | `	/* Check if the referenced object already exists */` |
|  2960990 | 11974 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2960990 | 11975 | `	if( pRef == 0 ){` |
|        - | 11976 | `		/* Create a new entry */` |
|  2938688 | 11977 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  2938688 | 11978 | `		if( pRef == 0 ){` |
|      ! 0 | 11979 | `			return SXERR_MEM;` |
|        - | 11980 | `		}` |
|  2938688 | 11981 | `		pRef->iFlags = iFlags;` |
|        - | 11982 | `		/* Install the entry */` |
|  2938688 | 11983 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1469343 | 11984 | `	}` |
|  2965902 | 11985 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 11986 | `		/* Safely ignore the exception frame */` |
|     4914 | 11987 | `		pFrame = pFrame->pParent;` |
|        2 | 11988 | `	}` |
|  2960990 | 11989 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 11990 | `		VmSlot sRef;` |
|        - | 11991 | `		/* Local frame,record referenced entry so that it can` |
|        - | 11992 | `		 * be deleted when we leave this frame.` |
|        - | 11993 | `		 */` |
|    67078 | 11994 | `		sRef.nIdx = nIdx;` |
|    67078 | 11995 | `		sRef.pUserData = pEntry;` |
|    67078 | 11996 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 11997 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 11998 | `		}` |
|    33538 | 11999 | `	}` |
|  2960990 | 12000 | `	if( pEntry ){` |
|        - | 12001 | `		/* Address of the hash-entry */` |
|    89194 | 12002 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    44596 | 12003 | `	}` |
|  2960990 | 12004 | `	if( pMapEntry ){` |
|        - | 12005 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2867730 | 12006 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1433864 | 12007 | `	}` |
|  2960990 | 12008 | `	return SXRET_OK;` |
|  1480496 | 12009 |  |
|        - | 12010 | `/*` |
|        - | 12011 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 12012 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12013 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12014 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12015 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12016 | ` * Refer to the official for more information on this powerful` |
|        - | 12017 | ` * extension.` |
|        - | 12018 | ` */` |
|  2906120 | 12019 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 12020 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12021 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12022 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12023 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 12024 | `	)` |
|        2 | 12025 |  |
|        - | 12026 | `	VmRefObj *pRef;` |
|        - | 12027 | `	sxu32 n;` |
|        - | 12028 | `	/* Check if the referenced object already exists */` |
|  2906122 | 12029 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2906122 | 12030 | `	if( pRef == 0 ){` |
|        - | 12031 | `		/* Not such entry */` |
|    67028 | 12032 | `		return SXERR_NOTFOUND;` |
|        - | 12033 | `	}` |
|        - | 12034 | `	/* Remove the desired entry */` |
|  2839096 | 12035 | `	if( pEntry ){` |
|        - | 12036 | `		SyHashEntry **apEntry;` |
|       51 | 12037 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      195 | 12038 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      145 | 12039 | `			if( apEntry[n] == pEntry ){` |
|        - | 12040 | `				/* Nullify the entry */` |
|       51 | 12041 | `				apEntry[n] = 0;` |
|        - | 12042 | `				/*` |
|        - | 12043 | `				 * NOTE:` |
|        - | 12044 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 12045 | `				 * we avoid wasting spaces.` |
|        - | 12046 | `				 */` |
|       25 | 12047 | `			}` |
|       73 | 12048 | `		}` |
|       25 | 12049 | `	}` |
|  2839096 | 12050 | `	if( pMapEntry ){` |
|        - | 12051 | `		ph7_hashmap_node **apNode;` |
|  2839046 | 12052 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5678178 | 12053 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2839134 | 12054 | `			if( apNode[n] == pMapEntry ){` |
|        - | 12055 | `				/* nullify the entry */` |
|  2839046 | 12056 | `				apNode[n] = 0;` |
|  1419522 | 12057 | `			}` |
|  1419568 | 12058 | `		}` |
|  1419522 | 12059 | `	}` |
|  2839096 | 12060 | `	return SXRET_OK;` |
|  1453062 | 12061 |  |
|        - | 12062 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 12063 | `/*` |
|        - | 12064 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 12065 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 12066 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 12067 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 12068 | ` * For more information on how to register IO stream devices,please` |
|        - | 12069 | ` * refer to the official documentation.` |
|        - | 12070 | ` */` |
|    21928 | 12071 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 12072 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 12073 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 12074 | `	int nByte              /* *pzDevice length*/` |
|        - | 12075 | `	)` |
|        2 | 12076 |  |
|        - | 12077 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 12078 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 12079 | `	SyString sDev,sCur;` |
|        - | 12080 | `	sxu32 n,nEntry;` |
|        - | 12081 | `	int rc;` |
|        - | 12082 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    21930 | 12083 | `	zNext = zCur = zIn = *pzDevice;` |
|    21930 | 12084 | `	zEnd = &zIn[nByte];` |
|  1394797 | 12085 | `	while( zIn < zEnd ){` |
|  1372871 | 12086 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 12087 | `			/* Got one */` |
|        3 | 12088 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 12089 | `			break;` |
|        - | 12090 | `		}` |
|        - | 12091 | `		/* Advance the cursor */` |
|  1372869 | 12092 | `		zIn++;` |
|        2 | 12093 | `	}` |
|    21930 | 12094 | `	if( zIn >= zEnd ){` |
|        - | 12095 | `		/* No such scheme,return the default stream */` |
|    21928 | 12096 | `		return pVm->pDefStream;` |
|        - | 12097 | `	}` |
|        3 | 12098 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 12099 | `	/* Remove leading and trailing white spaces */` |
|        3 | 12100 | `	SyStringFullTrim(&sDev);` |
|        - | 12101 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 12102 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 12103 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 12104 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 12105 | `		pStream = apStream[n];` |
|        3 | 12106 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 12107 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 12108 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 12109 | `		if( rc == 0 ){` |
|        - | 12110 | `			/* Stream device found */` |
|        3 | 12111 | `			*pzDevice = zNext;` |
|        3 | 12112 | `			return pStream;` |
|        - | 12113 | `		}` |
|      ! 0 | 12114 | `	}` |
|        - | 12115 | `	/* No such stream,return NULL */` |
|      ! 0 | 12116 | `	return 0;` |
|    10966 | 12117 |  |
|        - | 12118 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 12119 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 12120 |  |
