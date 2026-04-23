# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6027/7843 lines (76.85%)

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
|        - |     9 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |    10 | `#include <math.h>` |
|        - |    11 | `#endif` |
|        - |    12 | `/* Signed 64-bit multiplication with overflow detection. GCC/Clang expose` |
|        - |    13 | ` * __builtin_mul_overflow; MSVC does not, so we fall back to a portable` |
|        - |    14 | ` * UB-free bound-check implementation. Sets *pR to the wrapped product and` |
|        - |    15 | ` * returns non-zero on overflow. The fallback never divides a potentially-` |
|        - |    16 | ` * overflowed intermediate: all divisions are of compile-time constants` |
|        - |    17 | ` * (LARGEST_INT64/SMALLEST_INT64) by a factor already proven not to be -1,` |
|        - |    18 | ` * and the product itself is computed via unsigned multiplication to avoid` |
|        - |    19 | ` * signed-overflow UB. */` |
|        - |    20 | `#if defined(__GNUC__) \|\| defined(__clang__)` |
|        - |    21 | `#define VmMulOverflow64(a,b,pR) __builtin_mul_overflow((a),(b),(pR))` |
|        - |    22 | `#else` |
|        - |    23 | `static int VmMulOverflow64(sxi64 a, sxi64 b, sxi64 *pR)` |
|        1 |    24 |  |
|        1 |    25 | `	*pR = (sxi64)((sxu64)a * (sxu64)b);` |
|        - |    26 | `	/* Factors of 0 or ±1 never overflow; handle up front so the divisions` |
|        - |    27 | `	 * below are guaranteed safe (no SMALLEST_INT64 / -1, no /0). */` |
|        1 |    28 | `	if( a == 0 \|\| b == 0 \|\| a == 1 \|\| b == 1 ){` |
|        1 |    29 | `		return 0;` |
|        - |    30 | `	}` |
|        1 |    31 | `	if( a == -1 ){` |
|      ! 0 |    32 | `		return b == SMALLEST_INT64;` |
|        - |    33 | `	}` |
|        1 |    34 | `	if( b == -1 ){` |
|      ! 0 |    35 | `		return a == SMALLEST_INT64;` |
|        - |    36 | `	}` |
|        - |    37 | `	/* \|a\|,\|b\| >= 2 and neither is -1.  Bound check against the MAX/MIN` |
|        - |    38 | `	 * thresholds.  No division by -1 is possible here, and the quotients` |
|        - |    39 | `	 * of compile-time constants by {a,b} always fit in sxi64. */` |
|        1 |    40 | `	if( a > 0 ){` |
|        1 |    41 | `		if( b > 0 ){` |
|        1 |    42 | `			return a > LARGEST_INT64 / b;` |
|      ! 0 |    43 | `		}else{` |
|      ! 0 |    44 | `			return b < SMALLEST_INT64 / a;` |
|        - |    45 | `		}` |
|      ! 0 |    46 | `	}else{` |
|        1 |    47 | `		if( b > 0 ){` |
|        1 |    48 | `			return a < SMALLEST_INT64 / b;` |
|      ! 0 |    49 | `		}else{` |
|        1 |    50 | `			return b < LARGEST_INT64 / a;` |
|        - |    51 | `		}` |
|        - |    52 | `	}` |
|        1 |    53 |  |
|        - |    54 | `#endif` |
|        - |    55 | `/*` |
|        - |    56 | ` * The code in this file implements execution method of the PH7 Virtual Machine.` |
|        - |    57 | ` * The PH7 compiler (implemented in 'compiler.c' and 'parse.c') generates a bytecode program` |
|        - |    58 | ` * which is then executed by the virtual machine implemented here to do the work of the PHP` |
|        - |    59 | ` * statements.` |
|        - |    60 | ` * PH7 bytecode programs are similar in form to assembly language. The program consists` |
|        - |    61 | ` * of a linear sequence of operations .Each operation has an opcode and 3 operands.` |
|        - |    62 | ` * Operands P1 and P2 are integers where the first is signed while the second is unsigned.` |
|        - |    63 | ` * Operand P3 is an arbitrary pointer specific to each instruction. The P2 operand is usually` |
|        - |    64 | ` * the jump destination used by the OP_JMP,OP_JZ,OP_JNZ,... instructions.` |
|        - |    65 | ` * Opcodes will typically ignore one or more operands. Many opcodes ignore all three operands.` |
|        - |    66 | ` * Computation results are stored on a stack. Each entry on the stack is of type ph7_value.` |
|        - |    67 | ` * PH7 uses the ph7_value object to represent all values that can be stored in a PHP variable.` |
|        - |    68 | ` * Since PHP uses dynamic typing for the values it stores. Values stored in ph7_value objects` |
|        - |    69 | ` * can be integers,floating point values,strings,arrays,class instances (object in the PHP jargon)` |
|        - |    70 | ` * and so on.` |
|        - |    71 | ` * Internally,the PH7 virtual machine manipulates nearly all PHP values as ph7_values structures.` |
|        - |    72 | ` * Each ph7_value may cache multiple representations(string,integer etc.) of the same value.` |
|        - |    73 | ` * An implicit conversion from one type to the other occurs as necessary.` |
|        - |    74 | ` * Most of the code in this file is taken up by the [VmByteCodeExec()] function which does` |
|        - |    75 | ` * the work of interpreting a PH7 bytecode program. But other routines are also provided` |
|        - |    76 | ` * to help in building up a program instruction by instruction. Also note that sepcial` |
|        - |    77 | ` * functions that need access to the underlying virtual machine details such as [die()],` |
|        - |    78 | ` * [func_get_args()],[call_user_func()],[ob_start()] and many more are implemented here.` |
|        - |    79 | ` */` |
|        - |    80 | `/* VmFrame struct and VM_FRAME_* defines moved to ph7int.h */` |
|        - |    81 | `/*` |
|        - |    82 | ` * When a user defined variable is released (via manual unset($x) or garbage collected)` |
|        - |    83 | ` * memory object index is stored in an instance of the following structure and put` |
|        - |    84 | ` * in the free object table so that it can be reused again without allocating` |
|        - |    85 | ` * a new memory object.` |
|        - |    86 | ` */` |
|        - |    87 | `typedef struct VmSlot VmSlot;` |
|        - |    88 | `struct VmSlot` |
|        - |    89 |  |
|        - |    90 | `	sxu32 nIdx;      /* Index in pVm->aMemObj[] */` |
|        - |    91 | `	void *pUserData; /* Upper-layer private data */` |
|        - |    92 | `};` |
|        - |    93 | `/*` |
|        - |    94 | ` * An entry in the reference table is represented by an instance of the` |
|        - |    95 | ` * follwoing table.` |
|        - |    96 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - |    97 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - |    98 | ` * the reference implementation is consistent,solid and it's` |
|        - |    99 | ` * behavior resemble the C++ reference mechanism.` |
|        - |   100 | ` * Refer to the official for more information on this powerful` |
|        - |   101 | ` * extension.` |
|        - |   102 | ` */` |
|        - |   103 | `struct VmRefObj` |
|        - |   104 |  |
|        - |   105 | `	SySet aReference;  /* Table of references to this memory object */` |
|        - |   106 | `	SySet aArrEntries; /* Foreign hashmap entries [i.e: array(&$a) ] */` |
|        - |   107 | `	sxu32 nIdx;        /* Referenced object index */` |
|        - |   108 | `	sxi32 iFlags;      /* Configuration flags */` |
|        - |   109 | `	VmRefObj *pNextCollide,*pPrevCollide; /* Collision link */` |
|        - |   110 | `	VmRefObj *pNext,*pPrev;               /* List of all referenced objects */` |
|        - |   111 | `};` |
|        - |   112 | `#define VM_REF_IDX_KEEP  0x001 /* Do not restore the memory object to the free list */` |
|        - |   113 | `/* VmObEntry struct moved to ph7int.h */` |
|        - |   114 | `/*` |
|        - |   115 | ` * Each installed shutdown callback (registered using [register_shutdown_function()] )` |
|        - |   116 | ` * is stored in an instance of the following structure.` |
|        - |   117 | ` * Refer to the implementation of [register_shutdown_function(()] for more information.` |
|        - |   118 | ` */` |
|        - |   119 | `typedef struct VmShutdownCB VmShutdownCB;` |
|        - |   120 | `struct VmShutdownCB` |
|        - |   121 |  |
|        - |   122 | `	ph7_value sCallback; /* Shutdown callback */` |
|        - |   123 | `	ph7_value aArg[10];   /* Callback arguments (10 maximum arguments) */` |
|        - |   124 | `	int nArg;             /* Total number of given arguments */` |
|        - |   125 | `};` |
|        - |   126 | `/*` |
|        - |   127 | ` * Each installed autoload callback (registered using [spl_autoload_register()] )` |
|        - |   128 | ` * is stored in an instance of the following structure.` |
|        - |   129 | ` * Refer to the implementation of [spl_autoload_register()] for more information.` |
|        - |   130 | ` */` |
|        - |   131 | `typedef struct VmAutoloadCB VmAutoloadCB;` |
|        - |   132 | `struct VmAutoloadCB` |
|        - |   133 |  |
|        - |   134 | `	ph7_value sCallback; /* Autoload callback (string or [obj,method] array) */` |
|        - |   135 | `};` |
|        - |   136 | `/* Uncaught exception code value */` |
|        - |   137 | `#define PH7_EXCEPTION -255` |
|        - |   138 |  |
|        - |   139 | `/*` |
|        - |   140 | ` * Return TRUE if either operand is a NaN real value.` |
|        - |   141 | ` */` |
|   876764 |   142 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   143 |  |
|   876766 |   144 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |   145 | `		return TRUE;` |
|        - |   146 | `	}` |
|   876732 |   147 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   148 | `		return TRUE;` |
|        - |   149 | `	}` |
|   876722 |   150 | `	return FALSE;` |
|   438406 |   151 |  |
|        - |   152 | `/* SyhttpUri, SyhttpHeader and HTTP method/protocol defines moved to ph7int.h */` |
|        - |   153 | `/*` |
|        - |   154 | ` * Register a constant and it's associated expansion callback so that` |
|        - |   155 | ` * it can be expanded from the target PHP program.` |
|        - |   156 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|        - |   157 | ` * simple and work as follows:` |
|        - |   158 | ` * Each registered constant have a C procedure associated with it.` |
|        - |   159 | ` * This procedure known as the constant expansion callback is responsible` |
|        - |   160 | ` * of expanding the invoked constant to the desired value,for example:` |
|        - |   161 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|        - |   162 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|        - |   163 | ` * (Windows,Linux,...) and so on.` |
|        - |   164 | ` * Please refer to the official documentation for additional information.` |
|        - |   165 | ` */` |
|   569000 |   166 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |   167 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   168 | `	const SyString *pName,  /* Constant name */` |
|        - |   169 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   170 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |   171 | `	)` |
|        2 |   172 |  |
|        - |   173 | `	ph7_constant *pCons;` |
|        - |   174 | `	SyHashEntry *pEntry;` |
|        - |   175 | `	char *zDupName;` |
|        - |   176 | `	sxi32 rc;` |
|   569002 |   177 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   569002 |   178 | `	if( pEntry ){` |
|        - |   179 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   180 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   181 | `		pCons->xExpand = xExpand;` |
|        6 |   182 | `		pCons->pUserData = pUserData;` |
|        6 |   183 | `		return SXRET_OK;` |
|        - |   184 | `	}` |
|        - |   185 | `	/* Allocate a new constant instance */` |
|   568998 |   186 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   568998 |   187 | `	if( pCons == 0 ){` |
|      ! 0 |   188 | `		return 0;` |
|        - |   189 | `	}` |
|        - |   190 | `	/* Duplicate constant name */` |
|   568998 |   191 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   568998 |   192 | `	if( zDupName == 0 ){` |
|      ! 0 |   193 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   194 | `		return 0;` |
|        - |   195 | `	}` |
|        - |   196 | `	/* Install the constant */` |
|   568998 |   197 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   568998 |   198 | `	pCons->xExpand = xExpand;` |
|   568998 |   199 | `	pCons->pUserData = pUserData;` |
|   568998 |   200 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   568998 |   201 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   202 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   203 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   204 | `		return rc;` |
|        - |   205 | `	}` |
|        - |   206 | `	/* All done,constant can be invoked from PHP code */` |
|   568998 |   207 | `	return SXRET_OK;` |
|   284502 |   208 |  |
|        - |   209 | `/*` |
|        - |   210 | ` * Allocate a new foreign function instance.` |
|        - |   211 | ` * This function return SXRET_OK on success. Any other` |
|        - |   212 | ` * return value indicates failure.` |
|        - |   213 | ` * Please refer to the official documentation for an introduction to` |
|        - |   214 | ` * the foreign function mechanism.` |
|        - |   215 | ` */` |
|  1251416 |   216 | `static sxi32 PH7_NewForeignFunction(` |
|        - |   217 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   218 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   219 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   220 | `	void *pUserData,          /* Foreign function private data */` |
|        - |   221 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |   222 | `	)` |
|        2 |   223 |  |
|        - |   224 | `	ph7_user_func *pFunc;` |
|        - |   225 | `	char *zDup;` |
|        - |   226 | `	/* Allocate a new user function */` |
|  1251418 |   227 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1251418 |   228 | `	if( pFunc == 0 ){` |
|      ! 0 |   229 | `		return SXERR_MEM;` |
|        - |   230 | `	}` |
|        - |   231 | `	/* Duplicate function name */` |
|  1251418 |   232 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1251418 |   233 | `	if( zDup == 0 ){` |
|      ! 0 |   234 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   235 | `		return SXERR_MEM;` |
|        - |   236 | `	}` |
|        - |   237 | `	/* Zero the structure */` |
|  1251418 |   238 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   239 | `	/* Initialize structure fields */` |
|  1251418 |   240 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1251418 |   241 | `	pFunc->pVm   = pVm;` |
|  1251418 |   242 | `	pFunc->xFunc = xFunc;` |
|  1251418 |   243 | `	pFunc->pUserData = pUserData;` |
|  1251418 |   244 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   245 | `	/* Write a pointer to the new function */` |
|  1251418 |   246 | `	*ppOut = pFunc;` |
|  1251418 |   247 | `	return SXRET_OK;` |
|   625710 |   248 |  |
|        - |   249 | `/*` |
|        - |   250 | ` * Install a foreign function and it's associated callback so that` |
|        - |   251 | ` * it can be invoked from the target PHP code.` |
|        - |   252 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   253 | ` * return value indicates failure.` |
|        - |   254 | ` * Please refer to the official documentation for an introduction to` |
|        - |   255 | ` * the foreign function mechanism.` |
|        - |   256 | ` */` |
|  1254038 |   257 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |   258 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   259 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   260 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   261 | `	void *pUserData           /* Foreign function private data */` |
|        - |   262 | `	)` |
|        2 |   263 |  |
|        - |   264 | `	ph7_user_func *pFunc;` |
|        - |   265 | `	SyHashEntry *pEntry;` |
|        - |   266 | `	sxi32 rc;` |
|        - |   267 | `	/* Overwrite any previously registered function with the same name */` |
|  1254040 |   268 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1254040 |   269 | `	if( pEntry ){` |
|     2624 |   270 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2624 |   271 | `		pFunc->pUserData = pUserData;` |
|     2624 |   272 | `		pFunc->xFunc = xFunc;` |
|     2624 |   273 | `		SySetReset(&pFunc->aAux);` |
|     2624 |   274 | `		return SXRET_OK;` |
|        - |   275 | `	}` |
|        - |   276 | `	/* Create a new user function */` |
|  1251418 |   277 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1251418 |   278 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   279 | `		return rc;` |
|        - |   280 | `	}` |
|        - |   281 | `	/* Install the function in the corresponding hashtable */` |
|  1251418 |   282 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1251418 |   283 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   284 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   285 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   286 | `		return rc;` |
|        - |   287 | `	}` |
|        - |   288 | `	/* User function successfully installed */` |
|  1251418 |   289 | `	return SXRET_OK;` |
|   627021 |   290 |  |
|        - |   291 | `/*` |
|        - |   292 | ` * Initialize a VM function.` |
|        - |   293 | ` */` |
|   230532 |   294 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   295 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   296 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   297 | `	const char *zName,  /* Function name */` |
|        - |   298 | `	sxu32 nByte,        /* zName length */` |
|        - |   299 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   300 | `	void *pUserData     /* Function private data */` |
|        - |   301 | `	)` |
|        2 |   302 |  |
|        - |   303 | `	/* Zero the structure */` |
|   230534 |   304 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   305 | `	/* Initialize structure fields */` |
|        - |   306 | `	/* Arguments container */` |
|   230534 |   307 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   308 | `	/* Static variable container */` |
|   230534 |   309 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   310 | `	/* Bytecode container */` |
|   230534 |   311 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   312 | `    /* Preallocate some instruction slots */` |
|   230534 |   313 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   314 | `	/* Closure environment */` |
|   230534 |   315 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   316 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   230534 |   317 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   230534 |   318 | `	pFunc->iFlags = iFlags;` |
|   230534 |   319 | `	pFunc->pUserData = pUserData;` |
|        - |   320 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |   321 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   230534 |   322 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   230534 |   323 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   230534 |   324 | `	return SXRET_OK;` |
|        2 |   325 |  |
|        - |   326 | `/*` |
|        - |   327 | ` * Namespace-aware function lookup.` |
|        - |   328 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   329 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   330 | ` */` |
|        - |   331 | `/*` |
|        - |   332 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   333 | ` */` |
|   707100 |   334 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   335 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   336 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   337 | `	SyString *pName     /* Function name */` |
|        - |   338 | `	)` |
|        2 |   339 |  |
|        - |   340 | `	SyHashEntry *pEntry;` |
|        - |   341 | `	sxi32 rc;` |
|   707102 |   342 | `	if( pName == 0 ){` |
|        - |   343 | `		/* Use the built-in name */` |
|    39114 |   344 | `		pName = &pFunc->sName;` |
|    19556 |   345 | `	}` |
|        - |   346 | `	/* Check for duplicates (functions with the same name) first */` |
|   707102 |   347 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   707102 |   348 | `	if( pEntry ){` |
|   523834 |   349 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   523834 |   350 | `		if( pLink != pFunc ){` |
|        - |   351 | `			/* Link */` |
|      188 |   352 | `			pFunc->pNextName = pLink;` |
|      188 |   353 | `			pEntry->pUserData = pFunc;` |
|       93 |   354 | `		}` |
|   523834 |   355 | `		return SXRET_OK;` |
|        - |   356 | `	}` |
|        - |   357 | `	/* First time seen */` |
|   183270 |   358 | `	pFunc->pNextName = 0;` |
|   183270 |   359 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   183270 |   360 | `	return rc;` |
|   353552 |   361 |  |
|        - |   362 | `/*` |
|        - |   363 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   364 | ` */` |
|    53686 |   365 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   366 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   367 | `	ph7_class *pClass /* Target Class */` |
|        - |   368 | `	)` |
|        2 |   369 |  |
|    53688 |   370 | `	SyString *pName = &pClass->sName;` |
|        - |   371 | `	SyHashEntry *pEntry;` |
|        - |   372 | `	sxi32 rc;` |
|        - |   373 | `	/* Check for duplicates */` |
|    53688 |   374 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    53688 |   375 | `	if( pEntry ){` |
|       31 |   376 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   377 | `		/* Link entry with the same name */` |
|       31 |   378 | `		pClass->pNextName = pLink;` |
|       31 |   379 | `		pEntry->pUserData = pClass;` |
|       31 |   380 | `		return SXRET_OK;` |
|        - |   381 | `	}` |
|    53658 |   382 | `	pClass->pNextName = 0;` |
|        - |   383 | `	/* Perform a simple hashtable insertion */` |
|    53658 |   384 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    53658 |   385 | `	return rc;` |
|    26845 |   386 |  |
|        - |   387 | `/*` |
|        - |   388 | ` * Instruction builder interface.` |
|        - |   389 | ` */` |
|  3973752 |   390 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   391 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   392 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   393 | `	sxi32 iP1,    /* First operand */` |
|        - |   394 | `	sxu32 iP2,    /* Second operand */` |
|        - |   395 | `	void *p3,     /* Third operand */` |
|        - |   396 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   397 | `	)` |
|        2 |   398 |  |
|        - |   399 | `	VmInstr sInstr;` |
|        - |   400 | `	sxi32 rc;` |
|        - |   401 | `	/* Fill the VM instruction */` |
|  3973754 |   402 | `	sInstr.iOp = (sxu8)iOp;` |
|  3973754 |   403 | `	sInstr.iP1 = iP1;` |
|  3973754 |   404 | `	sInstr.iP2 = iP2;` |
|  3973754 |   405 | `	sInstr.p3  = p3;` |
|  3973754 |   406 | `	if( pIndex ){` |
|        - |   407 | `		/* Instruction index in the bytecode array */` |
|   215672 |   408 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   107835 |   409 | `	}` |
|        - |   410 | `	/* Finally,record the instruction */` |
|  3973754 |   411 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3973754 |   412 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   413 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   414 | `		/* Fall throw */` |
|      ! 0 |   415 | `	}` |
|  3973754 |   416 | `	return rc;` |
|        2 |   417 |  |
|        - |   418 | `/*` |
|        - |   419 | ` * Swap the current bytecode container with the given one.` |
|        - |   420 | ` */` |
|   515936 |   421 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   422 |  |
|   515938 |   423 | `	if( pContainer == 0 ){` |
|        - |   424 | `		/* Point to the default container */` |
|      ! 0 |   425 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   426 | `	}else{` |
|        - |   427 | `		/* Change container */` |
|   515938 |   428 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   429 | `	}` |
|   515938 |   430 | `	return SXRET_OK;` |
|        2 |   431 |  |
|        - |   432 | `/*` |
|        - |   433 | ` * Return the current bytecode container.` |
|        - |   434 | ` */` |
|   257968 |   435 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   436 |  |
|   257970 |   437 | `	return pVm->pByteContainer;` |
|        2 |   438 |  |
|        - |   439 | `/*` |
|        - |   440 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   441 | ` */` |
|   212658 |   442 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   443 |  |
|        - |   444 | `	VmInstr *pInstr;` |
|   212660 |   445 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   212660 |   446 | `	return pInstr;` |
|        2 |   447 |  |
|        - |   448 | `/*` |
|        - |   449 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   450 | ` */` |
|  1194434 |   451 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   452 |  |
|  1194436 |   453 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   454 |  |
|        - |   455 | `/*` |
|        - |   456 | ` * Pop the last VM instruction.` |
|        - |   457 | ` */` |
|   196912 |   458 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   459 |  |
|   196914 |   460 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   461 |  |
|        - |   462 | `/*` |
|        - |   463 | ` * Peek the last VM instruction.` |
|        - |   464 | ` */` |
|   782922 |   465 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   466 |  |
|   782924 |   467 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   468 |  |
|    30932 |   469 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   470 |  |
|        - |   471 | `	VmInstr *aInstr;` |
|        - |   472 | `	sxu32 n;` |
|    30934 |   473 | `	n = SySetUsed(pVm->pByteContainer);` |
|    30934 |   474 | `	if( n < 2 ){` |
|      ! 0 |   475 | `		return 0;` |
|        - |   476 | `	}` |
|    30934 |   477 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    30934 |   478 | `	return &aInstr[n - 2];` |
|    15468 |   479 |  |
|        - |   480 | `/*` |
|        - |   481 | ` * Allocate a new virtual machine frame.` |
|        - |   482 | ` */` |
|    20086 |   483 | `static VmFrame * VmNewFrame(` |
|        - |   484 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   485 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   486 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   487 | `	)` |
|        2 |   488 |  |
|        - |   489 | `	VmFrame *pFrame;` |
|        - |   490 | `	/* Allocate a new vm frame */` |
|    20088 |   491 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    20088 |   492 | `	if( pFrame == 0 ){` |
|      ! 0 |   493 | `		return 0;` |
|        - |   494 | `	}` |
|        - |   495 | `	/* Zero the structure */` |
|    20088 |   496 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   497 | `	/* Initialize frame fields */` |
|    20088 |   498 | `	pFrame->pUserData = pUserData;` |
|    20088 |   499 | `	pFrame->pThis = pThis;` |
|    20088 |   500 | `	pFrame->pVm = pVm;` |
|    20088 |   501 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    20088 |   502 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    20088 |   503 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    20088 |   504 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    20088 |   505 | `	return pFrame;` |
|    10045 |   506 |  |
|        - |   507 | `/* Forward declaration */` |
|        - |   508 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   509 | `/*` |
|        - |   510 | ` * Enter a VM frame.` |
|        - |   511 | ` */` |
|    20040 |   512 | `static sxi32 VmEnterFrame(` |
|        - |   513 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   514 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   515 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   516 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   517 | `	)` |
|        2 |   518 |  |
|        - |   519 | `	VmFrame *pFrame;` |
|        - |   520 | `	/* Allocate a new frame */` |
|    20042 |   521 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    20042 |   522 | `	if( pFrame == 0 ){` |
|      ! 0 |   523 | `		return SXERR_MEM;` |
|        - |   524 | `	}` |
|        - |   525 | `	/* Link to the list of active VM frame */` |
|    20042 |   526 | `	pFrame->pParent = pVm->pFrame;` |
|    20042 |   527 | `	pVm->pFrame = pFrame;` |
|    20042 |   528 | `	if( ppFrame ){` |
|        - |   529 | `		/* Write a pointer to the new VM frame */` |
|    17106 |   530 | `		*ppFrame = pFrame;` |
|     8552 |   531 | `	}` |
|    20042 |   532 | `	return SXRET_OK;` |
|    10022 |   533 |  |
|        - |   534 | `/*` |
|        - |   535 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   536 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   537 | ` * information.` |
|        - |   538 | ` */` |
|       58 |   539 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        2 |   540 |  |
|        - |   541 | `	VmFrame *pTarget,*pFrame;` |
|       60 |   542 | `	SyHashEntry *pEntry = 0;` |
|        - |   543 | `	sxi32 rc;` |
|        - |   544 | `	/* Point to the upper frame */` |
|       60 |   545 | `	pFrame = pVm->pFrame;` |
|       60 |   546 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       60 |   547 | `	pTarget = pFrame;` |
|       60 |   548 | `	pFrame = pTarget->pParent;` |
|       60 |   549 | `	while( pFrame ){` |
|       60 |   550 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   551 | `			/* Query the current frame */` |
|       60 |   552 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       60 |   553 | `			if( pEntry ){` |
|        - |   554 | `				/* Variable found */` |
|       60 |   555 | `				break;` |
|        - |   556 | `			}` |
|      ! 0 |   557 | `		}` |
|        - |   558 | `		/* Point to the upper frame */` |
|      ! 0 |   559 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   560 | `	}` |
|       60 |   561 | `	if( pEntry == 0 ){` |
|        - |   562 | `		/* Inexistant variable */` |
|      ! 0 |   563 | `		return SXERR_NOTFOUND;` |
|        - |   564 | `	}` |
|        - |   565 | `	/* Link to the current frame */` |
|       60 |   566 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       60 |   567 | `	if( rc == SXRET_OK ){` |
|        - |   568 | `		sxu32 nIdx;` |
|       60 |   569 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       60 |   570 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       29 |   571 | `	}` |
|       60 |   572 | `	return rc;` |
|       31 |   573 |  |
|        - |   574 | `/*` |
|        - |   575 | ` * Leave the top-most active frame.` |
|        - |   576 | ` */` |
|    17094 |   577 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   578 |  |
|    17096 |   579 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    17096 |   580 | `	if( pCurFrame ){` |
|        - |   581 | `		/* Unlink from the list of active VM frame */` |
|    17096 |   582 | `		pVm->pFrame = pCurFrame->pParent;` |
|    17096 |   583 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   584 | `			VmSlot  *aSlot;` |
|        - |   585 | `			sxu32 n;` |
|        - |   586 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    16876 |   587 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   114274 |   588 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   589 | `				/* Unset the local variable */` |
|    97400 |   590 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    48701 |   591 | `			}` |
|        - |   592 | `			/* Remove local reference */` |
|    16876 |   593 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   114336 |   594 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    97462 |   595 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    48732 |   596 | `			}` |
|     8437 |   597 | `		}` |
|        - |   598 | `		/* Release internal containers */` |
|    17096 |   599 | `		SyHashRelease(&pCurFrame->hVar);` |
|    17096 |   600 | `		SySetRelease(&pCurFrame->sArg);` |
|    17096 |   601 | `		SySetRelease(&pCurFrame->sLocal);` |
|    17096 |   602 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   603 | `		/* Release the whole structure */` |
|    17096 |   604 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     8547 |   605 | `	}` |
|    17096 |   606 |  |
|        - |   607 | `/*` |
|        - |   608 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   609 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   610 | ` * should be skipped when looking for the real execution context.` |
|        - |   611 | ` */` |
|  6832580 |   612 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   613 |  |
|  6833810 |   614 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     1230 |   615 | `		pFrame = pFrame->pParent;` |
|        2 |   616 | `	}` |
|  6832582 |   617 | `	return pFrame;` |
|        2 |   618 |  |
|        - |   619 | `/*` |
|        - |   620 | ` * Compare two functions signature and return the comparison result.` |
|        - |   621 | ` */` |
|      836 |   622 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   623 |  |
|      837 |   624 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      837 |   625 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      837 |   626 | `	const char *zSin = pSecond->zString;` |
|      837 |   627 | `	const char *zFin = pFirst->zString;` |
|      837 |   628 | `	const char *zPtr = zFin;` |
|      421 |   629 | `	for(;;){` |
|      843 |   630 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      413 |   631 | `			break;` |
|        - |   632 | `		}` |
|       19 |   633 | `		if( zFin[0] != zSin[0] ){` |
|        - |   634 | `			/* mismatch */` |
|       13 |   635 | `			break;` |
|        - |   636 | `		}` |
|        7 |   637 | `		zFin++;` |
|        7 |   638 | `		zSin++;` |
|        1 |   639 | `	}` |
|      837 |   640 | `	return (int)(zFin-zPtr);` |
|        1 |   641 |  |
|        - |   642 | `/*` |
|        - |   643 | ` * Select the appropriate VM function for the current call context.` |
|        - |   644 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   645 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   646 | ` * Refer to the official documentation for more information.` |
|        - |   647 | ` */` |
|      138 |   648 | `static ph7_vm_func * VmOverload(` |
|        - |   649 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   650 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   651 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   652 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   653 | `	)` |
|        2 |   654 |  |
|        - |   655 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   656 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   657 | `	ph7_vm_func *pLink;` |
|        - |   658 | `	SyString sArgSig;` |
|        - |   659 | `	SyBlob sSig;` |
|        - |   660 |  |
|      140 |   661 | `	pLink = pList;` |
|      140 |   662 | `	i = 0;` |
|        - |   663 | `	/* Put functions expecting the same number of passed arguments */` |
|     1086 |   664 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1024 |   665 | `		if( pLink == 0 ){` |
|       78 |   666 | `			break;` |
|        - |   667 | `		}` |
|      948 |   668 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   669 | `			/* Candidate for overloading */` |
|      902 |   670 | `			apSet[i++] = pLink;` |
|      450 |   671 | `		}` |
|        - |   672 | `		/* Point to the next entry */` |
|      948 |   673 | `		pLink = pLink->pNextName;` |
|        2 |   674 | `	}` |
|      140 |   675 | `	if( i < 1 ){` |
|        - |   676 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   677 | `		return pList;` |
|        - |   678 | `	}` |
|      140 |   679 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   680 | `		/* Return the only candidate */` |
|       32 |   681 | `		return apSet[0];` |
|        - |   682 | `	}` |
|        - |   683 | `	/* Calculate function signature */` |
|      109 |   684 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      367 |   685 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      259 |   686 | `		int c = 'n'; /* null */` |
|      259 |   687 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   688 | `			/* Hashmap */` |
|       45 |   689 | `			c = 'h';` |
|      237 |   690 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   691 | `			/* bool */` |
|      ! 0 |   692 | `			c = 'b';` |
|      215 |   693 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   694 | `			/* int */` |
|        7 |   695 | `			c = 'i';` |
|      212 |   696 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   697 | `			/* String */` |
|      107 |   698 | `			c = 's';` |
|      156 |   699 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   700 | `			/* Float */` |
|      ! 0 |   701 | `			c = 'f';` |
|      103 |   702 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   703 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|        3 |   704 | `			int marker = 'o';` |
|        3 |   705 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|        3 |   706 | `			SyString *pName = &pClass->sName;` |
|        3 |   707 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|        3 |   708 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|        3 |   709 | `			c = -1;` |
|        1 |   710 | `		}` |
|      259 |   711 | `		if( c > 0 ){` |
|      257 |   712 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      128 |   713 | `		}` |
|      130 |   714 | `	}` |
|      109 |   715 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      109 |   716 | `	iTarget = 0;` |
|      109 |   717 | `	iMax = -1;` |
|        - |   718 | `	/* Select the appropriate function */` |
|      945 |   719 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   720 | `		/* Compare the two signatures */` |
|      837 |   721 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      837 |   722 | `		if( iCur > iMax ){` |
|      113 |   723 | `			iMax = iCur;` |
|      113 |   724 | `			iTarget = j;` |
|       56 |   725 | `		}` |
|      419 |   726 | `	}` |
|      109 |   727 | `	SyBlobRelease(&sSig);` |
|        - |   728 | `	/* Appropriate function for the current call context */` |
|      109 |   729 | `	return apSet[iTarget];` |
|       71 |   730 |  |
|        - |   731 | `/* Forward declaration */` |
|        - |   732 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   733 | `/*` |
|        - |   734 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   735 | ` * it can be instanciated from the executed PHP script.` |
|        - |   736 | ` */` |
|   155488 |   737 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   738 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   739 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   740 | `	)` |
|        2 |   741 |  |
|        - |   742 | `	ph7_class_method *pMeth;` |
|        - |   743 | `	ph7_class_attr *pAttr;` |
|        - |   744 | `	SyHashEntry *pEntry;` |
|        - |   745 | `	sxi32 rc;` |
|        - |   746 | `	/* Reset the loop cursor */` |
|   155490 |   747 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   748 | `	/* Process only static and constant attribute */` |
|   609074 |   749 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   750 | `		/* Extract the current attribute */` |
|   375842 |   751 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   375842 |   752 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   753 | `			ph7_value *pMemObj;` |
|        - |   754 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1688 |   755 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1688 |   756 | `			if( pMemObj == 0 ){` |
|      ! 0 |   757 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   758 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   759 | `					&pClass->sName,&pAttr->sName` |
|        - |   760 | `					);` |
|      ! 0 |   761 | `				return SXERR_MEM;` |
|        - |   762 | `			}` |
|     1688 |   763 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   764 | `				/* Initialize attribute default value (any complex expression) */` |
|     1684 |   765 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      841 |   766 | `			}` |
|        - |   767 | `			/* Record attribute index */` |
|     1688 |   768 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   769 | `			/* Install static attribute in the reference table */` |
|     1688 |   770 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   771 | `			/* If this is a typed static property, register the slot so the` |
|        - |   772 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |   773 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |   774 | `			 * points at its own nIdx field (stable for the VM lifetime). */` |
|     1688 |   775 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       10 |   776 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       10 |   777 | `				if( pVmAttrS == 0 ){` |
|      ! 0 |   778 | `					return SXERR_MEM;` |
|        - |   779 | `				}` |
|       10 |   780 | `				pVmAttrS->pAttr = pAttr;` |
|       10 |   781 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|       10 |   782 | `				pVmAttrS->iState = 0;` |
|       10 |   783 | `				pVmAttrS->pOwner = pClass;` |
|        - |   784 | `				/* Static typed property with no default starts uninitialized */` |
|        8 |   785 | `				if( SySetUsed(&pAttr->aByteCode) == 0` |
|        8 |   786 | `				 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        6 |   787 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        2 |   788 | `				}` |
|       10 |   789 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|      ! 0 |   790 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|      ! 0 |   791 | `					return SXERR_MEM;` |
|        - |   792 | `				}` |
|        4 |   793 | `			}` |
|      843 |   794 | `		}` |
|        2 |   795 | `	}` |
|        - |   796 | `	/* Install class methods */` |
|   155490 |   797 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   798 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   799 | `		 */` |
|    76996 |   800 | `		return SXRET_OK;` |
|        - |   801 | `	}` |
|        - |   802 | `	/* Create constructor alias if not yet done */` |
|    78496 |   803 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   804 | `		/* User constructor with the same base class name */` |
|     6108 |   805 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     6108 |   806 | `		if( pEntry ){` |
|      ! 0 |   807 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   808 | `			/* Create the alias */` |
|      ! 0 |   809 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   810 | `		}` |
|     3053 |   811 | `	}` |
|        - |   812 | `	/* Install the methods now */` |
|    78496 |   813 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   785739 |   814 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   667998 |   815 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   667998 |   816 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   667990 |   817 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   667990 |   818 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   819 | `				return rc;` |
|        - |   820 | `			}` |
|   333994 |   821 | `		}` |
|        2 |   822 | `	}` |
|        - |   823 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    78496 |   824 | `	pClass->bMounted = TRUE;` |
|    78496 |   825 | `	return SXRET_OK;` |
|    77746 |   826 |  |
|        - |   827 | `/*` |
|        - |   828 | ` * Allocate a private frame for attributes of the given` |
|        - |   829 | ` * class instance (Object in the PHP jargon).` |
|        - |   830 | ` */` |
|     1712 |   831 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   832 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   833 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   834 | `	)` |
|        2 |   835 |  |
|     1714 |   836 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   837 | `	ph7_class_attr *pAttr;` |
|        - |   838 | `	SyHashEntry *pEntry;` |
|        - |   839 | `	sxi32 rc;` |
|        - |   840 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1714 |   841 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     7074 |   842 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   843 | `		VmClassAttr *pVmAttr;` |
|        - |   844 | `		/* Extract the current attribute */` |
|     5362 |   845 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     5362 |   846 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     5362 |   847 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   848 | `			return SXERR_MEM;` |
|        - |   849 | `		}` |
|     5362 |   850 | `		pVmAttr->pAttr = pAttr;` |
|     5362 |   851 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   852 | `			ph7_value *pMemObj;` |
|        - |   853 | `			/* Reserve a memory object for this attribute */` |
|     5338 |   854 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     5338 |   855 | `			if( pMemObj == 0 ){` |
|      ! 0 |   856 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   857 | `				return SXERR_MEM;` |
|        - |   858 | `			}` |
|     5338 |   859 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     5338 |   860 | `			pVmAttr->iState = 0;` |
|     5338 |   861 | `			pVmAttr->pOwner = pClass;` |
|     5338 |   862 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   863 | `				/* Initialize attribute default value (any complex expression) */` |
|     1828 |   864 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     4425 |   865 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   866 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   867 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       64 |   868 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       31 |   869 | `			}` |
|     5338 |   870 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     5338 |   871 | `			if( rc != SXRET_OK ){` |
|        - |   872 | `				VmSlot sSlot;` |
|        - |   873 | `				/* Restore memory object */` |
|      ! 0 |   874 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   875 | `				sSlot.pUserData = 0;` |
|      ! 0 |   876 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   877 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   878 | `				return SXERR_MEM;` |
|        - |   879 | `			}` |
|        - |   880 | `			/* Install attribute in the reference table */` |
|     5338 |   881 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   882 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   883 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   884 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     5338 |   885 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      158 |   886 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      158 |   887 | `				if( rc != SXRET_OK ){` |
|        - |   888 | `					VmSlot sSlot;` |
|      ! 0 |   889 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 |   890 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   891 | `					sSlot.pUserData = 0;` |
|      ! 0 |   892 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   893 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   894 | `					return SXERR_MEM;` |
|        - |   895 | `				}` |
|       78 |   896 | `			}` |
|     2670 |   897 | `		}else{` |
|        - |   898 | `			/* Install static/constant attribute */` |
|       26 |   899 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       26 |   900 | `			pVmAttr->iState = 0;` |
|       26 |   901 | `			pVmAttr->pOwner = pClass;` |
|       26 |   902 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       26 |   903 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   904 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   905 | `				return SXERR_MEM;` |
|        - |   906 | `			}` |
|        - |   907 | `		}` |
|        2 |   908 | `	}` |
|     1714 |   909 | `	return SXRET_OK;` |
|      858 |   910 |  |
|        - |   911 | `/* Forward declaration */` |
|        - |   912 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   913 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   914 | `/*` |
|        - |   915 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   916 | ` */` |
|        - |   917 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   918 | `/*` |
|        - |   919 | ` * Reserve a constant memory object.` |
|        - |   920 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   921 | ` */` |
|   424146 |   922 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   923 |  |
|        - |   924 | `	ph7_value *pObj;` |
|        - |   925 | `	sxi32 rc;` |
|   424148 |   926 | `	if( pIndex ){` |
|        - |   927 | `		/* Object index in the object table */` |
|   415340 |   928 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   207669 |   929 | `	}` |
|        - |   930 | `	/* Reserve a slot for the new object */` |
|   424148 |   931 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   424148 |   932 | `	if( rc != SXRET_OK ){` |
|        - |   933 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   934 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   935 | `		 */` |
|      ! 0 |   936 | `		return 0;` |
|        - |   937 | `	}` |
|   424148 |   938 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   424148 |   939 | `	return pObj;` |
|   212075 |   940 |  |
|        - |   941 | `/*` |
|        - |   942 | ` * Reserve a memory object.` |
|        - |   943 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   944 | ` */` |
|  2147984 |   945 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   946 |  |
|        - |   947 | `	ph7_value *pObj;` |
|        - |   948 | `	sxi32 rc;` |
|  2147986 |   949 | `	if( pIndex ){` |
|        - |   950 | `		/* Object index in the object table */` |
|  2147986 |   951 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1073992 |   952 | `	}` |
|        - |   953 | `	/* Reserve a slot for the new object */` |
|  2147986 |   954 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2147986 |   955 | `	if( rc != SXRET_OK ){` |
|        - |   956 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   957 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   958 | `		 */` |
|      ! 0 |   959 | `		return 0;` |
|        - |   960 | `	}` |
|  2147986 |   961 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2147986 |   962 | `	return pObj;` |
|  1073994 |   963 |  |
|        - |   964 | `/* Forward declaration */` |
|        - |   965 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   966 | `/* Forward declarations for Fiber C functions */` |
|        - |   967 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   968 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   969 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   970 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   971 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   972 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   973 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   974 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   975 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   976 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   977 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |   978 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |   979 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   980 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |   981 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |   982 | `static sxi32 VmCallClassMethodWithMap(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |   983 | `	ph7_class_method *pMethod, ph7_value *pResult, int nArg,` |
|        - |   984 | `	ph7_value **apArg, VmCallArgMap *pMap);` |
|        - |   985 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |   986 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   987 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |   988 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   989 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   990 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   991 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   992 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   993 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   994 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   995 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   996 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   997 | `/*` |
|        - |   998 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   999 | ` * directly as foreign functions.` |
|        - |  1000 | ` */` |
|        - |  1001 | `#define PH7_BUILTIN_LIB \` |
|        - |  1002 | `	"interface Throwable {"\` |
|        - |  1003 | `	"public function getMessage();"\` |
|        - |  1004 | `	"public function getCode();"\` |
|        - |  1005 | `	"public function getFile();"\` |
|        - |  1006 | `	"public function getLine();"\` |
|        - |  1007 | `	"public function getTrace();"\` |
|        - |  1008 | `	"public function getTraceAsString();"\` |
|        - |  1009 | `	"public function getPrevious();"\` |
|        - |  1010 | `	"public function __toString();"\` |
|        - |  1011 | `	"}"\` |
|        - |  1012 | `	"class Exception implements Throwable { "\` |
|        - |  1013 | `    "protected $message = '';"\` |
|        - |  1014 | `    "protected $code = 0;"\` |
|        - |  1015 | `    "protected $file;"\` |
|        - |  1016 | `    "protected $line;"\` |
|        - |  1017 | `    "protected $trace;"\` |
|        - |  1018 | `    "protected $previous;"\` |
|        - |  1019 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1020 | `	"   if( isset($message) ){"\` |
|        - |  1021 | `	"	  $this->message = $message;"\` |
|        - |  1022 | `	"   }"\` |
|        - |  1023 | `	"   $this->code = $code;"\` |
|        - |  1024 | `	"   $this->file = __FILE__;"\` |
|        - |  1025 | `	"   $this->line = __LINE__;"\` |
|        - |  1026 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1027 | `	"   if( isset($previous) ){"\` |
|        - |  1028 | `	"     $this->previous = $previous;"\` |
|        - |  1029 | `	"   }"\` |
|        - |  1030 | `	"}"\` |
|        - |  1031 | `	"public function getMessage(){"\` |
|        - |  1032 | `	"   return $this->message;"\` |
|        - |  1033 | `	"}"\` |
|        - |  1034 | `	" public function getCode(){"\` |
|        - |  1035 | `	"  return $this->code;"\` |
|        - |  1036 | `	"}"\` |
|        - |  1037 | `	"public function getFile(){"\` |
|        - |  1038 | `	"  return $this->file;"\` |
|        - |  1039 | `	"}"\` |
|        - |  1040 | `	"public function getLine(){"\` |
|        - |  1041 | `	"  return $this->line;"\` |
|        - |  1042 | `	"}"\` |
|        - |  1043 | `	"public function getTrace(){"\` |
|        - |  1044 | `	"   return $this->trace;"\` |
|        - |  1045 | `	"}"\` |
|        - |  1046 | `	"public function getTraceAsString(){"\` |
|        - |  1047 | `	"  return debug_string_backtrace();"\` |
|        - |  1048 | `	"}"\` |
|        - |  1049 | `	"public function getPrevious(){"\` |
|        - |  1050 | `	"    return $this->previous;"\` |
|        - |  1051 | `	"}"\` |
|        - |  1052 | `	"public function __toString(){"\` |
|        - |  1053 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1054 | `    "}"\` |
|        - |  1055 | `	"}"\` |
|        - |  1056 | `	"class Error implements Throwable { "\` |
|        - |  1057 | `    "protected $message = '';"\` |
|        - |  1058 | `    "protected $code = 0;"\` |
|        - |  1059 | `    "protected $file;"\` |
|        - |  1060 | `    "protected $line;"\` |
|        - |  1061 | `    "protected $trace;"\` |
|        - |  1062 | `    "protected $previous;"\` |
|        - |  1063 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1064 | `	"   if( isset($message) ){"\` |
|        - |  1065 | `	"	  $this->message = $message;"\` |
|        - |  1066 | `	"   }"\` |
|        - |  1067 | `	"   $this->code = $code;"\` |
|        - |  1068 | `	"   $this->file = __FILE__;"\` |
|        - |  1069 | `	"   $this->line = __LINE__;"\` |
|        - |  1070 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1071 | `	"   if( isset($previous) ){"\` |
|        - |  1072 | `	"     $this->previous = $previous;"\` |
|        - |  1073 | `	"   }"\` |
|        - |  1074 | `	"}"\` |
|        - |  1075 | `	"public function getMessage(){"\` |
|        - |  1076 | `	"   return $this->message;"\` |
|        - |  1077 | `	"}"\` |
|        - |  1078 | `	"public function getCode(){"\` |
|        - |  1079 | `	"  return $this->code;"\` |
|        - |  1080 | `	"}"\` |
|        - |  1081 | `	"public function getFile(){"\` |
|        - |  1082 | `	"  return $this->file;"\` |
|        - |  1083 | `	"}"\` |
|        - |  1084 | `	"public function getLine(){"\` |
|        - |  1085 | `	"  return $this->line;"\` |
|        - |  1086 | `	"}"\` |
|        - |  1087 | `	"public function getTrace(){"\` |
|        - |  1088 | `	"   return $this->trace;"\` |
|        - |  1089 | `	"}"\` |
|        - |  1090 | `	"public function getTraceAsString(){"\` |
|        - |  1091 | `	"  return debug_string_backtrace();"\` |
|        - |  1092 | `	"}"\` |
|        - |  1093 | `	"public function getPrevious(){"\` |
|        - |  1094 | `	"    return $this->previous;"\` |
|        - |  1095 | `	"}"\` |
|        - |  1096 | `	"public function __toString(){"\` |
|        - |  1097 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1098 | `	"}"\` |
|        - |  1099 | `	"}"\` |
|        - |  1100 | `	"class TypeError extends Error { }"\` |
|        - |  1101 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |  1102 | `	"class ValueError extends Error { }"\` |
|        - |  1103 | `	"class FiberError extends Error { }"\` |
|        - |  1104 | `	"class AssertionError extends Error { }"\` |
|        - |  1105 | `	"class ArithmeticError extends Error { }"\` |
|        - |  1106 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1107 | `	"class ErrorException extends Exception { "\` |
|        - |  1108 | `	"protected $severity;"\` |
|        - |  1109 | `	"public function __construct(string $message = null,"\` |
|        - |  1110 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Throwable $previous = null){"\` |
|        - |  1111 | `	"   if( isset($message) ){"\` |
|        - |  1112 | `	"	  $this->message = $message;"\` |
|        - |  1113 | `	"   }"\` |
|        - |  1114 | `	"   $this->severity = $severity;"\` |
|        - |  1115 | `	"   $this->code = $code;"\` |
|        - |  1116 | `	"   $this->file = $filename;"\` |
|        - |  1117 | `	"   $this->line = $lineno;"\` |
|        - |  1118 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1119 | `	"   if( isset($previous) ){"\` |
|        - |  1120 | `	"     $this->previous = $previous;"\` |
|        - |  1121 | `	"   }"\` |
|        - |  1122 | `	"}"\` |
|        - |  1123 | `	"public function getSeverity(){"\` |
|        - |  1124 | `	"   return $this->severity;"\` |
|        - |  1125 | `    "}"\` |
|        - |  1126 | `	"}"\` |
|        - |  1127 | `	"interface Iterator {"\` |
|        - |  1128 | `	"public function current();"\` |
|        - |  1129 | `	"public function key();"\` |
|        - |  1130 | `	"public function next();"\` |
|        - |  1131 | `	"public function rewind();"\` |
|        - |  1132 | `	"public function valid();"\` |
|        - |  1133 | `	"}"\` |
|        - |  1134 | `	"interface IteratorAggregate {"\` |
|        - |  1135 | `	"public function getIterator();"\` |
|        - |  1136 | `	"}"\` |
|        - |  1137 | `	"interface Serializable {"\` |
|        - |  1138 | `	"public function serialize();"\` |
|        - |  1139 | `	"public function unserialize(string $serialized);"\` |
|        - |  1140 | `	"}"\` |
|        - |  1141 | `	"/* Directory releated IO */"\` |
|        - |  1142 | `	"class Directory {"\` |
|        - |  1143 | `	"public $handle = null;"\` |
|        - |  1144 | `	"public $path  = null;"\` |
|        - |  1145 | `	"public function __construct(string $path)"\` |
|        - |  1146 | `	"{"\` |
|        - |  1147 | `	"   $this->handle = opendir($path);"\` |
|        - |  1148 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1149 | `	"      $this->path = $path;"\` |
|        - |  1150 | `	"   }"\` |
|        - |  1151 | `	"}"\` |
|        - |  1152 | `	"public function __destruct()"\` |
|        - |  1153 | `	"{"\` |
|        - |  1154 | `	"  if( $this->handle != null ){"\` |
|        - |  1155 | `	"       closedir($this->handle);"\` |
|        - |  1156 | `	"  }"\` |
|        - |  1157 | `	"}"\` |
|        - |  1158 | `	"public function read()"\` |
|        - |  1159 | `	"{"\` |
|        - |  1160 | `	"    return readdir($this->handle);"\` |
|        - |  1161 | `	"}"\` |
|        - |  1162 | `	"public function rewind()"\` |
|        - |  1163 | `	"{"\` |
|        - |  1164 | `	"    rewinddir($this->handle);"\` |
|        - |  1165 | `	"}"\` |
|        - |  1166 | `	"public function close()"\` |
|        - |  1167 | `	"{"\` |
|        - |  1168 | `	"    closedir($this->handle);"\` |
|        - |  1169 | `	"    $this->handle = null;"\` |
|        - |  1170 | `	"}"\` |
|        - |  1171 | `	"}"\` |
|        - |  1172 | `	"class Fiber {"\` |
|        - |  1173 | `	"  private $__ctx;"\` |
|        - |  1174 | `	"  private $__callable;"\` |
|        - |  1175 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1176 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1177 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1178 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1179 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1180 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1181 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1182 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1183 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1184 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1185 | `	"}"\` |
|        - |  1186 | `	"class Generator implements Iterator {"\` |
|        - |  1187 | `	"  private $__ctx;"\` |
|        - |  1188 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1189 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1190 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1191 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1192 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1193 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1194 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1195 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1196 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1197 | `	"}"\` |
|        - |  1198 | `	"class stdClass{"\` |
|        - |  1199 | `	"  public $value;"\` |
|        - |  1200 | `	" /* Magic methods */"\` |
|        - |  1201 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1202 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1203 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1204 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1205 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1206 | `	"}"\` |
|        - |  1207 | `	"function dir(string $path){"\` |
|        - |  1208 | `	"   return new Directory($path);"\` |
|        - |  1209 | `	"}"\` |
|        - |  1210 | `	"function Dir(string $path){"\` |
|        - |  1211 | `	"   return new Directory($path);"\` |
|        - |  1212 | `	"}"\` |
|        - |  1213 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1214 | `    "{"\` |
|        - |  1215 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1216 | `	"  $aDir = array();"\` |
|        - |  1217 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1218 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1219 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1220 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1221 | `	"   }"\` |
|        - |  1222 | `	"  closedir($pHandle);"\` |
|        - |  1223 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1224 | `	"      rsort($aDir);"\` |
|        - |  1225 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1226 | `	"      sort($aDir);"\` |
|        - |  1227 | `	"  }"\` |
|        - |  1228 | `	"  return $aDir;"\` |
|        - |  1229 | `	"}"\` |
|        - |  1230 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1231 | `	"/* Open the target directory */"\` |
|        - |  1232 | `	"$zDir = dirname($pattern);"\` |
|        - |  1233 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1234 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1235 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1236 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1237 | `	"	return FALSE;"\` |
|        - |  1238 | `	"}"\` |
|        - |  1239 | `	"$pattern = basename($pattern);"\` |
|        - |  1240 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1241 | `	"/* Loop throw available entries */"\` |
|        - |  1242 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1243 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1244 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1245 | `	"	if( $rc ){"\` |
|        - |  1246 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1247 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1248 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1249 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1250 | `	"		  }"\` |
|        - |  1251 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1252 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1253 | `	"		 continue;"\` |
|        - |  1254 | `	"	   }"\` |
|        - |  1255 | `	"	   /* Add the entry */"\` |
|        - |  1256 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1257 | `	"	}"\` |
|        - |  1258 | `	" }"\` |
|        - |  1259 | `	"/* Close the handle */"\` |
|        - |  1260 | `	"closedir($pHandle);"\` |
|        - |  1261 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1262 | `	"  /* Sort the array */"\` |
|        - |  1263 | `	"  sort($pArray);"\` |
|        - |  1264 | `	"}"\` |
|        - |  1265 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1266 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1267 | `	"  $pArray[] = $pattern;"\` |
|        - |  1268 | `	"}"\` |
|        - |  1269 | `	"/* Return the created array */"\` |
|        - |  1270 | `	"return $pArray;"\` |
|        - |  1271 | `   "}"\` |
|        - |  1272 | `   "/* Creates a temporary file */"\` |
|        - |  1273 | `   "function tmpfile(){"\` |
|        - |  1274 | `   "  /* Extract the temp directory */"\` |
|        - |  1275 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1276 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1277 | `   "    /* Use the current dir */"\` |
|        - |  1278 | `   "    $zTempDir = '.';"\` |
|        - |  1279 | `   "  }"\` |
|        - |  1280 | `   "  /* Create the file */"\` |
|        - |  1281 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1282 | `   "  return $pHandle;"\` |
|        - |  1283 | `   "}"\` |
|        - |  1284 | `   "/* Creates a temporary filename */"\` |
|        - |  1285 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1286 | `   "{"\` |
|        - |  1287 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1288 | `   "}"\` |
|        - |  1289 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1290 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1291 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1292 | `   "/* Copy arguments */"\` |
|        - |  1293 | `   "$nArgs = func_num_args();"\` |
|        - |  1294 | `   "$pNew = array();"\` |
|        - |  1295 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1296 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1297 | `    "}"\` |
|        - |  1298 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1299 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1300 | `	"/* Erase */"\` |
|        - |  1301 | `	"array_erase($pArray);"\` |
|        - |  1302 | `	"/* Unshift */"\` |
|        - |  1303 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1304 | `	"return sizeof($pArray);"\` |
|        - |  1305 | `    "}"\` |
|        - |  1306 | `	"function array_merge_recursive(){"\` |
|        - |  1307 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1308 | `    "$arrays = func_get_args();"\` |
|        - |  1309 | `    "$narrays = count($arrays);"\` |
|        - |  1310 | `    "$ret = array();"\` |
|        - |  1311 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1312 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1313 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1314 | `	 " }"\` |
|        - |  1315 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1316 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1317 | `     "  if( $keyIsInt ) {"\` |
|        - |  1318 | `     "   $ret[] = $value;"\` |
|        - |  1319 | `     "  } else {"\` |
|        - |  1320 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1321 | `     "    $cur = $ret[$key];"\` |
|        - |  1322 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1323 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1324 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1325 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1326 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1327 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1328 | `     "    } else {"\` |
|        - |  1329 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1330 | `     "    }"\` |
|        - |  1331 | `     "   } else {"\` |
|        - |  1332 | `     "    $ret[$key] = $value;"\` |
|        - |  1333 | `     "   }"\` |
|        - |  1334 | `     "  }"\` |
|        - |  1335 | `     " }"\` |
|        - |  1336 | `	 " }"\` |
|        - |  1337 | `	 " return $ret;"\` |
|        - |  1338 | `    "}"\` |
|        - |  1339 | `	"function max(){"\` |
|        - |  1340 | `    "  $pArgs = func_get_args();"\` |
|        - |  1341 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1342 | `	"  return null;"\` |
|        - |  1343 | `    " }"\` |
|        - |  1344 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1345 | `    " $pArg = $pArgs[0];"\` |
|        - |  1346 | `	" if( !is_array($pArg) ){"\` |
|        - |  1347 | `	"   return $pArg; "\` |
|        - |  1348 | `	" }"\` |
|        - |  1349 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1350 | `	"   return null;"\` |
|        - |  1351 | `	" }"\` |
|        - |  1352 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1353 | `	" reset($pArg);"\` |
|        - |  1354 | `	" $max = current($pArg);"\` |
|        - |  1355 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1356 | `	"   if( $val > $max ){"\` |
|        - |  1357 | `	"     $max = $val;"\` |
|        - |  1358 | `    " }"\` |
|        - |  1359 | `	" }"\` |
|        - |  1360 | `	" return $max;"\` |
|        - |  1361 | `    " }"\` |
|        - |  1362 | `    " $max = $pArgs[0];"\` |
|        - |  1363 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1364 | `    " $val = $pArgs[$i];"\` |
|        - |  1365 | `	"if( $val > $max ){"\` |
|        - |  1366 | `	" $max = $val;"\` |
|        - |  1367 | `	"}"\` |
|        - |  1368 | `    " }"\` |
|        - |  1369 | `	" return $max;"\` |
|        - |  1370 | `    "}"\` |
|        - |  1371 | `	"function min(){"\` |
|        - |  1372 | `    "  $pArgs = func_get_args();"\` |
|        - |  1373 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1374 | `	"  return null;"\` |
|        - |  1375 | `    " }"\` |
|        - |  1376 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1377 | `    " $pArg = $pArgs[0];"\` |
|        - |  1378 | `	" if( !is_array($pArg) ){"\` |
|        - |  1379 | `	"   return $pArg; "\` |
|        - |  1380 | `	" }"\` |
|        - |  1381 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1382 | `	"   return null;"\` |
|        - |  1383 | `	" }"\` |
|        - |  1384 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1385 | `	" reset($pArg);"\` |
|        - |  1386 | `	" $min = current($pArg);"\` |
|        - |  1387 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1388 | `	"   if( $val < $min ){"\` |
|        - |  1389 | `	"     $min = $val;"\` |
|        - |  1390 | `    " }"\` |
|        - |  1391 | `	" }"\` |
|        - |  1392 | `	" return $min;"\` |
|        - |  1393 | `    " }"\` |
|        - |  1394 | `    " $min = $pArgs[0];"\` |
|        - |  1395 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1396 | `    " $val = $pArgs[$i];"\` |
|        - |  1397 | `	"if( $val < $min ){"\` |
|        - |  1398 | `	" $min = $val;"\` |
|        - |  1399 | `	" }"\` |
|        - |  1400 | `    " }"\` |
|        - |  1401 | `	" return $min;"\` |
|        - |  1402 | `	"}"\` |
|        - |  1403 | `	"function fileowner(string $file){"\` |
|        - |  1404 | `    " $a = stat($file);"\` |
|        - |  1405 | `	" if( !is_array($a) ){"\` |
|        - |  1406 | `	"	return false;"\` |
|        - |  1407 | `	" }"\` |
|        - |  1408 | `	" return $a['uid'];"\` |
|        - |  1409 | `    "}"\` |
|        - |  1410 | `    "function filegroup(string $file){"\` |
|        - |  1411 | `	" $a = stat($file);"\` |
|        - |  1412 | `	" if( !is_array($a) ){"\` |
|        - |  1413 | `	"	return false;"\` |
|        - |  1414 | `	" }"\` |
|        - |  1415 | `	" return $a['gid'];"\` |
|        - |  1416 | `    "}"\` |
|        - |  1417 | `	 "function fileinode(string $file){"\` |
|        - |  1418 | `	" $a = stat($file);"\` |
|        - |  1419 | `	" if( !is_array($a) ){"\` |
|        - |  1420 | `	"	return false;"\` |
|        - |  1421 | `	" }"\` |
|        - |  1422 | `	" return $a['ino'];"\` |
|        - |  1423 | `    "}"` |
|        - |  1424 |  |
|        - |  1425 | `/*` |
|        - |  1426 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1427 | ` * start compiling the target PHP program.` |
|        - |  1428 | ` */` |
|     2936 |  1429 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1430 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1431 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1432 | `	 )` |
|        2 |  1433 |  |
|        - |  1434 | `	SyString sBuiltin;` |
|        - |  1435 | `	ph7_value *pObj;` |
|        - |  1436 | `	sxi32 rc;` |
|        - |  1437 | `	/* Zero the structure */` |
|     2938 |  1438 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1439 | `	/* Initialize VM fields */` |
|     2938 |  1440 | `	pVm->pEngine = &(*pEngine);` |
|     2938 |  1441 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1442 | `	/* Instructions containers */` |
|     2938 |  1443 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2938 |  1444 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2938 |  1445 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1446 | `	/* Object containers */` |
|     2938 |  1447 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2938 |  1448 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1449 | `	/* Virtual machine internal containers */` |
|     2938 |  1450 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2938 |  1451 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2938 |  1452 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2938 |  1453 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2938 |  1454 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2938 |  1455 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2938 |  1456 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2938 |  1457 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2938 |  1458 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2938 |  1459 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     2938 |  1460 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2938 |  1461 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2938 |  1462 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2938 |  1463 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2938 |  1464 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2938 |  1465 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2938 |  1466 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2938 |  1467 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     2938 |  1468 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     2938 |  1469 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     2938 |  1470 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2938 |  1471 | `	pVm->pPendingException = 0;` |
|        - |  1472 | `	/* Configuration containers */` |
|     2938 |  1473 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2938 |  1474 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2938 |  1475 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2938 |  1476 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2938 |  1477 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2938 |  1478 | `	pVm->iResponseStatus = 200;` |
|     2938 |  1479 | `	pVm->bHeadersSent = 0;` |
|     2938 |  1480 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1481 | `	/* Error callbacks containers */` |
|     2938 |  1482 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2938 |  1483 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2938 |  1484 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2938 |  1485 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2938 |  1486 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1487 | `	/* Set a default recursion limit */` |
|        - |  1488 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2938 |  1489 | `	pVm->nMaxDepth = 32;` |
|        - |  1490 | `#else` |
|        - |  1491 | `	pVm->nMaxDepth = 16;` |
|        - |  1492 | `#endif` |
|        - |  1493 | `	/* Default assertion flags */` |
|     2938 |  1494 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1495 | `	/* JSON return status */` |
|     2938 |  1496 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1497 | `	/* PRNG context */` |
|     2938 |  1498 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1499 | `	/* Install the null constant */` |
|     2938 |  1500 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2938 |  1501 | `	if( pObj == 0 ){` |
|      ! 0 |  1502 | `		rc = SXERR_MEM;` |
|      ! 0 |  1503 | `		goto Err;` |
|        - |  1504 | `	}` |
|     2938 |  1505 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1506 | `	/* Install the boolean TRUE constant */` |
|     2938 |  1507 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2938 |  1508 | `	if( pObj == 0 ){` |
|      ! 0 |  1509 | `		rc = SXERR_MEM;` |
|      ! 0 |  1510 | `		goto Err;` |
|        - |  1511 | `	}` |
|     2938 |  1512 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1513 | `	/* Install the boolean FALSE constant */` |
|     2938 |  1514 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2938 |  1515 | `	if( pObj == 0 ){` |
|      ! 0 |  1516 | `		rc = SXERR_MEM;` |
|      ! 0 |  1517 | `		goto Err;` |
|        - |  1518 | `	}` |
|     2938 |  1519 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1520 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1521 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1522 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2938 |  1523 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2938 |  1524 | `	if( pObj == 0 ){` |
|      ! 0 |  1525 | `		rc = SXERR_MEM;` |
|      ! 0 |  1526 | `		goto Err;` |
|        - |  1527 | `	}` |
|     2938 |  1528 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1529 | `	/* Create the global frame */` |
|     2938 |  1530 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2938 |  1531 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1532 | `		goto Err;` |
|        - |  1533 | `	}` |
|        - |  1534 | `	/* Initialize the code generator */` |
|     2938 |  1535 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2938 |  1536 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1537 | `		goto Err;` |
|        - |  1538 | `	}` |
|        - |  1539 | `	/* VM correctly initialized,set the magic number */` |
|     2938 |  1540 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2938 |  1541 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1542 | `	/* Compile the built-in library */` |
|     2938 |  1543 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1544 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2938 |  1545 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1546 | `	/* Register Fiber internal C functions */` |
|     2938 |  1547 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2938 |  1548 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2938 |  1549 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2938 |  1550 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2938 |  1551 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2938 |  1552 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2938 |  1553 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2938 |  1554 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2938 |  1555 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2938 |  1556 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1557 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2938 |  1558 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2938 |  1559 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2938 |  1560 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2938 |  1561 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2938 |  1562 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2938 |  1563 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2938 |  1564 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2938 |  1565 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2938 |  1566 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2938 |  1567 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1568 | `	/* Reset the code generator */` |
|     2938 |  1569 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2938 |  1570 | `	return SXRET_OK;` |
|      ! 0 |  1571 | `Err:` |
|      ! 0 |  1572 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1573 | `	return rc;` |
|     1470 |  1574 |  |
|        - |  1575 | `/*` |
|        - |  1576 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1577 | ` * routine which store the output in an internal blob.` |
|        - |  1578 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1579 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1580 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1581 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1582 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1583 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1584 | ` * to finish executing and extracting the output.` |
|        - |  1585 | ` */` |
|       38 |  1586 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1587 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1588 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1589 | `	void *pUserData     /* User private data */` |
|        - |  1590 | `	)` |
|      ! 0 |  1591 |  |
|        - |  1592 | `	 sxi32 rc;` |
|        - |  1593 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1594 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1595 | `	 return rc;` |
|      ! 0 |  1596 |  |
|        - |  1597 | `/*` |
|        - |  1598 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1599 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1600 | ` */` |
|    17266 |  1601 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1602 |  |
|    17268 |  1603 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    17268 |  1604 | `	if( xCons != VmObConsumer ){` |
|     7206 |  1605 | `		pVm->nOutputLen += nLen;` |
|     7206 |  1606 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      922 |  1607 | `			pVm->bHeadersSent = 1;` |
|      460 |  1608 | `		}` |
|     3602 |  1609 | `	}` |
|    17268 |  1610 |  |
|        - |  1611 | `#define VM_STACK_GUARD 16` |
|        - |  1612 | `/*` |
|        - |  1613 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1614 | ` * our compiled PHP program.` |
|        - |  1615 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1616 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1617 | ` */` |
|    40234 |  1618 | `static ph7_value * VmNewOperandStack(` |
|        - |  1619 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1620 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1621 | `	)` |
|        2 |  1622 |  |
|        - |  1623 | `	ph7_value *pStack;` |
|        - |  1624 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1625 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1626 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1627 | `  ** on the maximum stack depth required.` |
|        - |  1628 | `  **` |
|        - |  1629 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1630 | `  */` |
|    40236 |  1631 | `	nInstr += VM_STACK_GUARD;` |
|    40236 |  1632 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    40236 |  1633 | `	if( pStack == 0 ){` |
|      ! 0 |  1634 | `		return 0;` |
|        - |  1635 | `	}` |
|        - |  1636 | `	/* Initialize the operand stack */` |
|  2829390 |  1637 | `	while( nInstr > 0 ){` |
|  2789156 |  1638 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2789156 |  1639 | `		--nInstr;` |
|        2 |  1640 | `	}` |
|        - |  1641 | `	/* Ready for bytecode execution */` |
|    40236 |  1642 | `	return pStack;` |
|    20119 |  1643 |  |
|        - |  1644 | `/* Forward declaration */` |
|        - |  1645 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1646 | `/*` |
|        - |  1647 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1648 | ` * This routine gets called by the PH7 engine after` |
|        - |  1649 | ` * successful compilation of the target PHP program.` |
|        - |  1650 | ` */` |
|     2622 |  1651 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1652 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1653 | `	)` |
|        2 |  1654 |  |
|        - |  1655 | `	SyHashEntry *pEntry;` |
|        - |  1656 | `	sxi32 rc;` |
|     2624 |  1657 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1658 | `		/* Initialize your VM first */` |
|      ! 0 |  1659 | `		return SXERR_CORRUPT;` |
|        - |  1660 | `	}` |
|        - |  1661 | `	/* Mark the VM ready for byte-code execution */` |
|     2624 |  1662 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1663 | `	/* Release the code generator now we have compiled our program */` |
|     2624 |  1664 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1665 | `	/* Emit the DONE instruction */` |
|     2624 |  1666 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2624 |  1667 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1668 | `		return SXERR_MEM;` |
|        - |  1669 | `	}` |
|        - |  1670 | `	/* Script return value */` |
|     2624 |  1671 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1672 | `	/* Allocate a new operand stack */` |
|     2624 |  1673 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2624 |  1674 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1675 | `		return SXERR_MEM;` |
|        - |  1676 | `	}` |
|        - |  1677 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1678 | `	 * private data. */` |
|     2624 |  1679 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2624 |  1680 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1681 | `	/* Allocate the reference table */` |
|     2624 |  1682 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2624 |  1683 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2624 |  1684 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1685 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1686 | `		return SXERR_MEM;` |
|        - |  1687 | `	}` |
|        - |  1688 | `	/* Zero the reference table */` |
|     2624 |  1689 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1690 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2624 |  1691 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2624 |  1692 | `	if( rc != SXRET_OK ){` |
|        - |  1693 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1694 | `		return rc;` |
|        - |  1695 | `	}` |
|        - |  1696 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2624 |  1697 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2624 |  1698 | `	if( rc != SXRET_OK ){` |
|        - |  1699 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1700 | `		return rc;` |
|        - |  1701 | `	}` |
|        - |  1702 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2624 |  1703 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1704 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2624 |  1705 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1706 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2624 |  1707 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1708 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1709 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2624 |  1710 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2624 |  1711 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1712 | `#endif` |
|        - |  1713 | `	/* Initialize and install static and constants class attributes */` |
|     2624 |  1714 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    50082 |  1715 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    47460 |  1716 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    47460 |  1717 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1718 | `			return rc;` |
|        - |  1719 | `		}` |
|        2 |  1720 | `	}` |
|        - |  1721 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2624 |  1722 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1723 | `	/* VM is ready for bytecode execution */` |
|     2624 |  1724 | `	return SXRET_OK;` |
|     1313 |  1725 |  |
|        - |  1726 | `/*` |
|        - |  1727 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1728 | ` */` |
|      ! 0 |  1729 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1730 |  |
|      ! 0 |  1731 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1732 | `		return SXERR_CORRUPT;` |
|        - |  1733 | `	}` |
|        - |  1734 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1735 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1736 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1737 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1738 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1739 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1740 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1741 | `	pVm->bHttpContext = 0;` |
|        - |  1742 | `	/* Set the ready flag */` |
|      ! 0 |  1743 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1744 | `	return SXRET_OK;` |
|      ! 0 |  1745 |  |
|        - |  1746 | `/*` |
|        - |  1747 | ` * Release a Virtual Machine.` |
|        - |  1748 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1749 | ` */` |
|     2614 |  1750 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1751 |  |
|        - |  1752 | `	/* Set the stale magic number */` |
|     2616 |  1753 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1754 | `	/* Release the private memory subsystem */` |
|     2616 |  1755 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2616 |  1756 | `	return SXRET_OK;` |
|        2 |  1757 |  |
|        - |  1758 | `/*` |
|        - |  1759 | ` * Initialize a foreign function call context.` |
|        - |  1760 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1761 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1762 | ` * functions.` |
|        - |  1763 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1764 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1765 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1766 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1767 | ` */` |
|   651366 |  1768 | `static sxi32 VmInitCallContext(` |
|        - |  1769 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1770 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1771 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1772 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1773 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1774 | `	)` |
|        2 |  1775 |  |
|   651368 |  1776 | `	pOut->pFunc = pFunc;` |
|   651368 |  1777 | `	pOut->pVm   = pVm;` |
|   651368 |  1778 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   651368 |  1779 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1780 | `	/* Assume a null return value */` |
|   651368 |  1781 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   651368 |  1782 | `	pOut->pRet = pRet;` |
|   651368 |  1783 | `	pOut->iFlags = iFlags;` |
|   651368 |  1784 | `	return SXRET_OK;` |
|        2 |  1785 |  |
|        - |  1786 | `/*` |
|        - |  1787 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1788 | ` * left behind.` |
|        - |  1789 | ` */` |
|   651366 |  1790 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1791 |  |
|        - |  1792 | `	sxu32 n;` |
|   651368 |  1793 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     7930 |  1794 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    22966 |  1795 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    15038 |  1796 | `			if( apObj[n] == 0 ){` |
|        - |  1797 | `				/* Already released */` |
|      298 |  1798 | `				continue;` |
|        - |  1799 | `			}` |
|    14742 |  1800 | `			PH7_MemObjRelease(apObj[n]);` |
|    14742 |  1801 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     7372 |  1802 | `		}` |
|     7930 |  1803 | `		SySetRelease(&pCtx->sVar);` |
|     3964 |  1804 | `	}` |
|   651368 |  1805 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1806 | `		ph7_aux_data *aAux;` |
|        - |  1807 | `		void *pChunk;` |
|        - |  1808 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1809 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1810 | `		 */` |
|        9 |  1811 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1812 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1813 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1814 | `			/* Release the chunk */` |
|       25 |  1815 | `			if( pChunk ){` |
|       25 |  1816 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1817 | `			}` |
|       13 |  1818 | `		}` |
|        9 |  1819 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1820 | `	}` |
|   651368 |  1821 |  |
|        - |  1822 | `/*` |
|        - |  1823 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1824 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1825 | ` */` |
|      296 |  1826 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1827 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1828 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1829 | `	)` |
|        2 |  1830 |  |
|      298 |  1831 | `	if( pValue == 0 ){` |
|        - |  1832 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1833 | `		return;` |
|        - |  1834 | `	}` |
|      298 |  1835 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      298 |  1836 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1837 | `		sxu32 n;` |
|     1054 |  1838 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1054 |  1839 | `			if( apObj[n] == pValue ){` |
|      298 |  1840 | `				PH7_MemObjRelease(pValue);` |
|      298 |  1841 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1842 | `				/* Mark as released */` |
|      298 |  1843 | `				apObj[n] = 0;` |
|      298 |  1844 | `				break;` |
|        - |  1845 | `			}` |
|      380 |  1846 | `		}` |
|      148 |  1847 | `	}` |
|      150 |  1848 |  |
|        - |  1849 | `/*` |
|        - |  1850 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1851 | ` */` |
|  3738162 |  1852 | `static void VmPopOperand(` |
|        - |  1853 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1854 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1855 | `	)` |
|        2 |  1856 |  |
|  3738164 |  1857 | `	ph7_value *pTos = *ppTos;` |
|  7954732 |  1858 | `	while( nPop > 0 ){` |
|  4216570 |  1859 | `		PH7_MemObjRelease(pTos);` |
|  4216570 |  1860 | `		pTos--;` |
|  4216570 |  1861 | `		nPop--;` |
|        2 |  1862 | `	}` |
|        - |  1863 | `	/* Top of the stack */` |
|  3738164 |  1864 | `	*ppTos = pTos;` |
|  3738164 |  1865 |  |
|        - |  1866 | `/*` |
|        - |  1867 | ` * Reserve a memory object.` |
|        - |  1868 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1869 | ` */` |
|  3127186 |  1870 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1871 |  |
|  3127188 |  1872 | `	ph7_value *pObj = 0;` |
|        - |  1873 | `	VmSlot *pSlot;` |
|        - |  1874 | `	sxu32 nIdx;` |
|        - |  1875 | `	/* Check for a free slot */` |
|  3127188 |  1876 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3127188 |  1877 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3127188 |  1878 | `	if( pSlot ){` |
|   979204 |  1879 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   979204 |  1880 | `		nIdx = pSlot->nIdx;` |
|   489601 |  1881 | `	}` |
|  3127188 |  1882 | `	if( pObj == 0 ){` |
|        - |  1883 | `		/* Reserve a new memory object */` |
|  2147986 |  1884 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2147986 |  1885 | `		if( pObj == 0 ){` |
|      ! 0 |  1886 | `			return 0;` |
|        - |  1887 | `		}` |
|  1073992 |  1888 | `	}` |
|        - |  1889 | `	/* Set a null default value */` |
|  3127188 |  1890 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3127188 |  1891 | `	pObj->nIdx = nIdx;` |
|  3127188 |  1892 | `	return pObj;` |
|  1563595 |  1893 |  |
|        - |  1894 | `/*` |
|        - |  1895 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1896 | ` */` |
|    33702 |  1897 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1898 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1899 | `	const char *zKey,  /* Entry key */` |
|        - |  1900 | `	sxu32 nByte,       /* Key length */` |
|        - |  1901 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1902 | `	)` |
|        2 |  1903 |  |
|        - |  1904 | `	ph7_value sKey;` |
|        - |  1905 | `	sxi32 rc;` |
|    33704 |  1906 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    33704 |  1907 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1908 | `	/* Perform the insertion */` |
|    33704 |  1909 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    33704 |  1910 | `	PH7_MemObjRelease(&sKey);` |
|    33704 |  1911 | `	return rc;` |
|        2 |  1912 |  |
|        - |  1913 | `/*` |
|        - |  1914 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1915 | ` * Return a pointer to the variable value on success.` |
|        - |  1916 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1917 | ` */` |
|  3482226 |  1918 | `static ph7_value * VmExtractMemObj(` |
|        - |  1919 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1920 | `	const SyString *pName, /* Variable name */` |
|        - |  1921 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1922 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1923 | `	)` |
|        2 |  1924 |  |
|  3482228 |  1925 | `	int bNullify = FALSE;` |
|        - |  1926 | `	SyHashEntry *pEntry;` |
|        - |  1927 | `	VmFrame *pFrame;` |
|        - |  1928 | `	ph7_value *pObj;` |
|        - |  1929 | `	sxu32 nIdx;` |
|        - |  1930 | `	sxi32 rc;` |
|        - |  1931 | `	/* Point to the top active frame */` |
|  3482228 |  1932 | `	pFrame = pVm->pFrame;` |
|  3482228 |  1933 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1934 | `	/* Perform the lookup */` |
|  3482228 |  1935 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1936 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1937 | `		pName = &sAnnon;` |
|        - |  1938 | `		/* Always nullify the object */` |
|      ! 0 |  1939 | `		bNullify = TRUE;` |
|      ! 0 |  1940 | `		bDup = FALSE;` |
|      ! 0 |  1941 | `	}` |
|        - |  1942 | `	/* Check the superglobals table first */` |
|  3482228 |  1943 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3482228 |  1944 | `	if( pEntry == 0 ){` |
|        - |  1945 | `		/* Query the top active frame */` |
|  3482188 |  1946 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3482188 |  1947 | `		if( pEntry == 0 ){` |
|   105018 |  1948 | `			char *zName = (char *)pName->zString;` |
|        - |  1949 | `			VmSlot sLocal;` |
|   105018 |  1950 | `			if( !bCreate ){` |
|        - |  1951 | `				/* Do not create the variable,return NULL instead */` |
|      118 |  1952 | `				return 0;` |
|        - |  1953 | `			}` |
|        - |  1954 | `			/* No such variable,automatically create a new one and install` |
|        - |  1955 | `			 * it in the current frame.` |
|        - |  1956 | `			 */` |
|   104902 |  1957 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   104902 |  1958 | `			if( pObj == 0 ){` |
|      ! 0 |  1959 | `				return 0;` |
|        - |  1960 | `			}` |
|   104902 |  1961 | `			nIdx = pObj->nIdx;` |
|   104902 |  1962 | `			if( bDup ){` |
|        - |  1963 | `				/* Duplicate name */` |
|      172 |  1964 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      172 |  1965 | `				if( zName == 0 ){` |
|      ! 0 |  1966 | `					return 0;` |
|        - |  1967 | `				}` |
|       85 |  1968 | `			}` |
|        - |  1969 | `			/* Link to the top active VM frame */` |
|   104902 |  1970 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   104902 |  1971 | `			if( rc != SXRET_OK ){` |
|        - |  1972 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1973 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1974 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1975 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1976 | `				return 0;` |
|        - |  1977 | `			}` |
|   104902 |  1978 | `			if( pFrame->pParent != 0 ){` |
|        - |  1979 | `				/* Local variable */` |
|    97448 |  1980 | `				sLocal.nIdx = nIdx;` |
|    97448 |  1981 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    48725 |  1982 | `			}else{` |
|        - |  1983 | `				/* Register in the $GLOBALS array */` |
|     7456 |  1984 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1985 | `			}` |
|        - |  1986 | `			/* Install in the reference table */` |
|   104902 |  1987 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1988 | `			/* Save object index */` |
|   104902 |  1989 | `			pObj->nIdx = nIdx;` |
|    52452 |  1990 | `		}else{` |
|        - |  1991 | `			/* Extract variable contents */` |
|  3377172 |  1992 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3377172 |  1993 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3377172 |  1994 | `			if( bNullify && pObj ){` |
|      ! 0 |  1995 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1996 | `			}` |
|        - |  1997 | `		}` |
|  1741147 |  1998 | `	}else{` |
|        - |  1999 | `		/* Superglobal */` |
|       42 |  2000 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  2001 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  2002 | `	}` |
|  3482112 |  2003 | `	return pObj;` |
|  1741225 |  2004 |  |
|        - |  2005 | `/*` |
|        - |  2006 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  2007 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  2008 | ` */` |
|     2926 |  2009 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  2010 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  2011 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  2012 | `	sxu32 nByte        /* zName length */` |
|        - |  2013 | `	)` |
|        2 |  2014 |  |
|        - |  2015 | `	SyHashEntry *pEntry;` |
|        - |  2016 | `	ph7_value *pValue;` |
|        - |  2017 | `	sxu32 nIdx;` |
|        - |  2018 | `	/* Query the superglobal table */` |
|     2928 |  2019 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2928 |  2020 | `	if( pEntry == 0 ){` |
|        - |  2021 | `		/* No such entry */` |
|      ! 0 |  2022 | `		return 0;` |
|        - |  2023 | `	}` |
|        - |  2024 | `	/* Extract the superglobal index in the global object pool */` |
|     2928 |  2025 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2026 | `	/* Extract the variable value  */` |
|     2928 |  2027 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2928 |  2028 | `	return pValue;` |
|     1465 |  2029 |  |
|        - |  2030 | `/*` |
|        - |  2031 | ` * Perform a raw hashmap insertion.` |
|        - |  2032 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  2033 | ` */` |
|     2956 |  2034 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  2035 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  2036 | `	const char *zKey,   /* Entry key */` |
|        - |  2037 | `	int nKeylen,        /* zKey length*/` |
|        - |  2038 | `	const char *zData,  /* Entry data */` |
|        - |  2039 | `	int nLen            /* zData length */` |
|        - |  2040 | `	)` |
|        2 |  2041 |  |
|        - |  2042 | `	ph7_value sKey,sValue;` |
|        - |  2043 | `	sxi32 rc;` |
|     2958 |  2044 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2958 |  2045 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2958 |  2046 | `	if( zKey ){` |
|     2936 |  2047 | `		if( nKeylen < 0 ){` |
|     2884 |  2048 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1441 |  2049 | `		}` |
|     2936 |  2050 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1467 |  2051 | `	}` |
|     2958 |  2052 | `	if( zData ){` |
|     2958 |  2053 | `		if( nLen < 0 ){` |
|        - |  2054 | `			/* Compute length automatically */` |
|      144 |  2055 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  2056 | `		}` |
|     2958 |  2057 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1478 |  2058 | `	}` |
|        - |  2059 | `	/* Perform the insertion */` |
|     2958 |  2060 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2958 |  2061 | `	PH7_MemObjRelease(&sKey);` |
|     2958 |  2062 | `	PH7_MemObjRelease(&sValue);` |
|     2958 |  2063 | `	return rc;` |
|        2 |  2064 |  |
|        - |  2065 | `/*` |
|        - |  2066 | ` * Configure a working virtual machine instance.` |
|        - |  2067 | ` *` |
|        - |  2068 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  2069 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  2070 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  2071 | ` * The second argument to this function is an integer configuration option` |
|        - |  2072 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  2073 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  2074 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  2075 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  2076 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  2077 | ` */` |
|    42282 |  2078 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  2079 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  2080 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  2081 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  2082 | `	)` |
|        2 |  2083 |  |
|    42284 |  2084 | `	sxi32 rc = SXRET_OK;` |
|    42284 |  2085 | `	switch(nOp){` |
|     1303 |  2086 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2608 |  2087 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2608 |  2088 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  2089 | `		/* VM output consumer callback */` |
|        - |  2090 | `#ifdef UNTRUST` |
|        - |  2091 | `		if( xConsumer == 0 ){` |
|        - |  2092 | `			rc = SXERR_CORRUPT;` |
|        - |  2093 | `			break;` |
|        - |  2094 | `		}` |
|        - |  2095 | `#endif` |
|        - |  2096 | `		/* Install the output consumer */` |
|     2608 |  2097 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2608 |  2098 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2608 |  2099 | `		break;` |
|        - |  2100 | `							   }` |
|     1311 |  2101 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2102 | `		/* Import path */` |
|        - |  2103 | `		  const char *zPath;` |
|        - |  2104 | `		  SyString sPath;` |
|     2624 |  2105 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2106 | `#if defined(UNTRUST)` |
|        - |  2107 | `		  if( zPath == 0 ){` |
|        - |  2108 | `			  rc = SXERR_EMPTY;` |
|        - |  2109 | `			  break;` |
|        - |  2110 | `		  }` |
|        - |  2111 | `#endif` |
|     2624 |  2112 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2113 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2114 | `#ifdef __WINNT__` |
|        2 |  2115 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2116 | `#endif` |
|     5246 |  2117 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2118 | `		  /* Remove leading and trailing white spaces */` |
|     2624 |  2119 | `		  SyStringFullTrim(&sPath);` |
|     2624 |  2120 | `		  if( sPath.nByte > 0 ){` |
|        - |  2121 | `			  /* Store the path in the corresponding conatiner */` |
|     2624 |  2122 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1311 |  2123 | `		  }` |
|     2624 |  2124 | `		  break;` |
|        - |  2125 | `									 }` |
|     1311 |  2126 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2127 | `		/* Run-Time Error report */` |
|     2624 |  2128 | `		pVm->bErrReport = 1;` |
|     2624 |  2129 | `		break;` |
|      ! 0 |  2130 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2131 | `		/* Recursion depth */` |
|      ! 0 |  2132 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2133 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2134 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2135 | `		}` |
|      ! 0 |  2136 | `		break;` |
|        - |  2137 | `									   }` |
|      ! 0 |  2138 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2139 | `		/* VM output length in bytes */` |
|      ! 0 |  2140 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2141 | `#ifdef UNTRUST` |
|        - |  2142 | `		if( pOut == 0 ){` |
|        - |  2143 | `			rc = SXERR_CORRUPT;` |
|        - |  2144 | `			break;` |
|        - |  2145 | `		}` |
|        - |  2146 | `#endif` |
|      ! 0 |  2147 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2148 | `		break;` |
|        - |  2149 | `							   }` |
|        - |  2150 |  |
|    13110 |  2151 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2152 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2153 | `		/* Create a new superglobal/global variable */` |
|    26222 |  2154 | `		const char *zName = va_arg(ap,const char *);` |
|    26222 |  2155 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2156 | `		SyHashEntry *pEntry;` |
|        - |  2157 | `		ph7_value *pObj;` |
|        - |  2158 | `		sxu32 nByte;` |
|        - |  2159 | `		sxu32 nIdx;` |
|        - |  2160 | `#ifdef UNTRUST` |
|        - |  2161 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2162 | `			rc = SXERR_CORRUPT;` |
|        - |  2163 | `			break;` |
|        - |  2164 | `		}` |
|        - |  2165 | `#endif` |
|    26222 |  2166 | `		nByte = SyStrlen(zName);` |
|    26222 |  2167 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2168 | `			/* Check if the superglobal is already installed */` |
|    26222 |  2169 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    13112 |  2170 | `		}else{` |
|        - |  2171 | `			/* Query the top active VM frame */` |
|      ! 0 |  2172 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2173 | `		}` |
|    26222 |  2174 | `		if( pEntry ){` |
|        - |  2175 | `			/* Variable already installed */` |
|      ! 0 |  2176 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2177 | `			/* Extract contents */` |
|      ! 0 |  2178 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2179 | `			if( pObj ){` |
|        - |  2180 | `				/* Overwrite old contents */` |
|      ! 0 |  2181 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2182 | `			}` |
|      ! 0 |  2183 | `		}else{` |
|        - |  2184 | `			/* Install a new variable */` |
|    26222 |  2185 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    26222 |  2186 | `			if( pObj == 0 ){` |
|      ! 0 |  2187 | `				rc = SXERR_MEM;` |
|      ! 0 |  2188 | `				break;` |
|        - |  2189 | `			}` |
|    26222 |  2190 | `			nIdx = pObj->nIdx;` |
|        - |  2191 | `			/* Copy value */` |
|    26222 |  2192 | `			PH7_MemObjStore(pValue,pObj);` |
|    26222 |  2193 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2194 | `				/* Install the superglobal */` |
|    26222 |  2195 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    13112 |  2196 | `			}else{` |
|        - |  2197 | `				/* Install in the current frame */` |
|      ! 0 |  2198 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2199 | `			}` |
|    26222 |  2200 | `			if( rc == SXRET_OK ){` |
|        - |  2201 | `				SyHashEntry *pRef;` |
|    26222 |  2202 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    26222 |  2203 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    13112 |  2204 | `				}else{` |
|      ! 0 |  2205 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2206 | `				}` |
|        - |  2207 | `				/* Install in the reference table */` |
|    26222 |  2208 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    26222 |  2209 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2210 | `					/* Register in the $GLOBALS array */` |
|    26222 |  2211 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    13110 |  2212 | `				}` |
|    13110 |  2213 | `			}` |
|        - |  2214 | `		}` |
|    26222 |  2215 | `		break;` |
|        - |  2216 | `									}` |
|     1441 |  2217 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2218 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2219 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2220 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2221 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2222 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2223 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2884 |  2224 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2884 |  2225 | `		const char *zValue = va_arg(ap,const char *);` |
|     2884 |  2226 | `		int nLen = va_arg(ap,int);` |
|        - |  2227 | `		ph7_hashmap *pMap;` |
|        - |  2228 | `		ph7_value *pValue;` |
|     2884 |  2229 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2230 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2231 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2883 |  2232 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2233 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2234 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2882 |  2235 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2236 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2237 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2882 |  2238 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2239 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2240 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2882 |  2241 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2242 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2243 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2882 |  2244 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2245 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2246 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2247 | `		}else{` |
|        - |  2248 | `			/* Extract the $_SERVER superglobal */` |
|     2882 |  2249 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2250 | `		}` |
|     2884 |  2251 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2252 | `			/* No such entry */` |
|      ! 0 |  2253 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2254 | `			break;` |
|        - |  2255 | `		}` |
|        - |  2256 | `		/* Point to the hashmap */` |
|     2884 |  2257 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2258 | `		/* Perform the insertion */` |
|     2884 |  2259 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2884 |  2260 | `		break;` |
|        - |  2261 | `								   }` |
|       11 |  2262 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2263 | `		/* Script arguments */` |
|       24 |  2264 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2265 | `		ph7_hashmap *pMap;` |
|        - |  2266 | `		ph7_value *pValue;` |
|        - |  2267 | `		sxu32 n;` |
|       24 |  2268 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2269 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2270 | `			break;` |
|        - |  2271 | `		}` |
|        - |  2272 | `		/* Extract the $argv array */` |
|       24 |  2273 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2274 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2275 | `			/* No such entry */` |
|      ! 0 |  2276 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2277 | `			break;` |
|        - |  2278 | `		}` |
|        - |  2279 | `		/* Point to the hashmap */` |
|       24 |  2280 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2281 | `		/* Perform the insertion */` |
|       24 |  2282 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2283 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2284 | `		if( rc == SXRET_OK ){` |
|       24 |  2285 | `			if( pMap->nEntry > 1 ){` |
|        - |  2286 | `				/* Append space separator first */` |
|       18 |  2287 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2288 | `			}` |
|       24 |  2289 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2290 | `		}` |
|       24 |  2291 | `		break;` |
|        - |  2292 | `								  }` |
|      ! 0 |  2293 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2294 | `		/* error_log() consumer */` |
|      ! 0 |  2295 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2296 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2297 | `		break;` |
|        - |  2298 | `										}` |
|      ! 0 |  2299 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2300 | `		/* Script return value */` |
|      ! 0 |  2301 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2302 | `#ifdef UNTRUST` |
|        - |  2303 | `		if( ppValue == 0 ){` |
|        - |  2304 | `			rc = SXERR_CORRUPT;` |
|        - |  2305 | `			break;` |
|        - |  2306 | `		}` |
|        - |  2307 | `#endif` |
|      ! 0 |  2308 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2309 | `		break;` |
|        - |  2310 | `								   }` |
|     2622 |  2311 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2312 | `		/* Register an IO stream device */` |
|     5246 |  2313 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2314 | `		/* Make sure we are dealing with a valid IO stream */` |
|     7866 |  2315 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5246 |  2316 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2317 | `				/* Invalid stream */` |
|      ! 0 |  2318 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2319 | `				break;` |
|        - |  2320 | `		}` |
|     5246 |  2321 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2322 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2624 |  2323 | `			pVm->pDefStream = pStream;` |
|     1311 |  2324 | `		}` |
|        - |  2325 | `		/* Insert in the appropriate container */` |
|     5246 |  2326 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5246 |  2327 | `		break;` |
|        - |  2328 | `								  }` |
|        8 |  2329 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2330 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2331 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2332 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2333 | `#ifdef UNTRUST` |
|        - |  2334 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2335 | `			rc = SXERR_CORRUPT;` |
|        - |  2336 | `			break;` |
|        - |  2337 | `		}` |
|        - |  2338 | `#endif` |
|       16 |  2339 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2340 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2341 | `		break;` |
|        - |  2342 | `									   }` |
|        8 |  2343 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2344 | `		/* Raw HTTP request*/` |
|       16 |  2345 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2346 | `		int nByte = va_arg(ap,int);` |
|       16 |  2347 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2348 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2349 | `			break;` |
|        - |  2350 | `		}` |
|       16 |  2351 | `		if( nByte < 0 ){` |
|        - |  2352 | `			/* Compute length automatically */` |
|      ! 0 |  2353 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2354 | `		}` |
|        - |  2355 | `		/* Process the request */` |
|       16 |  2356 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2357 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2358 | `		if( rc == SXRET_OK ){` |
|       16 |  2359 | `			pVm->bHttpContext = 1;` |
|        8 |  2360 | `		}` |
|       16 |  2361 | `		break;` |
|        - |  2362 | `									}` |
|        8 |  2363 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2364 | `		/* Extract HTTP response status code */` |
|       16 |  2365 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2366 | `		if( pStatus ){` |
|       16 |  2367 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2368 | `		}` |
|       16 |  2369 | `		break;` |
|        - |  2370 | `										}` |
|        8 |  2371 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2372 | `		/* Iterate response headers via callback */` |
|        - |  2373 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2374 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2375 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2376 | `		if( xCallback ){` |
|       16 |  2377 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2378 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2379 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2380 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2381 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2382 | `							   pUserData);` |
|       12 |  2383 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2384 | `					break;` |
|        - |  2385 | `				}` |
|        6 |  2386 | `			}` |
|        8 |  2387 | `		}` |
|       16 |  2388 | `		break;` |
|        - |  2389 | `										 }` |
|      ! 0 |  2390 | `	default:` |
|        - |  2391 | `		/* Unknown configuration option */` |
|      ! 0 |  2392 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2393 | `		break;` |
|        - |  2394 | `	}` |
|    42284 |  2395 | `	return rc;` |
|        2 |  2396 |  |
|        - |  2397 | `/* Forward declaration */` |
|        - |  2398 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2399 | `/*` |
|        - |  2400 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2401 | ` * format.` |
|        - |  2402 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2403 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2404 | ` * (STDOUT).` |
|        - |  2405 | ` */` |
|        2 |  2406 | `static sxi32 VmByteCodeDump(` |
|        - |  2407 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2408 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2409 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2410 | `	)` |
|        1 |  2411 |  |
|        - |  2412 | `	static const char zDump[] = {` |
|        - |  2413 | `		"====================================================\n"` |
|        - |  2414 | `		"PH7 VM Dump\n"` |
|        - |  2415 | `		"====================================================\n"` |
|        - |  2416 | `	};` |
|        - |  2417 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2418 | `	sxi32 rc = SXRET_OK;` |
|        - |  2419 | `	sxu32 n;` |
|        - |  2420 | `	/* Point to the PH7 instructions */` |
|        3 |  2421 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2422 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2423 | `	n = 0;` |
|        3 |  2424 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2425 | `	/* Dump instructions */` |
|        7 |  2426 | `	for(;;){` |
|       15 |  2427 | `		if( pInstr >= pEnd ){` |
|        - |  2428 | `			/* No more instructions */` |
|        3 |  2429 | `			break;` |
|        - |  2430 | `		}` |
|        - |  2431 | `		/* Format and call the consumer callback */` |
|       19 |  2432 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2433 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2434 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2435 | `		if( rc != SXRET_OK ){` |
|        - |  2436 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2437 | `			return rc;` |
|        - |  2438 | `		}` |
|       13 |  2439 | `		++n;` |
|       13 |  2440 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2441 | `	}` |
|        3 |  2442 | `	return rc;` |
|        2 |  2443 |  |
|        - |  2444 | `/* Forward declaration */` |
|        - |  2445 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2446 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2447 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2448 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2449 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2450 | `/*` |
|        - |  2451 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2452 | ` * consumer callback.` |
|        - |  2453 | ` */` |
|      580 |  2454 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2455 |  |
|      581 |  2456 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      581 |  2457 | `	sxi32 rc = SXRET_OK;` |
|        - |  2458 | `	/* Append a new line */` |
|        - |  2459 | `#ifdef __WINNT__` |
|        1 |  2460 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2461 | `#else` |
|      580 |  2462 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2463 | `#endif` |
|        - |  2464 | `	/* Invoke the output consumer callback */` |
|      581 |  2465 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      581 |  2466 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      581 |  2467 | `	return rc;` |
|        1 |  2468 |  |
|        - |  2469 | `/*` |
|        - |  2470 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2471 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2472 | ` * information.` |
|        - |  2473 | ` */` |
|      136 |  2474 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2475 |  |
|      138 |  2476 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2477 | `		ph7_value apArg[4];` |
|        - |  2478 | `		ph7_value *apArgPtr[4];` |
|        - |  2479 | `		ph7_value sResult;` |
|        - |  2480 | `		SyString sErr;` |
|        - |  2481 | `		/* Prepare arguments */` |
|       64 |  2482 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2483 | `			/* use explicit message length to avoid reading past buffer */` |
|       64 |  2484 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       64 |  2485 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       64 |  2486 | `		if( pFile ){` |
|       64 |  2487 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       64 |  2488 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       33 |  2489 | `		}else{` |
|      ! 0 |  2490 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2491 | `		}` |
|       64 |  2492 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       64 |  2493 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2494 | `		/* Set up pointer array */` |
|       64 |  2495 | `		apArgPtr[0] = &apArg[0];` |
|       64 |  2496 | `		apArgPtr[1] = &apArg[1];` |
|       64 |  2497 | `		apArgPtr[2] = &apArg[2];` |
|       64 |  2498 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2499 | `		/* Call the handler */` |
|       64 |  2500 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2501 | `		/* Check return value */` |
|       64 |  2502 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2503 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2504 | `		}` |
|        - |  2505 | `		/* Release */` |
|       64 |  2506 | `		PH7_MemObjRelease(&apArg[0]);` |
|       64 |  2507 | `		PH7_MemObjRelease(&apArg[1]);` |
|       64 |  2508 | `		PH7_MemObjRelease(&apArg[2]);` |
|       64 |  2509 | `		PH7_MemObjRelease(&apArg[3]);` |
|       64 |  2510 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2511 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2512 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       64 |  2513 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2514 | `	}` |
|        - |  2515 | `	/* No handler, always call error handler */` |
|       75 |  2516 | `	return TRUE;` |
|       70 |  2517 |  |
|       98 |  2518 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2519 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2520 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2521 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2522 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2523 | `	)` |
|        2 |  2524 |  |
|      100 |  2525 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2526 | `	SyString *pFile;` |
|        - |  2527 | `	char *zErr;` |
|      100 |  2528 | `	sxi32 rc = SXRET_OK;` |
|      100 |  2529 | `	if( !pVm->bErrReport ){` |
|        - |  2530 | `		/* Don't bother reporting errors */` |
|        3 |  2531 | `		return SXRET_OK;` |
|        - |  2532 | `	}` |
|        - |  2533 | `	/* Reset the working buffer */` |
|       98 |  2534 | `	SyBlobReset(pWorker);` |
|        - |  2535 | `	/* Peek the processed file if available */` |
|       98 |  2536 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       98 |  2537 | `	if( pFile ){` |
|        - |  2538 | `		/* Append file name */` |
|       98 |  2539 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       98 |  2540 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       48 |  2541 | `	}` |
|        - |  2542 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2543 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2544 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2545 | `	 * E_DEPRECATED). */` |
|       98 |  2546 | `	zErr = "Error:  ";` |
|       98 |  2547 | `	switch(iErr){` |
|       19 |  2548 | `	case PH7_CTX_WARNING:` |
|       40 |  2549 | `		zErr = "Warning:  ";` |
|       40 |  2550 | `		break;` |
|        6 |  2551 | `	case PH7_CTX_NOTICE:` |
|       14 |  2552 | `		zErr = "Notice:  ";` |
|       12 |  2553 | `		break;` |
|       23 |  2554 | `	default:` |
|        - |  2555 | `		/* keep iErr unchanged */` |
|       46 |  2556 | `		break;` |
|        - |  2557 | `	}` |
|       98 |  2558 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       98 |  2559 | `	if( pFuncName ){` |
|        - |  2560 | `		/* Append function name first */` |
|       23 |  2561 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2562 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2563 | `	}` |
|       98 |  2564 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2565 | `	/* Check for user error handler.  compute length of C string */` |
|       98 |  2566 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2567 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2568 | `	}` |
|       98 |  2569 | `	return rc;` |
|       51 |  2570 |  |
|        - |  2571 | `/*` |
|        - |  2572 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2573 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2574 | ` * information.` |
|        - |  2575 | ` */` |
|       40 |  2576 | `static sxi32 VmThrowErrorAp(` |
|        - |  2577 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2578 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2579 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2580 | `	const char *zFormat, /* Format message */` |
|        - |  2581 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2582 | `	)` |
|        2 |  2583 |  |
|       42 |  2584 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2585 | `	SyBlob sMsg;` |
|        - |  2586 | `	SyString *pFile;` |
|        - |  2587 | `	char *zErr;` |
|       42 |  2588 | `	sxi32 rc = SXRET_OK;` |
|       42 |  2589 | `	if( !pVm->bErrReport ){` |
|        - |  2590 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2591 | `		return SXRET_OK;` |
|        - |  2592 | `	}` |
|        - |  2593 | `	/* Reset the working buffer */` |
|       42 |  2594 | `	SyBlobReset(pWorker);` |
|        - |  2595 | `	/* Peek the processed file if available */` |
|       42 |  2596 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       42 |  2597 | `	if( pFile ){` |
|        - |  2598 | `		/* Append file name */` |
|       42 |  2599 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       42 |  2600 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       20 |  2601 | `	}` |
|        - |  2602 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2603 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2604 | `	 * the correct errno value. */` |
|       42 |  2605 | `	zErr = "Error:  ";` |
|       42 |  2606 | `	switch(iErr){` |
|        4 |  2607 | `	case PH7_CTX_WARNING:` |
|        9 |  2608 | `		zErr = "Warning:  ";` |
|        9 |  2609 | `		break;` |
|        3 |  2610 | `	case PH7_CTX_NOTICE:` |
|        7 |  2611 | `		zErr = "Notice:  ";` |
|        6 |  2612 | `		break;` |
|       13 |  2613 | `	default:` |
|        - |  2614 | `		/* do not change iErr */` |
|       26 |  2615 | `		break;` |
|        - |  2616 | `	}` |
|       42 |  2617 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       42 |  2618 | `	if( pFuncName ){` |
|        - |  2619 | `		/* Append function name first */` |
|       26 |  2620 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2621 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2622 | `	}` |
|        - |  2623 | `	/* Format the raw message */` |
|       42 |  2624 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       42 |  2625 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2626 | `	/* Check if a user error handler is installed */` |
|       42 |  2627 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2628 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2629 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2630 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2631 | `	}` |
|       42 |  2632 | `	SyBlobRelease(&sMsg);` |
|       42 |  2633 | `	return rc;` |
|       22 |  2634 |  |
|        - |  2635 | `/*` |
|        - |  2636 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  2637 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  2638 | ` * possible.` |
|        - |  2639 | ` */` |
|       38 |  2640 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        1 |  2641 |  |
|        - |  2642 | `	ph7_class *pClass;` |
|       39 |  2643 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  2644 | `	ph7_class_instance *pThis;` |
|        - |  2645 | `	ph7_class_method *pCons;` |
|        - |  2646 | `	ph7_value sArg;` |
|        - |  2647 | `	ph7_value *apArg[1];` |
|        - |  2648 | `	SyBlob sMsg;` |
|        - |  2649 | `	SyString sMsgStr;` |
|        - |  2650 | `	VmFrame *pFrame;` |
|        - |  2651 | `	sxi32 rc;` |
|       39 |  2652 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       39 |  2653 | `	if( pClass == 0 ){` |
|      ! 0 |  2654 | `		return PH7_ABORT;` |
|        - |  2655 | `	}` |
|       39 |  2656 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       39 |  2657 | `	if( pThis == 0 ){` |
|      ! 0 |  2658 | `		return PH7_ABORT;` |
|        - |  2659 | `	}` |
|       39 |  2660 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2661 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  2662 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  2663 | `	{` |
|       39 |  2664 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       39 |  2665 | `		if( pOwner ){` |
|       39 |  2666 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       19 |  2667 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       20 |  2668 | `		}else{` |
|      ! 0 |  2669 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  2670 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  2671 | `		}` |
|        - |  2672 | `	}` |
|       39 |  2673 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       39 |  2674 | `	if( pCons ){` |
|       39 |  2675 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       39 |  2676 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       39 |  2677 | `		apArg[0] = &sArg;` |
|       39 |  2678 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       39 |  2679 | `		PH7_MemObjRelease(&sArg);` |
|       19 |  2680 | `	}` |
|       39 |  2681 | `	SyBlobRelease(&sMsg);` |
|       39 |  2682 | `	pFrame = pVm->pFrame;` |
|       39 |  2683 | `	if( pFrame ){` |
|       39 |  2684 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       39 |  2685 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       19 |  2686 | `	}` |
|       39 |  2687 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       39 |  2688 | `	PH7_ClassInstanceUnref(pThis);` |
|       39 |  2689 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2690 | `		return PH7_ABORT;` |
|        - |  2691 | `	}` |
|       39 |  2692 | `	return PH7_EXCEPTION;` |
|       20 |  2693 |  |
|        - |  2694 |  |
|        - |  2695 | `/*` |
|        - |  2696 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  2697 | ` */` |
|        4 |  2698 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  2699 |  |
|        - |  2700 | `	ph7_class *pErrClass;` |
|        - |  2701 | `	ph7_class_instance *pThis;` |
|        - |  2702 | `	ph7_class_method *pCons;` |
|        - |  2703 | `	ph7_value sArg;` |
|        - |  2704 | `	ph7_value *apArg[1];` |
|        - |  2705 | `	SyBlob sMsg;` |
|        - |  2706 | `	SyString sMsgStr;` |
|        - |  2707 | `	VmFrame *pFrame;` |
|        - |  2708 | `	sxi32 rc;` |
|        5 |  2709 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        5 |  2710 | `	if( pErrClass == 0 ){` |
|      ! 0 |  2711 | `		return PH7_ABORT;` |
|        - |  2712 | `	}` |
|        5 |  2713 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        5 |  2714 | `	if( pThis == 0 ){` |
|      ! 0 |  2715 | `		return PH7_ABORT;` |
|        - |  2716 | `	}` |
|        5 |  2717 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2718 | `	{` |
|        5 |  2719 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        5 |  2720 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        5 |  2721 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  2722 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  2723 | `	}` |
|        5 |  2724 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        5 |  2725 | `	if( pCons ){` |
|        5 |  2726 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        5 |  2727 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        5 |  2728 | `		apArg[0] = &sArg;` |
|        5 |  2729 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        5 |  2730 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  2731 | `	}` |
|        5 |  2732 | `	SyBlobRelease(&sMsg);` |
|        5 |  2733 | `	pFrame = pVm->pFrame;` |
|        5 |  2734 | `	if( pFrame ){` |
|        5 |  2735 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  2736 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  2737 | `	}` |
|        5 |  2738 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        5 |  2739 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  2740 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2741 | `		return PH7_ABORT;` |
|        - |  2742 | `	}` |
|        5 |  2743 | `	return PH7_EXCEPTION;` |
|        3 |  2744 |  |
|        - |  2745 |  |
|        - |  2746 | `/*` |
|        - |  2747 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  2748 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  2749 | ` * For class types, instanceof is verified.` |
|        - |  2750 | ` *` |
|        - |  2751 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  2752 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  2753 | ` */` |
|        - |  2754 | `/*` |
|        - |  2755 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  2756 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  2757 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  2758 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  2759 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  2760 | ` */` |
|       20 |  2761 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  2762 |  |
|        - |  2763 | `	const char *z, *zEnd, *zTail;` |
|        - |  2764 | `	sxu32 n;` |
|        - |  2765 | `	sxu8 bReal;` |
|        - |  2766 | `	sxi32 rc;` |
|       22 |  2767 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2768 | `		return 0;` |
|        - |  2769 | `	}` |
|       22 |  2770 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       22 |  2771 | `	n = SyBlobLength(&pValue->sBlob);` |
|       22 |  2772 | `	zEnd = z + n;` |
|       22 |  2773 | `	if( n == 0 ){` |
|      ! 0 |  2774 | `		return 0;` |
|        - |  2775 | `	}` |
|       22 |  2776 | `	zTail = 0;` |
|       22 |  2777 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       22 |  2778 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        7 |  2779 | `		return 0;` |
|        - |  2780 | `	}` |
|        - |  2781 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       16 |  2782 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  2783 | `		zTail++;` |
|      ! 0 |  2784 | `	}` |
|       16 |  2785 | `	return zTail == zEnd ? 1 : 0;` |
|       12 |  2786 |  |
|        - |  2787 |  |
|        - |  2788 | `/*` |
|        - |  2789 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  2790 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  2791 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  2792 | ` *   0 if it's not strictly numeric.` |
|        - |  2793 | ` */` |
|       16 |  2794 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  2795 |  |
|        - |  2796 | `	const char *z, *zEnd, *zTail;` |
|        - |  2797 | `	sxu32 n;` |
|       18 |  2798 | `	sxu8 bReal = 0;` |
|        - |  2799 | `	sxi32 rc;` |
|       18 |  2800 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2801 | `		return 0;` |
|        - |  2802 | `	}` |
|       18 |  2803 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2804 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2805 | `	zEnd = z + n;` |
|       18 |  2806 | `	if( n == 0 ) return 0;` |
|       18 |  2807 | `	zTail = 0;` |
|       18 |  2808 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2809 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  2810 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  2811 | `	if( zTail != zEnd ) return 0;` |
|       15 |  2812 | `	return bReal ? 2 : 1;` |
|       10 |  2813 |  |
|        - |  2814 |  |
|        - |  2815 | `/*` |
|        - |  2816 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - |  2817 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - |  2818 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - |  2819 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - |  2820 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - |  2821 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - |  2822 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - |  2823 | ` * throw.` |
|        - |  2824 | ` *` |
|        - |  2825 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  2826 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  2827 | ` */` |
|       98 |  2828 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)` |
|        2 |  2829 |  |
|        - |  2830 | `	sxu32 i;` |
|        - |  2831 | `	ph7_type_alt *aAlts;` |
|        - |  2832 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  2833 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|      100 |  2834 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  2835 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  2836 | `	}` |
|       88 |  2837 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|       88 |  2838 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       88 |  2839 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      260 |  2840 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      174 |  2841 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      150 |  2842 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      150 |  2843 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      150 |  2844 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       76 |  2845 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       48 |  2846 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  2847 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       88 |  2848 | `	}` |
|        - |  2849 | `	/* Object handling */` |
|       88 |  2850 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       18 |  2851 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       18 |  2852 | `		if( bHasClassAlt ){` |
|       14 |  2853 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  2854 | `			ph7_class *pSelfNow = 0;` |
|       14 |  2855 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  2856 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  2857 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  2858 | `			}` |
|       26 |  2859 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  2860 | `				ph7_class *pExpected;` |
|        - |  2861 | `				SyString *pCN;` |
|       22 |  2862 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  2863 | `				pCN = &aAlts[i].sClass;` |
|       22 |  2864 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  2865 | `					pExpected = pSelfNow;` |
|       22 |  2866 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  2867 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2868 | `				}else{` |
|       22 |  2869 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  2870 | `				}` |
|       22 |  2871 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  2872 | `					return SXRET_OK;` |
|        - |  2873 | `				}` |
|        8 |  2874 | `			}` |
|        2 |  2875 | `		}` |
|        9 |  2876 | `		return SXERR_INVALID;` |
|        - |  2877 | `	}` |
|        - |  2878 | `	/* Array handling */` |
|       72 |  2879 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  2880 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  2881 | `	}` |
|        - |  2882 | `	/* Scalar handling — exact match first */` |
|       66 |  2883 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       26 |  2884 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  2885 | `	}` |
|       42 |  2886 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  2887 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  2888 | `	}` |
|       38 |  2889 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       38 |  2890 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  2891 | `	}` |
|       18 |  2892 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2893 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  2894 | `	}` |
|       18 |  2895 | `	if( bStrict ){` |
|        - |  2896 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|      ! 0 |  2897 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 |  2898 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 |  2899 | `			return SXRET_OK;` |
|        - |  2900 | `		}` |
|      ! 0 |  2901 | `		return SXERR_INVALID;` |
|        - |  2902 | `	}` |
|        - |  2903 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  2904 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  2905 | `	 * to match PHP's union RFC. */` |
|        - |  2906 | `	{` |
|       18 |  2907 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  2908 | `		if( bHasInt ){` |
|        - |  2909 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  2910 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  2911 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2912 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2913 | `				return SXRET_OK;` |
|        - |  2914 | `			}` |
|       18 |  2915 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  2916 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  2917 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  2918 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2919 | `					return SXRET_OK;` |
|        - |  2920 | `				}` |
|      ! 0 |  2921 | `			}` |
|       18 |  2922 | `			if( kind == 1 ){` |
|        9 |  2923 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  2924 | `				return SXRET_OK;` |
|        - |  2925 | `			}` |
|        4 |  2926 | `		}` |
|       10 |  2927 | `		if( bHasFloat ){` |
|       10 |  2928 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  2929 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  2930 | `				return SXRET_OK;` |
|        - |  2931 | `			}` |
|       10 |  2932 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  2933 | `				PH7_MemObjToReal(pValue);` |
|        7 |  2934 | `				return SXRET_OK;` |
|        - |  2935 | `			}` |
|        1 |  2936 | `		}` |
|        3 |  2937 | `		if( bHasString ){` |
|      ! 0 |  2938 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  2939 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  2940 | `				return SXRET_OK;` |
|        - |  2941 | `			}` |
|      ! 0 |  2942 | `		}` |
|        3 |  2943 | `		if( bHasBool ){` |
|      ! 0 |  2944 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  2945 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  2946 | `				return SXRET_OK;` |
|        - |  2947 | `			}` |
|      ! 0 |  2948 | `		}` |
|        - |  2949 | `	}` |
|        3 |  2950 | `	return SXERR_INVALID;` |
|       51 |  2951 |  |
|        - |  2952 |  |
|        - |  2953 | `/*` |
|        - |  2954 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - |  2955 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - |  2956 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - |  2957 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - |  2958 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - |  2959 | ` */` |
|       34 |  2960 | `static sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        2 |  2961 |  |
|       36 |  2962 | `	if( bStrict ){` |
|        - |  2963 | `		/* Only int -> float widening is allowed implicitly. */` |
|       10 |  2964 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 |  2965 | `			PH7_MemObjToReal(pVal);` |
|        3 |  2966 | `			return SXRET_OK;` |
|        - |  2967 | `		}` |
|        7 |  2968 | `		return SXERR_INVALID;` |
|        - |  2969 | `	}` |
|        - |  2970 | `	{` |
|       28 |  2971 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|       28 |  2972 | `		if( xCast ) xCast(pVal);` |
|        - |  2973 | `	}` |
|       28 |  2974 | `	return SXRET_OK;` |
|       19 |  2975 |  |
|        - |  2976 |  |
|        - |  2977 | `/*` |
|        - |  2978 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - |  2979 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - |  2980 | ` *` |
|        - |  2981 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - |  2982 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - |  2983 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - |  2984 | ` * back to a static literal and ignore zBuf entirely.` |
|        - |  2985 | ` */` |
|        8 |  2986 | `static const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        1 |  2987 |  |
|        9 |  2988 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|        9 |  2989 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|        9 |  2990 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|        9 |  2991 | `		if( pDeclared->zString && nCopy > 0 ){` |
|        9 |  2992 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|        4 |  2993 | `		}` |
|        9 |  2994 | `		zBuf[nCopy] = 0;` |
|        9 |  2995 | `		return zBuf;` |
|        - |  2996 | `	}` |
|      ! 0 |  2997 | `	switch( nType ){` |
|      ! 0 |  2998 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 |  2999 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 |  3000 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 |  3001 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 |  3002 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 |  3003 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 |  3004 | `		default:             return "scalar";` |
|        - |  3005 | `	}` |
|        5 |  3006 |  |
|        - |  3007 |  |
|        - |  3008 | `/*` |
|        - |  3009 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  3010 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  3011 | ` */` |
|       18 |  3012 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        1 |  3013 |  |
|       19 |  3014 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       28 |  3015 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       18 |  3016 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       19 |  3017 | `	return zBuf;` |
|        1 |  3018 |  |
|        - |  3019 |  |
|    13038 |  3020 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  3021 |  |
|        - |  3022 | `	SyHashEntry *pSlot;` |
|        - |  3023 | `	VmClassAttr *pVmAttr;` |
|        - |  3024 | `	ph7_class_attr *pAttr;` |
|    13040 |  3025 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    13040 |  3026 | `	if( pSlot == 0 ){` |
|    12842 |  3027 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  3028 | `	}` |
|      200 |  3029 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      200 |  3030 | `	pAttr = pVmAttr->pAttr;` |
|      200 |  3031 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  3032 | `		return SXRET_OK;` |
|        - |  3033 | `	}` |
|        - |  3034 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|        - |  3035 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|        - |  3036 | `	 * matching PHP's documented behavior. */` |
|      200 |  3037 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       23 |  3038 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  3039 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|        - |  3040 |  |
|       16 |  3041 | `		if( rc == SXRET_OK ){` |
|        9 |  3042 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  3043 | `			return SXRET_OK;` |
|        - |  3044 | `		}` |
|        7 |  3045 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3046 | `			char zBuf[128];` |
|        4 |  3047 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  3048 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3049 | `		}` |
|        5 |  3050 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3051 | `	}` |
|        - |  3052 | `	/* NULL handling: allowed only if the type is nullable. */` |
|      186 |  3053 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  3054 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       12 |  3055 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       12 |  3056 | `			return SXRET_OK;` |
|        - |  3057 | `		}` |
|        3 |  3058 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  3059 | `	}` |
|        - |  3060 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  3061 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  3062 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      174 |  3063 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  3064 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3065 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  3066 | `			return SXRET_OK;` |
|        - |  3067 | `		}` |
|        7 |  3068 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3069 | `	}` |
|      164 |  3070 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  3071 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  3072 | `		 * currently active on the self-stack. */` |
|       26 |  3073 | `		ph7_class *pExpected = 0;` |
|       26 |  3074 | `		SyString *pClassName = &pAttr->sClass;` |
|       26 |  3075 | `		ph7_class *pSelfNow = 0;` |
|       26 |  3076 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        3 |  3077 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        3 |  3078 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        1 |  3079 | `		}` |
|       26 |  3080 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  3081 | `			pExpected = pSelfNow;` |
|       24 |  3082 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3083 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3084 | `		}else{` |
|       22 |  3085 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3086 | `		}` |
|       26 |  3087 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3088 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3089 | `		}` |
|       26 |  3090 | `		if( pExpected ){` |
|       22 |  3091 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       22 |  3092 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  3093 | `				char zBuf[128];` |
|        7 |  3094 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  3095 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3096 | `			}` |
|        8 |  3097 | `		}` |
|       22 |  3098 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       22 |  3099 | `		return SXRET_OK;` |
|        - |  3100 | `	}` |
|        - |  3101 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  3102 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|      140 |  3103 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3104 | `		char zBuf[128];` |
|       10 |  3105 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 |  3106 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3107 | `	}` |
|      134 |  3108 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       26 |  3109 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       26 |  3110 | `		if( xCast ){` |
|        - |  3111 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       26 |  3112 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3113 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3114 | `			}` |
|       24 |  3115 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  3116 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3117 | `			}` |
|        - |  3118 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  3119 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  3120 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       26 |  3121 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       17 |  3122 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       19 |  3123 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|        9 |  3124 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  3125 | `			}` |
|       12 |  3126 | `			xCast(pValue);` |
|        5 |  3127 | `		}` |
|        5 |  3128 | `	}` |
|      120 |  3129 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      120 |  3130 | `	return SXRET_OK;` |
|     6521 |  3131 |  |
|        - |  3132 |  |
|        - |  3133 | `/*` |
|        - |  3134 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3135 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3136 | ` * information.` |
|        - |  3137 | ` * ------------------------------------` |
|        - |  3138 | ` * Simple boring wrapper function.` |
|        - |  3139 | ` * ------------------------------------` |
|        - |  3140 | ` */` |
|       16 |  3141 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  3142 |  |
|        - |  3143 | `	va_list ap;` |
|        - |  3144 | `	sxi32 rc;` |
|       17 |  3145 | `	va_start(ap,zFormat);` |
|       17 |  3146 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       17 |  3147 | `	va_end(ap);` |
|       17 |  3148 | `	return rc;` |
|        1 |  3149 |  |
|        - |  3150 | `/*` |
|        - |  3151 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  3152 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  3153 | ` */` |
|       34 |  3154 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        1 |  3155 |  |
|        - |  3156 | `	ph7_class *pClass;` |
|        - |  3157 | `	ph7_class_instance *pThis;` |
|        - |  3158 | `	ph7_class_method *pCons;` |
|        - |  3159 | `	ph7_value sArg;` |
|        - |  3160 | `	ph7_value *apArg[1];` |
|        - |  3161 | `	SyBlob sMsg;` |
|        - |  3162 | `	SyString sMsgStr;` |
|        - |  3163 | `	VmFrame *pFrame;` |
|        - |  3164 | `	sxi32 rc;` |
|       35 |  3165 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       35 |  3166 | `	if( pClass == 0 ){` |
|      ! 0 |  3167 | `		return PH7_ABORT;` |
|        - |  3168 | `	}` |
|       35 |  3169 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       35 |  3170 | `	if( pThis == 0 ){` |
|      ! 0 |  3171 | `		return PH7_ABORT;` |
|        - |  3172 | `	}` |
|       35 |  3173 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       35 |  3174 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       17 |  3175 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       35 |  3176 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       35 |  3177 | `	if( pCons ){` |
|       35 |  3178 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       35 |  3179 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       35 |  3180 | `		apArg[0] = &sArg;` |
|       35 |  3181 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       35 |  3182 | `		PH7_MemObjRelease(&sArg);` |
|       17 |  3183 | `	}` |
|       35 |  3184 | `	SyBlobRelease(&sMsg);` |
|       35 |  3185 | `	pFrame = pVm->pFrame;` |
|       35 |  3186 | `	if( pFrame ){` |
|       35 |  3187 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       35 |  3188 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       17 |  3189 | `	}` |
|       35 |  3190 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       35 |  3191 | `	PH7_ClassInstanceUnref(pThis);` |
|       35 |  3192 | `	if( rc == SXERR_ABORT ){` |
|        5 |  3193 | `		return PH7_ABORT;` |
|        - |  3194 | `	}` |
|       31 |  3195 | `	return PH7_EXCEPTION;` |
|       18 |  3196 |  |
|        - |  3197 | `/*` |
|        - |  3198 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - |  3199 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - |  3200 | ` */` |
|        6 |  3201 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|        1 |  3202 |  |
|        - |  3203 | `	ph7_class *pClass;` |
|        - |  3204 | `	ph7_class_instance *pThis;` |
|        - |  3205 | `	ph7_class_method *pCons;` |
|        - |  3206 | `	ph7_value sArg;` |
|        - |  3207 | `	ph7_value *apArg[1];` |
|        - |  3208 | `	SyBlob sMsg;` |
|        - |  3209 | `	SyString sMsgStr;` |
|        - |  3210 | `	VmFrame *pFrame;` |
|        - |  3211 | `	sxi32 rc;` |
|        7 |  3212 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|        7 |  3213 | `	if( pClass == 0 ){` |
|      ! 0 |  3214 | `		return PH7_ABORT;` |
|        - |  3215 | `	}` |
|        7 |  3216 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|        7 |  3217 | `	if( pThis == 0 ){` |
|      ! 0 |  3218 | `		return PH7_ABORT;` |
|        - |  3219 | `	}` |
|        7 |  3220 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        7 |  3221 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|        3 |  3222 | `		pFuncName,zExpected,zGiven);` |
|        7 |  3223 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        7 |  3224 | `	if( pCons ){` |
|        7 |  3225 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        7 |  3226 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        7 |  3227 | `		apArg[0] = &sArg;` |
|        7 |  3228 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        7 |  3229 | `		PH7_MemObjRelease(&sArg);` |
|        3 |  3230 | `	}` |
|        7 |  3231 | `	SyBlobRelease(&sMsg);` |
|        7 |  3232 | `	pFrame = pVm->pFrame;` |
|        7 |  3233 | `	if( pFrame ){` |
|        7 |  3234 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        7 |  3235 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        3 |  3236 | `	}` |
|        7 |  3237 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        7 |  3238 | `	PH7_ClassInstanceUnref(pThis);` |
|        7 |  3239 | `	if( rc == SXERR_ABORT ){` |
|        7 |  3240 | `		return PH7_ABORT;` |
|        - |  3241 | `	}` |
|      ! 0 |  3242 | `	return PH7_EXCEPTION;` |
|        4 |  3243 |  |
|        - |  3244 | `/*` |
|        - |  3245 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - |  3246 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - |  3247 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - |  3248 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - |  3249 | ` */` |
|        - |  3250 | `/*` |
|        - |  3251 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|        - |  3252 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|        - |  3253 | ` * null SyString yields an empty C string. Returns zBuf.` |
|        - |  3254 | ` */` |
|       24 |  3255 | `static const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|        2 |  3256 |  |
|        - |  3257 | `	sxu32 nCopy;` |
|       26 |  3258 | `	if( nBuf == 0 ) return "";` |
|       26 |  3259 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|      ! 0 |  3260 | `		zBuf[0] = 0;` |
|      ! 0 |  3261 | `		return zBuf;` |
|        - |  3262 | `	}` |
|       26 |  3263 | `	nCopy = SyStringLength(pStr);` |
|       26 |  3264 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       26 |  3265 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|       26 |  3266 | `	zBuf[nCopy] = 0;` |
|       26 |  3267 | `	return zBuf;` |
|       14 |  3268 |  |
|        - |  3269 |  |
|      152 |  3270 | `static sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        2 |  3271 |  |
|      154 |  3272 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|        - |  3273 | `	const char *zGiven;` |
|        - |  3274 | `	char zBuf[128];` |
|        - |  3275 | `	char zTypeBuf[128];` |
|        - |  3276 | `	/* Untyped function: no enforcement. */` |
|      154 |  3277 | `	if( pFunc->nReturnType == 0 ){` |
|      ! 0 |  3278 | `		return SXRET_OK;` |
|        - |  3279 | `	}` |
|        - |  3280 | `	/* void return type: the function must not produce a value. */` |
|      154 |  3281 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|       20 |  3282 | `		if( pValue == 0 ){` |
|       18 |  3283 | `			return SXRET_OK;` |
|        - |  3284 | `		}` |
|        - |  3285 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - |  3286 | `		 * still counts as "returned a value" here. */` |
|        3 |  3287 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|        3 |  3288 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|        - |  3289 | `	}` |
|        - |  3290 | `	/* Function fell off the end without an explicit return: PHP implicitly` |
|        - |  3291 | `	 * returns null. For a typed non-nullable return, that's a TypeError. */` |
|      136 |  3292 | `	if( pValue == 0 ){` |
|      ! 0 |  3293 | `		const char *zExpected = "value";` |
|      ! 0 |  3294 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3295 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3296 | `		}` |
|      ! 0 |  3297 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|        - |  3298 | `	}` |
|        - |  3299 | `	/* Union return type — delegate. The function has no flag for nullable` |
|        - |  3300 | `	 * unions; a null alternative is represented inside aReturnUnion, so pass` |
|        - |  3301 | `	 * bNullable=0 here. */` |
|      136 |  3302 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - |  3303 | `		sxi32 rcU;` |
|      ! 0 |  3304 | `		int bNullable = 0;` |
|      ! 0 |  3305 | `		const char *zExpected = "union";` |
|        - |  3306 | ``		/* Scan alternatives for MEMOBJ_NULL, which serves as `T\|null`. */`` |
|        - |  3307 | `		{` |
|        - |  3308 | `			sxu32 i;` |
|      ! 0 |  3309 | `			ph7_type_alt *aAlts = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  3310 | `			for( i = 0; i < SySetUsed(&pFunc->aReturnUnion); i++ ){` |
|      ! 0 |  3311 | `				if( aAlts[i].nType == MEMOBJ_NULL ){ bNullable = 1; break; }` |
|      ! 0 |  3312 | `			}` |
|        - |  3313 | `		}` |
|      ! 0 |  3314 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|      ! 0 |  3315 | `		if( rcU == SXRET_OK ){` |
|      ! 0 |  3316 | `			return SXRET_OK;` |
|        - |  3317 | `		}` |
|      ! 0 |  3318 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3319 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3320 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  3321 | `			zGiven = "null";` |
|      ! 0 |  3322 | `		}else{` |
|      ! 0 |  3323 | `			zGiven = ph7_type_name(pValue);` |
|        - |  3324 | `		}` |
|      ! 0 |  3325 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3326 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3327 | `		}` |
|      ! 0 |  3328 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3329 | `	}` |
|        - |  3330 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - |  3331 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - |  3332 | `	 * it into the TypeError message. */` |
|      136 |  3333 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|        6 |  3334 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|        - |  3335 | `		const char *zExpected;` |
|        - |  3336 | `		ph7_class *pExpected;` |
|        6 |  3337 | `		ph7_class *pSelfNow = 0;` |
|        6 |  3338 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        6 |  3339 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        6 |  3340 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        2 |  3341 | `		}` |
|        6 |  3342 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        3 |  3343 | `			pExpected = pSelfNow;` |
|        4 |  3344 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3345 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3346 | `		}else{` |
|        3 |  3347 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3348 | `		}` |
|        6 |  3349 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|        6 |  3350 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3351 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 |  3352 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3353 | `		}` |
|        6 |  3354 | `		if( pExpected ){` |
|        6 |  3355 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        6 |  3356 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  3357 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3358 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3359 | `			}` |
|        2 |  3360 | `		}` |
|        6 |  3361 | `		return SXRET_OK;` |
|        - |  3362 | `	}` |
|        - |  3363 | `	/* Scalar return type. Allow null pass-through if the function is` |
|        - |  3364 | `	 * nullable (textual "?T" gets that flag, though union+null is handled` |
|        - |  3365 | `	 * above). There's no explicit nullable flag on ph7_vm_func, so detect` |
|        - |  3366 | `	 * via the type-text leading '?'. */` |
|      132 |  3367 | `	if( (pValue->iFlags & MEMOBJ_NULL) ){` |
|        6 |  3368 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0` |
|        8 |  3369 | `		 && pFunc->sReturnTypeName.zString[0] == '?' ){` |
|        8 |  3370 | `			return SXRET_OK;` |
|        - |  3371 | `		}` |
|      ! 0 |  3372 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3373 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3374 | `			"null");` |
|        - |  3375 | `	}` |
|        - |  3376 | `	/* Exact match? Done. */` |
|      126 |  3377 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|      120 |  3378 | `		return SXRET_OK;` |
|        - |  3379 | `	}` |
|        - |  3380 | `	/* Object->scalar is never compatible. */` |
|        8 |  3381 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3382 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3383 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3384 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3385 | `			zGiven);` |
|        - |  3386 | `	}` |
|        - |  3387 | `	/* Array <-> scalar is never compatible. */` |
|        8 |  3388 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      ! 0 |  3389 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3390 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3391 | `			ph7_type_name(pValue));` |
|        - |  3392 | `	}` |
|        - |  3393 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - |  3394 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - |  3395 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - |  3396 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|        8 |  3397 | `	if( !bStrict` |
|        5 |  3398 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|        4 |  3399 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|        6 |  3400 | `	 && !VmStringIsStrictNumeric(pValue) ){` |
|        4 |  3401 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3402 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3403 | `			"string");` |
|        - |  3404 | `	}` |
|        6 |  3405 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|        3 |  3406 | `		return SXRET_OK;` |
|        - |  3407 | `	}` |
|        4 |  3408 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3409 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 |  3410 | `		ph7_type_name(pValue));` |
|       78 |  3411 |  |
|        - |  3412 | `/*` |
|        - |  3413 | ` * Report a fatal named-argument error.` |
|        - |  3414 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  3415 | ` */` |
|        6 |  3416 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        1 |  3417 |  |
|        7 |  3418 | `	const char *zFunc = 0;` |
|        7 |  3419 | `	int nFunc = 0;` |
|        7 |  3420 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        7 |  3421 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        1 |  3422 |  |
|        - |  3423 | `/*` |
|        - |  3424 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3425 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3426 | ` * information.` |
|        - |  3427 | ` * ------------------------------------` |
|        - |  3428 | ` * Simple boring wrapper function.` |
|        - |  3429 | ` * ------------------------------------` |
|        - |  3430 | ` */` |
|       24 |  3431 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  3432 |  |
|        - |  3433 | `	sxi32 rc;` |
|       26 |  3434 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  3435 | `	return rc;` |
|        2 |  3436 |  |
|        - |  3437 | `/*` |
|        - |  3438 | ` * Resolve function context from the current frame.` |
|        - |  3439 | ` */` |
|      978 |  3440 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  3441 |  |
|        - |  3442 | `	VmFrame *pFrame;` |
|        - |  3443 | `	ph7_vm_func *pFunc;` |
|      979 |  3444 | `	*pzFuncName = 0;` |
|      979 |  3445 | `	*pnFuncLen = 0;` |
|      979 |  3446 | `	pFrame = pVm->pFrame;` |
|      979 |  3447 | `	if( pFrame == 0 ){` |
|      ! 0 |  3448 | `		return;` |
|        - |  3449 | `	}` |
|      979 |  3450 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      979 |  3451 | `	if( pFrame->pParent == 0 ){` |
|      955 |  3452 | `		return;` |
|        - |  3453 | `	}` |
|       25 |  3454 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       25 |  3455 | `	if( pFunc == 0 ){` |
|      ! 0 |  3456 | `		return;` |
|        - |  3457 | `	}` |
|       25 |  3458 | `	*pzFuncName = pFunc->sName.zString;` |
|       25 |  3459 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      490 |  3460 |  |
|        - |  3461 | `/*` |
|        - |  3462 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  3463 | ` */` |
|      504 |  3464 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  3465 |  |
|        - |  3466 | `	SyBlob sOut;` |
|        - |  3467 | `	SyString *pFile;` |
|      505 |  3468 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  3469 | `		return PH7_OK;` |
|        - |  3470 | `	}` |
|      505 |  3471 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  3472 | `		zClass = "Exception";` |
|      ! 0 |  3473 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  3474 | `	}` |
|      505 |  3475 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      483 |  3476 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      241 |  3477 | `	}` |
|      505 |  3478 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      505 |  3479 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      505 |  3480 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      505 |  3481 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      505 |  3482 | `	if( zMsg && nMsg > 0 ){` |
|      505 |  3483 | `		SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      505 |  3484 | `		SyBlobAppend(&sOut,zMsg,nMsg);` |
|      252 |  3485 | `	}` |
|      505 |  3486 | `	if( pFile ){` |
|      505 |  3487 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      505 |  3488 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      505 |  3489 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      252 |  3490 | `	}` |
|      505 |  3491 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      505 |  3492 | `	if( pFile ){` |
|      505 |  3493 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      505 |  3494 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      505 |  3495 | `		if( zFuncName && nFuncLen > 0 ){` |
|       25 |  3496 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|       13 |  3497 | `		}else{` |
|      481 |  3498 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  3499 | `		}` |
|      252 |  3500 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  3501 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  3502 | `	}else{` |
|      ! 0 |  3503 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  3504 | `	}` |
|      505 |  3505 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      505 |  3506 | `	if( pFile ){` |
|      505 |  3507 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      505 |  3508 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      505 |  3509 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      505 |  3510 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      252 |  3511 | `	}` |
|      505 |  3512 | `	VmCallErrorHandler(pVm,&sOut);` |
|      505 |  3513 | `	SyBlobRelease(&sOut);` |
|      505 |  3514 | `	return PH7_ABORT;` |
|      253 |  3515 |  |
|        - |  3516 | `/*` |
|        - |  3517 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  3518 | ` */` |
|      482 |  3519 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  3520 |  |
|        - |  3521 | `	ph7_vm *pVm;` |
|        - |  3522 | `	ph7_class *pClass;` |
|        - |  3523 | `	ph7_class_instance *pThis;` |
|        - |  3524 | `	ph7_class_method *pCons;` |
|        - |  3525 | `	ph7_value sArg;` |
|        - |  3526 | `	ph7_value *apArg[1];` |
|        - |  3527 | `	SyBlob sMsg;` |
|        - |  3528 | `	SyString sMsgStr;` |
|        - |  3529 | `	VmFrame *pFrame;` |
|        - |  3530 | `	va_list ap;` |
|        - |  3531 | `	sxi32 rc;` |
|        - |  3532 |  |
|      484 |  3533 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3534 | `		return PH7_ABORT;` |
|        - |  3535 | `	}` |
|      484 |  3536 | `	pVm = pCtx->pVm;` |
|      484 |  3537 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3538 | `		zClass = "Error";` |
|      ! 0 |  3539 | `	}` |
|      484 |  3540 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      484 |  3541 | `	if( pClass == 0 ){` |
|      ! 0 |  3542 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3543 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  3544 | `			zClass` |
|        - |  3545 | `			);` |
|        - |  3546 | `	}` |
|      484 |  3547 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      484 |  3548 | `	if( pThis == 0 ){` |
|      ! 0 |  3549 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3550 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  3551 | `			);` |
|        - |  3552 | `	}` |
|        - |  3553 |  |
|      484 |  3554 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      484 |  3555 | `	va_start(ap,zFormat);` |
|      484 |  3556 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      484 |  3557 | `	va_end(ap);` |
|        - |  3558 |  |
|      484 |  3559 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      484 |  3560 | `	if( pCons ){` |
|      484 |  3561 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      484 |  3562 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      484 |  3563 | `		apArg[0] = &sArg;` |
|      484 |  3564 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      484 |  3565 | `		PH7_MemObjRelease(&sArg);` |
|      241 |  3566 | `	}` |
|      484 |  3567 | `	SyBlobRelease(&sMsg);` |
|        - |  3568 |  |
|      484 |  3569 | `	pFrame = pVm->pFrame;` |
|      484 |  3570 | `	if( pFrame ){` |
|      484 |  3571 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      484 |  3572 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      241 |  3573 | `	}` |
|      484 |  3574 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      484 |  3575 | `	PH7_ClassInstanceUnref(pThis);` |
|      484 |  3576 | `	if( rc == SXERR_ABORT ){` |
|      471 |  3577 | `		return PH7_ABORT;` |
|        - |  3578 | `	}` |
|       14 |  3579 | `	return PH7_EXCEPTION;` |
|      243 |  3580 |  |
|        - |  3581 | `/*` |
|        - |  3582 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  3583 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  3584 | ` */` |
|      ! 0 |  3585 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  3586 |  |
|        - |  3587 | `	ph7_vm *pVm;` |
|        - |  3588 | `	SyBlob sMsg;` |
|      ! 0 |  3589 | `	const char *zFuncName = 0;` |
|      ! 0 |  3590 | `	int nFuncLen = 0;` |
|        - |  3591 | `	va_list ap;` |
|        - |  3592 | `	sxi32 rc;` |
|        - |  3593 |  |
|      ! 0 |  3594 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3595 | `		return PH7_OK;` |
|        - |  3596 | `	}` |
|      ! 0 |  3597 | `	pVm = pCtx->pVm;` |
|      ! 0 |  3598 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3599 | `		zClass = "Error";` |
|      ! 0 |  3600 | `	}` |
|        - |  3601 |  |
|      ! 0 |  3602 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3603 |  |
|      ! 0 |  3604 | `	va_start(ap,zFormat);` |
|      ! 0 |  3605 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  3606 | `	va_end(ap);` |
|        - |  3607 |  |
|      ! 0 |  3608 | `	if( pCtx->pFunc ){` |
|      ! 0 |  3609 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  3610 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  3611 | `	}` |
|      ! 0 |  3612 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  3613 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  3614 | `	}` |
|      ! 0 |  3615 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  3616 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  3617 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  3618 | `	return rc;` |
|      ! 0 |  3619 |  |
|        - |  3620 | `/*` |
|        - |  3621 | ` * Save the execution state of a fiber/generator context.` |
|        - |  3622 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  3623 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  3624 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  3625 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  3626 | ` * when VmByteCodeExec returns.` |
|        - |  3627 | ` */` |
|      144 |  3628 | `static sxi32 VmSuspendCtx(` |
|        - |  3629 | `	ph7_vm *pVm,` |
|        - |  3630 | `	ph7_exec_ctx *pCtx,` |
|        - |  3631 | `	sxi32 pc,` |
|        - |  3632 | `	sxi32 nTos` |
|        - |  3633 | `	)` |
|        2 |  3634 |  |
|       72 |  3635 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      146 |  3636 | `	pCtx->pc = pc;` |
|      146 |  3637 | `	pCtx->nTos = nTos;` |
|      146 |  3638 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      146 |  3639 | `	return PH7_SUSPEND;` |
|        2 |  3640 |  |
|        - |  3641 | `/*` |
|        - |  3642 | ` * Resolve named-argument mapping.` |
|        - |  3643 | ` *` |
|        - |  3644 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  3645 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  3646 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  3647 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  3648 | ` * every formal parameter that received a value.` |
|        - |  3649 | ` *` |
|        - |  3650 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  3651 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  3652 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  3653 | ` */` |
|       92 |  3654 | `static sxi32 VmResolveNamedArgs(` |
|        - |  3655 | `	ph7_vm *pVm,` |
|        - |  3656 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  3657 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  3658 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  3659 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  3660 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  3661 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  3662 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  3663 |  |
|        2 |  3664 |  |
|       94 |  3665 | `	sxi32 posIdx = 0;` |
|        - |  3666 | `	sxu32 i;` |
|        - |  3667 | `	char zErrMsg[256];` |
|       94 |  3668 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      278 |  3669 | `	for( i = 0; i < nActual; i++ ){` |
|      186 |  3670 | `		aSlot[i] = -2;` |
|       94 |  3671 | `	}` |
|      272 |  3672 | `	for( i = 0; i < nActual; i++ ){` |
|      269 |  3673 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  3674 | `			/* Named argument — find formal by name */` |
|      174 |  3675 | `			int found = 0;` |
|        - |  3676 | `			sxu32 k;` |
|      288 |  3677 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      274 |  3678 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      265 |  3679 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      252 |  3680 | `						pMap->aNames[i].zString,` |
|      378 |  3681 | `						pMap->aNames[i].nByte) == 0 ){` |
|      162 |  3682 | `					if( aUsed[k] ){` |
|        7 |  3683 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3684 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  3685 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        5 |  3686 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        5 |  3687 | `						return PH7_ABORT;` |
|        - |  3688 | `					}` |
|      158 |  3689 | `					aSlot[i] = (sxi32)k;` |
|      158 |  3690 | `					aUsed[k] = 1;` |
|      158 |  3691 | `					found = 1;` |
|      158 |  3692 | `					break;` |
|        - |  3693 | `				}` |
|       59 |  3694 | `			}` |
|      170 |  3695 | `			if( !found ){` |
|       14 |  3696 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  3697 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  3698 | `				}else{` |
|        4 |  3699 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3700 | `						"Unknown named parameter $%.*s",` |
|        2 |  3701 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  3702 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  3703 | `					return PH7_ABORT;` |
|        - |  3704 | `				}` |
|        5 |  3705 | `			}` |
|       85 |  3706 | `		}else{` |
|        - |  3707 | `			/* Positional argument */` |
|       14 |  3708 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       14 |  3709 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  3710 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3711 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  3712 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  3713 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  3714 | `					return PH7_ABORT;` |
|        - |  3715 | `				}` |
|       14 |  3716 | `				aSlot[i] = posIdx;` |
|       14 |  3717 | `				aUsed[posIdx] = 1;` |
|        6 |  3718 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  3719 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  3720 | `			}` |
|       14 |  3721 | `			posIdx++;` |
|        - |  3722 | `		}` |
|       91 |  3723 | `	}` |
|       87 |  3724 | `	return SXRET_OK;` |
|       48 |  3725 |  |
|        - |  3726 | `/*` |
|        - |  3727 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  3728 | ` *` |
|        - |  3729 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  3730 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  3731 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  3732 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  3733 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  3734 | ` * then the program execution is halted.` |
|        - |  3735 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  3736 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  3737 | ` * or to reset the VM to it's initial state.` |
|        - |  3738 | ` */` |
|    40332 |  3739 | `static sxi32 VmByteCodeExec(` |
|        - |  3740 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3741 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  3742 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  3743 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  3744 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  3745 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  3746 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  3747 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  3748 | `	ph7_vm_func *pEnforceRetFunc /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  3749 | `	)` |
|        2 |  3750 |  |
|        - |  3751 | `	VmInstr *pInstr;` |
|        - |  3752 | `	ph7_value *pTos;` |
|        - |  3753 | `	SySet aArg;` |
|        - |  3754 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  3755 | `	sxi32 pc;` |
|        - |  3756 | `	sxi32 rc;` |
|        - |  3757 | `	/* Argument container */` |
|    40334 |  3758 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    40334 |  3759 | `	if( nTos < 0 ){` |
|    37916 |  3760 | `		pTos = &pStack[-1];` |
|    18959 |  3761 | `	}else{` |
|     2420 |  3762 | `		pTos = &pStack[nTos];` |
|        - |  3763 | `	}` |
|    40334 |  3764 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    40334 |  3765 | `	pc = nPc;` |
|        - |  3766 | `/*` |
|        - |  3767 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  3768 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  3769 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  3770 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  3771 | ` */` |
|        - |  3772 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  3773 | `	{ \` |
|        - |  3774 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  3775 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  3776 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  3777 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  3778 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  3779 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  3780 | `				break; \` |
|        - |  3781 | `			} \` |
|        - |  3782 | `			goto Exception; \` |
|        - |  3783 | `		} \` |
|        - |  3784 | `	}` |
|        - |  3785 | `	/* Execute as much as we can */` |
|  5591272 |  3786 | `	for(;;){` |
|        - |  3787 | `		/* Fetch the instruction to execute */` |
| 11181842 |  3788 | `		pInstr = &aInstr[pc];` |
| 11181842 |  3789 | `		rc = SXRET_OK;` |
|        - |  3790 | `/*` |
|        - |  3791 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  3792 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  3793 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  3794 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  3795 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  3796 | ` */` |
| 11181842 |  3797 | `		switch(pInstr->iOp){` |
|        - |  3798 | `/*` |
|        - |  3799 | ` * DONE: P1 * *` |
|        - |  3800 | ` *` |
|        - |  3801 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  3802 | ` * and return immediately.` |
|        - |  3803 | ` */` |
|    19831 |  3804 | `case PH7_OP_DONE:` |
|        - |  3805 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  3806 | `	 * the fiber start/resume paths) set pEnforceRetFunc, so this branch is` |
|        - |  3807 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  3808 | `	 * callback trampolines, and the main script. */` |
|    39664 |  3809 | `	if( pEnforceRetFunc && pEnforceRetFunc->nReturnType > 0 ){` |
|      154 |  3810 | `		ph7_value *pRetVal = 0;` |
|      154 |  3811 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|      138 |  3812 | `			pRetVal = pTos;` |
|       68 |  3813 | `		}` |
|      154 |  3814 | `		rc = VmEnforceReturnType(&(*pVm), pEnforceRetFunc, pRetVal);` |
|      154 |  3815 | `		if( rc == PH7_ABORT ) goto Abort;` |
|      148 |  3816 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  3817 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      ! 0 |  3818 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3819 | `				pTos--;` |
|      ! 0 |  3820 | `			}` |
|      ! 0 |  3821 | `			goto Exception;` |
|        - |  3822 | `		}` |
|        - |  3823 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  3824 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  3825 | `		 * defensively we clear the pointer after a successful check). */` |
|      148 |  3826 | `		pEnforceRetFunc = 0;` |
|       73 |  3827 | `	}` |
|    39658 |  3828 | `	if( pInstr->iP1 ){` |
|        - |  3829 | `#ifdef UNTRUST` |
|        - |  3830 | `		if( pTos < pStack ){` |
|        - |  3831 | `			goto Abort;` |
|        - |  3832 | `		}` |
|        - |  3833 | `#endif` |
|    23876 |  3834 | `		if( pLastRef ){` |
|    15066 |  3835 | `			*pLastRef = pTos->nIdx;` |
|     7532 |  3836 | `		}` |
|    23876 |  3837 | `		if( pResult ){` |
|        - |  3838 | `			/* Execution result */` |
|    22632 |  3839 | `			PH7_MemObjStore(pTos,pResult);` |
|    11315 |  3840 | `		}` |
|    23876 |  3841 | `		VmPopOperand(&pTos,1);` |
|    27721 |  3842 | `	}else if( pLastRef ){` |
|        - |  3843 | `		/* Nothing referenced */` |
|     1538 |  3844 | `		*pLastRef = SXU32_HIGH;` |
|      768 |  3845 | `	}` |
|        - |  3846 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  3847 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  3848 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  3849 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  3850 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  3851 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  3852 | `	 * block can override it.` |
|        - |  3853 | `	 */` |
|    39660 |  3854 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  3855 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  3856 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  3857 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  3858 | `		pExc->pFrame = 0;` |
|        3 |  3859 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  3860 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  3861 | `			pExc->iFinallyDone = 1;` |
|        - |  3862 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  3863 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  3864 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  3865 | `				goto Abort;` |
|        - |  3866 | `			}` |
|        1 |  3867 | `		}` |
|        1 |  3868 | `	}` |
|    39658 |  3869 | `	goto Done;` |
|        - |  3870 | `/*` |
|        - |  3871 | ` * HALT: P1 * *` |
|        - |  3872 | ` *` |
|        - |  3873 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  3874 | ` * and abort immediately.` |
|        - |  3875 | ` */` |
|        4 |  3876 | `case PH7_OP_HALT:` |
|        9 |  3877 | `	if( pInstr->iP1 ){` |
|        - |  3878 | `#ifdef UNTRUST` |
|        - |  3879 | `		if( pTos < pStack ){` |
|        - |  3880 | `			goto Abort;` |
|        - |  3881 | `		}` |
|        - |  3882 | `#endif` |
|        9 |  3883 | `		if( pLastRef ){` |
|      ! 0 |  3884 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  3885 | `		}` |
|        9 |  3886 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  3887 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  3888 | `				/* Output the exit message */` |
|        7 |  3889 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  3890 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  3891 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  3892 | `			}` |
|        7 |  3893 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  3894 | `			/* Record exit status */` |
|        5 |  3895 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  3896 | `		}` |
|        9 |  3897 | `		VmPopOperand(&pTos,1);` |
|        4 |  3898 | `	}else if( pLastRef ){` |
|        - |  3899 | `		/* Nothing referenced */` |
|      ! 0 |  3900 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  3901 | `	}` |
|        - |  3902 | `	/* Check if we're in an included file context */` |
|        9 |  3903 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  3904 | `		/* Terminate the entire process */` |
|        9 |  3905 | `		exit(pVm->iExitStatus);` |
|        - |  3906 | `	}` |
|      ! 0 |  3907 | `	goto Abort;` |
|        - |  3908 | `/*` |
|        - |  3909 | ` * JMP: * P2 *` |
|        - |  3910 | ` *` |
|        - |  3911 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  3912 | ` * the one at index P2 from the beginning of the program.` |
|        - |  3913 | ` */` |
|   238760 |  3914 | `case PH7_OP_JMP:` |
|   477566 |  3915 | `	pc = pInstr->iP2 - 1;` |
|   477566 |  3916 | `	break;` |
|        - |  3917 | `/*` |
|        - |  3918 | ` * JZ: P1 P2 *` |
|        - |  3919 | ` *` |
|        - |  3920 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  3921 | ` * entry in the stack if P1 is zero.` |
|        - |  3922 | ` */` |
|   565565 |  3923 | `case PH7_OP_JZ:` |
|        - |  3924 | `#ifdef UNTRUST` |
|        - |  3925 | `	if( pTos < pStack ){` |
|        - |  3926 | `		goto Abort;` |
|        - |  3927 | `	}` |
|        - |  3928 | `#endif` |
|        - |  3929 | `	/* Get a boolean value */` |
|  1131220 |  3930 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      172 |  3931 | `		PH7_MemObjToBool(pTos);` |
|       85 |  3932 | `	}` |
|  1131220 |  3933 | `	if( !pTos->x.iVal ){` |
|        - |  3934 | `		/* Take the jump */` |
|   579810 |  3935 | `		pc = pInstr->iP2 - 1;` |
|   289904 |  3936 | `	}` |
|  1131220 |  3937 | `	if( !pInstr->iP1 ){` |
|   899240 |  3938 | `		VmPopOperand(&pTos,1);` |
|   449641 |  3939 | `	}` |
|  1131220 |  3940 | `	break;` |
|        - |  3941 | `/*` |
|        - |  3942 | ` * JNZ: P1 P2 *` |
|        - |  3943 | ` *` |
|        - |  3944 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  3945 | ` * entry in the stack if P1 is zero.` |
|        - |  3946 | ` */` |
|    59397 |  3947 | `case PH7_OP_JNZ:` |
|        - |  3948 | `#ifdef UNTRUST` |
|        - |  3949 | `	if( pTos < pStack ){` |
|        - |  3950 | `		goto Abort;` |
|        - |  3951 | `	}` |
|        - |  3952 | `#endif` |
|        - |  3953 | `	/* Get a boolean value */` |
|   118796 |  3954 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  3955 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  3956 | `	}` |
|   118796 |  3957 | `	if( pTos->x.iVal ){` |
|        - |  3958 | `		/* Take the jump */` |
|     5206 |  3959 | `		pc = pInstr->iP2 - 1;` |
|     2602 |  3960 | `	}` |
|   118796 |  3961 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  3962 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  3963 | `	}` |
|   118796 |  3964 | `	break;` |
|        - |  3965 | `/*` |
|        - |  3966 | ` * NOOP: * * *` |
|        - |  3967 | ` *` |
|        - |  3968 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  3969 | ` * destination.` |
|        - |  3970 | ` */` |
|      ! 0 |  3971 | `case PH7_OP_NOOP:` |
|      ! 0 |  3972 | `	break;` |
|        - |  3973 | `/*` |
|        - |  3974 | ` * POP: P1 * *` |
|        - |  3975 | ` *` |
|        - |  3976 | ` * Pop P1 elements from the operand stack.` |
|        - |  3977 | ` */` |
|   437471 |  3978 | `case PH7_OP_POP: {` |
|   874988 |  3979 | `	sxi32 n = pInstr->iP1;` |
|   874988 |  3980 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  3981 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       17 |  3982 | `		n = (sxi32)(pTos - pStack);` |
|        8 |  3983 | `	}` |
|   874988 |  3984 | `	VmPopOperand(&pTos,n);` |
|   874988 |  3985 | `	break;` |
|        - |  3986 | `				 }` |
|        - |  3987 | `/*` |
|        - |  3988 | ` * DUP: * * *` |
|        - |  3989 | ` *` |
|        - |  3990 | ` * Duplicate the top of the stack.` |
|        - |  3991 | ` */` |
|       41 |  3992 | `case PH7_OP_DUP:` |
|        - |  3993 | `#ifdef UNTRUST` |
|        - |  3994 | `	if( pTos < pStack ){` |
|        - |  3995 | `		goto Abort;` |
|        - |  3996 | `	}` |
|        - |  3997 | `#endif` |
|       84 |  3998 | `	pTos++;` |
|       84 |  3999 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  4000 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  4001 | `	break;` |
|        - |  4002 | `/*` |
|        - |  4003 | ` * NSSWITCH: * * P3` |
|        - |  4004 | ` *` |
|        - |  4005 | ` * Switch the active namespace at runtime.` |
|        - |  4006 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  4007 | ` */` |
|     7307 |  4008 | `case PH7_OP_NSSWITCH:` |
|    14616 |  4009 | `	SyBlobReset(&pVm->sNamespace);` |
|    14616 |  4010 | `	if( pInstr->p3 ){` |
|       98 |  4011 | `		const char *zNs = (const char *)pInstr->p3;` |
|       98 |  4012 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       48 |  4013 | `	}` |
|        - |  4014 | `	/* Clear namespace-scoped use-const imports */` |
|    14616 |  4015 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    14616 |  4016 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    14616 |  4017 | `	break;` |
|        - |  4018 | `/* OP_USECONST P1 * P3` |
|        - |  4019 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  4020 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  4021 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  4022 | ` */` |
|        7 |  4023 | `case PH7_OP_USECONST: {` |
|       16 |  4024 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  4025 | `	if( azPair ){` |
|       16 |  4026 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  4027 | `	}` |
|       16 |  4028 | `	break;` |
|        - |  4029 | `				}` |
|        - |  4030 | `/*` |
|        - |  4031 | ` * CVT_INT: * * *` |
|        - |  4032 | ` *` |
|        - |  4033 | ` * Force the top of the stack to be an integer.` |
|        - |  4034 | ` */` |
|       78 |  4035 | `case PH7_OP_CVT_INT:` |
|        - |  4036 | `#ifdef UNTRUST` |
|        - |  4037 | `	if( pTos < pStack ){` |
|        - |  4038 | `		goto Abort;` |
|        - |  4039 | `	}` |
|        - |  4040 | `#endif` |
|      158 |  4041 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      111 |  4042 | `		PH7_MemObjToInteger(pTos);` |
|       55 |  4043 | `	}` |
|        - |  4044 | `	/* Invalidate any prior representation */` |
|      158 |  4045 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      158 |  4046 | `	break;` |
|        - |  4047 | `/*` |
|        - |  4048 | ` * CVT_REAL: * * *` |
|        - |  4049 | ` *` |
|        - |  4050 | ` * Force the top of the stack to be a real.` |
|        - |  4051 | ` */` |
|        5 |  4052 | `case PH7_OP_CVT_REAL:` |
|        - |  4053 | `#ifdef UNTRUST` |
|        - |  4054 | `	if( pTos < pStack ){` |
|        - |  4055 | `		goto Abort;` |
|        - |  4056 | `	}` |
|        - |  4057 | `#endif` |
|       11 |  4058 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4059 | `		PH7_MemObjToReal(pTos);` |
|        3 |  4060 | `	}` |
|        - |  4061 | `	/* Invalidate any prior representation */` |
|       11 |  4062 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       11 |  4063 | `	break;` |
|        - |  4064 | `/*` |
|        - |  4065 | ` * CVT_STR: * * *` |
|        - |  4066 | ` *` |
|        - |  4067 | ` * Force the top of the stack to be a string.` |
|        - |  4068 | ` */` |
|      146 |  4069 | `case PH7_OP_CVT_STR:` |
|        - |  4070 | `#ifdef UNTRUST` |
|        - |  4071 | `	if( pTos < pStack ){` |
|        - |  4072 | `		goto Abort;` |
|        - |  4073 | `	}` |
|        - |  4074 | `#endif` |
|      294 |  4075 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  4076 | `		PH7_MemObjToString(pTos);` |
|      146 |  4077 | `	}` |
|      294 |  4078 | `	break;` |
|        - |  4079 | `/*` |
|        - |  4080 | ` * CVT_BOOL: * * *` |
|        - |  4081 | ` *` |
|        - |  4082 | ` * Force the top of the stack to be a boolean.` |
|        - |  4083 | ` */` |
|        5 |  4084 | `case PH7_OP_CVT_BOOL:` |
|        - |  4085 | `#ifdef UNTRUST` |
|        - |  4086 | `	if( pTos < pStack ){` |
|        - |  4087 | `		goto Abort;` |
|        - |  4088 | `	}` |
|        - |  4089 | `#endif` |
|       11 |  4090 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  4091 | `		PH7_MemObjToBool(pTos);` |
|        3 |  4092 | `	}` |
|       11 |  4093 | `	break;` |
|        - |  4094 | `/*` |
|        - |  4095 | ` * CVT_NULL: * * *` |
|        - |  4096 | ` *` |
|        - |  4097 | ` * Nullify the top of the stack.` |
|        - |  4098 | ` */` |
|        3 |  4099 | `case PH7_OP_CVT_NULL:` |
|        - |  4100 | `#ifdef UNTRUST` |
|        - |  4101 | `	if( pTos < pStack ){` |
|        - |  4102 | `		goto Abort;` |
|        - |  4103 | `	}` |
|        - |  4104 | `#endif` |
|        7 |  4105 | `	PH7_MemObjRelease(pTos);` |
|        7 |  4106 | `	break;` |
|        - |  4107 | `/*` |
|        - |  4108 | ` * CVT_NUMC: * * *` |
|        - |  4109 | ` *` |
|        - |  4110 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  4111 | ` */` |
|      ! 0 |  4112 | `case PH7_OP_CVT_NUMC:` |
|        - |  4113 | `#ifdef UNTRUST` |
|        - |  4114 | `	if( pTos < pStack ){` |
|        - |  4115 | `		goto Abort;` |
|        - |  4116 | `	}` |
|        - |  4117 | `#endif` |
|        - |  4118 | `	/* Force a numeric cast */` |
|      ! 0 |  4119 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  4120 | `	break;` |
|        - |  4121 | `/*` |
|        - |  4122 | ` * CVT_ARRAY: * * *` |
|        - |  4123 | ` *` |
|        - |  4124 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  4125 | ` */` |
|       10 |  4126 | `case PH7_OP_CVT_ARRAY:` |
|        - |  4127 | `#ifdef UNTRUST` |
|        - |  4128 | `	if( pTos < pStack ){` |
|        - |  4129 | `		goto Abort;` |
|        - |  4130 | `	}` |
|        - |  4131 | `#endif` |
|        - |  4132 | `	/* Force a hashmap cast */` |
|       21 |  4133 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  4134 | `	if( rc != SXRET_OK ){` |
|        - |  4135 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  4136 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4137 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  4138 | `	}` |
|       21 |  4139 | `	break;` |
|        - |  4140 | `/*` |
|        - |  4141 | ` * CVT_OBJ: * * *` |
|        - |  4142 | ` *` |
|        - |  4143 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  4144 | ` */` |
|        8 |  4145 | `case PH7_OP_CVT_OBJ:` |
|        - |  4146 | `#ifdef UNTRUST` |
|        - |  4147 | `	if( pTos < pStack ){` |
|        - |  4148 | `		goto Abort;` |
|        - |  4149 | `	}` |
|        - |  4150 | `#endif` |
|       17 |  4151 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  4152 | `		/* Force a 'stdClass()' cast */` |
|       17 |  4153 | `		PH7_MemObjToObject(pTos);` |
|        8 |  4154 | `	}` |
|       17 |  4155 | `	break;` |
|        - |  4156 | `/*` |
|        - |  4157 | ` * ERR_CTRL * * *` |
|        - |  4158 | ` *` |
|        - |  4159 | ` * Error control operator.` |
|        - |  4160 | ` */` |
|    14939 |  4161 | `case PH7_OP_ERR_CTRL:` |
|        - |  4162 | `	/*` |
|        - |  4163 | `	 * TICKET 1433-038:` |
|        - |  4164 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  4165 | `	 * use the public API,to control error output.` |
|        - |  4166 | `	 */` |
|    29878 |  4167 | `	break;` |
|        - |  4168 | `/*` |
|        - |  4169 | ` * IS_A * * *` |
|        - |  4170 | ` *` |
|        - |  4171 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  4172 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  4173 | ` * holding a class name or an object).` |
|        - |  4174 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  4175 | ` */` |
|       42 |  4176 | `case PH7_OP_IS_A:{` |
|       86 |  4177 | `	ph7_value *pNos = &pTos[-1];` |
|       86 |  4178 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  4179 | `#ifdef UNTRUST` |
|        - |  4180 | `	if( pNos < pStack ){` |
|        - |  4181 | `		goto Abort;` |
|        - |  4182 | `	}` |
|        - |  4183 | `#endif` |
|       86 |  4184 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       84 |  4185 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       84 |  4186 | `		ph7_class *pClass = 0;` |
|        - |  4187 | `		/* Extract the target class */` |
|       84 |  4188 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4189 | `			/* Instance already loaded */` |
|      ! 0 |  4190 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       84 |  4191 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       84 |  4192 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       84 |  4193 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  4194 | `			/* Handle self/static/parent keywords */` |
|       84 |  4195 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  4196 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       82 |  4197 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  4198 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       81 |  4199 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  4200 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  4201 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  4202 | `					pClass = pSelf->pBase;` |
|        2 |  4203 | `				}` |
|        3 |  4204 | `			}else{` |
|       74 |  4205 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  4206 | `			}` |
|       41 |  4207 | `		}` |
|       84 |  4208 | `		if( pClass ){` |
|        - |  4209 | `			/* Perform the query */` |
|       84 |  4210 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       41 |  4211 | `		}` |
|       41 |  4212 | `	}` |
|        - |  4213 | `	/* Push result */` |
|       86 |  4214 | `	VmPopOperand(&pTos,1);` |
|       86 |  4215 | `	PH7_MemObjRelease(pTos);` |
|       86 |  4216 | `	pTos->x.iVal = iRes;` |
|       86 |  4217 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       86 |  4218 | `	break;` |
|        - |  4219 | `				 }` |
|        - |  4220 |  |
|        - |  4221 | `/*` |
|        - |  4222 | ` * LOADC P1 P2 *` |
|        - |  4223 | ` *` |
|        - |  4224 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  4225 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  4226 | ` */` |
|   950336 |  4227 | `case PH7_OP_LOADC: {` |
|        - |  4228 | `	ph7_value *pObj;` |
|        - |  4229 | `	/* Reserve a room */` |
|  1900718 |  4230 | `	pTos++;` |
|  2841903 |  4231 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1900718 |  4232 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  4233 | `			SyHashEntry *pEntry;` |
|        - |  4234 | `			/* Check use const imports first — imports take precedence */` |
|        - |  4235 | `			{` |
|        - |  4236 | `				SyHashEntry *pConstImport;` |
|    27590 |  4237 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    18392 |  4238 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    18394 |  4239 | `				if( pConstImport ){` |
|       11 |  4240 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  4241 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  4242 | `					if( pEntry ){` |
|       11 |  4243 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  4244 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  4245 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  4246 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  4247 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  4248 | `						break;` |
|        - |  4249 | `					}` |
|        - |  4250 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  4251 | `				}` |
|        - |  4252 | `			}` |
|        - |  4253 | `			/* Candidate for expansion via user defined callbacks */` |
|    18384 |  4254 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    18384 |  4255 | `			if( pEntry ){` |
|    18380 |  4256 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  4257 | `				/* Set a NULL default value */` |
|    18380 |  4258 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    18380 |  4259 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  4260 | `				/* Invoke the callback and deal with the expanded value */` |
|    18380 |  4261 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  4262 | `				/* Mark as constant */` |
|    18380 |  4263 | `				pTos->nIdx = SXU32_HIGH;` |
|    18380 |  4264 | `				break;` |
|        - |  4265 | `			}` |
|        - |  4266 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  4267 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  4268 | `			 * use-const imports → current NS → global → string fallback).` |
|        - |  4269 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - |  4270 | `			{` |
|        6 |  4271 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  4272 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  4273 | `				sxu32 j;` |
|        6 |  4274 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       14 |  4275 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|        9 |  4276 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|        5 |  4277 | `				}` |
|        6 |  4278 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  4279 | `					/* Try current_namespace\name */` |
|      ! 0 |  4280 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  4281 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  4282 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  4283 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  4284 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  4285 | `					if( pEntry ){` |
|      ! 0 |  4286 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  4287 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4288 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  4289 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  4290 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4291 | `						break;` |
|        - |  4292 | `					}` |
|        - |  4293 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  4294 | `				}` |
|        6 |  4295 | `				if( isQualified ){` |
|        - |  4296 | `					/* Qualified name: must be a real constant. */` |
|        3 |  4297 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  4298 | `					SyBlob sErr;` |
|        3 |  4299 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  4300 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  4301 | `					if( pErrFile ){` |
|        3 |  4302 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  4303 | `					}` |
|        3 |  4304 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  4305 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  4306 | `					SyBlobRelease(&sErr);` |
|        3 |  4307 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  4308 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  4309 | `					goto LoadC_Done;` |
|        - |  4310 | `				}` |
|        - |  4311 | `			}` |
|        1 |  4312 | `		}` |
|  1882328 |  4313 | `		PH7_MemObjLoad(pObj,pTos);` |
|   941187 |  4314 | `	}else{` |
|        - |  4315 | `		/* Set a NULL value */` |
|      ! 0 |  4316 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4317 | `	}` |
|   941142 |  4318 | `LoadC_Done:` |
|        - |  4319 | `	/* Mark as constant */` |
|  1882330 |  4320 | `	pTos->nIdx = SXU32_HIGH;` |
|  1882330 |  4321 | `	break;` |
|        - |  4322 | `				  }` |
|        - |  4323 | `/*` |
|        - |  4324 | ` * LOAD: P1 * P3` |
|        - |  4325 | ` *` |
|        - |  4326 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  4327 | ` * from the P3 operand.` |
|        - |  4328 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  4329 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  4330 | ` */` |
|  1504470 |  4331 | `case PH7_OP_LOAD:{` |
|        - |  4332 | `	ph7_value *pObj;` |
|        - |  4333 | `	SyString sName;` |
|  3009162 |  4334 | `	if( pInstr->p3 == 0 ){` |
|        - |  4335 | `		/* Take the variable name from the top of the stack */` |
|        - |  4336 | `#ifdef UNTRUST` |
|        - |  4337 | `		if( pTos < pStack ){` |
|        - |  4338 | `			goto Abort;` |
|        - |  4339 | `		}` |
|        - |  4340 | `#endif` |
|        - |  4341 | `		/* Force a string cast */` |
|       19 |  4342 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4343 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4344 | `		}` |
|       19 |  4345 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  4346 | `	}else{` |
|  3009144 |  4347 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4348 | `		/* Reserve a room for the target object */` |
|  3009144 |  4349 | `		pTos++;` |
|        - |  4350 | `	}` |
|        - |  4351 | `	/* Extract the requested memory object */` |
|  3009162 |  4352 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  3009162 |  4353 | `	if( pObj == 0 ){` |
|       28 |  4354 | `		if( pInstr->iP1 ){` |
|        - |  4355 | `			/* Variable not found,load NULL */` |
|       28 |  4356 | `			if( !pInstr->p3 ){` |
|      ! 0 |  4357 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4358 | `			}else{` |
|       28 |  4359 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4360 | `			}` |
|       28 |  4361 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1504485 |  4362 | `			break;` |
|      ! 0 |  4363 | `		}else{` |
|        - |  4364 | `			/* Fatal error */` |
|      ! 0 |  4365 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4366 | `			goto Abort;` |
|        - |  4367 | `		}` |
|        - |  4368 | `	}` |
|        - |  4369 | `	/* Load variable contents */` |
|  3009136 |  4370 | `	PH7_MemObjLoad(pObj,pTos);` |
|  3009136 |  4371 | `	pTos->nIdx = pObj->nIdx;` |
|  3009136 |  4372 | `	break;` |
|        - |  4373 | `				   }` |
|        - |  4374 | `/*` |
|        - |  4375 | ` * LOAD_MAP P1 * *` |
|        - |  4376 | ` *` |
|        - |  4377 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  4378 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  4379 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  4380 | ` */` |
|    21134 |  4381 | `case PH7_OP_LOAD_MAP: {` |
|        - |  4382 | `	ph7_hashmap *pMap;` |
|        - |  4383 | `	/* Allocate a new hashmap instance */` |
|    42270 |  4384 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    42270 |  4385 | `	if( pMap == 0 ){` |
|      ! 0 |  4386 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4387 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  4388 | `		goto Abort;` |
|        - |  4389 | `	}` |
|    42270 |  4390 | `	if( pInstr->iP1 > 0 ){` |
|     2378 |  4391 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  4392 | `		/* Perform the insertion */` |
|     7294 |  4393 | `		while( pEntry < pTos ){` |
|     4918 |  4394 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  4395 | `				/* Insertion by reference */` |
|      142 |  4396 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  4397 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  4398 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  4399 | `					);` |
|       48 |  4400 | `			}else{` |
|        - |  4401 | `				/* Standard insertion */` |
|     7235 |  4402 | `				PH7_HashmapInsert(pMap,` |
|     4822 |  4403 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2411 |  4404 | `					&pEntry[1]` |
|        - |  4405 | `				);` |
|        - |  4406 | `			}` |
|        - |  4407 | `			/* Next pair on the stack */` |
|     4918 |  4408 | `			pEntry += 2;` |
|        2 |  4409 | `		}` |
|        - |  4410 | `		/* Pop P1 elements */` |
|     2378 |  4411 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1188 |  4412 | `	}` |
|        - |  4413 | `	/* Push the hashmap */` |
|    42270 |  4414 | `	pTos++;` |
|    42270 |  4415 | `	pTos->nIdx = SXU32_HIGH;` |
|    42270 |  4416 | `	pTos->x.pOther = pMap;` |
|    42270 |  4417 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    42270 |  4418 | `	break;` |
|        - |  4419 | `					  }` |
|        - |  4420 | `/*` |
|        - |  4421 | ` * LOAD_LIST: P1 * *` |
|        - |  4422 | ` *` |
|        - |  4423 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  4424 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  4425 | ` * Caveats:` |
|        - |  4426 | ` *  This implementation support only a single nesting level.` |
|        - |  4427 | ` */` |
|       48 |  4428 | `case PH7_OP_LOAD_LIST: {` |
|        - |  4429 | `	ph7_value *pEntry;` |
|       98 |  4430 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  4431 | `		/* Empty list,break immediately */` |
|      ! 0 |  4432 | `		break;` |
|        - |  4433 | `	}` |
|       98 |  4434 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  4435 | `#ifdef UNTRUST` |
|        - |  4436 | `	if( &pEntry[-1] < pStack ){` |
|        - |  4437 | `		goto Abort;` |
|        - |  4438 | `	}` |
|        - |  4439 | `#endif` |
|       98 |  4440 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  4441 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  4442 | `		ph7_hashmap_node *pNode;` |
|        - |  4443 | `		ph7_value sKey,*pObj;` |
|        - |  4444 | `		/* Start Copying */` |
|       91 |  4445 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  4446 | `		while( pEntry <= pTos ){` |
|      193 |  4447 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  4448 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  4449 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  4450 | `					if( rc == SXRET_OK ){` |
|        - |  4451 | `						/* Store node value */` |
|      165 |  4452 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  4453 | `					}else{` |
|        - |  4454 | `						/* Undefined array key */` |
|        - |  4455 | `						char zMsg[128];` |
|      ! 0 |  4456 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  4457 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4458 | `						PH7_MemObjRelease(pObj);` |
|        - |  4459 | `					}` |
|       82 |  4460 | `				}` |
|       82 |  4461 | `			}` |
|      193 |  4462 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  4463 | `			pEntry++;` |
|        1 |  4464 | `		}` |
|       46 |  4465 | `	}else{` |
|        - |  4466 | `		/* Source is not an array */` |
|        - |  4467 | `		ph7_value *pObj;` |
|       18 |  4468 | `		while( pEntry <= pTos ){` |
|       12 |  4469 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  4470 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  4471 | `					PH7_MemObjRelease(pObj);` |
|        5 |  4472 | `				}` |
|        5 |  4473 | `			}` |
|       12 |  4474 | `			pEntry++;` |
|        2 |  4475 | `		}` |
|        8 |  4476 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  4477 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  4478 | `			const char *zType = "unknown";` |
|        3 |  4479 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  4480 | `			char zMsg[256];` |
|        3 |  4481 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  4482 | `				zType = "string";` |
|        1 |  4483 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  4484 | `				zType = "int";` |
|      ! 0 |  4485 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4486 | `				zType = "float";` |
|      ! 0 |  4487 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4488 | `				zType = "object";` |
|      ! 0 |  4489 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  4490 | `				zType = "resource";` |
|      ! 0 |  4491 | `			}` |
|        3 |  4492 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  4493 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  4494 | `		}` |
|        - |  4495 | `	}` |
|       98 |  4496 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  4497 | `	break;` |
|        - |  4498 | `					   }` |
|        - |  4499 | `/*` |
|        - |  4500 | ` * LOAD_IDX: P1 P2 *` |
|        - |  4501 | ` *` |
|        - |  4502 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  4503 | ` * from the stack.` |
|        - |  4504 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  4505 | ` * instead.` |
|        - |  4506 | ` */` |
|   241751 |  4507 | `case PH7_OP_LOAD_IDX: {` |
|   483548 |  4508 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   483548 |  4509 | `	ph7_hashmap *pMap = 0;` |
|        - |  4510 | `	ph7_value *pIdx;` |
|   483548 |  4511 | `	pIdx = 0;` |
|   483548 |  4512 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  4513 | `		if( !pInstr->iP2){` |
|        - |  4514 | `			/* No available index,load NULL */` |
|      ! 0 |  4515 | `			if( pTos >= pStack ){` |
|      ! 0 |  4516 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4517 | `			}else{` |
|        - |  4518 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  4519 | `				pTos++;` |
|      ! 0 |  4520 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4521 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  4522 | `			}` |
|        - |  4523 | `			/* Emit a notice */` |
|      ! 0 |  4524 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  4525 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  4526 | `			break;` |
|        - |  4527 | `		}` |
|      ! 0 |  4528 | `	}else{` |
|   483548 |  4529 | `		pIdx = pTos;` |
|   483548 |  4530 | `		pTos--;` |
|        - |  4531 | `	}` |
|   483548 |  4532 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  4533 | `		/* String access */` |
|   377664 |  4534 | `		if( pIdx ){` |
|        - |  4535 | `			sxu32 nOfft;` |
|   377664 |  4536 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  4537 | `				/* Force an int cast */` |
|      ! 0 |  4538 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4539 | `			}` |
|   377664 |  4540 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   377664 |  4541 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  4542 | `				/* Invalid offset,load null */` |
|      ! 0 |  4543 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4544 | `			}else{` |
|   377664 |  4545 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   377664 |  4546 | `				int c = zData[nOfft];` |
|   377664 |  4547 | `				PH7_MemObjRelease(pTos);` |
|   377664 |  4548 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   377664 |  4549 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  4550 | `			}` |
|   188855 |  4551 | `		}else{` |
|        - |  4552 | `			/* No available index,load NULL */` |
|      ! 0 |  4553 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4554 | `		}` |
|   377664 |  4555 | `		break;` |
|        - |  4556 | `	}` |
|   105886 |  4557 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  4558 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4559 | `			ph7_value *pObj;` |
|        3 |  4560 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4561 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  4562 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  4563 | `			}` |
|        1 |  4564 | `		}` |
|        1 |  4565 | `	}` |
|   105886 |  4566 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   105886 |  4567 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   105886 |  4568 | `		if( pInstr->iP2 == 1 ){` |
|        - |  4569 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  4570 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  4571 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  4572 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      883 |  4573 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      441 |  4574 | `		}` |
|        - |  4575 | `		/* Point to the hashmap */` |
|   105886 |  4576 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   105886 |  4577 | `		if( pIdx ){` |
|        - |  4578 | `			/* Load the desired entry */` |
|   105886 |  4579 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    52942 |  4580 | `		}` |
|   105886 |  4581 | `		if( pInstr->iP2 == 3 ){` |
|        - |  4582 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  4583 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  4584 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  4585 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  4586 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  4587 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  4588 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  4589 | `			 * correct for the outermost write. */` |
|       19 |  4590 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  4591 | `			if( !needWrite && pNode ){` |
|       13 |  4592 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  4593 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  4594 | `					needWrite = 1;` |
|        3 |  4595 | `				}` |
|        6 |  4596 | `			}` |
|       19 |  4597 | `			if( needWrite ){` |
|       13 |  4598 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  4599 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  4600 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  4601 | `					 * into the new map's storage. */` |
|        7 |  4602 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  4603 | `					if( pIdx ){` |
|        7 |  4604 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  4605 | `					}` |
|        3 |  4606 | `				}` |
|        6 |  4607 | `			}` |
|        9 |  4608 | `		}` |
|   105886 |  4609 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) ){` |
|        - |  4610 | `			/* Create a new empty entry */` |
|      273 |  4611 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  4612 | `			if( rc == SXRET_OK ){` |
|        - |  4613 | `				/* Point to the last inserted entry */` |
|      273 |  4614 | `				pNode = pMap->pLast;` |
|      136 |  4615 | `			}` |
|      136 |  4616 | `		}` |
|    52942 |  4617 | `	}` |
|   105886 |  4618 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  4619 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  4620 | `		char zMsg[128];` |
|      ! 0 |  4621 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4622 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4623 | `		}` |
|      ! 0 |  4624 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  4625 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4626 | `	}` |
|   105886 |  4627 | `	if( pIdx ){` |
|   105886 |  4628 | `		PH7_MemObjRelease(pIdx);` |
|    52942 |  4629 | `	}` |
|   105886 |  4630 | `	if( rc == SXRET_OK ){` |
|        - |  4631 | `		/* Load entry contents */` |
|    47190 |  4632 | `		if( pMap->iRef < 2 ){` |
|        - |  4633 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  4634 | `			 * of the entry value,rather than pointing to it.` |
|        - |  4635 | `			 */` |
|       24 |  4636 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  4637 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  4638 | `		}else{` |
|    47168 |  4639 | `			pTos->nIdx = pNode->nValIdx;` |
|    47168 |  4640 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    47168 |  4641 | `			PH7_HashmapUnref(pMap);` |
|        - |  4642 | `		}` |
|    23596 |  4643 | `	}else{` |
|        - |  4644 | `		/* No such entry,load NULL */` |
|    58698 |  4645 | `		PH7_MemObjRelease(pTos);` |
|    58698 |  4646 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  4647 | `	}` |
|   105886 |  4648 | `	break;` |
|        - |  4649 | `					  }` |
|        - |  4650 | `/*` |
|        - |  4651 | ` * LOAD_CLOSURE * * P3` |
|        - |  4652 | ` *` |
|        - |  4653 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  4654 | ` * name in the stack.` |
|        - |  4655 | ` */` |
|       45 |  4656 | `case PH7_OP_LOAD_CLOSURE:{` |
|       91 |  4657 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       91 |  4658 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  4659 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  4660 | `		ph7_vm_func *pClosure;` |
|        - |  4661 | `		char *zName;` |
|        - |  4662 | `		sxu32 mLen;` |
|        - |  4663 | `		sxu32 n;` |
|        - |  4664 | `		/* Create a new VM function */` |
|       91 |  4665 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  4666 | `		/* Generate an unique closure name */` |
|       91 |  4667 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|       91 |  4668 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  4669 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  4670 | `			goto Abort;` |
|        - |  4671 | `		}` |
|       91 |  4672 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|       91 |  4673 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  4674 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  4675 | `		}` |
|        - |  4676 | `		/* Zero the stucture */` |
|       91 |  4677 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  4678 | `		/* Perform a structure assignment on read-only items */` |
|       91 |  4679 | `		pClosure->aArgs = pFunc->aArgs;` |
|       91 |  4680 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|       91 |  4681 | `		pClosure->aStatic = pFunc->aStatic;` |
|       91 |  4682 | `		pClosure->iFlags = pFunc->iFlags;` |
|       91 |  4683 | `		pClosure->pUserData = pFunc->pUserData;` |
|       91 |  4684 | `		pClosure->sSignature = pFunc->sSignature;` |
|       91 |  4685 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|       91 |  4686 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|       91 |  4687 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|       91 |  4688 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|       91 |  4689 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  4690 | `		/* Register the closure */` |
|       91 |  4691 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  4692 | `		/* Set up closure environment */` |
|       91 |  4693 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|       91 |  4694 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      245 |  4695 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  4696 | `			ph7_value *pValue;` |
|      155 |  4697 | `			pEnv = &aEnv[n];` |
|      155 |  4698 | `			sEnv.sName  = pEnv->sName;` |
|      155 |  4699 | `			sEnv.iFlags = pEnv->iFlags;` |
|      155 |  4700 | `			sEnv.nIdx = SXU32_HIGH;` |
|      155 |  4701 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      155 |  4702 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  4703 | `				/* Pass by reference */` |
|      ! 0 |  4704 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  4705 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  4706 | `					);` |
|      ! 0 |  4707 | `			}` |
|        - |  4708 | `			/* Standard pass by value */` |
|      155 |  4709 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      155 |  4710 | `			if( pValue ){` |
|        - |  4711 | `				/* Copy imported value */` |
|       69 |  4712 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       34 |  4713 | `			}` |
|        - |  4714 | `			/* Insert the imported variable */` |
|      155 |  4715 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       78 |  4716 | `		}` |
|        - |  4717 | `		/* Finally,load the closure name on the stack */` |
|       91 |  4718 | `		pTos++;` |
|       91 |  4719 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       45 |  4720 | `	}` |
|       91 |  4721 | `	break;` |
|        - |  4722 | `						 }` |
|        - |  4723 | `/*` |
|        - |  4724 | ` * STORE * P2 P3` |
|        - |  4725 | ` *` |
|        - |  4726 | ` * Perform a store (Assignment) operation.` |
|        - |  4727 | ` */` |
|   133536 |  4728 | `case PH7_OP_STORE: {` |
|        - |  4729 | `	ph7_value *pObj;` |
|        - |  4730 | `	SyString sName;` |
|        - |  4731 | `#ifdef UNTRUST` |
|        - |  4732 | `	if( pTos < pStack ){` |
|        - |  4733 | `		goto Abort;` |
|        - |  4734 | `	}` |
|        - |  4735 | `#endif` |
|   267074 |  4736 | `	if( pInstr->iP2 ){` |
|        - |  4737 | `		sxu32 nIdx;` |
|        - |  4738 | `		sxi32 rcT;` |
|        - |  4739 | `		/* Member store operation */` |
|     4292 |  4740 | `		nIdx = pTos->nIdx;` |
|     4292 |  4741 | `		VmPopOperand(&pTos,1);` |
|     4292 |  4742 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  4743 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4744 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  4745 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  4746 | `		}else{` |
|        - |  4747 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  4748 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     4288 |  4749 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     4288 |  4750 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  4751 | `				goto Abort;` |
|        - |  4752 | `			}` |
|     4288 |  4753 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  4754 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  4755 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  4756 | `				 * propagate out of the VM loop. */` |
|       37 |  4757 | `				VmPopOperand(&pTos,1);` |
|        - |  4758 | `				{` |
|       37 |  4759 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       37 |  4760 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       37 |  4761 | `						pc = pFrm2->iExceptionJump - 1;` |
|   133555 |  4762 | `						break;` |
|        - |  4763 | `					}` |
|        - |  4764 | `				}` |
|      ! 0 |  4765 | `				goto Exception;` |
|        - |  4766 | `			}` |
|        - |  4767 | `			/* Point to the desired memory object */` |
|     4252 |  4768 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     4252 |  4769 | `			if( pObj ){` |
|        - |  4770 | `				/* Perform the store operation */` |
|     4252 |  4771 | `				PH7_MemObjStore(pTos,pObj);` |
|     2125 |  4772 | `			}` |
|        - |  4773 | `		}` |
|     4256 |  4774 | `		break;` |
|   262784 |  4775 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  4776 | `		/* Take the variable name from the next on the stack */` |
|        7 |  4777 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4778 | `			/* Force a string cast */` |
|      ! 0 |  4779 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4780 | `		}` |
|        7 |  4781 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  4782 | `		pTos--;` |
|        - |  4783 | `#ifdef UNTRUST` |
|        - |  4784 | `		if( pTos < pStack  ){` |
|        - |  4785 | `			goto Abort;` |
|        - |  4786 | `		}` |
|        - |  4787 | `#endif` |
|        4 |  4788 | `	}else{` |
|   262778 |  4789 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4790 | `	}` |
|        - |  4791 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   262784 |  4792 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   262784 |  4793 | `	if( pObj == 0 ){` |
|      ! 0 |  4794 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4795 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4796 | `		goto Abort;` |
|        - |  4797 | `	}` |
|   262784 |  4798 | `	if( !pInstr->p3 ){` |
|        7 |  4799 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  4800 | `	}` |
|        - |  4801 | `	/* Perform the store operation */` |
|   262784 |  4802 | `	PH7_MemObjStore(pTos,pObj);` |
|   262784 |  4803 | `	break;` |
|        - |  4804 | `				   }` |
|        - |  4805 | `/*` |
|        - |  4806 | ` * STORE_IDX:   P1 * P3` |
|        - |  4807 | ` * STORE_IDX_R: P1 * P3` |
|        - |  4808 | ` *` |
|        - |  4809 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  4810 | ` */` |
|    91708 |  4811 | `case PH7_OP_STORE_IDX:` |
|        - |  4812 | `case PH7_OP_STORE_IDX_REF: {` |
|   183418 |  4813 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  4814 | `	ph7_value *pKey;` |
|        - |  4815 | `	sxu32 nIdx;` |
|   183418 |  4816 | `	if( pInstr->iP1 ){` |
|        - |  4817 | `		/* Key is next on stack */` |
|    61250 |  4818 | `		pKey = pTos;` |
|    61250 |  4819 | `		pTos--;` |
|    30626 |  4820 | `	}else{` |
|   122170 |  4821 | `		pKey = 0;` |
|        - |  4822 | `	}` |
|   183418 |  4823 | `	nIdx = pTos->nIdx;` |
|   183418 |  4824 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  4825 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  4826 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  4827 | `		 * checking true sharing count, then re-add after separation. */` |
|   183366 |  4828 | `		if( nIdx != SXU32_HIGH ){` |
|   183366 |  4829 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   275048 |  4830 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   183366 |  4831 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4832 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  4833 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  4834 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  4835 | `				 * refcounts if the backing array was already separated. */` |
|   183366 |  4836 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   183366 |  4837 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   183366 |  4838 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   183366 |  4839 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   183366 |  4840 | `					pTos->x.pOther = pMap;` |
|    91684 |  4841 | `				}else{` |
|        - |  4842 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  4843 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  4844 | `					pMap = pCur;` |
|        - |  4845 | `				}` |
|    91684 |  4846 | `			}else{` |
|      ! 0 |  4847 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4848 | `			}` |
|    91684 |  4849 | `		}else{` |
|      ! 0 |  4850 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4851 | `		}` |
|   183366 |  4852 | `		if( pMap->iRef < 2 ){` |
|        - |  4853 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  4854 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  4855 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  4856 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  4857 | `			pMap->iRef = 2;` |
|      ! 0 |  4858 | `		}` |
|    91684 |  4859 | `	}else{` |
|        - |  4860 | `		ph7_value *pObj;` |
|       53 |  4861 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  4862 | `		if( pObj == 0 ){` |
|      ! 0 |  4863 | `			if( pKey ){` |
|      ! 0 |  4864 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  4865 | `			}` |
|      ! 0 |  4866 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4867 | `			break;` |
|        - |  4868 | `		}` |
|        - |  4869 | `		/* Phase#1: Load the array */` |
|       53 |  4870 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  4871 | `			VmPopOperand(&pTos,1);` |
|       53 |  4872 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  4873 | `				/* Force a string cast */` |
|      ! 0 |  4874 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  4875 | `			}` |
|       53 |  4876 | `			if( pKey == 0 ){` |
|        - |  4877 | `				/* Append string */` |
|        3 |  4878 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  4879 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  4880 | `				}` |
|        2 |  4881 | `			}else{` |
|        - |  4882 | `				sxu32 nOfft;` |
|       51 |  4883 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  4884 | `					/* Force an int cast */` |
|       51 |  4885 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  4886 | `				}` |
|       51 |  4887 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  4888 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  4889 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  4890 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  4891 | `					zData[nOfft] = zBlob[0];` |
|       26 |  4892 | `				}else{` |
|      ! 0 |  4893 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  4894 | `						/* Perform an append operation */` |
|      ! 0 |  4895 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  4896 | `					}` |
|        - |  4897 | `				}` |
|        - |  4898 | `			}` |
|       53 |  4899 | `			if( pKey ){` |
|       51 |  4900 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  4901 | `			}` |
|       53 |  4902 | `			break;` |
|      ! 0 |  4903 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  4904 | `			/* Force a hashmap cast  */` |
|      ! 0 |  4905 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  4906 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  4907 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  4908 | `				goto Abort;` |
|        - |  4909 | `			}` |
|      ! 0 |  4910 | `		}` |
|        - |  4911 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  4912 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  4913 | `	}` |
|   183366 |  4914 | `	VmPopOperand(&pTos,1);` |
|        - |  4915 | `	/* Phase#2: Perform the insertion */` |
|   183366 |  4916 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  4917 | `		/* Insertion by reference */` |
|       15 |  4918 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  4919 | `	}else{` |
|   183352 |  4920 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  4921 | `	}` |
|   183366 |  4922 | `	if( pKey ){` |
|    61200 |  4923 | `		PH7_MemObjRelease(pKey);` |
|    30599 |  4924 | `	}` |
|   183366 |  4925 | `	break;` |
|        - |  4926 | `					   }` |
|        - |  4927 | `/*` |
|        - |  4928 | ` * INCR: P1 * *` |
|        - |  4929 | ` *` |
|        - |  4930 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  4931 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  4932 | ` * the stack and increment after that.` |
|        - |  4933 | ` */` |
|   163534 |  4934 | `case PH7_OP_INCR:` |
|        - |  4935 | `#ifdef UNTRUST` |
|        - |  4936 | `	if( pTos < pStack ){` |
|        - |  4937 | `		goto Abort;` |
|        - |  4938 | `	}` |
|        - |  4939 | `#endif` |
|   327114 |  4940 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   327114 |  4941 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4942 | `			ph7_value *pObj;` |
|   327114 |  4943 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  4944 | `				/* Force a numeric cast */` |
|   327114 |  4945 | `				PH7_MemObjToNumeric(pObj);` |
|   327114 |  4946 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4947 | `					pObj->rVal++;` |
|        - |  4948 | `					/* Try to get an integer representation */` |
|      ! 0 |  4949 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4950 | `				}else{` |
|   327114 |  4951 | `					pObj->x.iVal++;` |
|   327114 |  4952 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4953 | `				}` |
|   327114 |  4954 | `				if( pInstr->iP1 ){` |
|        - |  4955 | `					/* Pre-icrement */` |
|       77 |  4956 | `					PH7_MemObjStore(pObj,pTos);` |
|       38 |  4957 | `				}` |
|   163578 |  4958 | `			}` |
|   163580 |  4959 | `		}else{` |
|      ! 0 |  4960 | `			if( pInstr->iP1 ){` |
|        - |  4961 | `				/* Force a numeric cast */` |
|      ! 0 |  4962 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  4963 | `				/* Pre-increment */` |
|      ! 0 |  4964 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4965 | `					pTos->rVal++;` |
|        - |  4966 | `					/* Try to get an integer representation */` |
|      ! 0 |  4967 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4968 | `				}else{` |
|      ! 0 |  4969 | `					pTos->x.iVal++;` |
|      ! 0 |  4970 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4971 | `				}` |
|      ! 0 |  4972 | `			}` |
|        - |  4973 | `		}` |
|   163578 |  4974 | `	}` |
|   327114 |  4975 | `	break;` |
|        - |  4976 | `/*` |
|        - |  4977 | ` * DECR: P1 * *` |
|        - |  4978 | ` *` |
|        - |  4979 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  4980 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  4981 | ` * and decrement after that.` |
|        - |  4982 | ` */` |
|        2 |  4983 | `case PH7_OP_DECR:` |
|        - |  4984 | `#ifdef UNTRUST` |
|        - |  4985 | `	if( pTos < pStack ){` |
|        - |  4986 | `		goto Abort;` |
|        - |  4987 | `	}` |
|        - |  4988 | `#endif` |
|        5 |  4989 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  4990 | `		/* Force a numeric cast */` |
|        5 |  4991 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  4992 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4993 | `			ph7_value *pObj;` |
|        5 |  4994 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  4995 | `				/* Force a numeric cast */` |
|        5 |  4996 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  4997 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4998 | `					pObj->rVal--;` |
|        - |  4999 | `					/* Try to get an integer representation */` |
|      ! 0 |  5000 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5001 | `				}else{` |
|        5 |  5002 | `					pObj->x.iVal--;` |
|        5 |  5003 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5004 | `				}` |
|        5 |  5005 | `				if( pInstr->iP1 ){` |
|        - |  5006 | `					/* Pre-icrement */` |
|      ! 0 |  5007 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  5008 | `				}` |
|        2 |  5009 | `			}` |
|        3 |  5010 | `		}else{` |
|      ! 0 |  5011 | `			if( pInstr->iP1 ){` |
|        - |  5012 | `				/* Pre-increment */` |
|      ! 0 |  5013 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5014 | `					pTos->rVal--;` |
|        - |  5015 | `					/* Try to get an integer representation */` |
|      ! 0 |  5016 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5017 | `				}else{` |
|      ! 0 |  5018 | `					pTos->x.iVal--;` |
|      ! 0 |  5019 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5020 | `				}` |
|      ! 0 |  5021 | `			}` |
|        - |  5022 | `		}` |
|        2 |  5023 | `	}` |
|        5 |  5024 | `	break;` |
|        - |  5025 | `/*` |
|        - |  5026 | ` * UMINUS: * * *` |
|        - |  5027 | ` *` |
|        - |  5028 | ` * Perform a unary minus operation.` |
|        - |  5029 | ` */` |
|    27574 |  5030 | `case PH7_OP_UMINUS:` |
|        - |  5031 | `#ifdef UNTRUST` |
|        - |  5032 | `	if( pTos < pStack ){` |
|        - |  5033 | `		goto Abort;` |
|        - |  5034 | `	}` |
|        - |  5035 | `#endif` |
|        - |  5036 | `	/* Force a numeric (integer,real or both) cast */` |
|    55150 |  5037 | `	PH7_MemObjToNumeric(pTos);` |
|    55150 |  5038 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  5039 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  5040 | `	}` |
|    55150 |  5041 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    55120 |  5042 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    27559 |  5043 | `	}` |
|    55150 |  5044 | `	break;` |
|        - |  5045 | `/*` |
|        - |  5046 | ` * UPLUS: * * *` |
|        - |  5047 | ` *` |
|        - |  5048 | ` * Perform a unary plus operation.` |
|        - |  5049 | ` */` |
|       18 |  5050 | `case PH7_OP_UPLUS:` |
|        - |  5051 | `#ifdef UNTRUST` |
|        - |  5052 | `	if( pTos < pStack ){` |
|        - |  5053 | `		goto Abort;` |
|        - |  5054 | `	}` |
|        - |  5055 | `#endif` |
|        - |  5056 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  5057 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  5058 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5059 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  5060 | `	}` |
|       37 |  5061 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  5062 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  5063 | `	}` |
|       37 |  5064 | `	break;` |
|        - |  5065 | `/*` |
|        - |  5066 | ` * OP_LNOT: * * *` |
|        - |  5067 | ` *` |
|        - |  5068 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  5069 | ` * with its complement.` |
|        - |  5070 | ` */` |
|    42780 |  5071 | `case PH7_OP_LNOT:` |
|        - |  5072 | `#ifdef UNTRUST` |
|        - |  5073 | `	if( pTos < pStack ){` |
|        - |  5074 | `		goto Abort;` |
|        - |  5075 | `	}` |
|        - |  5076 | `#endif` |
|        - |  5077 | `	/* Force a boolean cast */` |
|    85606 |  5078 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       23 |  5079 | `		PH7_MemObjToBool(pTos);` |
|       11 |  5080 | `	}` |
|    85606 |  5081 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    85606 |  5082 | `	break;` |
|        - |  5083 | `/*` |
|        - |  5084 | ` * OP_BITNOT: * * *` |
|        - |  5085 | ` *` |
|        - |  5086 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  5087 | ` * with its ones-complement.` |
|        - |  5088 | ` */` |
|       15 |  5089 | `case PH7_OP_BITNOT:` |
|        - |  5090 | `#ifdef UNTRUST` |
|        - |  5091 | `	if( pTos < pStack ){` |
|        - |  5092 | `		goto Abort;` |
|        - |  5093 | `	}` |
|        - |  5094 | `#endif` |
|        - |  5095 | `	/* Force an integer cast */` |
|       32 |  5096 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5097 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5098 | `	}` |
|       32 |  5099 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       32 |  5100 | `	break;` |
|        - |  5101 | `/* OP_MUL * * *` |
|        - |  5102 | ` * OP_MUL_STORE * * *` |
|        - |  5103 | ` *` |
|        - |  5104 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  5105 | ` * and push the result back onto the stack.` |
|        - |  5106 | ` */` |
|     1280 |  5107 | `case PH7_OP_MUL:` |
|        - |  5108 | `case PH7_OP_MUL_STORE: {` |
|     2562 |  5109 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5110 | `	/* Force the operand to be numeric */` |
|        - |  5111 | `#ifdef UNTRUST` |
|        - |  5112 | `	if( pNos < pStack ){` |
|        - |  5113 | `		goto Abort;` |
|        - |  5114 | `	}` |
|        - |  5115 | `#endif` |
|     2562 |  5116 | `	PH7_MemObjToNumeric(pTos);` |
|     2562 |  5117 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5118 | `	/* Perform the requested operation */` |
|     2562 |  5119 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5120 | `		/* Floating point arithemic */` |
|        - |  5121 | `		ph7_real a,b,r;` |
|       19 |  5122 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  5123 | `			PH7_MemObjToReal(pTos);` |
|        4 |  5124 | `		}` |
|       19 |  5125 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  5126 | `			PH7_MemObjToReal(pNos);` |
|        3 |  5127 | `		}` |
|       19 |  5128 | `		a = pNos->rVal;` |
|       19 |  5129 | `		b = pTos->rVal;` |
|       19 |  5130 | `		r = a * b;` |
|        - |  5131 | `		/* Push the result */` |
|       19 |  5132 | `		pNos->rVal = r;` |
|       19 |  5133 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5134 | `		/* Try to get an integer representation */` |
|       19 |  5135 | `		PH7_MemObjTryInteger(pNos);` |
|       10 |  5136 | `	}else{` |
|        - |  5137 | `		/* Integer arithmetic */` |
|        - |  5138 | `		sxi64 a,b,r;` |
|     2544 |  5139 | `		a = pNos->x.iVal;` |
|     2544 |  5140 | `		b = pTos->x.iVal;` |
|     2544 |  5141 | `		r = a * b;` |
|        - |  5142 | `		/* Push the result */` |
|     2544 |  5143 | `		pNos->x.iVal = r;` |
|     2544 |  5144 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5145 | `	}` |
|     2562 |  5146 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  5147 | `		ph7_value *pObj;` |
|       32 |  5148 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5149 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  5150 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  5151 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  5152 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  5153 | `		}` |
|       15 |  5154 | `	}` |
|     2562 |  5155 | `	VmPopOperand(&pTos,1);` |
|     2562 |  5156 | `	break;` |
|        - |  5157 | `				 }` |
|        - |  5158 | `/* OP_POW * * *` |
|        - |  5159 | ` * OP_POW_STORE * * *` |
|        - |  5160 | ` *` |
|        - |  5161 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - |  5162 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - |  5163 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - |  5164 | ` * fits in sxi64; otherwise the result is a double.` |
|        - |  5165 | ` */` |
|       63 |  5166 | `case PH7_OP_POW:` |
|        - |  5167 | `case PH7_OP_POW_STORE: {` |
|      127 |  5168 | `	ph7_value *pNos = &pTos[-1];` |
|      127 |  5169 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|        - |  5170 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|        - |  5171 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|        - |  5172 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|        - |  5173 | `	 */` |
|      127 |  5174 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|      127 |  5175 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|        - |  5176 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  5177 | `	int bBothInt;` |
|      127 |  5178 | `	int usedInt = 0;` |
|        - |  5179 | `	ph7_real a, b, r;` |
|        - |  5180 | `#endif` |
|      127 |  5181 | `	sxi64 base_i = 0, exp_i = 0;` |
|        - |  5182 | `#ifdef UNTRUST` |
|        - |  5183 | `	if( pNos < pStack ){` |
|        - |  5184 | `		goto Abort;` |
|        - |  5185 | `	}` |
|        - |  5186 | `#endif` |
|      127 |  5187 | `	PH7_MemObjToNumeric(pTos);` |
|      127 |  5188 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5189 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      249 |  5190 | `	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&` |
|      122 |  5191 | `	           ((pNos->iFlags & MEMOBJ_REAL) == 0);` |
|      127 |  5192 | `	if( bBothInt ){` |
|      117 |  5193 | `		base_i = pBase->x.iVal;` |
|      117 |  5194 | `		exp_i  = pExp->x.iVal;` |
|       58 |  5195 | `	}` |
|      127 |  5196 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|      119 |  5197 | `		PH7_MemObjToReal(pBase);` |
|       59 |  5198 | `	}` |
|      127 |  5199 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|      125 |  5200 | `		PH7_MemObjToReal(pExp);` |
|       62 |  5201 | `	}` |
|      127 |  5202 | `	a = pBase->rVal;` |
|      127 |  5203 | `	b = pExp->rVal;` |
|      127 |  5204 | `	r = pow(a, b);` |
|        - |  5205 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|        - |  5206 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|        - |  5207 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|        - |  5208 | `	 * representable as double but not as signed int64. */` |
|      127 |  5209 | `	if( bBothInt && exp_i >= 0 ){` |
|      111 |  5210 | `		sxi64 result_i = 1;` |
|      111 |  5211 | `		sxi64 cur_base = base_i;` |
|      111 |  5212 | `		sxi64 cur_exp  = exp_i;` |
|      111 |  5213 | `		int overflow = 0;` |
|      383 |  5214 | `		while( cur_exp > 0 ){` |
|      277 |  5215 | `			if( cur_exp & 1 ){` |
|      183 |  5216 | `				if( VmMulOverflow64(result_i, cur_base, &result_i) ){` |
|        3 |  5217 | `					overflow = 1;` |
|        3 |  5218 | `					break;` |
|        - |  5219 | `				}` |
|       90 |  5220 | `			}` |
|      275 |  5221 | `			cur_exp >>= 1;` |
|      275 |  5222 | `			if( cur_exp > 0 ){` |
|      175 |  5223 | `				if( VmMulOverflow64(cur_base, cur_base, &cur_base) ){` |
|        3 |  5224 | `					overflow = 1;` |
|        3 |  5225 | `					break;` |
|        - |  5226 | `				}` |
|       86 |  5227 | `			}` |
|        1 |  5228 | `		}` |
|      111 |  5229 | `		if( !overflow ){` |
|      107 |  5230 | `			pNos->x.iVal = result_i;` |
|      107 |  5231 | `			MemObjSetType(pNos, MEMOBJ_INT);` |
|      107 |  5232 | `			usedInt = 1;` |
|       53 |  5233 | `		}` |
|       55 |  5234 | `	}` |
|      127 |  5235 | `	if( !usedInt ){` |
|       21 |  5236 | `		pNos->rVal = r;` |
|       21 |  5237 | `		MemObjSetType(pNos, MEMOBJ_REAL);` |
|       10 |  5238 | `	}` |
|        - |  5239 | `#else` |
|        - |  5240 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|        - |  5241 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|        - |  5242 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|        - |  5243 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|        - |  5244 | `	 * represented. */` |
|        - |  5245 | `	base_i = pBase->x.iVal;` |
|        - |  5246 | `	exp_i  = pExp->x.iVal;` |
|        - |  5247 | `	{` |
|        - |  5248 | `		sxi64 result_i = 1;` |
|        - |  5249 | `		sxi64 cur_base = base_i;` |
|        - |  5250 | `		sxi64 cur_exp  = exp_i;` |
|        - |  5251 | `		if( cur_exp < 0 ){` |
|        - |  5252 | `			result_i = 0;` |
|        - |  5253 | `		}else{` |
|        - |  5254 | `			while( cur_exp > 0 ){` |
|        - |  5255 | `				if( cur_exp & 1 ){` |
|        - |  5256 | `					result_i *= cur_base;` |
|        - |  5257 | `				}` |
|        - |  5258 | `				cur_exp >>= 1;` |
|        - |  5259 | `				if( cur_exp > 0 ){` |
|        - |  5260 | `					cur_base *= cur_base;` |
|        - |  5261 | `				}` |
|        - |  5262 | `			}` |
|        - |  5263 | `		}` |
|        - |  5264 | `		pNos->x.iVal = result_i;` |
|        - |  5265 | `		MemObjSetType(pNos, MEMOBJ_INT);` |
|        - |  5266 | `	}` |
|        - |  5267 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      127 |  5268 | `	if( bStore ){` |
|        - |  5269 | `		ph7_value *pObj;` |
|       23 |  5270 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5271 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       23 |  5272 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       23 |  5273 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       23 |  5274 | `			PH7_MemObjStore(pNos,pObj);` |
|       11 |  5275 | `		}` |
|       11 |  5276 | `	}` |
|      127 |  5277 | `	VmPopOperand(&pTos,1);` |
|      127 |  5278 | `	break;` |
|        - |  5279 | `				 }` |
|        - |  5280 | `/* OP_ADD * * *` |
|        - |  5281 | ` *` |
|        - |  5282 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5283 | ` * and push the result back onto the stack.` |
|        - |  5284 | ` */` |
|      494 |  5285 | `case PH7_OP_ADD:{` |
|      990 |  5286 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5287 | `#ifdef UNTRUST` |
|        - |  5288 | `	if( pNos < pStack ){` |
|        - |  5289 | `		goto Abort;` |
|        - |  5290 | `	}` |
|        - |  5291 | `#endif` |
|        - |  5292 | `	/* Perform the addition */` |
|      990 |  5293 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      990 |  5294 | `	VmPopOperand(&pTos,1);` |
|      990 |  5295 | `	break;` |
|        - |  5296 | `				}` |
|        - |  5297 | `/*` |
|        - |  5298 | ` * OP_ADD_STORE * * *` |
|        - |  5299 | ` *` |
|        - |  5300 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5301 | ` * and push the result back onto the stack.` |
|        - |  5302 | ` */` |
|      502 |  5303 | `case PH7_OP_ADD_STORE:{` |
|     1006 |  5304 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5305 | `	ph7_value *pObj;` |
|        - |  5306 | `	sxu32 nIdx;` |
|        - |  5307 | `#ifdef UNTRUST` |
|        - |  5308 | `	if( pNos < pStack ){` |
|        - |  5309 | `		goto Abort;` |
|        - |  5310 | `	}` |
|        - |  5311 | `#endif` |
|        - |  5312 | `	/* Perform the addition */` |
|     1006 |  5313 | `	nIdx = pTos->nIdx;` |
|     1006 |  5314 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  5315 | `	/* Peform the store operation */` |
|     1006 |  5316 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  5317 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1006 |  5318 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1006 |  5319 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1006 |  5320 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  5321 | `	}` |
|        - |  5322 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1006 |  5323 | `	PH7_MemObjStore(pTos,pNos);` |
|     1006 |  5324 | `	VmPopOperand(&pTos,1);` |
|     1006 |  5325 | `	break;` |
|        - |  5326 | `				}` |
|        - |  5327 | `/* OP_SUB * * *` |
|        - |  5328 | ` *` |
|        - |  5329 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5330 | ` * first (what was next on the stack) from the second (the` |
|        - |  5331 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5332 | ` */` |
|      302 |  5333 | `case PH7_OP_SUB: {` |
|      606 |  5334 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5335 | `#ifdef UNTRUST` |
|        - |  5336 | `	if( pNos < pStack ){` |
|        - |  5337 | `		goto Abort;` |
|        - |  5338 | `	}` |
|        - |  5339 | `#endif` |
|      606 |  5340 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5341 | `		/* Floating point arithemic */` |
|        - |  5342 | `		ph7_real a,b,r;` |
|       95 |  5343 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5344 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5345 | `		}` |
|       95 |  5346 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5347 | `			PH7_MemObjToReal(pNos);` |
|        2 |  5348 | `		}` |
|       95 |  5349 | `		a = pNos->rVal;` |
|       95 |  5350 | `		b = pTos->rVal;` |
|       95 |  5351 | `		r = a - b;` |
|        - |  5352 | `		/* Push the result */` |
|       95 |  5353 | `		pNos->rVal = r;` |
|       95 |  5354 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5355 | `		/* Try to get an integer representation */` |
|       95 |  5356 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  5357 | `	}else{` |
|        - |  5358 | `		/* Integer arithmetic */` |
|        - |  5359 | `		sxi64 a,b,r;` |
|      512 |  5360 | `		a = pNos->x.iVal;` |
|      512 |  5361 | `		b = pTos->x.iVal;` |
|      512 |  5362 | `		r = a - b;` |
|        - |  5363 | `		/* Push the result */` |
|      512 |  5364 | `		pNos->x.iVal = r;` |
|      512 |  5365 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5366 | `	}` |
|      606 |  5367 | `	VmPopOperand(&pTos,1);` |
|      606 |  5368 | `	break;` |
|        - |  5369 | `				 }` |
|        - |  5370 | `/* OP_SUB_STORE * * *` |
|        - |  5371 | ` *` |
|        - |  5372 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5373 | ` * first (what was next on the stack) from the second (the` |
|        - |  5374 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5375 | ` */` |
|        4 |  5376 | `case PH7_OP_SUB_STORE: {` |
|       10 |  5377 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5378 | `	ph7_value *pObj;` |
|        - |  5379 | `#ifdef UNTRUST` |
|        - |  5380 | `	if( pNos < pStack ){` |
|        - |  5381 | `		goto Abort;` |
|        - |  5382 | `	}` |
|        - |  5383 | `#endif` |
|       10 |  5384 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5385 | `		/* Floating point arithemic */` |
|        - |  5386 | `		ph7_real a,b,r;` |
|      ! 0 |  5387 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5388 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5389 | `		}` |
|      ! 0 |  5390 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5391 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  5392 | `		}` |
|      ! 0 |  5393 | `		a = pTos->rVal;` |
|      ! 0 |  5394 | `		b = pNos->rVal;` |
|      ! 0 |  5395 | `		r = a - b;` |
|        - |  5396 | `		/* Push the result */` |
|      ! 0 |  5397 | `		pNos->rVal = r;` |
|      ! 0 |  5398 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5399 | `		/* Try to get an integer representation */` |
|      ! 0 |  5400 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  5401 | `	}else{` |
|        - |  5402 | `		/* Integer arithmetic */` |
|        - |  5403 | `		sxi64 a,b,r;` |
|       10 |  5404 | `		a = pTos->x.iVal;` |
|       10 |  5405 | `		b = pNos->x.iVal;` |
|       10 |  5406 | `		r = a - b;` |
|        - |  5407 | `		/* Push the result */` |
|       10 |  5408 | `		pNos->x.iVal = r;` |
|       10 |  5409 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5410 | `	}` |
|       10 |  5411 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5412 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  5413 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  5414 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  5415 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  5416 | `	}` |
|       10 |  5417 | `	VmPopOperand(&pTos,1);` |
|       10 |  5418 | `	break;` |
|        - |  5419 | `				 }` |
|        - |  5420 |  |
|        - |  5421 | `/*` |
|        - |  5422 | ` * OP_MOD * * *` |
|        - |  5423 | ` *` |
|        - |  5424 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5425 | ` * first (what was next on the stack) from the second (the` |
|        - |  5426 | ` * top of the stack) and push the remainder after division` |
|        - |  5427 | ` * onto the stack.` |
|        - |  5428 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  5429 | ` */` |
|      308 |  5430 | `case PH7_OP_MOD:{` |
|      618 |  5431 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5432 | `	sxi64 a,b,r;` |
|        - |  5433 | `#ifdef UNTRUST` |
|        - |  5434 | `	if( pNos < pStack ){` |
|        - |  5435 | `		goto Abort;` |
|        - |  5436 | `	}` |
|        - |  5437 | `#endif` |
|        - |  5438 | `	/* Force the operands to be integer */` |
|      618 |  5439 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5440 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5441 | `	}` |
|      618 |  5442 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  5443 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  5444 | `	}` |
|        - |  5445 | `	/* Perform the requested operation */` |
|      618 |  5446 | `	a = pNos->x.iVal;` |
|      618 |  5447 | `	b = pTos->x.iVal;` |
|      618 |  5448 | `	if( b == 0 ){` |
|        3 |  5449 | `		r = 0;` |
|        3 |  5450 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  5451 | `		/* goto Abort; */` |
|        2 |  5452 | `	}else{` |
|      615 |  5453 | `		r = a%b;` |
|        - |  5454 | `	}` |
|        - |  5455 | `	/* Push the result */` |
|      618 |  5456 | `	pNos->x.iVal = r;` |
|      618 |  5457 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      618 |  5458 | `	VmPopOperand(&pTos,1);` |
|      618 |  5459 | `	break;` |
|        - |  5460 | `				}` |
|        - |  5461 | `/*` |
|        - |  5462 | ` * OP_MOD_STORE * * *` |
|        - |  5463 | ` *` |
|        - |  5464 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5465 | ` * first (what was next on the stack) from the second (the` |
|        - |  5466 | ` * top of the stack) and push the remainder after division` |
|        - |  5467 | ` * onto the stack.` |
|        - |  5468 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  5469 | ` */` |
|        1 |  5470 | `case PH7_OP_MOD_STORE: {` |
|        3 |  5471 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5472 | `	ph7_value *pObj;` |
|        - |  5473 | `	sxi64 a,b,r;` |
|        - |  5474 | `#ifdef UNTRUST` |
|        - |  5475 | `	if( pNos < pStack ){` |
|        - |  5476 | `		goto Abort;` |
|        - |  5477 | `	}` |
|        - |  5478 | `#endif` |
|        - |  5479 | `	/* Force the operands to be integer */` |
|        3 |  5480 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5481 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5482 | `	}` |
|        3 |  5483 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5484 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5485 | `	}` |
|        - |  5486 | `	/* Perform the requested operation */` |
|        3 |  5487 | `	a = pTos->x.iVal;` |
|        3 |  5488 | `	b = pNos->x.iVal;` |
|        3 |  5489 | `	if( b == 0 ){` |
|      ! 0 |  5490 | `		r = 0;` |
|      ! 0 |  5491 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  5492 | `		/* goto Abort; */` |
|      ! 0 |  5493 | `	}else{` |
|        3 |  5494 | `		r = a%b;` |
|        - |  5495 | `	}` |
|        - |  5496 | `	/* Push the result */` |
|        3 |  5497 | `	pNos->x.iVal = r;` |
|        3 |  5498 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  5499 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5500 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  5501 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  5502 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  5503 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  5504 | `	}` |
|        3 |  5505 | `	VmPopOperand(&pTos,1);` |
|        3 |  5506 | `	break;` |
|        - |  5507 | `				}` |
|        - |  5508 | `/*` |
|        - |  5509 | ` * OP_DIV * * *` |
|        - |  5510 | ` *` |
|        - |  5511 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5512 | ` * first (what was next on the stack) from the second (the` |
|        - |  5513 | ` * top of the stack) and push the result onto the stack.` |
|        - |  5514 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  5515 | ` */` |
|       31 |  5516 | `case PH7_OP_DIV:{` |
|       64 |  5517 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5518 | `	ph7_real a,b,r;` |
|        - |  5519 | `#ifdef UNTRUST` |
|        - |  5520 | `	if( pNos < pStack ){` |
|        - |  5521 | `		goto Abort;` |
|        - |  5522 | `	}` |
|        - |  5523 | `#endif` |
|        - |  5524 | `	/* Force the operands to be real */` |
|       64 |  5525 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       60 |  5526 | `		PH7_MemObjToReal(pTos);` |
|       29 |  5527 | `	}` |
|       64 |  5528 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       26 |  5529 | `		PH7_MemObjToReal(pNos);` |
|       12 |  5530 | `	}` |
|        - |  5531 | `	/* Perform the requested operation */` |
|       64 |  5532 | `	a = pNos->rVal;` |
|       64 |  5533 | `	b = pTos->rVal;` |
|       64 |  5534 | `	if( b == 0 ){` |
|        - |  5535 | `		/* Division by zero */` |
|        3 |  5536 | `		pNos->rVal = 0;` |
|        3 |  5537 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  5538 | `		/* goto Abort; */` |
|        2 |  5539 | `	}else{` |
|       61 |  5540 | `		r = a/b;` |
|        - |  5541 | `		/* Push the result */` |
|       61 |  5542 | `		pNos->rVal = r;` |
|       61 |  5543 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5544 | `		/* Try to get an integer representation */` |
|       61 |  5545 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  5546 | `	}` |
|       64 |  5547 | `	VmPopOperand(&pTos,1);` |
|       64 |  5548 | `	break;` |
|        - |  5549 | `				}` |
|        - |  5550 | `/*` |
|        - |  5551 | ` * OP_DIV_STORE * * *` |
|        - |  5552 | ` *` |
|        - |  5553 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5554 | ` * first (what was next on the stack) from the second (the` |
|        - |  5555 | ` * top of the stack) and push the result onto the stack.` |
|        - |  5556 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  5557 | ` */` |
|        2 |  5558 | `case PH7_OP_DIV_STORE:{` |
|        5 |  5559 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5560 | `	ph7_value *pObj;` |
|        - |  5561 | `	ph7_real a,b,r;` |
|        - |  5562 | `#ifdef UNTRUST` |
|        - |  5563 | `	if( pNos < pStack ){` |
|        - |  5564 | `		goto Abort;` |
|        - |  5565 | `	}` |
|        - |  5566 | `#endif` |
|        - |  5567 | `	/* Force the operands to be real */` |
|        5 |  5568 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5569 | `		PH7_MemObjToReal(pTos);` |
|        2 |  5570 | `	}` |
|        5 |  5571 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5572 | `		PH7_MemObjToReal(pNos);` |
|        2 |  5573 | `	}` |
|        - |  5574 | `	/* Perform the requested operation */` |
|        5 |  5575 | `	a = pTos->rVal;` |
|        5 |  5576 | `	b = pNos->rVal;` |
|        5 |  5577 | `	if( b == 0 ){` |
|        - |  5578 | `		/* Division by zero */` |
|      ! 0 |  5579 | `		r = 0;` |
|      ! 0 |  5580 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  5581 | `		/* goto Abort; */` |
|      ! 0 |  5582 | `	}else{` |
|        5 |  5583 | `		r = a/b;` |
|        - |  5584 | `		/* Push the result */` |
|        5 |  5585 | `		pNos->rVal = r;` |
|        5 |  5586 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5587 | `		/* Try to get an integer representation */` |
|        5 |  5588 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  5589 | `	}` |
|        5 |  5590 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5591 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  5592 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  5593 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  5594 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  5595 | `	}` |
|        5 |  5596 | `	VmPopOperand(&pTos,1);` |
|        5 |  5597 | `	break;` |
|        - |  5598 | `				}` |
|        - |  5599 | `/* OP_BAND * * *` |
|        - |  5600 | ` *` |
|        - |  5601 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5602 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  5603 | ` * two elements.` |
|        - |  5604 | `*/` |
|        - |  5605 | `/* OP_BOR * * *` |
|        - |  5606 | ` *` |
|        - |  5607 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5608 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  5609 | ` * two elements.` |
|        - |  5610 | ` */` |
|        - |  5611 | `/* OP_BXOR * * *` |
|        - |  5612 | ` *` |
|        - |  5613 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5614 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  5615 | ` * two elements.` |
|        - |  5616 | ` */` |
|       44 |  5617 | `case PH7_OP_BAND:` |
|        - |  5618 | `case PH7_OP_BOR:` |
|        - |  5619 | `case PH7_OP_BXOR:{` |
|       90 |  5620 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5621 | `	sxi64 a,b,r;` |
|        - |  5622 | `#ifdef UNTRUST` |
|        - |  5623 | `	if( pNos < pStack ){` |
|        - |  5624 | `		goto Abort;` |
|        - |  5625 | `	}` |
|        - |  5626 | `#endif` |
|        - |  5627 | `	/* Force the operands to be integer */` |
|       90 |  5628 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5629 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5630 | `	}` |
|       90 |  5631 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5632 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5633 | `	}` |
|        - |  5634 | `	/* Perform the requested operation */` |
|       90 |  5635 | `	a = pNos->x.iVal;` |
|       90 |  5636 | `	b = pTos->x.iVal;` |
|       90 |  5637 | `	switch(pInstr->iOp){` |
|        7 |  5638 | `	case PH7_OP_BOR_STORE:` |
|       15 |  5639 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  5640 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  5641 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       30 |  5642 | `	case PH7_OP_BAND_STORE:` |
|       30 |  5643 | `	case PH7_OP_BAND:` |
|       62 |  5644 | `	default:          r = a&b; break;` |
|        - |  5645 | `	}` |
|        - |  5646 | `	/* Push the result */` |
|       90 |  5647 | `	pNos->x.iVal = r;` |
|       90 |  5648 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       90 |  5649 | `	VmPopOperand(&pTos,1);` |
|       90 |  5650 | `	break;` |
|        - |  5651 | `				 }` |
|        - |  5652 | `/* OP_BAND_STORE * * *` |
|        - |  5653 | ` *` |
|        - |  5654 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5655 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  5656 | ` * two elements.` |
|        - |  5657 | `*/` |
|        - |  5658 | `/* OP_BOR_STORE * * *` |
|        - |  5659 | ` *` |
|        - |  5660 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5661 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  5662 | ` * two elements.` |
|        - |  5663 | ` */` |
|        - |  5664 | `/* OP_BXOR_STORE * * *` |
|        - |  5665 | ` *` |
|        - |  5666 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5667 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  5668 | ` * two elements.` |
|        - |  5669 | ` */` |
|       10 |  5670 | `case PH7_OP_BAND_STORE:` |
|        - |  5671 | `case PH7_OP_BOR_STORE:` |
|        - |  5672 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  5673 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5674 | `	ph7_value *pObj;` |
|        - |  5675 | `	sxi64 a,b,r;` |
|        - |  5676 | `#ifdef UNTRUST` |
|        - |  5677 | `	if( pNos < pStack ){` |
|        - |  5678 | `		goto Abort;` |
|        - |  5679 | `	}` |
|        - |  5680 | `#endif` |
|        - |  5681 | `	/* Force the operands to be integer */` |
|       21 |  5682 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5683 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5684 | `	}` |
|       21 |  5685 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5686 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5687 | `	}` |
|        - |  5688 | `	/* Perform the requested operation */` |
|       21 |  5689 | `	a = pTos->x.iVal;` |
|       21 |  5690 | `	b = pNos->x.iVal;` |
|       21 |  5691 | `	switch(pInstr->iOp){` |
|        3 |  5692 | `	case PH7_OP_BOR_STORE:` |
|        7 |  5693 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  5694 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  5695 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  5696 | `	case PH7_OP_BAND_STORE:` |
|        3 |  5697 | `	case PH7_OP_BAND:` |
|        7 |  5698 | `	default:          r = a&b; break;` |
|        - |  5699 | `	}` |
|        - |  5700 | `	/* Push the result */` |
|       21 |  5701 | `	pNos->x.iVal = r;` |
|       21 |  5702 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  5703 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5704 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  5705 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  5706 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  5707 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  5708 | `	}` |
|       21 |  5709 | `	VmPopOperand(&pTos,1);` |
|       21 |  5710 | `	break;` |
|        - |  5711 | `				 }` |
|        - |  5712 | `/* OP_SHL * * *` |
|        - |  5713 | ` *` |
|        - |  5714 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5715 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5716 | ` * left by N bits where N is the top element on the stack.` |
|        - |  5717 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5718 | ` */` |
|        - |  5719 | `/* OP_SHR * * *` |
|        - |  5720 | ` *` |
|        - |  5721 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5722 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5723 | ` * right by N bits where N is the top element on the stack.` |
|        - |  5724 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5725 | ` */` |
|       12 |  5726 | `case PH7_OP_SHL:` |
|        - |  5727 | `case PH7_OP_SHR: {` |
|       25 |  5728 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5729 | `	sxi64 a,r;` |
|        - |  5730 | `	sxi32 b;` |
|        - |  5731 | `#ifdef UNTRUST` |
|        - |  5732 | `	if( pNos < pStack ){` |
|        - |  5733 | `		goto Abort;` |
|        - |  5734 | `	}` |
|        - |  5735 | `#endif` |
|        - |  5736 | `	/* Force the operands to be integer */` |
|       25 |  5737 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5738 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5739 | `	}` |
|       25 |  5740 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5741 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5742 | `	}` |
|        - |  5743 | `	/* Perform the requested operation */` |
|       25 |  5744 | `	a = pNos->x.iVal;` |
|       25 |  5745 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  5746 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  5747 | `		r = a << b;` |
|        8 |  5748 | `	}else{` |
|       11 |  5749 | `		r = a >> b;` |
|        - |  5750 | `	}` |
|        - |  5751 | `	/* Push the result */` |
|       25 |  5752 | `	pNos->x.iVal = r;` |
|       25 |  5753 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  5754 | `	VmPopOperand(&pTos,1);` |
|       25 |  5755 | `	break;` |
|        - |  5756 | `				 }` |
|        - |  5757 | `/*  OP_SHL_STORE * * *` |
|        - |  5758 | ` *` |
|        - |  5759 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5760 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5761 | ` * left by N bits where N is the top element on the stack.` |
|        - |  5762 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5763 | ` */` |
|        - |  5764 | `/* OP_SHR_STORE * * *` |
|        - |  5765 | ` *` |
|        - |  5766 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5767 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5768 | ` * right by N bits where N is the top element on the stack.` |
|        - |  5769 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5770 | ` */` |
|        9 |  5771 | `case PH7_OP_SHL_STORE:` |
|        - |  5772 | `case PH7_OP_SHR_STORE: {` |
|       19 |  5773 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5774 | `	ph7_value *pObj;` |
|        - |  5775 | `	sxi64 a,r;` |
|        - |  5776 | `	sxi32 b;` |
|        - |  5777 | `#ifdef UNTRUST` |
|        - |  5778 | `	if( pNos < pStack ){` |
|        - |  5779 | `		goto Abort;` |
|        - |  5780 | `	}` |
|        - |  5781 | `#endif` |
|        - |  5782 | `	/* Force the operands to be integer */` |
|       19 |  5783 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5784 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5785 | `	}` |
|       19 |  5786 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5787 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5788 | `	}` |
|        - |  5789 | `	/* Perform the requested operation */` |
|       19 |  5790 | `	a = pTos->x.iVal;` |
|       19 |  5791 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  5792 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  5793 | `		r = a << b;` |
|        5 |  5794 | `	}else{` |
|       11 |  5795 | `		r = a >> b;` |
|        - |  5796 | `	}` |
|        - |  5797 | `	/* Push the result */` |
|       19 |  5798 | `	pNos->x.iVal = r;` |
|       19 |  5799 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  5800 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5801 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  5802 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  5803 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  5804 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  5805 | `	}` |
|       19 |  5806 | `	VmPopOperand(&pTos,1);` |
|       19 |  5807 | `	break;` |
|        - |  5808 | `				 }` |
|        - |  5809 | `/* CAT:  P1 * *` |
|        - |  5810 | ` *` |
|        - |  5811 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  5812 | ` * back.` |
|        - |  5813 | ` */` |
|    68556 |  5814 | `case PH7_OP_CAT:{` |
|        - |  5815 | `	ph7_value *pNos,*pCur;` |
|   137114 |  5816 | `	if( pInstr->iP1 < 1 ){` |
|   109836 |  5817 | `		pNos = &pTos[-1];` |
|    54919 |  5818 | `	}else{` |
|    27280 |  5819 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  5820 | `	}` |
|        - |  5821 | `#ifdef UNTRUST` |
|        - |  5822 | `	if( pNos < pStack ){` |
|        - |  5823 | `		goto Abort;` |
|        - |  5824 | `	}` |
|        - |  5825 | `#endif` |
|        - |  5826 | `	/* Force a string cast */` |
|   137114 |  5827 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1636 |  5828 | `		PH7_MemObjToString(pNos);` |
|      817 |  5829 | `	}` |
|   137114 |  5830 | `	pCur = &pNos[1];` |
|   276768 |  5831 | `	while( pCur <= pTos ){` |
|   139656 |  5832 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50878 |  5833 | `			PH7_MemObjToString(pCur);` |
|    25438 |  5834 | `		}` |
|        - |  5835 | `		/* Perform the concatenation */` |
|   139656 |  5836 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   139614 |  5837 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    69806 |  5838 | `		}` |
|   139656 |  5839 | `		SyBlobRelease(&pCur->sBlob);` |
|   139656 |  5840 | `		pCur++;` |
|        2 |  5841 | `	}` |
|   137114 |  5842 | `	pTos = pNos;` |
|   137114 |  5843 | `	break;` |
|        - |  5844 | `				}` |
|        - |  5845 | `/*  CAT_STORE: * * *` |
|        - |  5846 | ` *` |
|        - |  5847 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  5848 | ` * back.` |
|        - |  5849 | ` */` |
|     3808 |  5850 | `case PH7_OP_CAT_STORE:{` |
|     7618 |  5851 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5852 | `	ph7_value *pObj;` |
|        - |  5853 | `#ifdef UNTRUST` |
|        - |  5854 | `	if( pNos < pStack ){` |
|        - |  5855 | `		goto Abort;` |
|        - |  5856 | `	}` |
|        - |  5857 | `#endif` |
|     7618 |  5858 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5859 | `		/* Force a string cast */` |
|        3 |  5860 | `		PH7_MemObjToString(pTos);` |
|        1 |  5861 | `	}` |
|     7618 |  5862 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5863 | `		/* Force a string cast */` |
|      ! 0 |  5864 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5865 | `	}` |
|        - |  5866 | `	/* Perform the concatenation (Reverse order) */` |
|     7618 |  5867 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     7618 |  5868 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3808 |  5869 | `	}` |
|        - |  5870 | `	/* Perform the store operation */` |
|     7618 |  5871 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5872 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     7618 |  5873 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     7618 |  5874 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|     7616 |  5875 | `		PH7_MemObjStore(pTos,pObj);` |
|     3807 |  5876 | `	}` |
|     7616 |  5877 | `	PH7_MemObjStore(pTos,pNos);` |
|     7616 |  5878 | `	VmPopOperand(&pTos,1);` |
|     7616 |  5879 | `	break;` |
|        - |  5880 | `				}` |
|        - |  5881 | `/* OP_AND: * * *` |
|        - |  5882 | ` *` |
|        - |  5883 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  5884 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5885 | ` * stack.` |
|        - |  5886 | ` */` |
|        - |  5887 | `/* OP_OR: * * *` |
|        - |  5888 | ` *` |
|        - |  5889 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  5890 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5891 | ` * stack.` |
|        - |  5892 | ` */` |
|   104102 |  5893 | `case PH7_OP_LAND:` |
|        - |  5894 | `case PH7_OP_LOR: {` |
|   208250 |  5895 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5896 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  5897 | `#ifdef UNTRUST` |
|        - |  5898 | `	if( pNos < pStack ){` |
|        - |  5899 | `		goto Abort;` |
|        - |  5900 | `	}` |
|        - |  5901 | `#endif` |
|        - |  5902 | `	/* Force a boolean cast */` |
|   208250 |  5903 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  5904 | `		PH7_MemObjToBool(pTos);` |
|        1 |  5905 | `	}` |
|   208250 |  5906 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5907 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  5908 | `	}` |
|   208250 |  5909 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   208250 |  5910 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   208250 |  5911 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  5912 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    94662 |  5913 | `		v1 = and_logic[v1*3+v2];` |
|    47354 |  5914 | `	}else{` |
|        - |  5915 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   113590 |  5916 | `		v1 = or_logic[v1*3+v2];` |
|        - |  5917 | `	}` |
|   208250 |  5918 | `	if( v1 == 2 ){` |
|      ! 0 |  5919 | `		v1 = 1;` |
|      ! 0 |  5920 | `	}` |
|   208250 |  5921 | `	VmPopOperand(&pTos,1);` |
|   208250 |  5922 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   208250 |  5923 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   208250 |  5924 | `	break;` |
|        - |  5925 | `				 }` |
|        - |  5926 | `/*` |
|        - |  5927 | ` * OP_NULLC: * * *` |
|        - |  5928 | ` * Null coalescing operator '??'.` |
|        - |  5929 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  5930 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  5931 | ` */` |
|        - |  5932 | `/*` |
|        - |  5933 | ` * OP_NULLC: * P2 *` |
|        - |  5934 | ` * Short-circuit null coalescing '??'.` |
|        - |  5935 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  5936 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  5937 | ` */` |
|       52 |  5938 | `case PH7_OP_NULLC: {` |
|        - |  5939 | `#ifdef UNTRUST` |
|        - |  5940 | `	if( pTos < pStack ){` |
|        - |  5941 | `		goto Abort;` |
|        - |  5942 | `	}` |
|        - |  5943 | `#endif` |
|      106 |  5944 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  5945 | `		/* Left is not null — keep it and skip the RHS */` |
|       42 |  5946 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       22 |  5947 | `	}else{` |
|        - |  5948 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       66 |  5949 | `		VmPopOperand(&pTos, 1);` |
|        - |  5950 | `	}` |
|      106 |  5951 | `	break;` |
|        - |  5952 |  |
|        - |  5953 | `/*` |
|        - |  5954 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  5955 | ` * Null coalescing assignment short-circuit.` |
|        - |  5956 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  5957 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  5958 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  5959 | ` */` |
|       23 |  5960 | `case PH7_OP_NULLC_JMP: {` |
|        - |  5961 | `#ifdef UNTRUST` |
|        - |  5962 | `	if( pTos < pStack ){` |
|        - |  5963 | `		goto Abort;` |
|        - |  5964 | `	}` |
|        - |  5965 | `#endif` |
|       47 |  5966 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       19 |  5967 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|        9 |  5968 | `	}` |
|       47 |  5969 | `	break;` |
|        - |  5970 |  |
|        - |  5971 | `/*` |
|        - |  5972 | ` * OP_NULLC_STORE: * * *` |
|        - |  5973 | ` * Null coalescing assignment store.` |
|        - |  5974 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  5975 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  5976 | ` * expression result.` |
|        - |  5977 | ` */` |
|        - |  5978 | `/*` |
|        - |  5979 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  5980 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  5981 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  5982 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  5983 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  5984 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  5985 | ` */` |
|       51 |  5986 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  5987 | `#ifdef UNTRUST` |
|        - |  5988 | `	if( pTos < pStack ){` |
|        - |  5989 | `		goto Abort;` |
|        - |  5990 | `	}` |
|        - |  5991 | `#endif` |
|      104 |  5992 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  5993 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  5994 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  5995 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  5996 | `	}` |
|      104 |  5997 | `	break;` |
|        - |  5998 |  |
|       14 |  5999 | `case PH7_OP_NULLC_STORE: {` |
|       29 |  6000 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6001 | `	ph7_value *pObj;` |
|        - |  6002 | `	sxu32 nIdx;` |
|        - |  6003 | `#ifdef UNTRUST` |
|        - |  6004 | `	if( pNos < pStack ){` |
|        - |  6005 | `		goto Abort;` |
|        - |  6006 | `	}` |
|        - |  6007 | `#endif` |
|       29 |  6008 | `	nIdx = pNos->nIdx;` |
|       29 |  6009 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  6010 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6011 | `			"Cannot perform assignment on a constant class attribute");` |
|       29 |  6012 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       29 |  6013 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       29 |  6014 | `		PH7_MemObjStore(pTos,pObj);` |
|       14 |  6015 | `	}` |
|       29 |  6016 | `	PH7_MemObjStore(pTos,pNos);` |
|       29 |  6017 | `	VmPopOperand(&pTos,1);` |
|       29 |  6018 | `	break;` |
|        - |  6019 |  |
|        - |  6020 | `/*` |
|        - |  6021 | ` * OP_SPREAD: * * *` |
|        - |  6022 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  6023 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  6024 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  6025 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  6026 | ` */` |
|        9 |  6027 | `case PH7_OP_SPREAD: {` |
|        - |  6028 | `#ifdef UNTRUST` |
|        - |  6029 | `	if( pTos < pStack ){` |
|        - |  6030 | `		goto Abort;` |
|        - |  6031 | `	}` |
|        - |  6032 | `#endif` |
|       20 |  6033 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  6034 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  6035 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  6036 | `		if( nEntry == 0 ){` |
|        - |  6037 | `			/* Empty array — remove from stack */` |
|        3 |  6038 | `			VmPopOperand(&pTos, 1);` |
|        3 |  6039 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  6040 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  6041 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  6042 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  6043 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  6044 | `				VM_STACK_GUARD);` |
|      ! 0 |  6045 | `		}else{` |
|        - |  6046 | `			ph7_hashmap_node *pNode2;` |
|        - |  6047 | `			ph7_value *pElem;` |
|        - |  6048 | `			sxu32 i;` |
|        - |  6049 | `			/* Overwrite TOS with first element */` |
|       18 |  6050 | `			pNode2 = pMap->pFirst;` |
|       18 |  6051 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  6052 | `			PH7_MemObjRelease(pTos);` |
|       18 |  6053 | `			if( pElem ){` |
|       18 |  6054 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  6055 | `			}` |
|       18 |  6056 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  6057 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  6058 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  6059 | `			pNode2 = pNode2->pPrev;` |
|        - |  6060 | `			/* Push remaining elements */` |
|       44 |  6061 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  6062 | `				pTos++;` |
|       28 |  6063 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  6064 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  6065 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  6066 | `				if( pElem ){` |
|       28 |  6067 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  6068 | `				}` |
|       28 |  6069 | `				pNode2 = pNode2->pPrev;` |
|       15 |  6070 | `			}` |
|       18 |  6071 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  6072 | `		}` |
|        9 |  6073 | `	}` |
|        - |  6074 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  6075 | `	break;` |
|        - |  6076 |  |
|        - |  6077 | `/* OP_LXOR: * * *` |
|        - |  6078 | ` *` |
|        - |  6079 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  6080 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6081 | ` * stack.` |
|        - |  6082 | ` * According to the PHP language reference manual:` |
|        - |  6083 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  6084 | ` *  TRUE,but not both.` |
|        - |  6085 | ` */` |
|        5 |  6086 | `case PH7_OP_LXOR:{` |
|       11 |  6087 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  6088 | `	sxi32 v = 0;` |
|        - |  6089 | `#ifdef UNTRUST` |
|        - |  6090 | `	if( pNos < pStack ){` |
|        - |  6091 | `		goto Abort;` |
|        - |  6092 | `	}` |
|        - |  6093 | `#endif` |
|        - |  6094 | `	/* Force a boolean cast */` |
|       11 |  6095 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6096 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  6097 | `	}` |
|       11 |  6098 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6099 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6100 | `	}` |
|       11 |  6101 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  6102 | `		v = 1;` |
|        3 |  6103 | `	}` |
|       11 |  6104 | `	VmPopOperand(&pTos,1);` |
|       11 |  6105 | `	pTos->x.iVal = v;` |
|       11 |  6106 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  6107 | `	break;` |
|        - |  6108 | `				 }` |
|        - |  6109 | `/* OP_EQ P1 P2 P3` |
|        - |  6110 | ` *` |
|        - |  6111 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  6112 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6113 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6114 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6115 | ` */` |
|        - |  6116 | `/* OP_NEQ P1 P2 P3` |
|        - |  6117 | ` *` |
|        - |  6118 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  6119 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6120 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6121 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6122 | ` */` |
|     4332 |  6123 | `case PH7_OP_EQ:` |
|        - |  6124 | `case PH7_OP_NEQ: {` |
|     8666 |  6125 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6126 | `	/* Perform the comparison and act accordingly */` |
|        - |  6127 | `#ifdef UNTRUST` |
|        - |  6128 | `	if( pNos < pStack ){` |
|        - |  6129 | `		goto Abort;` |
|        - |  6130 | `	}` |
|        - |  6131 | `#endif` |
|     8666 |  6132 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     8666 |  6133 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  6134 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     8657 |  6135 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     8622 |  6136 | `		rc = rc == 0;` |
|     4312 |  6137 | `	}else{` |
|       28 |  6138 | `		rc = rc != 0;` |
|        - |  6139 | `	}` |
|     8666 |  6140 | `	VmPopOperand(&pTos,1);` |
|     8666 |  6141 | `	if( !pInstr->iP2 ){` |
|        - |  6142 | `		/* Push comparison result without taking the jump */` |
|     8666 |  6143 | `		PH7_MemObjRelease(pTos);` |
|     8666 |  6144 | `		pTos->x.iVal = rc;` |
|        - |  6145 | `		/* Invalidate any prior representation */` |
|     8666 |  6146 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4334 |  6147 | `	}else{` |
|      ! 0 |  6148 | `		if( rc ){` |
|        - |  6149 | `			/* Jump to the desired location */` |
|      ! 0 |  6150 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6151 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6152 | `		}` |
|        - |  6153 | `	}` |
|     8666 |  6154 | `	break;` |
|        - |  6155 | `				 }` |
|        - |  6156 | `/* OP_TEQ P1 P2 *` |
|        - |  6157 | ` *` |
|        - |  6158 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  6159 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6160 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6161 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6162 | ` */` |
|   152376 |  6163 | `case PH7_OP_TEQ: {` |
|   304754 |  6164 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6165 | `	/* Perform the comparison and act accordingly */` |
|        - |  6166 | `#ifdef UNTRUST` |
|        - |  6167 | `	if( pNos < pStack ){` |
|        - |  6168 | `		goto Abort;` |
|        - |  6169 | `	}` |
|        - |  6170 | `#endif` |
|   304754 |  6171 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   304754 |  6172 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6173 | `		rc = 0;` |
|        2 |  6174 | `	}else{` |
|   304752 |  6175 | `		rc = rc == 0;` |
|        - |  6176 | `	}` |
|   304754 |  6177 | `	VmPopOperand(&pTos,1);` |
|   304754 |  6178 | `	if( !pInstr->iP2 ){` |
|        - |  6179 | `		/* Push comparison result without taking the jump */` |
|   304754 |  6180 | `		PH7_MemObjRelease(pTos);` |
|   304754 |  6181 | `		pTos->x.iVal = rc;` |
|        - |  6182 | `		/* Invalidate any prior representation */` |
|   304754 |  6183 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   152378 |  6184 | `	}else{` |
|      ! 0 |  6185 | `		if( rc ){` |
|        - |  6186 | `			/* Jump to the desired location */` |
|      ! 0 |  6187 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6188 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6189 | `		}` |
|        - |  6190 | `	}` |
|   304754 |  6191 | `	break;` |
|        - |  6192 | `				 }` |
|        - |  6193 | `/* OP_TNE P1 P2 *` |
|        - |  6194 | ` *` |
|        - |  6195 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  6196 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  6197 | ` * instruction.` |
|        - |  6198 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6199 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6200 | ` *` |
|        - |  6201 | ` */` |
|   117632 |  6202 | `case PH7_OP_TNE: {` |
|   235266 |  6203 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6204 | `	/* Perform the comparison and act accordingly */` |
|        - |  6205 | `#ifdef UNTRUST` |
|        - |  6206 | `	if( pNos < pStack ){` |
|        - |  6207 | `		goto Abort;` |
|        - |  6208 | `	}` |
|        - |  6209 | `#endif` |
|   235266 |  6210 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   235266 |  6211 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6212 | `		rc = 1;` |
|        2 |  6213 | `	}else{` |
|   235264 |  6214 | `		rc = rc != 0;` |
|        - |  6215 | `	}` |
|   235266 |  6216 | `	VmPopOperand(&pTos,1);` |
|   235266 |  6217 | `	if( !pInstr->iP2 ){` |
|        - |  6218 | `		/* Push comparison result without taking the jump */` |
|   235266 |  6219 | `		PH7_MemObjRelease(pTos);` |
|   235266 |  6220 | `		pTos->x.iVal = rc;` |
|        - |  6221 | `		/* Invalidate any prior representation */` |
|   235266 |  6222 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   117634 |  6223 | `	}else{` |
|      ! 0 |  6224 | `		if( rc ){` |
|        - |  6225 | `			/* Jump to the desired location */` |
|      ! 0 |  6226 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6227 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6228 | `		}` |
|        - |  6229 | `	}` |
|   235266 |  6230 | `	break;` |
|        - |  6231 | `				 }` |
|        - |  6232 | `/* OP_LT P1 P2 P3` |
|        - |  6233 | ` *` |
|        - |  6234 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6235 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6236 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6237 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6238 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6239 | ` *` |
|        - |  6240 | ` */` |
|        - |  6241 | `/* OP_LE P1 P2 P3` |
|        - |  6242 | ` *` |
|        - |  6243 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6244 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6245 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6246 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6247 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6248 | ` *` |
|        - |  6249 | ` */` |
|   109839 |  6250 | `case PH7_OP_LT:` |
|        - |  6251 | `case PH7_OP_LE: {` |
|   219724 |  6252 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6253 | `	/* Perform the comparison and act accordingly */` |
|        - |  6254 | `#ifdef UNTRUST` |
|        - |  6255 | `	if( pNos < pStack ){` |
|        - |  6256 | `		goto Abort;` |
|        - |  6257 | `	}` |
|        - |  6258 | `#endif` |
|   219724 |  6259 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   219724 |  6260 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6261 | `		rc = 0;` |
|   219720 |  6262 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1562 |  6263 | `		rc = rc < 1;` |
|      782 |  6264 | `	}else{` |
|   218156 |  6265 | `		rc = rc < 0;` |
|        - |  6266 | `	}` |
|   219724 |  6267 | `	VmPopOperand(&pTos,1);` |
|   219724 |  6268 | `	if( !pInstr->iP2 ){` |
|        - |  6269 | `		/* Push comparison result without taking the jump */` |
|   219724 |  6270 | `		PH7_MemObjRelease(pTos);` |
|   219724 |  6271 | `		pTos->x.iVal = rc;` |
|        - |  6272 | `		/* Invalidate any prior representation */` |
|   219724 |  6273 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   109885 |  6274 | `	}else{` |
|      ! 0 |  6275 | `		if( rc ){` |
|        - |  6276 | `			/* Jump to the desired location */` |
|      ! 0 |  6277 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6278 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6279 | `		}` |
|        - |  6280 | `	}` |
|   219724 |  6281 | `	break;` |
|        - |  6282 | `				}` |
|        - |  6283 | `/* OP_GT P1 P2 P3` |
|        - |  6284 | ` *` |
|        - |  6285 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6286 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6287 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6288 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6289 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6290 | ` *` |
|        - |  6291 | ` */` |
|        - |  6292 | `/* OP_GE P1 P2 P3` |
|        - |  6293 | ` *` |
|        - |  6294 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6295 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6296 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6297 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6298 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6299 | ` *` |
|        - |  6300 | ` */` |
|    54156 |  6301 | `case PH7_OP_GT:` |
|        - |  6302 | `case PH7_OP_GE: {` |
|   108314 |  6303 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6304 | `	/* Perform the comparison and act accordingly */` |
|        - |  6305 | `#ifdef UNTRUST` |
|        - |  6306 | `	if( pNos < pStack ){` |
|        - |  6307 | `		goto Abort;` |
|        - |  6308 | `	}` |
|        - |  6309 | `#endif` |
|   108314 |  6310 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   108314 |  6311 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6312 | `		rc = 0;` |
|   108310 |  6313 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   108142 |  6314 | `		rc = rc >= 0;` |
|    54072 |  6315 | `	}else{` |
|      166 |  6316 | `		rc = rc > 0;` |
|        - |  6317 | `	}` |
|   108314 |  6318 | `	VmPopOperand(&pTos,1);` |
|   108314 |  6319 | `	if( !pInstr->iP2 ){` |
|        - |  6320 | `		/* Push comparison result without taking the jump */` |
|   108314 |  6321 | `		PH7_MemObjRelease(pTos);` |
|   108314 |  6322 | `		pTos->x.iVal = rc;` |
|        - |  6323 | `		/* Invalidate any prior representation */` |
|   108314 |  6324 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    54158 |  6325 | `	}else{` |
|      ! 0 |  6326 | `		if( rc ){` |
|        - |  6327 | `			/* Jump to the desired location */` |
|      ! 0 |  6328 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6329 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6330 | `		}` |
|        - |  6331 | `	}` |
|   108314 |  6332 | `	break;` |
|        - |  6333 | `				}` |
|        - |  6334 | `/* OP_SPACESHIP * * *` |
|        - |  6335 | ` *` |
|        - |  6336 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  6337 | ` *   -1 if left < right` |
|        - |  6338 | ` *    0 if left == right` |
|        - |  6339 | ` *    1 if left > right` |
|        - |  6340 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  6341 | ` */` |
|       25 |  6342 | `case PH7_OP_SPACESHIP: {` |
|       51 |  6343 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6344 | `#ifdef UNTRUST` |
|        - |  6345 | `	if( pNos < pStack ){` |
|        - |  6346 | `		goto Abort;` |
|        - |  6347 | `	}` |
|        - |  6348 | `#endif` |
|       51 |  6349 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  6350 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  6351 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  6352 | `		rc = 1;` |
|        4 |  6353 | `	}else{` |
|        - |  6354 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  6355 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  6356 | `	}` |
|       51 |  6357 | `	VmPopOperand(&pTos,1);` |
|       51 |  6358 | `	PH7_MemObjRelease(pTos);` |
|       51 |  6359 | `	pTos->x.iVal = rc;` |
|       51 |  6360 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  6361 | `	break;` |
|        - |  6362 | `				}` |
|        - |  6363 | `/* OP_SEQ P1 P2 *` |
|        - |  6364 | ` * Strict string comparison.` |
|        - |  6365 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  6366 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6367 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  6368 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  6369 | ` * use PH7_OP_EQ.` |
|        - |  6370 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6371 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6372 | ` */` |
|        - |  6373 | `/* OP_SNE P1 P2 *` |
|        - |  6374 | ` * Strict string comparison.` |
|        - |  6375 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  6376 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6377 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  6378 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  6379 | ` * use PH7_OP_EQ.` |
|        - |  6380 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6381 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6382 | ` */` |
|       18 |  6383 | `case PH7_OP_SEQ:` |
|        - |  6384 | `case PH7_OP_SNE: {` |
|       38 |  6385 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6386 | `	SyString s1,s2;` |
|        - |  6387 | `	/* Perform the comparison and act accordingly */` |
|        - |  6388 | `#ifdef UNTRUST` |
|        - |  6389 | `	if( pNos < pStack ){` |
|        - |  6390 | `		goto Abort;` |
|        - |  6391 | `	}` |
|        - |  6392 | `#endif` |
|        - |  6393 | `	/* Force a string cast */` |
|       38 |  6394 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  6395 | `		PH7_MemObjToString(pTos);` |
|        2 |  6396 | `	}` |
|       38 |  6397 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  6398 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6399 | `	}` |
|       38 |  6400 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  6401 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  6402 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  6403 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  6404 | `		rc = rc != 0;` |
|      ! 0 |  6405 | `	}else{` |
|       38 |  6406 | `		rc = rc == 0;` |
|        - |  6407 | `	}` |
|       38 |  6408 | `	VmPopOperand(&pTos,1);` |
|       38 |  6409 | `	if( !pInstr->iP2 ){` |
|        - |  6410 | `		/* Push comparison result without taking the jump */` |
|       38 |  6411 | `		PH7_MemObjRelease(pTos);` |
|       38 |  6412 | `		pTos->x.iVal = rc;` |
|        - |  6413 | `		/* Invalidate any prior representation */` |
|       38 |  6414 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  6415 | `	}else{` |
|      ! 0 |  6416 | `		if( rc ){` |
|        - |  6417 | `			/* Jump to the desired location */` |
|      ! 0 |  6418 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6419 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6420 | `		}` |
|        - |  6421 | `	}` |
|       38 |  6422 | `	break;` |
|        - |  6423 | `				 }` |
|        - |  6424 | `/*` |
|        - |  6425 | ` * OP_LOAD_REF * * *` |
|        - |  6426 | ` * Push the index of a referenced object on the stack.` |
|        - |  6427 | ` */` |
|       57 |  6428 | `case PH7_OP_LOAD_REF: {` |
|        - |  6429 | `	sxu32 nIdx;` |
|        - |  6430 | `#ifdef UNTRUST` |
|        - |  6431 | `	if( pTos < pStack ){` |
|        - |  6432 | `		goto Abort;` |
|        - |  6433 | `	}` |
|        - |  6434 | `#endif` |
|        - |  6435 | `	/* Extract memory object index */` |
|      115 |  6436 | `	nIdx = pTos->nIdx;` |
|      115 |  6437 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  6438 | `		/* Nullify the object */` |
|       95 |  6439 | `		PH7_MemObjRelease(pTos);` |
|        - |  6440 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  6441 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  6442 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  6443 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  6444 | `	}` |
|      115 |  6445 | `	break;` |
|        - |  6446 | `					  }` |
|        - |  6447 | `/*` |
|        - |  6448 | ` * OP_STORE_REF * * P3` |
|        - |  6449 | ` * Perform an assignment operation by reference.` |
|        - |  6450 | ` */` |
|       16 |  6451 | ` case PH7_OP_STORE_REF: {` |
|       34 |  6452 | `	 SyString sName = { 0 , 0 };` |
|        - |  6453 | `	 VmFrame *pFrameLocal;` |
|        - |  6454 | `	SyHashEntry *pEntry;` |
|        - |  6455 | `	sxu32 nIdx;` |
|        - |  6456 | `#ifdef UNTRUST` |
|        - |  6457 | `	if( pTos < pStack ){` |
|        - |  6458 | `		goto Abort;` |
|        - |  6459 | `	}` |
|        - |  6460 | `#endif` |
|       34 |  6461 | `	if( pInstr->p3 == 0 ){` |
|        - |  6462 | `		char *zName;` |
|        - |  6463 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  6464 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6465 | `			/* Force a string cast */` |
|      ! 0 |  6466 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6467 | `		}` |
|      ! 0 |  6468 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6469 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  6470 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6471 | `			if( zName ){` |
|      ! 0 |  6472 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6473 | `			}` |
|      ! 0 |  6474 | `		}` |
|      ! 0 |  6475 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6476 | `		pTos--;` |
|      ! 0 |  6477 | `	}else{` |
|       34 |  6478 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  6479 | `	}` |
|       34 |  6480 | `	nIdx = pTos->nIdx;` |
|       34 |  6481 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  6482 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6483 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6484 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  6485 | `		}else{` |
|        - |  6486 | `			ph7_value *pObj;` |
|        - |  6487 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  6488 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  6489 | `			if( pObj == 0 ){` |
|      ! 0 |  6490 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6491 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  6492 | `				goto Abort;` |
|        - |  6493 | `			}` |
|        - |  6494 | `			/* Perform the store operation */` |
|      ! 0 |  6495 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  6496 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  6497 | `		}` |
|       34 |  6498 | `	}else if( sName.nByte > 0){` |
|       34 |  6499 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  6500 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  6501 | `		}else{` |
|       34 |  6502 | `			pFrameLocal = pVm->pFrame;` |
|       34 |  6503 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  6504 | `			/* Query the local frame */` |
|       34 |  6505 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       34 |  6506 | `			if( pEntry ){` |
|      ! 0 |  6507 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  6508 | `			}else{` |
|       34 |  6509 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       34 |  6510 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  6511 | `					/* Insert in the $GLOBALS array */` |
|       30 |  6512 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       14 |  6513 | `				}` |
|       34 |  6514 | `				if( rc == SXRET_OK ){` |
|       34 |  6515 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       16 |  6516 | `				}` |
|        - |  6517 | `			}` |
|        - |  6518 | `		}` |
|       16 |  6519 | `	}` |
|       34 |  6520 | `	break;` |
|        - |  6521 | `				 }` |
|        - |  6522 | `/*` |
|        - |  6523 | ` * OP_UPLINK P1 * *` |
|        - |  6524 | ` * Link a variable to the top active VM frame.` |
|        - |  6525 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  6526 | ` */` |
|       28 |  6527 | `case PH7_OP_UPLINK: {` |
|       58 |  6528 | `	if( pVm->pFrame->pParent ){` |
|       58 |  6529 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  6530 | `		SyString sName;` |
|        - |  6531 | `		/* Perform the link */` |
|      116 |  6532 | `		while( pLink <= pTos ){` |
|       60 |  6533 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6534 | `				/* Force a string cast */` |
|      ! 0 |  6535 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  6536 | `			}` |
|       60 |  6537 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       60 |  6538 | `			if( sName.nByte > 0 ){` |
|       60 |  6539 | `				VmFrameLink(&(*pVm),&sName);` |
|       29 |  6540 | `			}` |
|       60 |  6541 | `			pLink++;` |
|        2 |  6542 | `		}` |
|       28 |  6543 | `	}` |
|       58 |  6544 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       58 |  6545 | `	break;` |
|        - |  6546 | `					}` |
|        - |  6547 | `/*` |
|        - |  6548 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  6549 | ` * Push an exception in the corresponding container so that` |
|        - |  6550 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  6551 | ` */` |
|      110 |  6552 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      222 |  6553 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  6554 | `	VmFrame *pFrameLocal;` |
|        - |  6555 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      222 |  6556 | `	pException->iFinallyDone = 0;` |
|      222 |  6557 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  6558 | `	/* Create the exception frame */` |
|      222 |  6559 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      222 |  6560 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  6561 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  6562 | `		goto Abort;` |
|        - |  6563 | `	}` |
|        - |  6564 | `	/* Mark the special frame */` |
|      222 |  6565 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      222 |  6566 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  6567 | `	/* Point to the frame that trigger the exception */` |
|      222 |  6568 | `	pFrameLocal = pFrameLocal->pParent;` |
|      222 |  6569 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      222 |  6570 | `	pException->pFrame = pFrameLocal;` |
|      222 |  6571 | `	break;` |
|        - |  6572 | `							}` |
|        - |  6573 | `/*` |
|        - |  6574 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  6575 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  6576 | ` */` |
|      109 |  6577 | `case PH7_OP_POP_EXCEPTION: {` |
|      220 |  6578 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      220 |  6579 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  6580 | `		ph7_exception **apException;` |
|        - |  6581 | `		/* Pop the loaded exception */` |
|       32 |  6582 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       32 |  6583 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       30 |  6584 | `			(void)SySetPop(&pVm->aException);` |
|       14 |  6585 | `		}` |
|       15 |  6586 | `	}` |
|      220 |  6587 | `	pException->pFrame = 0;` |
|        - |  6588 | `	/* Leave the exception frame */` |
|      220 |  6589 | `	VmLeaveFrame(&(*pVm));` |
|        - |  6590 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      220 |  6591 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  6592 | `		sxi32 rcFinally;` |
|       20 |  6593 | `		pException->iFinallyDone = 1;` |
|       20 |  6594 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  6595 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  6596 | `			goto Abort;` |
|        - |  6597 | `		}` |
|        9 |  6598 | `	}` |
|      220 |  6599 | `	break;` |
|        - |  6600 | `							}` |
|        - |  6601 |  |
|        - |  6602 | `/*` |
|        - |  6603 | ` * OP_THROW * P2 *` |
|        - |  6604 | ` * Throw an user exception.` |
|        - |  6605 | ` */` |
|       58 |  6606 | `case PH7_OP_THROW: {` |
|      118 |  6607 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      118 |  6608 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  6609 | `#ifdef UNTRUST` |
|        - |  6610 | `	if( pTos < pStack ){` |
|        - |  6611 | `		goto Abort;` |
|        - |  6612 | `	}` |
|        - |  6613 | `#endif` |
|      118 |  6614 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  6615 | `	/* Tell the upper layer that an exception was thrown */` |
|      118 |  6616 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      118 |  6617 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      118 |  6618 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6619 | `		ph7_class *pThrowable;` |
|        - |  6620 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      118 |  6621 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      119 |  6622 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  6623 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  6624 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  6625 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  6626 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  6627 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  6628 | `			if( pErrorClass ){` |
|        3 |  6629 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  6630 | `			}` |
|        3 |  6631 | `			if( pErrInst ){` |
|        - |  6632 | `				ph7_class_method *pCons;` |
|        3 |  6633 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  6634 | `				if( pCons ){` |
|        - |  6635 | `					ph7_value sArg;` |
|        - |  6636 | `					ph7_value *apArg[1];` |
|        - |  6637 | `					SyString sMsgStr;` |
|        - |  6638 | `					static const char zErrMsg[] =` |
|        - |  6639 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  6640 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  6641 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  6642 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  6643 | `					apArg[0] = &sArg;` |
|        3 |  6644 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  6645 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  6646 | `				}` |
|        3 |  6647 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  6648 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  6649 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6650 | `					goto Abort;` |
|        - |  6651 | `				}` |
|        2 |  6652 | `			}else{` |
|        - |  6653 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  6654 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  6655 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6656 | `					goto Abort;` |
|        - |  6657 | `				}` |
|        - |  6658 | `			}` |
|        2 |  6659 | `		}else{` |
|        - |  6660 | `			/* Throw the exception */` |
|      116 |  6661 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      116 |  6662 | `			if( rc == SXERR_ABORT ){` |
|        - |  6663 | `				/* Abort processing immediately */` |
|       11 |  6664 | `				goto Abort;` |
|        - |  6665 | `			}` |
|        - |  6666 | `		}` |
|       55 |  6667 | `	}else{` |
|        - |  6668 | `		/* Expecting a class instance */` |
|      ! 0 |  6669 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  6670 | `		if( rc == SXERR_ABORT ){` |
|        - |  6671 | `			/* Abort processing immediately */` |
|      ! 0 |  6672 | `			goto Abort;` |
|        - |  6673 | `		}` |
|        - |  6674 | `	}` |
|        - |  6675 | `	/* Pop the top entry */` |
|      108 |  6676 | `	VmPopOperand(&pTos,1);` |
|        - |  6677 | `	/* Perform an unconditional jump */` |
|      108 |  6678 | `	pc = nJump - 1;` |
|      108 |  6679 | `	break;` |
|        - |  6680 | `				   }` |
|        - |  6681 | `/*` |
|        - |  6682 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  6683 | ` * Prepare a foreach step.` |
|        - |  6684 | ` */` |
|     5743 |  6685 | `case PH7_OP_FOREACH_INIT: {` |
|    11488 |  6686 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  6687 | `	void *pName;` |
|        - |  6688 | `#ifdef UNTRUST` |
|        - |  6689 | `	if( pTos < pStack ){` |
|        - |  6690 | `		goto Abort;` |
|        - |  6691 | `	}` |
|        - |  6692 | `#endif` |
|    11488 |  6693 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6694 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  6695 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6696 | `			/* Force a string cast */` |
|      ! 0 |  6697 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6698 | `		}` |
|        - |  6699 | `		/* Duplicate name */` |
|      ! 0 |  6700 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6701 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6702 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6703 | `		}` |
|      ! 0 |  6704 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6705 | `	}` |
|    11488 |  6706 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  6707 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6708 | `			/* Force a string cast */` |
|      ! 0 |  6709 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6710 | `		}` |
|        - |  6711 | `		/* Duplicate name */` |
|      ! 0 |  6712 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6713 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6714 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6715 | `		}` |
|      ! 0 |  6716 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6717 | `	}` |
|        - |  6718 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    11488 |  6719 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6720 | `		/* Jump out of the loop */` |
|      ! 0 |  6721 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6722 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  6723 | `		}` |
|      ! 0 |  6724 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  6725 | `	}else{` |
|        - |  6726 | `		ph7_foreach_step *pStep;` |
|    11488 |  6727 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    11488 |  6728 | `		if( pStep == 0 ){` |
|      ! 0 |  6729 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  6730 | `			/* Jump out of the loop */` |
|      ! 0 |  6731 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6732 | `		}else{` |
|        - |  6733 | `			/* Zero the structure */` |
|    11488 |  6734 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  6735 | `			/* Prepare the step */` |
|    11488 |  6736 | `			pStep->iFlags = pInfo->iFlags;` |
|    11488 |  6737 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6738 | `				ph7_hashmap *pMap;` |
|        - |  6739 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  6740 | `				 * source array so mutations don't affect other sharers. */` |
|    11456 |  6741 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  6742 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  6743 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  6744 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6745 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  6746 | `						 * variable still points at the same hashmap as` |
|        - |  6747 | `						 * the stack value. */` |
|        9 |  6748 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  6749 | `							pCur->iRef--;` |
|        9 |  6750 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  6751 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  6752 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  6753 | `						}` |
|        4 |  6754 | `					}` |
|        4 |  6755 | `				}` |
|    11456 |  6756 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6757 | `				/* Reset the internal loop cursor */` |
|    11456 |  6758 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6759 | `				/* Mark the step */` |
|    11456 |  6760 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    11456 |  6761 | `				pStep->xIter.pMap = pMap;` |
|    11456 |  6762 | `				pMap->iRef++;` |
|     5729 |  6763 | `			}else{` |
|       34 |  6764 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6765 | `				ph7_class *pIteratorClass;` |
|        - |  6766 | `				/* Check if the object implements Iterator */` |
|       34 |  6767 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       45 |  6768 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  6769 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  6770 | `					ph7_class_method *pRewind;` |
|       24 |  6771 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  6772 | `					pStep->xIter.pThis = pThis;` |
|       24 |  6773 | `					pThis->iRef++;` |
|       24 |  6774 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  6775 | `					if( pRewind ){` |
|       24 |  6776 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  6777 | `					}` |
|       13 |  6778 | `				}else{` |
|        - |  6779 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  6780 | `					ph7_class *pIterAggClass;` |
|       12 |  6781 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  6782 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  6783 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  6784 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  6785 | `						ph7_class_method *pGetIter;` |
|        3 |  6786 | `						int iterAggOk = 0;` |
|        3 |  6787 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  6788 | `						if( pGetIter ){` |
|        - |  6789 | `							ph7_value sResult;` |
|        3 |  6790 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  6791 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  6792 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  6793 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  6794 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  6795 | `									ph7_class_method *pRewind;` |
|        3 |  6796 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  6797 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  6798 | `									pIterObj->iRef++;` |
|        - |  6799 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  6800 | `									pStep->pOwner = pThis;` |
|        3 |  6801 | `									pThis->iRef++;` |
|        3 |  6802 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  6803 | `									if( pRewind ){` |
|        3 |  6804 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  6805 | `									}` |
|        3 |  6806 | `									iterAggOk = 1;` |
|        1 |  6807 | `								}` |
|        1 |  6808 | `							}` |
|        3 |  6809 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  6810 | `						}` |
|        3 |  6811 | `						if( !iterAggOk ){` |
|        - |  6812 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  6813 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6814 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  6815 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  6816 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  6817 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  6818 | `						}` |
|        2 |  6819 | `					}else{` |
|        - |  6820 | `						/* Plain object iteration via hAttr */` |
|        9 |  6821 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  6822 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  6823 | `						pStep->xIter.pThis = pThis;` |
|        9 |  6824 | `						pThis->iRef++;` |
|        - |  6825 | `					}` |
|        - |  6826 | `				}` |
|        - |  6827 | `			}` |
|        - |  6828 | `		}` |
|    11488 |  6829 | `		if( pStep ){` |
|    11488 |  6830 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  6831 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  6832 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  6833 | `				/* Jump out of the loop */` |
|      ! 0 |  6834 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  6835 | `			}` |
|     5743 |  6836 | `		}` |
|        - |  6837 | `	}` |
|    11488 |  6838 | `	VmPopOperand(&pTos,1);` |
|    11488 |  6839 | `	break;` |
|        - |  6840 | `						  }` |
|        - |  6841 | `/*` |
|        - |  6842 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  6843 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  6844 | ` */` |
|    93869 |  6845 | `case PH7_OP_FOREACH_STEP: {` |
|   187740 |  6846 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  6847 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  6848 | `	ph7_value *pValue;` |
|        - |  6849 | `	VmFrame *pFrameLocal;` |
|        - |  6850 | `	/* Peek the last step */` |
|   187740 |  6851 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   187740 |  6852 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   187740 |  6853 | `	pFrameLocal = pVm->pFrame;` |
|   187740 |  6854 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   187740 |  6855 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   187612 |  6856 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  6857 | `		ph7_hashmap_node *pNode;` |
|        - |  6858 | `		/* Extract the current node value */` |
|   187612 |  6859 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   187612 |  6860 | `		if( pNode == 0 ){` |
|        - |  6861 | `			/* No more entry to process */` |
|    11454 |  6862 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    11454 |  6863 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6864 | `				/* Break the reference with the last element */` |
|        7 |  6865 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  6866 | `			}` |
|        - |  6867 | `			/* Automatically reset the loop cursor */` |
|    11454 |  6868 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6869 | `			/* Cleanup the mess left behind */` |
|    11454 |  6870 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    11454 |  6871 | `			SySetPop(&pInfo->aStep);` |
|    11454 |  6872 | `			PH7_HashmapUnref(pMap);` |
|     5728 |  6873 | `		}else{` |
|   176160 |  6874 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      426 |  6875 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      426 |  6876 | `				if( pKey ){` |
|      426 |  6877 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      212 |  6878 | `				}` |
|      212 |  6879 | `			}` |
|   176160 |  6880 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6881 | `				SyHashEntry *pEntry;` |
|        - |  6882 | `				/* Pass by reference */` |
|       23 |  6883 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  6884 | `				if( pEntry ){` |
|       21 |  6885 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  6886 | `				}else{` |
|        4 |  6887 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  6888 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  6889 | `				}` |
|       12 |  6890 | `			}else{` |
|        - |  6891 | `				/* Make a copy of the entry value */` |
|   176138 |  6892 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   176138 |  6893 | `				if( pValue ){` |
|   176138 |  6894 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    88068 |  6895 | `				}` |
|        - |  6896 | `			}` |
|        2 |  6897 | `		}` |
|    93935 |  6898 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  6899 | `		/* Iterator-based iteration.` |
|        - |  6900 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  6901 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  6902 | `		 */` |
|      106 |  6903 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  6904 | `		ph7_class_method *pMethod;` |
|        - |  6905 | `		ph7_value sResult;` |
|      106 |  6906 | `		int isValid = 0;` |
|        - |  6907 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  6908 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  6909 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  6910 | `		}else{` |
|       82 |  6911 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  6912 | `			if( pMethod ){` |
|       82 |  6913 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  6914 | `			}` |
|        - |  6915 | `		}` |
|        - |  6916 | `		/* Call valid() */` |
|      106 |  6917 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  6918 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  6919 | `		if( pMethod ){` |
|      106 |  6920 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  6921 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  6922 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  6923 | `		}` |
|      106 |  6924 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  6925 | `		if( !isValid ){` |
|        - |  6926 | `			/* Iterator exhausted */` |
|       24 |  6927 | `			pc = pInstr->iP2 - 1;` |
|        - |  6928 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  6929 | `			if( pStep->pOwner ){` |
|        3 |  6930 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  6931 | `			}` |
|       24 |  6932 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  6933 | `			SySetPop(&pInfo->aStep);` |
|       24 |  6934 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  6935 | `		}else{` |
|        - |  6936 | `			/* Call current() to get value */` |
|       84 |  6937 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  6938 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  6939 | `			if( pMethod ){` |
|       84 |  6940 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  6941 | `			}` |
|       84 |  6942 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  6943 | `			if( pValue ){` |
|       84 |  6944 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  6945 | `			}` |
|       84 |  6946 | `			PH7_MemObjRelease(&sResult);` |
|        - |  6947 | `			/* Call key() if needed */` |
|       84 |  6948 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  6949 | `				ph7_value sKey;` |
|       35 |  6950 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  6951 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  6952 | `				if( pMethod ){` |
|       35 |  6953 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  6954 | `				}` |
|       35 |  6955 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  6956 | `				if( pValue ){` |
|       35 |  6957 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  6958 | `				}` |
|       35 |  6959 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  6960 | `			}` |
|        - |  6961 | `		}` |
|       54 |  6962 | `	}else{` |
|       25 |  6963 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  6964 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  6965 | `		SyHashEntry *pEntry;` |
|        - |  6966 | `		/* Point to the next attribute */` |
|       29 |  6967 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  6968 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  6969 | `			/* Check access permission */` |
|       31 |  6970 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  6971 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  6972 | `					break; /* Access is granted */` |
|        - |  6973 | `			}` |
|        1 |  6974 | `		}` |
|       25 |  6975 | `		if( pEntry == 0 ){` |
|        - |  6976 | `			/* Clean up the mess left behind */` |
|        9 |  6977 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  6978 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6979 | `				/* Break the reference with the last element */` |
|        3 |  6980 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  6981 | `			}` |
|        9 |  6982 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  6983 | `			SySetPop(&pInfo->aStep);` |
|        9 |  6984 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  6985 | `		}else{` |
|       17 |  6986 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  6987 | `			ph7_value *pAttrValue;` |
|       17 |  6988 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  6989 | `				/* Fill with the current attribute name */` |
|       17 |  6990 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  6991 | `				if( pKey ){` |
|       17 |  6992 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  6993 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  6994 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  6995 | `				}` |
|        8 |  6996 | `			}` |
|        - |  6997 | `			/* Extract attribute value */` |
|       17 |  6998 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  6999 | `			if( pAttrValue ){` |
|       17 |  7000 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7001 | `					/* Pass by reference */` |
|        3 |  7002 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  7003 | `					if( pEntry ){` |
|        3 |  7004 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  7005 | `					}else{` |
|      ! 0 |  7006 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  7007 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  7008 | `					}` |
|        2 |  7009 | `				}else{` |
|        - |  7010 | `					/* Make a copy of the attribute value */` |
|       15 |  7011 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  7012 | `					if( pValue ){` |
|       15 |  7013 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  7014 | `					}` |
|        - |  7015 | `				}` |
|        8 |  7016 | `			}` |
|        - |  7017 | `		}` |
|        - |  7018 | `	}` |
|   187740 |  7019 | `	break;` |
|        - |  7020 | `						  }` |
|        - |  7021 | `/*` |
|        - |  7022 | ` * OP_MEMBER P1 P2` |
|        - |  7023 | ` * Load class attribute/method on the stack.` |
|        - |  7024 | ` */` |
|     3310 |  7025 | `case PH7_OP_MEMBER: {` |
|        - |  7026 | `	ph7_class_instance *pThis;` |
|        - |  7027 | `	ph7_value *pNos;` |
|        - |  7028 | `	SyString sName;` |
|     6622 |  7029 | `	if( !pInstr->iP1 ){` |
|     6396 |  7030 | `		pNos = &pTos[-1];` |
|        - |  7031 | `#ifdef UNTRUST` |
|        - |  7032 | `		if( pNos < pStack ){` |
|        - |  7033 | `			goto Abort;` |
|        - |  7034 | `		}` |
|        - |  7035 | `#endif` |
|     6396 |  7036 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7037 | `			ph7_class *pClass;` |
|        - |  7038 | `			/* Class already instantiated */` |
|     6394 |  7039 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  7040 | `			/* Point to the instantiated class */` |
|     6394 |  7041 | `			pClass = pThis->pClass;` |
|        - |  7042 | `			/* Extract attribute name first */` |
|     6394 |  7043 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     6394 |  7044 | `			if( pInstr->iP2 ){` |
|        - |  7045 | `				/* Method call */` |
|      666 |  7046 | `				ph7_class_method *pMeth = 0;` |
|      666 |  7047 | `				if( sName.nByte > 0 ){` |
|        - |  7048 | `					/* Extract the target method */` |
|      666 |  7049 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      332 |  7050 | `				}` |
|      666 |  7051 | `				if( pMeth == 0 ){` |
|      ! 0 |  7052 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  7053 | `						&pClass->sName,&sName` |
|        - |  7054 | `						);` |
|        - |  7055 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  7056 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  7057 | `					/* Pop the method name from the stack */` |
|      ! 0 |  7058 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7059 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  7060 | `				}else{` |
|        - |  7061 | `					/* Push method name on the stack */` |
|      666 |  7062 | `					PH7_MemObjRelease(pTos);` |
|      666 |  7063 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      666 |  7064 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7065 | `				}` |
|      666 |  7066 | `				pTos->nIdx = SXU32_HIGH;` |
|      334 |  7067 | `			}else{` |
|        - |  7068 | `				/* Attribute access */` |
|     5730 |  7069 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  7070 | `				SyHashEntry *pEntry;` |
|        - |  7071 | `				/* Extract the target attribute */` |
|     5730 |  7072 | `				if( sName.nByte > 0 ){` |
|     5730 |  7073 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     5730 |  7074 | `					if( pEntry ){` |
|        - |  7075 | `						/* Point to the attribute value */` |
|     5728 |  7076 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     2863 |  7077 | `					}` |
|     2864 |  7078 | `				}` |
|     5730 |  7079 | `				if( pObjAttr == 0 ){` |
|        - |  7080 | `					/* No such attribute,load null */` |
|        4 |  7081 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  7082 | `						&pClass->sName,&sName);` |
|        - |  7083 | `					/* Call the __get magic method if available */` |
|        3 |  7084 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  7085 | `				}` |
|     5730 |  7086 | `				VmPopOperand(&pTos,1);` |
|        - |  7087 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  7088 | `				 * This is due to the following case:` |
|        - |  7089 | `				 *     (new TestClass())->foo;` |
|        - |  7090 | `				 */` |
|     5730 |  7091 | `				pThis->iRef++;` |
|     5730 |  7092 | `				PH7_MemObjRelease(pTos);` |
|     5730 |  7093 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     5730 |  7094 | `				if( pObjAttr ){` |
|     5728 |  7095 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  7096 | `					/* Check attribute access */` |
|     5728 |  7097 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  7098 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  7099 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  7100 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  7101 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  7102 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     5726 |  7103 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     2900 |  7104 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       72 |  7105 | `							VmInstr *pNext = pInstr + 1;` |
|       72 |  7106 | `							int bIsLhs = 0;` |
|       72 |  7107 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       70 |  7108 | `								bIsLhs = 1;` |
|       34 |  7109 | `							}` |
|       72 |  7110 | `							if( !bIsLhs ){` |
|        3 |  7111 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  7112 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  7113 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  7114 | `									goto Abort;` |
|        - |  7115 | `								}` |
|        - |  7116 | `								{` |
|        3 |  7117 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7118 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7119 | `										pc = pFrm2->iExceptionJump - 1;` |
|     3310 |  7120 | `										break;` |
|        - |  7121 | `									}` |
|        - |  7122 | `								}` |
|      ! 0 |  7123 | `								goto Exception;` |
|        - |  7124 | `							}` |
|       34 |  7125 | `						}` |
|        - |  7126 | `						/* Load attribute */` |
|     5726 |  7127 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     5726 |  7128 | `						if( pValue ){` |
|     5726 |  7129 | `							if( pThis->iRef < 2 ){` |
|        - |  7130 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  7131 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  7132 | `								 */` |
|        7 |  7133 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  7134 | `							}else{` |
|        - |  7135 | `								/* Simple load */` |
|     5720 |  7136 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  7137 | `							}` |
|     5726 |  7138 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     5724 |  7139 | `								if( pThis->iRef > 1 ){` |
|        - |  7140 | `									/* Load attribute index */` |
|     5718 |  7141 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     2858 |  7142 | `								}` |
|     2861 |  7143 | `							}` |
|     2862 |  7144 | `						}` |
|     2864 |  7145 | `					}else{` |
|        - |  7146 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  7147 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  7148 | `						char zMsg[256];` |
|      ! 0 |  7149 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  7150 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7151 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7152 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  7153 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7154 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  7155 | `						goto Abort;` |
|        - |  7156 | `					}` |
|     2862 |  7157 | `				}` |
|        - |  7158 | `				/* Safely unreference the object */` |
|     5728 |  7159 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  7160 | `			}` |
|     3197 |  7161 | `		}else{` |
|        3 |  7162 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  7163 | `			VmPopOperand(&pTos,1);` |
|        3 |  7164 | `			PH7_MemObjRelease(pTos);` |
|        3 |  7165 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  7166 | `		}` |
|     3198 |  7167 | `	}else{` |
|        - |  7168 | `		/* Static member access using class name */` |
|      228 |  7169 | `		pNos = pTos;` |
|      228 |  7170 | `		pThis = 0;` |
|      228 |  7171 | `		if( !pInstr->p3 ){` |
|      190 |  7172 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      190 |  7173 | `			pNos--;` |
|        - |  7174 | `#ifdef UNTRUST` |
|        - |  7175 | `			if( pNos < pStack ){` |
|        - |  7176 | `				goto Abort;` |
|        - |  7177 | `			}` |
|        - |  7178 | `#endif` |
|       96 |  7179 | `		}else{` |
|        - |  7180 | `			/* Attribute name already computed */` |
|       40 |  7181 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7182 | `		}` |
|      228 |  7183 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      228 |  7184 | `			ph7_class *pClass = 0;` |
|      228 |  7185 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7186 | `				/* Class already instantiated */` |
|        5 |  7187 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  7188 | `				pClass = pThis->pClass;` |
|        5 |  7189 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  7190 | `			}else{` |
|        - |  7191 | `				/* Try to extract the target class */` |
|      224 |  7192 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      224 |  7193 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      224 |  7194 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  7195 | `					/* Handle self/static/parent keywords */` |
|      224 |  7196 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  7197 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  7198 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  7199 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  7200 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  7201 | `						}` |
|      194 |  7202 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  7203 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      164 |  7204 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       28 |  7205 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       28 |  7206 | `						if( pSelf && pSelf->pBase ){` |
|       28 |  7207 | `							pClass = pSelf->pBase;` |
|       13 |  7208 | `						}` |
|       15 |  7209 | `					}else{` |
|      112 |  7210 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  7211 | `					}` |
|      111 |  7212 | `				}` |
|        - |  7213 | `			}` |
|      228 |  7214 | `			if( pClass == 0 ){` |
|        - |  7215 | `				/* Undefined class */` |
|      ! 0 |  7216 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  7217 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  7218 | `					);` |
|      ! 0 |  7219 | `				if( !pInstr->p3 ){` |
|      ! 0 |  7220 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7221 | `				}` |
|      ! 0 |  7222 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  7223 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  7224 | `			}else{` |
|      228 |  7225 | `				if( pInstr->iP2 ){` |
|        - |  7226 | `					/* Method call */` |
|       86 |  7227 | `					ph7_class_method *pMeth = 0;` |
|       86 |  7228 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  7229 | `						/* Extract the target method */` |
|       86 |  7230 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  7231 | `					}` |
|       86 |  7232 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  7233 | `						if( pMeth ){` |
|      ! 0 |  7234 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  7235 | `								&pClass->sName,&sName` |
|        - |  7236 | `								);` |
|      ! 0 |  7237 | `						}else{` |
|      ! 0 |  7238 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7239 | `								&pClass->sName,&sName` |
|        - |  7240 | `								);` |
|        - |  7241 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  7242 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  7243 | `						}` |
|        - |  7244 | `						/* Pop the method name from the stack */` |
|      ! 0 |  7245 | `						if( !pInstr->p3 ){` |
|      ! 0 |  7246 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  7247 | `						}` |
|      ! 0 |  7248 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  7249 | `					}else{` |
|        - |  7250 | `						/* Push method name on the stack */` |
|       86 |  7251 | `						PH7_MemObjRelease(pTos);` |
|       86 |  7252 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       86 |  7253 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7254 | `					}` |
|       86 |  7255 | `					pTos->nIdx = SXU32_HIGH;` |
|       44 |  7256 | `				}else{` |
|        - |  7257 | `					/* Attribute access */` |
|      144 |  7258 | `					ph7_class_attr *pAttr = 0;` |
|        - |  7259 | `					/* Check for special ::class pseudo-constant */` |
|      190 |  7260 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  7261 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  7262 | `						/* ::class returns the fully qualified class name */` |
|        - |  7263 | `						/* Pop the attribute name from the stack */` |
|       60 |  7264 | `						if( !pInstr->p3 ){` |
|       60 |  7265 | `							VmPopOperand(&pTos,1);` |
|       29 |  7266 | `						}` |
|       60 |  7267 | `						PH7_MemObjRelease(pTos);` |
|        - |  7268 | `						/* Load the class name */` |
|       60 |  7269 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  7270 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  7271 | `					}else{` |
|        - |  7272 | `						/* Extract the target attribute */` |
|       86 |  7273 | `						if( sName.nByte > 0 ){` |
|       86 |  7274 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       42 |  7275 | `						}` |
|       86 |  7276 | `						if( pAttr == 0 ){` |
|        - |  7277 | `							/* No such attribute,load null */` |
|      ! 0 |  7278 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7279 | `								&pClass->sName,&sName);` |
|        - |  7280 | `							/* Call the __get magic method if available */` |
|      ! 0 |  7281 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  7282 | `						}` |
|        - |  7283 | `						/* Pop the attribute name from the stack */` |
|       86 |  7284 | `						if( !pInstr->p3 ){` |
|       48 |  7285 | `							VmPopOperand(&pTos,1);` |
|       23 |  7286 | `						}` |
|       86 |  7287 | `						PH7_MemObjRelease(pTos);` |
|       86 |  7288 | `						pTos->nIdx = SXU32_HIGH;` |
|       86 |  7289 | `						if( pAttr ){` |
|       86 |  7290 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  7291 | `								/* Access to a non static attribute */` |
|      ! 0 |  7292 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7293 | `									&pClass->sName,&pAttr->sName` |
|        - |  7294 | `									);` |
|      ! 0 |  7295 | `							}else{` |
|        - |  7296 | `								ph7_value *pValue;` |
|        - |  7297 | `								/* Check if the access to the attribute is allowed */` |
|       86 |  7298 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  7299 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  7300 | `									 * Same LHS-of-store peek as the instance path. */` |
|       80 |  7301 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       55 |  7302 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       41 |  7303 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       26 |  7304 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       28 |  7305 | `										if( pS ){` |
|       28 |  7306 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       28 |  7307 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  7308 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  7309 | `												int bIsLhs = 0;` |
|        8 |  7310 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  7311 | `													bIsLhs = 1;` |
|        2 |  7312 | `												}` |
|        8 |  7313 | `												if( !bIsLhs ){` |
|        3 |  7314 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  7315 | `													if( pThis ){` |
|      ! 0 |  7316 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7317 | `													}` |
|        3 |  7318 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  7319 | `														goto Abort;` |
|        - |  7320 | `													}` |
|        - |  7321 | `													{` |
|        3 |  7322 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7323 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7324 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  7325 | `															break;` |
|        - |  7326 | `														}` |
|        - |  7327 | `													}` |
|      ! 0 |  7328 | `													goto Exception;` |
|        - |  7329 | `												}` |
|        2 |  7330 | `											}` |
|       12 |  7331 | `										}` |
|       12 |  7332 | `									}` |
|        - |  7333 | `									/* Load the desired attribute */` |
|       80 |  7334 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       80 |  7335 | `									if( pValue ){` |
|       80 |  7336 | `										PH7_MemObjLoad(pValue,pTos);` |
|       80 |  7337 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  7338 | `											/* Load index number */` |
|       38 |  7339 | `											pTos->nIdx = pAttr->nIdx;` |
|       18 |  7340 | `										}` |
|       39 |  7341 | `									}` |
|       41 |  7342 | `								}else{` |
|        - |  7343 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  7344 | `									char zMsg[256];` |
|        5 |  7345 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  7346 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  7347 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  7348 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  7349 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  7350 | `									}else{` |
|      ! 0 |  7351 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7352 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7353 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  7354 | `									}` |
|        5 |  7355 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  7356 | `									goto Abort;` |
|        - |  7357 | `								}` |
|        - |  7358 | `							}` |
|       39 |  7359 | `						}` |
|        - |  7360 | `					}` |
|        - |  7361 | `				}` |
|      222 |  7362 | `				if( pThis ){` |
|        - |  7363 | `					/* Safely unreference the object */` |
|        5 |  7364 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  7365 | `				}` |
|        - |  7366 | `			}` |
|      112 |  7367 | `		}else{` |
|        - |  7368 | `			/* Pop operands */` |
|      ! 0 |  7369 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  7370 | `			if( !pInstr->p3 ){` |
|      ! 0 |  7371 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  7372 | `			}` |
|      ! 0 |  7373 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7374 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  7375 | `		}` |
|        - |  7376 | `	}` |
|     6614 |  7377 | `	break;` |
|        - |  7378 | `					}` |
|        - |  7379 | `/*` |
|        - |  7380 | ` * OP_NEW P1 * * *` |
|        - |  7381 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  7382 | ` */` |
|      532 |  7383 | `case PH7_OP_NEW: {` |
|     1066 |  7384 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1066 |  7385 | `	ph7_class *pClass = 0;` |
|        - |  7386 | `	ph7_class_instance *pNew;` |
|     1066 |  7387 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  7388 | `		/* Try to extract the desired class */` |
|     1598 |  7389 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1064 |  7390 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      532 |  7391 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7392 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  7393 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  7394 | `	}` |
|     1066 |  7395 | `	if( pClass == 0 ){` |
|        - |  7396 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  7397 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  7398 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  7399 | `			);` |
|        - |  7400 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  7401 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7402 | `		if( pInstr->iP1 > 0 ){` |
|        - |  7403 | `			/* Pop given arguments */` |
|      ! 0 |  7404 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  7405 | `		}` |
|      ! 0 |  7406 | `		goto Abort;` |
|      ! 0 |  7407 | `	}else{` |
|        - |  7408 | `		ph7_class_method *pCons;` |
|        - |  7409 | `		/* Create a new class instance */` |
|     1066 |  7410 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1066 |  7411 | `		if( pNew == 0 ){` |
|      ! 0 |  7412 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7413 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  7414 | `				&pClass->sName` |
|        - |  7415 | `			);` |
|      ! 0 |  7416 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7417 | `			if( pInstr->iP1 > 0 ){` |
|        - |  7418 | `				/* Pop given arguments */` |
|      ! 0 |  7419 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  7420 | `			}` |
|      ! 0 |  7421 | `			break;` |
|        - |  7422 | `		}` |
|        - |  7423 | `		/* Check if a constructor is available */` |
|     1066 |  7424 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1066 |  7425 | `		if( pCons == 0 ){` |
|      762 |  7426 | `			SyString *pName = &pClass->sName;` |
|        - |  7427 | `			/* Check for a constructor with the same base class name */` |
|      762 |  7428 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      380 |  7429 | `		}` |
|     1066 |  7430 | `		if( pCons ){` |
|        - |  7431 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  7432 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  7433 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  7434 | `			 * (including variadic string-key packing). */` |
|      306 |  7435 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|      306 |  7436 | `			SySetReset(&aArg);` |
|      600 |  7437 | `			while( pArg < pTos ){` |
|      296 |  7438 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      296 |  7439 | `				pArg++;` |
|        2 |  7440 | `			}` |
|      306 |  7441 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  7442 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  7443 | `				sxu32 n;` |
|       61 |  7444 | `				n = SySetUsed(&aArg);` |
|        - |  7445 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  7446 | `				 * for named args the missing-arg check happens downstream` |
|        - |  7447 | `				 * after resolution). */` |
|      109 |  7448 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       49 |  7449 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       49 |  7450 | `					if( pFuncArg ){` |
|       49 |  7451 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  7452 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  7453 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  7454 | `						}` |
|       24 |  7455 | `					}` |
|       49 |  7456 | `					n++;` |
|        1 |  7457 | `				}` |
|       30 |  7458 | `			}` |
|      306 |  7459 | `			VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  7460 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      306 |  7461 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  7462 | `				pNew->iRef = 1;` |
|      ! 0 |  7463 | `			}` |
|      152 |  7464 | `		}` |
|     1066 |  7465 | `		if( pInstr->iP1 > 0 ){` |
|        - |  7466 | `			/* Pop given arguments */` |
|      242 |  7467 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      120 |  7468 | `		}` |
|     1066 |  7469 | `		PH7_MemObjRelease(pTos);` |
|     1066 |  7470 | `		pTos->x.pOther = pNew;` |
|     1066 |  7471 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  7472 | `	}` |
|     1066 |  7473 | `	break;` |
|        - |  7474 | `				 }` |
|        - |  7475 | `/*` |
|        - |  7476 | ` * OP_CLONE * * *` |
|        - |  7477 | ` * Perfome a clone operation.` |
|        - |  7478 | ` */` |
|       24 |  7479 | `case PH7_OP_CLONE: {` |
|        - |  7480 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  7481 | `#ifdef UNTRUST` |
|        - |  7482 | `	if( pTos < pStack ){` |
|        - |  7483 | `		goto Abort;` |
|        - |  7484 | `	}` |
|        - |  7485 | `#endif` |
|        - |  7486 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  7487 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  7488 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7489 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  7490 | `		PH7_MemObjRelease(pTos);` |
|        5 |  7491 | `		break;` |
|        - |  7492 | `	}` |
|        - |  7493 | `	/* Point to the source */` |
|       46 |  7494 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7495 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  7496 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  7497 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7498 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  7499 | `			&pSrc->pClass->sName);` |
|      ! 0 |  7500 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7501 | `		break;` |
|        - |  7502 | `	}` |
|        - |  7503 | `	/* Perform the clone operation */` |
|       46 |  7504 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  7505 | `	PH7_MemObjRelease(pTos);` |
|       46 |  7506 | `	if( pClone == 0 ){` |
|      ! 0 |  7507 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7508 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  7509 | `	}else{` |
|        - |  7510 | `		/* Load the cloned object */` |
|       46 |  7511 | `		pTos->x.pOther = pClone;` |
|       46 |  7512 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  7513 | `	}` |
|       46 |  7514 | `	break;` |
|        - |  7515 | `				   }` |
|        - |  7516 | `/*` |
|        - |  7517 | ` * OP_SWITCH * * P3` |
|        - |  7518 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  7519 | ` */` |
|       26 |  7520 | `case PH7_OP_SWITCH: {` |
|       54 |  7521 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  7522 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  7523 | `	ph7_value sValue,sCaseValue;` |
|        - |  7524 | `	sxu32 n,nEntry;` |
|        - |  7525 | `#ifdef UNTRUST` |
|        - |  7526 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  7527 | `		goto Abort;` |
|        - |  7528 | `	}` |
|        - |  7529 | `#endif` |
|        - |  7530 | `	/* Point to the case table  */` |
|       54 |  7531 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  7532 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  7533 | `	/* Select the appropriate case block to execute */` |
|       54 |  7534 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  7535 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  7536 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  7537 | `		pCase = &aCase[n];` |
|      130 |  7538 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  7539 | `		/* Execute the case expression first */` |
|      130 |  7540 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  7541 | `		/* Compare the two expression */` |
|      130 |  7542 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  7543 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  7544 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  7545 | `		if( rc == 0 ){` |
|        - |  7546 | `			/* Value match,jump to this block */` |
|       52 |  7547 | `			pc = pCase->nStart - 1;` |
|       52 |  7548 | `			break;` |
|        - |  7549 | `		}` |
|       41 |  7550 | `	}` |
|       54 |  7551 | `	VmPopOperand(&pTos,1);` |
|       54 |  7552 | `	if( n >= nEntry ){` |
|        - |  7553 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  7554 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  7555 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  7556 | `		}else{` |
|        - |  7557 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  7558 | `			pc = pSwitch->nOut - 1;` |
|        - |  7559 | `		}` |
|        1 |  7560 | `	}` |
|       54 |  7561 | `	break;` |
|        - |  7562 | `					}` |
|        - |  7563 | `/*` |
|        - |  7564 | ` * OP_MATCH * * P3` |
|        - |  7565 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  7566 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  7567 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  7568 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  7569 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  7570 | ` */` |
|       54 |  7571 | `case PH7_OP_MATCH: {` |
|      110 |  7572 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      110 |  7573 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  7574 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  7575 | `	sxu32 i,j,nArm,nCond;` |
|      110 |  7576 | `	int matched = 0;` |
|        - |  7577 | `#ifdef UNTRUST` |
|        - |  7578 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  7579 | `		goto Abort;` |
|        - |  7580 | `	}` |
|        - |  7581 | `#endif` |
|      110 |  7582 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      110 |  7583 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      110 |  7584 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      110 |  7585 | `	PH7_MemObjInit(pVm,&sCond);` |
|      110 |  7586 | `	PH7_MemObjInit(pVm,&sResult);` |
|      110 |  7587 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      348 |  7588 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  7589 | `		pArm = &aArm[i];` |
|      240 |  7590 | `		if( pArm->bDefault ){` |
|       13 |  7591 | `			pDefault = pArm;` |
|       13 |  7592 | `			continue;` |
|        - |  7593 | `		}` |
|      228 |  7594 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  7595 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  7596 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  7597 | `			if( pCondBc == 0 ){` |
|      ! 0 |  7598 | `				continue;` |
|        - |  7599 | `			}` |
|      260 |  7600 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      260 |  7601 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  7602 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  7603 | `			if( rc == 0 ){` |
|       93 |  7604 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       93 |  7605 | `				matched = 1;` |
|       93 |  7606 | `				break;` |
|        - |  7607 | `			}` |
|       85 |  7608 | `		}` |
|      115 |  7609 | `	}` |
|      110 |  7610 | `	if( !matched && pDefault ){` |
|       13 |  7611 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       13 |  7612 | `		matched = 1;` |
|        6 |  7613 | `	}` |
|      110 |  7614 | `	if( !matched ){` |
|        5 |  7615 | `		const char *zType = "unknown";` |
|        - |  7616 | `		char zMsg[128];` |
|        - |  7617 | `		sxu32 nMsg;` |
|        5 |  7618 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  7619 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  7620 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  7621 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  7622 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  7623 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  7624 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  7625 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  7626 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  7627 | `		default: break;` |
|        - |  7628 | `		}` |
|        7 |  7629 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  7630 | `			"Unhandled match case of type %s",zType);` |
|        7 |  7631 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  7632 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  7633 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  7634 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  7635 | `		goto Abort;` |
|        - |  7636 | `	}` |
|      105 |  7637 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  7638 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  7639 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  7640 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  7641 | `	break;` |
|        - |  7642 | `					}` |
|        - |  7643 | `/*` |
|        - |  7644 | ` * OP_YIELD P1 P2 *` |
|        - |  7645 | ` *  Yield a value from a generator function.` |
|        - |  7646 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  7647 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  7648 | ` */` |
|       34 |  7649 | `case PH7_OP_YIELD: {` |
|        - |  7650 | `	ph7_generator *pGen;` |
|       70 |  7651 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  7652 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  7653 | `		goto Abort;` |
|        - |  7654 | `	}` |
|       70 |  7655 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  7656 | `	if( pInstr->iP2 ){` |
|        - |  7657 | `		/* yield $key => $value: value on top, key below */` |
|        - |  7658 | `#ifdef UNTRUST` |
|        - |  7659 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  7660 | `#endif` |
|        7 |  7661 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  7662 | `		VmPopOperand(&pTos, 1);` |
|        7 |  7663 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  7664 | `		VmPopOperand(&pTos, 1);` |
|        - |  7665 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  7666 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  7667 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  7668 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  7669 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  7670 | `			}` |
|        1 |  7671 | `		}` |
|       67 |  7672 | `	}else if( pInstr->iP1 ){` |
|        - |  7673 | `		/* yield $value */` |
|        - |  7674 | `#ifdef UNTRUST` |
|        - |  7675 | `		if( pTos < pStack ) goto Abort;` |
|        - |  7676 | `#endif` |
|       64 |  7677 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  7678 | `		VmPopOperand(&pTos, 1);` |
|        - |  7679 | `		/* Auto-increment key */` |
|       64 |  7680 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  7681 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  7682 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  7683 | `	}else{` |
|        - |  7684 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  7685 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7686 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7687 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  7688 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  7689 | `	}` |
|        - |  7690 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  7691 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  7692 | `	goto Suspend;` |
|        - |  7693 |  |
|        - |  7694 | `/*` |
|        - |  7695 | ` * OP_CALL P1 * *` |
|        - |  7696 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  7697 | ` *  function on the stack.` |
|        - |  7698 | ` */` |
|   334024 |  7699 | `case PH7_OP_CALL: {` |
|   668094 |  7700 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  7701 | `	ph7_value *pArg;` |
|   668094 |  7702 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   668094 |  7703 | `	pArg = &pTos[-nCallArgs];` |
|        - |  7704 | `	SyHashEntry *pEntry;` |
|        - |  7705 | `	SyString sName;` |
|        - |  7706 | `	/* Extract function name */` |
|   668094 |  7707 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  7708 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7709 | `			ph7_value sResult;` |
|      ! 0 |  7710 | `			SySetReset(&aArg);` |
|      ! 0 |  7711 | `			while( pArg < pTos ){` |
|      ! 0 |  7712 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  7713 | `				pArg++;` |
|      ! 0 |  7714 | `			}` |
|      ! 0 |  7715 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  7716 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  7717 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  7718 | `			SySetReset(&aArg);` |
|        - |  7719 | `			/* Pop given arguments */` |
|      ! 0 |  7720 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7721 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7722 | `			}` |
|        - |  7723 | `			/* Copy result */` |
|      ! 0 |  7724 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  7725 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7726 | `		}else{` |
|        3 |  7727 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  7728 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7729 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  7730 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  7731 | `			}else{` |
|        - |  7732 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  7733 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  7734 | `			}` |
|        - |  7735 | `			/* Pop given arguments */` |
|        3 |  7736 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7737 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7738 | `			}` |
|        - |  7739 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  7740 | `			PH7_MemObjRelease(pTos);` |
|        - |  7741 | `		}` |
|   333738 |  7742 | `		break;` |
|        - |  7743 | `	}` |
|   668092 |  7744 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  7745 | `	/* Check for a compiled function first.` |
|        - |  7746 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  7747 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   668092 |  7748 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  7749 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  7750 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  7751 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  7752 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  7753 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  7754 | `	{` |
|   668092 |  7755 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   668092 |  7756 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  7757 | `		const char *zFunc;` |
|        - |  7758 | `		const char *zEnd;` |
|        - |  7759 | `		const char *z;` |
|        - |  7760 | `		SyString sGlobal;` |
|       22 |  7761 | `		zFunc = sName.zString;` |
|       22 |  7762 | `		zEnd  = zFunc + sName.nByte;` |
|       22 |  7763 | `		z = zEnd;` |
|        - |  7764 | `		/* Find last namespace separator */` |
|      194 |  7765 | `		while( z > zFunc ){` |
|      194 |  7766 | `			if( z[-1] == '\\' ){` |
|       22 |  7767 | `				break;` |
|        - |  7768 | `			}` |
|      174 |  7769 | `			z--;` |
|        2 |  7770 | `		}` |
|       22 |  7771 | `		if( z > zFunc && z < zEnd ){` |
|        - |  7772 | `			/* Retry lookup using the unqualified/global function name */` |
|       22 |  7773 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       22 |  7774 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  7775 | `		}` |
|       10 |  7776 | `	}` |
|        - |  7777 | `	} /* end VmCallArgMap namespace scope */` |
|   668092 |  7778 | `	if( pEntry ){` |
|        - |  7779 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  7780 | `		ph7_class_instance *pThis;` |
|        - |  7781 | `		ph7_value *pFrameStack;` |
|        - |  7782 | `		ph7_vm_func *pVmFunc;` |
|        - |  7783 | `		ph7_class *pSelf;` |
|        - |  7784 | `		VmFrame *pFrame;` |
|        - |  7785 | `		ph7_value *pObj;` |
|        - |  7786 | `		VmSlot sArg;` |
|        - |  7787 | `		sxu32 n;` |
|        - |  7788 | `		/* initialize fields */` |
|    16722 |  7789 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    16722 |  7790 | `		pThis = 0;` |
|    16722 |  7791 | `		pSelf = 0;` |
|    16722 |  7792 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  7793 | `			ph7_class_method *pMeth;` |
|        - |  7794 | `			/* Class method call */` |
|     2590 |  7795 | `			ph7_value *pTarget = &pTos[-1];` |
|     2590 |  7796 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  7797 | `				/* Extract the 'this' pointer */` |
|     2590 |  7798 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  7799 | `					/* Instance already loaded */` |
|     2500 |  7800 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     2500 |  7801 | `					pThis->iRef++;` |
|     2500 |  7802 | `					pSelf = pThis->pClass;` |
|     1249 |  7803 | `				}` |
|     2590 |  7804 | `				if( pSelf == 0 ){` |
|       92 |  7805 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  7806 | `						/* "Late Static Binding" class name */` |
|      128 |  7807 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  7808 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  7809 | `					}` |
|       92 |  7810 | `					if( pSelf == 0 ){` |
|       21 |  7811 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  7812 | `					}` |
|       45 |  7813 | `				}` |
|     2590 |  7814 | `				if( pThis == 0  ){` |
|       92 |  7815 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       92 |  7816 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       92 |  7817 | `					if( pFrameLocal->pParent ){` |
|        - |  7818 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       66 |  7819 | `						pThis = pFrameLocal->pThis;` |
|       66 |  7820 | `						if( pThis ){` |
|       21 |  7821 | `							pThis->iRef++;` |
|       10 |  7822 | `						}` |
|       32 |  7823 | `					}` |
|       45 |  7824 | `				}` |
|     2590 |  7825 | `				VmPopOperand(&pTos,1);` |
|     2590 |  7826 | `				PH7_MemObjRelease(pTos);` |
|        - |  7827 | `				/* Synchronize pointers */` |
|     2590 |  7828 | `				pArg = &pTos[-nCallArgs];` |
|        - |  7829 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  7830 | `				 * user have already computed the random generated unique class method name` |
|        - |  7831 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  7832 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  7833 | `				 */` |
|     2590 |  7834 | `				while( pArg < pStack ){` |
|      ! 0 |  7835 | `					pArg++;` |
|      ! 0 |  7836 | `				}` |
|     2590 |  7837 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  7838 | `					/* Check if the call is allowed */` |
|     2590 |  7839 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     2590 |  7840 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  7841 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  7842 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  7843 | `							char zMsg[256];` |
|      ! 0 |  7844 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  7845 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  7846 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  7847 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  7848 | `							/* Pop given arguments */` |
|      ! 0 |  7849 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  7850 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7851 | `							}` |
|      ! 0 |  7852 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  7853 | `							goto Abort;` |
|        - |  7854 | `						}` |
|        6 |  7855 | `					}` |
|     1294 |  7856 | `				}` |
|     1294 |  7857 | `			}` |
|     1294 |  7858 | `		}` |
|        - |  7859 | `		/* Check The recursion limit */` |
|    16722 |  7860 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  7861 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7862 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  7863 | `				&pVmFunc->sName);` |
|        - |  7864 | `			/* Pop given arguments */` |
|        3 |  7865 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7866 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7867 | `			}` |
|        - |  7868 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  7869 | `			PH7_MemObjRelease(pTos);` |
|       14 |  7870 | `			break;` |
|        - |  7871 | `		}` |
|    16720 |  7872 | `		if( pVmFunc->pNextName ){` |
|        - |  7873 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  7874 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  7875 | `		}` |
|    16720 |  7876 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  7877 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  7878 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  7879 | `			ph7_generator *pGenerator;` |
|        - |  7880 | `			ph7_class_instance *pGenObj;` |
|        - |  7881 | `			ph7_value *pCtxAttr;` |
|        - |  7882 | `			SyString sAttrName;` |
|        - |  7883 | `			ph7_value **apCallArgs;` |
|        - |  7884 | `			int nGenArgs, iArg;` |
|        - |  7885 | `			/* Collect arguments from the operand stack */` |
|       24 |  7886 | `			nGenArgs = (int)(pTos - pArg);` |
|       24 |  7887 | `			apCallArgs = 0;` |
|       24 |  7888 | `			if( nGenArgs > 0 ){` |
|       14 |  7889 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  7890 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  7891 | `				if( apCallArgs == 0 ){` |
|        - |  7892 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  7893 | `					nGenArgs = 0;` |
|      ! 0 |  7894 | `				}else{` |
|       10 |  7895 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  7896 | `					int didReorder = 0;` |
|       10 |  7897 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  7898 | `						/* Named-argument reordering for generator */` |
|        5 |  7899 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  7900 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  7901 | `						sxu32 nNV = nF;` |
|        5 |  7902 | `						sxi32 iVIdx = -1;` |
|        - |  7903 | `						sxi32 *aGSlot;` |
|        - |  7904 | `						sxu8 *aGUsed;` |
|        - |  7905 | `						sxu32 gi;` |
|       13 |  7906 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  7907 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  7908 | `						}` |
|        7 |  7909 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  7910 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  7911 | `						if( aGSlot ){` |
|        5 |  7912 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  7913 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  7914 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  7915 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  7916 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  7917 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7918 | `								goto Abort;` |
|        - |  7919 | `							}` |
|        - |  7920 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  7921 | `							 * append overflow (variadic / positional beyond` |
|        - |  7922 | `							 * formals) so downstream sees every argument. */` |
|        - |  7923 | `							{` |
|        5 |  7924 | `								int nOut = 0;` |
|       13 |  7925 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  7926 | `									sxu32 gj;` |
|       13 |  7927 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  7928 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  7929 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  7930 | `											break;` |
|        - |  7931 | `										}` |
|        3 |  7932 | `									}` |
|        5 |  7933 | `								}` |
|       13 |  7934 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  7935 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  7936 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  7937 | `									}` |
|        5 |  7938 | `								}` |
|        5 |  7939 | `								nGenArgs = nOut;` |
|        - |  7940 | `							}` |
|        5 |  7941 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  7942 | `							didReorder = 1;` |
|        2 |  7943 | `						}` |
|        - |  7944 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  7945 | `						 * positional fill below — preserves arg order rather` |
|        - |  7946 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  7947 | `					}` |
|       10 |  7948 | `					if( !didReorder ){` |
|       12 |  7949 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  7950 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  7951 | `						}` |
|        2 |  7952 | `					}` |
|        - |  7953 | `				}` |
|        4 |  7954 | `			}` |
|        - |  7955 | `			/* Create execution context and generator wrapper */` |
|       24 |  7956 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       24 |  7957 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  7958 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7959 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  7960 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  7961 | `				break;` |
|        - |  7962 | `			}` |
|       24 |  7963 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       24 |  7964 | `			if( pGenerator == 0 ){` |
|      ! 0 |  7965 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  7966 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7967 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  7968 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  7969 | `				break;` |
|        - |  7970 | `			}` |
|        - |  7971 | `			/* Set up the frame with arguments, closure env, $this */` |
|       24 |  7972 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       24 |  7973 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       24 |  7974 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       24 |  7975 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       24 |  7976 | `			pExecCtx->pFrame->pParent = 0;` |
|       24 |  7977 | `			if( apCallArgs ){` |
|       10 |  7978 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  7979 | `			}` |
|       24 |  7980 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  7981 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  7982 | `				if( pThis ){` |
|      ! 0 |  7983 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7984 | `				}` |
|      ! 0 |  7985 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7986 | `					goto Abort;` |
|        - |  7987 | `				}` |
|      ! 0 |  7988 | `				break;` |
|        - |  7989 | `			}` |
|        - |  7990 | `			/* Create Generator class instance */` |
|       24 |  7991 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       24 |  7992 | `			if( pGenObj == 0 ){` |
|      ! 0 |  7993 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  7994 | `				break;` |
|        - |  7995 | `			}` |
|        - |  7996 | `			/* Store generator in __ctx attribute */` |
|       24 |  7997 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       24 |  7998 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       24 |  7999 | `			if( pCtxAttr ){` |
|       24 |  8000 | `				pCtxAttr->x.pOther = pGenerator;` |
|       24 |  8001 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       11 |  8002 | `			}` |
|        - |  8003 | `			/* Pop args and function name, push Generator object */` |
|       24 |  8004 | `			PH7_MemObjRelease(pTos);` |
|       24 |  8005 | `			pTos = &pTos[-nCallArgs];` |
|       24 |  8006 | `			pTos->x.pOther = pGenObj;` |
|       24 |  8007 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       24 |  8008 | `			pGenObj->iRef++;` |
|       24 |  8009 | `			if( pThis ){` |
|      ! 0 |  8010 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8011 | `			}` |
|       24 |  8012 | `			break;` |
|        - |  8013 | `		}` |
|        - |  8014 | `		/* Extract the formal argument set */` |
|    16698 |  8015 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  8016 | `		/* Create a new VM frame  */` |
|    16698 |  8017 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    16698 |  8018 | `		if( rc != SXRET_OK ){` |
|        - |  8019 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  8020 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8021 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  8022 | `				&pVmFunc->sName);` |
|        - |  8023 | `			/* Pop given arguments */` |
|      ! 0 |  8024 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8025 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8026 | `			}` |
|        - |  8027 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8028 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8029 | `			break;` |
|        - |  8030 | `		}` |
|    16698 |  8031 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  8032 | `			/* Install the '$this' variable */` |
|        - |  8033 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     2518 |  8034 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     2518 |  8035 | `			if( pObj ){` |
|        - |  8036 | `				/* Reflect the change */` |
|     2518 |  8037 | `				pObj->x.pOther = pThis;` |
|     2518 |  8038 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1258 |  8039 | `			}` |
|     1258 |  8040 | `		}` |
|    16698 |  8041 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  8042 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  8043 | `			/* Install static variables */` |
|      ! 0 |  8044 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  8045 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  8046 | `				pStatic = &aStatic[n];` |
|      ! 0 |  8047 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  8048 | `					/* Initialize the static variables */` |
|      ! 0 |  8049 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  8050 | `					if( pObj ){` |
|        - |  8051 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  8052 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  8053 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  8054 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  8055 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  8056 | `						}` |
|      ! 0 |  8057 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  8058 | `					}else{` |
|      ! 0 |  8059 | `						continue;` |
|        - |  8060 | `					}` |
|      ! 0 |  8061 | `				}` |
|        - |  8062 | `				/* Install in the current frame */` |
|      ! 0 |  8063 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  8064 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  8065 | `			}` |
|      ! 0 |  8066 | `		}` |
|        - |  8067 | `		/* Push arguments in the local frame */` |
|        - |  8068 | `		{` |
|    16698 |  8069 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  8070 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  8071 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    16698 |  8072 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    16698 |  8073 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  8074 | `			/* ============================================================` |
|        - |  8075 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  8076 | `			 *` |
|        - |  8077 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  8078 | `			 * or position, then install them in the frame.` |
|        - |  8079 | `			 * ============================================================ */` |
|       90 |  8080 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       90 |  8081 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       90 |  8082 | `			sxi32 iVariadicIdx = -1;` |
|        - |  8083 | `			sxu32 nNonVariadic;` |
|        - |  8084 | `			sxi32 *aSlot;` |
|        - |  8085 | `			sxu8  *aUsed;` |
|        - |  8086 | `			sxu32 i;` |
|        - |  8087 | `			/* Find variadic parameter index */` |
|      274 |  8088 | `			for( i = 0; i < nFormal; i++ ){` |
|      194 |  8089 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  8090 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  8091 | `					break;` |
|        - |  8092 | `				}` |
|       94 |  8093 | `			}` |
|       90 |  8094 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  8095 | `			/* Allocate mapping arrays */` |
|      134 |  8096 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       88 |  8097 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       90 |  8098 | `			if( aSlot == 0 ){` |
|      ! 0 |  8099 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  8100 | `				goto Abort;` |
|        - |  8101 | `			}` |
|       90 |  8102 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  8103 | `			/* Resolve named arguments to formal parameters */` |
|      134 |  8104 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       44 |  8105 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       90 |  8106 | `			if( rc == PH7_ABORT ){` |
|        7 |  8107 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  8108 | `				goto Abort;` |
|        - |  8109 | `			}` |
|        - |  8110 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      257 |  8111 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  8112 | `				/* Find the stack arg mapped to formal n */` |
|      175 |  8113 | `				sxi32 iSrc = -1;` |
|      291 |  8114 | `				for( i = 0; i < nActual; i++ ){` |
|      273 |  8115 | `					if( aSlot[i] == (sxi32)n ){` |
|      157 |  8116 | `						iSrc = (sxi32)i;` |
|      157 |  8117 | `						break;` |
|        - |  8118 | `					}` |
|       59 |  8119 | `				}` |
|      175 |  8120 | `				if( iSrc >= 0 ){` |
|        - |  8121 | `					/* Argument was provided — install with type checking */` |
|      157 |  8122 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  8123 | `					/* NULL-to-default redirect (existing behavior) */` |
|      156 |  8124 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  8125 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  8126 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  8127 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8128 | `					}` |
|        - |  8129 | `					/* Type checking: union types */` |
|      157 |  8130 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  8131 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  8132 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  8133 | `							bCallIsStrict);` |
|       13 |  8134 | `						if( rcU != SXRET_OK ){` |
|        - |  8135 | `							const char *zGiven;` |
|      ! 0 |  8136 | `							const char *zExpected = "union";` |
|        - |  8137 | `							char zBuf[128];` |
|        - |  8138 | `							char zTypeBuf[128];` |
|      ! 0 |  8139 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8140 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  8141 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  8142 | `								zGiven = "null";` |
|      ! 0 |  8143 | `							}else{` |
|      ! 0 |  8144 | `								zGiven = ph7_type_name(pVal);` |
|        - |  8145 | `							}` |
|      ! 0 |  8146 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  8147 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  8148 | `							}` |
|      ! 0 |  8149 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8150 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  8151 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8152 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8153 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  8154 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8155 | `							pFrameStack = 0;` |
|      ! 0 |  8156 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  8157 | `							goto SkipFuncBody;` |
|        - |  8158 | `						}` |
|      159 |  8159 | `					}else if( aFormalArg[n].nType > 0` |
|       85 |  8160 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  8161 | `						/* Scalar/class type checking */` |
|       17 |  8162 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  8163 | `							SyString *pName = &aFormalArg[n].sClass;` |
|      ! 0 |  8164 | `							ph7_class *pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  8165 | `							if( pClass ){` |
|      ! 0 |  8166 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8167 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  8168 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8169 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8170 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8171 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8172 | `									}` |
|      ! 0 |  8173 | `								}else{` |
|      ! 0 |  8174 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  8175 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  8176 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8177 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8178 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8179 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8180 | `									}` |
|        - |  8181 | `								}` |
|      ! 0 |  8182 | `							}` |
|       17 |  8183 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  8184 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  8185 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8186 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  8187 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8188 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8189 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8190 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8191 | `								pFrameStack = 0;` |
|      ! 0 |  8192 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8193 | `								goto SkipFuncBody;` |
|        7 |  8194 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8195 | `								char zTypeBuf[128];` |
|      ! 0 |  8196 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8197 | `									&aFormalArg[n].sName,` |
|      ! 0 |  8198 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  8199 | `									ph7_type_name(pVal));` |
|      ! 0 |  8200 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8201 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8202 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8203 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8204 | `								pFrameStack = 0;` |
|      ! 0 |  8205 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8206 | `								goto SkipFuncBody;` |
|        - |  8207 | `							}` |
|        3 |  8208 | `						}` |
|        8 |  8209 | `					}` |
|        - |  8210 | `					/* Install: by reference or by value */` |
|      157 |  8211 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  8212 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  8213 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  8214 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8215 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  8216 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  8217 | `							}` |
|      ! 0 |  8218 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  8219 | `						}else{` |
|        7 |  8220 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  8221 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  8222 | `							if( pRefEntry == 0 ){` |
|        7 |  8223 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  8224 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  8225 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  8226 | `								sArg.pUserData = 0;` |
|        5 |  8227 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  8228 | `							}` |
|        5 |  8229 | `							pObj = 0;` |
|        - |  8230 | `						}` |
|        3 |  8231 | `					}else{` |
|      153 |  8232 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  8233 | `					}` |
|      157 |  8234 | `					if( pObj ){` |
|      153 |  8235 | `						PH7_MemObjStore(pVal,pObj);` |
|      153 |  8236 | `						sArg.nIdx = pObj->nIdx;` |
|      153 |  8237 | `						sArg.pUserData = 0;` |
|      153 |  8238 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       76 |  8239 | `					}` |
|       79 |  8240 | `				}else{` |
|        - |  8241 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  8242 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  8243 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  8244 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  8245 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  8246 | `						if( pObj ){` |
|       19 |  8247 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  8248 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  8249 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  8250 | `							sArg.pUserData = 0;` |
|       19 |  8251 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  8252 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  8253 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8254 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8255 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  8256 | `							}` |
|        9 |  8257 | `						}` |
|        9 |  8258 | `					}` |
|        - |  8259 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  8260 | `				}` |
|       88 |  8261 | `			}` |
|        - |  8262 | `			/* Handle variadic parameter */` |
|       83 |  8263 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  8264 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  8265 | `				if( pObj ){` |
|        9 |  8266 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  8267 | `					{` |
|        9 |  8268 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  8269 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  8270 | `							if( aSlot[i] == -1 ){` |
|       16 |  8271 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  8272 | `									/* Named variadic entry: insert with string key */` |
|        - |  8273 | `									ph7_value sKey;` |
|       11 |  8274 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  8275 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  8276 | `										pCallMap3->aNames[i].zString,` |
|       10 |  8277 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  8278 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  8279 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  8280 | `								}else{` |
|        - |  8281 | `									/* Positional variadic entry */` |
|      ! 0 |  8282 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  8283 | `								}` |
|        5 |  8284 | `							}` |
|       12 |  8285 | `						}` |
|        - |  8286 | `					}` |
|        9 |  8287 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  8288 | `					sArg.pUserData = 0;` |
|        9 |  8289 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  8290 | `				}` |
|        5 |  8291 | `			}else{` |
|        - |  8292 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  8293 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  8294 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  8295 | `				 * the positional-only path's behavior. */` |
|       75 |  8296 | `				sxu32 nAnon = nNonVariadic;` |
|      219 |  8297 | `				for( i = 0; i < nActual; i++ ){` |
|      145 |  8298 | `					if( aSlot[i] == -2 ){` |
|        - |  8299 | `						char zAnonBuf[32];` |
|        - |  8300 | `						SyString sAnonName;` |
|      ! 0 |  8301 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  8302 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  8303 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  8304 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  8305 | `						if( pObj ){` |
|      ! 0 |  8306 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  8307 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  8308 | `							sArg.pUserData = 0;` |
|      ! 0 |  8309 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  8310 | `						}` |
|      ! 0 |  8311 | `						nAnon++;` |
|      ! 0 |  8312 | `					}` |
|       73 |  8313 | `				}` |
|        - |  8314 | `			}` |
|        - |  8315 | `			/* Release all stack arguments */` |
|      249 |  8316 | `			for( i = 0; i < nActual; i++ ){` |
|      167 |  8317 | `				PH7_MemObjRelease(&pArg[i]);` |
|       84 |  8318 | `			}` |
|       83 |  8319 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  8320 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       83 |  8321 | `			n = nFormal;` |
|       42 |  8322 | `		}else{` |
|        - |  8323 | `		/* ============================================================` |
|        - |  8324 | `		 * Positional-only matching path (original)` |
|        - |  8325 | `		 * ============================================================ */` |
|    16610 |  8326 | `		n = 0;` |
|    44576 |  8327 | `		while( pArg < pTos ){` |
|    28034 |  8328 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  8329 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       36 |  8330 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       36 |  8331 | `				if( pObj ){` |
|        - |  8332 | `					/* Initialize as empty array */` |
|       36 |  8333 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  8334 | `					{` |
|       36 |  8335 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      136 |  8336 | `						while( pArg < pTos ){` |
|        - |  8337 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  8338 | `							 *` |
|        - |  8339 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  8340 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  8341 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  8342 | `							 * non-union variadic path below has the same limitation;` |
|        - |  8343 | `							 * fixing both wants a separate counter for elements` |
|        - |  8344 | `							 * already packed into the variadic array. */` |
|      104 |  8345 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  8346 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  8347 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  8348 | `									bCallIsStrict);` |
|       16 |  8349 | `								if( rcU != SXRET_OK ){` |
|        - |  8350 | `									const char *zGiven;` |
|        3 |  8351 | `									const char *zExpected = "union";` |
|        - |  8352 | `									char zBuf[128];` |
|        - |  8353 | `									char zTypeBuf[128];` |
|        3 |  8354 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8355 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  8356 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  8357 | `										zGiven = "null";` |
|      ! 0 |  8358 | `									}else{` |
|        3 |  8359 | `										zGiven = ph7_type_name(pArg);` |
|        - |  8360 | `									}` |
|        3 |  8361 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 |  8362 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 |  8363 | `									}` |
|        4 |  8364 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  8365 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 |  8366 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8367 | `										goto Abort;` |
|        - |  8368 | `									}` |
|        3 |  8369 | `									PH7_MemObjRelease(pTos);` |
|        3 |  8370 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  8371 | `									pFrameStack = 0;` |
|        3 |  8372 | `									rc = PH7_EXCEPTION;` |
|        3 |  8373 | `									goto SkipFuncBody;` |
|        - |  8374 | `								}` |
|       14 |  8375 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  8376 | `								pArg++;` |
|       14 |  8377 | `								continue;` |
|        - |  8378 | `							}` |
|        - |  8379 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  8380 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      104 |  8381 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  8382 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  8383 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  8384 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  8385 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  8386 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8387 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  8388 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8389 | `										goto Abort;` |
|        - |  8390 | `									}` |
|        - |  8391 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  8392 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  8393 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8394 | `									pFrameStack = 0;` |
|      ! 0 |  8395 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  8396 | `									goto SkipFuncBody;` |
|       13 |  8397 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8398 | `									char zTypeBuf[128];` |
|      ! 0 |  8399 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8400 | `										&aFormalArg[n].sName,` |
|      ! 0 |  8401 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  8402 | `										ph7_type_name(pArg));` |
|      ! 0 |  8403 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8404 | `										goto Abort;` |
|        - |  8405 | `									}` |
|      ! 0 |  8406 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  8407 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8408 | `									pFrameStack = 0;` |
|      ! 0 |  8409 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  8410 | `									goto SkipFuncBody;` |
|        - |  8411 | `								}` |
|        6 |  8412 | `							}` |
|       90 |  8413 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|       90 |  8414 | `							pArg++;` |
|        2 |  8415 | `						}` |
|        - |  8416 | `					}` |
|       34 |  8417 | `					sArg.nIdx = pObj->nIdx;` |
|       34 |  8418 | `					sArg.pUserData = 0;` |
|       34 |  8419 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       16 |  8420 | `				}` |
|       34 |  8421 | `				break; /* All remaining args consumed */` |
|        - |  8422 | `			}` |
|    28000 |  8423 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    27840 |  8424 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       33 |  8425 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  8426 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  8427 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  8428 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  8429 | `						goto Abort;` |
|        - |  8430 | `					}` |
|      ! 0 |  8431 | `				}` |
|        - |  8432 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    27842 |  8433 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       89 |  8434 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       58 |  8435 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       29 |  8436 | `						bCallIsStrict);` |
|       60 |  8437 | `					if( rcU != SXRET_OK ){` |
|        - |  8438 | `						const char *zGiven;` |
|       19 |  8439 | `						const char *zExpected = "union";` |
|        - |  8440 | `						char zBuf[128];` |
|        - |  8441 | `						char zTypeBuf[128];` |
|       19 |  8442 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  8443 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  8444 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  8445 | `							zGiven = "null";` |
|        5 |  8446 | `						}else{` |
|        5 |  8447 | `							zGiven = ph7_type_name(pArg);` |
|        - |  8448 | `						}` |
|       19 |  8449 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       19 |  8450 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 |  8451 | `						}` |
|       28 |  8452 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  8453 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       19 |  8454 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  8455 | `							goto Abort;` |
|        - |  8456 | `						}` |
|       19 |  8457 | `						PH7_MemObjRelease(pTos);` |
|       19 |  8458 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  8459 | `						pFrameStack = 0;` |
|       19 |  8460 | `						rc = PH7_EXCEPTION;` |
|       19 |  8461 | `						goto SkipFuncBody;` |
|        - |  8462 | `					}` |
|       21 |  8463 | `				}else` |
|        - |  8464 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  8465 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    27808 |  8466 | `				if( aFormalArg[n].nType > 0` |
|    14547 |  8467 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1284 |  8468 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  8469 | `						/* Argument must be a class instance [i.e: object] */` |
|       26 |  8470 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  8471 | `						ph7_class *pClass;` |
|        - |  8472 | `						/* Try to extract the desired class */` |
|       26 |  8473 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       26 |  8474 | `						if( pClass ){` |
|       22 |  8475 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8476 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  8477 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8478 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8479 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8480 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  8481 | `								}` |
|      ! 0 |  8482 | `							}else{` |
|        - |  8483 | `								/* reuse pThis declared in outer scope */` |
|       22 |  8484 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  8485 | `								/* Make sure the object is an instance of the given class */` |
|       22 |  8486 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  8487 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8488 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8489 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8490 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  8491 | `								}` |
|        - |  8492 | `							}` |
|       12 |  8493 | `						}` |
|     1272 |  8494 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       24 |  8495 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  8496 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  8497 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  8498 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  8499 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  8500 | `								goto Abort;` |
|        - |  8501 | `							}` |
|        - |  8502 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  8503 | `							PH7_MemObjRelease(pTos);` |
|       11 |  8504 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  8505 | `							pFrameStack = 0;` |
|       11 |  8506 | `							rc = PH7_EXCEPTION;` |
|       11 |  8507 | `							goto SkipFuncBody;` |
|       14 |  8508 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8509 | `							char zTypeBuf[128];` |
|        7 |  8510 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        4 |  8511 | `								&aFormalArg[n].sName,` |
|        4 |  8512 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        2 |  8513 | `								ph7_type_name(pArg));` |
|        5 |  8514 | `							if( rc == PH7_ABORT ){` |
|        5 |  8515 | `								goto Abort;` |
|        - |  8516 | `							}` |
|      ! 0 |  8517 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  8518 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8519 | `							pFrameStack = 0;` |
|      ! 0 |  8520 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  8521 | `							goto SkipFuncBody;` |
|        - |  8522 | `						}` |
|        4 |  8523 | `					}` |
|      634 |  8524 | `				}` |
|    27810 |  8525 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  8526 | `					/* Pass by reference */` |
|       54 |  8527 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  8528 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  8529 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  8530 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8531 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  8532 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  8533 | `						}` |
|        - |  8534 | `						/* Switch to pass by value */` |
|      ! 0 |  8535 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  8536 | `					}else{` |
|        - |  8537 | `						SyHashEntry *pRefEntry;` |
|        - |  8538 | `						/* Install the referenced variable in the private function frame */` |
|       54 |  8539 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       54 |  8540 | `						if( pRefEntry == 0 ){` |
|       80 |  8541 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       52 |  8542 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       54 |  8543 | `							sArg.nIdx = pArg->nIdx;` |
|       54 |  8544 | `							sArg.pUserData = 0;` |
|       54 |  8545 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       26 |  8546 | `						}` |
|       54 |  8547 | `						pObj = 0;` |
|        - |  8548 | `					}` |
|       28 |  8549 | `				}else{` |
|        - |  8550 | `					/* Pass by value,make a copy of the given argument */` |
|    27758 |  8551 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  8552 | `				}` |
|    13906 |  8553 | `			}else{` |
|        - |  8554 | `				char zName[32];` |
|        - |  8555 | `				SyString sArgName;` |
|        - |  8556 | `				/* Set a dummy name */` |
|      160 |  8557 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      160 |  8558 | `				sArgName.zString = zName;` |
|        - |  8559 | `				/* Annonymous argument */` |
|      160 |  8560 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  8561 | `			}` |
|    27968 |  8562 | `			if( pObj ){` |
|    27916 |  8563 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  8564 | `				/* Insert argument index  */` |
|    27916 |  8565 | `				sArg.nIdx = pObj->nIdx;` |
|    27916 |  8566 | `				sArg.pUserData = 0;` |
|    27916 |  8567 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    13957 |  8568 | `			}` |
|    27968 |  8569 | `			PH7_MemObjRelease(pArg);` |
|    27968 |  8570 | `			pArg++;` |
|    27968 |  8571 | `			++n;` |
|        2 |  8572 | `		}` |
|        - |  8573 | `		} /* end named vs positional branch */` |
|        - |  8574 | `		/* Set up closure environment */` |
|    16658 |  8575 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  8576 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  8577 | `			ph7_value *pValue;` |
|        - |  8578 | `			sxu32 iEnv;` |
|      115 |  8579 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      295 |  8580 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      181 |  8581 | `				pEnv = &aEnv[iEnv];` |
|      181 |  8582 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  8583 | `					/* Do not install null value */` |
|      109 |  8584 | `					continue;` |
|        - |  8585 | `				}` |
|       73 |  8586 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       73 |  8587 | `				if( pValue == 0 ){` |
|      ! 0 |  8588 | `					continue;` |
|        - |  8589 | `				}` |
|        - |  8590 | `				/* Invalidate any prior representation */` |
|       73 |  8591 | `				PH7_MemObjRelease(pValue);` |
|        - |  8592 | `				/* Duplicate bound variable value */` |
|       73 |  8593 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       37 |  8594 | `			}` |
|       57 |  8595 | `		}` |
|        - |  8596 | `		/* Process default values for remaining formal parameters */` |
|    19106 |  8597 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2490 |  8598 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  8599 | `				/* Variadic parameter with no extra args — create empty array */` |
|       42 |  8600 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       42 |  8601 | `				if( pObj ){` |
|       42 |  8602 | `					PH7_MemObjToHashmap(pObj);` |
|       42 |  8603 | `					sArg.nIdx = pObj->nIdx;` |
|       42 |  8604 | `					sArg.pUserData = 0;` |
|       42 |  8605 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       20 |  8606 | `				}` |
|       42 |  8607 | `				n++;` |
|       42 |  8608 | `				break; /* Variadic is always last */` |
|        - |  8609 | `			}` |
|     2450 |  8610 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2444 |  8611 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2444 |  8612 | `				if( pObj ){` |
|        - |  8613 | `					/* Evaluate the default value and extract it's result */` |
|     2444 |  8614 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2444 |  8615 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  8616 | `						goto Abort;` |
|        - |  8617 | `					}` |
|        - |  8618 | `					/* Insert argument index */` |
|     2444 |  8619 | `					sArg.nIdx = pObj->nIdx;` |
|     2444 |  8620 | `					sArg.pUserData = 0;` |
|     2444 |  8621 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  8622 | `					/* Make sure the default argument is of the correct type */` |
|     2442 |  8623 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1636 |  8624 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 |  8625 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  8626 | `						/* Cast to the desired type */` |
|        3 |  8627 | `						xCast(pObj);` |
|        1 |  8628 | `					}` |
|     1221 |  8629 | `				}` |
|     1221 |  8630 | `			}` |
|     2450 |  8631 | `			++n;` |
|        2 |  8632 | `		}` |
|        - |  8633 | `		} /* end VmCallArgMap scope */` |
|        - |  8634 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  8635 | `		 * does not return anything.` |
|        - |  8636 | `		 */` |
|    16658 |  8637 | `		PH7_MemObjRelease(pTos);` |
|    16658 |  8638 | `		pTos = &pTos[-nCallArgs];` |
|        - |  8639 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    16658 |  8640 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    16658 |  8641 | `		if( pFrameStack == 0 ){` |
|        - |  8642 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  8643 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  8644 | `				&pVmFunc->sName);` |
|      ! 0 |  8645 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8646 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8647 | `			}` |
|      ! 0 |  8648 | `			break;` |
|        - |  8649 | `		}` |
|     8328 |  8650 | `SkipFuncBody:` |
|    16688 |  8651 | `		if( pSelf ){` |
|        - |  8652 | `			/* Push class name */` |
|     2588 |  8653 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1293 |  8654 | `		}` |
|        - |  8655 | `		/* Increment nesting level */` |
|    16688 |  8656 | `		pVm->nRecursionDepth++;` |
|    16688 |  8657 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  8658 | `			/* Execute function body */` |
|    24986 |  8659 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    16656 |  8660 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0);` |
|     8328 |  8661 | `		}` |
|        - |  8662 | `		/* Decrement nesting level */` |
|    16688 |  8663 | `		pVm->nRecursionDepth--;` |
|    16688 |  8664 | `		if( pSelf ){` |
|        - |  8665 | `			/* Pop class name */` |
|     2588 |  8666 | `			(void)SySetPop(&pVm->aSelf);` |
|     1293 |  8667 | `		}` |
|        - |  8668 | `		/* Cleanup the mess left behind */` |
|    16688 |  8669 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  8670 | `			/* Return by reference,reflect that */` |
|        9 |  8671 | `			if( n != SXU32_HIGH ){` |
|        9 |  8672 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  8673 | `				sxu32 i;` |
|        - |  8674 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  8675 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  8676 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  8677 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  8678 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  8679 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  8680 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  8681 | `								&pVmFunc->sName);` |
|      ! 0 |  8682 | `						}` |
|      ! 0 |  8683 | `						n = SXU32_HIGH;` |
|      ! 0 |  8684 | `						break;` |
|        - |  8685 | `					}` |
|        3 |  8686 | `				}` |
|        5 |  8687 | `			}else{` |
|      ! 0 |  8688 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  8689 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  8690 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  8691 | `						&pVmFunc->sName);` |
|      ! 0 |  8692 | `				}` |
|        - |  8693 | `			}` |
|        9 |  8694 | `			pTos->nIdx = n;` |
|        4 |  8695 | `		}` |
|        - |  8696 | `		/* Cleanup the mess left behind */` |
|    16688 |  8697 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  8698 | `			/* An exception was throw in this frame */` |
|       48 |  8699 | `			pFrame = pFrame->pParent;` |
|       48 |  8700 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  8701 | `				/* Pop the resutlt */` |
|       46 |  8702 | `				VmPopOperand(&pTos,1);` |
|        - |  8703 | `				/* Jump to this destination */` |
|       46 |  8704 | `				pc = pFrame->iExceptionJump - 1;` |
|       46 |  8705 | `				rc = PH7_OK;` |
|       24 |  8706 | `			}else{` |
|        3 |  8707 | `				if( pFrame->pParent ){` |
|        3 |  8708 | `					rc = PH7_EXCEPTION;` |
|        2 |  8709 | `				}else{` |
|        - |  8710 | `					/* Continue normal execution */` |
|      ! 0 |  8711 | `					rc = PH7_OK;` |
|        - |  8712 | `				}` |
|        - |  8713 | `			}` |
|       23 |  8714 | `		}` |
|        - |  8715 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    16688 |  8716 | `		if( pFrameStack ){` |
|    16658 |  8717 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     8328 |  8718 | `		}` |
|        - |  8719 | `		/* Leave the frame */` |
|    16688 |  8720 | `		VmLeaveFrame(&(*pVm));` |
|    16688 |  8721 | `		if( rc == PH7_ABORT ){` |
|        - |  8722 | `			/* Abort processing immeditaley */` |
|       15 |  8723 | `			goto Abort;` |
|    16674 |  8724 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  8725 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  8726 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  8727 | `			 * overwriting the state saved by the inner level.` |
|        - |  8728 | `			 * pTos points to the result slot (not yet written).` |
|        - |  8729 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  8730 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  8731 | `			goto Suspend;` |
|    16636 |  8732 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  8733 | `			goto Exception;` |
|        - |  8734 | `		}` |
|     8318 |  8735 | `	}else{` |
|        - |  8736 | `		ph7_user_func *pFunc;` |
|        - |  8737 | `		ph7_context sCtx;` |
|        - |  8738 | `		ph7_value sRet;` |
|        - |  8739 | `		/* Look for an installed foreign function.` |
|        - |  8740 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  8741 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - |  8742 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - |  8743 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   651372 |  8744 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8745 | `		{` |
|   651372 |  8746 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   651372 |  8747 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - |  8748 | `			/* Compiler-qualified: try short name as global fallback */` |
|       22 |  8749 | `			const char *zShort = sName.zString;` |
|        - |  8750 | `			sxu32 i;` |
|      334 |  8751 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      314 |  8752 | `				if( sName.zString[i] == '\\' ){` |
|       28 |  8753 | `					zShort = &sName.zString[i + 1];` |
|       13 |  8754 | `				}` |
|      158 |  8755 | `			}` |
|       22 |  8756 | `			if( zShort != sName.zString ){` |
|       22 |  8757 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       22 |  8758 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 |  8759 | `			}` |
|       10 |  8760 | `		}` |
|        - |  8761 | `		} /* end VmCallArgMap namespace scope */` |
|   651372 |  8762 | `		if( pEntry == 0 ){` |
|        - |  8763 | `			/* Call to undefined function */` |
|        5 |  8764 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  8765 | `			/* Pop given arguments */` |
|        5 |  8766 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  8767 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8768 | `			}` |
|        - |  8769 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  8770 | `			PH7_MemObjRelease(pTos);` |
|        9 |  8771 | `			break;` |
|        - |  8772 | `		}` |
|   651368 |  8773 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  8774 | `		/* Start collecting function arguments */` |
|   651368 |  8775 | `		SySetReset(&aArg);` |
|  1753276 |  8776 | `		while( pArg < pTos ){` |
|  1101910 |  8777 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1101910 |  8778 | `			pArg++;` |
|        2 |  8779 | `		}` |
|        - |  8780 | `		/* Assume a null return value */` |
|   651368 |  8781 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  8782 | `		/* Init the call context */` |
|   651368 |  8783 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  8784 | `		/* Call the foreign function */` |
|   651368 |  8785 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  8786 | `		/* Release the call context */` |
|   651368 |  8787 | `		VmReleaseCallContext(&sCtx);` |
|   651368 |  8788 | `		if( rc == PH7_ABORT ){` |
|      471 |  8789 | `			goto Abort;` |
|   650898 |  8790 | `		}else if( rc == PH7_EXCEPTION ){` |
|       14 |  8791 | `			VmFrame *pFrm = pVm->pFrame;` |
|       14 |  8792 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       14 |  8793 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  8794 | `				/* Exception was NOT caught, propagate */` |
|        5 |  8795 | `				goto Exception;` |
|        - |  8796 | `			}` |
|        - |  8797 | `			/* Exception was caught: pop args and the result slot */` |
|        9 |  8798 | `			PH7_MemObjRelease(&sRet);` |
|        9 |  8799 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  8800 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  8801 | `			}` |
|        - |  8802 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        9 |  8803 | `			VmPopOperand(&pTos,1);` |
|        - |  8804 | `			/* Jump past the try/catch block via the exception frame */` |
|        9 |  8805 | `			pFrm = pVm->pFrame;` |
|        9 |  8806 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        9 |  8807 | `				pc = pFrm->iExceptionJump - 1;` |
|        4 |  8808 | `			}` |
|        9 |  8809 | `			break;` |
|        - |  8810 | `		}` |
|   650886 |  8811 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  8812 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  8813 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  8814 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  8815 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  8816 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  8817 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  8818 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  8819 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  8820 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  8821 | `			}` |
|        - |  8822 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  8823 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  8824 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  8825 | `			goto Suspend;` |
|        - |  8826 | `		}` |
|   650848 |  8827 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8828 | `			/* Pop function name and arguments */` |
|   630342 |  8829 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   315192 |  8830 | `		}` |
|        - |  8831 | `		/* Save foreign function return value */` |
|   650848 |  8832 | `		PH7_MemObjStore(&sRet,pTos);` |
|   650848 |  8833 | `		PH7_MemObjRelease(&sRet);` |
|        - |  8834 | `	}` |
|   667480 |  8835 | `	break;` |
|        - |  8836 | `				  }` |
|        - |  8837 | `/*` |
|        - |  8838 | ` * OP_CONSUME: P1 * *` |
|        - |  8839 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  8840 | ` */` |
|    13944 |  8841 | `case PH7_OP_CONSUME: {` |
|    27890 |  8842 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    27890 |  8843 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  8844 |  |
|    27890 |  8845 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    27890 |  8846 | `	pCur = pOut;` |
|        - |  8847 | `	/* Start the consume process  */` |
|    55778 |  8848 | `	while( pOut <= pTos ){` |
|        - |  8849 | `		/* Force a string cast */` |
|    27890 |  8850 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      650 |  8851 | `			PH7_MemObjToString(pOut);` |
|      324 |  8852 | `		}` |
|    27890 |  8853 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  8854 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  8855 | `			/* Invoke the output consumer callback */` |
|    16310 |  8856 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    16310 |  8857 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    16310 |  8858 | `			SyBlobRelease(&pOut->sBlob);` |
|    16310 |  8859 | `			if( rc == SXERR_ABORT ){` |
|        - |  8860 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  8861 | `				goto Abort;` |
|        - |  8862 | `			}` |
|     8154 |  8863 | `		}` |
|    27890 |  8864 | `		pOut++;` |
|        2 |  8865 | `	}` |
|    27890 |  8866 | `	pTos = &pCur[-1];` |
|    27888 |  8867 | `	break;` |
|        - |  8868 | `					 }` |
|        - |  8869 |  |
|        - |  8870 | `		} /* Switch() */` |
| 11141510 |  8871 | `		pc++; /* Next instruction in the stream */` |
|        2 |  8872 | `	} /* For(;;) */` |
|    19828 |  8873 | `Done:` |
|    39658 |  8874 | `	SySetRelease(&aArg);` |
|    39658 |  8875 | `	return SXRET_OK;` |
|       72 |  8876 | `Suspend:` |
|      146 |  8877 | `	SySetRelease(&aArg);` |
|      146 |  8878 | `	return PH7_SUSPEND;` |
|      259 |  8879 | `Abort:` |
|      519 |  8880 | `	SySetRelease(&aArg);` |
|     1767 |  8881 | `	while( pTos >= pStack ){` |
|     1249 |  8882 | `		PH7_MemObjRelease(pTos);` |
|     1249 |  8883 | `		pTos--;` |
|        1 |  8884 | `	}` |
|      519 |  8885 | `	return PH7_ABORT;` |
|        3 |  8886 | `Exception:` |
|        8 |  8887 | `	SySetRelease(&aArg);` |
|       22 |  8888 | `	while( pTos >= pStack ){` |
|       16 |  8889 | `		PH7_MemObjRelease(pTos);` |
|       16 |  8890 | `		pTos--;` |
|        2 |  8891 | `	}` |
|        8 |  8892 | `	return PH7_EXCEPTION;` |
|    20164 |  8893 |  |
|        - |  8894 | `/*` |
|        - |  8895 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  8896 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  8897 | ` * See block-comment on that function for additional information.` |
|        - |  8898 | ` */` |
|    18590 |  8899 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  8900 |  |
|        - |  8901 | `	ph7_value *pStack;` |
|        - |  8902 | `	sxi32 rc;` |
|        - |  8903 | `	/* Allocate a new operand stack */` |
|    18592 |  8904 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    18592 |  8905 | `	if( pStack == 0 ){` |
|      ! 0 |  8906 | `		return SXERR_MEM;` |
|        - |  8907 | `	}` |
|        - |  8908 | `	/* Execute the program */` |
|    18592 |  8909 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0);` |
|        - |  8910 | `	/* Free the operand stack */` |
|    18592 |  8911 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  8912 | `	/* Execution result */` |
|    18592 |  8913 | `	return rc;` |
|     9297 |  8914 |  |
|        - |  8915 | `/*` |
|        - |  8916 | ` * Invoke any installed shutdown callbacks.` |
|        - |  8917 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  8918 | ` * or more calls to [register_shutdown_function()].` |
|        - |  8919 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  8920 | ` * execution ends.` |
|        - |  8921 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  8922 | ` * additional information.` |
|        - |  8923 | ` */` |
|     2614 |  8924 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  8925 |  |
|        - |  8926 | `	VmShutdownCB *pEntry;` |
|        - |  8927 | `	ph7_value *apArg[10];` |
|        - |  8928 | `	sxu32 n,nEntry;` |
|        - |  8929 | `	int i;` |
|        - |  8930 | `	/* Point to the stack of registered callbacks */` |
|     2616 |  8931 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    28756 |  8932 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    26142 |  8933 | `		apArg[i] = 0;` |
|    13072 |  8934 | `	}` |
|     2618 |  8935 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  8936 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  8937 | `		if( pEntry ){` |
|        - |  8938 | `			/* Prepare callback arguments if any */` |
|        3 |  8939 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  8940 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  8941 | `					break;` |
|        - |  8942 | `				}` |
|      ! 0 |  8943 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  8944 | `			}` |
|        - |  8945 | `			/* Invoke the callback */` |
|        3 |  8946 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  8947 | `			/*` |
|        - |  8948 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  8949 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  8950 | `			 */` |
|        3 |  8951 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  8952 | `			if( pEntry ){` |
|        3 |  8953 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  8954 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  8955 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  8956 | `				}` |
|        1 |  8957 | `			}` |
|        1 |  8958 | `		}` |
|        2 |  8959 | `	}` |
|     2616 |  8960 | `	SySetReset(&pVm->aShutdown);` |
|     2616 |  8961 |  |
|        - |  8962 | `/*` |
|        - |  8963 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  8964 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  8965 | ` * See block-comment on that function for additional information.` |
|        - |  8966 | ` */` |
|     2622 |  8967 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  8968 |  |
|        - |  8969 | `	/* Make sure we are ready to execute this program */` |
|     2624 |  8970 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  8971 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  8972 | `	}` |
|        - |  8973 | `	/* Set the execution magic number  */` |
|     2624 |  8974 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  8975 | `	/* Execute the program */` |
|     2624 |  8976 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0);` |
|        - |  8977 | `	/* Invoke any shutdown callbacks */` |
|     2620 |  8978 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  8979 | `	/*` |
|        - |  8980 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  8981 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  8982 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  8983 | `	 */` |
|     2620 |  8984 | `	return SXRET_OK;` |
|     1313 |  8985 |  |
|        - |  8986 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  8987 | `/*` |
|        - |  8988 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  8989 | ` * The context is in CREATED state and ready to be started.` |
|        - |  8990 | ` */` |
|       46 |  8991 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  8992 |  |
|        - |  8993 | `	ph7_exec_ctx *pCtx;` |
|        - |  8994 | `	ph7_value *pStack;` |
|        - |  8995 | `	VmFrame *pFrame;` |
|       48 |  8996 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       48 |  8997 | `	if( pCtx == 0 ){` |
|      ! 0 |  8998 | `		return 0;` |
|        - |  8999 | `	}` |
|       48 |  9000 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       48 |  9001 | `	pCtx->pVm = pVm;` |
|       48 |  9002 | `	pCtx->pFunc = pFunc;` |
|       48 |  9003 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       48 |  9004 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       48 |  9005 | `	pCtx->pc = 0;` |
|       48 |  9006 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       48 |  9007 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  9008 | `	/* Allocate a private operand stack */` |
|       48 |  9009 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       48 |  9010 | `	if( pStack == 0 ){` |
|      ! 0 |  9011 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9012 | `		return 0;` |
|        - |  9013 | `	}` |
|       48 |  9014 | `	pCtx->pStack = pStack;` |
|        - |  9015 | `	/* Create a detached frame for the fiber */` |
|       48 |  9016 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       48 |  9017 | `	if( pFrame == 0 ){` |
|      ! 0 |  9018 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  9019 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9020 | `		return 0;` |
|        - |  9021 | `	}` |
|       48 |  9022 | `	pCtx->pFrame = pFrame;` |
|       48 |  9023 | `	return pCtx;` |
|       25 |  9024 |  |
|        - |  9025 | `/*` |
|        - |  9026 | ` * Start executing a fiber context for the first time.` |
|        - |  9027 | ` */` |
|       46 |  9028 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  9029 |  |
|        - |  9030 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9031 | `	sxi32 rc;` |
|       48 |  9032 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9033 | `		return SXERR_INVALID;` |
|        - |  9034 | `	}` |
|        - |  9035 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 |  9036 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 |  9037 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9038 | `	/* Save and set the active context */` |
|       48 |  9039 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 |  9040 | `	pVm->pActiveCtx = pCtx;` |
|       48 |  9041 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 |  9042 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 |  9043 | `	pVm->nRecursionDepth++;` |
|        - |  9044 | `	/* Execute from the beginning */` |
|       48 |  9045 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 |  9046 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       46 |  9047 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|       48 |  9048 | `	pVm->nRecursionDepth--;` |
|        - |  9049 | `	/* Restore the previous context */` |
|       48 |  9050 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 |  9051 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9052 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 |  9053 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 |  9054 | `		pCtx->pFrame->pParent = 0;` |
|       46 |  9055 | `		if( pResult ){` |
|       24 |  9056 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  9057 | `		}` |
|       46 |  9058 | `		return SXRET_OK;` |
|        - |  9059 | `	}` |
|        - |  9060 | `	/* Detach frame */` |
|        3 |  9061 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  9062 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  9063 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  9064 | `	}` |
|        3 |  9065 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9066 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9067 | `		return PH7_ABORT;` |
|        - |  9068 | `	}` |
|        3 |  9069 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9070 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9071 | `		return PH7_EXCEPTION;` |
|        - |  9072 | `	}` |
|        - |  9073 | `	/* Normal completion */` |
|        3 |  9074 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  9075 | `	if( pResult ){` |
|        3 |  9076 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  9077 | `	}` |
|        3 |  9078 | `	return SXRET_OK;` |
|       25 |  9079 |  |
|        - |  9080 | `/*` |
|        - |  9081 | ` * Resume a suspended fiber context.` |
|        - |  9082 | ` */` |
|       98 |  9083 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  9084 |  |
|        - |  9085 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9086 | `	sxi32 rc;` |
|      100 |  9087 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  9088 | `		return SXERR_INVALID;` |
|        - |  9089 | `	}` |
|        - |  9090 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  9091 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  9092 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 |  9093 | `	if( pResumeValue ){` |
|       40 |  9094 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  9095 | `	}else{` |
|       62 |  9096 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  9097 | `	}` |
|      100 |  9098 | `	pCtx->nTos++;` |
|        - |  9099 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 |  9100 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 |  9101 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9102 | `	/* Save and set the active context */` |
|      100 |  9103 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 |  9104 | `	pVm->pActiveCtx = pCtx;` |
|      100 |  9105 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 |  9106 | `	pVm->nRecursionDepth++;` |
|        - |  9107 | `	/* Resume execution from saved PC */` |
|      100 |  9108 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 |  9109 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|       98 |  9110 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|      100 |  9111 | `	pVm->nRecursionDepth--;` |
|        - |  9112 | `	/* Restore the previous context */` |
|      100 |  9113 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 |  9114 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9115 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 |  9116 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 |  9117 | `		pCtx->pFrame->pParent = 0;` |
|       64 |  9118 | `		if( pResult ){` |
|       18 |  9119 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  9120 | `		}` |
|       64 |  9121 | `		return SXRET_OK;` |
|        - |  9122 | `	}` |
|        - |  9123 | `	/* Detach frame */` |
|       38 |  9124 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 |  9125 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 |  9126 | `		pCtx->pFrame->pParent = 0;` |
|       18 |  9127 | `	}` |
|       38 |  9128 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9129 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9130 | `		return PH7_ABORT;` |
|        - |  9131 | `	}` |
|       38 |  9132 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9133 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9134 | `		return PH7_EXCEPTION;` |
|        - |  9135 | `	}` |
|        - |  9136 | `	/* Normal completion */` |
|       38 |  9137 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 |  9138 | `	if( pResult ){` |
|       20 |  9139 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  9140 | `	}` |
|       38 |  9141 | `	return SXRET_OK;` |
|       51 |  9142 |  |
|        - |  9143 | `/*` |
|        - |  9144 | ` * Release an execution context and all its resources.` |
|        - |  9145 | ` */` |
|        4 |  9146 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  9147 |  |
|        5 |  9148 | `	if( pCtx == 0 ){` |
|      ! 0 |  9149 | `		return;` |
|        - |  9150 | `	}` |
|        5 |  9151 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  9152 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  9153 | `		return;` |
|        - |  9154 | `	}` |
|        5 |  9155 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  9156 | `	/* Release values */` |
|        5 |  9157 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  9158 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  9159 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  9160 | `	if( pCtx->pFrame ){` |
|        - |  9161 | `		VmSlot *aSlot;` |
|        - |  9162 | `		sxu32 n;` |
|        - |  9163 | `		/* Free local variables */` |
|        5 |  9164 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  9165 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  9166 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  9167 | `		}` |
|        - |  9168 | `		/* Remove local references */` |
|        5 |  9169 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  9170 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  9171 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  9172 | `		}` |
|        5 |  9173 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  9174 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  9175 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  9176 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  9177 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  9178 | `		pCtx->pFrame = 0;` |
|        2 |  9179 | `	}` |
|        - |  9180 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  9181 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  9182 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  9183 | `	if( pCtx->pStack ){` |
|        5 |  9184 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  9185 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  9186 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  9187 | `				PH7_MemObjRelease(pTos);` |
|        5 |  9188 | `				pTos--;` |
|        1 |  9189 | `			}` |
|        2 |  9190 | `		}` |
|        5 |  9191 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  9192 | `		pCtx->pStack = 0;` |
|        2 |  9193 | `	}` |
|        - |  9194 | `	/* Free the context itself */` |
|        5 |  9195 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  9196 |  |
|        - |  9197 | `/*` |
|        - |  9198 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  9199 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  9200 | ` */` |
|       90 |  9201 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  9202 |  |
|        - |  9203 | `	ph7_class_instance *pThis;` |
|        - |  9204 | `	SyString sAttr;` |
|        - |  9205 | `	ph7_value *pAttr;` |
|       92 |  9206 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9207 | `		return 0;` |
|        - |  9208 | `	}` |
|       92 |  9209 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  9210 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  9211 | `		return 0;` |
|        - |  9212 | `	}` |
|       92 |  9213 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  9214 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  9215 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  9216 | `		return 0;` |
|        - |  9217 | `	}` |
|       62 |  9218 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  9219 |  |
|        - |  9220 | `/*` |
|        - |  9221 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  9222 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  9223 | ` */` |
|       38 |  9224 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9225 |  |
|       40 |  9226 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  9227 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  9228 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9229 | `			"Cannot suspend outside of a fiber");` |
|        - |  9230 | `	}` |
|       40 |  9231 | `	if( nArg > 0 ){` |
|       40 |  9232 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  9233 | `	}else{` |
|      ! 0 |  9234 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  9235 | `	}` |
|       40 |  9236 | `	return PH7_SUSPEND;` |
|       21 |  9237 |  |
|        - |  9238 | `/*` |
|        - |  9239 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  9240 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  9241 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  9242 | ` */` |
|       24 |  9243 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9244 |  |
|        - |  9245 | `	ph7_class_instance *pThis;` |
|        - |  9246 | `	ph7_value *pAttr;` |
|        - |  9247 | `	SyString sAttrName;` |
|       26 |  9248 | `	if( nArg < 2 ){` |
|      ! 0 |  9249 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9250 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  9251 | `	}` |
|       26 |  9252 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9253 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9254 | `			"Fiber::__construct(): invalid $this");` |
|        - |  9255 | `	}` |
|       26 |  9256 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  9257 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  9258 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9259 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  9260 | `	}` |
|        - |  9261 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  9262 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  9263 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9264 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  9265 | `	}` |
|        - |  9266 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  9267 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  9268 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  9269 | `	if( pAttr ){` |
|       26 |  9270 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  9271 | `	}` |
|       26 |  9272 | `	return PH7_OK;` |
|       14 |  9273 |  |
|        - |  9274 | `/*` |
|        - |  9275 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  9276 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  9277 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  9278 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  9279 | ` */` |
|       24 |  9280 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  9281 | `	ph7_class_instance **ppThis)` |
|        2 |  9282 |  |
|       26 |  9283 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9284 | `	ph7_value *pCallable;` |
|        - |  9285 | `	SyString sAttrName;` |
|       26 |  9286 | `	*ppThis = 0;` |
|       26 |  9287 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  9288 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  9289 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  9290 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  9291 | `		return 0;` |
|        - |  9292 | `	}` |
|       26 |  9293 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  9294 | `		/* String callable — look up in user functions with overload support */` |
|        - |  9295 | `		SyString sName;` |
|        - |  9296 | `		SyHashEntry *pEntry;` |
|        - |  9297 | `		ph7_vm_func *pFunc;` |
|       26 |  9298 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  9299 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  9300 | `		if( pEntry == 0 ){` |
|      ! 0 |  9301 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  9302 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  9303 | `			return 0;` |
|        - |  9304 | `		}` |
|       26 |  9305 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  9306 | `		return pFunc;` |
|      ! 0 |  9307 | `	}else{` |
|        - |  9308 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  9309 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  9310 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  9311 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  9312 | `		if( pMethod == 0 ){` |
|      ! 0 |  9313 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9314 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  9315 | `			return 0;` |
|        - |  9316 | `		}` |
|      ! 0 |  9317 | `		*ppThis = pClosure;` |
|      ! 0 |  9318 | `		return &pMethod->sFunc;` |
|        - |  9319 | `	}` |
|       14 |  9320 |  |
|        - |  9321 | `/*` |
|        - |  9322 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  9323 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  9324 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  9325 | ` */` |
|       46 |  9326 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  9327 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  9328 |  |
|       48 |  9329 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  9330 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  9331 | `	sxu32 nFormal, n;` |
|        - |  9332 | `	VmSlot sSlot;` |
|        - |  9333 | `	sxi32 rc;` |
|        - |  9334 | `	/* Install $this for closure/method callables */` |
|       48 |  9335 | `	if( pClosureThis ){` |
|        - |  9336 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  9337 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  9338 | `		if( pObj ){` |
|      ! 0 |  9339 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  9340 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  9341 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  9342 | `		}` |
|      ! 0 |  9343 | `	}` |
|        - |  9344 | `	/* Install static variables */` |
|       48 |  9345 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  9346 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  9347 | `		ph7_value *pVal;` |
|      ! 0 |  9348 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  9349 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  9350 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  9351 | `			if( pVal ){` |
|      ! 0 |  9352 | `				sSlot.pUserData = 0;` |
|      ! 0 |  9353 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  9354 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  9355 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  9356 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  9357 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  9358 | `				}` |
|      ! 0 |  9359 | `			}` |
|      ! 0 |  9360 | `		}` |
|      ! 0 |  9361 | `	}` |
|        - |  9362 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       48 |  9363 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       48 |  9364 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       66 |  9365 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  9366 | `		ph7_value *pObj;` |
|       20 |  9367 | `		if( n < (sxu32)nArg ){` |
|        - |  9368 | `			/* Argument provided — install with type casting */` |
|       20 |  9369 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 |  9370 | `			if( pObj ){` |
|       20 |  9371 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  9372 | `				/* Type casting */` |
|       20 |  9373 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  9374 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9375 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9376 | `						if( xCast ){` |
|      ! 0 |  9377 | `							xCast(pObj);` |
|      ! 0 |  9378 | `						}` |
|      ! 0 |  9379 | `					}` |
|      ! 0 |  9380 | `				}` |
|       20 |  9381 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 |  9382 | `				sSlot.pUserData = 0;` |
|       20 |  9383 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 |  9384 | `			}` |
|        9 |  9385 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  9386 | `			/* Default value */` |
|      ! 0 |  9387 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  9388 | `			if( pObj ){` |
|      ! 0 |  9389 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  9390 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9391 | `					return rc;` |
|        - |  9392 | `				}` |
|      ! 0 |  9393 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  9394 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9395 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9396 | `						if( xCast ){` |
|      ! 0 |  9397 | `							xCast(pObj);` |
|      ! 0 |  9398 | `						}` |
|      ! 0 |  9399 | `					}` |
|      ! 0 |  9400 | `				}` |
|      ! 0 |  9401 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  9402 | `				sSlot.pUserData = 0;` |
|      ! 0 |  9403 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  9404 | `			}` |
|      ! 0 |  9405 | `		}` |
|       11 |  9406 | `	}` |
|        - |  9407 | `	/* Install closure environment (captured variables) */` |
|       48 |  9408 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  9409 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  9410 | `		ph7_value *pValue;` |
|        - |  9411 | `		sxu32 iEnv;` |
|        3 |  9412 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  9413 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  9414 | `			pEnv = &aEnv[iEnv];` |
|        7 |  9415 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  9416 | `				continue;` |
|        - |  9417 | `			}` |
|        5 |  9418 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  9419 | `			if( pValue == 0 ){` |
|      ! 0 |  9420 | `				continue;` |
|        - |  9421 | `			}` |
|        5 |  9422 | `			PH7_MemObjRelease(pValue);` |
|        5 |  9423 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  9424 | `		}` |
|        1 |  9425 | `	}` |
|       48 |  9426 | `	return SXRET_OK;` |
|       25 |  9427 |  |
|        - |  9428 | `/*` |
|        - |  9429 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  9430 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  9431 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  9432 | ` */` |
|       26 |  9433 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9434 |  |
|       28 |  9435 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9436 | `	ph7_class_instance *pThis;` |
|        - |  9437 | `	ph7_class_instance *pClosureThis;` |
|        - |  9438 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  9439 | `	ph7_vm_func *pFunc;` |
|        - |  9440 | `	ph7_value sResult;` |
|        - |  9441 | `	ph7_value *pCtxAttr;` |
|        - |  9442 | `	SyString sAttrName;` |
|        - |  9443 | `	sxi32 rc;` |
|       28 |  9444 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9445 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  9446 | `	}` |
|       28 |  9447 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  9448 | `	/* Check if already started (has a __ctx) */` |
|       28 |  9449 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 |  9450 | `	if( pExecCtx != 0 ){` |
|        3 |  9451 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9452 | `			"Cannot start a fiber that has already been started");` |
|        - |  9453 | `	}` |
|        - |  9454 | `	/* Resolve callable */` |
|       26 |  9455 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 |  9456 | `	if( pFunc == 0 ){` |
|      ! 0 |  9457 | `		return PH7_EXCEPTION;` |
|        - |  9458 | `	}` |
|        - |  9459 | `	/* Create execution context now that we know the function */` |
|       26 |  9460 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 |  9461 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  9462 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9463 | `			"Fiber::start(): out of memory");` |
|        - |  9464 | `	}` |
|        - |  9465 | `	/* Store context in $this->__ctx */` |
|       26 |  9466 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 |  9467 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  9468 | `	if( pCtxAttr ){` |
|       26 |  9469 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 |  9470 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  9471 | `	}` |
|        - |  9472 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  9473 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  9474 | `	 * into the fiber's frame, not the caller's. */` |
|       26 |  9475 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 |  9476 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  9477 | `	/* Unpack the args array and install into the frame */` |
|        - |  9478 | `	{` |
|       26 |  9479 | `		ph7_value **apValues = 0;` |
|       26 |  9480 | `		int nActual = 0;` |
|       26 |  9481 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 |  9482 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  9483 | `			ph7_hashmap_node *pNode;` |
|       26 |  9484 | `			sxu32 nCount = pMap->nEntry;` |
|       26 |  9485 | `			if( nCount > 0 ){` |
|        3 |  9486 | `				sxu32 idx = 0;` |
|        4 |  9487 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  9488 | `					nCount * sizeof(ph7_value *));` |
|        3 |  9489 | `				if( apValues ){` |
|        3 |  9490 | `					pNode = pMap->pFirst;` |
|        7 |  9491 | `					while( pNode && idx < nCount ){` |
|        5 |  9492 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  9493 | `						idx++;` |
|        5 |  9494 | `						pNode = pNode->pPrev;` |
|        1 |  9495 | `					}` |
|        3 |  9496 | `					nActual = (int)idx;` |
|        1 |  9497 | `				}` |
|        1 |  9498 | `			}` |
|       12 |  9499 | `		}` |
|       26 |  9500 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 |  9501 | `		if( apValues ){` |
|        3 |  9502 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  9503 | `		}` |
|        - |  9504 | `	}` |
|        - |  9505 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 |  9506 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 |  9507 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 |  9508 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9509 | `		return PH7_ABORT;` |
|        - |  9510 | `	}` |
|       26 |  9511 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 |  9512 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 |  9513 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9514 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9515 | `		return PH7_ABORT;` |
|        - |  9516 | `	}` |
|       26 |  9517 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9518 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9519 | `		return PH7_EXCEPTION;` |
|        - |  9520 | `	}` |
|       26 |  9521 | `	ph7_result_value(pCtx, &sResult);` |
|       26 |  9522 | `	PH7_MemObjRelease(&sResult);` |
|       26 |  9523 | `	return PH7_OK;` |
|       15 |  9524 |  |
|        - |  9525 | `/*` |
|        - |  9526 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  9527 | ` */` |
|       36 |  9528 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9529 |  |
|       38 |  9530 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9531 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  9532 | `	ph7_value sResult;` |
|        - |  9533 | `	ph7_value *pResumeVal;` |
|        - |  9534 | `	sxi32 rc;` |
|       38 |  9535 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9536 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  9537 | `		return PH7_OK;` |
|        - |  9538 | `	}` |
|       38 |  9539 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 |  9540 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  9541 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  9542 | `		return PH7_OK;` |
|        - |  9543 | `	}` |
|       38 |  9544 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  9545 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9546 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  9547 | `	}` |
|       36 |  9548 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 |  9549 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 |  9550 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 |  9551 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9552 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9553 | `		return PH7_ABORT;` |
|        - |  9554 | `	}` |
|       36 |  9555 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9556 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9557 | `		return PH7_EXCEPTION;` |
|        - |  9558 | `	}` |
|       36 |  9559 | `	ph7_result_value(pCtx, &sResult);` |
|       36 |  9560 | `	PH7_MemObjRelease(&sResult);` |
|       36 |  9561 | `	return PH7_OK;` |
|       20 |  9562 |  |
|        - |  9563 | `/*` |
|        - |  9564 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  9565 | ` */` |
|        6 |  9566 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9567 |  |
|        8 |  9568 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9569 | `	ph7_exec_ctx *pExecCtx;` |
|        8 |  9570 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9571 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9572 | `		return PH7_OK;` |
|        - |  9573 | `	}` |
|        8 |  9574 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 |  9575 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  9576 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9577 | `		return PH7_OK;` |
|        - |  9578 | `	}` |
|        8 |  9579 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  9580 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9581 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9582 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  9583 | `		}` |
|      ! 0 |  9584 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9585 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  9586 | `	}` |
|        8 |  9587 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 |  9588 | `	return PH7_OK;` |
|        5 |  9589 |  |
|        - |  9590 | `/*` |
|        - |  9591 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  9592 | ` */` |
|        6 |  9593 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9594 |  |
|        - |  9595 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9596 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9597 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9598 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  9599 | `	return PH7_OK;` |
|        4 |  9600 |  |
|      ! 0 |  9601 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  9602 |  |
|        - |  9603 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  9604 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  9605 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  9606 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  9607 | `	return PH7_OK;` |
|      ! 0 |  9608 |  |
|        6 |  9609 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9610 |  |
|        - |  9611 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9612 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9613 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9614 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  9615 | `	return PH7_OK;` |
|        4 |  9616 |  |
|        6 |  9617 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9618 |  |
|        - |  9619 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9620 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9621 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9622 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  9623 | `	return PH7_OK;` |
|        4 |  9624 |  |
|        - |  9625 | `/*` |
|        - |  9626 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  9627 | ` */` |
|        4 |  9628 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9629 |  |
|        5 |  9630 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9631 | `	ph7_exec_ctx *pExecCtx;` |
|        5 |  9632 | `	if( nArg < 1 ){` |
|      ! 0 |  9633 | `		return PH7_OK;` |
|        - |  9634 | `	}` |
|        5 |  9635 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 |  9636 | `	if( pExecCtx ){` |
|        5 |  9637 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  9638 | `		/* Clear the attribute so double-free is prevented */` |
|        5 |  9639 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 |  9640 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  9641 | `			SyString sAttrName;` |
|        - |  9642 | `			ph7_value *pAttr;` |
|        5 |  9643 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 |  9644 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 |  9645 | `			if( pAttr ){` |
|        5 |  9646 | `				PH7_MemObjRelease(pAttr);` |
|        2 |  9647 | `			}` |
|        2 |  9648 | `		}` |
|        2 |  9649 | `	}` |
|        5 |  9650 | `	return PH7_OK;` |
|        3 |  9651 |  |
|        - |  9652 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  9653 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  9654 |  |
|        - |  9655 | `	ph7_class_instance *pThis;` |
|      ! 0 |  9656 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  9657 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  9658 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  9659 |  |
|      ! 0 |  9660 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  9661 |  |
|        - |  9662 | `	ph7_class_instance *pThis;` |
|      ! 0 |  9663 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  9664 | `	ph7_exec_ctx *pCtx;` |
|        - |  9665 | `	ph7_vm_func *pFunc;` |
|        - |  9666 | `	ph7_value *pCallable;` |
|        - |  9667 | `	ph7_value *pCtxAttr;` |
|        - |  9668 | `	SyString sAttrName;` |
|        - |  9669 | `	/* Must not already be started */` |
|      ! 0 |  9670 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9671 | `	if( pCtx != 0 ){` |
|      ! 0 |  9672 | `		return SXERR_INVALID;` |
|        - |  9673 | `	}` |
|      ! 0 |  9674 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9675 | `		return SXERR_INVALID;` |
|        - |  9676 | `	}` |
|      ! 0 |  9677 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  9678 | `	/* Get the callable */` |
|      ! 0 |  9679 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  9680 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9681 | `	if( pCallable == 0 ){` |
|      ! 0 |  9682 | `		return SXERR_INVALID;` |
|        - |  9683 | `	}` |
|        - |  9684 | `	/* Resolve callable */` |
|      ! 0 |  9685 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  9686 | `		SyString sName;` |
|        - |  9687 | `		SyHashEntry *pEntry;` |
|      ! 0 |  9688 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  9689 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  9690 | `		if( pEntry == 0 ){` |
|      ! 0 |  9691 | `			return SXERR_NOTFOUND;` |
|        - |  9692 | `		}` |
|      ! 0 |  9693 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  9694 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9695 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  9696 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  9697 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  9698 | `		if( pMethod == 0 ){` |
|      ! 0 |  9699 | `			return SXERR_INVALID;` |
|        - |  9700 | `		}` |
|      ! 0 |  9701 | `		pClosureThis = pClosure;` |
|      ! 0 |  9702 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  9703 | `	}else{` |
|      ! 0 |  9704 | `		return SXERR_INVALID;` |
|        - |  9705 | `	}` |
|        - |  9706 | `	/* Create context */` |
|      ! 0 |  9707 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  9708 | `	if( pCtx == 0 ){` |
|      ! 0 |  9709 | `		return SXERR_MEM;` |
|        - |  9710 | `	}` |
|        - |  9711 | `	/* Store in __ctx */` |
|      ! 0 |  9712 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  9713 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9714 | `	if( pCtxAttr ){` |
|      ! 0 |  9715 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  9716 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  9717 | `	}` |
|        - |  9718 | `	/* Set up frame with args */` |
|      ! 0 |  9719 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  9720 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  9721 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  9722 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  9723 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  9724 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  9725 |  |
|      ! 0 |  9726 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  9727 |  |
|      ! 0 |  9728 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9729 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  9730 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  9731 |  |
|      ! 0 |  9732 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9733 |  |
|      ! 0 |  9734 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9735 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  9736 |  |
|      ! 0 |  9737 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9738 |  |
|      ! 0 |  9739 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9740 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  9741 |  |
|      ! 0 |  9742 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9743 |  |
|      ! 0 |  9744 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9745 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  9746 | `	return &pCtx->sRetValue;` |
|      ! 0 |  9747 |  |
|        - |  9748 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  9749 | `/*` |
|        - |  9750 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  9751 | ` */` |
|       22 |  9752 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 |  9753 |  |
|        - |  9754 | `	ph7_generator *pGen;` |
|       24 |  9755 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       24 |  9756 | `	if( pGen == 0 ){` |
|      ! 0 |  9757 | `		return 0;` |
|        - |  9758 | `	}` |
|       24 |  9759 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       24 |  9760 | `	pGen->pCtx = pCtx;` |
|       24 |  9761 | `	pGen->iImplicitKey = 0;` |
|       24 |  9762 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       24 |  9763 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  9764 | `	/* Link the generator back to the exec context */` |
|       24 |  9765 | `	pCtx->pPrivate = pGen;` |
|       24 |  9766 | `	return pGen;` |
|       13 |  9767 |  |
|        - |  9768 | `/*` |
|        - |  9769 | ` * Release a generator and its execution context.` |
|        - |  9770 | ` */` |
|      ! 0 |  9771 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  9772 |  |
|      ! 0 |  9773 | `	if( pGen == 0 ){` |
|      ! 0 |  9774 | `		return;` |
|        - |  9775 | `	}` |
|      ! 0 |  9776 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  9777 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  9778 | `	if( pGen->pCtx ){` |
|      ! 0 |  9779 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  9780 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  9781 | `		pGen->pCtx = 0;` |
|      ! 0 |  9782 | `	}` |
|      ! 0 |  9783 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  9784 |  |
|        - |  9785 | `/*` |
|        - |  9786 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  9787 | ` */` |
|      236 |  9788 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 |  9789 |  |
|        - |  9790 | `	ph7_class_instance *pThis;` |
|        - |  9791 | `	SyString sAttr;` |
|        - |  9792 | `	ph7_value *pAttr;` |
|      238 |  9793 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9794 | `		return 0;` |
|        - |  9795 | `	}` |
|      238 |  9796 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 |  9797 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  9798 | `		return 0;` |
|        - |  9799 | `	}` |
|      238 |  9800 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 |  9801 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 |  9802 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  9803 | `		return 0;` |
|        - |  9804 | `	}` |
|      238 |  9805 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 |  9806 |  |
|        - |  9807 | `/*` |
|        - |  9808 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  9809 | ` */` |
|       22 |  9810 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9811 |  |
|        - |  9812 | `	ph7_generator *pGen;` |
|        - |  9813 | `	sxi32 rc;` |
|       24 |  9814 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 |  9815 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 |  9816 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 |  9817 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 |  9818 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 |  9819 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 |  9820 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 |  9821 | `	}` |
|       24 |  9822 | `	return PH7_OK;` |
|       13 |  9823 |  |
|        - |  9824 | `/*` |
|        - |  9825 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  9826 | ` */` |
|       68 |  9827 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9828 |  |
|        - |  9829 | `	ph7_generator *pGen;` |
|       70 |  9830 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 |  9831 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 |  9832 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 |  9833 | `	return PH7_OK;` |
|       36 |  9834 |  |
|        - |  9835 | `/*` |
|        - |  9836 | ` * Generator::current() — return the last yielded value.` |
|        - |  9837 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  9838 | ` */` |
|       68 |  9839 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9840 |  |
|        - |  9841 | `	ph7_generator *pGen;` |
|        - |  9842 | `	sxi32 rc;` |
|       70 |  9843 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 |  9844 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 |  9845 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 |  9846 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9847 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  9848 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  9849 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  9850 | `	}` |
|       70 |  9851 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 |  9852 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 |  9853 | `	}else{` |
|      ! 0 |  9854 | `		ph7_result_null(pCtx);` |
|        - |  9855 | `	}` |
|       70 |  9856 | `	return PH7_OK;` |
|       36 |  9857 |  |
|        - |  9858 | `/*` |
|        - |  9859 | ` * Generator::key() — return the last yielded key.` |
|        - |  9860 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  9861 | ` */` |
|       12 |  9862 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9863 |  |
|        - |  9864 | `	ph7_generator *pGen;` |
|        - |  9865 | `	sxi32 rc;` |
|       13 |  9866 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  9867 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  9868 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  9869 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9870 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  9871 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  9872 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  9873 | `	}` |
|       13 |  9874 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  9875 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  9876 | `	}else{` |
|      ! 0 |  9877 | `		ph7_result_null(pCtx);` |
|        - |  9878 | `	}` |
|       13 |  9879 | `	return PH7_OK;` |
|        7 |  9880 |  |
|        - |  9881 | `/*` |
|        - |  9882 | ` * Generator::next() — advance to the next yield point.` |
|        - |  9883 | ` */` |
|       60 |  9884 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9885 |  |
|        - |  9886 | `	ph7_generator *pGen;` |
|        - |  9887 | `	sxi32 rc;` |
|       62 |  9888 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 |  9889 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 |  9890 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 |  9891 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9892 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 |  9893 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 |  9894 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 |  9895 | `	}else{` |
|      ! 0 |  9896 | `		return PH7_OK;` |
|        - |  9897 | `	}` |
|       62 |  9898 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 |  9899 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 |  9900 | `	return PH7_OK;` |
|       32 |  9901 |  |
|        - |  9902 | `/*` |
|        - |  9903 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  9904 | ` */` |
|        4 |  9905 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9906 |  |
|        - |  9907 | `	ph7_generator *pGen;` |
|        - |  9908 | `	ph7_value *pSendVal;` |
|        - |  9909 | `	sxi32 rc;` |
|        5 |  9910 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  9911 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  9912 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  9913 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  9914 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  9915 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  9916 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  9917 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  9918 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  9919 | `	}else{` |
|      ! 0 |  9920 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9921 | `		return PH7_OK;` |
|        - |  9922 | `	}` |
|        5 |  9923 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  9924 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  9925 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  9926 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  9927 | `	}else{` |
|        3 |  9928 | `		ph7_result_null(pCtx);` |
|        - |  9929 | `	}` |
|        5 |  9930 | `	return PH7_OK;` |
|        3 |  9931 |  |
|        - |  9932 | `/*` |
|        - |  9933 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  9934 | ` *` |
|        - |  9935 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  9936 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  9937 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  9938 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  9939 | ` * the exception to the caller.` |
|        - |  9940 | ` */` |
|      ! 0 |  9941 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  9942 |  |
|        - |  9943 | `	ph7_generator *pGen;` |
|        - |  9944 | `	const char *zMsg;` |
|        - |  9945 | `	int nLen;` |
|      ! 0 |  9946 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  9947 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  9948 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  9949 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  9950 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  9951 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  9952 | `			"Cannot throw into a closed generator");` |
|        - |  9953 | `	}` |
|        - |  9954 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  9955 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  9956 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  9957 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  9958 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9959 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  9960 | `	nLen = 0;` |
|      ! 0 |  9961 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  9962 | `		/* Try to get the exception's message */` |
|        - |  9963 | `		SyString sAttr;` |
|        - |  9964 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  9965 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  9966 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  9967 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  9968 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  9969 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  9970 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  9971 | `		}` |
|      ! 0 |  9972 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  9973 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  9974 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  9975 | `	}` |
|      ! 0 |  9976 | `	(void)nLen;` |
|      ! 0 |  9977 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  9978 |  |
|        - |  9979 | `/*` |
|        - |  9980 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  9981 | ` */` |
|        2 |  9982 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9983 |  |
|        - |  9984 | `	ph7_generator *pGen;` |
|        3 |  9985 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  9986 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  9987 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  9988 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  9989 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  9990 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  9991 | `	}` |
|        3 |  9992 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  9993 | `	return PH7_OK;` |
|        2 |  9994 |  |
|        - |  9995 | `/*` |
|        - |  9996 | ` * Generator::__destruct() — clean up.` |
|        - |  9997 | ` */` |
|      ! 0 |  9998 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  9999 |  |
|        - | 10000 | `	ph7_generator *pGen;` |
|      ! 0 | 10001 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 | 10002 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10003 | `	if( pGen ){` |
|      ! 0 | 10004 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 | 10005 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10006 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10007 | `			SyString sAttrName;` |
|        - | 10008 | `			ph7_value *pAttr;` |
|      ! 0 | 10009 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10010 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10011 | `			if( pAttr ){` |
|      ! 0 | 10012 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 | 10013 | `			}` |
|      ! 0 | 10014 | `		}` |
|      ! 0 | 10015 | `	}` |
|      ! 0 | 10016 | `	return PH7_OK;` |
|      ! 0 | 10017 |  |
|        - | 10018 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 10019 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 10020 | `/*` |
|        - | 10021 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 10022 | ` * the desired message.` |
|        - | 10023 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 10024 | ` * in 'api.c' for additional information.` |
|        - | 10025 | ` */` |
|      370 | 10026 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 10027 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 10028 | `	SyString *pString /* Message to output */` |
|        - | 10029 | `	)` |
|        2 | 10030 |  |
|      372 | 10031 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 | 10032 | `	sxi32 rc = SXRET_OK;` |
|        - | 10033 | `	/* Call the output consumer */` |
|      372 | 10034 | `	if( pString->nByte > 0 ){` |
|      372 | 10035 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 | 10036 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 | 10037 | `	}` |
|      372 | 10038 | `	return rc;` |
|        2 | 10039 |  |
|        - | 10040 | `/*` |
|        - | 10041 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 10042 | ` * callback to consume the formatted message.` |
|        - | 10043 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 10044 | ` * in 'api.c' for additional information.` |
|        - | 10045 | ` */` |
|        2 | 10046 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 10047 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 10048 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 10049 | `	va_list ap           /* Variable list of arguments */` |
|        - | 10050 | `	)` |
|        1 | 10051 |  |
|        3 | 10052 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 10053 | `	sxi32 rc = SXRET_OK;` |
|        - | 10054 | `	SyBlob sWorker;` |
|        - | 10055 | `	/* Format the message and call the output consumer */` |
|        3 | 10056 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 10057 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 10058 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 10059 | `		/* Consume the formatted message */` |
|        3 | 10060 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 10061 | `	}` |
|        3 | 10062 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 10063 | `	/* Release the working buffer */` |
|        3 | 10064 | `	SyBlobRelease(&sWorker);` |
|        3 | 10065 | `	return rc;` |
|        1 | 10066 |  |
|        - | 10067 | `/*` |
|        - | 10068 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 10069 | ` * This function never fail and always return a pointer` |
|        - | 10070 | ` * to a null terminated string.` |
|        - | 10071 | ` */` |
|       12 | 10072 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 10073 |  |
|       13 | 10074 | `	const char *zOp = "Unknown     ";` |
|       13 | 10075 | `	switch(nOp){` |
|        3 | 10076 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 10077 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 10078 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 10079 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 10080 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 10081 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 10082 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 10083 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 10084 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 10085 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 10086 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 10087 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 10088 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 10089 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 10090 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 10091 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 10092 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 10093 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 10094 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 10095 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 10096 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 10097 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 10098 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 10099 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 10100 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 10101 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 10102 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 10103 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 10104 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 10105 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 10106 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 10107 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 10108 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 10109 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 10110 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 10111 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 10112 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 10113 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 10114 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 10115 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 10116 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 10117 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 10118 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 10119 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 10120 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 10121 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 10122 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 10123 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 10124 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 10125 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 10126 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 10127 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 10128 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 10129 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 10130 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 10131 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 10132 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 10133 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 10134 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 10135 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 10136 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 10137 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 10138 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 10139 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 10140 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 10141 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 10142 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 10143 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 | 10144 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 | 10145 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 10146 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 10147 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 10148 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 10149 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 10150 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 10151 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 10152 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 10153 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 10154 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 10155 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 10156 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 10157 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 10158 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 10159 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 10160 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 10161 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 10162 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 10163 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 10164 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 10165 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 10166 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 10167 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 10168 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 10169 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 10170 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 10171 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 10172 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 10173 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 10174 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 10175 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 10176 | `	default:` |
|      ! 0 | 10177 | `		break;` |
|        - | 10178 | `	}` |
|       13 | 10179 | `	return zOp;` |
|        1 | 10180 |  |
|        - | 10181 | `/*` |
|        - | 10182 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 10183 | ` * The xConsumer() callback which is an used defined function` |
|        - | 10184 | ` * is responsible of consuming the generated dump.` |
|        - | 10185 | ` */` |
|        2 | 10186 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 10187 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 10188 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 10189 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 10190 | `	)` |
|        1 | 10191 |  |
|        - | 10192 | `	sxi32 rc;` |
|        3 | 10193 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 10194 | `	return rc;` |
|        1 | 10195 |  |
|        - | 10196 | `/*` |
|        - | 10197 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 10198 | ` * outside a class body [i.e: global or function scope].` |
|        - | 10199 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 10200 | ` * in 'compile.c' for additional information.` |
|        - | 10201 | ` */` |
|       14 | 10202 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 10203 |  |
|       15 | 10204 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 10205 | `	/* Evaluate and expand constant value */` |
|       15 | 10206 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 | 10207 |  |
|        - | 10208 | `/*` |
|        - | 10209 | ` * Section:` |
|        - | 10210 | ` *  Function handling functions.` |
|        - | 10211 | ` * Status:` |
|        - | 10212 | ` *    Stable.` |
|        - | 10213 | ` */` |
|        - | 10214 | `/*` |
|        - | 10215 | ` * int func_num_args(void)` |
|        - | 10216 | ` *   Returns the number of arguments passed to the function.` |
|        - | 10217 | ` * Parameters` |
|        - | 10218 | ` *   None.` |
|        - | 10219 | ` * Return` |
|        - | 10220 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 10221 | ` *  or -1 if called from the globe scope.` |
|        - | 10222 | ` */` |
|      944 | 10223 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10224 |  |
|        - | 10225 | `	VmFrame *pFrame;` |
|        - | 10226 | `	ph7_vm *pVm;` |
|        - | 10227 | `	/* Point to the target VM */` |
|      946 | 10228 | `	pVm = pCtx->pVm;` |
|        - | 10229 | `	/* Current frame */` |
|      946 | 10230 | `	pFrame = pVm->pFrame;` |
|      946 | 10231 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      946 | 10232 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 10233 | `		SXUNUSED(nArg);` |
|      ! 0 | 10234 | `		SXUNUSED(apArg);` |
|        - | 10235 | `		/* Global frame,return -1 */` |
|      ! 0 | 10236 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 10237 | `		return SXRET_OK;` |
|        - | 10238 | `	}` |
|        - | 10239 | `	/* Total number of arguments passed to the enclosing function */` |
|      946 | 10240 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      946 | 10241 | `	ph7_result_int(pCtx,nArg);` |
|      946 | 10242 | `	return SXRET_OK;` |
|      474 | 10243 |  |
|        - | 10244 | `/*` |
|        - | 10245 | ` * value func_get_arg(int $arg_num)` |
|        - | 10246 | ` *   Return an item from the argument list.` |
|        - | 10247 | ` * Parameters` |
|        - | 10248 | ` *  Argument number(index start from zero).` |
|        - | 10249 | ` * Return` |
|        - | 10250 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 10251 | ` */` |
|       22 | 10252 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10253 |  |
|       24 | 10254 | `	ph7_value *pObj = 0;` |
|       24 | 10255 | `	VmSlot *pSlot = 0;` |
|        - | 10256 | `	VmFrame *pFrame;` |
|        - | 10257 | `	ph7_vm *pVm;` |
|        - | 10258 | `	/* Point to the target VM */` |
|       24 | 10259 | `	pVm = pCtx->pVm;` |
|        - | 10260 | `	/* Current frame */` |
|       24 | 10261 | `	pFrame = pVm->pFrame;` |
|       24 | 10262 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 10263 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 10264 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 10265 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 10266 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10267 | `		return SXRET_OK;` |
|        - | 10268 | `	}` |
|        - | 10269 | `	/* Extract the desired index */` |
|       21 | 10270 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 10271 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 10272 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 10273 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10274 | `		return SXRET_OK;` |
|        - | 10275 | `	}` |
|        - | 10276 | `	/* Extract the desired argument */` |
|       21 | 10277 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 10278 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 10279 | `			/* Return the desired argument */` |
|       21 | 10280 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 10281 | `		}else{` |
|        - | 10282 | `			/* No such argument,return false */` |
|      ! 0 | 10283 | `			ph7_result_bool(pCtx,0);` |
|        - | 10284 | `		}` |
|       11 | 10285 | `	}else{` |
|        - | 10286 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 10287 | `		ph7_result_bool(pCtx,0);` |
|        - | 10288 | `	}` |
|       21 | 10289 | `	return SXRET_OK;` |
|       13 | 10290 |  |
|        - | 10291 | `/*` |
|        - | 10292 | ` * array func_get_args_byref(void)` |
|        - | 10293 | ` *   Returns an array comprising a function's argument list.` |
|        - | 10294 | ` * Parameters` |
|        - | 10295 | ` *  None.` |
|        - | 10296 | ` * Return` |
|        - | 10297 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 10298 | ` *  member of the current user-defined function's argument list.` |
|        - | 10299 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 10300 | ` * NOTE:` |
|        - | 10301 | ` *  Arguments are returned to the array by reference.` |
|        - | 10302 | ` */` |
|        2 | 10303 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10304 |  |
|        - | 10305 | `	ph7_value *pArray;` |
|        - | 10306 | `	VmFrame *pFrame;` |
|        - | 10307 | `	VmSlot *aSlot;` |
|        - | 10308 | `	sxu32 n;` |
|        - | 10309 | `	/* Point to the current frame */` |
|        3 | 10310 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 10311 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 10312 | `	if( pFrame->pParent == 0 ){` |
|        - | 10313 | `		/* Global frame,return FALSE */` |
|      ! 0 | 10314 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 10315 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10316 | `		return SXRET_OK;` |
|        - | 10317 | `	}` |
|        - | 10318 | `	/* Create a new array */` |
|        3 | 10319 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10320 | `	if( pArray == 0 ){` |
|      ! 0 | 10321 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10322 | `		SXUNUSED(apArg);` |
|      ! 0 | 10323 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10324 | `		return SXRET_OK;` |
|        - | 10325 | `	}` |
|        - | 10326 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 10327 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 10328 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 10329 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 10330 | `	}` |
|        - | 10331 | `	/* Return the freshly created array */` |
|        3 | 10332 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10333 | `	return SXRET_OK;` |
|        2 | 10334 |  |
|        - | 10335 | `/*` |
|        - | 10336 | ` * array func_get_args(void)` |
|        - | 10337 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 10338 | ` * Parameters` |
|        - | 10339 | ` *  None.` |
|        - | 10340 | ` * Return` |
|        - | 10341 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 10342 | ` *  member of the current user-defined function's argument list.` |
|        - | 10343 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 10344 | ` */` |
|       88 | 10345 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10346 |  |
|       90 | 10347 | `	ph7_value *pObj = 0;` |
|        - | 10348 | `	ph7_value *pArray;` |
|        - | 10349 | `	VmFrame *pFrame;` |
|        - | 10350 | `	VmSlot *aSlot;` |
|        - | 10351 | `	sxu32 n;` |
|        - | 10352 | `	/* Point to the current frame */` |
|       90 | 10353 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 | 10354 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 | 10355 | `	if( pFrame->pParent == 0 ){` |
|        - | 10356 | `		/* Global frame,return FALSE */` |
|      ! 0 | 10357 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 10358 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10359 | `		return SXRET_OK;` |
|        - | 10360 | `	}` |
|        - | 10361 | `	/* Create a new array */` |
|       90 | 10362 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 | 10363 | `	if( pArray == 0 ){` |
|      ! 0 | 10364 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10365 | `		SXUNUSED(apArg);` |
|      ! 0 | 10366 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10367 | `		return SXRET_OK;` |
|        - | 10368 | `	}` |
|        - | 10369 | `	/* Start filling the array with the given arguments */` |
|       90 | 10370 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 | 10371 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 | 10372 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 | 10373 | `		if( pObj ){` |
|      134 | 10374 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 10375 | `		}` |
|       68 | 10376 | `	}` |
|        - | 10377 | `	/* Return the freshly created array */` |
|       90 | 10378 | `	ph7_result_value(pCtx,pArray);` |
|       90 | 10379 | `	return SXRET_OK;` |
|       46 | 10380 |  |
|        - | 10381 | `/*` |
|        - | 10382 | ` * bool function_exists(string $name)` |
|        - | 10383 | ` *  Return TRUE if the given function has been defined.` |
|        - | 10384 | ` * Parameters` |
|        - | 10385 | ` *  The name of the desired function.` |
|        - | 10386 | ` * Return` |
|        - | 10387 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 10388 | ` */` |
|     1680 | 10389 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10390 |  |
|        - | 10391 | `	const char *zName;` |
|        - | 10392 | `	ph7_vm *pVm;` |
|        - | 10393 | `	int nLen;` |
|        - | 10394 | `	int res;` |
|     1682 | 10395 | `	if( nArg < 1 ){` |
|        - | 10396 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 10397 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10398 | `		return SXRET_OK;` |
|        - | 10399 | `	}` |
|        - | 10400 | `	/* Point to the target VM */` |
|     1682 | 10401 | `	pVm = pCtx->pVm;` |
|        - | 10402 | `	/* Extract the function name */` |
|     1682 | 10403 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 10404 | `	/* Assume the function is not defined */` |
|     1682 | 10405 | `	res = 0;` |
|        - | 10406 | `	/* Perform the lookup */` |
|     2520 | 10407 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1676 | 10408 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 10409 | `			/* Function is defined */` |
|      206 | 10410 | `			res = 1;` |
|      102 | 10411 | `	}` |
|     1682 | 10412 | `	ph7_result_bool(pCtx,res);` |
|     1682 | 10413 | `	return SXRET_OK;` |
|      842 | 10414 |  |
|        - | 10415 | `/*` |
|        - | 10416 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 10417 | ` * [i.e: Whether it is callable or not].` |
|        - | 10418 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 10419 | ` */` |
|    20756 | 10420 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 | 10421 |  |
|    20758 | 10422 | `	int res = 0;` |
|    20758 | 10423 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 10424 | `		/* Call the magic method __invoke if available */` |
|      ! 0 | 10425 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - | 10426 | `		ph7_class_method *pMethod;` |
|      ! 0 | 10427 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 | 10428 | `		if( pMethod && CallInvoke ){` |
|        - | 10429 | `			ph7_value sResult;` |
|        - | 10430 | `			sxi32 rc;` |
|        - | 10431 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 | 10432 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 | 10433 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 | 10434 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 | 10435 | `				res = sResult.x.iVal != 0;` |
|      ! 0 | 10436 | `			}` |
|      ! 0 | 10437 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10438 | `		}` |
|    20758 | 10439 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 | 10440 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 | 10441 | `		if( pMap->nEntry == 2 ){` |
|        - | 10442 | `			ph7_class *pClass;` |
|        - | 10443 | `			ph7_value *pV;` |
|        - | 10444 | `			/* Extract the target class */` |
|       12 | 10445 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 | 10446 | `			if( pV ){` |
|       12 | 10447 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 | 10448 | `				if( pClass ){` |
|        - | 10449 | `					ph7_class_method *pMethod;` |
|        - | 10450 | `					/* Extract the target method */` |
|       10 | 10451 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 10452 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 10453 | `						/* Perform the lookup */` |
|       10 | 10454 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 10455 | `						if( pMethod ){` |
|        - | 10456 | `							/* Method is callable */` |
|        5 | 10457 | `							res = 1;` |
|        2 | 10458 | `						}` |
|        4 | 10459 | `					}` |
|        4 | 10460 | `				}` |
|        5 | 10461 | `			}` |
|        7 | 10462 | `		}` |
|    20745 | 10463 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 10464 | `		const char *zName;` |
|        - | 10465 | `		int nLen;` |
|        - | 10466 | `		/* Extract the name */` |
|     5452 | 10467 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 10468 | `		/* Perform the lookup */` |
|     5467 | 10469 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 10470 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 10471 | `				/* Function is callable */` |
|     5434 | 10472 | `				res = 1;` |
|     2716 | 10473 | `		}` |
|     2725 | 10474 | `	}` |
|    20758 | 10475 | `	return res;` |
|        2 | 10476 |  |
|        - | 10477 | `/*` |
|        - | 10478 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 10479 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 10480 | ` * Parameters` |
|        - | 10481 | ` * $name` |
|        - | 10482 | ` *    The callback function to check` |
|        - | 10483 | ` * $syntax_only` |
|        - | 10484 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 10485 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 10486 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 10487 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 10488 | ` *    a string.` |
|        - | 10489 | ` * Return` |
|        - | 10490 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 10491 | ` */` |
|       14 | 10492 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10493 |  |
|        - | 10494 | `	ph7_vm *pVm;` |
|        - | 10495 | `	int res;` |
|       15 | 10496 | `	if( nArg < 1 ){` |
|        - | 10497 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 10498 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10499 | `		return SXRET_OK;` |
|        - | 10500 | `	}` |
|        - | 10501 | `	/* Point to the target VM */` |
|       15 | 10502 | `	pVm = pCtx->pVm;` |
|        - | 10503 | `	/* Perform the requested operation */` |
|       15 | 10504 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 | 10505 | `	ph7_result_bool(pCtx,res);` |
|       15 | 10506 | `	return SXRET_OK;` |
|        8 | 10507 |  |
|        - | 10508 | `/*` |
|        - | 10509 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 10510 | ` * defined below.` |
|        - | 10511 | ` */` |
|     1218 | 10512 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 10513 |  |
|     1219 | 10514 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 10515 | `	ph7_value sName;` |
|        - | 10516 | `	sxi32 rc;` |
|        - | 10517 | `	/* Prepare the function name for insertion */` |
|     1219 | 10518 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1219 | 10519 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 10520 | `	/* Perform the insertion */` |
|     1219 | 10521 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1219 | 10522 | `	PH7_MemObjRelease(&sName);` |
|     1219 | 10523 | `	return rc;` |
|        1 | 10524 |  |
|        - | 10525 | `/*` |
|        - | 10526 | ` * array get_defined_functions(void)` |
|        - | 10527 | ` *  Returns an array of all defined functions.` |
|        - | 10528 | ` * Parameter` |
|        - | 10529 | ` *  None.` |
|        - | 10530 | ` * Return` |
|        - | 10531 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 10532 | ` *  both built-in (internal) and user-defined.` |
|        - | 10533 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 10534 | ` *  defined ones using $arr["user"].` |
|        - | 10535 | ` * Note:` |
|        - | 10536 | ` *  NULL is returned on failure.` |
|        - | 10537 | ` */` |
|        2 | 10538 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10539 |  |
|        - | 10540 | `	ph7_value *pArray,*pEntry;` |
|        - | 10541 | `	/* NOTE:` |
|        - | 10542 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 10543 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 10544 | `	 */` |
|        3 | 10545 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10546 | ` 	if( pArray == 0 ){` |
|      ! 0 | 10547 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10548 | `		SXUNUSED(apArg);` |
|        - | 10549 | `		/* Return NULL */` |
|      ! 0 | 10550 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10551 | `		return SXRET_OK;` |
|        - | 10552 | `	}` |
|        3 | 10553 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 10554 | `	if( pEntry == 0 ){` |
|        - | 10555 | `		/* Return NULL */` |
|      ! 0 | 10556 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10557 | `		return SXRET_OK;` |
|        - | 10558 | `	}` |
|        - | 10559 | `	/* Fill with the appropriate information */` |
|        3 | 10560 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 10561 | `	/* Create the 'internal' index */` |
|        3 | 10562 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 10563 | `	/* Create the user-func array */` |
|        3 | 10564 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 10565 | `	if( pEntry == 0 ){` |
|        - | 10566 | `		/* Return NULL */` |
|      ! 0 | 10567 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10568 | `		return SXRET_OK;` |
|        - | 10569 | `	}` |
|        - | 10570 | `	/* Fill with the appropriate information */` |
|        3 | 10571 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 10572 | `	/* Create the 'user' index */` |
|        3 | 10573 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 10574 | `	/* Return the multi-dimensional array */` |
|        3 | 10575 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10576 | `	return SXRET_OK;` |
|        2 | 10577 |  |
|        - | 10578 | `/*` |
|        - | 10579 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 10580 | ` *  Register a function for execution on shutdown.` |
|        - | 10581 | ` * Note` |
|        - | 10582 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 10583 | ` *  be called in the same order as they were registered.` |
|        - | 10584 | ` * Parameters` |
|        - | 10585 | ` *  $callback` |
|        - | 10586 | ` *   The shutdown callback to register.` |
|        - | 10587 | ` * $param` |
|        - | 10588 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 10589 | ` * Return` |
|        - | 10590 | ` *  Nothing.` |
|        - | 10591 | ` */` |
|        2 | 10592 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10593 |  |
|        - | 10594 | `	VmShutdownCB sEntry;` |
|        - | 10595 | `	int i,j;` |
|        3 | 10596 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 10597 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 10598 | `		return PH7_OK;` |
|        - | 10599 | `	}` |
|        - | 10600 | `	/* Zero the Entry */` |
|        3 | 10601 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 10602 | `	/* Initialize fields */` |
|        3 | 10603 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 10604 | `	/* Save the callback name for later invocation name */` |
|        3 | 10605 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 | 10606 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 | 10607 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 | 10608 | `	}` |
|        - | 10609 | `	/* Copy arguments */` |
|        3 | 10610 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 10611 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 10612 | `			/* Limit reached */` |
|      ! 0 | 10613 | `			break;` |
|        - | 10614 | `		}` |
|      ! 0 | 10615 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 10616 | `	}` |
|        3 | 10617 | `	sEntry.nArg = j;` |
|        - | 10618 | `	/* Install the callback */` |
|        3 | 10619 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 | 10620 | `	return PH7_OK;` |
|        2 | 10621 |  |
|        - | 10622 | `/*` |
|        - | 10623 | ` * Section:` |
|        - | 10624 | ` *  Class handling functions.` |
|        - | 10625 | ` * Status:` |
|        - | 10626 | ` *    Stable.` |
|        - | 10627 | ` */` |
|        - | 10628 | `/*` |
|        - | 10629 | ` * Extract the top active class. NULL is returned` |
|        - | 10630 | ` * if the class stack is empty.` |
|        - | 10631 | ` */` |
|      792 | 10632 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 10633 |  |
|      794 | 10634 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 10635 | `	ph7_class **apClass;` |
|      794 | 10636 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 10637 | `		/* Empty stack,return NULL */` |
|       15 | 10638 | `		return 0;` |
|        - | 10639 | `	}` |
|        - | 10640 | `	/* Peek the last entry */` |
|      780 | 10641 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      780 | 10642 | `	return apClass[pSet->nUsed - 1];` |
|      398 | 10643 |  |
|        - | 10644 | `/*` |
|        - | 10645 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 10646 | ` *   Get the class that declared the currently executing method.` |
|        - | 10647 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 10648 | ` *` |
|        - | 10649 | ` * Parameters` |
|        - | 10650 | ` *   pVm: Target VM` |
|        - | 10651 | ` *` |
|        - | 10652 | ` * Return` |
|        - | 10653 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 10654 | ` *   - Not executing within a class method` |
|        - | 10655 | ` *` |
|        - | 10656 | ` * Note` |
|        - | 10657 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 10658 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 10659 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 10660 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 10661 | ` *   declaring class.` |
|        - | 10662 | ` */` |
|       98 | 10663 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 10664 |  |
|      100 | 10665 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10666 | `	ph7_vm_func *pVmFunc;` |
|        - | 10667 |  |
|        - | 10668 | `	/* Skip exception frames to find the actual method frame */` |
|      100 | 10669 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 10670 |  |
|        - | 10671 | `	/* Check if we're in a method context */` |
|      100 | 10672 | `	if( pFrame->pParent ){` |
|       96 | 10673 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       96 | 10674 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 10675 | `			/* Return the declaring class */` |
|       96 | 10676 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 10677 | `		}` |
|      ! 0 | 10678 | `	}` |
|        - | 10679 |  |
|        5 | 10680 | `	return 0;` |
|       51 | 10681 |  |
|        - | 10682 |  |
|        - | 10683 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 10684 | `/*` |
|        - | 10685 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 10686 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 10687 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 10688 | ` * return value indicates failure.` |
|        - | 10689 | ` */` |
|        - | 10690 | `/*` |
|        - | 10691 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 10692 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 10693 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 10694 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 10695 | ` */` |
|     1840 | 10696 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 10697 | `	ph7_vm *pVm,` |
|        - | 10698 | `	ph7_class_instance *pThis,` |
|        - | 10699 | `	ph7_class_method *pMethod,` |
|        - | 10700 | `	ph7_value *pResult,` |
|        - | 10701 | `	int nArg,` |
|        - | 10702 | `	ph7_value **apArg,` |
|        - | 10703 | `	VmCallArgMap *pMap` |
|        - | 10704 | `	)` |
|        2 | 10705 |  |
|        - | 10706 | `	ph7_value *aStack;` |
|        - | 10707 | `	VmInstr aInstr[2];` |
|        - | 10708 | `	int iCursor;` |
|        - | 10709 | `	int i;` |
|     1842 | 10710 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     1842 | 10711 | `	if( aStack == 0 ){` |
|      ! 0 | 10712 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 10713 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 10714 | `		return SXERR_MEM;` |
|        - | 10715 | `	}` |
|     2732 | 10716 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      892 | 10717 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|      892 | 10718 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      447 | 10719 | `	}` |
|     1842 | 10720 | `	iCursor = nArg + 1;` |
|     1842 | 10721 | `	if( pThis ){` |
|     1836 | 10722 | `		pThis->iRef++;` |
|     1836 | 10723 | `		aStack[i].x.pOther = pThis;` |
|     1836 | 10724 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      917 | 10725 | `	}` |
|     1842 | 10726 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     1842 | 10727 | `	i++;` |
|     1842 | 10728 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1842 | 10729 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1842 | 10730 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1842 | 10731 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     1842 | 10732 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1842 | 10733 | `	aInstr[0].iP1 = nArg;` |
|     1842 | 10734 | `	aInstr[0].iP2 = 0;` |
|     1842 | 10735 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     1842 | 10736 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1842 | 10737 | `	aInstr[1].iP1 = 1;` |
|     1842 | 10738 | `	aInstr[1].iP2 = 0;` |
|     1842 | 10739 | `	aInstr[1].p3  = 0;` |
|     1842 | 10740 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0);` |
|     1842 | 10741 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1842 | 10742 | `	return PH7_OK;` |
|      922 | 10743 |  |
|     1536 | 10744 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 10745 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 10746 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 10747 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 10748 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 10749 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 10750 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 10751 | `	)` |
|        2 | 10752 |  |
|     1538 | 10753 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 10754 |  |
|        - | 10755 | `/*` |
|        - | 10756 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 10757 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 10758 | ` * in the apArg[] array.` |
|        - | 10759 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 10760 | ` * return value indicates failure.` |
|        - | 10761 | ` */` |
|      980 | 10762 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 10763 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 10764 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 10765 | `	int nArg,          /* Total number of given arguments */` |
|        - | 10766 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 10767 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 10768 | `	)` |
|        2 | 10769 |  |
|        - | 10770 | `	ph7_value *aStack;` |
|        - | 10771 | `	VmInstr aInstr[2];` |
|        - | 10772 | `	int i;` |
|      982 | 10773 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 10774 | `		/* Don't bother processing,it's invalid anyway */` |
|      491 | 10775 | `		if( pResult ){` |
|        - | 10776 | `			/* Assume a null return value */` |
|      ! 0 | 10777 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 10778 | `		}` |
|      491 | 10779 | `		return SXERR_INVALID;` |
|        - | 10780 | `	}` |
|      492 | 10781 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 10782 | `		/* Class method */` |
|       11 | 10783 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 | 10784 | `		ph7_class_method *pMethod = 0;` |
|       11 | 10785 | `		ph7_class_instance *pThis = 0;` |
|       11 | 10786 | `		ph7_class *pClass = 0;` |
|        - | 10787 | `		ph7_value *pValue;` |
|        - | 10788 | `		sxi32 rc;` |
|       11 | 10789 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 10790 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 10791 | `			if( pResult ){` |
|        - | 10792 | `				/* Assume a null return value */` |
|      ! 0 | 10793 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10794 | `			}` |
|      ! 0 | 10795 | `			return SXRET_OK;` |
|        - | 10796 | `		}` |
|        - | 10797 | `		/* Extract the class name or an instance of it */` |
|       11 | 10798 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 | 10799 | `		if( pValue ){` |
|       11 | 10800 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 | 10801 | `		}` |
|       11 | 10802 | `		if( pClass == 0 ){` |
|        - | 10803 | `			/* No such class,return NULL */` |
|      ! 0 | 10804 | `			if( pResult ){` |
|      ! 0 | 10805 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10806 | `			}` |
|      ! 0 | 10807 | `			return SXRET_OK;` |
|        - | 10808 | `		}` |
|       11 | 10809 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 10810 | `			/* Point to the class instance */` |
|        5 | 10811 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 | 10812 | `		}` |
|        - | 10813 | `		/* Try to extract the method */` |
|       11 | 10814 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 | 10815 | `		if( pValue ){` |
|       11 | 10816 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 | 10817 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 | 10818 | `					SyBlobLength(&pValue->sBlob));` |
|        5 | 10819 | `			}` |
|        5 | 10820 | `		}` |
|       11 | 10821 | `		if( pMethod == 0 ){` |
|        - | 10822 | `			/* No such method,return NULL */` |
|      ! 0 | 10823 | `			if( pResult ){` |
|      ! 0 | 10824 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10825 | `			}` |
|      ! 0 | 10826 | `			return SXRET_OK;` |
|        - | 10827 | `		}` |
|        - | 10828 | `		/* Call the class method */` |
|       11 | 10829 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 | 10830 | `		return rc;` |
|        - | 10831 | `	}` |
|        - | 10832 | `	/* Create a new operand stack */` |
|      482 | 10833 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      482 | 10834 | `	if( aStack == 0 ){` |
|      ! 0 | 10835 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 10836 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 10837 | `		if( pResult ){` |
|        - | 10838 | `			/* Assume a null return value */` |
|      ! 0 | 10839 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 10840 | `		}` |
|      ! 0 | 10841 | `		return SXERR_MEM;` |
|        - | 10842 | `	}` |
|        - | 10843 | `	/* Fill the operand stack with the given arguments */` |
|     1544 | 10844 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1064 | 10845 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 10846 | `		/*` |
|        - | 10847 | `		 * Symisc eXtension:` |
|        - | 10848 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 10849 | `		 */` |
|     1064 | 10850 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      533 | 10851 | `	}` |
|        - | 10852 | `	/* Push the function name */` |
|      482 | 10853 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      482 | 10854 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 10855 | `	/* Emit the CALL istruction */` |
|      482 | 10856 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      482 | 10857 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      482 | 10858 | `	aInstr[0].iP2 = 0;` |
|      482 | 10859 | `	aInstr[0].p3  = 0;` |
|        - | 10860 | `	/* Emit the DONE instruction */` |
|      482 | 10861 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      482 | 10862 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      482 | 10863 | `	aInstr[1].iP2 = 0;` |
|      482 | 10864 | `	aInstr[1].p3  = 0;` |
|        - | 10865 | `	/* Execute the function body (if available) */` |
|      482 | 10866 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0);` |
|        - | 10867 | `	/* Clean up the mess left behind */` |
|      482 | 10868 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      482 | 10869 | `	return PH7_OK;` |
|      492 | 10870 |  |
|        - | 10871 | `/*` |
|        - | 10872 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 10873 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 10874 | ` * parameter.` |
|        - | 10875 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 10876 | ` * return value indicates failure.` |
|        - | 10877 | ` */` |
|      236 | 10878 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 10879 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 10880 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 10881 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 10882 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 10883 | `	)` |
|        1 | 10884 |  |
|        - | 10885 | `	ph7_value *pArg;` |
|        - | 10886 | `	SySet aArg;` |
|        - | 10887 | `	va_list ap;` |
|        - | 10888 | `	sxi32 rc;` |
|      237 | 10889 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 10890 | `	/* Copy arguments one after one */` |
|      237 | 10891 | `	va_start(ap,pResult);` |
|      393 | 10892 | `	for(;;){` |
|      787 | 10893 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 | 10894 | `		if( pArg == 0 ){` |
|      237 | 10895 | `			break;` |
|        - | 10896 | `		}` |
|      551 | 10897 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 10898 | `	}` |
|        - | 10899 | `	/* Call the core routine */` |
|      237 | 10900 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 10901 | `	/* Cleanup */` |
|      237 | 10902 | `	SySetRelease(&aArg);` |
|      237 | 10903 | `	return rc;` |
|        1 | 10904 |  |
|        - | 10905 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 10906 | `/*` |
|        - | 10907 | ` * bool defined(string $name)` |
|        - | 10908 | ` *  Checks whether a given named constant exists.` |
|        - | 10909 | ` * Parameter:` |
|        - | 10910 | ` *  Name of the desired constant.` |
|        - | 10911 | ` * Return` |
|        - | 10912 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 10913 | ` */` |
|       14 | 10914 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10915 |  |
|        - | 10916 | `	const char *zName;` |
|       16 | 10917 | `	int nLen = 0;` |
|       16 | 10918 | `	int res = 0;` |
|       16 | 10919 | `	if( nArg < 1 ){` |
|        - | 10920 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 10921 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 10922 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10923 | `		return SXRET_OK;` |
|        - | 10924 | `	}` |
|        - | 10925 | `	/* Extract constant name */` |
|       16 | 10926 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 10927 | `	/* Perform the lookup */` |
|       16 | 10928 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 10929 | `		/* Already defined */` |
|       10 | 10930 | `		res = 1;` |
|        4 | 10931 | `	}` |
|       16 | 10932 | `	ph7_result_bool(pCtx,res);` |
|       16 | 10933 | `	return SXRET_OK;` |
|        9 | 10934 |  |
|        - | 10935 | `/*` |
|        - | 10936 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 10937 | ` * below.` |
|        - | 10938 | ` */` |
|       10 | 10939 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 10940 |  |
|       12 | 10941 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 10942 | `	/* Expand constant value */` |
|       12 | 10943 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 | 10944 |  |
|        - | 10945 | `/*` |
|        - | 10946 | ` * bool define(string $constant_name,expression value)` |
|        - | 10947 | ` *  Defines a named constant at runtime.` |
|        - | 10948 | ` * Parameter:` |
|        - | 10949 | ` *  $constant_name` |
|        - | 10950 | ` *   The name of the constant` |
|        - | 10951 | ` *  $value` |
|        - | 10952 | ` *   Constant value` |
|        - | 10953 | ` * Return:` |
|        - | 10954 | ` *   TRUE on success,FALSE on failure.` |
|        - | 10955 | ` */` |
|       12 | 10956 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10957 |  |
|        - | 10958 | `	const char *zName;  /* Constant name */` |
|        - | 10959 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 | 10960 | `	int nLen = 0;       /* Name length */` |
|        - | 10961 | `	sxi32 rc;` |
|       14 | 10962 | `	if( nArg < 2 ){` |
|        - | 10963 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 10964 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 10965 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10966 | `		return SXRET_OK;` |
|        - | 10967 | `	}` |
|       14 | 10968 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 10969 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 10970 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10971 | `		return SXRET_OK;` |
|        - | 10972 | `	}` |
|        - | 10973 | `	/* Extract constant name */` |
|       14 | 10974 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 | 10975 | `	if( nLen < 1 ){` |
|      ! 0 | 10976 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 10977 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10978 | `		return SXRET_OK;` |
|        - | 10979 | `	}` |
|        - | 10980 | `	/* Duplicate constant value */` |
|       14 | 10981 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 | 10982 | `	if( pValue == 0 ){` |
|      ! 0 | 10983 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 10984 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10985 | `		return SXRET_OK;` |
|        - | 10986 | `	}` |
|        - | 10987 | `	/* Initialize the memory object */` |
|       14 | 10988 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 10989 | `	/* Register the constant */` |
|       14 | 10990 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 | 10991 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10992 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 10993 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 10994 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10995 | `		return SXRET_OK;` |
|        - | 10996 | `	}` |
|        - | 10997 | `	/* Duplicate constant value */` |
|       14 | 10998 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 | 10999 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 11000 | `		/* Lower case the constant name */` |
|      ! 0 | 11001 | `		char *zCur = (char *)zName;` |
|      ! 0 | 11002 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 11003 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 11004 | `				/* UTF-8 stream */` |
|      ! 0 | 11005 | `				zCur++;` |
|      ! 0 | 11006 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 11007 | `					zCur++;` |
|      ! 0 | 11008 | `				}` |
|      ! 0 | 11009 | `				continue;` |
|        - | 11010 | `			}` |
|      ! 0 | 11011 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 11012 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 11013 | `				zCur[0] = (char)c;` |
|      ! 0 | 11014 | `			}` |
|      ! 0 | 11015 | `			zCur++;` |
|      ! 0 | 11016 | `		}` |
|        - | 11017 | `		/* Finally,register the constant */` |
|      ! 0 | 11018 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 | 11019 | `	}` |
|        - | 11020 | `	/* All done,return TRUE */` |
|       14 | 11021 | `	ph7_result_bool(pCtx,1);` |
|       14 | 11022 | `	return SXRET_OK;` |
|        8 | 11023 |  |
|        - | 11024 | `/*` |
|        - | 11025 | ` * value constant(string $name)` |
|        - | 11026 | ` *  Returns the value of a constant` |
|        - | 11027 | ` * Parameter` |
|        - | 11028 | ` *  $name` |
|        - | 11029 | ` *    Name of the constant.` |
|        - | 11030 | ` * Return` |
|        - | 11031 | ` *  Constant value or NULL if not defined.` |
|        - | 11032 | ` */` |
|        8 | 11033 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11034 |  |
|        - | 11035 | `	SyHashEntry *pEntry;` |
|        - | 11036 | `	ph7_constant *pCons;` |
|        - | 11037 | `	const char *zName; /* Constant name */` |
|        - | 11038 | `	ph7_value sVal;    /* Constant value */` |
|        - | 11039 | `	int nLen;` |
|       10 | 11040 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11041 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 11042 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 11043 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11044 | `		return SXRET_OK;` |
|        - | 11045 | `	}` |
|        - | 11046 | `	/* Extract the constant name */` |
|       10 | 11047 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11048 | `	/* Perform the query */` |
|       10 | 11049 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 11050 | `	if( pEntry == 0 ){` |
|        3 | 11051 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 11052 | `		ph7_result_null(pCtx);` |
|        3 | 11053 | `		return SXRET_OK;` |
|        - | 11054 | `	}` |
|        8 | 11055 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 11056 | `	/* Point to the structure that describe the constant */` |
|        8 | 11057 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 11058 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 11059 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 11060 | `	/* Return that value */` |
|        8 | 11061 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 11062 | `	/* Cleanup */` |
|        8 | 11063 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 11064 | `	return SXRET_OK;` |
|        6 | 11065 |  |
|        - | 11066 | `/*` |
|        - | 11067 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 11068 | ` * defined below.` |
|        - | 11069 | ` */` |
|      452 | 11070 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11071 |  |
|      453 | 11072 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11073 | `	ph7_value sName;` |
|        - | 11074 | `	sxi32 rc;` |
|        - | 11075 | `	/* Prepare the constant name for insertion */` |
|      453 | 11076 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      453 | 11077 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11078 | `	/* Perform the insertion */` |
|      453 | 11079 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      453 | 11080 | `	PH7_MemObjRelease(&sName);` |
|      453 | 11081 | `	return rc;` |
|        1 | 11082 |  |
|        - | 11083 | `/*` |
|        - | 11084 | ` * array get_defined_constants(void)` |
|        - | 11085 | ` *  Returns an associative array with the names of all defined` |
|        - | 11086 | ` *  constants.` |
|        - | 11087 | ` * Parameters` |
|        - | 11088 | ` *  NONE.` |
|        - | 11089 | ` * Returns` |
|        - | 11090 | ` *  Returns the names of all the constants currently defined.` |
|        - | 11091 | ` */` |
|        2 | 11092 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11093 |  |
|        - | 11094 | `	ph7_value *pArray;` |
|        - | 11095 | `	/* Create the array first*/` |
|        3 | 11096 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11097 | `	if( pArray == 0 ){` |
|      ! 0 | 11098 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11099 | `		SXUNUSED(apArg);` |
|        - | 11100 | `		/* Return NULL */` |
|      ! 0 | 11101 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11102 | `		return SXRET_OK;` |
|        - | 11103 | `	}` |
|        - | 11104 | `	/* Fill the array with the defined constants */` |
|        3 | 11105 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 11106 | `	/* Return the created array */` |
|        3 | 11107 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11108 | `	return SXRET_OK;` |
|        2 | 11109 |  |
|        - | 11110 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 11111 | `/*` |
|        - | 11112 | ` * Section:` |
|        - | 11113 | ` *  Random numbers/string generators.` |
|        - | 11114 | ` * Status:` |
|        - | 11115 | ` *    Stable.` |
|        - | 11116 | ` */` |
|        - | 11117 | `/*` |
|        - | 11118 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 11119 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - | 11120 | ` * used by te SQLite3 library.` |
|        - | 11121 | ` */` |
|     2697 | 11122 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 11123 |  |
|        - | 11124 | `	sxu32 iNum;` |
|     2699 | 11125 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2699 | 11126 | `	return iNum;` |
|        2 | 11127 |  |
|        - | 11128 | `/*` |
|        - | 11129 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 11130 | ` * Note that the generated string is NOT null terminated.` |
|        - | 11131 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - | 11132 | ` * by te SQLite3 library.` |
|        - | 11133 | ` */` |
|   191552 | 11134 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 11135 |  |
|        - | 11136 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 11137 | `	int i;` |
|        - | 11138 | `	/* Generate a binary string first */` |
|   191554 | 11139 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 11140 | `	/* Turn the binary string into english based alphabet */` |
|  2107242 | 11141 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1915690 | 11142 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   957846 | 11143 | `	 }` |
|   191554 | 11144 |  |
|        - | 11145 | `/*` |
|        - | 11146 | ` * int rand()` |
|        - | 11147 | ` * int mt_rand()` |
|        - | 11148 | ` * int rand(int $min,int $max)` |
|        - | 11149 | ` * int mt_rand(int $min,int $max)` |
|        - | 11150 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 11151 | ` * Parameter` |
|        - | 11152 | ` *  $min` |
|        - | 11153 | ` *    The lowest value to return (default: 0)` |
|        - | 11154 | ` *  $max` |
|        - | 11155 | ` *   The highest value to return (default: getrandmax())` |
|        - | 11156 | ` * Return` |
|        - | 11157 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 11158 | ` * Note:` |
|        - | 11159 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11160 | ` *  by te SQLite3 library.` |
|        - | 11161 | ` */` |
|       20 | 11162 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11163 |  |
|        - | 11164 | `	sxu32 iNum;` |
|        - | 11165 | `	/* Generate the random number */` |
|       21 | 11166 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 11167 | `	if( nArg > 1 ){` |
|        - | 11168 | `		sxu32 iMin,iMax;` |
|        3 | 11169 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 11170 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 11171 | `		if( iMin < iMax ){` |
|        3 | 11172 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 11173 | `			if( iDiv > 0 ){` |
|        3 | 11174 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 11175 | `			}` |
|        1 | 11176 | `		}else if(iMax > 0 ){` |
|      ! 0 | 11177 | `			iNum %= iMax;` |
|      ! 0 | 11178 | `		}` |
|        1 | 11179 | `	}` |
|        - | 11180 | `	/* Return the number */` |
|       21 | 11181 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 11182 | `	return SXRET_OK;` |
|        1 | 11183 |  |
|        - | 11184 | `/*` |
|        - | 11185 | ` * int getrandmax(void)` |
|        - | 11186 | ` * int mt_getrandmax(void)` |
|        - | 11187 | ` * int rc4_getrandmax(void)` |
|        - | 11188 | ` *   Show largest possible random value` |
|        - | 11189 | ` * Return` |
|        - | 11190 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 11191 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 11192 | ` * Note:` |
|        - | 11193 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11194 | ` *  by te SQLite3 library.` |
|        - | 11195 | ` */` |
|        4 | 11196 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11197 |  |
|        2 | 11198 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 11199 | `	SXUNUSED(apArg);` |
|        5 | 11200 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 11201 | `	return SXRET_OK;` |
|        1 | 11202 |  |
|        - | 11203 | `/*` |
|        - | 11204 | ` * string rand_str()` |
|        - | 11205 | ` * string rand_str(int $len)` |
|        - | 11206 | ` *  Generate a random string (English alphabet).` |
|        - | 11207 | ` * Parameter` |
|        - | 11208 | ` *  $len` |
|        - | 11209 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 11210 | ` * Return` |
|        - | 11211 | ` *   A pseudo random string.` |
|        - | 11212 | ` * Note:` |
|        - | 11213 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11214 | ` *  by te SQLite3 library.` |
|        - | 11215 | ` *  This function is a symisc extension.` |
|        - | 11216 | ` */` |
|      120 | 11217 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11218 |  |
|        - | 11219 | `	char zString[1024];` |
|      122 | 11220 | `	int iLen = 0x10;` |
|      122 | 11221 | `	if( nArg > 0 ){` |
|        - | 11222 | `		/* Get the desired length */` |
|      122 | 11223 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 11224 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 11225 | `			/* Default length */` |
|        3 | 11226 | `			iLen = 0x10;` |
|        1 | 11227 | `		}` |
|       60 | 11228 | `	}` |
|        - | 11229 | `	/* Generate the random string */` |
|      122 | 11230 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 11231 | `	/* Return the generated string */` |
|      122 | 11232 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 11233 | `	return SXRET_OK;` |
|        2 | 11234 |  |
|        - | 11235 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11236 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 11237 | `/* Unique ID private data */` |
|        - | 11238 | `struct unique_id_data` |
|        - | 11239 |  |
|        - | 11240 | `	ph7_context *pCtx; /* Call context */` |
|        - | 11241 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 11242 | `};` |
|        - | 11243 | `/*` |
|        - | 11244 | ` * Binary to hex consumer callback.` |
|        - | 11245 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 11246 | ` * defined below.` |
|        - | 11247 | ` */` |
|      192 | 11248 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 11249 |  |
|      193 | 11250 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 11251 | `	sxu32 nBuflen;` |
|        - | 11252 | `	/* Extract result buffer length */` |
|      193 | 11253 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 11254 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 11255 | `			/*` |
|        - | 11256 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 11257 | `			 * string will be 13 characters long` |
|        - | 11258 | `			 */` |
|       25 | 11259 | `		return SXERR_ABORT;` |
|        - | 11260 | `	}` |
|      169 | 11261 | `	if( nBuflen > 22 ){` |
|      ! 0 | 11262 | `		return SXERR_ABORT;` |
|        - | 11263 | `	}` |
|        - | 11264 | `	/* Safely Consume the hex stream */` |
|      169 | 11265 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 11266 | `	return SXRET_OK;` |
|       97 | 11267 |  |
|        - | 11268 | `/*` |
|        - | 11269 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 11270 | ` *  Generate a unique ID` |
|        - | 11271 | ` * Parameter` |
|        - | 11272 | ` * $prefix` |
|        - | 11273 | ` *  Append this prefix to the generated unique ID.` |
|        - | 11274 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 11275 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 11276 | ` * $more_entropy` |
|        - | 11277 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 11278 | ` *  that the result will be unique.` |
|        - | 11279 | ` * Return` |
|        - | 11280 | ` *  Returns the unique identifier, as a string.` |
|        - | 11281 | ` */` |
|       24 | 11282 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11283 |  |
|        - | 11284 | `	struct unique_id_data sUniq;` |
|        - | 11285 | `	unsigned char zDigest[20];` |
|       25 | 11286 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11287 | `	const char *zPrefix;` |
|        - | 11288 | `	SHA1Context sCtx;` |
|        - | 11289 | `	char zRandom[7];` |
|        - | 11290 | `	int nPrefix;` |
|        - | 11291 | `	int entropy;` |
|        - | 11292 | `	/* Generate a random string first */` |
|       25 | 11293 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 11294 | `	/* Initialize fields */` |
|       25 | 11295 | `	zPrefix = 0;` |
|       25 | 11296 | `	nPrefix = 0;` |
|       25 | 11297 | `	entropy = 0;` |
|       25 | 11298 | `	if( nArg > 0 ){` |
|        - | 11299 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 11300 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 11301 | `		if( nArg > 1 ){` |
|      ! 0 | 11302 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 11303 | `		}` |
|      ! 0 | 11304 | `	}` |
|       25 | 11305 | `	SHA1Init(&sCtx);` |
|        - | 11306 | `	/* Generate the random ID */` |
|       25 | 11307 | `	if( nPrefix > 0 ){` |
|      ! 0 | 11308 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 11309 | `	}` |
|        - | 11310 | `	/* Append the random ID */` |
|       25 | 11311 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 11312 | `	/* Append the random string */` |
|       25 | 11313 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 11314 | `	/* Increment the number */` |
|       25 | 11315 | `	pVm->unique_id++;` |
|       25 | 11316 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 11317 | `	/* Hexify the digest */` |
|       25 | 11318 | `	sUniq.pCtx = pCtx;` |
|       25 | 11319 | `	sUniq.entropy = entropy;` |
|       25 | 11320 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 11321 | `	/* All done */` |
|       25 | 11322 | `	return PH7_OK;` |
|        1 | 11323 |  |
|        - | 11324 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 11325 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 11326 | `/*` |
|        - | 11327 | ` * Section:` |
|        - | 11328 | ` *  Language construct implementation as foreign functions.` |
|        - | 11329 | ` * Status:` |
|        - | 11330 | ` *    Stable.` |
|        - | 11331 | ` */` |
|        - | 11332 | `/*` |
|        - | 11333 | ` * void echo($string...)` |
|        - | 11334 | ` *  Output one or more messages.` |
|        - | 11335 | ` * Parameters` |
|        - | 11336 | ` *  $string` |
|        - | 11337 | ` *   Message to output.` |
|        - | 11338 | ` * Return` |
|        - | 11339 | ` *  NULL.` |
|        - | 11340 | ` */` |
|      ! 0 | 11341 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 11342 |  |
|        - | 11343 | `	const char *zData;` |
|      ! 0 | 11344 | `	int nDataLen = 0;` |
|        - | 11345 | `	ph7_vm *pVm;` |
|        - | 11346 | `	int i,rc;` |
|        - | 11347 | `	/* Point to the target VM */` |
|      ! 0 | 11348 | `	pVm = pCtx->pVm;` |
|        - | 11349 | `	/* Output */` |
|      ! 0 | 11350 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 11351 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 11352 | `		if( nDataLen > 0 ){` |
|      ! 0 | 11353 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 11354 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 11355 | `			if( rc == SXERR_ABORT ){` |
|        - | 11356 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 11357 | `				return PH7_ABORT;` |
|        - | 11358 | `			}` |
|      ! 0 | 11359 | `		}` |
|      ! 0 | 11360 | `	}` |
|      ! 0 | 11361 | `	return SXRET_OK;` |
|      ! 0 | 11362 |  |
|        - | 11363 | `/*` |
|        - | 11364 | ` * int print($string...)` |
|        - | 11365 | ` *  Output one or more messages.` |
|        - | 11366 | ` * Parameters` |
|        - | 11367 | ` *  $string` |
|        - | 11368 | ` *   Message to output.` |
|        - | 11369 | ` * Return` |
|        - | 11370 | ` *  1 always.` |
|        - | 11371 | ` */` |
|        2 | 11372 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11373 |  |
|        - | 11374 | `	const char *zData;` |
|        3 | 11375 | `	int nDataLen = 0;` |
|        - | 11376 | `	ph7_vm *pVm;` |
|        - | 11377 | `	int i,rc;` |
|        - | 11378 | `	/* Point to the target VM */` |
|        3 | 11379 | `	pVm = pCtx->pVm;` |
|        - | 11380 | `	/* Output */` |
|        5 | 11381 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 11382 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 11383 | `		if( nDataLen > 0 ){` |
|        3 | 11384 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 11385 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 11386 | `			if( rc == SXERR_ABORT ){` |
|        - | 11387 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 11388 | `				return PH7_ABORT;` |
|        - | 11389 | `			}` |
|        1 | 11390 | `		}` |
|        2 | 11391 | `	}` |
|        - | 11392 | `	/* Return 1 */` |
|        3 | 11393 | `	ph7_result_int(pCtx,1);` |
|        3 | 11394 | `	return SXRET_OK;` |
|        2 | 11395 |  |
|        - | 11396 | `/*` |
|        - | 11397 | ` * void exit(string $msg)` |
|        - | 11398 | ` * void exit(int $status)` |
|        - | 11399 | ` * void die(string $ms)` |
|        - | 11400 | ` * void die(int $status)` |
|        - | 11401 | ` *   Output a message and terminate program execution.` |
|        - | 11402 | ` * Parameter` |
|        - | 11403 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 11404 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 11405 | ` *  and not printed` |
|        - | 11406 | ` * Return` |
|        - | 11407 | ` *  NULL` |
|        - | 11408 | ` */` |
|      ! 0 | 11409 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 11410 |  |
|      ! 0 | 11411 | `	if( nArg > 0 ){` |
|      ! 0 | 11412 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 11413 | `			const char *zData;` |
|      ! 0 | 11414 | `			int iLen = 0;` |
|        - | 11415 | `			/* Print exit message */` |
|      ! 0 | 11416 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 11417 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 11418 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 11419 | `			sxi32 iExitStatus;` |
|        - | 11420 | `			/* Record exit status code */` |
|      ! 0 | 11421 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 11422 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 11423 | `		}` |
|      ! 0 | 11424 | `	}` |
|        - | 11425 | `	/* Check if we are in an included file */` |
|      ! 0 | 11426 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - | 11427 | `		/* Exit the entire process */` |
|      ! 0 | 11428 | `		exit(pCtx->pVm->iExitStatus);` |
|        - | 11429 | `	}` |
|        - | 11430 | `	/* Abort processing immediately */` |
|      ! 0 | 11431 | `	return PH7_ABORT;` |
|      ! 0 | 11432 |  |
|        - | 11433 | `/*` |
|        - | 11434 | ` * bool isset($var,...)` |
|        - | 11435 | ` *  Finds out whether a variable is set.` |
|        - | 11436 | ` * Parameters` |
|        - | 11437 | ` *  One or more variable to check.` |
|        - | 11438 | ` * Return` |
|        - | 11439 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 11440 | ` */` |
|    86062 | 11441 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11442 |  |
|        - | 11443 | `	ph7_value *pObj;` |
|    86064 | 11444 | `	int res = 0;` |
|        - | 11445 | `	int i;` |
|    86064 | 11446 | `	if( nArg < 1 ){` |
|        - | 11447 | `		/* Missing arguments,return false */` |
|      ! 0 | 11448 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 11449 | `		return SXRET_OK;` |
|        - | 11450 | `	}` |
|        - | 11451 | `	/* Iterate over available arguments */` |
|   112662 | 11452 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    86064 | 11453 | `		pObj = apArg[i];` |
|    86064 | 11454 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    58692 | 11455 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 11456 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 11457 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 11458 | `			}` |
|    29345 | 11459 | `		}` |
|    86064 | 11460 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    86064 | 11461 | `		if( !res ){` |
|        - | 11462 | `			/* Variable not set,return FALSE */` |
|    59466 | 11463 | `			ph7_result_bool(pCtx,0);` |
|    59466 | 11464 | `			return SXRET_OK;` |
|        - | 11465 | `		}` |
|    13301 | 11466 | `	}` |
|        - | 11467 | `	/* All given variable are set,return TRUE */` |
|    26600 | 11468 | `	ph7_result_bool(pCtx,1);` |
|    26600 | 11469 | `	return SXRET_OK;` |
|    43033 | 11470 |  |
|        - | 11471 | `/*` |
|        - | 11472 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 11473 | ` * frame,the reference table and discard it's contents.` |
|        - | 11474 | ` * This function never fail and always return SXRET_OK.` |
|        - | 11475 | ` */` |
|  3086426 | 11476 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 11477 |  |
|        - | 11478 | `	ph7_value *pObj;` |
|        - | 11479 | `	VmRefObj *pRef;` |
|  3086428 | 11480 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3086428 | 11481 | `	if( pObj ){` |
|        - | 11482 | `		/* Release the object */` |
|  3086428 | 11483 | `		PH7_MemObjRelease(pObj);` |
|  1543213 | 11484 | `	}` |
|        - | 11485 | `	/* Remove old reference links */` |
|  3086428 | 11486 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3086428 | 11487 | `	if( pRef ){` |
|  3086422 | 11488 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 11489 | `		/* Unlink from the reference table */` |
|  3086422 | 11490 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3086422 | 11491 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 11492 | `			VmSlot sFree;` |
|        - | 11493 | `			/* Restore to the free list */` |
|  3086414 | 11494 | `			sFree.nIdx = nObjIdx;` |
|  3086414 | 11495 | `			sFree.pUserData = 0;` |
|  3086414 | 11496 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1543206 | 11497 | `		}` |
|  1543210 | 11498 | `	}` |
|  3086428 | 11499 | `	return SXRET_OK;` |
|        2 | 11500 |  |
|        - | 11501 | `/*` |
|        - | 11502 | ` * void unset($var,...)` |
|        - | 11503 | ` *   Unset one or more given variable.` |
|        - | 11504 | ` * Parameters` |
|        - | 11505 | ` *  One or more variable to unset.` |
|        - | 11506 | ` * Return` |
|        - | 11507 | ` *  Nothing.` |
|        - | 11508 | ` */` |
|     7304 | 11509 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11510 |  |
|        - | 11511 | `	ph7_value *pObj;` |
|        - | 11512 | `	ph7_vm *pVm;` |
|        - | 11513 | `	int i;` |
|        - | 11514 | `	/* Point to the target VM */` |
|     7306 | 11515 | `	pVm = pCtx->pVm;` |
|        - | 11516 | `	/* Iterate and unset */` |
|    14610 | 11517 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7306 | 11518 | `		pObj = apArg[i];` |
|     7306 | 11519 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 | 11520 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 11521 | `				/* Throw an error */` |
|      ! 0 | 11522 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 11523 | `			}` |
|      ! 0 | 11524 | `		}else{` |
|     7306 | 11525 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 11526 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     7306 | 11527 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     7300 | 11528 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3649 | 11529 | `			}` |
|        - | 11530 | `		}` |
|     3654 | 11531 | `	}` |
|     7306 | 11532 | `	return SXRET_OK;` |
|        2 | 11533 |  |
|        - | 11534 | `/*` |
|        - | 11535 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 11536 | ` */` |
|      110 | 11537 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11538 |  |
|      111 | 11539 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 | 11540 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 11541 | `	ph7_value *pObj;` |
|        - | 11542 | `	sxu32 nIdx;` |
|        - | 11543 | `	/* Extract the memory object */` |
|      111 | 11544 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 | 11545 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 | 11546 | `	if( pObj ){` |
|      111 | 11547 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 | 11548 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 11549 | `				SyString sName;` |
|        - | 11550 | `				ph7_value sKey;` |
|        - | 11551 | `				/* Perform the insertion */` |
|      109 | 11552 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 | 11553 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 | 11554 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 | 11555 | `				PH7_MemObjRelease(&sKey);` |
|       54 | 11556 | `			}` |
|       54 | 11557 | `		}` |
|       55 | 11558 | `	}` |
|      111 | 11559 | `	return SXRET_OK;` |
|        1 | 11560 |  |
|        - | 11561 | `/*` |
|        - | 11562 | ` * array get_defined_vars(void)` |
|        - | 11563 | ` *  Returns an array of all defined variables.` |
|        - | 11564 | ` * Parameter` |
|        - | 11565 | ` *  None` |
|        - | 11566 | ` * Return` |
|        - | 11567 | ` *  An array with all the variables defined in the current scope.` |
|        - | 11568 | ` */` |
|        2 | 11569 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11570 |  |
|        3 | 11571 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11572 | `	ph7_value *pArray;` |
|        - | 11573 | `	/* Create a new array */` |
|        3 | 11574 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11575 | ` 	if( pArray == 0 ){` |
|      ! 0 | 11576 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11577 | `		SXUNUSED(apArg);` |
|        - | 11578 | `		/* Return NULL */` |
|      ! 0 | 11579 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11580 | `		return SXRET_OK;` |
|        - | 11581 | `	}` |
|        - | 11582 | `	/* Superglobals first */` |
|        3 | 11583 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 11584 | `	/* Then variable defined in the current frame */` |
|        3 | 11585 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 11586 | `	/* Finally,return the created array */` |
|        3 | 11587 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11588 | `	return SXRET_OK;` |
|        2 | 11589 |  |
|        - | 11590 | `/*` |
|        - | 11591 | ` * bool gettype($var)` |
|        - | 11592 | ` *  Get the type of a variable` |
|        - | 11593 | ` * Parameters` |
|        - | 11594 | ` *   $var` |
|        - | 11595 | ` *    The variable being type checked.` |
|        - | 11596 | ` * Return` |
|        - | 11597 | ` *   String representation of the given variable type.` |
|        - | 11598 | ` */` |
|       32 | 11599 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11600 |  |
|       34 | 11601 | `	const char *zType = "Empty";` |
|       34 | 11602 | `	if( nArg > 0 ){` |
|       34 | 11603 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 11604 | `	}` |
|        - | 11605 | `	/* Return the variable type */` |
|       34 | 11606 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 11607 | `	return SXRET_OK;` |
|        2 | 11608 |  |
|        - | 11609 | `/*` |
|        - | 11610 | ` * string get_resource_type(resource $handle)` |
|        - | 11611 | ` *  This function gets the type of the given resource.` |
|        - | 11612 | ` * Parameters` |
|        - | 11613 | ` *  $handle` |
|        - | 11614 | ` *  The evaluated resource handle.` |
|        - | 11615 | ` * Return` |
|        - | 11616 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 11617 | ` *  representing its type. If the type is not identified by this function` |
|        - | 11618 | ` *  the return value will be the string Unknown.` |
|        - | 11619 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 11620 | ` *  is not a resource.` |
|        - | 11621 | ` */` |
|        2 | 11622 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11623 |  |
|        3 | 11624 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 11625 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 11626 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11627 | `		return PH7_OK;` |
|        - | 11628 | `	}` |
|        3 | 11629 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 11630 | `	return SXRET_OK;` |
|        2 | 11631 |  |
|        - | 11632 | `/*` |
|        - | 11633 | ` * void var_dump(expression,....)` |
|        - | 11634 | ` *   var_dump � Dumps information about a variable` |
|        - | 11635 | ` * Parameters` |
|        - | 11636 | ` *   One or more expression to dump.` |
|        - | 11637 | ` * Returns` |
|        - | 11638 | ` *  Nothing.` |
|        - | 11639 | ` */` |
|      218 | 11640 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11641 |  |
|        - | 11642 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 11643 | `	int i;` |
|      220 | 11644 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 11645 | `	/* Dump one or more expressions */` |
|      444 | 11646 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 11647 | `		ph7_value *pObj = apArg[i];` |
|        - | 11648 | `		/* Reset the working buffer */` |
|      226 | 11649 | `		SyBlobReset(&sDump);` |
|        - | 11650 | `		/* Dump the given expression */` |
|      226 | 11651 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 11652 | `		/* Output */` |
|      226 | 11653 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 11654 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 11655 | `		}` |
|      114 | 11656 | `	}` |
|        - | 11657 | `	/* Release the working buffer */` |
|      220 | 11658 | `	SyBlobRelease(&sDump);` |
|      220 | 11659 | `	return SXRET_OK;` |
|        2 | 11660 |  |
|        - | 11661 | `/*` |
|        - | 11662 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 11663 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 11664 | ` * Parameters` |
|        - | 11665 | ` *   expression: Expression to dump` |
|        - | 11666 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 11667 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 11668 | ` *            print_r() will return the information rather than print it.` |
|        - | 11669 | ` * Return` |
|        - | 11670 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 11671 | ` *  Otherwise, the return value is TRUE.` |
|        - | 11672 | ` */` |
|       16 | 11673 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11674 |  |
|       17 | 11675 | `	int ret_string = 0;` |
|        - | 11676 | `	SyBlob sDump;` |
|       17 | 11677 | `	if( nArg < 1 ){` |
|        - | 11678 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 11679 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11680 | `		return SXRET_OK;` |
|        - | 11681 | `	}` |
|       17 | 11682 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 11683 | `	if ( nArg > 1 ){` |
|        - | 11684 | `		/* Where to redirect output */` |
|       11 | 11685 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 11686 | `	}` |
|        - | 11687 | `	/* Generate dump */` |
|       17 | 11688 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 11689 | `	if( !ret_string ){` |
|        - | 11690 | `		/* Output dump */` |
|        7 | 11691 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11692 | `		/* Return true */` |
|        7 | 11693 | `		ph7_result_bool(pCtx,1);` |
|        4 | 11694 | `	}else{` |
|        - | 11695 | `		/* Generated dump as return value */` |
|       11 | 11696 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11697 | `	}` |
|        - | 11698 | `	/* Release the working buffer */` |
|       17 | 11699 | `	SyBlobRelease(&sDump);` |
|       17 | 11700 | `	return SXRET_OK;` |
|        9 | 11701 |  |
|        - | 11702 | `/*` |
|        - | 11703 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 11704 | ` * Same job as print_r. (see coment above)` |
|        - | 11705 | ` */` |
|        2 | 11706 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11707 |  |
|        3 | 11708 | `	int ret_string = 0;` |
|        - | 11709 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 11710 | `	if( nArg < 1 ){` |
|        - | 11711 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 11712 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11713 | `		return SXRET_OK;` |
|        - | 11714 | `	}` |
|        3 | 11715 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 11716 | `	if ( nArg > 1 ){` |
|        - | 11717 | `		/* Where to redirect output */` |
|        3 | 11718 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 11719 | `	}` |
|        - | 11720 | `	/* Generate dump */` |
|        3 | 11721 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 11722 | `	if( !ret_string ){` |
|        - | 11723 | `		/* Output dump */` |
|      ! 0 | 11724 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11725 | `		/* Return NULL */` |
|      ! 0 | 11726 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11727 | `	}else{` |
|        - | 11728 | `		/* Generated dump as return value */` |
|        3 | 11729 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11730 | `	}` |
|        - | 11731 | `	/* Release the working buffer */` |
|        3 | 11732 | `	SyBlobRelease(&sDump);` |
|        3 | 11733 | `	return SXRET_OK;` |
|        2 | 11734 |  |
|        - | 11735 | `/*` |
|        - | 11736 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 11737 | ` *  Set/get the various assert flags.` |
|        - | 11738 | ` * Parameter` |
|        - | 11739 | ` * $what` |
|        - | 11740 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 11741 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 11742 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 11743 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 11744 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 11745 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 11746 | ` * $value` |
|        - | 11747 | ` *   An optional new value for the option.` |
|        - | 11748 | ` * Return` |
|        - | 11749 | ` *  Old setting on success or FALSE on failure.` |
|        - | 11750 | ` */` |
|       28 | 11751 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11752 |  |
|       30 | 11753 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11754 | `	int iOption;` |
|        - | 11755 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 11756 | `	if( nArg < 1 ){` |
|        3 | 11757 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11758 | `			"ArgumentCountError",` |
|        - | 11759 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 11760 | `			);` |
|        - | 11761 | `	}` |
|        - | 11762 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 11763 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 11764 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 11765 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11766 | `			"TypeError",` |
|        - | 11767 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 11768 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 11769 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 11770 | `			);` |
|        - | 11771 | `	}` |
|       28 | 11772 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 11773 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 11774 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 11775 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 11776 | `	switch( iOption ){` |
|        5 | 11777 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 11778 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 11779 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 11780 | `		if( nArg > 1 ){` |
|        5 | 11781 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 11782 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 11783 | `			}else{` |
|        3 | 11784 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 11785 | `			}` |
|        2 | 11786 | `		}` |
|       12 | 11787 | `		break;` |
|        1 | 11788 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 11789 | `		/* Return old callback or null */` |
|        3 | 11790 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 11791 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 11792 | `		}else{` |
|        3 | 11793 | `			ph7_result_null(pCtx);` |
|        - | 11794 | `		}` |
|        3 | 11795 | `		if( nArg > 1 ){` |
|      ! 0 | 11796 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 11797 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 11798 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 11799 | `			}else{` |
|      ! 0 | 11800 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 11801 | `			}` |
|      ! 0 | 11802 | `		}` |
|        3 | 11803 | `		break;` |
|        5 | 11804 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 11805 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 11806 | `		if( nArg > 1 ){` |
|        5 | 11807 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 11808 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 11809 | `			}else{` |
|        3 | 11810 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 11811 | `			}` |
|        2 | 11812 | `		}` |
|       11 | 11813 | `		break;` |
|      ! 0 | 11814 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 11815 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 11816 | `		break;` |
|        1 | 11817 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 11818 | `		ph7_result_int(pCtx, 1);` |
|        3 | 11819 | `		break;` |
|      ! 0 | 11820 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 11821 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 11822 | `		break;` |
|        1 | 11823 | `	default:` |
|        - | 11824 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 11825 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11826 | `			"ValueError",` |
|        - | 11827 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 11828 | `			);` |
|        - | 11829 | `	}` |
|       26 | 11830 | `	return PH7_OK;` |
|       16 | 11831 |  |
|        - | 11832 | `/*` |
|        - | 11833 | ` * bool assert(mixed $assertion)` |
|        - | 11834 | ` *  Checks if assertion is FALSE.` |
|        - | 11835 | ` * Parameter` |
|        - | 11836 | ` *  $assertion` |
|        - | 11837 | ` *    The assertion to test.` |
|        - | 11838 | ` * Return` |
|        - | 11839 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 11840 | ` */` |
|       24 | 11841 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11842 |  |
|       26 | 11843 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11844 | `	int iFlags,iResult;` |
|        - | 11845 | `	const char *zDesc;` |
|        - | 11846 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 11847 | `	if( nArg < 1 ){` |
|        3 | 11848 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11849 | `			"ArgumentCountError",` |
|        - | 11850 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 11851 | `			);` |
|        - | 11852 | `	}` |
|       24 | 11853 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 11854 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 11855 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 11856 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 11857 | `		return PH7_OK;` |
|        - | 11858 | `	}` |
|        - | 11859 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 11860 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 11861 | `	if( !iResult ){` |
|        - | 11862 | `		/* Assertion failed */` |
|        - | 11863 | `		/* Extract optional description */` |
|       13 | 11864 | `		zDesc = 0;` |
|       13 | 11865 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 11866 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 11867 | `		}` |
|       13 | 11868 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 11869 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 11870 | `			ph7_value sFile,sLine;` |
|        - | 11871 | `			ph7_value *apCbArg[3];` |
|        - | 11872 | `			SyString *pFile;` |
|        - | 11873 | `			/* Extract the processed script */` |
|      ! 0 | 11874 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 11875 | `			if( pFile == 0 ){` |
|      ! 0 | 11876 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 11877 | `			}` |
|        - | 11878 | `			/* Invoke the callback */` |
|      ! 0 | 11879 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 11880 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 11881 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 11882 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 11883 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 11884 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 11885 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 11886 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 11887 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 11888 | `		}` |
|       13 | 11889 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 11890 | `			/* Abort VM execution immediately */` |
|      ! 0 | 11891 | `			return PH7_ABORT;` |
|        - | 11892 | `		}` |
|        - | 11893 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 11894 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 11895 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11896 | `				"AssertionError",` |
|        - | 11897 | `				"%s",` |
|        1 | 11898 | `				zDesc` |
|        - | 11899 | `				);` |
|      ! 0 | 11900 | `		}else{` |
|       11 | 11901 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11902 | `				"AssertionError",` |
|        - | 11903 | `				"assert(false)"` |
|        - | 11904 | `				);` |
|        - | 11905 | `		}` |
|        - | 11906 | `	}` |
|        - | 11907 | `	/* Assertion passed */` |
|       11 | 11908 | `	ph7_result_bool(pCtx,1);` |
|       11 | 11909 | `	return PH7_OK;` |
|       14 | 11910 |  |
|        - | 11911 | `/*` |
|        - | 11912 | ` * Section:` |
|        - | 11913 | ` *  Error reporting functions.` |
|        - | 11914 | ` * Status:` |
|        - | 11915 | ` *    Stable.` |
|        - | 11916 | ` */` |
|        - | 11917 | `/*` |
|        - | 11918 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 11919 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 11920 | ` * Parameters` |
|        - | 11921 | ` *  $error_msg` |
|        - | 11922 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 11923 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 11924 | ` * $error_type` |
|        - | 11925 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 11926 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 11927 | ` * Return` |
|        - | 11928 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 11929 | ` */` |
|       12 | 11930 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11931 |  |
|       14 | 11932 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 11933 | `	int rc = PH7_OK;` |
|       14 | 11934 | `	if( nArg > 0 ){` |
|        - | 11935 | `		const char *zErr;` |
|        - | 11936 | `		int nLen;` |
|        - | 11937 | `		/* Extract the error message */` |
|       12 | 11938 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 11939 | `		if( nArg > 1 ){` |
|        - | 11940 | `			/* Extract the error type */` |
|       12 | 11941 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 11942 | `			switch( nErr ){` |
|        1 | 11943 | `			case 1:   /* E_ERROR */` |
|        - | 11944 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 11945 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 11946 | `			case 256: /* E_USER_ERROR */` |
|        3 | 11947 | `				nErr = PH7_CTX_ERR;` |
|        3 | 11948 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 11949 | `				break;` |
|        1 | 11950 | `			case 2:   /* E_WARNING */` |
|        - | 11951 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 11952 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 11953 | `			case 512: /* E_USER_WARNING */` |
|        3 | 11954 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 11955 | `				break;` |
|        3 | 11956 | `			default:` |
|        8 | 11957 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 11958 | `				break;` |
|        - | 11959 | `			}` |
|        5 | 11960 | `		}` |
|        - | 11961 | `		/* Report error */` |
|       12 | 11962 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 11963 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 11964 | `			return rc;` |
|        - | 11965 | `		}` |
|        - | 11966 | `		/* Return true */` |
|       12 | 11967 | `		ph7_result_bool(pCtx,1);` |
|        7 | 11968 | `	}else{` |
|        - | 11969 | `		/* Missing arguments,return FALSE */` |
|        3 | 11970 | `		ph7_result_bool(pCtx,0);` |
|        - | 11971 | `	}` |
|       14 | 11972 | `	return rc;` |
|        8 | 11973 |  |
|        - | 11974 | `/*` |
|        - | 11975 | ` * int error_reporting([int $level])` |
|        - | 11976 | ` *  Sets which PHP errors are reported.` |
|        - | 11977 | ` * Parameters` |
|        - | 11978 | ` *  $level` |
|        - | 11979 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 11980 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 11981 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 11982 | ` *   levels will not always behave as expected.` |
|        - | 11983 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 11984 | ` *   in the predefined constants.` |
|        - | 11985 | ` * Return` |
|        - | 11986 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 11987 | ` *   parameter is given.` |
|        - | 11988 | ` */` |
|       38 | 11989 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11990 |  |
|       40 | 11991 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11992 | `	int nOld;` |
|        - | 11993 | `	/* Extract the old reporting level */` |
|       40 | 11994 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 | 11995 | `	if( nArg > 0 ){` |
|        - | 11996 | `		int nNew;` |
|        - | 11997 | `		/* Extract the desired error reporting level */` |
|       32 | 11998 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 | 11999 | `		if( !nNew ){` |
|        - | 12000 | `			/* Do not report errors at all */` |
|        5 | 12001 | `			pVm->bErrReport = 0;` |
|        3 | 12002 | `		}else{` |
|        - | 12003 | `			/* Report all errors */` |
|       28 | 12004 | `			pVm->bErrReport = 1;` |
|        - | 12005 | `		}` |
|       15 | 12006 | `	}` |
|        - | 12007 | `	/* Return the old level */` |
|       40 | 12008 | `	ph7_result_int(pCtx,nOld);` |
|       40 | 12009 | `	return PH7_OK;` |
|        2 | 12010 |  |
|        - | 12011 | `/*` |
|        - | 12012 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 12013 | ` *  Send an error message somewhere.` |
|        - | 12014 | ` * Parameter` |
|        - | 12015 | ` *  $message` |
|        - | 12016 | ` *   The error message that should be logged.` |
|        - | 12017 | ` *  $message_type` |
|        - | 12018 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 12019 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 12020 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 12021 | ` *       This is the default option.` |
|        - | 12022 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 12023 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 12024 | ` *    2  No longer an option.` |
|        - | 12025 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 12026 | ` *       to the end of the message string.` |
|        - | 12027 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 12028 | ` *  $destination` |
|        - | 12029 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 12030 | ` *  $extra_headers` |
|        - | 12031 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 12032 | ` * Return` |
|        - | 12033 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12034 | ` * NOTE:` |
|        - | 12035 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 12036 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 12037 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 12038 | ` *  Otherwise this function is no-op.` |
|        - | 12039 | ` */` |
|        4 | 12040 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12041 |  |
|        - | 12042 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 12043 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 12044 | `	int iType = 0;` |
|        5 | 12045 | `	if( nArg < 1 ){` |
|        - | 12046 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 12047 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12048 | `		return PH7_OK;` |
|        - | 12049 | `	}` |
|        5 | 12050 | `	if( pVm->xErrLog  ){` |
|        - | 12051 | `		/* Invoke the user callback */` |
|      ! 0 | 12052 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 12053 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 12054 | `		if( nArg > 1 ){` |
|      ! 0 | 12055 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 12056 | `			if( nArg > 2 ){` |
|      ! 0 | 12057 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 12058 | `				if( nArg > 3 ){` |
|      ! 0 | 12059 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 12060 | `				}` |
|      ! 0 | 12061 | `			}` |
|      ! 0 | 12062 | `		}` |
|      ! 0 | 12063 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 12064 | `	}` |
|        - | 12065 | `	/* Retun TRUE */` |
|        5 | 12066 | `	ph7_result_bool(pCtx,1);` |
|        5 | 12067 | `	return PH7_OK;` |
|        3 | 12068 |  |
|        - | 12069 | `/*` |
|        - | 12070 | ` * bool restore_exception_handler(void)` |
|        - | 12071 | ` *  Restores the previously defined exception handler function.` |
|        - | 12072 | ` * Parameter` |
|        - | 12073 | ` *  None` |
|        - | 12074 | ` * Return` |
|        - | 12075 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 12076 | ` */` |
|        4 | 12077 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12078 |  |
|        5 | 12079 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12080 | `	ph7_value *pOld,*pNew;` |
|        - | 12081 | `	/* Point to the old and the new handler */` |
|        5 | 12082 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 12083 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 12084 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 12085 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 12086 | `		SXUNUSED(apArg);` |
|        - | 12087 | `		/* No installed handler,return FALSE */` |
|        5 | 12088 | `		ph7_result_bool(pCtx,0);` |
|        5 | 12089 | `		return PH7_OK;` |
|        - | 12090 | `	}` |
|        - | 12091 | `	/* Copy the old handler */` |
|      ! 0 | 12092 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 12093 | `	PH7_MemObjRelease(pOld);` |
|        - | 12094 | `	/* Return TRUE */` |
|      ! 0 | 12095 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 12096 | `	return PH7_OK;` |
|        3 | 12097 |  |
|        - | 12098 | `/*` |
|        - | 12099 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 12100 | ` *  Sets a user-defined exception handler function.` |
|        - | 12101 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 12102 | ` * NOTE` |
|        - | 12103 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 12104 | ` *  the satndard PHP engine.` |
|        - | 12105 | ` * Parameters` |
|        - | 12106 | ` *  $exception_handler` |
|        - | 12107 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 12108 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 12109 | ` *   that was thrown.` |
|        - | 12110 | ` *  Note:` |
|        - | 12111 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 12112 | ` * Return` |
|        - | 12113 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 12114 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 12115 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 12116 | ` */` |
|        4 | 12117 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12118 |  |
|        6 | 12119 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12120 | `	ph7_value *pOld,*pNew;` |
|        - | 12121 | `	/* Point to the old and the new handler */` |
|        6 | 12122 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 12123 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 12124 | `	/* Return the old handler */` |
|        6 | 12125 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 12126 | `	if( nArg > 0 ){` |
|        6 | 12127 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 12128 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 12129 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 12130 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 12131 | `		}else{` |
|        6 | 12132 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 12133 | `			/* Install the new handler */` |
|        6 | 12134 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 12135 | `		}` |
|        2 | 12136 | `	}` |
|        6 | 12137 | `	return PH7_OK;` |
|        2 | 12138 |  |
|        - | 12139 | `/*` |
|        - | 12140 | ` * bool restore_error_handler(void)` |
|        - | 12141 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 12142 | ` * Parameters:` |
|        - | 12143 | ` *  None.` |
|        - | 12144 | ` * Return` |
|        - | 12145 | ` *  Always TRUE.` |
|        - | 12146 | ` */` |
|        6 | 12147 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12148 |  |
|        7 | 12149 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12150 | `	ph7_value *pOld,*pNew;` |
|        - | 12151 | `	/* Point to the old and the new handler */` |
|        7 | 12152 | `	pOld = &pVm->aErrCB[0];` |
|        7 | 12153 | `	pNew = &pVm->aErrCB[1];` |
|        7 | 12154 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 12155 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 12156 | `		SXUNUSED(apArg);` |
|        - | 12157 | `		/* No installed callback,return FALSE */` |
|        7 | 12158 | `		ph7_result_bool(pCtx,0);` |
|        7 | 12159 | `		return PH7_OK;` |
|        - | 12160 | `	}` |
|        - | 12161 | `	/* Copy the old callback */` |
|      ! 0 | 12162 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 12163 | `	PH7_MemObjRelease(pOld);` |
|        - | 12164 | `	/* Return TRUE */` |
|      ! 0 | 12165 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 12166 | `	return PH7_OK;` |
|        4 | 12167 |  |
|        - | 12168 | `/*` |
|        - | 12169 | ` * value set_error_handler(callable $error_handler)` |
|        - | 12170 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 12171 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 12172 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 12173 | ` *  Sets a user-defined error handler function.` |
|        - | 12174 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 12175 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 12176 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 12177 | ` *  conditions (using trigger_error()).` |
|        - | 12178 | ` * Parameters` |
|        - | 12179 | ` *  $error_handler` |
|        - | 12180 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 12181 | ` *   describing the error.` |
|        - | 12182 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 12183 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 12184 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 12185 | ` *   The function can be shown as:` |
|        - | 12186 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 12187 | ` *     errno` |
|        - | 12188 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 12189 | ` *   errstr` |
|        - | 12190 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 12191 | ` *   errfile` |
|        - | 12192 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 12193 | ` *     was raised in, as a string.` |
|        - | 12194 | ` *  Note:` |
|        - | 12195 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 12196 | ` * Return` |
|        - | 12197 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 12198 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 12199 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 12200 | ` */` |
|    10160 | 12201 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12202 |  |
|    10162 | 12203 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12204 | `	ph7_value *pOld,*pNew;` |
|        - | 12205 | `	/* Point to the old and the new handler */` |
|    10162 | 12206 | `	pOld = &pVm->aErrCB[0];` |
|    10162 | 12207 | `	pNew = &pVm->aErrCB[1];` |
|        - | 12208 | `	/* Return the old handler */` |
|    10162 | 12209 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    10162 | 12210 | `	if( nArg > 0 ){` |
|    10162 | 12211 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 12212 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5079 | 12213 | `			PH7_MemObjRelease(pNew);` |
|     5079 | 12214 | `			ph7_result_bool(pCtx,1);` |
|     2540 | 12215 | `		}else{` |
|     5084 | 12216 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 12217 | `			/* Install the new handler */` |
|     5084 | 12218 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 12219 | `		}` |
|     5080 | 12220 | `	}` |
|    10162 | 12221 | `	return PH7_OK;` |
|        2 | 12222 |  |
|        - | 12223 | `/*` |
|        - | 12224 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 12225 | ` *  Generates a backtrace.` |
|        - | 12226 | ` * Paramaeter` |
|        - | 12227 | ` *  $options` |
|        - | 12228 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 12229 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 12230 | ` *   all the function/method arguments, to save memory.` |
|        - | 12231 | ` * $limit` |
|        - | 12232 | ` *   (Not Used)` |
|        - | 12233 | ` * Return` |
|        - | 12234 | ` *  An array.The possible returned elements are as follows:` |
|        - | 12235 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 12236 | ` *          Name        Type      Description` |
|        - | 12237 | ` *          ------      ------     -----------` |
|        - | 12238 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 12239 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 12240 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 12241 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 12242 | ` *          object      object    The current object.` |
|        - | 12243 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 12244 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 12245 | ` */` |
|      734 | 12246 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12247 |  |
|      736 | 12248 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12249 | `	ph7_value *pArray;` |
|        - | 12250 | `	ph7_class *pClass;` |
|        - | 12251 | `	ph7_value *pValue;` |
|        - | 12252 | `	SyString *pFile;` |
|        - | 12253 | `	/* Create a new array */` |
|      736 | 12254 | `	pArray = ph7_context_new_array(pCtx);` |
|      736 | 12255 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      736 | 12256 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 12257 | `		/* Out of memory,return NULL */` |
|      ! 0 | 12258 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 12259 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12260 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12261 | `		SXUNUSED(apArg);` |
|      ! 0 | 12262 | `		return PH7_OK;` |
|        - | 12263 | `	}` |
|        - | 12264 | `	/* Dump running function name and it's arguments  */` |
|      736 | 12265 | `	if( pVm->pFrame->pParent ){` |
|      736 | 12266 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 12267 | `		ph7_vm_func *pFunc;` |
|        - | 12268 | `		ph7_value *pArg;` |
|      736 | 12269 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      736 | 12270 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      736 | 12271 | `		if( pFrame->pParent && pFunc ){` |
|      736 | 12272 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      736 | 12273 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      736 | 12274 | `			ph7_value_reset_string_cursor(pValue);` |
|      367 | 12275 | `		}` |
|        - | 12276 | `		/* Function arguments */` |
|      736 | 12277 | `		pArg = ph7_context_new_array(pCtx);` |
|      736 | 12278 | `		if( pArg  ){` |
|        - | 12279 | `			ph7_value *pObj;` |
|        - | 12280 | `			VmSlot *aSlot;` |
|        - | 12281 | `			sxu32 n;` |
|        - | 12282 | `			/* Start filling the array with the given arguments */` |
|      736 | 12283 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2942 | 12284 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2208 | 12285 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2208 | 12286 | `				if( pObj ){` |
|     2208 | 12287 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1103 | 12288 | `				}` |
|     1105 | 12289 | `			}` |
|        - | 12290 | `			/* Save the array */` |
|      736 | 12291 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      367 | 12292 | `		}` |
|      367 | 12293 | `	}` |
|      736 | 12294 | `	ph7_value_int(pValue,1);` |
|        - | 12295 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 12296 | `	 * line numbers at run-time. )` |
|        - | 12297 | `	 */` |
|      736 | 12298 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 12299 | `	/* Current processed script */` |
|      736 | 12300 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      736 | 12301 | `	if( pFile ){` |
|      736 | 12302 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      736 | 12303 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      736 | 12304 | `		ph7_value_reset_string_cursor(pValue);` |
|      367 | 12305 | `	}` |
|        - | 12306 | `	/* Top class */` |
|      736 | 12307 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      736 | 12308 | `	if( pClass ){` |
|      732 | 12309 | `		ph7_value_reset_string_cursor(pValue);` |
|      732 | 12310 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      732 | 12311 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      365 | 12312 | `	}` |
|        - | 12313 | `	/* Return the freshly created array */` |
|      736 | 12314 | `	ph7_result_value(pCtx,pArray);` |
|        - | 12315 | `	/*` |
|        - | 12316 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 12317 | `	 * as soon we return from this function.` |
|        - | 12318 | `	 */` |
|      736 | 12319 | `	return PH7_OK;` |
|      369 | 12320 |  |
|        - | 12321 | `/*` |
|        - | 12322 | ` * Generate a small backtrace.` |
|        - | 12323 | ` * Store the generated dump in the given BLOB` |
|        - | 12324 | ` */` |
|        4 | 12325 | `static int VmMiniBacktrace(` |
|        - | 12326 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 12327 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 12328 | `	)` |
|        1 | 12329 |  |
|        5 | 12330 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12331 | `	ph7_vm_func *pFunc;` |
|        - | 12332 | `	ph7_class *pClass;` |
|        - | 12333 | `	SyString *pFile;` |
|        - | 12334 | `	/* Called function */` |
|        5 | 12335 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 12336 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 12337 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 12338 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 12339 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 12340 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 12341 | `	}else{` |
|      ! 0 | 12342 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 12343 | `	}` |
|        5 | 12344 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 12345 | `	/* Current processed script */` |
|        5 | 12346 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 12347 | `	if( pFile ){` |
|        5 | 12348 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 12349 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 12350 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 12351 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 12352 | `	}` |
|        - | 12353 | `	/* Top class */` |
|        5 | 12354 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 12355 | `	if( pClass ){` |
|      ! 0 | 12356 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 12357 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 12358 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 12359 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 12360 | `	}` |
|        5 | 12361 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 12362 | `	/* All done */` |
|        5 | 12363 | `	return SXRET_OK;` |
|        1 | 12364 |  |
|        - | 12365 | `/*` |
|        - | 12366 | ` * void debug_print_backtrace()` |
|        - | 12367 | ` *  Prints a backtrace` |
|        - | 12368 | ` * Parameters` |
|        - | 12369 | ` * None` |
|        - | 12370 | ` * Return` |
|        - | 12371 | ` * NULL` |
|        - | 12372 | ` */` |
|        2 | 12373 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12374 |  |
|        3 | 12375 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12376 | `	SyBlob sDump;` |
|        3 | 12377 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 12378 | `	/* Generate the backtrace */` |
|        3 | 12379 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 12380 | `	/* Output backtrace */` |
|        3 | 12381 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12382 | `	/* All done,cleanup */` |
|        3 | 12383 | `	SyBlobRelease(&sDump);` |
|        1 | 12384 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 12385 | `	SXUNUSED(apArg);` |
|        3 | 12386 | `	return PH7_OK;` |
|        1 | 12387 |  |
|        - | 12388 | `/*` |
|        - | 12389 | ` * string debug_string_backtrace()` |
|        - | 12390 | ` *  Generate a backtrace` |
|        - | 12391 | ` * Parameters` |
|        - | 12392 | ` * None` |
|        - | 12393 | ` * Return` |
|        - | 12394 | ` *  A mini backtrace().` |
|        - | 12395 | ` * Note that this is a symisc extension.` |
|        - | 12396 | ` */` |
|        2 | 12397 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12398 |  |
|        3 | 12399 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12400 | `	SyBlob sDump;` |
|        3 | 12401 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 12402 | `	/* Generate the backtrace */` |
|        3 | 12403 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 12404 | `	/* Return the backtrace */` |
|        3 | 12405 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 12406 | `	/* All done,cleanup */` |
|        3 | 12407 | `	SyBlobRelease(&sDump);` |
|        1 | 12408 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 12409 | `	SXUNUSED(apArg);` |
|        3 | 12410 | `	return PH7_OK;` |
|        1 | 12411 |  |
|        - | 12412 | `/*` |
|        - | 12413 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 12414 | ` * exception is triggered.` |
|        - | 12415 | ` */` |
|      492 | 12416 | `static sxi32 VmUncaughtException(` |
|        - | 12417 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 12418 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 12419 | `	)` |
|        1 | 12420 |  |
|        - | 12421 | `	ph7_value *apArg[2],sArg;` |
|      493 | 12422 | `	int nArg = 1;` |
|        - | 12423 | `	sxi32 rc;` |
|      493 | 12424 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 12425 | `		/* Nesting limit reached */` |
|      ! 0 | 12426 | `		return SXRET_OK;` |
|        - | 12427 | `	}` |
|        - | 12428 | `	/* Call any exception handler if available */` |
|      493 | 12429 | `	PH7_MemObjInit(pVm,&sArg);` |
|      493 | 12430 | `	if( pThis ){` |
|        - | 12431 | `		/* Load the exception instance */` |
|      493 | 12432 | `		sArg.x.pOther = pThis;` |
|      493 | 12433 | `		pThis->iRef++;` |
|      493 | 12434 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      247 | 12435 | `	}else{` |
|      ! 0 | 12436 | `		nArg = 0;` |
|        - | 12437 | `	}` |
|      493 | 12438 | `	apArg[0] = &sArg;` |
|        - | 12439 | `	/* Call the exception handler if available */` |
|      493 | 12440 | `	pVm->nExceptDepth++;` |
|      493 | 12441 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      493 | 12442 | `	pVm->nExceptDepth--;` |
|      493 | 12443 | `	if( rc != SXRET_OK ){` |
|        - | 12444 | `		SyBlob sMsgBuf;` |
|      491 | 12445 | `		const char *zClass = "Exception";` |
|      491 | 12446 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 12447 | `		const char *zMsg;` |
|        - | 12448 | `		sxu32 nMsg;` |
|        - | 12449 | `		const char *zFuncName;` |
|        - | 12450 | `		int nFuncLen;` |
|      491 | 12451 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      491 | 12452 | `		if( pThis ){` |
|        - | 12453 | `			ph7_class_method *pGetMessage;` |
|        - | 12454 | `			ph7_value sMsg;` |
|        - | 12455 | `			const char *zTmp;` |
|        - | 12456 | `			int nTmp;` |
|      491 | 12457 | `			zClass = pThis->pClass->sName.zString;` |
|      491 | 12458 | `			nClass = pThis->pClass->sName.nByte;` |
|      491 | 12459 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      491 | 12460 | `			if( pGetMessage ){` |
|      491 | 12461 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      491 | 12462 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      491 | 12463 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      491 | 12464 | `					if( zTmp && nTmp > 0 ){` |
|      491 | 12465 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      245 | 12466 | `					}` |
|      245 | 12467 | `				}` |
|      491 | 12468 | `				PH7_MemObjRelease(&sMsg);` |
|      245 | 12469 | `			}` |
|      245 | 12470 | `		}` |
|      491 | 12471 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      491 | 12472 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      491 | 12473 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      491 | 12474 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      491 | 12475 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 12476 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      491 | 12477 | `		rc = SXERR_ABORT;` |
|      245 | 12478 | `	}` |
|      493 | 12479 | `	PH7_MemObjRelease(&sArg);` |
|      493 | 12480 | `	return rc;` |
|      247 | 12481 |  |
|        - | 12482 | `/*` |
|        - | 12483 | ` * Throw a user exception.` |
|        - | 12484 | ` *` |
|        - | 12485 | ` * Exception dispatch follows this sequence:` |
|        - | 12486 | ` *` |
|        - | 12487 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 12488 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 12489 | ` *` |
|        - | 12490 | ` * 2. If NO catch matches:` |
|        - | 12491 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 12492 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 12493 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 12494 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 12495 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 12496 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 12497 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 12498 | ` *` |
|        - | 12499 | ` * 3. If a catch DOES match:` |
|        - | 12500 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 12501 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 12502 | ` *       inside the catch body from immediately propagating past our` |
|        - | 12503 | ` *       finally block.` |
|        - | 12504 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 12505 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 12506 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 12507 | ` *       in pPendingException (step 2c).` |
|        - | 12508 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 12509 | ` *    d. Run finally (if present).` |
|        - | 12510 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 12511 | ` *       that handlers are restored and finally has run.` |
|        - | 12512 | ` */` |
|      690 | 12513 | `static sxi32 VmThrowException(` |
|        - | 12514 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 12515 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 12516 | `	)` |
|        2 | 12517 |  |
|        - | 12518 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 12519 | `	ph7_exception **apException;` |
|        - | 12520 | `	ph7_exception *pException;` |
|        - | 12521 | `	/* Point to the stack of loaded exceptions */` |
|      692 | 12522 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      692 | 12523 | `	pException = 0;` |
|      692 | 12524 | `	pCatch = 0;` |
|      692 | 12525 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 12526 | `		ph7_exception_block *aCatch;` |
|        - | 12527 | `		ph7_class *pClass;` |
|        - | 12528 | `		SyString *aNames;` |
|        - | 12529 | `		sxu32 nNames;` |
|        - | 12530 | `		int matched;` |
|        - | 12531 | `		sxu32 j,k;` |
|        - | 12532 | `		/* Locate the appropriate block to execute */` |
|      192 | 12533 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      192 | 12534 | `		(void)SySetPop(&pVm->aException);` |
|      192 | 12535 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      200 | 12536 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 12537 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      198 | 12538 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      198 | 12539 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      198 | 12540 | `			matched = 0;` |
|      224 | 12541 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 12542 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 12543 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 12544 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      216 | 12545 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      216 | 12546 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 12547 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 12548 | `					continue;` |
|        - | 12549 | `				}` |
|      216 | 12550 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      190 | 12551 | `					matched = 1;` |
|      190 | 12552 | `					break;` |
|        - | 12553 | `				}` |
|       14 | 12554 | `			}` |
|      198 | 12555 | `			if( matched ){` |
|        - | 12556 | `				/* Catch block found,break immediately */` |
|      190 | 12557 | `				pCatch = &aCatch[j];` |
|      190 | 12558 | `				break;` |
|        - | 12559 | `			}` |
|        5 | 12560 | `		}` |
|       95 | 12561 | `	}` |
|        - | 12562 | `	/* Execute the cached block if available */` |
|      692 | 12563 | `	if( pCatch == 0 ){` |
|        - | 12564 | `		sxi32 rc;` |
|        - | 12565 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      504 | 12566 | `		if( pException && pException->iHasFinally ){` |
|        3 | 12567 | `			pException->iFinallyDone = 1;` |
|        3 | 12568 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 12569 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12570 | `				return SXERR_ABORT;` |
|        - | 12571 | `			}` |
|        1 | 12572 | `		}` |
|        - | 12573 | `		/* Check if there is an outer exception handler on the stack */` |
|      504 | 12574 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 12575 | `			/* Re-throw to the outer handler */` |
|        3 | 12576 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 12577 | `		}` |
|        - | 12578 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 12579 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 12580 | `		 * exception instead of reporting it uncaught.` |
|        - | 12581 | `		 */` |
|      502 | 12582 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 12583 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 12584 | `			 * by looking for a catch frame on the stack.` |
|        - | 12585 | `			 */` |
|      502 | 12586 | `			VmFrame *pF = pVm->pFrame;` |
|      502 | 12587 | `			int inCatch = 0;` |
|     1010 | 12588 | `			while( pF ){` |
|      518 | 12589 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        9 | 12590 | `					inCatch = 1;` |
|        9 | 12591 | `					break;` |
|        - | 12592 | `				}` |
|      509 | 12593 | `				pF = pF->pParent;` |
|        1 | 12594 | `			}` |
|      502 | 12595 | `			if( inCatch ){` |
|        - | 12596 | `				/* Defer — will be re-thrown after finally runs */` |
|        9 | 12597 | `				pThis->iRef++;` |
|        9 | 12598 | `				pVm->pPendingException = pThis;` |
|        9 | 12599 | `				return SXRET_OK;` |
|        - | 12600 | `			}` |
|      246 | 12601 | `		}` |
|        - | 12602 | `		/* Truly uncaught */` |
|      493 | 12603 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      493 | 12604 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 12605 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 12606 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 12607 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 12608 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 12609 | `			}` |
|      ! 0 | 12610 | `		}` |
|      493 | 12611 | `		return rc;` |
|      ! 0 | 12612 | `	}else{` |
|      190 | 12613 | `		VmFrame *pFrame = pVm->pFrame;` |
|      190 | 12614 | `		ph7_exception **apSaved = 0;` |
|        - | 12615 | `		sxu32 nSavedCount;` |
|        - | 12616 | `		sxi32 rc;` |
|      190 | 12617 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      190 | 12618 | `		if( pException->pFrame == pFrame ){` |
|      140 | 12619 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       69 | 12620 | `		}` |
|        - | 12621 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 12622 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 12623 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 12624 | `		 */` |
|      190 | 12625 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      190 | 12626 | `		if( nSavedCount > 0 ){` |
|       16 | 12627 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        5 | 12628 | `				nSavedCount * sizeof(ph7_exception *));` |
|       11 | 12629 | `			if( apSaved ){` |
|       16 | 12630 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        5 | 12631 | `					nSavedCount * sizeof(ph7_exception *));` |
|       11 | 12632 | `				SySetReset(&pVm->aException);` |
|        5 | 12633 | `			}` |
|        5 | 12634 | `		}` |
|        - | 12635 | `		/* Create a private frame first */` |
|      190 | 12636 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      190 | 12637 | `		if( rc == SXRET_OK ){` |
|      190 | 12638 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      190 | 12639 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      190 | 12640 | `			if( pObj ){` |
|      190 | 12641 | `				pThis->iRef++;` |
|      190 | 12642 | `				pObj->x.pOther = pThis;` |
|      190 | 12643 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       94 | 12644 | `			}` |
|        - | 12645 | `			/* Execute the catch block */` |
|      190 | 12646 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 12647 | `			/* Leave the frame */` |
|      190 | 12648 | `			VmLeaveFrame(&(*pVm));` |
|       94 | 12649 | `		}` |
|        - | 12650 | `		/* Restore the outer exception handlers */` |
|      190 | 12651 | `		if( apSaved ){` |
|        - | 12652 | `			sxu32 k;` |
|        - | 12653 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 12654 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 12655 | `			 * Restore the original outer entries.` |
|        - | 12656 | `			 */` |
|       11 | 12657 | `			SySetReset(&pVm->aException);` |
|       21 | 12658 | `			for(k = 0; k < nSavedCount; k++){` |
|       11 | 12659 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        6 | 12660 | `			}` |
|       11 | 12661 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        5 | 12662 | `		}` |
|        - | 12663 | `		/* Execute the finally block after catch */` |
|      190 | 12664 | `		if( pException->iHasFinally ){` |
|       16 | 12665 | `			pException->iFinallyDone = 1;` |
|        - | 12666 | `			{` |
|       16 | 12667 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 12668 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 12669 | `					return SXERR_ABORT;` |
|        - | 12670 | `				}` |
|        - | 12671 | `			}` |
|        7 | 12672 | `		}` |
|      190 | 12673 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12674 | `			return SXERR_ABORT;` |
|        - | 12675 | `		}` |
|        - | 12676 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 12677 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 12678 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 12679 | `		 */` |
|      190 | 12680 | `		if( pVm->pPendingException ){` |
|        9 | 12681 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 12682 | `			pVm->pPendingException = 0;` |
|        9 | 12683 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 12684 | `		}` |
|        - | 12685 | `	}` |
|        - | 12686 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 12687 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 12688 | `	 */` |
|      182 | 12689 | `	return SXRET_OK;` |
|      347 | 12690 |  |
|        - | 12691 | `/*` |
|        - | 12692 | ` * Section:` |
|        - | 12693 | ` *  Version,Credits and Copyright related functions.` |
|        - | 12694 | ` * Status:` |
|        - | 12695 | ` *    Stable.` |
|        - | 12696 | ` */` |
|        - | 12697 | `/*` |
|        - | 12698 | ` * string ph7version(void)` |
|        - | 12699 | ` *  Returns the running version of the PH7 version.` |
|        - | 12700 | ` * Parameters` |
|        - | 12701 | ` *  None` |
|        - | 12702 | ` * Return` |
|        - | 12703 | ` * Current PH7 version.` |
|        - | 12704 | ` */` |
|        2 | 12705 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12706 |  |
|        1 | 12707 | `	SXUNUSED(nArg);` |
|        1 | 12708 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 12709 | `	/* Current engine version */` |
|        3 | 12710 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 12711 | `	return PH7_OK;` |
|        1 | 12712 |  |
|        - | 12713 | `/*` |
|        - | 12714 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 12715 | ` */` |
|        - | 12716 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 12717 | ` "<html><head>"\` |
|        - | 12718 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 12719 | ` "<style type=\"text/css\">"\` |
|        - | 12720 | ` "div {"\` |
|        - | 12721 | `     "border: 1px solid #cccccc;"\` |
|        - | 12722 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 12723 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 12724 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 12725 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 12726 | `     "-webkit-border-radius: 10px;"\` |
|        - | 12727 | `     "-o-border-radius: 10px;"\` |
|        - | 12728 | `     "border-radius: 10px;"\` |
|        - | 12729 | `     "padding-left: 2em;"\` |
|        - | 12730 | `     "background-color: white;"\` |
|        - | 12731 | `     "margin-left: auto;"\` |
|        - | 12732 | `     "font-family: verdana;"\` |
|        - | 12733 | `     "padding-right: 2em;"\` |
|        - | 12734 | `     "margin-right: auto;"\` |
|        - | 12735 | `     "}"\` |
|        - | 12736 | `     "body {"\` |
|        - | 12737 | `     "padding: 0.2em;"\` |
|        - | 12738 | `     "font-style: normal;"\` |
|        - | 12739 | `     "font-size: medium;"\` |
|        - | 12740 | `     "background-color: #f2f2f2;"\` |
|        - | 12741 | `     "}"\` |
|        - | 12742 | `     "hr {"\` |
|        - | 12743 | `     "border-style: solid none none;"\` |
|        - | 12744 | `     "border-width: 1px medium medium;"\` |
|        - | 12745 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 12746 | `     "height: 1px;"\` |
|        - | 12747 | `     "}"\` |
|        - | 12748 | `     "a {"\` |
|        - | 12749 | `     "color: #3366cc;"\` |
|        - | 12750 | `     "text-decoration: none;"\` |
|        - | 12751 | `     "}"\` |
|        - | 12752 | `     "a:hover {"\` |
|        - | 12753 | `     "color: #999999;"\` |
|        - | 12754 | `     "}"\` |
|        - | 12755 | `     "a:active {"\` |
|        - | 12756 | `     "color: #663399;"\` |
|        - | 12757 | `     "}"\` |
|        - | 12758 | `     "h1 {"\` |
|        - | 12759 | `     "margin: 0;"\` |
|        - | 12760 | `     "padding: 0;"\` |
|        - | 12761 | `     "font-family: Verdana;"\` |
|        - | 12762 | `     "font-weight: bold;"\` |
|        - | 12763 | `     "font-style: normal;"\` |
|        - | 12764 | `     "font-size: medium;"\` |
|        - | 12765 | `     "text-transform: capitalize;"\` |
|        - | 12766 | `     "color: #0a328c;"\` |
|        - | 12767 | `     "}"\` |
|        - | 12768 | `     "p {"\` |
|        - | 12769 | `     "margin: 0 auto;"\` |
|        - | 12770 | `     "font-size: medium;"\` |
|        - | 12771 | `     "font-style: normal;"\` |
|        - | 12772 | `     "font-family: verdana;"\` |
|        - | 12773 | `     "}"\` |
|        - | 12774 | `"</style></head><body>"\` |
|        - | 12775 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 12776 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 12777 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 12778 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 12779 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 12780 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 12781 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 12782 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 12783 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 12784 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 12785 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 12786 |  |
|        - | 12787 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 12788 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 12789 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 12790 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 12791 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 12792 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 12793 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 12794 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 12795 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 12796 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 12797 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 12798 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 12799 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 12800 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 12801 |  |
|        - | 12802 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 12803 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 12804 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 12805 | `"&nbsp;*<br>"\` |
|        - | 12806 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 12807 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 12808 | `"&nbsp;* are met:<br>"\` |
|        - | 12809 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 12810 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 12811 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 12812 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 12813 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 12814 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 12815 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 12816 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 12817 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 12818 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 12819 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 12820 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 12821 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 12822 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 12823 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 12824 | `"&nbsp;*<br>"\` |
|        - | 12825 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 12826 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 12827 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 12828 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 12829 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 12830 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 12831 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 12832 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 12833 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 12834 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 12835 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 12836 | `"&nbsp;*/<br>"\` |
|        - | 12837 | `"</span></small></small></p>"\` |
|        - | 12838 | `"</div></body></html>"` |
|        - | 12839 | `/*` |
|        - | 12840 | ` * bool ph7credits(void)` |
|        - | 12841 | ` * bool ph7info(void)` |
|        - | 12842 | ` * bool ph7copyright(void)` |
|        - | 12843 | ` *  Prints out the credits for PH7 engine` |
|        - | 12844 | ` * Parameters` |
|        - | 12845 | ` *  None` |
|        - | 12846 | ` * Return` |
|        - | 12847 | ` *  Always TRUE` |
|        - | 12848 | ` */` |
|        2 | 12849 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12850 |  |
|        3 | 12851 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 12852 | `	/* Expand the HTML page above*/` |
|        3 | 12853 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 12854 | `	ph7_context_output_format(` |
|        1 | 12855 | `		pCtx,` |
|        - | 12856 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 12857 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 12858 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 12859 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 12860 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 12861 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 12862 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 12863 | `#ifdef __WINNT__` |
|        - | 12864 | `		"Windows NT"` |
|        - | 12865 | `#elif defined(__UNIXES__)` |
|        - | 12866 | `		"UNIX-Like"` |
|        - | 12867 | `#else` |
|        - | 12868 | `		"Other OS"` |
|        - | 12869 | `#endif` |
|        - | 12870 | `		);` |
|        3 | 12871 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 12872 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 12873 | `	SXUNUSED(apArg);` |
|        - | 12874 | `	/* Return TRUE */` |
|        - | 12875 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 12876 | `	return PH7_OK;` |
|        1 | 12877 |  |
|        - | 12878 | `/*` |
|        - | 12879 | ` * Section:` |
|        - | 12880 | ` *    URL related routines.` |
|        - | 12881 | ` * Status:` |
|        - | 12882 | ` *    Stable.` |
|        - | 12883 | ` */` |
|        - | 12884 | `/*` |
|        - | 12885 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 12886 | ` *  Parse a URL and return its fields.` |
|        - | 12887 | ` * Parameters` |
|        - | 12888 | ` *  $url` |
|        - | 12889 | ` *   The URL to parse.` |
|        - | 12890 | ` * $component` |
|        - | 12891 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 12892 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 12893 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 12894 | ` *  in which case the return value will be an integer).` |
|        - | 12895 | ` * Return` |
|        - | 12896 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 12897 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 12898 | ` *  this array are:` |
|        - | 12899 | ` *   scheme - e.g. http` |
|        - | 12900 | ` *   host` |
|        - | 12901 | ` *   port` |
|        - | 12902 | ` *   user` |
|        - | 12903 | ` *   pass` |
|        - | 12904 | ` *   path` |
|        - | 12905 | ` *   query - after the question mark ?` |
|        - | 12906 | ` *   fragment - after the hashmark #` |
|        - | 12907 | ` * Note:` |
|        - | 12908 | ` *  FALSE is returned on failure.` |
|        - | 12909 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 12910 | ` *  with the standard PHP engine.` |
|        - | 12911 | ` */` |
|       28 | 12912 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12913 |  |
|        - | 12914 | `	const char *zStr; /* Input string */` |
|        - | 12915 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 12916 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 12917 | `	int nLen;` |
|        - | 12918 | `	sxi32 rc;` |
|       29 | 12919 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12920 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 12921 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12922 | `		return PH7_OK;` |
|        - | 12923 | `	}` |
|        - | 12924 | `	/* Extract the given URI */` |
|       29 | 12925 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 12926 | `	if( nLen < 1 ){` |
|        - | 12927 | `		/* Nothing to process,return FALSE */` |
|        3 | 12928 | `		ph7_result_bool(pCtx,0);` |
|        3 | 12929 | `		return PH7_OK;` |
|        - | 12930 | `	}` |
|        - | 12931 | `	/* Get a parse */` |
|       27 | 12932 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 12933 | `	if( rc != SXRET_OK ){` |
|        - | 12934 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 12935 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12936 | `		return PH7_OK;` |
|        - | 12937 | `	}` |
|       27 | 12938 | `	if( nArg > 1 ){` |
|      ! 0 | 12939 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 12940 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 12941 | `		switch(nComponent){` |
|      ! 0 | 12942 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 12943 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 12944 | `			if( pComp->nByte < 1 ){` |
|        - | 12945 | `				/* No available value,return NULL */` |
|      ! 0 | 12946 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12947 | `			}else{` |
|      ! 0 | 12948 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12949 | `			}` |
|      ! 0 | 12950 | `			break;` |
|      ! 0 | 12951 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 12952 | `			pComp = &sURI.sHost;` |
|      ! 0 | 12953 | `			if( pComp->nByte < 1 ){` |
|        - | 12954 | `				/* No available value,return NULL */` |
|      ! 0 | 12955 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12956 | `			}else{` |
|      ! 0 | 12957 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12958 | `			}` |
|      ! 0 | 12959 | `			break;` |
|      ! 0 | 12960 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 12961 | `			pComp = &sURI.sPort;` |
|      ! 0 | 12962 | `			if( pComp->nByte < 1 ){` |
|        - | 12963 | `				/* No available value,return NULL */` |
|      ! 0 | 12964 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12965 | `			}else{` |
|      ! 0 | 12966 | `				int iPort = 0;` |
|        - | 12967 | `				/* Cast the value to integer */` |
|      ! 0 | 12968 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 12969 | `				ph7_result_int(pCtx,iPort);` |
|        - | 12970 | `			}` |
|      ! 0 | 12971 | `			break;` |
|      ! 0 | 12972 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 12973 | `			pComp = &sURI.sUser;` |
|      ! 0 | 12974 | `			if( pComp->nByte < 1 ){` |
|        - | 12975 | `				/* No available value,return NULL */` |
|      ! 0 | 12976 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12977 | `			}else{` |
|      ! 0 | 12978 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12979 | `			}` |
|      ! 0 | 12980 | `			break;` |
|      ! 0 | 12981 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 12982 | `			pComp = &sURI.sPass;` |
|      ! 0 | 12983 | `			if( pComp->nByte < 1 ){` |
|        - | 12984 | `				/* No available value,return NULL */` |
|      ! 0 | 12985 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12986 | `			}else{` |
|      ! 0 | 12987 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12988 | `			}` |
|      ! 0 | 12989 | `			break;` |
|      ! 0 | 12990 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 12991 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 12992 | `			if( pComp->nByte < 1 ){` |
|        - | 12993 | `				/* No available value,return NULL */` |
|      ! 0 | 12994 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12995 | `			}else{` |
|      ! 0 | 12996 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12997 | `			}` |
|      ! 0 | 12998 | `			break;` |
|      ! 0 | 12999 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 13000 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 13001 | `			if( pComp->nByte < 1 ){` |
|        - | 13002 | `				/* No available value,return NULL */` |
|      ! 0 | 13003 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13004 | `			}else{` |
|      ! 0 | 13005 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13006 | `			}` |
|      ! 0 | 13007 | `			break;` |
|      ! 0 | 13008 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 13009 | `			pComp = &sURI.sPath;` |
|      ! 0 | 13010 | `			if( pComp->nByte < 1 ){` |
|        - | 13011 | `				/* No available value,return NULL */` |
|      ! 0 | 13012 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13013 | `			}else{` |
|      ! 0 | 13014 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13015 | `			}` |
|      ! 0 | 13016 | `			break;` |
|      ! 0 | 13017 | `		default:` |
|        - | 13018 | `			/* No such entry,return NULL */` |
|      ! 0 | 13019 | `			ph7_result_null(pCtx);` |
|      ! 0 | 13020 | `			break;` |
|        - | 13021 | `		}` |
|      ! 0 | 13022 | `	}else{` |
|        - | 13023 | `		ph7_value *pArray,*pValue;` |
|        - | 13024 | `		/* Return an associative array */` |
|       27 | 13025 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 13026 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 13027 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 13028 | `			/* Out of memory */` |
|      ! 0 | 13029 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 13030 | `			/* Return false */` |
|      ! 0 | 13031 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 13032 | `			return PH7_OK;` |
|        - | 13033 | `		}` |
|        - | 13034 | `		/* Fill the array */` |
|       27 | 13035 | `		pComp = &sURI.sScheme;` |
|       27 | 13036 | `		if( pComp->nByte > 0 ){` |
|       19 | 13037 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 13038 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 13039 | `		}` |
|        - | 13040 | `		/* Reset the string cursor */` |
|       27 | 13041 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13042 | `		pComp = &sURI.sHost;` |
|       27 | 13043 | `		if( pComp->nByte > 0 ){` |
|       25 | 13044 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 13045 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 13046 | `		}` |
|        - | 13047 | `		/* Reset the string cursor */` |
|       27 | 13048 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13049 | `		pComp = &sURI.sPort;` |
|       27 | 13050 | `		if( pComp->nByte > 0 ){` |
|       11 | 13051 | `			int iPort = 0;/* cc warning */` |
|        - | 13052 | `			/* Convert to integer */` |
|       11 | 13053 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 13054 | `			ph7_value_int(pValue,iPort);` |
|       11 | 13055 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 13056 | `		}` |
|        - | 13057 | `		/* Reset the string cursor */` |
|       27 | 13058 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13059 | `		pComp = &sURI.sUser;` |
|       27 | 13060 | `		if( pComp->nByte > 0 ){` |
|        7 | 13061 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 13062 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 13063 | `		}` |
|        - | 13064 | `		/* Reset the string cursor */` |
|       27 | 13065 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13066 | `		pComp = &sURI.sPass;` |
|       27 | 13067 | `		if( pComp->nByte > 0 ){` |
|        7 | 13068 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 13069 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 13070 | `		}` |
|        - | 13071 | `		/* Reset the string cursor */` |
|       27 | 13072 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13073 | `		pComp = &sURI.sPath;` |
|       27 | 13074 | `		if( pComp->nByte > 0 ){` |
|       17 | 13075 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 13076 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 13077 | `		}` |
|        - | 13078 | `		/* Reset the string cursor */` |
|       27 | 13079 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13080 | `		pComp = &sURI.sQuery;` |
|       27 | 13081 | `		if( pComp->nByte > 0 ){` |
|        5 | 13082 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 13083 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 13084 | `		}` |
|        - | 13085 | `		/* Reset the string cursor */` |
|       27 | 13086 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13087 | `		pComp = &sURI.sFragment;` |
|       27 | 13088 | `		if( pComp->nByte > 0 ){` |
|        5 | 13089 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 13090 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 13091 | `		}` |
|        - | 13092 | `		/* Return the created array */` |
|       27 | 13093 | `		ph7_result_value(pCtx,pArray);` |
|        - | 13094 | `		/* NOTE:` |
|        - | 13095 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 13096 | `		 * automatically as soon we return from this function.` |
|        - | 13097 | `		 */` |
|        - | 13098 | `	}` |
|        - | 13099 | `	/* All done */` |
|       27 | 13100 | `	return PH7_OK;` |
|       15 | 13101 |  |
|        - | 13102 | `/*` |
|        - | 13103 | ` * Section:` |
|        - | 13104 | ` *   Array related routines.` |
|        - | 13105 | ` * Status:` |
|        - | 13106 | ` *    Stable.` |
|        - | 13107 | ` * Note 2012-5-21 01:04:15:` |
|        - | 13108 | ` *  Array related functions that need access to the underlying` |
|        - | 13109 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 13110 | ` */` |
|        - | 13111 | `/*` |
|        - | 13112 | ` * The [compact()] function store it's state information in an instance` |
|        - | 13113 | ` * of the following structure.` |
|        - | 13114 | ` */` |
|        - | 13115 | `struct compact_data` |
|        - | 13116 |  |
|        - | 13117 | `	ph7_value *pArray;  /* Target array */` |
|        - | 13118 | `	int nRecCount;      /* Recursion count */` |
|        - | 13119 | `};` |
|        - | 13120 | `/*` |
|        - | 13121 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 13122 | ` */` |
|      ! 0 | 13123 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 13124 |  |
|      ! 0 | 13125 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 13126 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 13127 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 13128 | `	/* Act according to the hashmap value */` |
|      ! 0 | 13129 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 13130 | `		SyString sVar;` |
|      ! 0 | 13131 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 13132 | `		if( sVar.nByte > 0 ){` |
|        - | 13133 | `			/* Query the current frame */` |
|      ! 0 | 13134 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 13135 | `			/* ^` |
|        - | 13136 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 13137 | `			 */` |
|      ! 0 | 13138 | `			if( pKey ){` |
|        - | 13139 | `				/* Perform the insertion */` |
|      ! 0 | 13140 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 13141 | `			}` |
|      ! 0 | 13142 | `		}` |
|      ! 0 | 13143 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 13144 | `		int rc;` |
|        - | 13145 | `		/* Recursively traverse this array */` |
|      ! 0 | 13146 | `		pData->nRecCount++;` |
|      ! 0 | 13147 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 13148 | `		pData->nRecCount--;` |
|      ! 0 | 13149 | `		return rc;` |
|        - | 13150 | `	}` |
|      ! 0 | 13151 | `	return SXRET_OK;` |
|      ! 0 | 13152 |  |
|        - | 13153 | `/*` |
|        - | 13154 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 13155 | ` *  Create array containing variables and their values.` |
|        - | 13156 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 13157 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 13158 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 13159 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 13160 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 13161 | ` * Parameters` |
|        - | 13162 | ` *  $varname` |
|        - | 13163 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 13164 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 13165 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 13166 | ` *   it recursively.` |
|        - | 13167 | ` * Return` |
|        - | 13168 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 13169 | ` */` |
|        2 | 13170 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13171 |  |
|        - | 13172 | `	ph7_value *pArray,*pObj;` |
|        3 | 13173 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13174 | `	const char *zName;` |
|        - | 13175 | `	SyString sVar;` |
|        - | 13176 | `	int i,nLen;` |
|        3 | 13177 | `	if( nArg < 1 ){` |
|        - | 13178 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 13179 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13180 | `		return PH7_OK;` |
|        - | 13181 | `	}` |
|        - | 13182 | `	/* Create the array */` |
|        3 | 13183 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 13184 | `	if( pArray == 0 ){` |
|        - | 13185 | `		/* Out of memory */` |
|      ! 0 | 13186 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 13187 | `		/* Return NULL */` |
|      ! 0 | 13188 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13189 | `		return PH7_OK;` |
|        - | 13190 | `	}` |
|        - | 13191 | `	/* Perform the requested operation */` |
|        7 | 13192 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 13193 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 13194 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 13195 | `				struct compact_data sData;` |
|      ! 0 | 13196 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 13197 | `				/* Recursively walk the array */` |
|      ! 0 | 13198 | `				sData.nRecCount = 0;` |
|      ! 0 | 13199 | `				sData.pArray = pArray;` |
|      ! 0 | 13200 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 13201 | `			}` |
|      ! 0 | 13202 | `		}else{` |
|        - | 13203 | `			/* Extract variable name */` |
|        5 | 13204 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 13205 | `			if( nLen > 0 ){` |
|        5 | 13206 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 13207 | `				/* Check if the variable is available in the current frame */` |
|        5 | 13208 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 13209 | `				if( pObj ){` |
|        5 | 13210 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 13211 | `				}` |
|        2 | 13212 | `			}` |
|        - | 13213 | `		}` |
|        3 | 13214 | `	}` |
|        - | 13215 | `	/* Return the array */` |
|        3 | 13216 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 13217 | `	return PH7_OK;` |
|        2 | 13218 |  |
|        - | 13219 | `/*` |
|        - | 13220 | ` * The [extract()] function store it's state information in an instance` |
|        - | 13221 | ` * of the following structure.` |
|        - | 13222 | ` */` |
|        - | 13223 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 13224 | `struct extract_aux_data` |
|        - | 13225 |  |
|        - | 13226 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 13227 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 13228 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 13229 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 13230 | `	int iFlags;           /* Control flags */` |
|        - | 13231 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 13232 | `};` |
|        - | 13233 | `/* Forward declaration */` |
|        - | 13234 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 13235 | `/*` |
|        - | 13236 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 13237 | ` *   Import variables into the current symbol table from an array.` |
|        - | 13238 | ` * Parameters` |
|        - | 13239 | ` * $var_array` |
|        - | 13240 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 13241 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 13242 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 13243 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 13244 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 13245 | ` * $extract_type` |
|        - | 13246 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 13247 | ` *  It can be one of the following values:` |
|        - | 13248 | ` *   EXTR_OVERWRITE` |
|        - | 13249 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 13250 | ` *   EXTR_SKIP` |
|        - | 13251 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 13252 | ` *   EXTR_PREFIX_SAME` |
|        - | 13253 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 13254 | ` *   EXTR_PREFIX_ALL` |
|        - | 13255 | ` *       Prefix all variable names with prefix.` |
|        - | 13256 | ` *   EXTR_PREFIX_INVALID` |
|        - | 13257 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 13258 | ` *   EXTR_IF_EXISTS` |
|        - | 13259 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 13260 | ` *       otherwise do nothing.` |
|        - | 13261 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 13262 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 13263 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 13264 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 13265 | ` *      the current symbol table.` |
|        - | 13266 | ` * $prefix` |
|        - | 13267 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 13268 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 13269 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 13270 | ` *  underscore character.` |
|        - | 13271 | ` * Return` |
|        - | 13272 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 13273 | ` */` |
|        4 | 13274 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13275 |  |
|        - | 13276 | `	extract_aux_data sAux;` |
|        - | 13277 | `	ph7_hashmap *pMap;` |
|        5 | 13278 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 13279 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 13280 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 13281 | `		return PH7_OK;` |
|        - | 13282 | `	}` |
|        - | 13283 | `	/* Point to the target hashmap */` |
|        5 | 13284 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 13285 | `	if( pMap->nEntry < 1 ){` |
|        - | 13286 | `		/* Empty map,return  0 */` |
|      ! 0 | 13287 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 13288 | `		return PH7_OK;` |
|        - | 13289 | `	}` |
|        - | 13290 | `	/* Prepare the aux data */` |
|        5 | 13291 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 13292 | `	if( nArg > 1 ){` |
|        3 | 13293 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 13294 | `		if( nArg > 2 ){` |
|      ! 0 | 13295 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 13296 | `		}` |
|        1 | 13297 | `	}` |
|        5 | 13298 | `	sAux.pVm = pCtx->pVm;` |
|        - | 13299 | `	/* Invoke the worker callback */` |
|        5 | 13300 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 13301 | `	/* Number of variables successfully imported */` |
|        5 | 13302 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 13303 | `	return PH7_OK;` |
|        3 | 13304 |  |
|        - | 13305 | `/*` |
|        - | 13306 | ` * Worker callback for the [extract()] function defined` |
|        - | 13307 | ` * below.` |
|        - | 13308 | ` */` |
|        8 | 13309 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 13310 |  |
|        9 | 13311 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 13312 | `	int iFlags = pAux->iFlags;` |
|        9 | 13313 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 13314 | `	ph7_value *pObj;` |
|        - | 13315 | `	SyString sVar;` |
|        9 | 13316 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 13317 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 13318 | `	}` |
|        - | 13319 | `	/* Perform a string cast */` |
|        9 | 13320 | `	PH7_MemObjToString(pKey);` |
|        9 | 13321 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 13322 | `		/* Unavailable variable name */` |
|      ! 0 | 13323 | `		return SXRET_OK;` |
|        - | 13324 | `	}` |
|        9 | 13325 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 13326 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 13327 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 13328 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 13329 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 13330 | `			);` |
|      ! 0 | 13331 | `	}else{` |
|       13 | 13332 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 13333 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 13334 | `	}` |
|        9 | 13335 | `	sVar.zString = pAux->zWorker;` |
|        - | 13336 | `	/* Try to extract the variable */` |
|        9 | 13337 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 13338 | `	if( pObj ){` |
|        - | 13339 | `		/* Collision */` |
|        5 | 13340 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 13341 | `			return SXRET_OK;` |
|        - | 13342 | `		}` |
|        5 | 13343 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 13344 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 13345 | `				/* Already prefixed */` |
|      ! 0 | 13346 | `				return SXRET_OK;` |
|        - | 13347 | `			}` |
|      ! 0 | 13348 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 13349 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 13350 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 13351 | `				);` |
|      ! 0 | 13352 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 13353 | `		}` |
|        3 | 13354 | `	}else{` |
|        - | 13355 | `		/* Create the variable */` |
|        5 | 13356 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 13357 | `	}` |
|        9 | 13358 | `	if( pObj ){` |
|        - | 13359 | `		/* Overwrite the old value */` |
|        9 | 13360 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 13361 | `		/* Increment counter */` |
|        9 | 13362 | `		pAux->iCount++;` |
|        4 | 13363 | `	}` |
|        9 | 13364 | `	return SXRET_OK;` |
|        5 | 13365 |  |
|        - | 13366 | `/*` |
|        - | 13367 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 13368 | ` * defined below.` |
|        - | 13369 | ` */` |
|        2 | 13370 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 13371 |  |
|        3 | 13372 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 13373 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 13374 | `	ph7_value *pObj;` |
|        - | 13375 | `	SyString sVar;` |
|        - | 13376 | `	/* Perform a string cast */` |
|        3 | 13377 | `	PH7_MemObjToString(pKey);` |
|        3 | 13378 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 13379 | `		/* Unavailable variable name */` |
|      ! 0 | 13380 | `		return SXRET_OK;` |
|        - | 13381 | `	}` |
|        3 | 13382 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 13383 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 13384 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 13385 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 13386 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 13387 | `			);` |
|        2 | 13388 | `	}else{` |
|      ! 0 | 13389 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 13390 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 13391 | `	}` |
|        3 | 13392 | `	sVar.zString = pAux->zWorker;` |
|        - | 13393 | `	/* Extract the variable */` |
|        3 | 13394 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 13395 | `	if( pObj ){` |
|        3 | 13396 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 13397 | `	}` |
|        3 | 13398 | `	return SXRET_OK;` |
|        2 | 13399 |  |
|        - | 13400 | `/*` |
|        - | 13401 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 13402 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 13403 | ` * Parameters` |
|        - | 13404 | ` * $types` |
|        - | 13405 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 13406 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 13407 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 13408 | ` *  POST includes the POST uploaded file information.` |
|        - | 13409 | ` *  Note:` |
|        - | 13410 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 13411 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 13412 | ` * $prefix` |
|        - | 13413 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 13414 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 13415 | ` *  variable named $pref_userid.` |
|        - | 13416 | ` * Return` |
|        - | 13417 | ` *  TRUE on success or FALSE on failure.` |
|        - | 13418 | ` */` |
|        2 | 13419 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13420 |  |
|        - | 13421 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 13422 | `	extract_aux_data sAux;` |
|        - | 13423 | `	int nLen,nPrefixLen;` |
|        - | 13424 | `	ph7_value *pSuper;` |
|        - | 13425 | `	ph7_vm *pVm;` |
|        - | 13426 | `	/* By default import only $_GET variables  */` |
|        3 | 13427 | `	zImport = "G";` |
|        3 | 13428 | `	nLen = (int)sizeof(char);` |
|        3 | 13429 | `	zPrefix = 0;` |
|        3 | 13430 | `	nPrefixLen = 0;` |
|        3 | 13431 | `	if( nArg > 0 ){` |
|        3 | 13432 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 13433 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 13434 | `		}` |
|        3 | 13435 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 13436 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 13437 | `		}` |
|        1 | 13438 | `	}` |
|        - | 13439 | `	/* Point to the underlying VM */` |
|        3 | 13440 | `	pVm = pCtx->pVm;` |
|        - | 13441 | `	/* Initialize the aux data */` |
|        3 | 13442 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 13443 | `	sAux.zPrefix = zPrefix;` |
|        3 | 13444 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 13445 | `	sAux.pVm = pVm;` |
|        - | 13446 | `	/* Extract */` |
|        3 | 13447 | `	zEnd = &zImport[nLen];` |
|        5 | 13448 | `	while( zImport < zEnd ){` |
|        3 | 13449 | `		int c = zImport[0];` |
|        3 | 13450 | `		pSuper = 0;` |
|        3 | 13451 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 13452 | `			/* Import $_GET variables */` |
|        3 | 13453 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 13454 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 13455 | `			/* Import $_POST variables */` |
|      ! 0 | 13456 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 13457 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 13458 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 13459 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 13460 | `		}` |
|        3 | 13461 | `		if( pSuper ){` |
|        - | 13462 | `			/* Iterate throw array entries */` |
|        3 | 13463 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 13464 | `		}` |
|        - | 13465 | `		/* Advance the cursor */` |
|        3 | 13466 | `		zImport++;` |
|        1 | 13467 | `	}` |
|        - | 13468 | `	/* All done,return TRUE*/` |
|        3 | 13469 | `	ph7_result_bool(pCtx,0);` |
|        3 | 13470 | `	return PH7_OK;` |
|        1 | 13471 |  |
|        - | 13472 | `/*` |
|        - | 13473 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 13474 | ` * Refer to the eval() language construct implementation for more` |
|        - | 13475 | ` * information.` |
|        - | 13476 | ` */` |
|    11896 | 13477 | `static sxi32 VmEvalChunk(` |
|        - | 13478 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 13479 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 13480 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 13481 | `	int iFlags,         /* Compile flag */` |
|        - | 13482 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 13483 | `	)` |
|        2 | 13484 |  |
|        - | 13485 | `	SySet *pByteCode,aByteCode;` |
|        - | 13486 | `	SyBlob sSavedNs;` |
|    11898 | 13487 | `	ProcConsumer xErr = 0;` |
|    11898 | 13488 | `	void *pErrData = 0;` |
|        - | 13489 | `	/* Initialize bytecode container */` |
|    11898 | 13490 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    11898 | 13491 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 13492 | `	/* Reset the code generator */` |
|    11898 | 13493 | `	if( bTrueReturn ){` |
|        - | 13494 | `		/* Included file,log compile-time errors */` |
|     8942 | 13495 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     8942 | 13496 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4470 | 13497 | `	}` |
|    11898 | 13498 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 13499 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 13500 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 13501 | `	 * the caller's namespace is restored. */` |
|    11898 | 13502 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    11898 | 13503 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    11898 | 13504 | `	if( bTrueReturn ){` |
|        - | 13505 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     8942 | 13506 | `		SyBlobReset(&pVm->sNamespace);` |
|     4470 | 13507 | `	}` |
|        - | 13508 | `	/* Swap bytecode container */` |
|    11898 | 13509 | `	pByteCode = pVm->pByteContainer;` |
|    11898 | 13510 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 13511 | `	/* Compile the chunk */` |
|    11898 | 13512 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    17846 | 13513 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 13514 | `		/* Compilation error,return false */` |
|        3 | 13515 | `		if( pCtx ){` |
|        3 | 13516 | `			ph7_result_bool(pCtx,0);` |
|        1 | 13517 | `		}` |
|        2 | 13518 | `	}else{` |
|        - | 13519 | `		/* Mount any newly defined classes */` |
|        - | 13520 | `		SyHashEntry *pEntry;` |
|        - | 13521 | `		ph7_class *pClass;` |
|        - | 13522 | `		ph7_value sResult; /* Return value */` |
|        - | 13523 | `		sxi32 rc;` |
|    11896 | 13524 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   565583 | 13525 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   547742 | 13526 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 13527 | `			/* Only mount classes that haven't been mounted yet */` |
|   547742 | 13528 | `			if( !pClass->bMounted ){` |
|   108032 | 13529 | `				rc = VmMountUserClass(pVm,pClass);` |
|   108032 | 13530 | `				if( rc != SXRET_OK ){` |
|        - | 13531 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 13532 | `					if( pCtx ){` |
|      ! 0 | 13533 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 13534 | `					}` |
|      ! 0 | 13535 | `					goto Cleanup;` |
|        - | 13536 | `				}` |
|    54015 | 13537 | `			}` |
|        2 | 13538 | `		}` |
|    11896 | 13539 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 13540 | `			/* Out of memory */` |
|      ! 0 | 13541 | `			if( pCtx ){` |
|      ! 0 | 13542 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 13543 | `			}` |
|      ! 0 | 13544 | `			goto Cleanup;` |
|        - | 13545 | `		}` |
|    11896 | 13546 | `		if( bTrueReturn ){` |
|        - | 13547 | `			/* Assume a boolean true return value */` |
|     8942 | 13548 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4472 | 13549 | `		}else{` |
|        - | 13550 | `			/* Assume a null return value */` |
|     2956 | 13551 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 13552 | `		}` |
|        - | 13553 | `		/* Execute the compiled chunk */` |
|    11896 | 13554 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    11896 | 13555 | `		if( pCtx ){` |
|        - | 13556 | `			/* Set the execution result */` |
|     8960 | 13557 | `			ph7_result_value(pCtx,&sResult);` |
|     4479 | 13558 | `		}` |
|    11896 | 13559 | `		PH7_MemObjRelease(&sResult);` |
|        - | 13560 | `	}` |
|     5948 | 13561 | `Cleanup:` |
|        - | 13562 | `	/* Cleanup the mess left behind */` |
|    11898 | 13563 | `	pVm->pByteContainer = pByteCode;` |
|    11898 | 13564 | `	SySetRelease(&aByteCode);` |
|        - | 13565 | `	/* Restore caller's namespace state */` |
|    11898 | 13566 | `	SyBlobReset(&pVm->sNamespace);` |
|    11898 | 13567 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    11898 | 13568 | `	SyBlobRelease(&sSavedNs);` |
|    11898 | 13569 | `	return SXRET_OK;` |
|        2 | 13570 |  |
|        - | 13571 | `/*` |
|        - | 13572 | ` * value eval(string $code)` |
|        - | 13573 | ` *   Evaluate a string as PHP code.` |
|        - | 13574 | ` * Parameter` |
|        - | 13575 | ` *  code: PHP code to evaluate.` |
|        - | 13576 | ` * Return` |
|        - | 13577 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 13578 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 13579 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 13580 | ` */` |
|       22 | 13581 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13582 |  |
|        - | 13583 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 13584 | `	if( nArg < 1 ){` |
|        - | 13585 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13586 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13587 | `		return SXRET_OK;` |
|        - | 13588 | `	}` |
|        - | 13589 | `	/* Chunk to evaluate */` |
|       24 | 13590 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 13591 | `	if( sChunk.nByte < 1 ){` |
|        - | 13592 | `		/* Empty string,return NULL */` |
|        3 | 13593 | `		ph7_result_null(pCtx);` |
|        3 | 13594 | `		return SXRET_OK;` |
|        - | 13595 | `	}` |
|        - | 13596 | `	/* Eval the chunk */` |
|       22 | 13597 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 13598 | `	return SXRET_OK;` |
|       13 | 13599 |  |
|        - | 13600 | `/*` |
|        - | 13601 | ` * Check if a file path is already included.` |
|        - | 13602 | ` */` |
|    17876 | 13603 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 13604 |  |
|        - | 13605 | `	SyString *aEntries;` |
|        - | 13606 | `	sxu32 n;` |
|    17878 | 13607 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 13608 | `	/* Perform a linear search */` |
| 79835854 | 13609 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 79817984 | 13610 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 13611 | `			/* Already included */` |
|        7 | 13612 | `			return TRUE;` |
|        - | 13613 | `		}` |
| 39908990 | 13614 | `	}` |
|    17872 | 13615 | `	return FALSE;` |
|     8940 | 13616 |  |
|        - | 13617 | `/*` |
|        - | 13618 | ` * Push a file path in the appropriate VM container.` |
|        - | 13619 | ` */` |
|    20804 | 13620 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 13621 |  |
|        - | 13622 | `	SyString sPath;` |
|        - | 13623 | `	char *zDup;` |
|        - | 13624 | `#ifdef __WINNT__` |
|        - | 13625 | `	char *zCur;` |
|        - | 13626 | `#endif` |
|        - | 13627 | `	sxi32 rc;` |
|    20806 | 13628 | `	if( nLen < 0 ){` |
|     2930 | 13629 | `		nLen = SyStrlen(zPath);` |
|     1464 | 13630 | `	}` |
|        - | 13631 | `	/* Duplicate the file path first */` |
|    20806 | 13632 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    20806 | 13633 | `	if( zDup == 0 ){` |
|      ! 0 | 13634 | `		return SXERR_MEM;` |
|        - | 13635 | `	}` |
|        - | 13636 | `#ifdef __WINNT__` |
|        - | 13637 | `	/* Normalize path on windows` |
|        - | 13638 | `	 * Example:` |
|        - | 13639 | `	 *    Path/To/File.php` |
|        - | 13640 | `	 * becomes` |
|        - | 13641 | `	 *   path\to\file.php` |
|        - | 13642 | `	 */` |
|        2 | 13643 | `	zCur = zDup;` |
|        2 | 13644 | `	while( zCur[0] != 0 ){` |
|        2 | 13645 | `		if( zCur[0] == '/' ){` |
|        2 | 13646 | `			zCur[0] = '\\';` |
|        2 | 13647 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 13648 | `			int c = SyToLower(zCur[0]);` |
|        1 | 13649 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 13650 | `		}` |
|        2 | 13651 | `		zCur++;` |
|        2 | 13652 | `	}` |
|        - | 13653 | `#endif` |
|        - | 13654 | `	/* Install the file path */` |
|    20806 | 13655 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    20806 | 13656 | `	if( !bMain ){` |
|    17878 | 13657 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 13658 | `			/* Already included */` |
|        7 | 13659 | `			*pNew = 0;` |
|        4 | 13660 | `		}else{` |
|        - | 13661 | `			/* Insert in the corresponding container */` |
|    17872 | 13662 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    17872 | 13663 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 13664 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 13665 | `				return rc;` |
|        - | 13666 | `			}` |
|    17872 | 13667 | `			*pNew = 1;` |
|        - | 13668 | `		}` |
|     8938 | 13669 | `	}` |
|    20806 | 13670 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    20806 | 13671 | `	return SXRET_OK;` |
|    10404 | 13672 |  |
|        - | 13673 | `/*` |
|        - | 13674 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 13675 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 13676 | ` * indicates failure.` |
|        - | 13677 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 13678 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 13679 | ` * operations.` |
|        - | 13680 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 13681 | ` * this function is a no-op.` |
|        - | 13682 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 13683 | ` * constructs for more information.` |
|        - | 13684 | ` */` |
|     8950 | 13685 | `static sxi32 VmExecIncludedFile(` |
|        - | 13686 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 13687 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 13688 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 13689 | `	 )` |
|        2 | 13690 |  |
|        - | 13691 | `	sxi32 rc;` |
|        - | 13692 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13693 | `	const ph7_io_stream *pStream;` |
|        - | 13694 | `	SyBlob sContents;` |
|        - | 13695 | `	void *pHandle;` |
|        - | 13696 | `	ph7_vm *pVm;` |
|        - | 13697 | `	int isNew;` |
|        - | 13698 | `	/* Initialize fields */` |
|     8952 | 13699 | `	pVm = pCtx->pVm;` |
|     8952 | 13700 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     8952 | 13701 | `	isNew = 0;` |
|        - | 13702 | `	/* Extract the associated stream */` |
|     8952 | 13703 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 13704 | `	/*` |
|        - | 13705 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 13706 | `	 * in a read-only mode.` |
|        - | 13707 | `	 */` |
|     8952 | 13708 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     8952 | 13709 | `	if( pHandle == 0 ){` |
|        8 | 13710 | `		return SXERR_IO;` |
|        - | 13711 | `	}` |
|     8946 | 13712 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     8946 | 13713 | `	if( IncludeOnce && !isNew ){` |
|        - | 13714 | `		/* Already included */` |
|        5 | 13715 | `		rc = SXERR_EXISTS;` |
|        3 | 13716 | `	}else{` |
|        - | 13717 | `		/* Read the whole file contents */` |
|     8942 | 13718 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     8942 | 13719 | `		if( rc == SXRET_OK ){` |
|        - | 13720 | `			SyString sScript;` |
|        - | 13721 | `			/* Compile and execute the script */` |
|     8942 | 13722 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     8942 | 13723 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4470 | 13724 | `		}` |
|        - | 13725 | `	}` |
|        - | 13726 | `	/* Pop from the set of included file */` |
|     8946 | 13727 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 13728 | `	/* Close the handle */` |
|     8946 | 13729 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 13730 | `	/* Release the working buffer */` |
|     8946 | 13731 | `	SyBlobRelease(&sContents);` |
|        - | 13732 | `#else` |
|        - | 13733 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 13734 | `	SXUNUSED(pPath);` |
|        - | 13735 | `	SXUNUSED(IncludeOnce);` |
|        - | 13736 | `	rc = SXERR_IO;` |
|        - | 13737 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     8946 | 13738 | `	return rc;` |
|     4477 | 13739 |  |
|        - | 13740 | `/*` |
|        - | 13741 | ` * string get_include_path(void)` |
|        - | 13742 | ` *  Gets the current include_path configuration option.` |
|        - | 13743 | ` * Parameter` |
|        - | 13744 | ` *  None` |
|        - | 13745 | ` * Return` |
|        - | 13746 | ` *  Included paths as a string` |
|        - | 13747 | ` */` |
|        2 | 13748 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13749 |  |
|        3 | 13750 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13751 | `	SyString *aEntry;` |
|        - | 13752 | `	int dir_sep;` |
|        - | 13753 | `	sxu32 n;` |
|        - | 13754 | `#ifdef __WINNT__` |
|        1 | 13755 | `	dir_sep = ';';` |
|        - | 13756 | `#else` |
|        - | 13757 | `	/* Assume UNIX path separator */` |
|        2 | 13758 | `	dir_sep = ':';` |
|        - | 13759 | `#endif` |
|        1 | 13760 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13761 | `	SXUNUSED(apArg);` |
|        - | 13762 | `	/* Point to the list of import paths */` |
|        3 | 13763 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 13764 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 13765 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 13766 | `		if( n > 0 ){` |
|        - | 13767 | `			/* Append dir seprator */` |
|      ! 0 | 13768 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 13769 | `		}` |
|        - | 13770 | `		/* Append path */` |
|        3 | 13771 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 13772 | `	}` |
|        3 | 13773 | `	return PH7_OK;` |
|        1 | 13774 |  |
|        - | 13775 | `/*` |
|        - | 13776 | ` * string get_get_included_files(void)` |
|        - | 13777 | ` *  Gets the current include_path configuration option.` |
|        - | 13778 | ` * Parameter` |
|        - | 13779 | ` *  None` |
|        - | 13780 | ` * Return` |
|        - | 13781 | ` *  Included paths as a string` |
|        - | 13782 | ` */` |
|        2 | 13783 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13784 |  |
|        3 | 13785 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 13786 | `	ph7_value *pArray,*pWorker;` |
|        - | 13787 | `	SyString *pEntry;` |
|        - | 13788 | `	int c,d;` |
|        - | 13789 | `	/* Create an array and a working value */` |
|        3 | 13790 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 13791 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 13792 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 13793 | `		/* Out of memory,return null */` |
|      ! 0 | 13794 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13795 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13796 | `		SXUNUSED(apArg);` |
|      ! 0 | 13797 | `		return PH7_OK;` |
|        - | 13798 | `	}` |
|        3 | 13799 | `	c = d = '/';` |
|        - | 13800 | `#ifdef __WINNT__` |
|        1 | 13801 | `	d = '\\';` |
|        - | 13802 | `#endif` |
|        - | 13803 | `	/* Iterate throw entries */` |
|        3 | 13804 | `	SySetResetCursor(pFiles);` |
|     3839 | 13805 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 13806 | `		const char *zBase,*zEnd;` |
|        - | 13807 | `		int iLen;` |
|        - | 13808 | `		/* reset the string cursor */` |
|     3837 | 13809 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 13810 | `		/* Extract base name */` |
|     3837 | 13811 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 13812 | `		/* Ignore trailing '/' */` |
|     5755 | 13813 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 13814 | `			zEnd--;` |
|      ! 0 | 13815 | `		}` |
|     3837 | 13816 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   118297 | 13817 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   112543 | 13818 | `			zEnd--;` |
|        1 | 13819 | `		}` |
|     3837 | 13820 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3837 | 13821 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 13822 | `		/* Copy entry name */` |
|     3837 | 13823 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 13824 | `		/* Perform the insertion */` |
|     3837 | 13825 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 13826 | `	}` |
|        - | 13827 | `	/* All done,return the created array */` |
|        3 | 13828 | `	ph7_result_value(pCtx,pArray);` |
|        - | 13829 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 13830 | `	 * by the engine as soon we return from this foreign` |
|        - | 13831 | `	 * function.` |
|        - | 13832 | `	 */` |
|        3 | 13833 | `	return PH7_OK;` |
|        2 | 13834 |  |
|        - | 13835 | `/*` |
|        - | 13836 | ` * include:` |
|        - | 13837 | ` * According to the PHP reference manual.` |
|        - | 13838 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 13839 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 13840 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 13841 | ` *  include() will finally check in the calling script's own directory` |
|        - | 13842 | ` *  and the current working directory before failing. The include()` |
|        - | 13843 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 13844 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 13845 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 13846 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 13847 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 13848 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 13849 | ` *  directory to find the requested file.` |
|        - | 13850 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 13851 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 13852 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 13853 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 13854 | ` */` |
|     8932 | 13855 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13856 |  |
|        - | 13857 | `	SyString sFile;` |
|        - | 13858 | `	sxi32 rc;` |
|     8934 | 13859 | `	if( nArg < 1 ){` |
|        - | 13860 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13861 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13862 | `		return SXRET_OK;` |
|        - | 13863 | `	}` |
|        - | 13864 | `	/* File to include */` |
|     8934 | 13865 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     8934 | 13866 | `	if( sFile.nByte < 1 ){` |
|        - | 13867 | `		/* Empty string,return NULL */` |
|      ! 0 | 13868 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13869 | `		return SXRET_OK;` |
|        - | 13870 | `	}` |
|        - | 13871 | `	/* Open,compile and execute the desired script */` |
|     8934 | 13872 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     8934 | 13873 | `	if( rc != SXRET_OK ){` |
|        - | 13874 | `		/* Emit a warning and return false */` |
|        3 | 13875 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 13876 | `		ph7_result_bool(pCtx,0);` |
|        1 | 13877 | `	}` |
|     8934 | 13878 | `	return SXRET_OK;` |
|     4468 | 13879 |  |
|        - | 13880 | `/*` |
|        - | 13881 | ` * include_once:` |
|        - | 13882 | ` *  According to the PHP reference manual.` |
|        - | 13883 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 13884 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 13885 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 13886 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 13887 | ` *   just once.` |
|        - | 13888 | ` */` |
|        4 | 13889 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13890 |  |
|        - | 13891 | `	SyString sFile;` |
|        - | 13892 | `	sxi32 rc;` |
|        5 | 13893 | `	if( nArg < 1 ){` |
|        - | 13894 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13895 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13896 | `		return SXRET_OK;` |
|        - | 13897 | `	}` |
|        - | 13898 | `	/* File to include */` |
|        5 | 13899 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 13900 | `	if( sFile.nByte < 1 ){` |
|        - | 13901 | `		/* Empty string,return NULL */` |
|      ! 0 | 13902 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13903 | `		return SXRET_OK;` |
|        - | 13904 | `	}` |
|        - | 13905 | `	/* Open,compile and execute the desired script */` |
|        5 | 13906 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 13907 | `	if( rc == SXERR_EXISTS ){` |
|        - | 13908 | `		/* File already included,return TRUE */` |
|        3 | 13909 | `		ph7_result_bool(pCtx,1);` |
|        3 | 13910 | `		return SXRET_OK;` |
|        - | 13911 | `	}` |
|        3 | 13912 | `	if( rc != SXRET_OK ){` |
|        - | 13913 | `		/* Emit a warning and return false */` |
|      ! 0 | 13914 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 13915 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13916 | ` 	}` |
|        3 | 13917 | `	return SXRET_OK;` |
|        3 | 13918 |  |
|        - | 13919 | `/*` |
|        - | 13920 | ` * require.` |
|        - | 13921 | ` *  According to the PHP reference manual.` |
|        - | 13922 | ` *   require() is identical to include() except upon failure it will` |
|        - | 13923 | ` *   also produce a fatal level error.` |
|        - | 13924 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 13925 | ` *   emits a warning  which allows the script to continue.` |
|        - | 13926 | ` */` |
|        6 | 13927 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13928 |  |
|        - | 13929 | `	SyString sFile;` |
|        - | 13930 | `	sxi32 rc;` |
|        8 | 13931 | `	if( nArg < 1 ){` |
|        - | 13932 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13933 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13934 | `		return SXRET_OK;` |
|        - | 13935 | `	}` |
|        - | 13936 | `	/* File to include */` |
|        8 | 13937 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 13938 | `	if( sFile.nByte < 1 ){` |
|        - | 13939 | `		/* Empty string,return NULL */` |
|      ! 0 | 13940 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13941 | `		return SXRET_OK;` |
|        - | 13942 | `	}` |
|        - | 13943 | `	/* Open,compile and execute the desired script */` |
|        8 | 13944 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 13945 | `	if( rc != SXRET_OK ){` |
|        - | 13946 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 13947 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 13948 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13949 | `		return PH7_ABORT;` |
|        - | 13950 | `	}` |
|        8 | 13951 | `	return SXRET_OK;` |
|        5 | 13952 |  |
|        - | 13953 | `/*` |
|        - | 13954 | ` * require_once:` |
|        - | 13955 | ` *  According to the PHP reference manual.` |
|        - | 13956 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 13957 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 13958 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 13959 | ` *   and how it differs from its non _once siblings.` |
|        - | 13960 | ` */` |
|        4 | 13961 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13962 |  |
|        - | 13963 | `	SyString sFile;` |
|        - | 13964 | `	sxi32 rc;` |
|        5 | 13965 | `	if( nArg < 1 ){` |
|        - | 13966 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13967 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13968 | `		return SXRET_OK;` |
|        - | 13969 | `	}` |
|        - | 13970 | `	/* File to include */` |
|        5 | 13971 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 13972 | `	if( sFile.nByte < 1 ){` |
|        - | 13973 | `		/* Empty string,return NULL */` |
|      ! 0 | 13974 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13975 | `		return SXRET_OK;` |
|        - | 13976 | `	}` |
|        - | 13977 | `	/* Open,compile and execute the desired script */` |
|        5 | 13978 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 13979 | `	if( rc == SXERR_EXISTS ){` |
|        - | 13980 | `		/* File already included,return TRUE */` |
|        3 | 13981 | `		ph7_result_bool(pCtx,1);` |
|        3 | 13982 | `		return SXRET_OK;` |
|        - | 13983 | `	}` |
|        3 | 13984 | `	if( rc != SXRET_OK ){` |
|        - | 13985 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 13986 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 13987 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13988 | `		return PH7_ABORT;` |
|        - | 13989 | `	}` |
|        3 | 13990 | `	return SXRET_OK;` |
|        3 | 13991 |  |
|        - | 13992 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 13993 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 13994 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 13995 | `/*` |
|        - | 13996 | ` * Section:` |
|        - | 13997 | ` *  SPL Autoloading functions.` |
|        - | 13998 | ` * Status:` |
|        - | 13999 | ` *  Stable.` |
|        - | 14000 | ` */` |
|        - | 14001 | `/*` |
|        - | 14002 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 14003 | ` *  Register given function as __autoload() implementation.` |
|        - | 14004 | ` * Parameters` |
|        - | 14005 | ` *  callback` |
|        - | 14006 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 14007 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 14008 | ` *  throw` |
|        - | 14009 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 14010 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 14011 | ` *  prepend` |
|        - | 14012 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 14013 | ` *   autoload stack instead of appending it.` |
|        - | 14014 | ` * Return` |
|        - | 14015 | ` *  TRUE on success, FALSE on failure.` |
|        - | 14016 | ` */` |
|       34 | 14017 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14018 |  |
|        - | 14019 | `	VmAutoloadCB sEntry;` |
|       36 | 14020 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 14021 | `	int iPrepend = 0;` |
|        - | 14022 | `	sxu32 n;` |
|       36 | 14023 | `	if( nArg < 1 ){` |
|        - | 14024 | `		/* No callback provided — register default spl_autoload.` |
|        - | 14025 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 14026 | `		/* Check for duplicates first */` |
|        9 | 14027 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 14028 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 14029 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 14030 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 14031 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 14032 | `				ph7_result_bool(pCtx,1);` |
|        5 | 14033 | `				return SXRET_OK;` |
|        - | 14034 | `			}` |
|      ! 0 | 14035 | `		}` |
|        5 | 14036 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 14037 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 14038 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 14039 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 14040 | `		ph7_result_bool(pCtx,1);` |
|        5 | 14041 | `		return SXRET_OK;` |
|        - | 14042 | `	}` |
|        - | 14043 | `	/* Validate that the callback is callable */` |
|       28 | 14044 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 14045 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 14046 | `		if( nArg >= 2 ){` |
|      ! 0 | 14047 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 14048 | `		}` |
|      ! 0 | 14049 | `		if( iThrow ){` |
|      ! 0 | 14050 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 14051 | `				"Argument is not callable");` |
|      ! 0 | 14052 | `		}` |
|      ! 0 | 14053 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14054 | `		return SXRET_OK;` |
|        - | 14055 | `	}` |
|        - | 14056 | `	/* Check for duplicates */` |
|       46 | 14057 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 14058 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 14059 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 14060 | `			/* Already registered */` |
|      ! 0 | 14061 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 14062 | `			return SXRET_OK;` |
|        - | 14063 | `		}` |
|       11 | 14064 | `	}` |
|        - | 14065 | `	/* Check prepend flag */` |
|       28 | 14066 | `	if( nArg >= 3 ){` |
|        3 | 14067 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 14068 | `	}` |
|        - | 14069 | `	/* Store the callback */` |
|       28 | 14070 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 14071 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 14072 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 14073 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 14074 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 14075 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 14076 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 14077 | `		VmAutoloadCB *aBase;` |
|        3 | 14078 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 14079 | `		/* Rotate: move last entry to front */` |
|        3 | 14080 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 14081 | `		if( aBase ){` |
|        - | 14082 | `			VmAutoloadCB sTemp;` |
|        - | 14083 | `			sxu32 i;` |
|        3 | 14084 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 14085 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 14086 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 14087 | `			}` |
|        3 | 14088 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 14089 | `		}` |
|        2 | 14090 | `	}else{` |
|       26 | 14091 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 14092 | `	}` |
|       28 | 14093 | `	ph7_result_bool(pCtx,1);` |
|       28 | 14094 | `	return SXRET_OK;` |
|       19 | 14095 |  |
|        - | 14096 | `/*` |
|        - | 14097 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 14098 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 14099 | ` * Parameters` |
|        - | 14100 | ` *  callback` |
|        - | 14101 | ` *   The autoload function being unregistered.` |
|        - | 14102 | ` * Return` |
|        - | 14103 | ` *  TRUE on success, FALSE on failure.` |
|        - | 14104 | ` */` |
|       32 | 14105 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14106 |  |
|       34 | 14107 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14108 | `	sxu32 n,nEntry;` |
|       34 | 14109 | `	if( nArg < 1 ){` |
|      ! 0 | 14110 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14111 | `		return SXRET_OK;` |
|        - | 14112 | `	}` |
|       34 | 14113 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 14114 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 14115 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 14116 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 14117 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 14118 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 14119 | `			sxu32 i;` |
|       32 | 14120 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 14121 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 14122 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 14123 | `			}` |
|        - | 14124 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 14125 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 14126 | `			ph7_result_bool(pCtx,1);` |
|       32 | 14127 | `			return SXRET_OK;` |
|        - | 14128 | `		}` |
|        3 | 14129 | `	}` |
|        3 | 14130 | `	ph7_result_bool(pCtx,0);` |
|        3 | 14131 | `	return SXRET_OK;` |
|       18 | 14132 |  |
|        - | 14133 | `/*` |
|        - | 14134 | ` * array spl_autoload_functions(void)` |
|        - | 14135 | ` *  Return all registered __autoload() functions.` |
|        - | 14136 | ` * Return` |
|        - | 14137 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 14138 | ` *  an empty array is returned.` |
|        - | 14139 | ` */` |
|       20 | 14140 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14141 |  |
|       21 | 14142 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14143 | `	ph7_value *pArray;` |
|        - | 14144 | `	sxu32 n,nEntry;` |
|       10 | 14145 | `	SXUNUSED(nArg);` |
|       10 | 14146 | `	SXUNUSED(apArg);` |
|       21 | 14147 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 14148 | `	if( pArray == 0 ){` |
|      ! 0 | 14149 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14150 | `		return SXRET_OK;` |
|        - | 14151 | `	}` |
|       21 | 14152 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 14153 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 14154 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 14155 | `		if( pEntry ){` |
|       15 | 14156 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 14157 | `		}` |
|        8 | 14158 | `	}` |
|       21 | 14159 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 14160 | `	return SXRET_OK;` |
|       11 | 14161 |  |
|        - | 14162 | `/*` |
|        - | 14163 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 14164 | ` *  Default implementation of __autoload().` |
|        - | 14165 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 14166 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 14167 | ` * Parameters` |
|        - | 14168 | ` *  class` |
|        - | 14169 | ` *   The class name being searched.` |
|        - | 14170 | ` *  file_extensions` |
|        - | 14171 | ` *   Comma-separated list of file extensions to try.` |
|        - | 14172 | ` */` |
|        2 | 14173 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14174 |  |
|        - | 14175 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 14176 | `	SyBlob sPath;` |
|        - | 14177 | `	int nClass;` |
|        - | 14178 | `	sxi32 rc;` |
|        3 | 14179 | `	if( nArg < 1 ){` |
|      ! 0 | 14180 | `		return SXRET_OK;` |
|        - | 14181 | `	}` |
|        3 | 14182 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 14183 | `	if( nClass < 1 ){` |
|      ! 0 | 14184 | `		return SXRET_OK;` |
|        - | 14185 | `	}` |
|        - | 14186 | `	/* Default extensions */` |
|        3 | 14187 | `	zExt = ".php,.inc";` |
|        3 | 14188 | `	if( nArg >= 2 ){` |
|        - | 14189 | `		int nExt;` |
|      ! 0 | 14190 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 14191 | `		if( nExt < 1 ){` |
|      ! 0 | 14192 | `			zExt = ".php,.inc";` |
|      ! 0 | 14193 | `		}` |
|      ! 0 | 14194 | `	}` |
|        3 | 14195 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 14196 | `	/* Iterate over comma-separated extensions */` |
|        3 | 14197 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 14198 | `	zCur = zExt;` |
|        7 | 14199 | `	while( zCur < zEnd ){` |
|        - | 14200 | `		const char *zComma;` |
|        - | 14201 | `		SyString sFile;` |
|        - | 14202 | `		int i;` |
|        - | 14203 | `		/* Find next comma or end */` |
|        5 | 14204 | `		zComma = zCur;` |
|       21 | 14205 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 14206 | `			zComma++;` |
|        1 | 14207 | `		}` |
|        - | 14208 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 14209 | `		SyBlobReset(&sPath);` |
|       69 | 14210 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 14211 | `			char c = zClass[i];` |
|       65 | 14212 | `			if( c == '\\' ){` |
|      ! 0 | 14213 | `				c = '/';` |
|       65 | 14214 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 14215 | `				c = c + ('a' - 'A');` |
|        6 | 14216 | `			}` |
|       65 | 14217 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 14218 | `		}` |
|        - | 14219 | `		/* Append extension */` |
|        5 | 14220 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 14221 | `		/* Try to include the file */` |
|        5 | 14222 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 14223 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 14224 | `		if( rc == SXRET_OK ){` |
|        - | 14225 | `			/* File included successfully */` |
|      ! 0 | 14226 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 14227 | `			return SXRET_OK;` |
|        - | 14228 | `		}` |
|        - | 14229 | `		/* Move past the comma */` |
|        5 | 14230 | `		zCur = zComma;` |
|        5 | 14231 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 14232 | `			zCur++;` |
|        1 | 14233 | `		}` |
|        1 | 14234 | `	}` |
|        3 | 14235 | `	SyBlobRelease(&sPath);` |
|        3 | 14236 | `	return SXRET_OK;` |
|        2 | 14237 |  |
|        - | 14238 | `/* Table of built-in VM functions. */` |
|        - | 14239 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 14240 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 14241 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 14242 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 14243 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 14244 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 14245 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 14246 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 14247 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 14248 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 14249 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 14250 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 14251 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 14252 | `	    /* Constants management */` |
|        - | 14253 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 14254 | `	{ "define",   vm_builtin_define               },` |
|        - | 14255 | `	{ "constant", vm_builtin_constant             },` |
|        - | 14256 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 14257 | `	   /* Class/Object functions */` |
|        - | 14258 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 14259 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 14260 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 14261 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 14262 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 14263 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 14264 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 14265 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 14266 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 14267 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 14268 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 14269 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 14270 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 14271 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 14272 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 14273 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 14274 | `	   /* SPL Autoloading */` |
|        - | 14275 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 14276 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 14277 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 14278 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 14279 | `	   /* Random numbers/strings generators */` |
|        - | 14280 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 14281 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 14282 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 14283 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 14284 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 14285 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14286 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 14287 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 14288 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 14289 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 14290 | `	   /* Language constructs functions */` |
|        - | 14291 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 14292 | `	{ "print", vm_builtin_print                   },` |
|        - | 14293 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 14294 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 14295 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 14296 | `	  /* Variable handling functions */` |
|        - | 14297 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 14298 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 14299 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 14300 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 14301 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 14302 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 14303 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 14304 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 14305 | `	  /* Ouput control functions */` |
|        - | 14306 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 14307 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 14308 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 14309 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 14310 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 14311 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 14312 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 14313 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 14314 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 14315 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 14316 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 14317 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 14318 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 14319 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 14320 | `	  /* Assertion functions */` |
|        - | 14321 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 14322 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 14323 | `	  /* Error reporting functions */` |
|        - | 14324 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 14325 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 14326 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 14327 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 14328 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 14329 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 14330 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 14331 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 14332 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 14333 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 14334 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 14335 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 14336 | `	  /* Release info */` |
|        - | 14337 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 14338 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 14339 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 14340 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 14341 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 14342 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 14343 | `	  /* hashmap */` |
|        - | 14344 | `	{"compact",          vm_builtin_compact       },` |
|        - | 14345 | `	{"extract",          vm_builtin_extract       },` |
|        - | 14346 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 14347 | `	  /* URL related function */` |
|        - | 14348 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 14349 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 14350 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14351 | `	   /* XML processing functions */` |
|        - | 14352 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 14353 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 14354 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 14355 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 14356 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 14357 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 14358 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 14359 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 14360 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 14361 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 14362 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 14363 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 14364 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 14365 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 14366 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 14367 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 14368 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 14369 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 14370 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 14371 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 14372 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 14373 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 14374 | `	   /* UTF-8 encoding/decoding */` |
|        - | 14375 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 14376 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 14377 | `	   /* Command line processing */` |
|        - | 14378 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 14379 | `	   /* JSON encoding/decoding */` |
|        - | 14380 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 14381 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 14382 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 14383 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 14384 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 14385 | `	   /* Files/URI inclusion facility */` |
|        - | 14386 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 14387 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 14388 | `	{ "include",      vm_builtin_include          },` |
|        - | 14389 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 14390 | `	{ "require",      vm_builtin_require          },` |
|        - | 14391 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 14392 | `};` |
|        - | 14393 | `/*` |
|        - | 14394 | ` * Register the built-in VM functions defined above.` |
|        - | 14395 | ` */` |
|     2622 | 14396 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 14397 |  |
|        - | 14398 | `	sxi32 rc;` |
|        - | 14399 | `	sxu32 n;` |
|   338240 | 14400 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 14401 | `		/* Note that these special functions have access` |
|        - | 14402 | `		 * to the underlying virtual machine as their` |
|        - | 14403 | `		 * private data.` |
|        - | 14404 | `		 */` |
|   335618 | 14405 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   335618 | 14406 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 14407 | `			return rc;` |
|        - | 14408 | `		}` |
|   167810 | 14409 | `	}` |
|     2624 | 14410 | `	return SXRET_OK;` |
|     1313 | 14411 |  |
|        - | 14412 | `/*` |
|        - | 14413 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 14414 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 14415 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 14416 | ` */` |
|    40750 | 14417 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 14418 |  |
|    40752 | 14419 | `	if( !iLoadable ){` |
|    39056 | 14420 | `		return pClass;` |
|        - | 14421 | `	}` |
|     1702 | 14422 | `	while(pClass){` |
|     1698 | 14423 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1694 | 14424 | `			return pClass;` |
|        - | 14425 | `		}` |
|        5 | 14426 | `		pClass = pClass->pNextName;` |
|        1 | 14427 | `	}` |
|        5 | 14428 | `	return 0;` |
|    20377 | 14429 |  |
|        - | 14430 | `/*` |
|        - | 14431 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 14432 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 14433 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 14434 | ` * registered in the VM's class table.` |
|        - | 14435 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 14436 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 14437 | ` */` |
|       38 | 14438 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 14439 |  |
|        - | 14440 | `	VmAutoloadCB *pEntry;` |
|        - | 14441 | `	ph7_value sArg,sResult;` |
|        - | 14442 | `	SyHashEntry *pHashEntry;` |
|        - | 14443 | `	ph7_class *pClass;` |
|        - | 14444 | `	sxu32 n,nEntry;` |
|       40 | 14445 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       40 | 14446 | `	if( nEntry < 1 ){` |
|       26 | 14447 | `		return 0;` |
|        - | 14448 | `	}` |
|        - | 14449 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 14450 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 14451 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 14452 | `	}` |
|        - | 14453 | `	/* Mark this class as being autoloaded */` |
|       14 | 14454 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 14455 | `	/* Prepare the class name argument */` |
|       14 | 14456 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 14457 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 14458 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 14459 | `	pClass = 0;` |
|       28 | 14460 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 14461 | `		ph7_value *apArg[1];` |
|       24 | 14462 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 14463 | `		if( pEntry == 0 ){` |
|      ! 0 | 14464 | `			continue;` |
|        - | 14465 | `		}` |
|       24 | 14466 | `		apArg[0] = &sArg;` |
|       24 | 14467 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 14468 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 14469 | `			continue;` |
|        - | 14470 | `		}` |
|        - | 14471 | `		/* Check if the class is now available */` |
|       24 | 14472 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 14473 | `		if( pHashEntry ){` |
|       10 | 14474 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 14475 | `			if( pClass ){` |
|       10 | 14476 | `				break;` |
|        - | 14477 | `			}` |
|      ! 0 | 14478 | `		}` |
|        9 | 14479 | `	}` |
|       14 | 14480 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 14481 | `	PH7_MemObjRelease(&sResult);` |
|        - | 14482 | `	/* Remove reentrancy guard */` |
|       14 | 14483 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 14484 | `	return pClass;` |
|       21 | 14485 |  |
|        - | 14486 | `/*` |
|        - | 14487 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 14488 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 14489 | ` */` |
|       18 | 14490 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 14491 |  |
|       20 | 14492 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 14493 |  |
|        - | 14494 | `/*` |
|        - | 14495 | ` * Check if the given name refer to an installed class.` |
|        - | 14496 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 14497 | ` */` |
|    40762 | 14498 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 14499 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 14500 | `	const char *zName,  /* Name of the target class */` |
|        - | 14501 | `	sxu32 nByte,        /* zName length */` |
|        - | 14502 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 14503 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 14504 | `						 */` |
|        - | 14505 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 14506 | `	)` |
|        2 | 14507 |  |
|        - | 14508 | `	SyHashEntry *pEntry;` |
|        - | 14509 | `	ph7_class *pClass;` |
|    20381 | 14510 | `	SXUNUSED(iNest);` |
|        - | 14511 | `	/* Exact class lookup.` |
|        - | 14512 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 14513 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    40764 | 14514 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    40764 | 14515 | `	if( pEntry == 0 ){` |
|        - | 14516 | `		/* Class not found in hash table — try autoload before giving up */` |
|       22 | 14517 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 14518 | `	}` |
|    40744 | 14519 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    40744 | 14520 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    20383 | 14521 |  |
|        - | 14522 | `/*` |
|        - | 14523 | ` * Reference Table Implementation` |
|        - | 14524 | ` * Status: stable <chm@symisc.net>` |
|        - | 14525 | ` * Intro` |
|        - | 14526 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 14527 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 14528 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 14529 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 14530 | ` *  Refer to the official for more information on this powerful` |
|        - | 14531 | ` *  extension.` |
|        - | 14532 | ` */` |
|        - | 14533 | `/*` |
|        - | 14534 | ` * Allocate a new reference entry.` |
|        - | 14535 | ` */` |
|  3124564 | 14536 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 14537 |  |
|        - | 14538 | `	VmRefObj *pRef;` |
|        - | 14539 | `	/* Allocate a new instance */` |
|  3124566 | 14540 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3124566 | 14541 | `	if( pRef == 0 ){` |
|      ! 0 | 14542 | `		return 0;` |
|        - | 14543 | `	}` |
|        - | 14544 | `	/* Zero the structure */` |
|  3124566 | 14545 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 14546 | `	/* Initialize fields */` |
|  3124566 | 14547 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3124566 | 14548 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3124566 | 14549 | `	pRef->nIdx = nIdx;` |
|  3124566 | 14550 | `	return pRef;` |
|  1562284 | 14551 |  |
|        - | 14552 | `/*` |
|        - | 14553 | ` * Default hash function used by the reference table` |
|        - | 14554 | ` * for lookup/insertion operations.` |
|        - | 14555 | ` */` |
| 17200436 | 14556 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 14557 |  |
|        - | 14558 | `	/* Calculate the hash based on the memory object index */` |
| 17200438 | 14559 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 14560 |  |
|        - | 14561 | `/*` |
|        - | 14562 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 14563 | ` * in the reference table.` |
|        - | 14564 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 14565 | ` * otherwise.` |
|        - | 14566 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14567 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14568 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14569 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14570 | ` * Refer to the official for more information on this powerful` |
|        - | 14571 | ` * extension.` |
|        - | 14572 | ` */` |
|  9319268 | 14573 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 14574 |  |
|        - | 14575 | `	VmRefObj *pRef;` |
|        - | 14576 | `	sxu32 nBucket;` |
|        - | 14577 | `	/* Point to the appropriate bucket */` |
|  9319270 | 14578 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 14579 | `	/* Perform the lookup */` |
|  9319270 | 14580 | `	pRef = pVm->apRefObj[nBucket];` |
| 20249617 | 14581 | `	for(;;){` |
| 40481985 | 14582 | `		if( pRef == 0 ){` |
|  3221978 | 14583 | `			break;` |
|        - | 14584 | `		}` |
| 37260009 | 14585 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 14586 | `			/* Entry found */` |
|  6097294 | 14587 | `			return pRef;` |
|        - | 14588 | `		}` |
|        - | 14589 | `		/* Point to the next entry */` |
| 31162717 | 14590 | `		pRef = pRef->pNextCollide;` |
|        2 | 14591 | `	}` |
|        - | 14592 | `	/* No such entry,return NULL */` |
|  3221978 | 14593 | `	return 0;` |
|  4659636 | 14594 |  |
|        - | 14595 | `/*` |
|        - | 14596 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14597 | ` *` |
|        - | 14598 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14599 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14600 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14601 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14602 | ` * Refer to the official for more information on this powerful` |
|        - | 14603 | ` * extension.` |
|        - | 14604 | ` */` |
|  3124564 | 14605 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14606 |  |
|        - | 14607 | `	sxu32 nBucket;` |
|  3124566 | 14608 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 14609 | `		VmRefObj **apNew;` |
|        - | 14610 | `		sxu32 nNew;` |
|        - | 14611 | `		/* Allocate a larger table */` |
|     4462 | 14612 | `		nNew = pVm->nRefSize << 1;` |
|     4462 | 14613 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4462 | 14614 | `		if( apNew ){` |
|     4462 | 14615 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 14616 | `			sxu32 n;` |
|        - | 14617 | `			/* Zero the structure */` |
|     4462 | 14618 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 14619 | `			/* Rehash all referenced entries */` |
|  2845672 | 14620 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 14621 | `				/* Remove old collision links */` |
|  2841212 | 14622 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 14623 | `				/* Point to the appropriate bucket */` |
|  2841212 | 14624 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 14625 | `				/* Insert the entry  */` |
|  2841212 | 14626 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2841212 | 14627 | `				if( apNew[nBucket] ){` |
|  2298896 | 14628 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 14629 | `				}` |
|  2841212 | 14630 | `				apNew[nBucket] = pEntry;` |
|        - | 14631 | `				/* Point to the next entry */` |
|  2841212 | 14632 | `				pEntry = pEntry->pNext;` |
|  1420607 | 14633 | `			}` |
|        - | 14634 | `			/* Release the old table */` |
|     4462 | 14635 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 14636 | `			/* Install the new one */` |
|     4462 | 14637 | `			pVm->apRefObj = apNew;` |
|     4462 | 14638 | `			pVm->nRefSize = nNew;` |
|     2230 | 14639 | `		}` |
|     2230 | 14640 | `	}` |
|        - | 14641 | `	/* Point to the appropriate bucket */` |
|  3124566 | 14642 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 14643 | `	/* Insert the entry */` |
|  3124566 | 14644 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3124566 | 14645 | `	if( pVm->apRefObj[nBucket] ){` |
|  2561006 | 14646 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1280529 | 14647 | `	}` |
|  3124566 | 14648 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3124566 | 14649 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3124566 | 14650 | `	pVm->nRefUsed++;` |
|  3124566 | 14651 | `	return SXRET_OK;` |
|        2 | 14652 |  |
|        - | 14653 | `/*` |
|        - | 14654 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 14655 | ` * the reference table.` |
|        - | 14656 | ` * This function is invoked when the user perform an unset` |
|        - | 14657 | ` * call [i.e: unset($var); ].` |
|        - | 14658 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14659 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14660 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14661 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14662 | ` * Refer to the official for more information on this powerful` |
|        - | 14663 | ` * extension.` |
|        - | 14664 | ` */` |
|  3086420 | 14665 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14666 |  |
|        - | 14667 | `	ph7_hashmap_node **apNode;` |
|        - | 14668 | `	SyHashEntry **apEntry;` |
|        - | 14669 | `	sxu32 n;` |
|        - | 14670 | `	/* Point to the reference table */` |
|  3086422 | 14671 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3086422 | 14672 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 14673 | `	/* Unlink the entry from the reference table */` |
|  3190326 | 14674 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   103906 | 14675 | `		if( apEntry[n] ){` |
|   103856 | 14676 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    51927 | 14677 | `		}` |
|    51954 | 14678 | `	}` |
|  6070744 | 14679 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2984324 | 14680 | `		if( apNode[n] ){` |
|     7420 | 14681 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3709 | 14682 | `		}` |
|  1492163 | 14683 | `	}` |
|  3086422 | 14684 | `	if( pRef->pPrevCollide ){` |
|  1171028 | 14685 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   585686 | 14686 | `	}else{` |
|  1915396 | 14687 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 14688 | `	}` |
|  3086422 | 14689 | `	if( pRef->pNextCollide ){` |
|  1747951 | 14690 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   873975 | 14691 | `	}` |
|  3086422 | 14692 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 14693 | `	/* Release the node */` |
|  3086422 | 14694 | `	SySetRelease(&pRef->aReference);` |
|  3086422 | 14695 | `	SySetRelease(&pRef->aArrEntries);` |
|  3086422 | 14696 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3086422 | 14697 | `	pVm->nRefUsed--;` |
|  3086422 | 14698 | `	return SXRET_OK;` |
|        2 | 14699 |  |
|        - | 14700 | `/*` |
|        - | 14701 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14702 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14703 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14704 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14705 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14706 | ` * Refer to the official for more information on this powerful` |
|        - | 14707 | ` * extension.` |
|        - | 14708 | ` */` |
|  3158466 | 14709 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 14710 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14711 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14712 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14713 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 14714 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 14715 | `	)` |
|        2 | 14716 |  |
|  3158468 | 14717 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 14718 | `	VmRefObj *pRef;` |
|        - | 14719 | `	/* Check if the referenced object already exists */` |
|  3158468 | 14720 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3158468 | 14721 | `	if( pRef == 0 ){` |
|        - | 14722 | `		/* Create a new entry */` |
|  3124566 | 14723 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3124566 | 14724 | `		if( pRef == 0 ){` |
|      ! 0 | 14725 | `			return SXERR_MEM;` |
|        - | 14726 | `		}` |
|  3124566 | 14727 | `		pRef->iFlags = iFlags;` |
|        - | 14728 | `		/* Install the entry */` |
|  3124566 | 14729 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1562282 | 14730 | `	}` |
|  3158468 | 14731 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3158468 | 14732 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 14733 | `		VmSlot sRef;` |
|        - | 14734 | `		/* Local frame,record referenced entry so that it can` |
|        - | 14735 | `		 * be deleted when we leave this frame.` |
|        - | 14736 | `		 */` |
|    97510 | 14737 | `		sRef.nIdx = nIdx;` |
|    97510 | 14738 | `		sRef.pUserData = pEntry;` |
|    97510 | 14739 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 14740 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 14741 | `		}` |
|    48754 | 14742 | `	}` |
|  3158468 | 14743 | `	if( pEntry ){` |
|        - | 14744 | `		/* Address of the hash-entry */` |
|   131212 | 14745 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    65605 | 14746 | `	}` |
|  3158468 | 14747 | `	if( pMapEntry ){` |
|        - | 14748 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3020236 | 14749 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1510117 | 14750 | `	}` |
|  3158468 | 14751 | `	return SXRET_OK;` |
|  1579235 | 14752 |  |
|        - | 14753 | `/*` |
|        - | 14754 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 14755 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14756 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14757 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14758 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14759 | ` * Refer to the official for more information on this powerful` |
|        - | 14760 | ` * extension.` |
|        - | 14761 | ` */` |
|  3074376 | 14762 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 14763 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14764 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14765 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14766 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 14767 | `	)` |
|        2 | 14768 |  |
|        - | 14769 | `	VmRefObj *pRef;` |
|        - | 14770 | `	sxu32 n;` |
|        - | 14771 | `	/* Check if the referenced object already exists */` |
|  3074378 | 14772 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3074378 | 14773 | `	if( pRef == 0 ){` |
|        - | 14774 | `		/* Not such entry */` |
|    97408 | 14775 | `		return SXERR_NOTFOUND;` |
|        - | 14776 | `	}` |
|        - | 14777 | `	/* Remove the desired entry */` |
|  2976972 | 14778 | `	if( pEntry ){` |
|        - | 14779 | `		SyHashEntry **apEntry;` |
|       62 | 14780 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      228 | 14781 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      168 | 14782 | `			if( apEntry[n] == pEntry ){` |
|        - | 14783 | `				/* Nullify the entry */` |
|       62 | 14784 | `				apEntry[n] = 0;` |
|        - | 14785 | `				/*` |
|        - | 14786 | `				 * NOTE:` |
|        - | 14787 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 14788 | `				 * we avoid wasting spaces.` |
|        - | 14789 | `				 */` |
|       30 | 14790 | `			}` |
|       85 | 14791 | `		}` |
|       30 | 14792 | `	}` |
|  2976972 | 14793 | `	if( pMapEntry ){` |
|        - | 14794 | `		ph7_hashmap_node **apNode;` |
|  2976912 | 14795 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5953916 | 14796 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2977006 | 14797 | `			if( apNode[n] == pMapEntry ){` |
|        - | 14798 | `				/* nullify the entry */` |
|  2976912 | 14799 | `				apNode[n] = 0;` |
|  1488455 | 14800 | `			}` |
|  1488504 | 14801 | `		}` |
|  1488455 | 14802 | `	}` |
|  2976972 | 14803 | `	return SXRET_OK;` |
|  1537190 | 14804 |  |
|        - | 14805 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 14806 | `/*` |
|        - | 14807 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 14808 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 14809 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 14810 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 14811 | ` * For more information on how to register IO stream devices,please` |
|        - | 14812 | ` * refer to the official documentation.` |
|        - | 14813 | ` */` |
|    27214 | 14814 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 14815 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 14816 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 14817 | `	int nByte              /* *pzDevice length*/` |
|        - | 14818 | `	)` |
|        2 | 14819 |  |
|        - | 14820 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 14821 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 14822 | `	SyString sDev,sCur;` |
|        - | 14823 | `	sxu32 n,nEntry;` |
|        - | 14824 | `	int rc;` |
|        - | 14825 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    27216 | 14826 | `	zNext = zCur = zIn = *pzDevice;` |
|    27216 | 14827 | `	zEnd = &zIn[nByte];` |
|  1727525 | 14828 | `	while( zIn < zEnd ){` |
|  1700313 | 14829 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 14830 | `			/* Got one */` |
|        3 | 14831 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 14832 | `			break;` |
|        - | 14833 | `		}` |
|        - | 14834 | `		/* Advance the cursor */` |
|  1700311 | 14835 | `		zIn++;` |
|        2 | 14836 | `	}` |
|    27216 | 14837 | `	if( zIn >= zEnd ){` |
|        - | 14838 | `		/* No such scheme,return the default stream */` |
|    27214 | 14839 | `		return pVm->pDefStream;` |
|        - | 14840 | `	}` |
|        3 | 14841 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 14842 | `	/* Remove leading and trailing white spaces */` |
|        3 | 14843 | `	SyStringFullTrim(&sDev);` |
|        - | 14844 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 14845 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 14846 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 14847 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 14848 | `		pStream = apStream[n];` |
|        3 | 14849 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 14850 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 14851 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 14852 | `		if( rc == 0 ){` |
|        - | 14853 | `			/* Stream device found */` |
|        3 | 14854 | `			*pzDevice = zNext;` |
|        3 | 14855 | `			return pStream;` |
|        - | 14856 | `		}` |
|      ! 0 | 14857 | `	}` |
|        - | 14858 | `	/* No such stream,return NULL */` |
|      ! 0 | 14859 | `	return 0;` |
|    13609 | 14860 |  |
|        - | 14861 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 14862 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 14863 |  |
