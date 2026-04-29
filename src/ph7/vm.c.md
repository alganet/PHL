# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6169/7994 lines (77.17%)

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
|   896812 |   142 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   143 |  |
|   896814 |   144 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |   145 | `		return TRUE;` |
|        - |   146 | `	}` |
|   896780 |   147 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   148 | `		return TRUE;` |
|        - |   149 | `	}` |
|   896770 |   150 | `	return FALSE;` |
|   448430 |   151 |  |
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
|   581152 |   166 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   581154 |   177 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   581154 |   178 | `	if( pEntry ){` |
|        - |   179 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   180 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   181 | `		pCons->xExpand = xExpand;` |
|        6 |   182 | `		pCons->pUserData = pUserData;` |
|        6 |   183 | `		return SXRET_OK;` |
|        - |   184 | `	}` |
|        - |   185 | `	/* Allocate a new constant instance */` |
|   581150 |   186 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   581150 |   187 | `	if( pCons == 0 ){` |
|      ! 0 |   188 | `		return 0;` |
|        - |   189 | `	}` |
|        - |   190 | `	/* Duplicate constant name */` |
|   581150 |   191 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   581150 |   192 | `	if( zDupName == 0 ){` |
|      ! 0 |   193 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   194 | `		return 0;` |
|        - |   195 | `	}` |
|        - |   196 | `	/* Install the constant */` |
|   581150 |   197 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   581150 |   198 | `	pCons->xExpand = xExpand;` |
|   581150 |   199 | `	pCons->pUserData = pUserData;` |
|   581150 |   200 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   581150 |   201 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   202 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   203 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   204 | `		return rc;` |
|        - |   205 | `	}` |
|        - |   206 | `	/* All done,constant can be invoked from PHP code */` |
|   581150 |   207 | `	return SXRET_OK;` |
|   290578 |   208 |  |
|        - |   209 | `/*` |
|        - |   210 | ` * Allocate a new foreign function instance.` |
|        - |   211 | ` * This function return SXRET_OK on success. Any other` |
|        - |   212 | ` * return value indicates failure.` |
|        - |   213 | ` * Please refer to the official documentation for an introduction to` |
|        - |   214 | ` * the foreign function mechanism.` |
|        - |   215 | ` */` |
|  1291406 |   216 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1291408 |   227 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1291408 |   228 | `	if( pFunc == 0 ){` |
|      ! 0 |   229 | `		return SXERR_MEM;` |
|        - |   230 | `	}` |
|        - |   231 | `	/* Duplicate function name */` |
|  1291408 |   232 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1291408 |   233 | `	if( zDup == 0 ){` |
|      ! 0 |   234 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   235 | `		return SXERR_MEM;` |
|        - |   236 | `	}` |
|        - |   237 | `	/* Zero the structure */` |
|  1291408 |   238 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   239 | `	/* Initialize structure fields */` |
|  1291408 |   240 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1291408 |   241 | `	pFunc->pVm   = pVm;` |
|  1291408 |   242 | `	pFunc->xFunc = xFunc;` |
|  1291408 |   243 | `	pFunc->pUserData = pUserData;` |
|  1291408 |   244 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   245 | `	/* Write a pointer to the new function */` |
|  1291408 |   246 | `	*ppOut = pFunc;` |
|  1291408 |   247 | `	return SXRET_OK;` |
|   645705 |   248 |  |
|        - |   249 | `/*` |
|        - |   250 | ` * Install a foreign function and it's associated callback so that` |
|        - |   251 | ` * it can be invoked from the target PHP code.` |
|        - |   252 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   253 | ` * return value indicates failure.` |
|        - |   254 | ` * Please refer to the official documentation for an introduction to` |
|        - |   255 | ` * the foreign function mechanism.` |
|        - |   256 | ` */` |
|  1294084 |   257 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1294086 |   268 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1294086 |   269 | `	if( pEntry ){` |
|     2680 |   270 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2680 |   271 | `		pFunc->pUserData = pUserData;` |
|     2680 |   272 | `		pFunc->xFunc = xFunc;` |
|     2680 |   273 | `		SySetReset(&pFunc->aAux);` |
|     2680 |   274 | `		return SXRET_OK;` |
|        - |   275 | `	}` |
|        - |   276 | `	/* Create a new user function */` |
|  1291408 |   277 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1291408 |   278 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   279 | `		return rc;` |
|        - |   280 | `	}` |
|        - |   281 | `	/* Install the function in the corresponding hashtable */` |
|  1291408 |   282 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1291408 |   283 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   284 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   285 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   286 | `		return rc;` |
|        - |   287 | `	}` |
|        - |   288 | `	/* User function successfully installed */` |
|  1291408 |   289 | `	return SXRET_OK;` |
|   647044 |   290 |  |
|        - |   291 | `/*` |
|        - |   292 | ` * Initialize a VM function.` |
|        - |   293 | ` */` |
|   234962 |   294 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   295 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   296 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   297 | `	const char *zName,  /* Function name */` |
|        - |   298 | `	sxu32 nByte,        /* zName length */` |
|        - |   299 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   300 | `	void *pUserData     /* Function private data */` |
|        - |   301 | `	)` |
|        2 |   302 |  |
|        - |   303 | `	/* Zero the structure */` |
|   234964 |   304 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   305 | `	/* Initialize structure fields */` |
|        - |   306 | `	/* Arguments container */` |
|   234964 |   307 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   308 | `	/* Static variable container */` |
|   234964 |   309 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   310 | `	/* Bytecode container */` |
|   234964 |   311 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   312 | `    /* Preallocate some instruction slots */` |
|   234964 |   313 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   314 | `	/* Closure environment */` |
|   234964 |   315 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   316 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   234964 |   317 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   234964 |   318 | `	pFunc->iFlags = iFlags;` |
|   234964 |   319 | `	pFunc->pUserData = pUserData;` |
|        - |   320 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |   321 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   234964 |   322 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   234964 |   323 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   234964 |   324 | `	return SXRET_OK;` |
|        2 |   325 |  |
|        - |   326 | `/*` |
|        - |   327 | ` * Namespace-aware function lookup.` |
|        - |   328 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   329 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   330 | ` */` |
|        - |   331 | `/*` |
|        - |   332 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   333 | ` */` |
|   721332 |   334 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   335 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   336 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   337 | `	SyString *pName     /* Function name */` |
|        - |   338 | `	)` |
|        2 |   339 |  |
|        - |   340 | `	SyHashEntry *pEntry;` |
|        - |   341 | `	sxi32 rc;` |
|   721334 |   342 | `	if( pName == 0 ){` |
|        - |   343 | `		/* Use the built-in name */` |
|    39854 |   344 | `		pName = &pFunc->sName;` |
|    19926 |   345 | `	}` |
|        - |   346 | `	/* Check for duplicates (functions with the same name) first */` |
|   721334 |   347 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   721334 |   348 | `	if( pEntry ){` |
|   534532 |   349 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   534532 |   350 | `		if( pLink != pFunc ){` |
|        - |   351 | `			/* Link */` |
|      188 |   352 | `			pFunc->pNextName = pLink;` |
|      188 |   353 | `			pEntry->pUserData = pFunc;` |
|       93 |   354 | `		}` |
|   534532 |   355 | `		return SXRET_OK;` |
|        - |   356 | `	}` |
|        - |   357 | `	/* First time seen */` |
|   186804 |   358 | `	pFunc->pNextName = 0;` |
|   186804 |   359 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   186804 |   360 | `	return rc;` |
|   360668 |   361 |  |
|        - |   362 | `/*` |
|        - |   363 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   364 | ` */` |
|    54754 |   365 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   366 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   367 | `	ph7_class *pClass /* Target Class */` |
|        - |   368 | `	)` |
|        2 |   369 |  |
|    54756 |   370 | `	SyString *pName = &pClass->sName;` |
|        - |   371 | `	SyHashEntry *pEntry;` |
|        - |   372 | `	sxi32 rc;` |
|        - |   373 | `	/* Check for duplicates */` |
|    54756 |   374 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    54756 |   375 | `	if( pEntry ){` |
|       31 |   376 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   377 | `		/* Link entry with the same name */` |
|       31 |   378 | `		pClass->pNextName = pLink;` |
|       31 |   379 | `		pEntry->pUserData = pClass;` |
|       31 |   380 | `		return SXRET_OK;` |
|        - |   381 | `	}` |
|    54726 |   382 | `	pClass->pNextName = 0;` |
|        - |   383 | `	/* Perform a simple hashtable insertion */` |
|    54726 |   384 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    54726 |   385 | `	return rc;` |
|    27379 |   386 |  |
|        - |   387 | `/*` |
|        - |   388 | ` * Instruction builder interface.` |
|        - |   389 | ` */` |
|  4052288 |   390 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  4052290 |   402 | `	sInstr.iOp = (sxu8)iOp;` |
|  4052290 |   403 | `	sInstr.iP1 = iP1;` |
|  4052290 |   404 | `	sInstr.iP2 = iP2;` |
|  4052290 |   405 | `	sInstr.p3  = p3;` |
|  4052290 |   406 | `	if( pIndex ){` |
|        - |   407 | `		/* Instruction index in the bytecode array */` |
|   220152 |   408 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   110075 |   409 | `	}` |
|        - |   410 | `	/* Finally,record the instruction */` |
|  4052290 |   411 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  4052290 |   412 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   413 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   414 | `		/* Fall throw */` |
|      ! 0 |   415 | `	}` |
|  4052290 |   416 | `	return rc;` |
|        2 |   417 |  |
|        - |   418 | `/*` |
|        - |   419 | ` * Swap the current bytecode container with the given one.` |
|        - |   420 | ` */` |
|   525952 |   421 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   422 |  |
|   525954 |   423 | `	if( pContainer == 0 ){` |
|        - |   424 | `		/* Point to the default container */` |
|      ! 0 |   425 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   426 | `	}else{` |
|        - |   427 | `		/* Change container */` |
|   525954 |   428 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   429 | `	}` |
|   525954 |   430 | `	return SXRET_OK;` |
|        2 |   431 |  |
|        - |   432 | `/*` |
|        - |   433 | ` * Return the current bytecode container.` |
|        - |   434 | ` */` |
|   262976 |   435 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   436 |  |
|   262978 |   437 | `	return pVm->pByteContainer;` |
|        2 |   438 |  |
|        - |   439 | `/*` |
|        - |   440 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   441 | ` */` |
|   217082 |   442 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   443 |  |
|        - |   444 | `	VmInstr *pInstr;` |
|   217084 |   445 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   217084 |   446 | `	return pInstr;` |
|        2 |   447 |  |
|        - |   448 | `/*` |
|        - |   449 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   450 | ` */` |
|  1218042 |   451 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   452 |  |
|  1218044 |   453 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   454 |  |
|        - |   455 | `/*` |
|        - |   456 | ` * Pop the last VM instruction.` |
|        - |   457 | ` */` |
|   200656 |   458 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   459 |  |
|   200658 |   460 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   461 |  |
|        - |   462 | `/*` |
|        - |   463 | ` * Peek the last VM instruction.` |
|        - |   464 | ` */` |
|   798182 |   465 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   466 |  |
|   798184 |   467 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   468 |  |
|    31620 |   469 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   470 |  |
|        - |   471 | `	VmInstr *aInstr;` |
|        - |   472 | `	sxu32 n;` |
|    31622 |   473 | `	n = SySetUsed(pVm->pByteContainer);` |
|    31622 |   474 | `	if( n < 2 ){` |
|      ! 0 |   475 | `		return 0;` |
|        - |   476 | `	}` |
|    31622 |   477 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    31622 |   478 | `	return &aInstr[n - 2];` |
|    15812 |   479 |  |
|        - |   480 | `/*` |
|        - |   481 | ` * Allocate a new virtual machine frame.` |
|        - |   482 | ` */` |
|    20964 |   483 | `static VmFrame * VmNewFrame(` |
|        - |   484 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   485 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   486 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   487 | `	)` |
|        2 |   488 |  |
|        - |   489 | `	VmFrame *pFrame;` |
|        - |   490 | `	/* Allocate a new vm frame */` |
|    20966 |   491 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    20966 |   492 | `	if( pFrame == 0 ){` |
|      ! 0 |   493 | `		return 0;` |
|        - |   494 | `	}` |
|        - |   495 | `	/* Zero the structure */` |
|    20966 |   496 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   497 | `	/* Initialize frame fields */` |
|    20966 |   498 | `	pFrame->pUserData = pUserData;` |
|    20966 |   499 | `	pFrame->pThis = pThis;` |
|    20966 |   500 | `	pFrame->pVm = pVm;` |
|    20966 |   501 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    20966 |   502 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    20966 |   503 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    20966 |   504 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    20966 |   505 | `	return pFrame;` |
|    10484 |   506 |  |
|        - |   507 | `/* Forward declaration */` |
|        - |   508 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   509 | `/*` |
|        - |   510 | ` * Enter a VM frame.` |
|        - |   511 | ` */` |
|    20918 |   512 | `static sxi32 VmEnterFrame(` |
|        - |   513 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   514 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   515 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   516 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   517 | `	)` |
|        2 |   518 |  |
|        - |   519 | `	VmFrame *pFrame;` |
|        - |   520 | `	/* Allocate a new frame */` |
|    20920 |   521 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    20920 |   522 | `	if( pFrame == 0 ){` |
|      ! 0 |   523 | `		return SXERR_MEM;` |
|        - |   524 | `	}` |
|        - |   525 | `	/* Link to the list of active VM frame */` |
|    20920 |   526 | `	pFrame->pParent = pVm->pFrame;` |
|    20920 |   527 | `	pVm->pFrame = pFrame;` |
|    20920 |   528 | `	if( ppFrame ){` |
|        - |   529 | `		/* Write a pointer to the new VM frame */` |
|    17928 |   530 | `		*ppFrame = pFrame;` |
|     8963 |   531 | `	}` |
|    20920 |   532 | `	return SXRET_OK;` |
|    10461 |   533 |  |
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
|    17916 |   577 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   578 |  |
|    17918 |   579 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    17918 |   580 | `	if( pCurFrame ){` |
|        - |   581 | `		/* Unlink from the list of active VM frame */` |
|    17918 |   582 | `		pVm->pFrame = pCurFrame->pParent;` |
|    17918 |   583 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   584 | `			VmSlot  *aSlot;` |
|        - |   585 | `			sxu32 n;` |
|        - |   586 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    17618 |   587 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   118252 |   588 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   589 | `				/* Unset the local variable */` |
|   100636 |   590 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    50319 |   591 | `			}` |
|        - |   592 | `			/* Remove local reference */` |
|    17618 |   593 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   118314 |   594 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   100698 |   595 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    50350 |   596 | `			}` |
|     8808 |   597 | `		}` |
|        - |   598 | `		/* Release internal containers */` |
|    17918 |   599 | `		SyHashRelease(&pCurFrame->hVar);` |
|    17918 |   600 | `		SySetRelease(&pCurFrame->sArg);` |
|    17918 |   601 | `		SySetRelease(&pCurFrame->sLocal);` |
|    17918 |   602 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   603 | `		/* Release the whole structure */` |
|    17918 |   604 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     8958 |   605 | `	}` |
|    17918 |   606 |  |
|        - |   607 | `/*` |
|        - |   608 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   609 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   610 | ` * should be skipped when looking for the real execution context.` |
|        - |   611 | ` */` |
|  6958712 |   612 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   613 |  |
|  6960656 |   614 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     1944 |   615 | `		pFrame = pFrame->pParent;` |
|        2 |   616 | `	}` |
|  6958714 |   617 | `	return pFrame;` |
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
|   159796 |   737 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   738 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   739 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   740 | `	)` |
|        2 |   741 |  |
|        - |   742 | `	ph7_class_method *pMeth;` |
|        - |   743 | `	ph7_class_attr *pAttr;` |
|        - |   744 | `	SyHashEntry *pEntry;` |
|        - |   745 | `	sxi32 rc;` |
|        - |   746 | `	/* Reset the loop cursor */` |
|   159798 |   747 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   748 | `	/* Process only static and constant attribute */` |
|   623132 |   749 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   750 | `		/* Extract the current attribute */` |
|   383438 |   751 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   383438 |   752 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   753 | `			ph7_value *pMemObj;` |
|        - |   754 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1776 |   755 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1776 |   756 | `			if( pMemObj == 0 ){` |
|      ! 0 |   757 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   758 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   759 | `					&pClass->sName,&pAttr->sName` |
|        - |   760 | `					);` |
|      ! 0 |   761 | `				return SXERR_MEM;` |
|        - |   762 | `			}` |
|     1776 |   763 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   764 | `				/* Initialize attribute default value (any complex expression) */` |
|     1772 |   765 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      885 |   766 | `			}` |
|        - |   767 | `			/* Record attribute index */` |
|     1776 |   768 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   769 | `			/* Install static attribute in the reference table */` |
|     1776 |   770 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   771 | `			/* If this is a typed static property, register the slot so the` |
|        - |   772 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |   773 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |   774 | `			 * points at its own nIdx field (stable for the VM lifetime). */` |
|     1776 |   775 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
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
|      887 |   794 | `		}` |
|        2 |   795 | `	}` |
|        - |   796 | `	/* Install class methods */` |
|   159798 |   797 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   798 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   799 | `		 */` |
|    79676 |   800 | `		return SXRET_OK;` |
|        - |   801 | `	}` |
|        - |   802 | `	/* Create constructor alias if not yet done */` |
|    80124 |   803 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   804 | `		/* User constructor with the same base class name */` |
|     6276 |   805 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     6276 |   806 | `		if( pEntry ){` |
|      ! 0 |   807 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   808 | `			/* Create the alias */` |
|      ! 0 |   809 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   810 | `		}` |
|     3137 |   811 | `	}` |
|        - |   812 | `	/* Install the methods now */` |
|    80124 |   813 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   801673 |   814 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   681490 |   815 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   681490 |   816 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   681482 |   817 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   681482 |   818 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   819 | `				return rc;` |
|        - |   820 | `			}` |
|   340740 |   821 | `		}` |
|        2 |   822 | `	}` |
|        - |   823 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    80124 |   824 | `	pClass->bMounted = TRUE;` |
|    80124 |   825 | `	return SXRET_OK;` |
|    79900 |   826 |  |
|        - |   827 | `/*` |
|        - |   828 | ` * Allocate a private frame for attributes of the given` |
|        - |   829 | ` * class instance (Object in the PHP jargon).` |
|        - |   830 | ` */` |
|     1882 |   831 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   832 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   833 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   834 | `	)` |
|        2 |   835 |  |
|     1884 |   836 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   837 | `	ph7_class_attr *pAttr;` |
|        - |   838 | `	SyHashEntry *pEntry;` |
|        - |   839 | `	sxi32 rc;` |
|        - |   840 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1884 |   841 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     7836 |   842 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   843 | `		VmClassAttr *pVmAttr;` |
|        - |   844 | `		/* Extract the current attribute */` |
|     5954 |   845 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     5954 |   846 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     5954 |   847 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   848 | `			return SXERR_MEM;` |
|        - |   849 | `		}` |
|     5954 |   850 | `		pVmAttr->pAttr = pAttr;` |
|     5954 |   851 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   852 | `			ph7_value *pMemObj;` |
|        - |   853 | `			/* Reserve a memory object for this attribute */` |
|     5930 |   854 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     5930 |   855 | `			if( pMemObj == 0 ){` |
|      ! 0 |   856 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   857 | `				return SXERR_MEM;` |
|        - |   858 | `			}` |
|     5930 |   859 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     5930 |   860 | `			pVmAttr->iState = 0;` |
|     5930 |   861 | `			pVmAttr->pOwner = pClass;` |
|     5930 |   862 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   863 | `				/* Initialize attribute default value (any complex expression) */` |
|     2024 |   864 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     4919 |   865 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   866 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   867 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       68 |   868 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       33 |   869 | `			}` |
|     5930 |   870 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     5930 |   871 | `			if( rc != SXRET_OK ){` |
|        - |   872 | `				VmSlot sSlot;` |
|        - |   873 | `				/* Restore memory object */` |
|      ! 0 |   874 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   875 | `				sSlot.pUserData = 0;` |
|      ! 0 |   876 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   877 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   878 | `				return SXERR_MEM;` |
|        - |   879 | `			}` |
|        - |   880 | `			/* Install attribute in the reference table */` |
|     5930 |   881 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   882 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   883 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   884 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     5930 |   885 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      162 |   886 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      162 |   887 | `				if( rc != SXRET_OK ){` |
|        - |   888 | `					VmSlot sSlot;` |
|      ! 0 |   889 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 |   890 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   891 | `					sSlot.pUserData = 0;` |
|      ! 0 |   892 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   893 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   894 | `					return SXERR_MEM;` |
|        - |   895 | `				}` |
|       80 |   896 | `			}` |
|     2966 |   897 | `		}else{` |
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
|     1884 |   909 | `	return SXRET_OK;` |
|      943 |   910 |  |
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
|   433074 |   922 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   923 |  |
|        - |   924 | `	ph7_value *pObj;` |
|        - |   925 | `	sxi32 rc;` |
|   433076 |   926 | `	if( pIndex ){` |
|        - |   927 | `		/* Object index in the object table */` |
|   424100 |   928 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   212049 |   929 | `	}` |
|        - |   930 | `	/* Reserve a slot for the new object */` |
|   433076 |   931 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   433076 |   932 | `	if( rc != SXRET_OK ){` |
|        - |   933 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   934 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   935 | `		 */` |
|      ! 0 |   936 | `		return 0;` |
|        - |   937 | `	}` |
|   433076 |   938 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   433076 |   939 | `	return pObj;` |
|   216539 |   940 |  |
|        - |   941 | `/*` |
|        - |   942 | ` * Reserve a memory object.` |
|        - |   943 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   944 | ` */` |
|  2149314 |   945 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   946 |  |
|        - |   947 | `	ph7_value *pObj;` |
|        - |   948 | `	sxi32 rc;` |
|  2149316 |   949 | `	if( pIndex ){` |
|        - |   950 | `		/* Object index in the object table */` |
|  2149316 |   951 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1074657 |   952 | `	}` |
|        - |   953 | `	/* Reserve a slot for the new object */` |
|  2149316 |   954 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2149316 |   955 | `	if( rc != SXRET_OK ){` |
|        - |   956 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   957 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   958 | `		 */` |
|      ! 0 |   959 | `		return 0;` |
|        - |   960 | `	}` |
|  2149316 |   961 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2149316 |   962 | `	return pObj;` |
|  1074659 |   963 |  |
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
|        - |   985 | `static sxi32 VmCallObjectInvoke(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |   986 | `	int nArg, ph7_value **apArg, ph7_value *pResult, VmCallArgMap *pMap);` |
|        - |   987 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis);` |
|        - |   988 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |   989 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   990 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |   991 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   992 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   993 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   994 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   995 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   996 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   997 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   998 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   999 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1000 | `/*` |
|        - |  1001 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |  1002 | ` * directly as foreign functions.` |
|        - |  1003 | ` */` |
|        - |  1004 | `#define PH7_BUILTIN_LIB \` |
|        - |  1005 | `	"interface Throwable {"\` |
|        - |  1006 | `	"public function getMessage();"\` |
|        - |  1007 | `	"public function getCode();"\` |
|        - |  1008 | `	"public function getFile();"\` |
|        - |  1009 | `	"public function getLine();"\` |
|        - |  1010 | `	"public function getTrace();"\` |
|        - |  1011 | `	"public function getTraceAsString();"\` |
|        - |  1012 | `	"public function getPrevious();"\` |
|        - |  1013 | `	"public function __toString();"\` |
|        - |  1014 | `	"}"\` |
|        - |  1015 | `	"class Exception implements Throwable { "\` |
|        - |  1016 | `    "protected $message = '';"\` |
|        - |  1017 | `    "protected $code = 0;"\` |
|        - |  1018 | `    "protected $file;"\` |
|        - |  1019 | `    "protected $line;"\` |
|        - |  1020 | `    "protected $trace;"\` |
|        - |  1021 | `    "protected $previous;"\` |
|        - |  1022 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1023 | `	"   if( isset($message) ){"\` |
|        - |  1024 | `	"	  $this->message = $message;"\` |
|        - |  1025 | `	"   }"\` |
|        - |  1026 | `	"   $this->code = $code;"\` |
|        - |  1027 | `	"   $this->file = __FILE__;"\` |
|        - |  1028 | `	"   $this->line = __LINE__;"\` |
|        - |  1029 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1030 | `	"   if( isset($previous) ){"\` |
|        - |  1031 | `	"     $this->previous = $previous;"\` |
|        - |  1032 | `	"   }"\` |
|        - |  1033 | `	"}"\` |
|        - |  1034 | `	"public function getMessage(){"\` |
|        - |  1035 | `	"   return $this->message;"\` |
|        - |  1036 | `	"}"\` |
|        - |  1037 | `	" public function getCode(){"\` |
|        - |  1038 | `	"  return $this->code;"\` |
|        - |  1039 | `	"}"\` |
|        - |  1040 | `	"public function getFile(){"\` |
|        - |  1041 | `	"  return $this->file;"\` |
|        - |  1042 | `	"}"\` |
|        - |  1043 | `	"public function getLine(){"\` |
|        - |  1044 | `	"  return $this->line;"\` |
|        - |  1045 | `	"}"\` |
|        - |  1046 | `	"public function getTrace(){"\` |
|        - |  1047 | `	"   return $this->trace;"\` |
|        - |  1048 | `	"}"\` |
|        - |  1049 | `	"public function getTraceAsString(){"\` |
|        - |  1050 | `	"  return debug_string_backtrace();"\` |
|        - |  1051 | `	"}"\` |
|        - |  1052 | `	"public function getPrevious(){"\` |
|        - |  1053 | `	"    return $this->previous;"\` |
|        - |  1054 | `	"}"\` |
|        - |  1055 | `	"public function __toString(){"\` |
|        - |  1056 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1057 | `    "}"\` |
|        - |  1058 | `	"}"\` |
|        - |  1059 | `	"class Error implements Throwable { "\` |
|        - |  1060 | `    "protected $message = '';"\` |
|        - |  1061 | `    "protected $code = 0;"\` |
|        - |  1062 | `    "protected $file;"\` |
|        - |  1063 | `    "protected $line;"\` |
|        - |  1064 | `    "protected $trace;"\` |
|        - |  1065 | `    "protected $previous;"\` |
|        - |  1066 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1067 | `	"   if( isset($message) ){"\` |
|        - |  1068 | `	"	  $this->message = $message;"\` |
|        - |  1069 | `	"   }"\` |
|        - |  1070 | `	"   $this->code = $code;"\` |
|        - |  1071 | `	"   $this->file = __FILE__;"\` |
|        - |  1072 | `	"   $this->line = __LINE__;"\` |
|        - |  1073 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1074 | `	"   if( isset($previous) ){"\` |
|        - |  1075 | `	"     $this->previous = $previous;"\` |
|        - |  1076 | `	"   }"\` |
|        - |  1077 | `	"}"\` |
|        - |  1078 | `	"public function getMessage(){"\` |
|        - |  1079 | `	"   return $this->message;"\` |
|        - |  1080 | `	"}"\` |
|        - |  1081 | `	"public function getCode(){"\` |
|        - |  1082 | `	"  return $this->code;"\` |
|        - |  1083 | `	"}"\` |
|        - |  1084 | `	"public function getFile(){"\` |
|        - |  1085 | `	"  return $this->file;"\` |
|        - |  1086 | `	"}"\` |
|        - |  1087 | `	"public function getLine(){"\` |
|        - |  1088 | `	"  return $this->line;"\` |
|        - |  1089 | `	"}"\` |
|        - |  1090 | `	"public function getTrace(){"\` |
|        - |  1091 | `	"   return $this->trace;"\` |
|        - |  1092 | `	"}"\` |
|        - |  1093 | `	"public function getTraceAsString(){"\` |
|        - |  1094 | `	"  return debug_string_backtrace();"\` |
|        - |  1095 | `	"}"\` |
|        - |  1096 | `	"public function getPrevious(){"\` |
|        - |  1097 | `	"    return $this->previous;"\` |
|        - |  1098 | `	"}"\` |
|        - |  1099 | `	"public function __toString(){"\` |
|        - |  1100 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1101 | `	"}"\` |
|        - |  1102 | `	"}"\` |
|        - |  1103 | `	"class TypeError extends Error { }"\` |
|        - |  1104 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |  1105 | `	"class ValueError extends Error { }"\` |
|        - |  1106 | `	"class FiberError extends Error { }"\` |
|        - |  1107 | `	"class AssertionError extends Error { }"\` |
|        - |  1108 | `	"class ArithmeticError extends Error { }"\` |
|        - |  1109 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1110 | `	"class ErrorException extends Exception { "\` |
|        - |  1111 | `	"protected $severity;"\` |
|        - |  1112 | `	"public function __construct(string $message = null,"\` |
|        - |  1113 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Throwable $previous = null){"\` |
|        - |  1114 | `	"   if( isset($message) ){"\` |
|        - |  1115 | `	"	  $this->message = $message;"\` |
|        - |  1116 | `	"   }"\` |
|        - |  1117 | `	"   $this->severity = $severity;"\` |
|        - |  1118 | `	"   $this->code = $code;"\` |
|        - |  1119 | `	"   $this->file = $filename;"\` |
|        - |  1120 | `	"   $this->line = $lineno;"\` |
|        - |  1121 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1122 | `	"   if( isset($previous) ){"\` |
|        - |  1123 | `	"     $this->previous = $previous;"\` |
|        - |  1124 | `	"   }"\` |
|        - |  1125 | `	"}"\` |
|        - |  1126 | `	"public function getSeverity(){"\` |
|        - |  1127 | `	"   return $this->severity;"\` |
|        - |  1128 | `    "}"\` |
|        - |  1129 | `	"}"\` |
|        - |  1130 | `	"interface Iterator {"\` |
|        - |  1131 | `	"public function current();"\` |
|        - |  1132 | `	"public function key();"\` |
|        - |  1133 | `	"public function next();"\` |
|        - |  1134 | `	"public function rewind();"\` |
|        - |  1135 | `	"public function valid();"\` |
|        - |  1136 | `	"}"\` |
|        - |  1137 | `	"interface IteratorAggregate {"\` |
|        - |  1138 | `	"public function getIterator();"\` |
|        - |  1139 | `	"}"\` |
|        - |  1140 | `	"interface Serializable {"\` |
|        - |  1141 | `	"public function serialize();"\` |
|        - |  1142 | `	"public function unserialize(string $serialized);"\` |
|        - |  1143 | `	"}"\` |
|        - |  1144 | `	"/* Directory releated IO */"\` |
|        - |  1145 | `	"class Directory {"\` |
|        - |  1146 | `	"public $handle = null;"\` |
|        - |  1147 | `	"public $path  = null;"\` |
|        - |  1148 | `	"public function __construct(string $path)"\` |
|        - |  1149 | `	"{"\` |
|        - |  1150 | `	"   $this->handle = opendir($path);"\` |
|        - |  1151 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1152 | `	"      $this->path = $path;"\` |
|        - |  1153 | `	"   }"\` |
|        - |  1154 | `	"}"\` |
|        - |  1155 | `	"public function __destruct()"\` |
|        - |  1156 | `	"{"\` |
|        - |  1157 | `	"  if( $this->handle != null ){"\` |
|        - |  1158 | `	"       closedir($this->handle);"\` |
|        - |  1159 | `	"  }"\` |
|        - |  1160 | `	"}"\` |
|        - |  1161 | `	"public function read()"\` |
|        - |  1162 | `	"{"\` |
|        - |  1163 | `	"    return readdir($this->handle);"\` |
|        - |  1164 | `	"}"\` |
|        - |  1165 | `	"public function rewind()"\` |
|        - |  1166 | `	"{"\` |
|        - |  1167 | `	"    rewinddir($this->handle);"\` |
|        - |  1168 | `	"}"\` |
|        - |  1169 | `	"public function close()"\` |
|        - |  1170 | `	"{"\` |
|        - |  1171 | `	"    closedir($this->handle);"\` |
|        - |  1172 | `	"    $this->handle = null;"\` |
|        - |  1173 | `	"}"\` |
|        - |  1174 | `	"}"\` |
|        - |  1175 | `	"class Fiber {"\` |
|        - |  1176 | `	"  private $__ctx;"\` |
|        - |  1177 | `	"  private $__callable;"\` |
|        - |  1178 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1179 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1180 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1181 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1182 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1183 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1184 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1185 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1186 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1187 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1188 | `	"}"\` |
|        - |  1189 | `	"class Generator implements Iterator {"\` |
|        - |  1190 | `	"  private $__ctx;"\` |
|        - |  1191 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1192 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1193 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1194 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1195 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1196 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1197 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1198 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1199 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1200 | `	"}"\` |
|        - |  1201 | `	"class stdClass{"\` |
|        - |  1202 | `	"  public $value;"\` |
|        - |  1203 | `	" /* Magic methods */"\` |
|        - |  1204 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1205 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1206 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1207 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1208 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1209 | `	"}"\` |
|        - |  1210 | `	"function dir(string $path){"\` |
|        - |  1211 | `	"   return new Directory($path);"\` |
|        - |  1212 | `	"}"\` |
|        - |  1213 | `	"function Dir(string $path){"\` |
|        - |  1214 | `	"   return new Directory($path);"\` |
|        - |  1215 | `	"}"\` |
|        - |  1216 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1217 | `    "{"\` |
|        - |  1218 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1219 | `	"  $aDir = array();"\` |
|        - |  1220 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1221 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1222 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1223 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1224 | `	"   }"\` |
|        - |  1225 | `	"  closedir($pHandle);"\` |
|        - |  1226 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1227 | `	"      rsort($aDir);"\` |
|        - |  1228 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1229 | `	"      sort($aDir);"\` |
|        - |  1230 | `	"  }"\` |
|        - |  1231 | `	"  return $aDir;"\` |
|        - |  1232 | `	"}"\` |
|        - |  1233 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1234 | `	"/* Open the target directory */"\` |
|        - |  1235 | `	"$zDir = dirname($pattern);"\` |
|        - |  1236 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1237 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1238 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1239 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1240 | `	"	return FALSE;"\` |
|        - |  1241 | `	"}"\` |
|        - |  1242 | `	"$pattern = basename($pattern);"\` |
|        - |  1243 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1244 | `	"/* Loop throw available entries */"\` |
|        - |  1245 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1246 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1247 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1248 | `	"	if( $rc ){"\` |
|        - |  1249 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1250 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1251 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1252 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1253 | `	"		  }"\` |
|        - |  1254 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1255 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1256 | `	"		 continue;"\` |
|        - |  1257 | `	"	   }"\` |
|        - |  1258 | `	"	   /* Add the entry */"\` |
|        - |  1259 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1260 | `	"	}"\` |
|        - |  1261 | `	" }"\` |
|        - |  1262 | `	"/* Close the handle */"\` |
|        - |  1263 | `	"closedir($pHandle);"\` |
|        - |  1264 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1265 | `	"  /* Sort the array */"\` |
|        - |  1266 | `	"  sort($pArray);"\` |
|        - |  1267 | `	"}"\` |
|        - |  1268 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1269 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1270 | `	"  $pArray[] = $pattern;"\` |
|        - |  1271 | `	"}"\` |
|        - |  1272 | `	"/* Return the created array */"\` |
|        - |  1273 | `	"return $pArray;"\` |
|        - |  1274 | `   "}"\` |
|        - |  1275 | `   "/* Creates a temporary file */"\` |
|        - |  1276 | `   "function tmpfile(){"\` |
|        - |  1277 | `   "  /* Extract the temp directory */"\` |
|        - |  1278 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1279 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1280 | `   "    /* Use the current dir */"\` |
|        - |  1281 | `   "    $zTempDir = '.';"\` |
|        - |  1282 | `   "  }"\` |
|        - |  1283 | `   "  /* Create the file */"\` |
|        - |  1284 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1285 | `   "  return $pHandle;"\` |
|        - |  1286 | `   "}"\` |
|        - |  1287 | `   "/* Creates a temporary filename */"\` |
|        - |  1288 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1289 | `   "{"\` |
|        - |  1290 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1291 | `   "}"\` |
|        - |  1292 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1293 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1294 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1295 | `   "/* Copy arguments */"\` |
|        - |  1296 | `   "$nArgs = func_num_args();"\` |
|        - |  1297 | `   "$pNew = array();"\` |
|        - |  1298 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1299 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1300 | `    "}"\` |
|        - |  1301 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1302 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1303 | `	"/* Erase */"\` |
|        - |  1304 | `	"array_erase($pArray);"\` |
|        - |  1305 | `	"/* Unshift */"\` |
|        - |  1306 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1307 | `	"return sizeof($pArray);"\` |
|        - |  1308 | `    "}"\` |
|        - |  1309 | `	"function array_merge_recursive(){"\` |
|        - |  1310 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1311 | `    "$arrays = func_get_args();"\` |
|        - |  1312 | `    "$narrays = count($arrays);"\` |
|        - |  1313 | `    "$ret = array();"\` |
|        - |  1314 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1315 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1316 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1317 | `	 " }"\` |
|        - |  1318 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1319 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1320 | `     "  if( $keyIsInt ) {"\` |
|        - |  1321 | `     "   $ret[] = $value;"\` |
|        - |  1322 | `     "  } else {"\` |
|        - |  1323 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1324 | `     "    $cur = $ret[$key];"\` |
|        - |  1325 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1326 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1327 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1328 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1329 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1330 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1331 | `     "    } else {"\` |
|        - |  1332 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1333 | `     "    }"\` |
|        - |  1334 | `     "   } else {"\` |
|        - |  1335 | `     "    $ret[$key] = $value;"\` |
|        - |  1336 | `     "   }"\` |
|        - |  1337 | `     "  }"\` |
|        - |  1338 | `     " }"\` |
|        - |  1339 | `	 " }"\` |
|        - |  1340 | `	 " return $ret;"\` |
|        - |  1341 | `    "}"\` |
|        - |  1342 | `	"function max(){"\` |
|        - |  1343 | `    "  $pArgs = func_get_args();"\` |
|        - |  1344 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1345 | `	"  return null;"\` |
|        - |  1346 | `    " }"\` |
|        - |  1347 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1348 | `    " $pArg = $pArgs[0];"\` |
|        - |  1349 | `	" if( !is_array($pArg) ){"\` |
|        - |  1350 | `	"   return $pArg; "\` |
|        - |  1351 | `	" }"\` |
|        - |  1352 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1353 | `	"   return null;"\` |
|        - |  1354 | `	" }"\` |
|        - |  1355 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1356 | `	" reset($pArg);"\` |
|        - |  1357 | `	" $max = current($pArg);"\` |
|        - |  1358 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1359 | `	"   if( $val > $max ){"\` |
|        - |  1360 | `	"     $max = $val;"\` |
|        - |  1361 | `    " }"\` |
|        - |  1362 | `	" }"\` |
|        - |  1363 | `	" return $max;"\` |
|        - |  1364 | `    " }"\` |
|        - |  1365 | `    " $max = $pArgs[0];"\` |
|        - |  1366 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1367 | `    " $val = $pArgs[$i];"\` |
|        - |  1368 | `	"if( $val > $max ){"\` |
|        - |  1369 | `	" $max = $val;"\` |
|        - |  1370 | `	"}"\` |
|        - |  1371 | `    " }"\` |
|        - |  1372 | `	" return $max;"\` |
|        - |  1373 | `    "}"\` |
|        - |  1374 | `	"function min(){"\` |
|        - |  1375 | `    "  $pArgs = func_get_args();"\` |
|        - |  1376 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1377 | `	"  return null;"\` |
|        - |  1378 | `    " }"\` |
|        - |  1379 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1380 | `    " $pArg = $pArgs[0];"\` |
|        - |  1381 | `	" if( !is_array($pArg) ){"\` |
|        - |  1382 | `	"   return $pArg; "\` |
|        - |  1383 | `	" }"\` |
|        - |  1384 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1385 | `	"   return null;"\` |
|        - |  1386 | `	" }"\` |
|        - |  1387 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1388 | `	" reset($pArg);"\` |
|        - |  1389 | `	" $min = current($pArg);"\` |
|        - |  1390 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1391 | `	"   if( $val < $min ){"\` |
|        - |  1392 | `	"     $min = $val;"\` |
|        - |  1393 | `    " }"\` |
|        - |  1394 | `	" }"\` |
|        - |  1395 | `	" return $min;"\` |
|        - |  1396 | `    " }"\` |
|        - |  1397 | `    " $min = $pArgs[0];"\` |
|        - |  1398 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1399 | `    " $val = $pArgs[$i];"\` |
|        - |  1400 | `	"if( $val < $min ){"\` |
|        - |  1401 | `	" $min = $val;"\` |
|        - |  1402 | `	" }"\` |
|        - |  1403 | `    " }"\` |
|        - |  1404 | `	" return $min;"\` |
|        - |  1405 | `	"}"\` |
|        - |  1406 | `	"function fileowner(string $file){"\` |
|        - |  1407 | `    " $a = stat($file);"\` |
|        - |  1408 | `	" if( !is_array($a) ){"\` |
|        - |  1409 | `	"	return false;"\` |
|        - |  1410 | `	" }"\` |
|        - |  1411 | `	" return $a['uid'];"\` |
|        - |  1412 | `    "}"\` |
|        - |  1413 | `    "function filegroup(string $file){"\` |
|        - |  1414 | `	" $a = stat($file);"\` |
|        - |  1415 | `	" if( !is_array($a) ){"\` |
|        - |  1416 | `	"	return false;"\` |
|        - |  1417 | `	" }"\` |
|        - |  1418 | `	" return $a['gid'];"\` |
|        - |  1419 | `    "}"\` |
|        - |  1420 | `	 "function fileinode(string $file){"\` |
|        - |  1421 | `	" $a = stat($file);"\` |
|        - |  1422 | `	" if( !is_array($a) ){"\` |
|        - |  1423 | `	"	return false;"\` |
|        - |  1424 | `	" }"\` |
|        - |  1425 | `	" return $a['ino'];"\` |
|        - |  1426 | `    "}"` |
|        - |  1427 |  |
|        - |  1428 | `/*` |
|        - |  1429 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1430 | ` * start compiling the target PHP program.` |
|        - |  1431 | ` */` |
|     2992 |  1432 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1433 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1434 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1435 | `	 )` |
|        2 |  1436 |  |
|        - |  1437 | `	SyString sBuiltin;` |
|        - |  1438 | `	ph7_value *pObj;` |
|        - |  1439 | `	sxi32 rc;` |
|        - |  1440 | `	/* Zero the structure */` |
|     2994 |  1441 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1442 | `	/* Initialize VM fields */` |
|     2994 |  1443 | `	pVm->pEngine = &(*pEngine);` |
|     2994 |  1444 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1445 | `	/* Instructions containers */` |
|     2994 |  1446 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2994 |  1447 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2994 |  1448 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1449 | `	/* Object containers */` |
|     2994 |  1450 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2994 |  1451 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1452 | `	/* Virtual machine internal containers */` |
|     2994 |  1453 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2994 |  1454 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2994 |  1455 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2994 |  1456 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2994 |  1457 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2994 |  1458 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2994 |  1459 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2994 |  1460 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2994 |  1461 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2994 |  1462 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     2994 |  1463 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2994 |  1464 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2994 |  1465 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2994 |  1466 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2994 |  1467 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2994 |  1468 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2994 |  1469 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2994 |  1470 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     2994 |  1471 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     2994 |  1472 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     2994 |  1473 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2994 |  1474 | `	pVm->pPendingException = 0;` |
|        - |  1475 | `	/* Configuration containers */` |
|     2994 |  1476 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2994 |  1477 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2994 |  1478 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2994 |  1479 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2994 |  1480 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2994 |  1481 | `	pVm->iResponseStatus = 200;` |
|     2994 |  1482 | `	pVm->bHeadersSent = 0;` |
|     2994 |  1483 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1484 | `	/* Error callbacks containers */` |
|     2994 |  1485 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2994 |  1486 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2994 |  1487 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2994 |  1488 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2994 |  1489 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1490 | `	/* Set a default recursion limit */` |
|        - |  1491 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2994 |  1492 | `	pVm->nMaxDepth = 32;` |
|        - |  1493 | `#else` |
|        - |  1494 | `	pVm->nMaxDepth = 16;` |
|        - |  1495 | `#endif` |
|        - |  1496 | `	/* Default assertion flags */` |
|     2994 |  1497 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1498 | `	/* JSON return status */` |
|     2994 |  1499 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1500 | `	/* PRNG context */` |
|     2994 |  1501 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1502 | `	/* Install the null constant */` |
|     2994 |  1503 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2994 |  1504 | `	if( pObj == 0 ){` |
|      ! 0 |  1505 | `		rc = SXERR_MEM;` |
|      ! 0 |  1506 | `		goto Err;` |
|        - |  1507 | `	}` |
|     2994 |  1508 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1509 | `	/* Install the boolean TRUE constant */` |
|     2994 |  1510 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2994 |  1511 | `	if( pObj == 0 ){` |
|      ! 0 |  1512 | `		rc = SXERR_MEM;` |
|      ! 0 |  1513 | `		goto Err;` |
|        - |  1514 | `	}` |
|     2994 |  1515 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1516 | `	/* Install the boolean FALSE constant */` |
|     2994 |  1517 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2994 |  1518 | `	if( pObj == 0 ){` |
|      ! 0 |  1519 | `		rc = SXERR_MEM;` |
|      ! 0 |  1520 | `		goto Err;` |
|        - |  1521 | `	}` |
|     2994 |  1522 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1523 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1524 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1525 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2994 |  1526 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2994 |  1527 | `	if( pObj == 0 ){` |
|      ! 0 |  1528 | `		rc = SXERR_MEM;` |
|      ! 0 |  1529 | `		goto Err;` |
|        - |  1530 | `	}` |
|     2994 |  1531 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1532 | `	/* Create the global frame */` |
|     2994 |  1533 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2994 |  1534 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1535 | `		goto Err;` |
|        - |  1536 | `	}` |
|        - |  1537 | `	/* Initialize the code generator */` |
|     2994 |  1538 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2994 |  1539 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1540 | `		goto Err;` |
|        - |  1541 | `	}` |
|        - |  1542 | `	/* VM correctly initialized,set the magic number */` |
|     2994 |  1543 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2994 |  1544 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1545 | `	/* Compile the built-in library */` |
|     2994 |  1546 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1547 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2994 |  1548 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1549 | `	/* Register Fiber internal C functions */` |
|     2994 |  1550 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2994 |  1551 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2994 |  1552 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2994 |  1553 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2994 |  1554 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2994 |  1555 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2994 |  1556 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2994 |  1557 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2994 |  1558 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2994 |  1559 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1560 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2994 |  1561 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2994 |  1562 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2994 |  1563 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2994 |  1564 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2994 |  1565 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2994 |  1566 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2994 |  1567 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2994 |  1568 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2994 |  1569 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2994 |  1570 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1571 | `	/* Reset the code generator */` |
|     2994 |  1572 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2994 |  1573 | `	return SXRET_OK;` |
|      ! 0 |  1574 | `Err:` |
|      ! 0 |  1575 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1576 | `	return rc;` |
|     1498 |  1577 |  |
|        - |  1578 | `/*` |
|        - |  1579 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1580 | ` * routine which store the output in an internal blob.` |
|        - |  1581 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1582 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1583 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1584 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1585 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1586 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1587 | ` * to finish executing and extracting the output.` |
|        - |  1588 | ` */` |
|       38 |  1589 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1590 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1591 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1592 | `	void *pUserData     /* User private data */` |
|        - |  1593 | `	)` |
|      ! 0 |  1594 |  |
|        - |  1595 | `	 sxi32 rc;` |
|        - |  1596 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1597 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1598 | `	 return rc;` |
|      ! 0 |  1599 |  |
|        - |  1600 | `/*` |
|        - |  1601 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1602 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1603 | ` */` |
|    17880 |  1604 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1605 |  |
|    17882 |  1606 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    17882 |  1607 | `	if( xCons != VmObConsumer ){` |
|     7448 |  1608 | `		pVm->nOutputLen += nLen;` |
|     7448 |  1609 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      950 |  1610 | `			pVm->bHeadersSent = 1;` |
|      474 |  1611 | `		}` |
|     3723 |  1612 | `	}` |
|    17882 |  1613 |  |
|        - |  1614 | `#define VM_STACK_GUARD 16` |
|        - |  1615 | `/*` |
|        - |  1616 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1617 | ` * our compiled PHP program.` |
|        - |  1618 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1619 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1620 | ` */` |
|    42234 |  1621 | `static ph7_value * VmNewOperandStack(` |
|        - |  1622 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1623 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1624 | `	)` |
|        2 |  1625 |  |
|        - |  1626 | `	ph7_value *pStack;` |
|        - |  1627 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1628 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1629 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1630 | `  ** on the maximum stack depth required.` |
|        - |  1631 | `  **` |
|        - |  1632 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1633 | `  */` |
|    42236 |  1634 | `	nInstr += VM_STACK_GUARD;` |
|    42236 |  1635 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    42236 |  1636 | `	if( pStack == 0 ){` |
|      ! 0 |  1637 | `		return 0;` |
|        - |  1638 | `	}` |
|        - |  1639 | `	/* Initialize the operand stack */` |
|  2912086 |  1640 | `	while( nInstr > 0 ){` |
|  2869852 |  1641 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2869852 |  1642 | `		--nInstr;` |
|        2 |  1643 | `	}` |
|        - |  1644 | `	/* Ready for bytecode execution */` |
|    42236 |  1645 | `	return pStack;` |
|    21119 |  1646 |  |
|        - |  1647 | `/* Forward declaration */` |
|        - |  1648 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1649 | `/*` |
|        - |  1650 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1651 | ` * This routine gets called by the PH7 engine after` |
|        - |  1652 | ` * successful compilation of the target PHP program.` |
|        - |  1653 | ` */` |
|     2678 |  1654 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1655 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1656 | `	)` |
|        2 |  1657 |  |
|        - |  1658 | `	SyHashEntry *pEntry;` |
|        - |  1659 | `	sxi32 rc;` |
|     2680 |  1660 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1661 | `		/* Initialize your VM first */` |
|      ! 0 |  1662 | `		return SXERR_CORRUPT;` |
|        - |  1663 | `	}` |
|        - |  1664 | `	/* Mark the VM ready for byte-code execution */` |
|     2680 |  1665 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1666 | `	/* Release the code generator now we have compiled our program */` |
|     2680 |  1667 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1668 | `	/* Emit the DONE instruction */` |
|     2680 |  1669 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2680 |  1670 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1671 | `		return SXERR_MEM;` |
|        - |  1672 | `	}` |
|        - |  1673 | `	/* Script return value */` |
|     2680 |  1674 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1675 | `	/* Allocate a new operand stack */` |
|     2680 |  1676 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2680 |  1677 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1678 | `		return SXERR_MEM;` |
|        - |  1679 | `	}` |
|        - |  1680 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1681 | `	 * private data. */` |
|     2680 |  1682 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2680 |  1683 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1684 | `	/* Allocate the reference table */` |
|     2680 |  1685 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2680 |  1686 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2680 |  1687 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1688 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1689 | `		return SXERR_MEM;` |
|        - |  1690 | `	}` |
|        - |  1691 | `	/* Zero the reference table */` |
|     2680 |  1692 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1693 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2680 |  1694 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2680 |  1695 | `	if( rc != SXRET_OK ){` |
|        - |  1696 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1697 | `		return rc;` |
|        - |  1698 | `	}` |
|        - |  1699 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2680 |  1700 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2680 |  1701 | `	if( rc != SXRET_OK ){` |
|        - |  1702 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1703 | `		return rc;` |
|        - |  1704 | `	}` |
|        - |  1705 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2680 |  1706 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1707 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2680 |  1708 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1709 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2680 |  1710 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1711 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1712 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2680 |  1713 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2680 |  1714 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1715 | `#endif` |
|        - |  1716 | `	/* Initialize and install static and constants class attributes */` |
|     2680 |  1717 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    51160 |  1718 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    48482 |  1719 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    48482 |  1720 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1721 | `			return rc;` |
|        - |  1722 | `		}` |
|        2 |  1723 | `	}` |
|        - |  1724 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2680 |  1725 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1726 | `	/* VM is ready for bytecode execution */` |
|     2680 |  1727 | `	return SXRET_OK;` |
|     1341 |  1728 |  |
|        - |  1729 | `/*` |
|        - |  1730 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1731 | ` */` |
|      ! 0 |  1732 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1733 |  |
|      ! 0 |  1734 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1735 | `		return SXERR_CORRUPT;` |
|        - |  1736 | `	}` |
|        - |  1737 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1738 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1739 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1740 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1741 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1742 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1743 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1744 | `	pVm->bHttpContext = 0;` |
|        - |  1745 | `	/* Set the ready flag */` |
|      ! 0 |  1746 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1747 | `	return SXRET_OK;` |
|      ! 0 |  1748 |  |
|        - |  1749 | `/*` |
|        - |  1750 | ` * Release a Virtual Machine.` |
|        - |  1751 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1752 | ` */` |
|     2670 |  1753 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1754 |  |
|        - |  1755 | `	/* Set the stale magic number */` |
|     2672 |  1756 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1757 | `	/* Release the private memory subsystem */` |
|     2672 |  1758 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2672 |  1759 | `	return SXRET_OK;` |
|        2 |  1760 |  |
|        - |  1761 | `/*` |
|        - |  1762 | ` * Initialize a foreign function call context.` |
|        - |  1763 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1764 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1765 | ` * functions.` |
|        - |  1766 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1767 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1768 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1769 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1770 | ` */` |
|   671642 |  1771 | `static sxi32 VmInitCallContext(` |
|        - |  1772 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1773 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1774 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1775 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1776 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1777 | `	)` |
|        2 |  1778 |  |
|   671644 |  1779 | `	pOut->pFunc = pFunc;` |
|   671644 |  1780 | `	pOut->pVm   = pVm;` |
|   671644 |  1781 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   671644 |  1782 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1783 | `	/* Assume a null return value */` |
|   671644 |  1784 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   671644 |  1785 | `	pOut->pRet = pRet;` |
|   671644 |  1786 | `	pOut->iFlags = iFlags;` |
|   671644 |  1787 | `	return SXRET_OK;` |
|        2 |  1788 |  |
|        - |  1789 | `/*` |
|        - |  1790 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1791 | ` * left behind.` |
|        - |  1792 | ` */` |
|   671642 |  1793 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1794 |  |
|        - |  1795 | `	sxu32 n;` |
|   671644 |  1796 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     8228 |  1797 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    23950 |  1798 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    15724 |  1799 | `			if( apObj[n] == 0 ){` |
|        - |  1800 | `				/* Already released */` |
|      318 |  1801 | `				continue;` |
|        - |  1802 | `			}` |
|    15408 |  1803 | `			PH7_MemObjRelease(apObj[n]);` |
|    15408 |  1804 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     7705 |  1805 | `		}` |
|     8228 |  1806 | `		SySetRelease(&pCtx->sVar);` |
|     4113 |  1807 | `	}` |
|   671644 |  1808 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1809 | `		ph7_aux_data *aAux;` |
|        - |  1810 | `		void *pChunk;` |
|        - |  1811 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1812 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1813 | `		 */` |
|        9 |  1814 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1815 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1816 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1817 | `			/* Release the chunk */` |
|       25 |  1818 | `			if( pChunk ){` |
|       25 |  1819 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1820 | `			}` |
|       13 |  1821 | `		}` |
|        9 |  1822 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1823 | `	}` |
|   671644 |  1824 |  |
|        - |  1825 | `/*` |
|        - |  1826 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1827 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1828 | ` */` |
|      316 |  1829 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1830 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1831 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1832 | `	)` |
|        2 |  1833 |  |
|      318 |  1834 | `	if( pValue == 0 ){` |
|        - |  1835 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1836 | `		return;` |
|        - |  1837 | `	}` |
|      318 |  1838 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      318 |  1839 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1840 | `		sxu32 n;` |
|     1116 |  1841 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1116 |  1842 | `			if( apObj[n] == pValue ){` |
|      318 |  1843 | `				PH7_MemObjRelease(pValue);` |
|      318 |  1844 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1845 | `				/* Mark as released */` |
|      318 |  1846 | `				apObj[n] = 0;` |
|      318 |  1847 | `				break;` |
|        - |  1848 | `			}` |
|      401 |  1849 | `		}` |
|      158 |  1850 | `	}` |
|      160 |  1851 |  |
|        - |  1852 | `/*` |
|        - |  1853 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1854 | ` */` |
|  3834412 |  1855 | `static void VmPopOperand(` |
|        - |  1856 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1857 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1858 | `	)` |
|        2 |  1859 |  |
|  3834414 |  1860 | `	ph7_value *pTos = *ppTos;` |
|  8161784 |  1861 | `	while( nPop > 0 ){` |
|  4327372 |  1862 | `		PH7_MemObjRelease(pTos);` |
|  4327372 |  1863 | `		pTos--;` |
|  4327372 |  1864 | `		nPop--;` |
|        2 |  1865 | `	}` |
|        - |  1866 | `	/* Top of the stack */` |
|  3834414 |  1867 | `	*ppTos = pTos;` |
|  3834414 |  1868 |  |
|        - |  1869 | `/*` |
|        - |  1870 | ` * Reserve a memory object.` |
|        - |  1871 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1872 | ` */` |
|  3160814 |  1873 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1874 |  |
|  3160816 |  1875 | `	ph7_value *pObj = 0;` |
|        - |  1876 | `	VmSlot *pSlot;` |
|        - |  1877 | `	sxu32 nIdx;` |
|        - |  1878 | `	/* Check for a free slot */` |
|  3160816 |  1879 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3160816 |  1880 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3160816 |  1881 | `	if( pSlot ){` |
|  1011502 |  1882 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  1011502 |  1883 | `		nIdx = pSlot->nIdx;` |
|   505750 |  1884 | `	}` |
|  3160816 |  1885 | `	if( pObj == 0 ){` |
|        - |  1886 | `		/* Reserve a new memory object */` |
|  2149316 |  1887 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2149316 |  1888 | `		if( pObj == 0 ){` |
|      ! 0 |  1889 | `			return 0;` |
|        - |  1890 | `		}` |
|  1074657 |  1891 | `	}` |
|        - |  1892 | `	/* Set a null default value */` |
|  3160816 |  1893 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3160816 |  1894 | `	pObj->nIdx = nIdx;` |
|  3160816 |  1895 | `	return pObj;` |
|  1580409 |  1896 |  |
|        - |  1897 | `/*` |
|        - |  1898 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1899 | ` */` |
|    34304 |  1900 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1901 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1902 | `	const char *zKey,  /* Entry key */` |
|        - |  1903 | `	sxu32 nByte,       /* Key length */` |
|        - |  1904 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1905 | `	)` |
|        2 |  1906 |  |
|        - |  1907 | `	ph7_value sKey;` |
|        - |  1908 | `	sxi32 rc;` |
|    34306 |  1909 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    34306 |  1910 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1911 | `	/* Perform the insertion */` |
|    34306 |  1912 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    34306 |  1913 | `	PH7_MemObjRelease(&sKey);` |
|    34306 |  1914 | `	return rc;` |
|        2 |  1915 |  |
|        - |  1916 | `/*` |
|        - |  1917 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1918 | ` * Return a pointer to the variable value on success.` |
|        - |  1919 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1920 | ` */` |
|  3567894 |  1921 | `static ph7_value * VmExtractMemObj(` |
|        - |  1922 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1923 | `	const SyString *pName, /* Variable name */` |
|        - |  1924 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1925 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1926 | `	)` |
|        2 |  1927 |  |
|  3567896 |  1928 | `	int bNullify = FALSE;` |
|        - |  1929 | `	SyHashEntry *pEntry;` |
|        - |  1930 | `	VmFrame *pFrame;` |
|        - |  1931 | `	ph7_value *pObj;` |
|        - |  1932 | `	sxu32 nIdx;` |
|        - |  1933 | `	sxi32 rc;` |
|        - |  1934 | `	/* Point to the top active frame */` |
|  3567896 |  1935 | `	pFrame = pVm->pFrame;` |
|  3567896 |  1936 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1937 | `	/* Perform the lookup */` |
|  3567896 |  1938 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1939 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1940 | `		pName = &sAnnon;` |
|        - |  1941 | `		/* Always nullify the object */` |
|      ! 0 |  1942 | `		bNullify = TRUE;` |
|      ! 0 |  1943 | `		bDup = FALSE;` |
|      ! 0 |  1944 | `	}` |
|        - |  1945 | `	/* Check the superglobals table first */` |
|  3567896 |  1946 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3567896 |  1947 | `	if( pEntry == 0 ){` |
|        - |  1948 | `		/* Query the top active frame */` |
|  3567856 |  1949 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3567856 |  1950 | `		if( pEntry == 0 ){` |
|   108300 |  1951 | `			char *zName = (char *)pName->zString;` |
|        - |  1952 | `			VmSlot sLocal;` |
|   108300 |  1953 | `			if( !bCreate ){` |
|        - |  1954 | `				/* Do not create the variable,return NULL instead */` |
|      122 |  1955 | `				return 0;` |
|        - |  1956 | `			}` |
|        - |  1957 | `			/* No such variable,automatically create a new one and install` |
|        - |  1958 | `			 * it in the current frame.` |
|        - |  1959 | `			 */` |
|   108180 |  1960 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   108180 |  1961 | `			if( pObj == 0 ){` |
|      ! 0 |  1962 | `				return 0;` |
|        - |  1963 | `			}` |
|   108180 |  1964 | `			nIdx = pObj->nIdx;` |
|   108180 |  1965 | `			if( bDup ){` |
|        - |  1966 | `				/* Duplicate name */` |
|      196 |  1967 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      196 |  1968 | `				if( zName == 0 ){` |
|      ! 0 |  1969 | `					return 0;` |
|        - |  1970 | `				}` |
|       97 |  1971 | `			}` |
|        - |  1972 | `			/* Link to the top active VM frame */` |
|   108180 |  1973 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   108180 |  1974 | `			if( rc != SXRET_OK ){` |
|        - |  1975 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1976 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1977 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1978 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1979 | `				return 0;` |
|        - |  1980 | `			}` |
|   108180 |  1981 | `			if( pFrame->pParent != 0 ){` |
|        - |  1982 | `				/* Local variable */` |
|   100684 |  1983 | `				sLocal.nIdx = nIdx;` |
|   100684 |  1984 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    50343 |  1985 | `			}else{` |
|        - |  1986 | `				/* Register in the $GLOBALS array */` |
|     7498 |  1987 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1988 | `			}` |
|        - |  1989 | `			/* Install in the reference table */` |
|   108180 |  1990 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1991 | `			/* Save object index */` |
|   108180 |  1992 | `			pObj->nIdx = nIdx;` |
|    54091 |  1993 | `		}else{` |
|        - |  1994 | `			/* Extract variable contents */` |
|  3459558 |  1995 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3459558 |  1996 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3459558 |  1997 | `			if( bNullify && pObj ){` |
|      ! 0 |  1998 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1999 | `			}` |
|        - |  2000 | `		}` |
|  1783979 |  2001 | `	}else{` |
|        - |  2002 | `		/* Superglobal */` |
|       42 |  2003 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  2004 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  2005 | `	}` |
|  3567776 |  2006 | `	return pObj;` |
|  1784059 |  2007 |  |
|        - |  2008 | `/*` |
|        - |  2009 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  2010 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  2011 | ` */` |
|     2982 |  2012 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  2013 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  2014 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  2015 | `	sxu32 nByte        /* zName length */` |
|        - |  2016 | `	)` |
|        2 |  2017 |  |
|        - |  2018 | `	SyHashEntry *pEntry;` |
|        - |  2019 | `	ph7_value *pValue;` |
|        - |  2020 | `	sxu32 nIdx;` |
|        - |  2021 | `	/* Query the superglobal table */` |
|     2984 |  2022 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2984 |  2023 | `	if( pEntry == 0 ){` |
|        - |  2024 | `		/* No such entry */` |
|      ! 0 |  2025 | `		return 0;` |
|        - |  2026 | `	}` |
|        - |  2027 | `	/* Extract the superglobal index in the global object pool */` |
|     2984 |  2028 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2029 | `	/* Extract the variable value  */` |
|     2984 |  2030 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2984 |  2031 | `	return pValue;` |
|     1493 |  2032 |  |
|        - |  2033 | `/*` |
|        - |  2034 | ` * Perform a raw hashmap insertion.` |
|        - |  2035 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  2036 | ` */` |
|     3012 |  2037 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  2038 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  2039 | `	const char *zKey,   /* Entry key */` |
|        - |  2040 | `	int nKeylen,        /* zKey length*/` |
|        - |  2041 | `	const char *zData,  /* Entry data */` |
|        - |  2042 | `	int nLen            /* zData length */` |
|        - |  2043 | `	)` |
|        2 |  2044 |  |
|        - |  2045 | `	ph7_value sKey,sValue;` |
|        - |  2046 | `	sxi32 rc;` |
|     3014 |  2047 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     3014 |  2048 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     3014 |  2049 | `	if( zKey ){` |
|     2992 |  2050 | `		if( nKeylen < 0 ){` |
|     2940 |  2051 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1469 |  2052 | `		}` |
|     2992 |  2053 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1495 |  2054 | `	}` |
|     3014 |  2055 | `	if( zData ){` |
|     3014 |  2056 | `		if( nLen < 0 ){` |
|        - |  2057 | `			/* Compute length automatically */` |
|      144 |  2058 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  2059 | `		}` |
|     3014 |  2060 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1506 |  2061 | `	}` |
|        - |  2062 | `	/* Perform the insertion */` |
|     3014 |  2063 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     3014 |  2064 | `	PH7_MemObjRelease(&sKey);` |
|     3014 |  2065 | `	PH7_MemObjRelease(&sValue);` |
|     3014 |  2066 | `	return rc;` |
|        2 |  2067 |  |
|        - |  2068 | `/*` |
|        - |  2069 | ` * Configure a working virtual machine instance.` |
|        - |  2070 | ` *` |
|        - |  2071 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  2072 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  2073 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  2074 | ` * The second argument to this function is an integer configuration option` |
|        - |  2075 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  2076 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  2077 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  2078 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  2079 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  2080 | ` */` |
|    43178 |  2081 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  2082 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  2083 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  2084 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  2085 | `	)` |
|        2 |  2086 |  |
|    43180 |  2087 | `	sxi32 rc = SXRET_OK;` |
|    43180 |  2088 | `	switch(nOp){` |
|     1331 |  2089 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2664 |  2090 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2664 |  2091 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  2092 | `		/* VM output consumer callback */` |
|        - |  2093 | `#ifdef UNTRUST` |
|        - |  2094 | `		if( xConsumer == 0 ){` |
|        - |  2095 | `			rc = SXERR_CORRUPT;` |
|        - |  2096 | `			break;` |
|        - |  2097 | `		}` |
|        - |  2098 | `#endif` |
|        - |  2099 | `		/* Install the output consumer */` |
|     2664 |  2100 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2664 |  2101 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2664 |  2102 | `		break;` |
|        - |  2103 | `							   }` |
|     1339 |  2104 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2105 | `		/* Import path */` |
|        - |  2106 | `		  const char *zPath;` |
|        - |  2107 | `		  SyString sPath;` |
|     2680 |  2108 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2109 | `#if defined(UNTRUST)` |
|        - |  2110 | `		  if( zPath == 0 ){` |
|        - |  2111 | `			  rc = SXERR_EMPTY;` |
|        - |  2112 | `			  break;` |
|        - |  2113 | `		  }` |
|        - |  2114 | `#endif` |
|     2680 |  2115 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2116 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2117 | `#ifdef __WINNT__` |
|        2 |  2118 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2119 | `#endif` |
|     5358 |  2120 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2121 | `		  /* Remove leading and trailing white spaces */` |
|     2680 |  2122 | `		  SyStringFullTrim(&sPath);` |
|     2680 |  2123 | `		  if( sPath.nByte > 0 ){` |
|        - |  2124 | `			  /* Store the path in the corresponding conatiner */` |
|     2680 |  2125 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1339 |  2126 | `		  }` |
|     2680 |  2127 | `		  break;` |
|        - |  2128 | `									 }` |
|     1339 |  2129 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2130 | `		/* Run-Time Error report */` |
|     2680 |  2131 | `		pVm->bErrReport = 1;` |
|     2680 |  2132 | `		break;` |
|      ! 0 |  2133 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2134 | `		/* Recursion depth */` |
|      ! 0 |  2135 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2136 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2137 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2138 | `		}` |
|      ! 0 |  2139 | `		break;` |
|        - |  2140 | `									   }` |
|      ! 0 |  2141 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2142 | `		/* VM output length in bytes */` |
|      ! 0 |  2143 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2144 | `#ifdef UNTRUST` |
|        - |  2145 | `		if( pOut == 0 ){` |
|        - |  2146 | `			rc = SXERR_CORRUPT;` |
|        - |  2147 | `			break;` |
|        - |  2148 | `		}` |
|        - |  2149 | `#endif` |
|      ! 0 |  2150 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2151 | `		break;` |
|        - |  2152 | `							   }` |
|        - |  2153 |  |
|    13390 |  2154 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2155 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2156 | `		/* Create a new superglobal/global variable */` |
|    26782 |  2157 | `		const char *zName = va_arg(ap,const char *);` |
|    26782 |  2158 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2159 | `		SyHashEntry *pEntry;` |
|        - |  2160 | `		ph7_value *pObj;` |
|        - |  2161 | `		sxu32 nByte;` |
|        - |  2162 | `		sxu32 nIdx;` |
|        - |  2163 | `#ifdef UNTRUST` |
|        - |  2164 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2165 | `			rc = SXERR_CORRUPT;` |
|        - |  2166 | `			break;` |
|        - |  2167 | `		}` |
|        - |  2168 | `#endif` |
|    26782 |  2169 | `		nByte = SyStrlen(zName);` |
|    26782 |  2170 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2171 | `			/* Check if the superglobal is already installed */` |
|    26782 |  2172 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    13392 |  2173 | `		}else{` |
|        - |  2174 | `			/* Query the top active VM frame */` |
|      ! 0 |  2175 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2176 | `		}` |
|    26782 |  2177 | `		if( pEntry ){` |
|        - |  2178 | `			/* Variable already installed */` |
|      ! 0 |  2179 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2180 | `			/* Extract contents */` |
|      ! 0 |  2181 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2182 | `			if( pObj ){` |
|        - |  2183 | `				/* Overwrite old contents */` |
|      ! 0 |  2184 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2185 | `			}` |
|      ! 0 |  2186 | `		}else{` |
|        - |  2187 | `			/* Install a new variable */` |
|    26782 |  2188 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    26782 |  2189 | `			if( pObj == 0 ){` |
|      ! 0 |  2190 | `				rc = SXERR_MEM;` |
|      ! 0 |  2191 | `				break;` |
|        - |  2192 | `			}` |
|    26782 |  2193 | `			nIdx = pObj->nIdx;` |
|        - |  2194 | `			/* Copy value */` |
|    26782 |  2195 | `			PH7_MemObjStore(pValue,pObj);` |
|    26782 |  2196 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2197 | `				/* Install the superglobal */` |
|    26782 |  2198 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    13392 |  2199 | `			}else{` |
|        - |  2200 | `				/* Install in the current frame */` |
|      ! 0 |  2201 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2202 | `			}` |
|    26782 |  2203 | `			if( rc == SXRET_OK ){` |
|        - |  2204 | `				SyHashEntry *pRef;` |
|    26782 |  2205 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    26782 |  2206 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    13392 |  2207 | `				}else{` |
|      ! 0 |  2208 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2209 | `				}` |
|        - |  2210 | `				/* Install in the reference table */` |
|    26782 |  2211 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    26782 |  2212 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2213 | `					/* Register in the $GLOBALS array */` |
|    26782 |  2214 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    13390 |  2215 | `				}` |
|    13390 |  2216 | `			}` |
|        - |  2217 | `		}` |
|    26782 |  2218 | `		break;` |
|        - |  2219 | `									}` |
|     1469 |  2220 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2221 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2222 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2223 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2224 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2225 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2226 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2940 |  2227 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2940 |  2228 | `		const char *zValue = va_arg(ap,const char *);` |
|     2940 |  2229 | `		int nLen = va_arg(ap,int);` |
|        - |  2230 | `		ph7_hashmap *pMap;` |
|        - |  2231 | `		ph7_value *pValue;` |
|     2940 |  2232 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2233 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2234 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2939 |  2235 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2236 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2237 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2938 |  2238 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2239 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2240 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2938 |  2241 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2242 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2243 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2938 |  2244 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2245 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2246 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2938 |  2247 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2248 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2249 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2250 | `		}else{` |
|        - |  2251 | `			/* Extract the $_SERVER superglobal */` |
|     2938 |  2252 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2253 | `		}` |
|     2940 |  2254 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2255 | `			/* No such entry */` |
|      ! 0 |  2256 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2257 | `			break;` |
|        - |  2258 | `		}` |
|        - |  2259 | `		/* Point to the hashmap */` |
|     2940 |  2260 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2261 | `		/* Perform the insertion */` |
|     2940 |  2262 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2940 |  2263 | `		break;` |
|        - |  2264 | `								   }` |
|       11 |  2265 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2266 | `		/* Script arguments */` |
|       24 |  2267 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2268 | `		ph7_hashmap *pMap;` |
|        - |  2269 | `		ph7_value *pValue;` |
|        - |  2270 | `		sxu32 n;` |
|       24 |  2271 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2272 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2273 | `			break;` |
|        - |  2274 | `		}` |
|        - |  2275 | `		/* Extract the $argv array */` |
|       24 |  2276 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2277 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2278 | `			/* No such entry */` |
|      ! 0 |  2279 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2280 | `			break;` |
|        - |  2281 | `		}` |
|        - |  2282 | `		/* Point to the hashmap */` |
|       24 |  2283 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2284 | `		/* Perform the insertion */` |
|       24 |  2285 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2286 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2287 | `		if( rc == SXRET_OK ){` |
|       24 |  2288 | `			if( pMap->nEntry > 1 ){` |
|        - |  2289 | `				/* Append space separator first */` |
|       18 |  2290 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2291 | `			}` |
|       24 |  2292 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2293 | `		}` |
|       24 |  2294 | `		break;` |
|        - |  2295 | `								  }` |
|      ! 0 |  2296 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2297 | `		/* error_log() consumer */` |
|      ! 0 |  2298 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2299 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2300 | `		break;` |
|        - |  2301 | `										}` |
|      ! 0 |  2302 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2303 | `		/* Script return value */` |
|      ! 0 |  2304 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2305 | `#ifdef UNTRUST` |
|        - |  2306 | `		if( ppValue == 0 ){` |
|        - |  2307 | `			rc = SXERR_CORRUPT;` |
|        - |  2308 | `			break;` |
|        - |  2309 | `		}` |
|        - |  2310 | `#endif` |
|      ! 0 |  2311 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2312 | `		break;` |
|        - |  2313 | `								   }` |
|     2678 |  2314 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2315 | `		/* Register an IO stream device */` |
|     5358 |  2316 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2317 | `		/* Make sure we are dealing with a valid IO stream */` |
|     8034 |  2318 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5358 |  2319 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2320 | `				/* Invalid stream */` |
|      ! 0 |  2321 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2322 | `				break;` |
|        - |  2323 | `		}` |
|     5358 |  2324 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2325 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2680 |  2326 | `			pVm->pDefStream = pStream;` |
|     1339 |  2327 | `		}` |
|        - |  2328 | `		/* Insert in the appropriate container */` |
|     5358 |  2329 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5358 |  2330 | `		break;` |
|        - |  2331 | `								  }` |
|        8 |  2332 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2333 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2334 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2335 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2336 | `#ifdef UNTRUST` |
|        - |  2337 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2338 | `			rc = SXERR_CORRUPT;` |
|        - |  2339 | `			break;` |
|        - |  2340 | `		}` |
|        - |  2341 | `#endif` |
|       16 |  2342 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2343 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2344 | `		break;` |
|        - |  2345 | `									   }` |
|        8 |  2346 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2347 | `		/* Raw HTTP request*/` |
|       16 |  2348 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2349 | `		int nByte = va_arg(ap,int);` |
|       16 |  2350 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2351 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2352 | `			break;` |
|        - |  2353 | `		}` |
|       16 |  2354 | `		if( nByte < 0 ){` |
|        - |  2355 | `			/* Compute length automatically */` |
|      ! 0 |  2356 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2357 | `		}` |
|        - |  2358 | `		/* Process the request */` |
|       16 |  2359 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2360 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2361 | `		if( rc == SXRET_OK ){` |
|       16 |  2362 | `			pVm->bHttpContext = 1;` |
|        8 |  2363 | `		}` |
|       16 |  2364 | `		break;` |
|        - |  2365 | `									}` |
|        8 |  2366 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2367 | `		/* Extract HTTP response status code */` |
|       16 |  2368 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2369 | `		if( pStatus ){` |
|       16 |  2370 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2371 | `		}` |
|       16 |  2372 | `		break;` |
|        - |  2373 | `										}` |
|        8 |  2374 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2375 | `		/* Iterate response headers via callback */` |
|        - |  2376 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2377 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2378 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2379 | `		if( xCallback ){` |
|       16 |  2380 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2381 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2382 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2383 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2384 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2385 | `							   pUserData);` |
|       12 |  2386 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2387 | `					break;` |
|        - |  2388 | `				}` |
|        6 |  2389 | `			}` |
|        8 |  2390 | `		}` |
|       16 |  2391 | `		break;` |
|        - |  2392 | `										 }` |
|      ! 0 |  2393 | `	default:` |
|        - |  2394 | `		/* Unknown configuration option */` |
|      ! 0 |  2395 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2396 | `		break;` |
|        - |  2397 | `	}` |
|    43180 |  2398 | `	return rc;` |
|        2 |  2399 |  |
|        - |  2400 | `/* Forward declaration */` |
|        - |  2401 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2402 | `/*` |
|        - |  2403 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2404 | ` * format.` |
|        - |  2405 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2406 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2407 | ` * (STDOUT).` |
|        - |  2408 | ` */` |
|        2 |  2409 | `static sxi32 VmByteCodeDump(` |
|        - |  2410 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2411 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2412 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2413 | `	)` |
|        1 |  2414 |  |
|        - |  2415 | `	static const char zDump[] = {` |
|        - |  2416 | `		"====================================================\n"` |
|        - |  2417 | `		"PH7 VM Dump\n"` |
|        - |  2418 | `		"====================================================\n"` |
|        - |  2419 | `	};` |
|        - |  2420 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2421 | `	sxi32 rc = SXRET_OK;` |
|        - |  2422 | `	sxu32 n;` |
|        - |  2423 | `	/* Point to the PH7 instructions */` |
|        3 |  2424 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2425 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2426 | `	n = 0;` |
|        3 |  2427 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2428 | `	/* Dump instructions */` |
|        7 |  2429 | `	for(;;){` |
|       15 |  2430 | `		if( pInstr >= pEnd ){` |
|        - |  2431 | `			/* No more instructions */` |
|        3 |  2432 | `			break;` |
|        - |  2433 | `		}` |
|        - |  2434 | `		/* Format and call the consumer callback */` |
|       19 |  2435 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2436 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2437 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2438 | `		if( rc != SXRET_OK ){` |
|        - |  2439 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2440 | `			return rc;` |
|        - |  2441 | `		}` |
|       13 |  2442 | `		++n;` |
|       13 |  2443 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2444 | `	}` |
|        3 |  2445 | `	return rc;` |
|        2 |  2446 |  |
|        - |  2447 | `/* Forward declaration */` |
|        - |  2448 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2449 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2450 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2451 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2452 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2453 | `/*` |
|        - |  2454 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2455 | ` * consumer callback.` |
|        - |  2456 | ` */` |
|      598 |  2457 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2458 |  |
|      599 |  2459 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      599 |  2460 | `	sxi32 rc = SXRET_OK;` |
|        - |  2461 | `	/* Append a new line */` |
|        - |  2462 | `#ifdef __WINNT__` |
|        1 |  2463 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2464 | `#else` |
|      598 |  2465 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2466 | `#endif` |
|        - |  2467 | `	/* Invoke the output consumer callback */` |
|      599 |  2468 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      599 |  2469 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      599 |  2470 | `	return rc;` |
|        1 |  2471 |  |
|        - |  2472 | `/*` |
|        - |  2473 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2474 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2475 | ` * information.` |
|        - |  2476 | ` */` |
|      148 |  2477 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2478 |  |
|      150 |  2479 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2480 | `		ph7_value apArg[4];` |
|        - |  2481 | `		ph7_value *apArgPtr[4];` |
|        - |  2482 | `		ph7_value sResult;` |
|        - |  2483 | `		SyString sErr;` |
|        - |  2484 | `		/* Prepare arguments */` |
|       76 |  2485 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2486 | `			/* use explicit message length to avoid reading past buffer */` |
|       76 |  2487 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       76 |  2488 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       76 |  2489 | `		if( pFile ){` |
|       76 |  2490 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       76 |  2491 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       39 |  2492 | `		}else{` |
|      ! 0 |  2493 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2494 | `		}` |
|       76 |  2495 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       76 |  2496 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2497 | `		/* Set up pointer array */` |
|       76 |  2498 | `		apArgPtr[0] = &apArg[0];` |
|       76 |  2499 | `		apArgPtr[1] = &apArg[1];` |
|       76 |  2500 | `		apArgPtr[2] = &apArg[2];` |
|       76 |  2501 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2502 | `		/* Call the handler */` |
|       76 |  2503 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2504 | `		/* Check return value */` |
|       76 |  2505 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2506 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2507 | `		}` |
|        - |  2508 | `		/* Release */` |
|       76 |  2509 | `		PH7_MemObjRelease(&apArg[0]);` |
|       76 |  2510 | `		PH7_MemObjRelease(&apArg[1]);` |
|       76 |  2511 | `		PH7_MemObjRelease(&apArg[2]);` |
|       76 |  2512 | `		PH7_MemObjRelease(&apArg[3]);` |
|       76 |  2513 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2514 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2515 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       76 |  2516 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2517 | `	}` |
|        - |  2518 | `	/* No handler, always call error handler */` |
|       75 |  2519 | `	return TRUE;` |
|       76 |  2520 |  |
|      110 |  2521 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2522 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2523 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2524 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2525 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2526 | `	)` |
|        2 |  2527 |  |
|      112 |  2528 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2529 | `	SyString *pFile;` |
|        - |  2530 | `	char *zErr;` |
|      112 |  2531 | `	sxi32 rc = SXRET_OK;` |
|      112 |  2532 | `	if( !pVm->bErrReport ){` |
|        - |  2533 | `		/* Don't bother reporting errors */` |
|        3 |  2534 | `		return SXRET_OK;` |
|        - |  2535 | `	}` |
|        - |  2536 | `	/* Reset the working buffer */` |
|      110 |  2537 | `	SyBlobReset(pWorker);` |
|        - |  2538 | `	/* Peek the processed file if available */` |
|      110 |  2539 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      110 |  2540 | `	if( pFile ){` |
|        - |  2541 | `		/* Append file name */` |
|      110 |  2542 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|      110 |  2543 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       54 |  2544 | `	}` |
|        - |  2545 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2546 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2547 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2548 | `	 * E_DEPRECATED). */` |
|      110 |  2549 | `	zErr = "Error:  ";` |
|      110 |  2550 | `	switch(iErr){` |
|       19 |  2551 | `	case PH7_CTX_WARNING:` |
|       40 |  2552 | `		zErr = "Warning:  ";` |
|       40 |  2553 | `		break;` |
|        6 |  2554 | `	case PH7_CTX_NOTICE:` |
|       14 |  2555 | `		zErr = "Notice:  ";` |
|       12 |  2556 | `		break;` |
|       29 |  2557 | `	default:` |
|        - |  2558 | `		/* keep iErr unchanged */` |
|       58 |  2559 | `		break;` |
|        - |  2560 | `	}` |
|      110 |  2561 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|      110 |  2562 | `	if( pFuncName ){` |
|        - |  2563 | `		/* Append function name first */` |
|       23 |  2564 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2565 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2566 | `	}` |
|      110 |  2567 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2568 | `	/* Check for user error handler.  compute length of C string */` |
|      110 |  2569 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2570 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2571 | `	}` |
|      110 |  2572 | `	return rc;` |
|       57 |  2573 |  |
|        - |  2574 | `/*` |
|        - |  2575 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2576 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2577 | ` * information.` |
|        - |  2578 | ` */` |
|       40 |  2579 | `static sxi32 VmThrowErrorAp(` |
|        - |  2580 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2581 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2582 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2583 | `	const char *zFormat, /* Format message */` |
|        - |  2584 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2585 | `	)` |
|        2 |  2586 |  |
|       42 |  2587 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2588 | `	SyBlob sMsg;` |
|        - |  2589 | `	SyString *pFile;` |
|        - |  2590 | `	char *zErr;` |
|       42 |  2591 | `	sxi32 rc = SXRET_OK;` |
|       42 |  2592 | `	if( !pVm->bErrReport ){` |
|        - |  2593 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2594 | `		return SXRET_OK;` |
|        - |  2595 | `	}` |
|        - |  2596 | `	/* Reset the working buffer */` |
|       42 |  2597 | `	SyBlobReset(pWorker);` |
|        - |  2598 | `	/* Peek the processed file if available */` |
|       42 |  2599 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       42 |  2600 | `	if( pFile ){` |
|        - |  2601 | `		/* Append file name */` |
|       42 |  2602 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       42 |  2603 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       20 |  2604 | `	}` |
|        - |  2605 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2606 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2607 | `	 * the correct errno value. */` |
|       42 |  2608 | `	zErr = "Error:  ";` |
|       42 |  2609 | `	switch(iErr){` |
|        4 |  2610 | `	case PH7_CTX_WARNING:` |
|        9 |  2611 | `		zErr = "Warning:  ";` |
|        9 |  2612 | `		break;` |
|        3 |  2613 | `	case PH7_CTX_NOTICE:` |
|        7 |  2614 | `		zErr = "Notice:  ";` |
|        6 |  2615 | `		break;` |
|       13 |  2616 | `	default:` |
|        - |  2617 | `		/* do not change iErr */` |
|       26 |  2618 | `		break;` |
|        - |  2619 | `	}` |
|       42 |  2620 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       42 |  2621 | `	if( pFuncName ){` |
|        - |  2622 | `		/* Append function name first */` |
|       26 |  2623 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2624 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2625 | `	}` |
|        - |  2626 | `	/* Format the raw message */` |
|       42 |  2627 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       42 |  2628 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2629 | `	/* Check if a user error handler is installed */` |
|       42 |  2630 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2631 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2632 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2633 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2634 | `	}` |
|       42 |  2635 | `	SyBlobRelease(&sMsg);` |
|       42 |  2636 | `	return rc;` |
|       22 |  2637 |  |
|        - |  2638 | `/*` |
|        - |  2639 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  2640 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  2641 | ` * possible.` |
|        - |  2642 | ` */` |
|       38 |  2643 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        1 |  2644 |  |
|        - |  2645 | `	ph7_class *pClass;` |
|       39 |  2646 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  2647 | `	ph7_class_instance *pThis;` |
|        - |  2648 | `	ph7_class_method *pCons;` |
|        - |  2649 | `	ph7_value sArg;` |
|        - |  2650 | `	ph7_value *apArg[1];` |
|        - |  2651 | `	SyBlob sMsg;` |
|        - |  2652 | `	SyString sMsgStr;` |
|        - |  2653 | `	VmFrame *pFrame;` |
|        - |  2654 | `	sxi32 rc;` |
|       39 |  2655 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       39 |  2656 | `	if( pClass == 0 ){` |
|      ! 0 |  2657 | `		return PH7_ABORT;` |
|        - |  2658 | `	}` |
|       39 |  2659 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       39 |  2660 | `	if( pThis == 0 ){` |
|      ! 0 |  2661 | `		return PH7_ABORT;` |
|        - |  2662 | `	}` |
|       39 |  2663 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2664 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  2665 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  2666 | `	{` |
|       39 |  2667 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       39 |  2668 | `		if( pOwner ){` |
|       39 |  2669 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       19 |  2670 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       20 |  2671 | `		}else{` |
|      ! 0 |  2672 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  2673 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  2674 | `		}` |
|        - |  2675 | `	}` |
|       39 |  2676 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       39 |  2677 | `	if( pCons ){` |
|       39 |  2678 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       39 |  2679 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       39 |  2680 | `		apArg[0] = &sArg;` |
|       39 |  2681 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       39 |  2682 | `		PH7_MemObjRelease(&sArg);` |
|       19 |  2683 | `	}` |
|       39 |  2684 | `	SyBlobRelease(&sMsg);` |
|       39 |  2685 | `	pFrame = pVm->pFrame;` |
|       39 |  2686 | `	if( pFrame ){` |
|       39 |  2687 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       39 |  2688 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       19 |  2689 | `	}` |
|       39 |  2690 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       39 |  2691 | `	PH7_ClassInstanceUnref(pThis);` |
|       39 |  2692 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2693 | `		return PH7_ABORT;` |
|        - |  2694 | `	}` |
|       39 |  2695 | `	return PH7_EXCEPTION;` |
|       20 |  2696 |  |
|        - |  2697 |  |
|        - |  2698 | `/*` |
|        - |  2699 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  2700 | ` */` |
|        4 |  2701 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  2702 |  |
|        - |  2703 | `	ph7_class *pErrClass;` |
|        - |  2704 | `	ph7_class_instance *pThis;` |
|        - |  2705 | `	ph7_class_method *pCons;` |
|        - |  2706 | `	ph7_value sArg;` |
|        - |  2707 | `	ph7_value *apArg[1];` |
|        - |  2708 | `	SyBlob sMsg;` |
|        - |  2709 | `	SyString sMsgStr;` |
|        - |  2710 | `	VmFrame *pFrame;` |
|        - |  2711 | `	sxi32 rc;` |
|        5 |  2712 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        5 |  2713 | `	if( pErrClass == 0 ){` |
|      ! 0 |  2714 | `		return PH7_ABORT;` |
|        - |  2715 | `	}` |
|        5 |  2716 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        5 |  2717 | `	if( pThis == 0 ){` |
|      ! 0 |  2718 | `		return PH7_ABORT;` |
|        - |  2719 | `	}` |
|        5 |  2720 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2721 | `	{` |
|        5 |  2722 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        5 |  2723 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        5 |  2724 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  2725 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  2726 | `	}` |
|        5 |  2727 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        5 |  2728 | `	if( pCons ){` |
|        5 |  2729 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        5 |  2730 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        5 |  2731 | `		apArg[0] = &sArg;` |
|        5 |  2732 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        5 |  2733 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  2734 | `	}` |
|        5 |  2735 | `	SyBlobRelease(&sMsg);` |
|        5 |  2736 | `	pFrame = pVm->pFrame;` |
|        5 |  2737 | `	if( pFrame ){` |
|        5 |  2738 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  2739 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  2740 | `	}` |
|        5 |  2741 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        5 |  2742 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  2743 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2744 | `		return PH7_ABORT;` |
|        - |  2745 | `	}` |
|        5 |  2746 | `	return PH7_EXCEPTION;` |
|        3 |  2747 |  |
|        - |  2748 |  |
|        - |  2749 | `/*` |
|        - |  2750 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  2751 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  2752 | ` * For class types, instanceof is verified.` |
|        - |  2753 | ` *` |
|        - |  2754 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  2755 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  2756 | ` */` |
|        - |  2757 | `/*` |
|        - |  2758 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  2759 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  2760 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  2761 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  2762 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  2763 | ` */` |
|       20 |  2764 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  2765 |  |
|        - |  2766 | `	const char *z, *zEnd, *zTail;` |
|        - |  2767 | `	sxu32 n;` |
|        - |  2768 | `	sxu8 bReal;` |
|        - |  2769 | `	sxi32 rc;` |
|       22 |  2770 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2771 | `		return 0;` |
|        - |  2772 | `	}` |
|       22 |  2773 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       22 |  2774 | `	n = SyBlobLength(&pValue->sBlob);` |
|       22 |  2775 | `	zEnd = z + n;` |
|       22 |  2776 | `	if( n == 0 ){` |
|      ! 0 |  2777 | `		return 0;` |
|        - |  2778 | `	}` |
|       22 |  2779 | `	zTail = 0;` |
|       22 |  2780 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       22 |  2781 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        7 |  2782 | `		return 0;` |
|        - |  2783 | `	}` |
|        - |  2784 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       16 |  2785 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  2786 | `		zTail++;` |
|      ! 0 |  2787 | `	}` |
|       16 |  2788 | `	return zTail == zEnd ? 1 : 0;` |
|       12 |  2789 |  |
|        - |  2790 |  |
|        - |  2791 | `/*` |
|        - |  2792 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  2793 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  2794 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  2795 | ` *   0 if it's not strictly numeric.` |
|        - |  2796 | ` */` |
|       16 |  2797 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  2798 |  |
|        - |  2799 | `	const char *z, *zEnd, *zTail;` |
|        - |  2800 | `	sxu32 n;` |
|       18 |  2801 | `	sxu8 bReal = 0;` |
|        - |  2802 | `	sxi32 rc;` |
|       18 |  2803 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2804 | `		return 0;` |
|        - |  2805 | `	}` |
|       18 |  2806 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2807 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2808 | `	zEnd = z + n;` |
|       18 |  2809 | `	if( n == 0 ) return 0;` |
|       18 |  2810 | `	zTail = 0;` |
|       18 |  2811 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2812 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  2813 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  2814 | `	if( zTail != zEnd ) return 0;` |
|       15 |  2815 | `	return bReal ? 2 : 1;` |
|       10 |  2816 |  |
|        - |  2817 |  |
|        - |  2818 | `/*` |
|        - |  2819 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - |  2820 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - |  2821 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - |  2822 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - |  2823 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - |  2824 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - |  2825 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - |  2826 | ` * throw.` |
|        - |  2827 | ` *` |
|        - |  2828 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  2829 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  2830 | ` */` |
|       98 |  2831 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)` |
|        2 |  2832 |  |
|        - |  2833 | `	sxu32 i;` |
|        - |  2834 | `	ph7_type_alt *aAlts;` |
|        - |  2835 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  2836 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|      100 |  2837 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  2838 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  2839 | `	}` |
|       88 |  2840 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|       88 |  2841 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       88 |  2842 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      260 |  2843 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      174 |  2844 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      150 |  2845 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      150 |  2846 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      150 |  2847 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       76 |  2848 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       48 |  2849 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  2850 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       88 |  2851 | `	}` |
|        - |  2852 | `	/* Object handling */` |
|       88 |  2853 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       18 |  2854 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       18 |  2855 | `		if( bHasClassAlt ){` |
|       14 |  2856 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  2857 | `			ph7_class *pSelfNow = 0;` |
|       14 |  2858 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  2859 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  2860 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  2861 | `			}` |
|       26 |  2862 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  2863 | `				ph7_class *pExpected;` |
|        - |  2864 | `				SyString *pCN;` |
|       22 |  2865 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  2866 | `				pCN = &aAlts[i].sClass;` |
|       22 |  2867 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  2868 | `					pExpected = pSelfNow;` |
|       22 |  2869 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  2870 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2871 | `				}else{` |
|       22 |  2872 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  2873 | `				}` |
|       22 |  2874 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  2875 | `					return SXRET_OK;` |
|        - |  2876 | `				}` |
|        8 |  2877 | `			}` |
|        2 |  2878 | `		}` |
|        9 |  2879 | `		return SXERR_INVALID;` |
|        - |  2880 | `	}` |
|        - |  2881 | `	/* Array handling */` |
|       72 |  2882 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  2883 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  2884 | `	}` |
|        - |  2885 | `	/* Scalar handling — exact match first */` |
|       66 |  2886 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       26 |  2887 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  2888 | `	}` |
|       42 |  2889 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  2890 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  2891 | `	}` |
|       38 |  2892 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       38 |  2893 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  2894 | `	}` |
|       18 |  2895 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2896 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  2897 | `	}` |
|       18 |  2898 | `	if( bStrict ){` |
|        - |  2899 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|      ! 0 |  2900 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 |  2901 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 |  2902 | `			return SXRET_OK;` |
|        - |  2903 | `		}` |
|      ! 0 |  2904 | `		return SXERR_INVALID;` |
|        - |  2905 | `	}` |
|        - |  2906 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  2907 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  2908 | `	 * to match PHP's union RFC. */` |
|        - |  2909 | `	{` |
|       18 |  2910 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  2911 | `		if( bHasInt ){` |
|        - |  2912 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  2913 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  2914 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2915 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2916 | `				return SXRET_OK;` |
|        - |  2917 | `			}` |
|       18 |  2918 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  2919 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  2920 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  2921 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2922 | `					return SXRET_OK;` |
|        - |  2923 | `				}` |
|      ! 0 |  2924 | `			}` |
|       18 |  2925 | `			if( kind == 1 ){` |
|        9 |  2926 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  2927 | `				return SXRET_OK;` |
|        - |  2928 | `			}` |
|        4 |  2929 | `		}` |
|       10 |  2930 | `		if( bHasFloat ){` |
|       10 |  2931 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  2932 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  2933 | `				return SXRET_OK;` |
|        - |  2934 | `			}` |
|       10 |  2935 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  2936 | `				PH7_MemObjToReal(pValue);` |
|        7 |  2937 | `				return SXRET_OK;` |
|        - |  2938 | `			}` |
|        1 |  2939 | `		}` |
|        3 |  2940 | `		if( bHasString ){` |
|      ! 0 |  2941 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  2942 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  2943 | `				return SXRET_OK;` |
|        - |  2944 | `			}` |
|      ! 0 |  2945 | `		}` |
|        3 |  2946 | `		if( bHasBool ){` |
|      ! 0 |  2947 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  2948 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  2949 | `				return SXRET_OK;` |
|        - |  2950 | `			}` |
|      ! 0 |  2951 | `		}` |
|        - |  2952 | `	}` |
|        3 |  2953 | `	return SXERR_INVALID;` |
|       51 |  2954 |  |
|        - |  2955 |  |
|        - |  2956 | `/*` |
|        - |  2957 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - |  2958 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - |  2959 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - |  2960 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - |  2961 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - |  2962 | ` */` |
|       34 |  2963 | `static sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        2 |  2964 |  |
|       36 |  2965 | `	if( bStrict ){` |
|        - |  2966 | `		/* Only int -> float widening is allowed implicitly. */` |
|       10 |  2967 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 |  2968 | `			PH7_MemObjToReal(pVal);` |
|        3 |  2969 | `			return SXRET_OK;` |
|        - |  2970 | `		}` |
|        7 |  2971 | `		return SXERR_INVALID;` |
|        - |  2972 | `	}` |
|        - |  2973 | `	{` |
|       28 |  2974 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|       28 |  2975 | `		if( xCast ) xCast(pVal);` |
|        - |  2976 | `	}` |
|       28 |  2977 | `	return SXRET_OK;` |
|       19 |  2978 |  |
|        - |  2979 |  |
|        - |  2980 | `/*` |
|        - |  2981 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - |  2982 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - |  2983 | ` *` |
|        - |  2984 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - |  2985 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - |  2986 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - |  2987 | ` * back to a static literal and ignore zBuf entirely.` |
|        - |  2988 | ` */` |
|        8 |  2989 | `static const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        1 |  2990 |  |
|        9 |  2991 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|        9 |  2992 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|        9 |  2993 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|        9 |  2994 | `		if( pDeclared->zString && nCopy > 0 ){` |
|        9 |  2995 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|        4 |  2996 | `		}` |
|        9 |  2997 | `		zBuf[nCopy] = 0;` |
|        9 |  2998 | `		return zBuf;` |
|        - |  2999 | `	}` |
|      ! 0 |  3000 | `	switch( nType ){` |
|      ! 0 |  3001 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 |  3002 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 |  3003 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 |  3004 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 |  3005 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 |  3006 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 |  3007 | `		default:             return "scalar";` |
|        - |  3008 | `	}` |
|        5 |  3009 |  |
|        - |  3010 |  |
|        - |  3011 | `/*` |
|        - |  3012 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  3013 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  3014 | ` */` |
|       18 |  3015 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        1 |  3016 |  |
|       19 |  3017 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       28 |  3018 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       18 |  3019 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       19 |  3020 | `	return zBuf;` |
|        1 |  3021 |  |
|        - |  3022 |  |
|    13702 |  3023 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  3024 |  |
|        - |  3025 | `	SyHashEntry *pSlot;` |
|        - |  3026 | `	VmClassAttr *pVmAttr;` |
|        - |  3027 | `	ph7_class_attr *pAttr;` |
|    13704 |  3028 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    13704 |  3029 | `	if( pSlot == 0 ){` |
|    13502 |  3030 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  3031 | `	}` |
|      204 |  3032 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      204 |  3033 | `	pAttr = pVmAttr->pAttr;` |
|      204 |  3034 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  3035 | `		return SXRET_OK;` |
|        - |  3036 | `	}` |
|        - |  3037 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|        - |  3038 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|        - |  3039 | `	 * matching PHP's documented behavior. */` |
|      204 |  3040 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       23 |  3041 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  3042 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|        - |  3043 |  |
|       16 |  3044 | `		if( rc == SXRET_OK ){` |
|        9 |  3045 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  3046 | `			return SXRET_OK;` |
|        - |  3047 | `		}` |
|        7 |  3048 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3049 | `			char zBuf[128];` |
|        4 |  3050 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  3051 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3052 | `		}` |
|        5 |  3053 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3054 | `	}` |
|        - |  3055 | `	/* NULL handling: allowed only if the type is nullable. */` |
|      190 |  3056 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  3057 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       12 |  3058 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       12 |  3059 | `			return SXRET_OK;` |
|        - |  3060 | `		}` |
|        3 |  3061 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  3062 | `	}` |
|        - |  3063 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  3064 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  3065 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      178 |  3066 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  3067 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3068 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  3069 | `			return SXRET_OK;` |
|        - |  3070 | `		}` |
|        7 |  3071 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3072 | `	}` |
|      168 |  3073 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  3074 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  3075 | `		 * currently active on the self-stack. */` |
|       26 |  3076 | `		ph7_class *pExpected = 0;` |
|       26 |  3077 | `		SyString *pClassName = &pAttr->sClass;` |
|       26 |  3078 | `		ph7_class *pSelfNow = 0;` |
|       26 |  3079 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        3 |  3080 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        3 |  3081 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        1 |  3082 | `		}` |
|       26 |  3083 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  3084 | `			pExpected = pSelfNow;` |
|       24 |  3085 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3086 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3087 | `		}else{` |
|       22 |  3088 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3089 | `		}` |
|       26 |  3090 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3091 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3092 | `		}` |
|       26 |  3093 | `		if( pExpected ){` |
|       22 |  3094 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       22 |  3095 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  3096 | `				char zBuf[128];` |
|        7 |  3097 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  3098 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3099 | `			}` |
|        8 |  3100 | `		}` |
|       22 |  3101 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       22 |  3102 | `		return SXRET_OK;` |
|        - |  3103 | `	}` |
|        - |  3104 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  3105 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|      144 |  3106 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3107 | `		char zBuf[128];` |
|       10 |  3108 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 |  3109 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3110 | `	}` |
|      138 |  3111 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       26 |  3112 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       26 |  3113 | `		if( xCast ){` |
|        - |  3114 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       26 |  3115 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3116 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3117 | `			}` |
|       24 |  3118 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  3119 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3120 | `			}` |
|        - |  3121 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  3122 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  3123 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       26 |  3124 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       17 |  3125 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       19 |  3126 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|        9 |  3127 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  3128 | `			}` |
|       12 |  3129 | `			xCast(pValue);` |
|        5 |  3130 | `		}` |
|        5 |  3131 | `	}` |
|      124 |  3132 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      124 |  3133 | `	return SXRET_OK;` |
|     6853 |  3134 |  |
|        - |  3135 |  |
|        - |  3136 | `/*` |
|        - |  3137 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3138 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3139 | ` * information.` |
|        - |  3140 | ` * ------------------------------------` |
|        - |  3141 | ` * Simple boring wrapper function.` |
|        - |  3142 | ` * ------------------------------------` |
|        - |  3143 | ` */` |
|       16 |  3144 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  3145 |  |
|        - |  3146 | `	va_list ap;` |
|        - |  3147 | `	sxi32 rc;` |
|       17 |  3148 | `	va_start(ap,zFormat);` |
|       17 |  3149 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       17 |  3150 | `	va_end(ap);` |
|       17 |  3151 | `	return rc;` |
|        1 |  3152 |  |
|        - |  3153 | `/*` |
|        - |  3154 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  3155 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  3156 | ` */` |
|       34 |  3157 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        1 |  3158 |  |
|        - |  3159 | `	ph7_class *pClass;` |
|        - |  3160 | `	ph7_class_instance *pThis;` |
|        - |  3161 | `	ph7_class_method *pCons;` |
|        - |  3162 | `	ph7_value sArg;` |
|        - |  3163 | `	ph7_value *apArg[1];` |
|        - |  3164 | `	SyBlob sMsg;` |
|        - |  3165 | `	SyString sMsgStr;` |
|        - |  3166 | `	VmFrame *pFrame;` |
|        - |  3167 | `	sxi32 rc;` |
|       35 |  3168 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       35 |  3169 | `	if( pClass == 0 ){` |
|      ! 0 |  3170 | `		return PH7_ABORT;` |
|        - |  3171 | `	}` |
|       35 |  3172 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       35 |  3173 | `	if( pThis == 0 ){` |
|      ! 0 |  3174 | `		return PH7_ABORT;` |
|        - |  3175 | `	}` |
|       35 |  3176 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       35 |  3177 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       17 |  3178 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       35 |  3179 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       35 |  3180 | `	if( pCons ){` |
|       35 |  3181 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       35 |  3182 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       35 |  3183 | `		apArg[0] = &sArg;` |
|       35 |  3184 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       35 |  3185 | `		PH7_MemObjRelease(&sArg);` |
|       17 |  3186 | `	}` |
|       35 |  3187 | `	SyBlobRelease(&sMsg);` |
|       35 |  3188 | `	pFrame = pVm->pFrame;` |
|       35 |  3189 | `	if( pFrame ){` |
|       35 |  3190 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       35 |  3191 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       17 |  3192 | `	}` |
|       35 |  3193 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       35 |  3194 | `	PH7_ClassInstanceUnref(pThis);` |
|       35 |  3195 | `	if( rc == SXERR_ABORT ){` |
|        5 |  3196 | `		return PH7_ABORT;` |
|        - |  3197 | `	}` |
|       31 |  3198 | `	return PH7_EXCEPTION;` |
|       18 |  3199 |  |
|        - |  3200 | `/*` |
|        - |  3201 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - |  3202 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - |  3203 | ` */` |
|        6 |  3204 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|        1 |  3205 |  |
|        - |  3206 | `	ph7_class *pClass;` |
|        - |  3207 | `	ph7_class_instance *pThis;` |
|        - |  3208 | `	ph7_class_method *pCons;` |
|        - |  3209 | `	ph7_value sArg;` |
|        - |  3210 | `	ph7_value *apArg[1];` |
|        - |  3211 | `	SyBlob sMsg;` |
|        - |  3212 | `	SyString sMsgStr;` |
|        - |  3213 | `	VmFrame *pFrame;` |
|        - |  3214 | `	sxi32 rc;` |
|        7 |  3215 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|        7 |  3216 | `	if( pClass == 0 ){` |
|      ! 0 |  3217 | `		return PH7_ABORT;` |
|        - |  3218 | `	}` |
|        7 |  3219 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|        7 |  3220 | `	if( pThis == 0 ){` |
|      ! 0 |  3221 | `		return PH7_ABORT;` |
|        - |  3222 | `	}` |
|        7 |  3223 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        7 |  3224 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|        3 |  3225 | `		pFuncName,zExpected,zGiven);` |
|        7 |  3226 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        7 |  3227 | `	if( pCons ){` |
|        7 |  3228 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        7 |  3229 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        7 |  3230 | `		apArg[0] = &sArg;` |
|        7 |  3231 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        7 |  3232 | `		PH7_MemObjRelease(&sArg);` |
|        3 |  3233 | `	}` |
|        7 |  3234 | `	SyBlobRelease(&sMsg);` |
|        7 |  3235 | `	pFrame = pVm->pFrame;` |
|        7 |  3236 | `	if( pFrame ){` |
|        7 |  3237 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        7 |  3238 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        3 |  3239 | `	}` |
|        7 |  3240 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        7 |  3241 | `	PH7_ClassInstanceUnref(pThis);` |
|        7 |  3242 | `	if( rc == SXERR_ABORT ){` |
|        7 |  3243 | `		return PH7_ABORT;` |
|        - |  3244 | `	}` |
|      ! 0 |  3245 | `	return PH7_EXCEPTION;` |
|        4 |  3246 |  |
|        - |  3247 | `/*` |
|        - |  3248 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - |  3249 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - |  3250 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - |  3251 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - |  3252 | ` */` |
|        - |  3253 | `/*` |
|        - |  3254 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|        - |  3255 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|        - |  3256 | ` * null SyString yields an empty C string. Returns zBuf.` |
|        - |  3257 | ` */` |
|       24 |  3258 | `static const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|        2 |  3259 |  |
|        - |  3260 | `	sxu32 nCopy;` |
|       26 |  3261 | `	if( nBuf == 0 ) return "";` |
|       26 |  3262 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|      ! 0 |  3263 | `		zBuf[0] = 0;` |
|      ! 0 |  3264 | `		return zBuf;` |
|        - |  3265 | `	}` |
|       26 |  3266 | `	nCopy = SyStringLength(pStr);` |
|       26 |  3267 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       26 |  3268 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|       26 |  3269 | `	zBuf[nCopy] = 0;` |
|       26 |  3270 | `	return zBuf;` |
|       14 |  3271 |  |
|        - |  3272 |  |
|      188 |  3273 | `static sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        2 |  3274 |  |
|      190 |  3275 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|        - |  3276 | `	const char *zGiven;` |
|        - |  3277 | `	char zBuf[128];` |
|        - |  3278 | `	char zTypeBuf[128];` |
|        - |  3279 | `	/* Untyped function: no enforcement. */` |
|      190 |  3280 | `	if( pFunc->nReturnType == 0 ){` |
|      ! 0 |  3281 | `		return SXRET_OK;` |
|        - |  3282 | `	}` |
|        - |  3283 | `	/* void return type: the function must not produce a value. */` |
|      190 |  3284 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|       20 |  3285 | `		if( pValue == 0 ){` |
|       18 |  3286 | `			return SXRET_OK;` |
|        - |  3287 | `		}` |
|        - |  3288 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - |  3289 | `		 * still counts as "returned a value" here. */` |
|        3 |  3290 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|        3 |  3291 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|        - |  3292 | `	}` |
|        - |  3293 | `	/* Function fell off the end without an explicit return: PHP implicitly` |
|        - |  3294 | `	 * returns null. For a typed non-nullable return, that's a TypeError. */` |
|      172 |  3295 | `	if( pValue == 0 ){` |
|      ! 0 |  3296 | `		const char *zExpected = "value";` |
|      ! 0 |  3297 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3298 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3299 | `		}` |
|      ! 0 |  3300 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|        - |  3301 | `	}` |
|        - |  3302 | `	/* Union return type — delegate. The function has no flag for nullable` |
|        - |  3303 | `	 * unions; a null alternative is represented inside aReturnUnion, so pass` |
|        - |  3304 | `	 * bNullable=0 here. */` |
|      172 |  3305 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - |  3306 | `		sxi32 rcU;` |
|      ! 0 |  3307 | `		int bNullable = 0;` |
|      ! 0 |  3308 | `		const char *zExpected = "union";` |
|        - |  3309 | ``		/* Scan alternatives for MEMOBJ_NULL, which serves as `T\|null`. */`` |
|        - |  3310 | `		{` |
|        - |  3311 | `			sxu32 i;` |
|      ! 0 |  3312 | `			ph7_type_alt *aAlts = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  3313 | `			for( i = 0; i < SySetUsed(&pFunc->aReturnUnion); i++ ){` |
|      ! 0 |  3314 | `				if( aAlts[i].nType == MEMOBJ_NULL ){ bNullable = 1; break; }` |
|      ! 0 |  3315 | `			}` |
|        - |  3316 | `		}` |
|      ! 0 |  3317 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|      ! 0 |  3318 | `		if( rcU == SXRET_OK ){` |
|      ! 0 |  3319 | `			return SXRET_OK;` |
|        - |  3320 | `		}` |
|      ! 0 |  3321 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3322 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3323 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  3324 | `			zGiven = "null";` |
|      ! 0 |  3325 | `		}else{` |
|      ! 0 |  3326 | `			zGiven = ph7_type_name(pValue);` |
|        - |  3327 | `		}` |
|      ! 0 |  3328 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3329 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3330 | `		}` |
|      ! 0 |  3331 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3332 | `	}` |
|        - |  3333 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - |  3334 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - |  3335 | `	 * it into the TypeError message. */` |
|      172 |  3336 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|        6 |  3337 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|        - |  3338 | `		const char *zExpected;` |
|        - |  3339 | `		ph7_class *pExpected;` |
|        6 |  3340 | `		ph7_class *pSelfNow = 0;` |
|        6 |  3341 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        6 |  3342 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        6 |  3343 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        2 |  3344 | `		}` |
|        6 |  3345 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        3 |  3346 | `			pExpected = pSelfNow;` |
|        4 |  3347 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3348 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3349 | `		}else{` |
|        3 |  3350 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3351 | `		}` |
|        6 |  3352 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|        6 |  3353 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3354 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 |  3355 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3356 | `		}` |
|        6 |  3357 | `		if( pExpected ){` |
|        6 |  3358 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        6 |  3359 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  3360 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3361 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3362 | `			}` |
|        2 |  3363 | `		}` |
|        6 |  3364 | `		return SXRET_OK;` |
|        - |  3365 | `	}` |
|        - |  3366 | `	/* Scalar return type. Allow null pass-through if the function is` |
|        - |  3367 | `	 * nullable (textual "?T" gets that flag, though union+null is handled` |
|        - |  3368 | `	 * above). There's no explicit nullable flag on ph7_vm_func, so detect` |
|        - |  3369 | `	 * via the type-text leading '?'. */` |
|      168 |  3370 | `	if( (pValue->iFlags & MEMOBJ_NULL) ){` |
|        6 |  3371 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0` |
|        8 |  3372 | `		 && pFunc->sReturnTypeName.zString[0] == '?' ){` |
|        8 |  3373 | `			return SXRET_OK;` |
|        - |  3374 | `		}` |
|      ! 0 |  3375 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3376 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3377 | `			"null");` |
|        - |  3378 | `	}` |
|        - |  3379 | `	/* Exact match? Done. */` |
|      162 |  3380 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|      156 |  3381 | `		return SXRET_OK;` |
|        - |  3382 | `	}` |
|        - |  3383 | `	/* Object->scalar is never compatible. */` |
|        8 |  3384 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3385 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3386 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3387 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3388 | `			zGiven);` |
|        - |  3389 | `	}` |
|        - |  3390 | `	/* Array <-> scalar is never compatible. */` |
|        8 |  3391 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      ! 0 |  3392 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3393 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3394 | `			ph7_type_name(pValue));` |
|        - |  3395 | `	}` |
|        - |  3396 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - |  3397 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - |  3398 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - |  3399 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|        8 |  3400 | `	if( !bStrict` |
|        5 |  3401 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|        4 |  3402 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|        6 |  3403 | `	 && !VmStringIsStrictNumeric(pValue) ){` |
|        4 |  3404 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3405 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3406 | `			"string");` |
|        - |  3407 | `	}` |
|        6 |  3408 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|        3 |  3409 | `		return SXRET_OK;` |
|        - |  3410 | `	}` |
|        4 |  3411 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3412 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 |  3413 | `		ph7_type_name(pValue));` |
|       96 |  3414 |  |
|        - |  3415 | `/*` |
|        - |  3416 | ` * Report a fatal named-argument error.` |
|        - |  3417 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  3418 | ` */` |
|        6 |  3419 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        1 |  3420 |  |
|        7 |  3421 | `	const char *zFunc = 0;` |
|        7 |  3422 | `	int nFunc = 0;` |
|        7 |  3423 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        7 |  3424 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        1 |  3425 |  |
|        - |  3426 | `/*` |
|        - |  3427 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3428 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3429 | ` * information.` |
|        - |  3430 | ` * ------------------------------------` |
|        - |  3431 | ` * Simple boring wrapper function.` |
|        - |  3432 | ` * ------------------------------------` |
|        - |  3433 | ` */` |
|       24 |  3434 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  3435 |  |
|        - |  3436 | `	sxi32 rc;` |
|       26 |  3437 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  3438 | `	return rc;` |
|        2 |  3439 |  |
|        - |  3440 | `/*` |
|        - |  3441 | ` * Resolve function context from the current frame.` |
|        - |  3442 | ` */` |
|     1014 |  3443 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  3444 |  |
|        - |  3445 | `	VmFrame *pFrame;` |
|        - |  3446 | `	ph7_vm_func *pFunc;` |
|     1015 |  3447 | `	*pzFuncName = 0;` |
|     1015 |  3448 | `	*pnFuncLen = 0;` |
|     1015 |  3449 | `	pFrame = pVm->pFrame;` |
|     1015 |  3450 | `	if( pFrame == 0 ){` |
|      ! 0 |  3451 | `		return;` |
|        - |  3452 | `	}` |
|     1015 |  3453 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1015 |  3454 | `	if( pFrame->pParent == 0 ){` |
|      991 |  3455 | `		return;` |
|        - |  3456 | `	}` |
|       25 |  3457 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       25 |  3458 | `	if( pFunc == 0 ){` |
|      ! 0 |  3459 | `		return;` |
|        - |  3460 | `	}` |
|       25 |  3461 | `	*pzFuncName = pFunc->sName.zString;` |
|       25 |  3462 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      508 |  3463 |  |
|        - |  3464 | `/*` |
|        - |  3465 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  3466 | ` */` |
|      522 |  3467 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  3468 |  |
|        - |  3469 | `	SyBlob sOut;` |
|        - |  3470 | `	SyString *pFile;` |
|      523 |  3471 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  3472 | `		return PH7_OK;` |
|        - |  3473 | `	}` |
|      523 |  3474 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  3475 | `		zClass = "Exception";` |
|      ! 0 |  3476 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  3477 | `	}` |
|      523 |  3478 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      501 |  3479 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      250 |  3480 | `	}` |
|      523 |  3481 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      523 |  3482 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      523 |  3483 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      523 |  3484 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      523 |  3485 | `	if( zMsg && nMsg > 0 ){` |
|      523 |  3486 | `		SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      523 |  3487 | `		SyBlobAppend(&sOut,zMsg,nMsg);` |
|      261 |  3488 | `	}` |
|      523 |  3489 | `	if( pFile ){` |
|      523 |  3490 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      523 |  3491 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      523 |  3492 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      261 |  3493 | `	}` |
|      523 |  3494 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      523 |  3495 | `	if( pFile ){` |
|      523 |  3496 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      523 |  3497 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      523 |  3498 | `		if( zFuncName && nFuncLen > 0 ){` |
|       25 |  3499 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|       13 |  3500 | `		}else{` |
|      499 |  3501 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  3502 | `		}` |
|      261 |  3503 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  3504 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  3505 | `	}else{` |
|      ! 0 |  3506 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  3507 | `	}` |
|      523 |  3508 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      523 |  3509 | `	if( pFile ){` |
|      523 |  3510 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      523 |  3511 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      523 |  3512 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      523 |  3513 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      261 |  3514 | `	}` |
|      523 |  3515 | `	VmCallErrorHandler(pVm,&sOut);` |
|      523 |  3516 | `	SyBlobRelease(&sOut);` |
|      523 |  3517 | `	return PH7_ABORT;` |
|      262 |  3518 |  |
|        - |  3519 | `/*` |
|        - |  3520 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  3521 | ` */` |
|      568 |  3522 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  3523 |  |
|        - |  3524 | `	ph7_vm *pVm;` |
|        - |  3525 | `	ph7_class *pClass;` |
|        - |  3526 | `	ph7_class_instance *pThis;` |
|        - |  3527 | `	ph7_class_method *pCons;` |
|        - |  3528 | `	ph7_value sArg;` |
|        - |  3529 | `	ph7_value *apArg[1];` |
|        - |  3530 | `	SyBlob sMsg;` |
|        - |  3531 | `	SyString sMsgStr;` |
|        - |  3532 | `	VmFrame *pFrame;` |
|        - |  3533 | `	va_list ap;` |
|        - |  3534 | `	sxi32 rc;` |
|        - |  3535 |  |
|      570 |  3536 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3537 | `		return PH7_ABORT;` |
|        - |  3538 | `	}` |
|      570 |  3539 | `	pVm = pCtx->pVm;` |
|      570 |  3540 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3541 | `		zClass = "Error";` |
|      ! 0 |  3542 | `	}` |
|      570 |  3543 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      570 |  3544 | `	if( pClass == 0 ){` |
|      ! 0 |  3545 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3546 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  3547 | `			zClass` |
|        - |  3548 | `			);` |
|        - |  3549 | `	}` |
|      570 |  3550 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      570 |  3551 | `	if( pThis == 0 ){` |
|      ! 0 |  3552 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3553 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  3554 | `			);` |
|        - |  3555 | `	}` |
|        - |  3556 |  |
|      570 |  3557 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      570 |  3558 | `	va_start(ap,zFormat);` |
|      570 |  3559 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      570 |  3560 | `	va_end(ap);` |
|        - |  3561 |  |
|      570 |  3562 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      570 |  3563 | `	if( pCons ){` |
|      570 |  3564 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      570 |  3565 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      570 |  3566 | `		apArg[0] = &sArg;` |
|      570 |  3567 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      570 |  3568 | `		PH7_MemObjRelease(&sArg);` |
|      284 |  3569 | `	}` |
|      570 |  3570 | `	SyBlobRelease(&sMsg);` |
|        - |  3571 |  |
|      570 |  3572 | `	pFrame = pVm->pFrame;` |
|      570 |  3573 | `	if( pFrame ){` |
|      570 |  3574 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      570 |  3575 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      284 |  3576 | `	}` |
|      570 |  3577 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      570 |  3578 | `	PH7_ClassInstanceUnref(pThis);` |
|      570 |  3579 | `	if( rc == SXERR_ABORT ){` |
|      489 |  3580 | `		return PH7_ABORT;` |
|        - |  3581 | `	}` |
|       82 |  3582 | `	return PH7_EXCEPTION;` |
|      286 |  3583 |  |
|        - |  3584 | `/*` |
|        - |  3585 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  3586 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  3587 | ` */` |
|      ! 0 |  3588 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  3589 |  |
|        - |  3590 | `	ph7_vm *pVm;` |
|        - |  3591 | `	SyBlob sMsg;` |
|      ! 0 |  3592 | `	const char *zFuncName = 0;` |
|      ! 0 |  3593 | `	int nFuncLen = 0;` |
|        - |  3594 | `	va_list ap;` |
|        - |  3595 | `	sxi32 rc;` |
|        - |  3596 |  |
|      ! 0 |  3597 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3598 | `		return PH7_OK;` |
|        - |  3599 | `	}` |
|      ! 0 |  3600 | `	pVm = pCtx->pVm;` |
|      ! 0 |  3601 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3602 | `		zClass = "Error";` |
|      ! 0 |  3603 | `	}` |
|        - |  3604 |  |
|      ! 0 |  3605 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3606 |  |
|      ! 0 |  3607 | `	va_start(ap,zFormat);` |
|      ! 0 |  3608 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  3609 | `	va_end(ap);` |
|        - |  3610 |  |
|      ! 0 |  3611 | `	if( pCtx->pFunc ){` |
|      ! 0 |  3612 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  3613 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  3614 | `	}` |
|      ! 0 |  3615 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  3616 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  3617 | `	}` |
|      ! 0 |  3618 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  3619 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  3620 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  3621 | `	return rc;` |
|      ! 0 |  3622 |  |
|        - |  3623 | `/*` |
|        - |  3624 | ` * Save the execution state of a fiber/generator context.` |
|        - |  3625 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  3626 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  3627 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  3628 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  3629 | ` * when VmByteCodeExec returns.` |
|        - |  3630 | ` */` |
|      144 |  3631 | `static sxi32 VmSuspendCtx(` |
|        - |  3632 | `	ph7_vm *pVm,` |
|        - |  3633 | `	ph7_exec_ctx *pCtx,` |
|        - |  3634 | `	sxi32 pc,` |
|        - |  3635 | `	sxi32 nTos` |
|        - |  3636 | `	)` |
|        2 |  3637 |  |
|       72 |  3638 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      146 |  3639 | `	pCtx->pc = pc;` |
|      146 |  3640 | `	pCtx->nTos = nTos;` |
|      146 |  3641 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      146 |  3642 | `	return PH7_SUSPEND;` |
|        2 |  3643 |  |
|        - |  3644 | `/*` |
|        - |  3645 | ` * Resolve named-argument mapping.` |
|        - |  3646 | ` *` |
|        - |  3647 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  3648 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  3649 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  3650 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  3651 | ` * every formal parameter that received a value.` |
|        - |  3652 | ` *` |
|        - |  3653 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  3654 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  3655 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  3656 | ` */` |
|       98 |  3657 | `static sxi32 VmResolveNamedArgs(` |
|        - |  3658 | `	ph7_vm *pVm,` |
|        - |  3659 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  3660 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  3661 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  3662 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  3663 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  3664 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  3665 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  3666 |  |
|        2 |  3667 |  |
|      100 |  3668 | `	sxi32 posIdx = 0;` |
|        - |  3669 | `	sxu32 i;` |
|        - |  3670 | `	char zErrMsg[256];` |
|      100 |  3671 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      296 |  3672 | `	for( i = 0; i < nActual; i++ ){` |
|      198 |  3673 | `		aSlot[i] = -2;` |
|      100 |  3674 | `	}` |
|      290 |  3675 | `	for( i = 0; i < nActual; i++ ){` |
|      286 |  3676 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  3677 | `			/* Named argument — find formal by name */` |
|      184 |  3678 | `			int found = 0;` |
|        - |  3679 | `			sxu32 k;` |
|      304 |  3680 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      290 |  3681 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      281 |  3682 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      268 |  3683 | `						pMap->aNames[i].zString,` |
|      402 |  3684 | `						pMap->aNames[i].nByte) == 0 ){` |
|      172 |  3685 | `					if( aUsed[k] ){` |
|        7 |  3686 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3687 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  3688 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        5 |  3689 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        5 |  3690 | `						return PH7_ABORT;` |
|        - |  3691 | `					}` |
|      168 |  3692 | `					aSlot[i] = (sxi32)k;` |
|      168 |  3693 | `					aUsed[k] = 1;` |
|      168 |  3694 | `					found = 1;` |
|      168 |  3695 | `					break;` |
|        - |  3696 | `				}` |
|       62 |  3697 | `			}` |
|      180 |  3698 | `			if( !found ){` |
|       14 |  3699 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  3700 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  3701 | `				}else{` |
|        4 |  3702 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3703 | `						"Unknown named parameter $%.*s",` |
|        2 |  3704 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  3705 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  3706 | `					return PH7_ABORT;` |
|        - |  3707 | `				}` |
|        5 |  3708 | `			}` |
|       90 |  3709 | `		}else{` |
|        - |  3710 | `			/* Positional argument */` |
|       16 |  3711 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       16 |  3712 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  3713 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3714 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  3715 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  3716 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  3717 | `					return PH7_ABORT;` |
|        - |  3718 | `				}` |
|       16 |  3719 | `				aSlot[i] = posIdx;` |
|       16 |  3720 | `				aUsed[posIdx] = 1;` |
|        7 |  3721 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  3722 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  3723 | `			}` |
|       16 |  3724 | `			posIdx++;` |
|        - |  3725 | `		}` |
|       97 |  3726 | `	}` |
|       93 |  3727 | `	return SXRET_OK;` |
|       51 |  3728 |  |
|        - |  3729 | `/*` |
|        - |  3730 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  3731 | ` *` |
|        - |  3732 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  3733 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  3734 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  3735 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  3736 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  3737 | ` * then the program execution is halted.` |
|        - |  3738 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  3739 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  3740 | ` * or to reset the VM to it's initial state.` |
|        - |  3741 | ` */` |
|    42332 |  3742 | `static sxi32 VmByteCodeExec(` |
|        - |  3743 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3744 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  3745 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  3746 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  3747 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  3748 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  3749 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  3750 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  3751 | `	ph7_vm_func *pEnforceRetFunc /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  3752 | `	)` |
|        2 |  3753 |  |
|        - |  3754 | `	VmInstr *pInstr;` |
|        - |  3755 | `	ph7_value *pTos;` |
|        - |  3756 | `	SySet aArg;` |
|        - |  3757 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  3758 | `	sxi32 pc;` |
|        - |  3759 | `	sxi32 rc;` |
|        - |  3760 | `	/* Argument container */` |
|    42334 |  3761 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    42334 |  3762 | `	if( nTos < 0 ){` |
|    39596 |  3763 | `		pTos = &pStack[-1];` |
|    19799 |  3764 | `	}else{` |
|     2740 |  3765 | `		pTos = &pStack[nTos];` |
|        - |  3766 | `	}` |
|    42334 |  3767 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    42334 |  3768 | `	pc = nPc;` |
|        - |  3769 | `/*` |
|        - |  3770 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  3771 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  3772 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  3773 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  3774 | ` */` |
|        - |  3775 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  3776 | `	{ \` |
|        - |  3777 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  3778 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  3779 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  3780 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  3781 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  3782 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  3783 | `				break; \` |
|        - |  3784 | `			} \` |
|        - |  3785 | `			goto Exception; \` |
|        - |  3786 | `		} \` |
|        - |  3787 | `	}` |
|        - |  3788 | `	/* Execute as much as we can */` |
|  5734887 |  3789 | `	for(;;){` |
|        - |  3790 | `		/* Fetch the instruction to execute */` |
| 11469072 |  3791 | `		pInstr = &aInstr[pc];` |
| 11469072 |  3792 | `		rc = SXRET_OK;` |
|        - |  3793 | `/*` |
|        - |  3794 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  3795 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  3796 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  3797 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  3798 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  3799 | ` */` |
| 11469072 |  3800 | `		switch(pInstr->iOp){` |
|        - |  3801 | `/*` |
|        - |  3802 | ` * DONE: P1 * *` |
|        - |  3803 | ` *` |
|        - |  3804 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  3805 | ` * and return immediately.` |
|        - |  3806 | ` */` |
|    20822 |  3807 | `case PH7_OP_DONE:` |
|        - |  3808 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  3809 | `	 * the fiber start/resume paths) set pEnforceRetFunc, so this branch is` |
|        - |  3810 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  3811 | `	 * callback trampolines, and the main script. */` |
|    41646 |  3812 | `	if( pEnforceRetFunc && pEnforceRetFunc->nReturnType > 0 ){` |
|      190 |  3813 | `		ph7_value *pRetVal = 0;` |
|      190 |  3814 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|      174 |  3815 | `			pRetVal = pTos;` |
|       86 |  3816 | `		}` |
|      190 |  3817 | `		rc = VmEnforceReturnType(&(*pVm), pEnforceRetFunc, pRetVal);` |
|      190 |  3818 | `		if( rc == PH7_ABORT ) goto Abort;` |
|      184 |  3819 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  3820 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      ! 0 |  3821 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3822 | `				pTos--;` |
|      ! 0 |  3823 | `			}` |
|      ! 0 |  3824 | `			goto Exception;` |
|        - |  3825 | `		}` |
|        - |  3826 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  3827 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  3828 | `		 * defensively we clear the pointer after a successful check). */` |
|      184 |  3829 | `		pEnforceRetFunc = 0;` |
|       91 |  3830 | `	}` |
|    41640 |  3831 | `	if( pInstr->iP1 ){` |
|        - |  3832 | `#ifdef UNTRUST` |
|        - |  3833 | `		if( pTos < pStack ){` |
|        - |  3834 | `			goto Abort;` |
|        - |  3835 | `		}` |
|        - |  3836 | `#endif` |
|    25250 |  3837 | `		if( pLastRef ){` |
|    15622 |  3838 | `			*pLastRef = pTos->nIdx;` |
|     7810 |  3839 | `		}` |
|    25250 |  3840 | `		if( pResult ){` |
|        - |  3841 | `			/* Execution result */` |
|    23906 |  3842 | `			PH7_MemObjStore(pTos,pResult);` |
|    11952 |  3843 | `		}` |
|    25250 |  3844 | `		VmPopOperand(&pTos,1);` |
|    29016 |  3845 | `	}else if( pLastRef ){` |
|        - |  3846 | `		/* Nothing referenced */` |
|     1644 |  3847 | `		*pLastRef = SXU32_HIGH;` |
|      821 |  3848 | `	}` |
|        - |  3849 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  3850 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  3851 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  3852 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  3853 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  3854 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  3855 | `	 * block can override it.` |
|        - |  3856 | `	 */` |
|    41642 |  3857 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  3858 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  3859 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  3860 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  3861 | `		pExc->pFrame = 0;` |
|        3 |  3862 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  3863 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  3864 | `			pExc->iFinallyDone = 1;` |
|        - |  3865 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  3866 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  3867 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  3868 | `				goto Abort;` |
|        - |  3869 | `			}` |
|        1 |  3870 | `		}` |
|        1 |  3871 | `	}` |
|    41640 |  3872 | `	goto Done;` |
|        - |  3873 | `/*` |
|        - |  3874 | ` * HALT: P1 * *` |
|        - |  3875 | ` *` |
|        - |  3876 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  3877 | ` * and abort immediately.` |
|        - |  3878 | ` */` |
|        4 |  3879 | `case PH7_OP_HALT:` |
|        9 |  3880 | `	if( pInstr->iP1 ){` |
|        - |  3881 | `#ifdef UNTRUST` |
|        - |  3882 | `		if( pTos < pStack ){` |
|        - |  3883 | `			goto Abort;` |
|        - |  3884 | `		}` |
|        - |  3885 | `#endif` |
|        9 |  3886 | `		if( pLastRef ){` |
|      ! 0 |  3887 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  3888 | `		}` |
|        9 |  3889 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  3890 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  3891 | `				/* Output the exit message */` |
|        7 |  3892 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  3893 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  3894 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  3895 | `			}` |
|        7 |  3896 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  3897 | `			/* Record exit status */` |
|        5 |  3898 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  3899 | `		}` |
|        9 |  3900 | `		VmPopOperand(&pTos,1);` |
|        4 |  3901 | `	}else if( pLastRef ){` |
|        - |  3902 | `		/* Nothing referenced */` |
|      ! 0 |  3903 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  3904 | `	}` |
|        - |  3905 | `	/* Check if we're in an included file context */` |
|        9 |  3906 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  3907 | `		/* Terminate the entire process */` |
|        9 |  3908 | `		exit(pVm->iExitStatus);` |
|        - |  3909 | `	}` |
|      ! 0 |  3910 | `	goto Abort;` |
|        - |  3911 | `/*` |
|        - |  3912 | ` * JMP: * P2 *` |
|        - |  3913 | ` *` |
|        - |  3914 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  3915 | ` * the one at index P2 from the beginning of the program.` |
|        - |  3916 | ` */` |
|   244726 |  3917 | `case PH7_OP_JMP:` |
|   489498 |  3918 | `	pc = pInstr->iP2 - 1;` |
|   489498 |  3919 | `	break;` |
|        - |  3920 | `/*` |
|        - |  3921 | ` * JZ: P1 P2 *` |
|        - |  3922 | ` *` |
|        - |  3923 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  3924 | ` * entry in the stack if P1 is zero.` |
|        - |  3925 | ` */` |
|   580220 |  3926 | `case PH7_OP_JZ:` |
|        - |  3927 | `#ifdef UNTRUST` |
|        - |  3928 | `	if( pTos < pStack ){` |
|        - |  3929 | `		goto Abort;` |
|        - |  3930 | `	}` |
|        - |  3931 | `#endif` |
|        - |  3932 | `	/* Get a boolean value */` |
|  1160530 |  3933 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      172 |  3934 | `		PH7_MemObjToBool(pTos);` |
|       85 |  3935 | `	}` |
|  1160530 |  3936 | `	if( !pTos->x.iVal ){` |
|        - |  3937 | `		/* Take the jump */` |
|   595066 |  3938 | `		pc = pInstr->iP2 - 1;` |
|   297532 |  3939 | `	}` |
|  1160530 |  3940 | `	if( !pInstr->iP1 ){` |
|   921534 |  3941 | `		VmPopOperand(&pTos,1);` |
|   460788 |  3942 | `	}` |
|  1160530 |  3943 | `	break;` |
|        - |  3944 | `/*` |
|        - |  3945 | ` * JNZ: P1 P2 *` |
|        - |  3946 | ` *` |
|        - |  3947 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  3948 | ` * entry in the stack if P1 is zero.` |
|        - |  3949 | ` */` |
|    60767 |  3950 | `case PH7_OP_JNZ:` |
|        - |  3951 | `#ifdef UNTRUST` |
|        - |  3952 | `	if( pTos < pStack ){` |
|        - |  3953 | `		goto Abort;` |
|        - |  3954 | `	}` |
|        - |  3955 | `#endif` |
|        - |  3956 | `	/* Get a boolean value */` |
|   121536 |  3957 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  3958 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  3959 | `	}` |
|   121536 |  3960 | `	if( pTos->x.iVal ){` |
|        - |  3961 | `		/* Take the jump */` |
|     5382 |  3962 | `		pc = pInstr->iP2 - 1;` |
|     2690 |  3963 | `	}` |
|   121536 |  3964 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  3965 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  3966 | `	}` |
|   121536 |  3967 | `	break;` |
|        - |  3968 | `/*` |
|        - |  3969 | ` * NOOP: * * *` |
|        - |  3970 | ` *` |
|        - |  3971 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  3972 | ` * destination.` |
|        - |  3973 | ` */` |
|      ! 0 |  3974 | `case PH7_OP_NOOP:` |
|      ! 0 |  3975 | `	break;` |
|        - |  3976 | `/*` |
|        - |  3977 | ` * POP: P1 * *` |
|        - |  3978 | ` *` |
|        - |  3979 | ` * Pop P1 elements from the operand stack.` |
|        - |  3980 | ` */` |
|   448218 |  3981 | `case PH7_OP_POP: {` |
|   896482 |  3982 | `	sxi32 n = pInstr->iP1;` |
|   896482 |  3983 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  3984 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       17 |  3985 | `		n = (sxi32)(pTos - pStack);` |
|        8 |  3986 | `	}` |
|   896482 |  3987 | `	VmPopOperand(&pTos,n);` |
|   896482 |  3988 | `	break;` |
|        - |  3989 | `				 }` |
|        - |  3990 | `/*` |
|        - |  3991 | ` * DUP: * * *` |
|        - |  3992 | ` *` |
|        - |  3993 | ` * Duplicate the top of the stack.` |
|        - |  3994 | ` */` |
|       41 |  3995 | `case PH7_OP_DUP:` |
|        - |  3996 | `#ifdef UNTRUST` |
|        - |  3997 | `	if( pTos < pStack ){` |
|        - |  3998 | `		goto Abort;` |
|        - |  3999 | `	}` |
|        - |  4000 | `#endif` |
|       84 |  4001 | `	pTos++;` |
|       84 |  4002 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  4003 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  4004 | `	break;` |
|        - |  4005 | `/*` |
|        - |  4006 | ` * NSSWITCH: * * P3` |
|        - |  4007 | ` *` |
|        - |  4008 | ` * Switch the active namespace at runtime.` |
|        - |  4009 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  4010 | ` */` |
|     7527 |  4011 | `case PH7_OP_NSSWITCH:` |
|    15056 |  4012 | `	SyBlobReset(&pVm->sNamespace);` |
|    15056 |  4013 | `	if( pInstr->p3 ){` |
|       98 |  4014 | `		const char *zNs = (const char *)pInstr->p3;` |
|       98 |  4015 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       48 |  4016 | `	}` |
|        - |  4017 | `	/* Clear namespace-scoped use-const imports */` |
|    15056 |  4018 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    15056 |  4019 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    15056 |  4020 | `	break;` |
|        - |  4021 | `/* OP_USECONST P1 * P3` |
|        - |  4022 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  4023 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  4024 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  4025 | ` */` |
|        7 |  4026 | `case PH7_OP_USECONST: {` |
|       16 |  4027 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  4028 | `	if( azPair ){` |
|       16 |  4029 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  4030 | `	}` |
|       16 |  4031 | `	break;` |
|        - |  4032 | `				}` |
|        - |  4033 | `/*` |
|        - |  4034 | ` * CVT_INT: * * *` |
|        - |  4035 | ` *` |
|        - |  4036 | ` * Force the top of the stack to be an integer.` |
|        - |  4037 | ` */` |
|       78 |  4038 | `case PH7_OP_CVT_INT:` |
|        - |  4039 | `#ifdef UNTRUST` |
|        - |  4040 | `	if( pTos < pStack ){` |
|        - |  4041 | `		goto Abort;` |
|        - |  4042 | `	}` |
|        - |  4043 | `#endif` |
|      158 |  4044 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      111 |  4045 | `		PH7_MemObjToInteger(pTos);` |
|       55 |  4046 | `	}` |
|        - |  4047 | `	/* Invalidate any prior representation */` |
|      158 |  4048 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      158 |  4049 | `	break;` |
|        - |  4050 | `/*` |
|        - |  4051 | ` * CVT_REAL: * * *` |
|        - |  4052 | ` *` |
|        - |  4053 | ` * Force the top of the stack to be a real.` |
|        - |  4054 | ` */` |
|        5 |  4055 | `case PH7_OP_CVT_REAL:` |
|        - |  4056 | `#ifdef UNTRUST` |
|        - |  4057 | `	if( pTos < pStack ){` |
|        - |  4058 | `		goto Abort;` |
|        - |  4059 | `	}` |
|        - |  4060 | `#endif` |
|       11 |  4061 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4062 | `		PH7_MemObjToReal(pTos);` |
|        3 |  4063 | `	}` |
|        - |  4064 | `	/* Invalidate any prior representation */` |
|       11 |  4065 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       11 |  4066 | `	break;` |
|        - |  4067 | `/*` |
|        - |  4068 | ` * CVT_STR: * * *` |
|        - |  4069 | ` *` |
|        - |  4070 | ` * Force the top of the stack to be a string.` |
|        - |  4071 | ` */` |
|      146 |  4072 | `case PH7_OP_CVT_STR:` |
|        - |  4073 | `#ifdef UNTRUST` |
|        - |  4074 | `	if( pTos < pStack ){` |
|        - |  4075 | `		goto Abort;` |
|        - |  4076 | `	}` |
|        - |  4077 | `#endif` |
|      294 |  4078 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  4079 | `		PH7_MemObjToString(pTos);` |
|      146 |  4080 | `	}` |
|      294 |  4081 | `	break;` |
|        - |  4082 | `/*` |
|        - |  4083 | ` * CVT_BOOL: * * *` |
|        - |  4084 | ` *` |
|        - |  4085 | ` * Force the top of the stack to be a boolean.` |
|        - |  4086 | ` */` |
|        5 |  4087 | `case PH7_OP_CVT_BOOL:` |
|        - |  4088 | `#ifdef UNTRUST` |
|        - |  4089 | `	if( pTos < pStack ){` |
|        - |  4090 | `		goto Abort;` |
|        - |  4091 | `	}` |
|        - |  4092 | `#endif` |
|       11 |  4093 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  4094 | `		PH7_MemObjToBool(pTos);` |
|        3 |  4095 | `	}` |
|       11 |  4096 | `	break;` |
|        - |  4097 | `/*` |
|        - |  4098 | ` * CVT_NULL: * * *` |
|        - |  4099 | ` *` |
|        - |  4100 | ` * Nullify the top of the stack.` |
|        - |  4101 | ` */` |
|        3 |  4102 | `case PH7_OP_CVT_NULL:` |
|        - |  4103 | `#ifdef UNTRUST` |
|        - |  4104 | `	if( pTos < pStack ){` |
|        - |  4105 | `		goto Abort;` |
|        - |  4106 | `	}` |
|        - |  4107 | `#endif` |
|        7 |  4108 | `	PH7_MemObjRelease(pTos);` |
|        7 |  4109 | `	break;` |
|        - |  4110 | `/*` |
|        - |  4111 | ` * CVT_NUMC: * * *` |
|        - |  4112 | ` *` |
|        - |  4113 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  4114 | ` */` |
|      ! 0 |  4115 | `case PH7_OP_CVT_NUMC:` |
|        - |  4116 | `#ifdef UNTRUST` |
|        - |  4117 | `	if( pTos < pStack ){` |
|        - |  4118 | `		goto Abort;` |
|        - |  4119 | `	}` |
|        - |  4120 | `#endif` |
|        - |  4121 | `	/* Force a numeric cast */` |
|      ! 0 |  4122 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  4123 | `	break;` |
|        - |  4124 | `/*` |
|        - |  4125 | ` * CVT_ARRAY: * * *` |
|        - |  4126 | ` *` |
|        - |  4127 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  4128 | ` */` |
|       10 |  4129 | `case PH7_OP_CVT_ARRAY:` |
|        - |  4130 | `#ifdef UNTRUST` |
|        - |  4131 | `	if( pTos < pStack ){` |
|        - |  4132 | `		goto Abort;` |
|        - |  4133 | `	}` |
|        - |  4134 | `#endif` |
|        - |  4135 | `	/* Force a hashmap cast */` |
|       21 |  4136 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  4137 | `	if( rc != SXRET_OK ){` |
|        - |  4138 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  4139 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4140 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  4141 | `	}` |
|       21 |  4142 | `	break;` |
|        - |  4143 | `/*` |
|        - |  4144 | ` * CVT_OBJ: * * *` |
|        - |  4145 | ` *` |
|        - |  4146 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  4147 | ` */` |
|        8 |  4148 | `case PH7_OP_CVT_OBJ:` |
|        - |  4149 | `#ifdef UNTRUST` |
|        - |  4150 | `	if( pTos < pStack ){` |
|        - |  4151 | `		goto Abort;` |
|        - |  4152 | `	}` |
|        - |  4153 | `#endif` |
|       17 |  4154 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  4155 | `		/* Force a 'stdClass()' cast */` |
|       17 |  4156 | `		PH7_MemObjToObject(pTos);` |
|        8 |  4157 | `	}` |
|       17 |  4158 | `	break;` |
|        - |  4159 | `/*` |
|        - |  4160 | ` * ERR_CTRL * * *` |
|        - |  4161 | ` *` |
|        - |  4162 | ` * Error control operator.` |
|        - |  4163 | ` */` |
|    15456 |  4164 | `case PH7_OP_ERR_CTRL:` |
|        - |  4165 | `	/*` |
|        - |  4166 | `	 * TICKET 1433-038:` |
|        - |  4167 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  4168 | `	 * use the public API,to control error output.` |
|        - |  4169 | `	 */` |
|    30912 |  4170 | `	break;` |
|        - |  4171 | `/*` |
|        - |  4172 | ` * IS_A * * *` |
|        - |  4173 | ` *` |
|        - |  4174 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  4175 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  4176 | ` * holding a class name or an object).` |
|        - |  4177 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  4178 | ` */` |
|       42 |  4179 | `case PH7_OP_IS_A:{` |
|       86 |  4180 | `	ph7_value *pNos = &pTos[-1];` |
|       86 |  4181 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  4182 | `#ifdef UNTRUST` |
|        - |  4183 | `	if( pNos < pStack ){` |
|        - |  4184 | `		goto Abort;` |
|        - |  4185 | `	}` |
|        - |  4186 | `#endif` |
|       86 |  4187 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       84 |  4188 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       84 |  4189 | `		ph7_class *pClass = 0;` |
|        - |  4190 | `		/* Extract the target class */` |
|       84 |  4191 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4192 | `			/* Instance already loaded */` |
|      ! 0 |  4193 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       84 |  4194 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       84 |  4195 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       84 |  4196 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  4197 | `			/* Handle self/static/parent keywords */` |
|       84 |  4198 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  4199 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       82 |  4200 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  4201 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       81 |  4202 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  4203 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  4204 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  4205 | `					pClass = pSelf->pBase;` |
|        2 |  4206 | `				}` |
|        3 |  4207 | `			}else{` |
|       74 |  4208 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  4209 | `			}` |
|       41 |  4210 | `		}` |
|       84 |  4211 | `		if( pClass ){` |
|        - |  4212 | `			/* Perform the query */` |
|       84 |  4213 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       41 |  4214 | `		}` |
|       41 |  4215 | `	}` |
|        - |  4216 | `	/* Push result */` |
|       86 |  4217 | `	VmPopOperand(&pTos,1);` |
|       86 |  4218 | `	PH7_MemObjRelease(pTos);` |
|       86 |  4219 | `	pTos->x.iVal = iRes;` |
|       86 |  4220 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       86 |  4221 | `	break;` |
|        - |  4222 | `				 }` |
|        - |  4223 |  |
|        - |  4224 | `/*` |
|        - |  4225 | ` * LOADC P1 P2 *` |
|        - |  4226 | ` *` |
|        - |  4227 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  4228 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  4229 | ` */` |
|   977717 |  4230 | `case PH7_OP_LOADC: {` |
|        - |  4231 | `	ph7_value *pObj;` |
|        - |  4232 | `	/* Reserve a room */` |
|  1955480 |  4233 | `	pTos++;` |
|  2923769 |  4234 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1955480 |  4235 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  4236 | `			SyHashEntry *pEntry;` |
|        - |  4237 | `			/* Check use const imports first — imports take precedence */` |
|        - |  4238 | `			{` |
|        - |  4239 | `				SyHashEntry *pConstImport;` |
|    28421 |  4240 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    18946 |  4241 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    18948 |  4242 | `				if( pConstImport ){` |
|       11 |  4243 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  4244 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  4245 | `					if( pEntry ){` |
|       11 |  4246 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  4247 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  4248 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  4249 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  4250 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  4251 | `						break;` |
|        - |  4252 | `					}` |
|        - |  4253 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  4254 | `				}` |
|        - |  4255 | `			}` |
|        - |  4256 | `			/* Candidate for expansion via user defined callbacks */` |
|    18938 |  4257 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    18938 |  4258 | `			if( pEntry ){` |
|    18934 |  4259 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  4260 | `				/* Set a NULL default value */` |
|    18934 |  4261 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    18934 |  4262 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  4263 | `				/* Invoke the callback and deal with the expanded value */` |
|    18934 |  4264 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  4265 | `				/* Mark as constant */` |
|    18934 |  4266 | `				pTos->nIdx = SXU32_HIGH;` |
|    18934 |  4267 | `				break;` |
|        - |  4268 | `			}` |
|        - |  4269 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  4270 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  4271 | `			 * use-const imports → current NS → global → string fallback).` |
|        - |  4272 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - |  4273 | `			{` |
|        6 |  4274 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  4275 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  4276 | `				sxu32 j;` |
|        6 |  4277 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       14 |  4278 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|        9 |  4279 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|        5 |  4280 | `				}` |
|        6 |  4281 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  4282 | `					/* Try current_namespace\name */` |
|      ! 0 |  4283 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  4284 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  4285 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  4286 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  4287 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  4288 | `					if( pEntry ){` |
|      ! 0 |  4289 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  4290 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4291 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  4292 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  4293 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4294 | `						break;` |
|        - |  4295 | `					}` |
|        - |  4296 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  4297 | `				}` |
|        6 |  4298 | `				if( isQualified ){` |
|        - |  4299 | `					/* Qualified name: must be a real constant. */` |
|        3 |  4300 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  4301 | `					SyBlob sErr;` |
|        3 |  4302 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  4303 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  4304 | `					if( pErrFile ){` |
|        3 |  4305 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  4306 | `					}` |
|        3 |  4307 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  4308 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  4309 | `					SyBlobRelease(&sErr);` |
|        3 |  4310 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  4311 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  4312 | `					goto LoadC_Done;` |
|        - |  4313 | `				}` |
|        - |  4314 | `			}` |
|        1 |  4315 | `		}` |
|  1936536 |  4316 | `		PH7_MemObjLoad(pObj,pTos);` |
|   968291 |  4317 | `	}else{` |
|        - |  4318 | `		/* Set a NULL value */` |
|      ! 0 |  4319 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4320 | `	}` |
|   968246 |  4321 | `LoadC_Done:` |
|        - |  4322 | `	/* Mark as constant */` |
|  1936538 |  4323 | `	pTos->nIdx = SXU32_HIGH;` |
|  1936538 |  4324 | `	break;` |
|        - |  4325 | `				  }` |
|        - |  4326 | `/*` |
|        - |  4327 | ` * LOAD: P1 * P3` |
|        - |  4328 | ` *` |
|        - |  4329 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  4330 | ` * from the P3 operand.` |
|        - |  4331 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  4332 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  4333 | ` */` |
|  1539648 |  4334 | `case PH7_OP_LOAD:{` |
|        - |  4335 | `	ph7_value *pObj;` |
|        - |  4336 | `	SyString sName;` |
|  3079518 |  4337 | `	if( pInstr->p3 == 0 ){` |
|        - |  4338 | `		/* Take the variable name from the top of the stack */` |
|        - |  4339 | `#ifdef UNTRUST` |
|        - |  4340 | `		if( pTos < pStack ){` |
|        - |  4341 | `			goto Abort;` |
|        - |  4342 | `		}` |
|        - |  4343 | `#endif` |
|        - |  4344 | `		/* Force a string cast */` |
|       19 |  4345 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4346 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4347 | `		}` |
|       19 |  4348 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  4349 | `	}else{` |
|  3079500 |  4350 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4351 | `		/* Reserve a room for the target object */` |
|  3079500 |  4352 | `		pTos++;` |
|        - |  4353 | `	}` |
|        - |  4354 | `	/* Extract the requested memory object */` |
|  3079518 |  4355 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  3079518 |  4356 | `	if( pObj == 0 ){` |
|       28 |  4357 | `		if( pInstr->iP1 ){` |
|        - |  4358 | `			/* Variable not found,load NULL */` |
|       28 |  4359 | `			if( !pInstr->p3 ){` |
|      ! 0 |  4360 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4361 | `			}else{` |
|       28 |  4362 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4363 | `			}` |
|       28 |  4364 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1539663 |  4365 | `			break;` |
|      ! 0 |  4366 | `		}else{` |
|        - |  4367 | `			/* Fatal error */` |
|      ! 0 |  4368 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4369 | `			goto Abort;` |
|        - |  4370 | `		}` |
|        - |  4371 | `	}` |
|        - |  4372 | `	/* Load variable contents */` |
|  3079492 |  4373 | `	PH7_MemObjLoad(pObj,pTos);` |
|  3079492 |  4374 | `	pTos->nIdx = pObj->nIdx;` |
|  3079492 |  4375 | `	break;` |
|        - |  4376 | `				   }` |
|        - |  4377 | `/*` |
|        - |  4378 | ` * LOAD_MAP P1 * *` |
|        - |  4379 | ` *` |
|        - |  4380 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  4381 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  4382 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  4383 | ` */` |
|    21799 |  4384 | `case PH7_OP_LOAD_MAP: {` |
|        - |  4385 | `	ph7_hashmap *pMap;` |
|        - |  4386 | `	/* Allocate a new hashmap instance */` |
|    43600 |  4387 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    43600 |  4388 | `	if( pMap == 0 ){` |
|      ! 0 |  4389 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4390 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  4391 | `		goto Abort;` |
|        - |  4392 | `	}` |
|    43600 |  4393 | `	if( pInstr->iP1 > 0 ){` |
|     2390 |  4394 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  4395 | `		/* Perform the insertion */` |
|     7366 |  4396 | `		while( pEntry < pTos ){` |
|     4978 |  4397 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  4398 | `				/* Insertion by reference */` |
|      142 |  4399 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  4400 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  4401 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  4402 | `					);` |
|       48 |  4403 | `			}else{` |
|        - |  4404 | `				/* Standard insertion */` |
|     7325 |  4405 | `				PH7_HashmapInsert(pMap,` |
|     4882 |  4406 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2441 |  4407 | `					&pEntry[1]` |
|        - |  4408 | `				);` |
|        - |  4409 | `			}` |
|        - |  4410 | `			/* Next pair on the stack */` |
|     4978 |  4411 | `			pEntry += 2;` |
|        2 |  4412 | `		}` |
|        - |  4413 | `		/* Pop P1 elements */` |
|     2390 |  4414 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1194 |  4415 | `	}` |
|        - |  4416 | `	/* Push the hashmap */` |
|    43600 |  4417 | `	pTos++;` |
|    43600 |  4418 | `	pTos->nIdx = SXU32_HIGH;` |
|    43600 |  4419 | `	pTos->x.pOther = pMap;` |
|    43600 |  4420 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    43600 |  4421 | `	break;` |
|        - |  4422 | `					  }` |
|        - |  4423 | `/*` |
|        - |  4424 | ` * LOAD_LIST: P1 * *` |
|        - |  4425 | ` *` |
|        - |  4426 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  4427 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  4428 | ` * Caveats:` |
|        - |  4429 | ` *  This implementation support only a single nesting level.` |
|        - |  4430 | ` */` |
|       48 |  4431 | `case PH7_OP_LOAD_LIST: {` |
|        - |  4432 | `	ph7_value *pEntry;` |
|       98 |  4433 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  4434 | `		/* Empty list,break immediately */` |
|      ! 0 |  4435 | `		break;` |
|        - |  4436 | `	}` |
|       98 |  4437 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  4438 | `#ifdef UNTRUST` |
|        - |  4439 | `	if( &pEntry[-1] < pStack ){` |
|        - |  4440 | `		goto Abort;` |
|        - |  4441 | `	}` |
|        - |  4442 | `#endif` |
|       98 |  4443 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  4444 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  4445 | `		ph7_hashmap_node *pNode;` |
|        - |  4446 | `		ph7_value sKey,*pObj;` |
|        - |  4447 | `		/* Start Copying */` |
|       91 |  4448 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  4449 | `		while( pEntry <= pTos ){` |
|      193 |  4450 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  4451 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  4452 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  4453 | `					if( rc == SXRET_OK ){` |
|        - |  4454 | `						/* Store node value */` |
|      165 |  4455 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  4456 | `					}else{` |
|        - |  4457 | `						/* Undefined array key */` |
|        - |  4458 | `						char zMsg[128];` |
|      ! 0 |  4459 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  4460 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4461 | `						PH7_MemObjRelease(pObj);` |
|        - |  4462 | `					}` |
|       82 |  4463 | `				}` |
|       82 |  4464 | `			}` |
|      193 |  4465 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  4466 | `			pEntry++;` |
|        1 |  4467 | `		}` |
|       46 |  4468 | `	}else{` |
|        - |  4469 | `		/* Source is not an array */` |
|        - |  4470 | `		ph7_value *pObj;` |
|       18 |  4471 | `		while( pEntry <= pTos ){` |
|       12 |  4472 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  4473 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  4474 | `					PH7_MemObjRelease(pObj);` |
|        5 |  4475 | `				}` |
|        5 |  4476 | `			}` |
|       12 |  4477 | `			pEntry++;` |
|        2 |  4478 | `		}` |
|        8 |  4479 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  4480 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  4481 | `			const char *zType = "unknown";` |
|        3 |  4482 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  4483 | `			char zMsg[256];` |
|        3 |  4484 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  4485 | `				zType = "string";` |
|        1 |  4486 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  4487 | `				zType = "int";` |
|      ! 0 |  4488 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4489 | `				zType = "float";` |
|      ! 0 |  4490 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4491 | `				zType = "object";` |
|      ! 0 |  4492 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  4493 | `				zType = "resource";` |
|      ! 0 |  4494 | `			}` |
|        3 |  4495 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  4496 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  4497 | `		}` |
|        - |  4498 | `	}` |
|       98 |  4499 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  4500 | `	break;` |
|        - |  4501 | `					   }` |
|        - |  4502 | `/*` |
|        - |  4503 | ` * LOAD_IDX: P1 P2 *` |
|        - |  4504 | ` *` |
|        - |  4505 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  4506 | ` * from the stack.` |
|        - |  4507 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  4508 | ` * instead.` |
|        - |  4509 | ` */` |
|   247170 |  4510 | `case PH7_OP_LOAD_IDX: {` |
|   494386 |  4511 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   494386 |  4512 | `	ph7_hashmap *pMap = 0;` |
|        - |  4513 | `	ph7_value *pIdx;` |
|   494386 |  4514 | `	pIdx = 0;` |
|   494386 |  4515 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  4516 | `		if( !pInstr->iP2){` |
|        - |  4517 | `			/* No available index,load NULL */` |
|      ! 0 |  4518 | `			if( pTos >= pStack ){` |
|      ! 0 |  4519 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4520 | `			}else{` |
|        - |  4521 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  4522 | `				pTos++;` |
|      ! 0 |  4523 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4524 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  4525 | `			}` |
|        - |  4526 | `			/* Emit a notice */` |
|      ! 0 |  4527 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  4528 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  4529 | `			break;` |
|        - |  4530 | `		}` |
|      ! 0 |  4531 | `	}else{` |
|   494386 |  4532 | `		pIdx = pTos;` |
|   494386 |  4533 | `		pTos--;` |
|        - |  4534 | `	}` |
|   494386 |  4535 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  4536 | `		/* String access */` |
|   385116 |  4537 | `		if( pIdx ){` |
|        - |  4538 | `			sxu32 nOfft;` |
|   385116 |  4539 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  4540 | `				/* Force an int cast */` |
|      ! 0 |  4541 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4542 | `			}` |
|   385116 |  4543 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   385116 |  4544 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  4545 | `				/* Invalid offset,load null */` |
|      ! 0 |  4546 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4547 | `			}else{` |
|   385116 |  4548 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   385116 |  4549 | `				int c = zData[nOfft];` |
|   385116 |  4550 | `				PH7_MemObjRelease(pTos);` |
|   385116 |  4551 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   385116 |  4552 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  4553 | `			}` |
|   192581 |  4554 | `		}else{` |
|        - |  4555 | `			/* No available index,load NULL */` |
|      ! 0 |  4556 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4557 | `		}` |
|   385116 |  4558 | `		break;` |
|        - |  4559 | `	}` |
|   109272 |  4560 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  4561 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4562 | `			ph7_value *pObj;` |
|        3 |  4563 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4564 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  4565 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  4566 | `			}` |
|        1 |  4567 | `		}` |
|        1 |  4568 | `	}` |
|   109272 |  4569 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   109272 |  4570 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   109272 |  4571 | `		if( pInstr->iP2 == 1 ){` |
|        - |  4572 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  4573 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  4574 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  4575 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      883 |  4576 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      441 |  4577 | `		}` |
|        - |  4578 | `		/* Point to the hashmap */` |
|   109272 |  4579 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   109272 |  4580 | `		if( pIdx ){` |
|        - |  4581 | `			/* Load the desired entry */` |
|   109272 |  4582 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    54635 |  4583 | `		}` |
|   109272 |  4584 | `		if( pInstr->iP2 == 3 ){` |
|        - |  4585 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  4586 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  4587 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  4588 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  4589 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  4590 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  4591 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  4592 | `			 * correct for the outermost write. */` |
|       19 |  4593 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  4594 | `			if( !needWrite && pNode ){` |
|       13 |  4595 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  4596 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  4597 | `					needWrite = 1;` |
|        3 |  4598 | `				}` |
|        6 |  4599 | `			}` |
|       19 |  4600 | `			if( needWrite ){` |
|       13 |  4601 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  4602 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  4603 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  4604 | `					 * into the new map's storage. */` |
|        7 |  4605 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  4606 | `					if( pIdx ){` |
|        7 |  4607 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  4608 | `					}` |
|        3 |  4609 | `				}` |
|        6 |  4610 | `			}` |
|        9 |  4611 | `		}` |
|   109272 |  4612 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) ){` |
|        - |  4613 | `			/* Create a new empty entry */` |
|      273 |  4614 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  4615 | `			if( rc == SXRET_OK ){` |
|        - |  4616 | `				/* Point to the last inserted entry */` |
|      273 |  4617 | `				pNode = pMap->pLast;` |
|      136 |  4618 | `			}` |
|      136 |  4619 | `		}` |
|    54635 |  4620 | `	}` |
|   109272 |  4621 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  4622 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  4623 | `		char zMsg[128];` |
|      ! 0 |  4624 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4625 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4626 | `		}` |
|      ! 0 |  4627 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  4628 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4629 | `	}` |
|   109272 |  4630 | `	if( pIdx ){` |
|   109272 |  4631 | `		PH7_MemObjRelease(pIdx);` |
|    54635 |  4632 | `	}` |
|   109272 |  4633 | `	if( rc == SXRET_OK ){` |
|        - |  4634 | `		/* Load entry contents */` |
|    48528 |  4635 | `		if( pMap->iRef < 2 ){` |
|        - |  4636 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  4637 | `			 * of the entry value,rather than pointing to it.` |
|        - |  4638 | `			 */` |
|       24 |  4639 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  4640 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  4641 | `		}else{` |
|    48506 |  4642 | `			pTos->nIdx = pNode->nValIdx;` |
|    48506 |  4643 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    48506 |  4644 | `			PH7_HashmapUnref(pMap);` |
|        - |  4645 | `		}` |
|    24265 |  4646 | `	}else{` |
|        - |  4647 | `		/* No such entry,load NULL */` |
|    60746 |  4648 | `		PH7_MemObjRelease(pTos);` |
|    60746 |  4649 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  4650 | `	}` |
|   109272 |  4651 | `	break;` |
|        - |  4652 | `					  }` |
|        - |  4653 | `/*` |
|        - |  4654 | ` * LOAD_CLOSURE * * P3` |
|        - |  4655 | ` *` |
|        - |  4656 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  4657 | ` * name in the stack.` |
|        - |  4658 | ` */` |
|       47 |  4659 | `case PH7_OP_LOAD_CLOSURE:{` |
|       96 |  4660 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       96 |  4661 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  4662 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  4663 | `		ph7_vm_func *pClosure;` |
|        - |  4664 | `		char *zName;` |
|        - |  4665 | `		sxu32 mLen;` |
|        - |  4666 | `		sxu32 n;` |
|        - |  4667 | `		/* Create a new VM function */` |
|       96 |  4668 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  4669 | `		/* Generate an unique closure name */` |
|       96 |  4670 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|       96 |  4671 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  4672 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  4673 | `			goto Abort;` |
|        - |  4674 | `		}` |
|       96 |  4675 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|       96 |  4676 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  4677 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  4678 | `		}` |
|        - |  4679 | `		/* Zero the stucture */` |
|       96 |  4680 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  4681 | `		/* Perform a structure assignment on read-only items */` |
|       96 |  4682 | `		pClosure->aArgs = pFunc->aArgs;` |
|       96 |  4683 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|       96 |  4684 | `		pClosure->aStatic = pFunc->aStatic;` |
|       96 |  4685 | `		pClosure->iFlags = pFunc->iFlags;` |
|       96 |  4686 | `		pClosure->pUserData = pFunc->pUserData;` |
|       96 |  4687 | `		pClosure->sSignature = pFunc->sSignature;` |
|       96 |  4688 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|       96 |  4689 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|       96 |  4690 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|       96 |  4691 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|       96 |  4692 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  4693 | `		/* Register the closure */` |
|       96 |  4694 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  4695 | `		/* Set up closure environment */` |
|       96 |  4696 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|       96 |  4697 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      256 |  4698 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  4699 | `			ph7_value *pValue;` |
|      162 |  4700 | `			pEnv = &aEnv[n];` |
|      162 |  4701 | `			sEnv.sName  = pEnv->sName;` |
|      162 |  4702 | `			sEnv.iFlags = pEnv->iFlags;` |
|      162 |  4703 | `			sEnv.nIdx = SXU32_HIGH;` |
|      162 |  4704 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      162 |  4705 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  4706 | `				/* Pass by reference */` |
|      ! 0 |  4707 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  4708 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  4709 | `					);` |
|      ! 0 |  4710 | `			}` |
|        - |  4711 | `			/* Standard pass by value */` |
|      162 |  4712 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      162 |  4713 | `			if( pValue ){` |
|        - |  4714 | `				/* Copy imported value */` |
|       72 |  4715 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       35 |  4716 | `			}` |
|        - |  4717 | `			/* Insert the imported variable */` |
|      162 |  4718 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       82 |  4719 | `		}` |
|        - |  4720 | `		/* Finally,load the closure name on the stack */` |
|       96 |  4721 | `		pTos++;` |
|       96 |  4722 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       47 |  4723 | `	}` |
|       96 |  4724 | `	break;` |
|        - |  4725 | `						 }` |
|        - |  4726 | `/*` |
|        - |  4727 | ` * STORE * P2 P3` |
|        - |  4728 | ` *` |
|        - |  4729 | ` * Perform a store (Assignment) operation.` |
|        - |  4730 | ` */` |
|   137908 |  4731 | `case PH7_OP_STORE: {` |
|        - |  4732 | `	ph7_value *pObj;` |
|        - |  4733 | `	SyString sName;` |
|        - |  4734 | `#ifdef UNTRUST` |
|        - |  4735 | `	if( pTos < pStack ){` |
|        - |  4736 | `		goto Abort;` |
|        - |  4737 | `	}` |
|        - |  4738 | `#endif` |
|   275818 |  4739 | `	if( pInstr->iP2 ){` |
|        - |  4740 | `		sxu32 nIdx;` |
|        - |  4741 | `		sxi32 rcT;` |
|        - |  4742 | `		/* Member store operation */` |
|     4786 |  4743 | `		nIdx = pTos->nIdx;` |
|     4786 |  4744 | `		VmPopOperand(&pTos,1);` |
|     4786 |  4745 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  4746 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4747 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  4748 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  4749 | `		}else{` |
|        - |  4750 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  4751 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     4782 |  4752 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     4782 |  4753 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  4754 | `				goto Abort;` |
|        - |  4755 | `			}` |
|     4782 |  4756 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  4757 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  4758 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  4759 | `				 * propagate out of the VM loop. */` |
|       37 |  4760 | `				VmPopOperand(&pTos,1);` |
|        - |  4761 | `				{` |
|       37 |  4762 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       37 |  4763 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       37 |  4764 | `						pc = pFrm2->iExceptionJump - 1;` |
|   137927 |  4765 | `						break;` |
|        - |  4766 | `					}` |
|        - |  4767 | `				}` |
|      ! 0 |  4768 | `				goto Exception;` |
|        - |  4769 | `			}` |
|        - |  4770 | `			/* Point to the desired memory object */` |
|     4746 |  4771 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     4746 |  4772 | `			if( pObj ){` |
|        - |  4773 | `				/* Perform the store operation */` |
|     4746 |  4774 | `				PH7_MemObjStore(pTos,pObj);` |
|     2372 |  4775 | `			}` |
|        - |  4776 | `		}` |
|     4750 |  4777 | `		break;` |
|   271034 |  4778 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  4779 | `		/* Take the variable name from the next on the stack */` |
|        7 |  4780 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4781 | `			/* Force a string cast */` |
|      ! 0 |  4782 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4783 | `		}` |
|        7 |  4784 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  4785 | `		pTos--;` |
|        - |  4786 | `#ifdef UNTRUST` |
|        - |  4787 | `		if( pTos < pStack  ){` |
|        - |  4788 | `			goto Abort;` |
|        - |  4789 | `		}` |
|        - |  4790 | `#endif` |
|        4 |  4791 | `	}else{` |
|   271028 |  4792 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4793 | `	}` |
|        - |  4794 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   271034 |  4795 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   271034 |  4796 | `	if( pObj == 0 ){` |
|      ! 0 |  4797 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4798 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4799 | `		goto Abort;` |
|        - |  4800 | `	}` |
|   271034 |  4801 | `	if( !pInstr->p3 ){` |
|        7 |  4802 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  4803 | `	}` |
|        - |  4804 | `	/* Perform the store operation */` |
|   271034 |  4805 | `	PH7_MemObjStore(pTos,pObj);` |
|   271034 |  4806 | `	break;` |
|        - |  4807 | `				   }` |
|        - |  4808 | `/*` |
|        - |  4809 | ` * STORE_IDX:   P1 * P3` |
|        - |  4810 | ` * STORE_IDX_R: P1 * P3` |
|        - |  4811 | ` *` |
|        - |  4812 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  4813 | ` */` |
|    93634 |  4814 | `case PH7_OP_STORE_IDX:` |
|        - |  4815 | `case PH7_OP_STORE_IDX_REF: {` |
|   187270 |  4816 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  4817 | `	ph7_value *pKey;` |
|        - |  4818 | `	sxu32 nIdx;` |
|   187270 |  4819 | `	if( pInstr->iP1 ){` |
|        - |  4820 | `		/* Key is next on stack */` |
|    62162 |  4821 | `		pKey = pTos;` |
|    62162 |  4822 | `		pTos--;` |
|    31082 |  4823 | `	}else{` |
|   125110 |  4824 | `		pKey = 0;` |
|        - |  4825 | `	}` |
|   187270 |  4826 | `	nIdx = pTos->nIdx;` |
|   187270 |  4827 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  4828 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  4829 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  4830 | `		 * checking true sharing count, then re-add after separation. */` |
|   187218 |  4831 | `		if( nIdx != SXU32_HIGH ){` |
|   187218 |  4832 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   280826 |  4833 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   187218 |  4834 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4835 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  4836 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  4837 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  4838 | `				 * refcounts if the backing array was already separated. */` |
|   187218 |  4839 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   187218 |  4840 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   187218 |  4841 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   187218 |  4842 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   187218 |  4843 | `					pTos->x.pOther = pMap;` |
|    93610 |  4844 | `				}else{` |
|        - |  4845 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  4846 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  4847 | `					pMap = pCur;` |
|        - |  4848 | `				}` |
|    93610 |  4849 | `			}else{` |
|      ! 0 |  4850 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4851 | `			}` |
|    93610 |  4852 | `		}else{` |
|      ! 0 |  4853 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4854 | `		}` |
|   187218 |  4855 | `		if( pMap->iRef < 2 ){` |
|        - |  4856 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  4857 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  4858 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  4859 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  4860 | `			pMap->iRef = 2;` |
|      ! 0 |  4861 | `		}` |
|    93610 |  4862 | `	}else{` |
|        - |  4863 | `		ph7_value *pObj;` |
|       53 |  4864 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  4865 | `		if( pObj == 0 ){` |
|      ! 0 |  4866 | `			if( pKey ){` |
|      ! 0 |  4867 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  4868 | `			}` |
|      ! 0 |  4869 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4870 | `			break;` |
|        - |  4871 | `		}` |
|        - |  4872 | `		/* Phase#1: Load the array */` |
|       53 |  4873 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  4874 | `			VmPopOperand(&pTos,1);` |
|       53 |  4875 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  4876 | `				/* Force a string cast */` |
|      ! 0 |  4877 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  4878 | `			}` |
|       53 |  4879 | `			if( pKey == 0 ){` |
|        - |  4880 | `				/* Append string */` |
|        3 |  4881 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  4882 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  4883 | `				}` |
|        2 |  4884 | `			}else{` |
|        - |  4885 | `				sxu32 nOfft;` |
|       51 |  4886 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  4887 | `					/* Force an int cast */` |
|       51 |  4888 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  4889 | `				}` |
|       51 |  4890 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  4891 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  4892 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  4893 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  4894 | `					zData[nOfft] = zBlob[0];` |
|       26 |  4895 | `				}else{` |
|      ! 0 |  4896 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  4897 | `						/* Perform an append operation */` |
|      ! 0 |  4898 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  4899 | `					}` |
|        - |  4900 | `				}` |
|        - |  4901 | `			}` |
|       53 |  4902 | `			if( pKey ){` |
|       51 |  4903 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  4904 | `			}` |
|       53 |  4905 | `			break;` |
|      ! 0 |  4906 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  4907 | `			/* Force a hashmap cast  */` |
|      ! 0 |  4908 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  4909 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  4910 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  4911 | `				goto Abort;` |
|        - |  4912 | `			}` |
|      ! 0 |  4913 | `		}` |
|        - |  4914 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  4915 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  4916 | `	}` |
|   187218 |  4917 | `	VmPopOperand(&pTos,1);` |
|        - |  4918 | `	/* Phase#2: Perform the insertion */` |
|   187218 |  4919 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  4920 | `		/* Insertion by reference */` |
|       15 |  4921 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  4922 | `	}else{` |
|   187204 |  4923 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  4924 | `	}` |
|   187218 |  4925 | `	if( pKey ){` |
|    62112 |  4926 | `		PH7_MemObjRelease(pKey);` |
|    31055 |  4927 | `	}` |
|   187218 |  4928 | `	break;` |
|        - |  4929 | `					   }` |
|        - |  4930 | `/*` |
|        - |  4931 | ` * INCR: P1 * *` |
|        - |  4932 | ` *` |
|        - |  4933 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  4934 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  4935 | ` * the stack and increment after that.` |
|        - |  4936 | ` */` |
|   166533 |  4937 | `case PH7_OP_INCR:` |
|        - |  4938 | `#ifdef UNTRUST` |
|        - |  4939 | `	if( pTos < pStack ){` |
|        - |  4940 | `		goto Abort;` |
|        - |  4941 | `	}` |
|        - |  4942 | `#endif` |
|   333112 |  4943 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   333112 |  4944 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4945 | `			ph7_value *pObj;` |
|   333112 |  4946 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  4947 | `				/* Force a numeric cast */` |
|   333112 |  4948 | `				PH7_MemObjToNumeric(pObj);` |
|   333112 |  4949 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4950 | `					pObj->rVal++;` |
|        - |  4951 | `					/* Try to get an integer representation */` |
|      ! 0 |  4952 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4953 | `				}else{` |
|   333112 |  4954 | `					pObj->x.iVal++;` |
|   333112 |  4955 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4956 | `				}` |
|   333112 |  4957 | `				if( pInstr->iP1 ){` |
|        - |  4958 | `					/* Pre-icrement */` |
|       77 |  4959 | `					PH7_MemObjStore(pObj,pTos);` |
|       38 |  4960 | `				}` |
|   166577 |  4961 | `			}` |
|   166579 |  4962 | `		}else{` |
|      ! 0 |  4963 | `			if( pInstr->iP1 ){` |
|        - |  4964 | `				/* Force a numeric cast */` |
|      ! 0 |  4965 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  4966 | `				/* Pre-increment */` |
|      ! 0 |  4967 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4968 | `					pTos->rVal++;` |
|        - |  4969 | `					/* Try to get an integer representation */` |
|      ! 0 |  4970 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4971 | `				}else{` |
|      ! 0 |  4972 | `					pTos->x.iVal++;` |
|      ! 0 |  4973 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4974 | `				}` |
|      ! 0 |  4975 | `			}` |
|        - |  4976 | `		}` |
|   166577 |  4977 | `	}` |
|   333112 |  4978 | `	break;` |
|        - |  4979 | `/*` |
|        - |  4980 | ` * DECR: P1 * *` |
|        - |  4981 | ` *` |
|        - |  4982 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  4983 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  4984 | ` * and decrement after that.` |
|        - |  4985 | ` */` |
|        2 |  4986 | `case PH7_OP_DECR:` |
|        - |  4987 | `#ifdef UNTRUST` |
|        - |  4988 | `	if( pTos < pStack ){` |
|        - |  4989 | `		goto Abort;` |
|        - |  4990 | `	}` |
|        - |  4991 | `#endif` |
|        5 |  4992 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  4993 | `		/* Force a numeric cast */` |
|        5 |  4994 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  4995 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4996 | `			ph7_value *pObj;` |
|        5 |  4997 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  4998 | `				/* Force a numeric cast */` |
|        5 |  4999 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  5000 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5001 | `					pObj->rVal--;` |
|        - |  5002 | `					/* Try to get an integer representation */` |
|      ! 0 |  5003 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5004 | `				}else{` |
|        5 |  5005 | `					pObj->x.iVal--;` |
|        5 |  5006 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5007 | `				}` |
|        5 |  5008 | `				if( pInstr->iP1 ){` |
|        - |  5009 | `					/* Pre-icrement */` |
|      ! 0 |  5010 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  5011 | `				}` |
|        2 |  5012 | `			}` |
|        3 |  5013 | `		}else{` |
|      ! 0 |  5014 | `			if( pInstr->iP1 ){` |
|        - |  5015 | `				/* Pre-increment */` |
|      ! 0 |  5016 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5017 | `					pTos->rVal--;` |
|        - |  5018 | `					/* Try to get an integer representation */` |
|      ! 0 |  5019 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5020 | `				}else{` |
|      ! 0 |  5021 | `					pTos->x.iVal--;` |
|      ! 0 |  5022 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5023 | `				}` |
|      ! 0 |  5024 | `			}` |
|        - |  5025 | `		}` |
|        2 |  5026 | `	}` |
|        5 |  5027 | `	break;` |
|        - |  5028 | `/*` |
|        - |  5029 | ` * UMINUS: * * *` |
|        - |  5030 | ` *` |
|        - |  5031 | ` * Perform a unary minus operation.` |
|        - |  5032 | ` */` |
|    28614 |  5033 | `case PH7_OP_UMINUS:` |
|        - |  5034 | `#ifdef UNTRUST` |
|        - |  5035 | `	if( pTos < pStack ){` |
|        - |  5036 | `		goto Abort;` |
|        - |  5037 | `	}` |
|        - |  5038 | `#endif` |
|        - |  5039 | `	/* Force a numeric (integer,real or both) cast */` |
|    57230 |  5040 | `	PH7_MemObjToNumeric(pTos);` |
|    57230 |  5041 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  5042 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  5043 | `	}` |
|    57230 |  5044 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    57200 |  5045 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    28599 |  5046 | `	}` |
|    57230 |  5047 | `	break;` |
|        - |  5048 | `/*` |
|        - |  5049 | ` * UPLUS: * * *` |
|        - |  5050 | ` *` |
|        - |  5051 | ` * Perform a unary plus operation.` |
|        - |  5052 | ` */` |
|       18 |  5053 | `case PH7_OP_UPLUS:` |
|        - |  5054 | `#ifdef UNTRUST` |
|        - |  5055 | `	if( pTos < pStack ){` |
|        - |  5056 | `		goto Abort;` |
|        - |  5057 | `	}` |
|        - |  5058 | `#endif` |
|        - |  5059 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  5060 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  5061 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5062 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  5063 | `	}` |
|       37 |  5064 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  5065 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  5066 | `	}` |
|       37 |  5067 | `	break;` |
|        - |  5068 | `/*` |
|        - |  5069 | ` * OP_LNOT: * * *` |
|        - |  5070 | ` *` |
|        - |  5071 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  5072 | ` * with its complement.` |
|        - |  5073 | ` */` |
|    44090 |  5074 | `case PH7_OP_LNOT:` |
|        - |  5075 | `#ifdef UNTRUST` |
|        - |  5076 | `	if( pTos < pStack ){` |
|        - |  5077 | `		goto Abort;` |
|        - |  5078 | `	}` |
|        - |  5079 | `#endif` |
|        - |  5080 | `	/* Force a boolean cast */` |
|    88226 |  5081 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       23 |  5082 | `		PH7_MemObjToBool(pTos);` |
|       11 |  5083 | `	}` |
|    88226 |  5084 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    88226 |  5085 | `	break;` |
|        - |  5086 | `/*` |
|        - |  5087 | ` * OP_BITNOT: * * *` |
|        - |  5088 | ` *` |
|        - |  5089 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  5090 | ` * with its ones-complement.` |
|        - |  5091 | ` */` |
|       15 |  5092 | `case PH7_OP_BITNOT:` |
|        - |  5093 | `#ifdef UNTRUST` |
|        - |  5094 | `	if( pTos < pStack ){` |
|        - |  5095 | `		goto Abort;` |
|        - |  5096 | `	}` |
|        - |  5097 | `#endif` |
|        - |  5098 | `	/* Force an integer cast */` |
|       32 |  5099 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5100 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5101 | `	}` |
|       32 |  5102 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       32 |  5103 | `	break;` |
|        - |  5104 | `/* OP_MUL * * *` |
|        - |  5105 | ` * OP_MUL_STORE * * *` |
|        - |  5106 | ` *` |
|        - |  5107 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  5108 | ` * and push the result back onto the stack.` |
|        - |  5109 | ` */` |
|     1287 |  5110 | `case PH7_OP_MUL:` |
|        - |  5111 | `case PH7_OP_MUL_STORE: {` |
|     2576 |  5112 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5113 | `	/* Force the operand to be numeric */` |
|        - |  5114 | `#ifdef UNTRUST` |
|        - |  5115 | `	if( pNos < pStack ){` |
|        - |  5116 | `		goto Abort;` |
|        - |  5117 | `	}` |
|        - |  5118 | `#endif` |
|     2576 |  5119 | `	PH7_MemObjToNumeric(pTos);` |
|     2576 |  5120 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5121 | `	/* Perform the requested operation */` |
|     2576 |  5122 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5123 | `		/* Floating point arithemic */` |
|        - |  5124 | `		ph7_real a,b,r;` |
|       19 |  5125 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  5126 | `			PH7_MemObjToReal(pTos);` |
|        4 |  5127 | `		}` |
|       19 |  5128 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  5129 | `			PH7_MemObjToReal(pNos);` |
|        3 |  5130 | `		}` |
|       19 |  5131 | `		a = pNos->rVal;` |
|       19 |  5132 | `		b = pTos->rVal;` |
|       19 |  5133 | `		r = a * b;` |
|        - |  5134 | `		/* Push the result */` |
|       19 |  5135 | `		pNos->rVal = r;` |
|       19 |  5136 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5137 | `		/* Try to get an integer representation */` |
|       19 |  5138 | `		PH7_MemObjTryInteger(pNos);` |
|       10 |  5139 | `	}else{` |
|        - |  5140 | `		/* Integer arithmetic */` |
|        - |  5141 | `		sxi64 a,b,r;` |
|     2558 |  5142 | `		a = pNos->x.iVal;` |
|     2558 |  5143 | `		b = pTos->x.iVal;` |
|     2558 |  5144 | `		r = a * b;` |
|        - |  5145 | `		/* Push the result */` |
|     2558 |  5146 | `		pNos->x.iVal = r;` |
|     2558 |  5147 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5148 | `	}` |
|     2576 |  5149 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  5150 | `		ph7_value *pObj;` |
|       32 |  5151 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5152 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  5153 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  5154 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  5155 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  5156 | `		}` |
|       15 |  5157 | `	}` |
|     2576 |  5158 | `	VmPopOperand(&pTos,1);` |
|     2576 |  5159 | `	break;` |
|        - |  5160 | `				 }` |
|        - |  5161 | `/* OP_POW * * *` |
|        - |  5162 | ` * OP_POW_STORE * * *` |
|        - |  5163 | ` *` |
|        - |  5164 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - |  5165 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - |  5166 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - |  5167 | ` * fits in sxi64; otherwise the result is a double.` |
|        - |  5168 | ` */` |
|       66 |  5169 | `case PH7_OP_POW:` |
|        - |  5170 | `case PH7_OP_POW_STORE: {` |
|      133 |  5171 | `	ph7_value *pNos = &pTos[-1];` |
|      133 |  5172 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|        - |  5173 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|        - |  5174 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|        - |  5175 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|        - |  5176 | `	 */` |
|      133 |  5177 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|      133 |  5178 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|        - |  5179 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  5180 | `	int bBothInt;` |
|      133 |  5181 | `	int usedInt = 0;` |
|        - |  5182 | `	ph7_real a, b, r;` |
|        - |  5183 | `#endif` |
|      133 |  5184 | `	sxi64 base_i = 0, exp_i = 0;` |
|        - |  5185 | `#ifdef UNTRUST` |
|        - |  5186 | `	if( pNos < pStack ){` |
|        - |  5187 | `		goto Abort;` |
|        - |  5188 | `	}` |
|        - |  5189 | `#endif` |
|      133 |  5190 | `	PH7_MemObjToNumeric(pTos);` |
|      133 |  5191 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5192 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      261 |  5193 | `	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&` |
|      128 |  5194 | `	           ((pNos->iFlags & MEMOBJ_REAL) == 0);` |
|      133 |  5195 | `	if( bBothInt ){` |
|      123 |  5196 | `		base_i = pBase->x.iVal;` |
|      123 |  5197 | `		exp_i  = pExp->x.iVal;` |
|       61 |  5198 | `	}` |
|      133 |  5199 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|      125 |  5200 | `		PH7_MemObjToReal(pBase);` |
|       62 |  5201 | `	}` |
|      133 |  5202 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|      131 |  5203 | `		PH7_MemObjToReal(pExp);` |
|       65 |  5204 | `	}` |
|      133 |  5205 | `	a = pBase->rVal;` |
|      133 |  5206 | `	b = pExp->rVal;` |
|      133 |  5207 | `	r = pow(a, b);` |
|        - |  5208 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|        - |  5209 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|        - |  5210 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|        - |  5211 | `	 * representable as double but not as signed int64. */` |
|      133 |  5212 | `	if( bBothInt && exp_i >= 0 ){` |
|      117 |  5213 | `		sxi64 result_i = 1;` |
|      117 |  5214 | `		sxi64 cur_base = base_i;` |
|      117 |  5215 | `		sxi64 cur_exp  = exp_i;` |
|      117 |  5216 | `		int overflow = 0;` |
|      401 |  5217 | `		while( cur_exp > 0 ){` |
|      289 |  5218 | `			if( cur_exp & 1 ){` |
|      189 |  5219 | `				if( VmMulOverflow64(result_i, cur_base, &result_i) ){` |
|        3 |  5220 | `					overflow = 1;` |
|        3 |  5221 | `					break;` |
|        - |  5222 | `				}` |
|       93 |  5223 | `			}` |
|      287 |  5224 | `			cur_exp >>= 1;` |
|      287 |  5225 | `			if( cur_exp > 0 ){` |
|      181 |  5226 | `				if( VmMulOverflow64(cur_base, cur_base, &cur_base) ){` |
|        3 |  5227 | `					overflow = 1;` |
|        3 |  5228 | `					break;` |
|        - |  5229 | `				}` |
|       89 |  5230 | `			}` |
|        1 |  5231 | `		}` |
|      117 |  5232 | `		if( !overflow ){` |
|      113 |  5233 | `			pNos->x.iVal = result_i;` |
|      113 |  5234 | `			MemObjSetType(pNos, MEMOBJ_INT);` |
|      113 |  5235 | `			usedInt = 1;` |
|       56 |  5236 | `		}` |
|       58 |  5237 | `	}` |
|      133 |  5238 | `	if( !usedInt ){` |
|       21 |  5239 | `		pNos->rVal = r;` |
|       21 |  5240 | `		MemObjSetType(pNos, MEMOBJ_REAL);` |
|       10 |  5241 | `	}` |
|        - |  5242 | `#else` |
|        - |  5243 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|        - |  5244 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|        - |  5245 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|        - |  5246 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|        - |  5247 | `	 * represented. */` |
|        - |  5248 | `	base_i = pBase->x.iVal;` |
|        - |  5249 | `	exp_i  = pExp->x.iVal;` |
|        - |  5250 | `	{` |
|        - |  5251 | `		sxi64 result_i = 1;` |
|        - |  5252 | `		sxi64 cur_base = base_i;` |
|        - |  5253 | `		sxi64 cur_exp  = exp_i;` |
|        - |  5254 | `		if( cur_exp < 0 ){` |
|        - |  5255 | `			result_i = 0;` |
|        - |  5256 | `		}else{` |
|        - |  5257 | `			while( cur_exp > 0 ){` |
|        - |  5258 | `				if( cur_exp & 1 ){` |
|        - |  5259 | `					result_i *= cur_base;` |
|        - |  5260 | `				}` |
|        - |  5261 | `				cur_exp >>= 1;` |
|        - |  5262 | `				if( cur_exp > 0 ){` |
|        - |  5263 | `					cur_base *= cur_base;` |
|        - |  5264 | `				}` |
|        - |  5265 | `			}` |
|        - |  5266 | `		}` |
|        - |  5267 | `		pNos->x.iVal = result_i;` |
|        - |  5268 | `		MemObjSetType(pNos, MEMOBJ_INT);` |
|        - |  5269 | `	}` |
|        - |  5270 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      133 |  5271 | `	if( bStore ){` |
|        - |  5272 | `		ph7_value *pObj;` |
|       23 |  5273 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5274 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       23 |  5275 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       23 |  5276 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       23 |  5277 | `			PH7_MemObjStore(pNos,pObj);` |
|       11 |  5278 | `		}` |
|       11 |  5279 | `	}` |
|      133 |  5280 | `	VmPopOperand(&pTos,1);` |
|      133 |  5281 | `	break;` |
|        - |  5282 | `				 }` |
|        - |  5283 | `/* OP_ADD * * *` |
|        - |  5284 | ` *` |
|        - |  5285 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5286 | ` * and push the result back onto the stack.` |
|        - |  5287 | ` */` |
|      513 |  5288 | `case PH7_OP_ADD:{` |
|     1028 |  5289 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5290 | `#ifdef UNTRUST` |
|        - |  5291 | `	if( pNos < pStack ){` |
|        - |  5292 | `		goto Abort;` |
|        - |  5293 | `	}` |
|        - |  5294 | `#endif` |
|        - |  5295 | `	/* Perform the addition */` |
|     1028 |  5296 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|     1028 |  5297 | `	VmPopOperand(&pTos,1);` |
|     1028 |  5298 | `	break;` |
|        - |  5299 | `				}` |
|        - |  5300 | `/*` |
|        - |  5301 | ` * OP_ADD_STORE * * *` |
|        - |  5302 | ` *` |
|        - |  5303 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5304 | ` * and push the result back onto the stack.` |
|        - |  5305 | ` */` |
|      502 |  5306 | `case PH7_OP_ADD_STORE:{` |
|     1006 |  5307 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5308 | `	ph7_value *pObj;` |
|        - |  5309 | `	sxu32 nIdx;` |
|        - |  5310 | `#ifdef UNTRUST` |
|        - |  5311 | `	if( pNos < pStack ){` |
|        - |  5312 | `		goto Abort;` |
|        - |  5313 | `	}` |
|        - |  5314 | `#endif` |
|        - |  5315 | `	/* Perform the addition */` |
|     1006 |  5316 | `	nIdx = pTos->nIdx;` |
|     1006 |  5317 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  5318 | `	/* Peform the store operation */` |
|     1006 |  5319 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  5320 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1006 |  5321 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1006 |  5322 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1006 |  5323 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  5324 | `	}` |
|        - |  5325 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1006 |  5326 | `	PH7_MemObjStore(pTos,pNos);` |
|     1006 |  5327 | `	VmPopOperand(&pTos,1);` |
|     1006 |  5328 | `	break;` |
|        - |  5329 | `				}` |
|        - |  5330 | `/* OP_SUB * * *` |
|        - |  5331 | ` *` |
|        - |  5332 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5333 | ` * first (what was next on the stack) from the second (the` |
|        - |  5334 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5335 | ` */` |
|      348 |  5336 | `case PH7_OP_SUB: {` |
|      698 |  5337 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5338 | `#ifdef UNTRUST` |
|        - |  5339 | `	if( pNos < pStack ){` |
|        - |  5340 | `		goto Abort;` |
|        - |  5341 | `	}` |
|        - |  5342 | `#endif` |
|      698 |  5343 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5344 | `		/* Floating point arithemic */` |
|        - |  5345 | `		ph7_real a,b,r;` |
|       95 |  5346 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5347 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5348 | `		}` |
|       95 |  5349 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5350 | `			PH7_MemObjToReal(pNos);` |
|        2 |  5351 | `		}` |
|       95 |  5352 | `		a = pNos->rVal;` |
|       95 |  5353 | `		b = pTos->rVal;` |
|       95 |  5354 | `		r = a - b;` |
|        - |  5355 | `		/* Push the result */` |
|       95 |  5356 | `		pNos->rVal = r;` |
|       95 |  5357 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5358 | `		/* Try to get an integer representation */` |
|       95 |  5359 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  5360 | `	}else{` |
|        - |  5361 | `		/* Integer arithmetic */` |
|        - |  5362 | `		sxi64 a,b,r;` |
|      604 |  5363 | `		a = pNos->x.iVal;` |
|      604 |  5364 | `		b = pTos->x.iVal;` |
|      604 |  5365 | `		r = a - b;` |
|        - |  5366 | `		/* Push the result */` |
|      604 |  5367 | `		pNos->x.iVal = r;` |
|      604 |  5368 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5369 | `	}` |
|      698 |  5370 | `	VmPopOperand(&pTos,1);` |
|      698 |  5371 | `	break;` |
|        - |  5372 | `				 }` |
|        - |  5373 | `/* OP_SUB_STORE * * *` |
|        - |  5374 | ` *` |
|        - |  5375 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5376 | ` * first (what was next on the stack) from the second (the` |
|        - |  5377 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5378 | ` */` |
|        4 |  5379 | `case PH7_OP_SUB_STORE: {` |
|       10 |  5380 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5381 | `	ph7_value *pObj;` |
|        - |  5382 | `#ifdef UNTRUST` |
|        - |  5383 | `	if( pNos < pStack ){` |
|        - |  5384 | `		goto Abort;` |
|        - |  5385 | `	}` |
|        - |  5386 | `#endif` |
|       10 |  5387 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5388 | `		/* Floating point arithemic */` |
|        - |  5389 | `		ph7_real a,b,r;` |
|      ! 0 |  5390 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5391 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5392 | `		}` |
|      ! 0 |  5393 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5394 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  5395 | `		}` |
|      ! 0 |  5396 | `		a = pTos->rVal;` |
|      ! 0 |  5397 | `		b = pNos->rVal;` |
|      ! 0 |  5398 | `		r = a - b;` |
|        - |  5399 | `		/* Push the result */` |
|      ! 0 |  5400 | `		pNos->rVal = r;` |
|      ! 0 |  5401 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5402 | `		/* Try to get an integer representation */` |
|      ! 0 |  5403 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  5404 | `	}else{` |
|        - |  5405 | `		/* Integer arithmetic */` |
|        - |  5406 | `		sxi64 a,b,r;` |
|       10 |  5407 | `		a = pTos->x.iVal;` |
|       10 |  5408 | `		b = pNos->x.iVal;` |
|       10 |  5409 | `		r = a - b;` |
|        - |  5410 | `		/* Push the result */` |
|       10 |  5411 | `		pNos->x.iVal = r;` |
|       10 |  5412 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5413 | `	}` |
|       10 |  5414 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5415 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  5416 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  5417 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  5418 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  5419 | `	}` |
|       10 |  5420 | `	VmPopOperand(&pTos,1);` |
|       10 |  5421 | `	break;` |
|        - |  5422 | `				 }` |
|        - |  5423 |  |
|        - |  5424 | `/*` |
|        - |  5425 | ` * OP_MOD * * *` |
|        - |  5426 | ` *` |
|        - |  5427 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5428 | ` * first (what was next on the stack) from the second (the` |
|        - |  5429 | ` * top of the stack) and push the remainder after division` |
|        - |  5430 | ` * onto the stack.` |
|        - |  5431 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  5432 | ` */` |
|      308 |  5433 | `case PH7_OP_MOD:{` |
|      618 |  5434 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5435 | `	sxi64 a,b,r;` |
|        - |  5436 | `#ifdef UNTRUST` |
|        - |  5437 | `	if( pNos < pStack ){` |
|        - |  5438 | `		goto Abort;` |
|        - |  5439 | `	}` |
|        - |  5440 | `#endif` |
|        - |  5441 | `	/* Force the operands to be integer */` |
|      618 |  5442 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5443 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5444 | `	}` |
|      618 |  5445 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  5446 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  5447 | `	}` |
|        - |  5448 | `	/* Perform the requested operation */` |
|      618 |  5449 | `	a = pNos->x.iVal;` |
|      618 |  5450 | `	b = pTos->x.iVal;` |
|      618 |  5451 | `	if( b == 0 ){` |
|        3 |  5452 | `		r = 0;` |
|        3 |  5453 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  5454 | `		/* goto Abort; */` |
|        2 |  5455 | `	}else{` |
|      615 |  5456 | `		r = a%b;` |
|        - |  5457 | `	}` |
|        - |  5458 | `	/* Push the result */` |
|      618 |  5459 | `	pNos->x.iVal = r;` |
|      618 |  5460 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      618 |  5461 | `	VmPopOperand(&pTos,1);` |
|      618 |  5462 | `	break;` |
|        - |  5463 | `				}` |
|        - |  5464 | `/*` |
|        - |  5465 | ` * OP_MOD_STORE * * *` |
|        - |  5466 | ` *` |
|        - |  5467 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5468 | ` * first (what was next on the stack) from the second (the` |
|        - |  5469 | ` * top of the stack) and push the remainder after division` |
|        - |  5470 | ` * onto the stack.` |
|        - |  5471 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  5472 | ` */` |
|        1 |  5473 | `case PH7_OP_MOD_STORE: {` |
|        3 |  5474 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5475 | `	ph7_value *pObj;` |
|        - |  5476 | `	sxi64 a,b,r;` |
|        - |  5477 | `#ifdef UNTRUST` |
|        - |  5478 | `	if( pNos < pStack ){` |
|        - |  5479 | `		goto Abort;` |
|        - |  5480 | `	}` |
|        - |  5481 | `#endif` |
|        - |  5482 | `	/* Force the operands to be integer */` |
|        3 |  5483 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5484 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5485 | `	}` |
|        3 |  5486 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5487 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5488 | `	}` |
|        - |  5489 | `	/* Perform the requested operation */` |
|        3 |  5490 | `	a = pTos->x.iVal;` |
|        3 |  5491 | `	b = pNos->x.iVal;` |
|        3 |  5492 | `	if( b == 0 ){` |
|      ! 0 |  5493 | `		r = 0;` |
|      ! 0 |  5494 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  5495 | `		/* goto Abort; */` |
|      ! 0 |  5496 | `	}else{` |
|        3 |  5497 | `		r = a%b;` |
|        - |  5498 | `	}` |
|        - |  5499 | `	/* Push the result */` |
|        3 |  5500 | `	pNos->x.iVal = r;` |
|        3 |  5501 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  5502 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5503 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  5504 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  5505 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  5506 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  5507 | `	}` |
|        3 |  5508 | `	VmPopOperand(&pTos,1);` |
|        3 |  5509 | `	break;` |
|        - |  5510 | `				}` |
|        - |  5511 | `/*` |
|        - |  5512 | ` * OP_DIV * * *` |
|        - |  5513 | ` *` |
|        - |  5514 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5515 | ` * first (what was next on the stack) from the second (the` |
|        - |  5516 | ` * top of the stack) and push the result onto the stack.` |
|        - |  5517 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  5518 | ` */` |
|       31 |  5519 | `case PH7_OP_DIV:{` |
|       64 |  5520 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5521 | `	ph7_real a,b,r;` |
|        - |  5522 | `#ifdef UNTRUST` |
|        - |  5523 | `	if( pNos < pStack ){` |
|        - |  5524 | `		goto Abort;` |
|        - |  5525 | `	}` |
|        - |  5526 | `#endif` |
|        - |  5527 | `	/* Force the operands to be real */` |
|       64 |  5528 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       60 |  5529 | `		PH7_MemObjToReal(pTos);` |
|       29 |  5530 | `	}` |
|       64 |  5531 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       26 |  5532 | `		PH7_MemObjToReal(pNos);` |
|       12 |  5533 | `	}` |
|        - |  5534 | `	/* Perform the requested operation */` |
|       64 |  5535 | `	a = pNos->rVal;` |
|       64 |  5536 | `	b = pTos->rVal;` |
|       64 |  5537 | `	if( b == 0 ){` |
|        - |  5538 | `		/* Division by zero */` |
|        3 |  5539 | `		pNos->rVal = 0;` |
|        3 |  5540 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  5541 | `		/* goto Abort; */` |
|        2 |  5542 | `	}else{` |
|       61 |  5543 | `		r = a/b;` |
|        - |  5544 | `		/* Push the result */` |
|       61 |  5545 | `		pNos->rVal = r;` |
|       61 |  5546 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5547 | `		/* Try to get an integer representation */` |
|       61 |  5548 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  5549 | `	}` |
|       64 |  5550 | `	VmPopOperand(&pTos,1);` |
|       64 |  5551 | `	break;` |
|        - |  5552 | `				}` |
|        - |  5553 | `/*` |
|        - |  5554 | ` * OP_DIV_STORE * * *` |
|        - |  5555 | ` *` |
|        - |  5556 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5557 | ` * first (what was next on the stack) from the second (the` |
|        - |  5558 | ` * top of the stack) and push the result onto the stack.` |
|        - |  5559 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  5560 | ` */` |
|        2 |  5561 | `case PH7_OP_DIV_STORE:{` |
|        5 |  5562 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5563 | `	ph7_value *pObj;` |
|        - |  5564 | `	ph7_real a,b,r;` |
|        - |  5565 | `#ifdef UNTRUST` |
|        - |  5566 | `	if( pNos < pStack ){` |
|        - |  5567 | `		goto Abort;` |
|        - |  5568 | `	}` |
|        - |  5569 | `#endif` |
|        - |  5570 | `	/* Force the operands to be real */` |
|        5 |  5571 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5572 | `		PH7_MemObjToReal(pTos);` |
|        2 |  5573 | `	}` |
|        5 |  5574 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5575 | `		PH7_MemObjToReal(pNos);` |
|        2 |  5576 | `	}` |
|        - |  5577 | `	/* Perform the requested operation */` |
|        5 |  5578 | `	a = pTos->rVal;` |
|        5 |  5579 | `	b = pNos->rVal;` |
|        5 |  5580 | `	if( b == 0 ){` |
|        - |  5581 | `		/* Division by zero */` |
|      ! 0 |  5582 | `		r = 0;` |
|      ! 0 |  5583 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  5584 | `		/* goto Abort; */` |
|      ! 0 |  5585 | `	}else{` |
|        5 |  5586 | `		r = a/b;` |
|        - |  5587 | `		/* Push the result */` |
|        5 |  5588 | `		pNos->rVal = r;` |
|        5 |  5589 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5590 | `		/* Try to get an integer representation */` |
|        5 |  5591 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  5592 | `	}` |
|        5 |  5593 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5594 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  5595 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  5596 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  5597 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  5598 | `	}` |
|        5 |  5599 | `	VmPopOperand(&pTos,1);` |
|        5 |  5600 | `	break;` |
|        - |  5601 | `				}` |
|        - |  5602 | `/* OP_BAND * * *` |
|        - |  5603 | ` *` |
|        - |  5604 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5605 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  5606 | ` * two elements.` |
|        - |  5607 | `*/` |
|        - |  5608 | `/* OP_BOR * * *` |
|        - |  5609 | ` *` |
|        - |  5610 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5611 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  5612 | ` * two elements.` |
|        - |  5613 | ` */` |
|        - |  5614 | `/* OP_BXOR * * *` |
|        - |  5615 | ` *` |
|        - |  5616 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5617 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  5618 | ` * two elements.` |
|        - |  5619 | ` */` |
|       44 |  5620 | `case PH7_OP_BAND:` |
|        - |  5621 | `case PH7_OP_BOR:` |
|        - |  5622 | `case PH7_OP_BXOR:{` |
|       90 |  5623 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5624 | `	sxi64 a,b,r;` |
|        - |  5625 | `#ifdef UNTRUST` |
|        - |  5626 | `	if( pNos < pStack ){` |
|        - |  5627 | `		goto Abort;` |
|        - |  5628 | `	}` |
|        - |  5629 | `#endif` |
|        - |  5630 | `	/* Force the operands to be integer */` |
|       90 |  5631 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5632 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5633 | `	}` |
|       90 |  5634 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5635 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5636 | `	}` |
|        - |  5637 | `	/* Perform the requested operation */` |
|       90 |  5638 | `	a = pNos->x.iVal;` |
|       90 |  5639 | `	b = pTos->x.iVal;` |
|       90 |  5640 | `	switch(pInstr->iOp){` |
|        7 |  5641 | `	case PH7_OP_BOR_STORE:` |
|       15 |  5642 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  5643 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  5644 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       30 |  5645 | `	case PH7_OP_BAND_STORE:` |
|       30 |  5646 | `	case PH7_OP_BAND:` |
|       62 |  5647 | `	default:          r = a&b; break;` |
|        - |  5648 | `	}` |
|        - |  5649 | `	/* Push the result */` |
|       90 |  5650 | `	pNos->x.iVal = r;` |
|       90 |  5651 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       90 |  5652 | `	VmPopOperand(&pTos,1);` |
|       90 |  5653 | `	break;` |
|        - |  5654 | `				 }` |
|        - |  5655 | `/* OP_BAND_STORE * * *` |
|        - |  5656 | ` *` |
|        - |  5657 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5658 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  5659 | ` * two elements.` |
|        - |  5660 | `*/` |
|        - |  5661 | `/* OP_BOR_STORE * * *` |
|        - |  5662 | ` *` |
|        - |  5663 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5664 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  5665 | ` * two elements.` |
|        - |  5666 | ` */` |
|        - |  5667 | `/* OP_BXOR_STORE * * *` |
|        - |  5668 | ` *` |
|        - |  5669 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5670 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  5671 | ` * two elements.` |
|        - |  5672 | ` */` |
|       10 |  5673 | `case PH7_OP_BAND_STORE:` |
|        - |  5674 | `case PH7_OP_BOR_STORE:` |
|        - |  5675 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  5676 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5677 | `	ph7_value *pObj;` |
|        - |  5678 | `	sxi64 a,b,r;` |
|        - |  5679 | `#ifdef UNTRUST` |
|        - |  5680 | `	if( pNos < pStack ){` |
|        - |  5681 | `		goto Abort;` |
|        - |  5682 | `	}` |
|        - |  5683 | `#endif` |
|        - |  5684 | `	/* Force the operands to be integer */` |
|       21 |  5685 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5686 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5687 | `	}` |
|       21 |  5688 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5689 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5690 | `	}` |
|        - |  5691 | `	/* Perform the requested operation */` |
|       21 |  5692 | `	a = pTos->x.iVal;` |
|       21 |  5693 | `	b = pNos->x.iVal;` |
|       21 |  5694 | `	switch(pInstr->iOp){` |
|        3 |  5695 | `	case PH7_OP_BOR_STORE:` |
|        7 |  5696 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  5697 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  5698 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  5699 | `	case PH7_OP_BAND_STORE:` |
|        3 |  5700 | `	case PH7_OP_BAND:` |
|        7 |  5701 | `	default:          r = a&b; break;` |
|        - |  5702 | `	}` |
|        - |  5703 | `	/* Push the result */` |
|       21 |  5704 | `	pNos->x.iVal = r;` |
|       21 |  5705 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  5706 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5707 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  5708 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  5709 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  5710 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  5711 | `	}` |
|       21 |  5712 | `	VmPopOperand(&pTos,1);` |
|       21 |  5713 | `	break;` |
|        - |  5714 | `				 }` |
|        - |  5715 | `/* OP_SHL * * *` |
|        - |  5716 | ` *` |
|        - |  5717 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5718 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5719 | ` * left by N bits where N is the top element on the stack.` |
|        - |  5720 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5721 | ` */` |
|        - |  5722 | `/* OP_SHR * * *` |
|        - |  5723 | ` *` |
|        - |  5724 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5725 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5726 | ` * right by N bits where N is the top element on the stack.` |
|        - |  5727 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5728 | ` */` |
|       12 |  5729 | `case PH7_OP_SHL:` |
|        - |  5730 | `case PH7_OP_SHR: {` |
|       25 |  5731 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5732 | `	sxi64 a,r;` |
|        - |  5733 | `	sxi32 b;` |
|        - |  5734 | `#ifdef UNTRUST` |
|        - |  5735 | `	if( pNos < pStack ){` |
|        - |  5736 | `		goto Abort;` |
|        - |  5737 | `	}` |
|        - |  5738 | `#endif` |
|        - |  5739 | `	/* Force the operands to be integer */` |
|       25 |  5740 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5741 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5742 | `	}` |
|       25 |  5743 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5744 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5745 | `	}` |
|        - |  5746 | `	/* Perform the requested operation */` |
|       25 |  5747 | `	a = pNos->x.iVal;` |
|       25 |  5748 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  5749 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  5750 | `		r = a << b;` |
|        8 |  5751 | `	}else{` |
|       11 |  5752 | `		r = a >> b;` |
|        - |  5753 | `	}` |
|        - |  5754 | `	/* Push the result */` |
|       25 |  5755 | `	pNos->x.iVal = r;` |
|       25 |  5756 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  5757 | `	VmPopOperand(&pTos,1);` |
|       25 |  5758 | `	break;` |
|        - |  5759 | `				 }` |
|        - |  5760 | `/*  OP_SHL_STORE * * *` |
|        - |  5761 | ` *` |
|        - |  5762 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5763 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5764 | ` * left by N bits where N is the top element on the stack.` |
|        - |  5765 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5766 | ` */` |
|        - |  5767 | `/* OP_SHR_STORE * * *` |
|        - |  5768 | ` *` |
|        - |  5769 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5770 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5771 | ` * right by N bits where N is the top element on the stack.` |
|        - |  5772 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5773 | ` */` |
|        9 |  5774 | `case PH7_OP_SHL_STORE:` |
|        - |  5775 | `case PH7_OP_SHR_STORE: {` |
|       19 |  5776 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5777 | `	ph7_value *pObj;` |
|        - |  5778 | `	sxi64 a,r;` |
|        - |  5779 | `	sxi32 b;` |
|        - |  5780 | `#ifdef UNTRUST` |
|        - |  5781 | `	if( pNos < pStack ){` |
|        - |  5782 | `		goto Abort;` |
|        - |  5783 | `	}` |
|        - |  5784 | `#endif` |
|        - |  5785 | `	/* Force the operands to be integer */` |
|       19 |  5786 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5787 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5788 | `	}` |
|       19 |  5789 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5790 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5791 | `	}` |
|        - |  5792 | `	/* Perform the requested operation */` |
|       19 |  5793 | `	a = pTos->x.iVal;` |
|       19 |  5794 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  5795 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  5796 | `		r = a << b;` |
|        5 |  5797 | `	}else{` |
|       11 |  5798 | `		r = a >> b;` |
|        - |  5799 | `	}` |
|        - |  5800 | `	/* Push the result */` |
|       19 |  5801 | `	pNos->x.iVal = r;` |
|       19 |  5802 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  5803 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5804 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  5805 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  5806 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  5807 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  5808 | `	}` |
|       19 |  5809 | `	VmPopOperand(&pTos,1);` |
|       19 |  5810 | `	break;` |
|        - |  5811 | `				 }` |
|        - |  5812 | `/* CAT:  P1 * *` |
|        - |  5813 | ` *` |
|        - |  5814 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  5815 | ` * back.` |
|        - |  5816 | ` */` |
|    69994 |  5817 | `case PH7_OP_CAT:{` |
|        - |  5818 | `	ph7_value *pNos,*pCur;` |
|   139990 |  5819 | `	if( pInstr->iP1 < 1 ){` |
|   112696 |  5820 | `		pNos = &pTos[-1];` |
|    56349 |  5821 | `	}else{` |
|    27296 |  5822 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  5823 | `	}` |
|        - |  5824 | `#ifdef UNTRUST` |
|        - |  5825 | `	if( pNos < pStack ){` |
|        - |  5826 | `		goto Abort;` |
|        - |  5827 | `	}` |
|        - |  5828 | `#endif` |
|        - |  5829 | `	/* Force a string cast */` |
|   139990 |  5830 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1640 |  5831 | `		PH7_MemObjToString(pNos);` |
|      819 |  5832 | `	}` |
|   139990 |  5833 | `	pCur = &pNos[1];` |
|   282560 |  5834 | `	while( pCur <= pTos ){` |
|   142572 |  5835 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50900 |  5836 | `			PH7_MemObjToString(pCur);` |
|    25449 |  5837 | `		}` |
|        - |  5838 | `		/* Perform the concatenation */` |
|   142572 |  5839 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   142530 |  5840 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    71264 |  5841 | `		}` |
|   142572 |  5842 | `		SyBlobRelease(&pCur->sBlob);` |
|   142572 |  5843 | `		pCur++;` |
|        2 |  5844 | `	}` |
|   139990 |  5845 | `	pTos = pNos;` |
|   139990 |  5846 | `	break;` |
|        - |  5847 | `				}` |
|        - |  5848 | `/*  CAT_STORE: * * *` |
|        - |  5849 | ` *` |
|        - |  5850 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  5851 | ` * back.` |
|        - |  5852 | ` */` |
|     3893 |  5853 | `case PH7_OP_CAT_STORE:{` |
|     7788 |  5854 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5855 | `	ph7_value *pObj;` |
|        - |  5856 | `#ifdef UNTRUST` |
|        - |  5857 | `	if( pNos < pStack ){` |
|        - |  5858 | `		goto Abort;` |
|        - |  5859 | `	}` |
|        - |  5860 | `#endif` |
|     7788 |  5861 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5862 | `		/* Force a string cast */` |
|        3 |  5863 | `		PH7_MemObjToString(pTos);` |
|        1 |  5864 | `	}` |
|     7788 |  5865 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5866 | `		/* Force a string cast */` |
|      ! 0 |  5867 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5868 | `	}` |
|        - |  5869 | `	/* Perform the concatenation (Reverse order) */` |
|     7788 |  5870 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     7788 |  5871 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3893 |  5872 | `	}` |
|        - |  5873 | `	/* Perform the store operation */` |
|     7788 |  5874 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5875 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     7788 |  5876 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     7788 |  5877 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|     7786 |  5878 | `		PH7_MemObjStore(pTos,pObj);` |
|     3892 |  5879 | `	}` |
|     7786 |  5880 | `	PH7_MemObjStore(pTos,pNos);` |
|     7786 |  5881 | `	VmPopOperand(&pTos,1);` |
|     7786 |  5882 | `	break;` |
|        - |  5883 | `				}` |
|        - |  5884 | `/* OP_AND: * * *` |
|        - |  5885 | ` *` |
|        - |  5886 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  5887 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5888 | ` * stack.` |
|        - |  5889 | ` */` |
|        - |  5890 | `/* OP_OR: * * *` |
|        - |  5891 | ` *` |
|        - |  5892 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  5893 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5894 | ` * stack.` |
|        - |  5895 | ` */` |
|   106814 |  5896 | `case PH7_OP_LAND:` |
|        - |  5897 | `case PH7_OP_LOR: {` |
|   213674 |  5898 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5899 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  5900 | `#ifdef UNTRUST` |
|        - |  5901 | `	if( pNos < pStack ){` |
|        - |  5902 | `		goto Abort;` |
|        - |  5903 | `	}` |
|        - |  5904 | `#endif` |
|        - |  5905 | `	/* Force a boolean cast */` |
|   213674 |  5906 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  5907 | `		PH7_MemObjToBool(pTos);` |
|        1 |  5908 | `	}` |
|   213674 |  5909 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5910 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  5911 | `	}` |
|   213674 |  5912 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   213674 |  5913 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   213674 |  5914 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  5915 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    97522 |  5916 | `		v1 = and_logic[v1*3+v2];` |
|    48784 |  5917 | `	}else{` |
|        - |  5918 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   116154 |  5919 | `		v1 = or_logic[v1*3+v2];` |
|        - |  5920 | `	}` |
|   213674 |  5921 | `	if( v1 == 2 ){` |
|      ! 0 |  5922 | `		v1 = 1;` |
|      ! 0 |  5923 | `	}` |
|   213674 |  5924 | `	VmPopOperand(&pTos,1);` |
|   213674 |  5925 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   213674 |  5926 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   213674 |  5927 | `	break;` |
|        - |  5928 | `				 }` |
|        - |  5929 | `/*` |
|        - |  5930 | ` * OP_NULLC: * * *` |
|        - |  5931 | ` * Null coalescing operator '??'.` |
|        - |  5932 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  5933 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  5934 | ` */` |
|        - |  5935 | `/*` |
|        - |  5936 | ` * OP_NULLC: * P2 *` |
|        - |  5937 | ` * Short-circuit null coalescing '??'.` |
|        - |  5938 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  5939 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  5940 | ` */` |
|       52 |  5941 | `case PH7_OP_NULLC: {` |
|        - |  5942 | `#ifdef UNTRUST` |
|        - |  5943 | `	if( pTos < pStack ){` |
|        - |  5944 | `		goto Abort;` |
|        - |  5945 | `	}` |
|        - |  5946 | `#endif` |
|      106 |  5947 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  5948 | `		/* Left is not null — keep it and skip the RHS */` |
|       42 |  5949 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       22 |  5950 | `	}else{` |
|        - |  5951 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       66 |  5952 | `		VmPopOperand(&pTos, 1);` |
|        - |  5953 | `	}` |
|      106 |  5954 | `	break;` |
|        - |  5955 |  |
|        - |  5956 | `/*` |
|        - |  5957 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  5958 | ` * Null coalescing assignment short-circuit.` |
|        - |  5959 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  5960 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  5961 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  5962 | ` */` |
|       23 |  5963 | `case PH7_OP_NULLC_JMP: {` |
|        - |  5964 | `#ifdef UNTRUST` |
|        - |  5965 | `	if( pTos < pStack ){` |
|        - |  5966 | `		goto Abort;` |
|        - |  5967 | `	}` |
|        - |  5968 | `#endif` |
|       47 |  5969 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       19 |  5970 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|        9 |  5971 | `	}` |
|       47 |  5972 | `	break;` |
|        - |  5973 |  |
|        - |  5974 | `/*` |
|        - |  5975 | ` * OP_NULLC_STORE: * * *` |
|        - |  5976 | ` * Null coalescing assignment store.` |
|        - |  5977 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  5978 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  5979 | ` * expression result.` |
|        - |  5980 | ` */` |
|        - |  5981 | `/*` |
|        - |  5982 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  5983 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  5984 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  5985 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  5986 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  5987 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  5988 | ` */` |
|       51 |  5989 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  5990 | `#ifdef UNTRUST` |
|        - |  5991 | `	if( pTos < pStack ){` |
|        - |  5992 | `		goto Abort;` |
|        - |  5993 | `	}` |
|        - |  5994 | `#endif` |
|      104 |  5995 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  5996 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  5997 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  5998 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  5999 | `	}` |
|      104 |  6000 | `	break;` |
|        - |  6001 |  |
|       14 |  6002 | `case PH7_OP_NULLC_STORE: {` |
|       29 |  6003 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6004 | `	ph7_value *pObj;` |
|        - |  6005 | `	sxu32 nIdx;` |
|        - |  6006 | `#ifdef UNTRUST` |
|        - |  6007 | `	if( pNos < pStack ){` |
|        - |  6008 | `		goto Abort;` |
|        - |  6009 | `	}` |
|        - |  6010 | `#endif` |
|       29 |  6011 | `	nIdx = pNos->nIdx;` |
|       29 |  6012 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  6013 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6014 | `			"Cannot perform assignment on a constant class attribute");` |
|       29 |  6015 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       29 |  6016 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       29 |  6017 | `		PH7_MemObjStore(pTos,pObj);` |
|       14 |  6018 | `	}` |
|       29 |  6019 | `	PH7_MemObjStore(pTos,pNos);` |
|       29 |  6020 | `	VmPopOperand(&pTos,1);` |
|       29 |  6021 | `	break;` |
|        - |  6022 |  |
|        - |  6023 | `/*` |
|        - |  6024 | ` * OP_SPREAD: * * *` |
|        - |  6025 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  6026 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  6027 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  6028 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  6029 | ` */` |
|        9 |  6030 | `case PH7_OP_SPREAD: {` |
|        - |  6031 | `#ifdef UNTRUST` |
|        - |  6032 | `	if( pTos < pStack ){` |
|        - |  6033 | `		goto Abort;` |
|        - |  6034 | `	}` |
|        - |  6035 | `#endif` |
|       20 |  6036 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  6037 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  6038 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  6039 | `		if( nEntry == 0 ){` |
|        - |  6040 | `			/* Empty array — remove from stack */` |
|        3 |  6041 | `			VmPopOperand(&pTos, 1);` |
|        3 |  6042 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  6043 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  6044 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  6045 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  6046 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  6047 | `				VM_STACK_GUARD);` |
|      ! 0 |  6048 | `		}else{` |
|        - |  6049 | `			ph7_hashmap_node *pNode2;` |
|        - |  6050 | `			ph7_value *pElem;` |
|        - |  6051 | `			sxu32 i;` |
|        - |  6052 | `			/* Overwrite TOS with first element */` |
|       18 |  6053 | `			pNode2 = pMap->pFirst;` |
|       18 |  6054 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  6055 | `			PH7_MemObjRelease(pTos);` |
|       18 |  6056 | `			if( pElem ){` |
|       18 |  6057 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  6058 | `			}` |
|       18 |  6059 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  6060 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  6061 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  6062 | `			pNode2 = pNode2->pPrev;` |
|        - |  6063 | `			/* Push remaining elements */` |
|       44 |  6064 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  6065 | `				pTos++;` |
|       28 |  6066 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  6067 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  6068 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  6069 | `				if( pElem ){` |
|       28 |  6070 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  6071 | `				}` |
|       28 |  6072 | `				pNode2 = pNode2->pPrev;` |
|       15 |  6073 | `			}` |
|       18 |  6074 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  6075 | `		}` |
|        9 |  6076 | `	}` |
|        - |  6077 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  6078 | `	break;` |
|        - |  6079 |  |
|        - |  6080 | `/* OP_LXOR: * * *` |
|        - |  6081 | ` *` |
|        - |  6082 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  6083 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6084 | ` * stack.` |
|        - |  6085 | ` * According to the PHP language reference manual:` |
|        - |  6086 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  6087 | ` *  TRUE,but not both.` |
|        - |  6088 | ` */` |
|        5 |  6089 | `case PH7_OP_LXOR:{` |
|       11 |  6090 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  6091 | `	sxi32 v = 0;` |
|        - |  6092 | `#ifdef UNTRUST` |
|        - |  6093 | `	if( pNos < pStack ){` |
|        - |  6094 | `		goto Abort;` |
|        - |  6095 | `	}` |
|        - |  6096 | `#endif` |
|        - |  6097 | `	/* Force a boolean cast */` |
|       11 |  6098 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6099 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  6100 | `	}` |
|       11 |  6101 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6102 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6103 | `	}` |
|       11 |  6104 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  6105 | `		v = 1;` |
|        3 |  6106 | `	}` |
|       11 |  6107 | `	VmPopOperand(&pTos,1);` |
|       11 |  6108 | `	pTos->x.iVal = v;` |
|       11 |  6109 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  6110 | `	break;` |
|        - |  6111 | `				 }` |
|        - |  6112 | `/* OP_EQ P1 P2 P3` |
|        - |  6113 | ` *` |
|        - |  6114 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  6115 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6116 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6117 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6118 | ` */` |
|        - |  6119 | `/* OP_NEQ P1 P2 P3` |
|        - |  6120 | ` *` |
|        - |  6121 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  6122 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6123 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6124 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6125 | ` */` |
|     4444 |  6126 | `case PH7_OP_EQ:` |
|        - |  6127 | `case PH7_OP_NEQ: {` |
|     8890 |  6128 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6129 | `	/* Perform the comparison and act accordingly */` |
|        - |  6130 | `#ifdef UNTRUST` |
|        - |  6131 | `	if( pNos < pStack ){` |
|        - |  6132 | `		goto Abort;` |
|        - |  6133 | `	}` |
|        - |  6134 | `#endif` |
|     8890 |  6135 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     8890 |  6136 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  6137 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     8881 |  6138 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     8846 |  6139 | `		rc = rc == 0;` |
|     4424 |  6140 | `	}else{` |
|       28 |  6141 | `		rc = rc != 0;` |
|        - |  6142 | `	}` |
|     8890 |  6143 | `	VmPopOperand(&pTos,1);` |
|     8890 |  6144 | `	if( !pInstr->iP2 ){` |
|        - |  6145 | `		/* Push comparison result without taking the jump */` |
|     8890 |  6146 | `		PH7_MemObjRelease(pTos);` |
|     8890 |  6147 | `		pTos->x.iVal = rc;` |
|        - |  6148 | `		/* Invalidate any prior representation */` |
|     8890 |  6149 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4446 |  6150 | `	}else{` |
|      ! 0 |  6151 | `		if( rc ){` |
|        - |  6152 | `			/* Jump to the desired location */` |
|      ! 0 |  6153 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6154 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6155 | `		}` |
|        - |  6156 | `	}` |
|     8890 |  6157 | `	break;` |
|        - |  6158 | `				 }` |
|        - |  6159 | `/* OP_TEQ P1 P2 *` |
|        - |  6160 | ` *` |
|        - |  6161 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  6162 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6163 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6164 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6165 | ` */` |
|   156281 |  6166 | `case PH7_OP_TEQ: {` |
|   312564 |  6167 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6168 | `	/* Perform the comparison and act accordingly */` |
|        - |  6169 | `#ifdef UNTRUST` |
|        - |  6170 | `	if( pNos < pStack ){` |
|        - |  6171 | `		goto Abort;` |
|        - |  6172 | `	}` |
|        - |  6173 | `#endif` |
|   312564 |  6174 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   312564 |  6175 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6176 | `		rc = 0;` |
|        2 |  6177 | `	}else{` |
|   312562 |  6178 | `		rc = rc == 0;` |
|        - |  6179 | `	}` |
|   312564 |  6180 | `	VmPopOperand(&pTos,1);` |
|   312564 |  6181 | `	if( !pInstr->iP2 ){` |
|        - |  6182 | `		/* Push comparison result without taking the jump */` |
|   312564 |  6183 | `		PH7_MemObjRelease(pTos);` |
|   312564 |  6184 | `		pTos->x.iVal = rc;` |
|        - |  6185 | `		/* Invalidate any prior representation */` |
|   312564 |  6186 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   156283 |  6187 | `	}else{` |
|      ! 0 |  6188 | `		if( rc ){` |
|        - |  6189 | `			/* Jump to the desired location */` |
|      ! 0 |  6190 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6191 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6192 | `		}` |
|        - |  6193 | `	}` |
|   312564 |  6194 | `	break;` |
|        - |  6195 | `				 }` |
|        - |  6196 | `/* OP_TNE P1 P2 *` |
|        - |  6197 | ` *` |
|        - |  6198 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  6199 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  6200 | ` * instruction.` |
|        - |  6201 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6202 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6203 | ` *` |
|        - |  6204 | ` */` |
|   120519 |  6205 | `case PH7_OP_TNE: {` |
|   241040 |  6206 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6207 | `	/* Perform the comparison and act accordingly */` |
|        - |  6208 | `#ifdef UNTRUST` |
|        - |  6209 | `	if( pNos < pStack ){` |
|        - |  6210 | `		goto Abort;` |
|        - |  6211 | `	}` |
|        - |  6212 | `#endif` |
|   241040 |  6213 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   241040 |  6214 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6215 | `		rc = 1;` |
|        2 |  6216 | `	}else{` |
|   241038 |  6217 | `		rc = rc != 0;` |
|        - |  6218 | `	}` |
|   241040 |  6219 | `	VmPopOperand(&pTos,1);` |
|   241040 |  6220 | `	if( !pInstr->iP2 ){` |
|        - |  6221 | `		/* Push comparison result without taking the jump */` |
|   241040 |  6222 | `		PH7_MemObjRelease(pTos);` |
|   241040 |  6223 | `		pTos->x.iVal = rc;` |
|        - |  6224 | `		/* Invalidate any prior representation */` |
|   241040 |  6225 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   120521 |  6226 | `	}else{` |
|      ! 0 |  6227 | `		if( rc ){` |
|        - |  6228 | `			/* Jump to the desired location */` |
|      ! 0 |  6229 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6230 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6231 | `		}` |
|        - |  6232 | `	}` |
|   241040 |  6233 | `	break;` |
|        - |  6234 | `				 }` |
|        - |  6235 | `/* OP_LT P1 P2 P3` |
|        - |  6236 | ` *` |
|        - |  6237 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6238 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6239 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6240 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6241 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6242 | ` *` |
|        - |  6243 | ` */` |
|        - |  6244 | `/* OP_LE P1 P2 P3` |
|        - |  6245 | ` *` |
|        - |  6246 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6247 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6248 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6249 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6250 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6251 | ` *` |
|        - |  6252 | ` */` |
|   111863 |  6253 | `case PH7_OP_LT:` |
|        - |  6254 | `case PH7_OP_LE: {` |
|   223772 |  6255 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6256 | `	/* Perform the comparison and act accordingly */` |
|        - |  6257 | `#ifdef UNTRUST` |
|        - |  6258 | `	if( pNos < pStack ){` |
|        - |  6259 | `		goto Abort;` |
|        - |  6260 | `	}` |
|        - |  6261 | `#endif` |
|   223772 |  6262 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   223772 |  6263 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6264 | `		rc = 0;` |
|   223768 |  6265 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1602 |  6266 | `		rc = rc < 1;` |
|      802 |  6267 | `	}else{` |
|   222164 |  6268 | `		rc = rc < 0;` |
|        - |  6269 | `	}` |
|   223772 |  6270 | `	VmPopOperand(&pTos,1);` |
|   223772 |  6271 | `	if( !pInstr->iP2 ){` |
|        - |  6272 | `		/* Push comparison result without taking the jump */` |
|   223772 |  6273 | `		PH7_MemObjRelease(pTos);` |
|   223772 |  6274 | `		pTos->x.iVal = rc;` |
|        - |  6275 | `		/* Invalidate any prior representation */` |
|   223772 |  6276 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   111909 |  6277 | `	}else{` |
|      ! 0 |  6278 | `		if( rc ){` |
|        - |  6279 | `			/* Jump to the desired location */` |
|      ! 0 |  6280 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6281 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6282 | `		}` |
|        - |  6283 | `	}` |
|   223772 |  6284 | `	break;` |
|        - |  6285 | `				}` |
|        - |  6286 | `/* OP_GT P1 P2 P3` |
|        - |  6287 | ` *` |
|        - |  6288 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6289 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6290 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6291 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6292 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6293 | ` *` |
|        - |  6294 | ` */` |
|        - |  6295 | `/* OP_GE P1 P2 P3` |
|        - |  6296 | ` *` |
|        - |  6297 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6298 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6299 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6300 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6301 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6302 | ` *` |
|        - |  6303 | ` */` |
|    55252 |  6304 | `case PH7_OP_GT:` |
|        - |  6305 | `case PH7_OP_GE: {` |
|   110506 |  6306 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6307 | `	/* Perform the comparison and act accordingly */` |
|        - |  6308 | `#ifdef UNTRUST` |
|        - |  6309 | `	if( pNos < pStack ){` |
|        - |  6310 | `		goto Abort;` |
|        - |  6311 | `	}` |
|        - |  6312 | `#endif` |
|   110506 |  6313 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   110506 |  6314 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6315 | `		rc = 0;` |
|   110502 |  6316 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   110114 |  6317 | `		rc = rc >= 0;` |
|    55058 |  6318 | `	}else{` |
|      386 |  6319 | `		rc = rc > 0;` |
|        - |  6320 | `	}` |
|   110506 |  6321 | `	VmPopOperand(&pTos,1);` |
|   110506 |  6322 | `	if( !pInstr->iP2 ){` |
|        - |  6323 | `		/* Push comparison result without taking the jump */` |
|   110506 |  6324 | `		PH7_MemObjRelease(pTos);` |
|   110506 |  6325 | `		pTos->x.iVal = rc;` |
|        - |  6326 | `		/* Invalidate any prior representation */` |
|   110506 |  6327 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    55254 |  6328 | `	}else{` |
|      ! 0 |  6329 | `		if( rc ){` |
|        - |  6330 | `			/* Jump to the desired location */` |
|      ! 0 |  6331 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6332 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6333 | `		}` |
|        - |  6334 | `	}` |
|   110506 |  6335 | `	break;` |
|        - |  6336 | `				}` |
|        - |  6337 | `/* OP_SPACESHIP * * *` |
|        - |  6338 | ` *` |
|        - |  6339 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  6340 | ` *   -1 if left < right` |
|        - |  6341 | ` *    0 if left == right` |
|        - |  6342 | ` *    1 if left > right` |
|        - |  6343 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  6344 | ` */` |
|       25 |  6345 | `case PH7_OP_SPACESHIP: {` |
|       51 |  6346 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6347 | `#ifdef UNTRUST` |
|        - |  6348 | `	if( pNos < pStack ){` |
|        - |  6349 | `		goto Abort;` |
|        - |  6350 | `	}` |
|        - |  6351 | `#endif` |
|       51 |  6352 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  6353 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  6354 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  6355 | `		rc = 1;` |
|        4 |  6356 | `	}else{` |
|        - |  6357 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  6358 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  6359 | `	}` |
|       51 |  6360 | `	VmPopOperand(&pTos,1);` |
|       51 |  6361 | `	PH7_MemObjRelease(pTos);` |
|       51 |  6362 | `	pTos->x.iVal = rc;` |
|       51 |  6363 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  6364 | `	break;` |
|        - |  6365 | `				}` |
|        - |  6366 | `/* OP_SEQ P1 P2 *` |
|        - |  6367 | ` * Strict string comparison.` |
|        - |  6368 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  6369 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6370 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  6371 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  6372 | ` * use PH7_OP_EQ.` |
|        - |  6373 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6374 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6375 | ` */` |
|        - |  6376 | `/* OP_SNE P1 P2 *` |
|        - |  6377 | ` * Strict string comparison.` |
|        - |  6378 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  6379 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6380 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  6381 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  6382 | ` * use PH7_OP_EQ.` |
|        - |  6383 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6384 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6385 | ` */` |
|       18 |  6386 | `case PH7_OP_SEQ:` |
|        - |  6387 | `case PH7_OP_SNE: {` |
|       38 |  6388 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6389 | `	SyString s1,s2;` |
|        - |  6390 | `	/* Perform the comparison and act accordingly */` |
|        - |  6391 | `#ifdef UNTRUST` |
|        - |  6392 | `	if( pNos < pStack ){` |
|        - |  6393 | `		goto Abort;` |
|        - |  6394 | `	}` |
|        - |  6395 | `#endif` |
|        - |  6396 | `	/* Force a string cast */` |
|       38 |  6397 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  6398 | `		PH7_MemObjToString(pTos);` |
|        2 |  6399 | `	}` |
|       38 |  6400 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  6401 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6402 | `	}` |
|       38 |  6403 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  6404 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  6405 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  6406 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  6407 | `		rc = rc != 0;` |
|      ! 0 |  6408 | `	}else{` |
|       38 |  6409 | `		rc = rc == 0;` |
|        - |  6410 | `	}` |
|       38 |  6411 | `	VmPopOperand(&pTos,1);` |
|       38 |  6412 | `	if( !pInstr->iP2 ){` |
|        - |  6413 | `		/* Push comparison result without taking the jump */` |
|       38 |  6414 | `		PH7_MemObjRelease(pTos);` |
|       38 |  6415 | `		pTos->x.iVal = rc;` |
|        - |  6416 | `		/* Invalidate any prior representation */` |
|       38 |  6417 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  6418 | `	}else{` |
|      ! 0 |  6419 | `		if( rc ){` |
|        - |  6420 | `			/* Jump to the desired location */` |
|      ! 0 |  6421 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6422 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6423 | `		}` |
|        - |  6424 | `	}` |
|       38 |  6425 | `	break;` |
|        - |  6426 | `				 }` |
|        - |  6427 | `/*` |
|        - |  6428 | ` * OP_LOAD_REF * * *` |
|        - |  6429 | ` * Push the index of a referenced object on the stack.` |
|        - |  6430 | ` */` |
|       57 |  6431 | `case PH7_OP_LOAD_REF: {` |
|        - |  6432 | `	sxu32 nIdx;` |
|        - |  6433 | `#ifdef UNTRUST` |
|        - |  6434 | `	if( pTos < pStack ){` |
|        - |  6435 | `		goto Abort;` |
|        - |  6436 | `	}` |
|        - |  6437 | `#endif` |
|        - |  6438 | `	/* Extract memory object index */` |
|      115 |  6439 | `	nIdx = pTos->nIdx;` |
|      115 |  6440 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  6441 | `		/* Nullify the object */` |
|       95 |  6442 | `		PH7_MemObjRelease(pTos);` |
|        - |  6443 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  6444 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  6445 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  6446 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  6447 | `	}` |
|      115 |  6448 | `	break;` |
|        - |  6449 | `					  }` |
|        - |  6450 | `/*` |
|        - |  6451 | ` * OP_STORE_REF * * P3` |
|        - |  6452 | ` * Perform an assignment operation by reference.` |
|        - |  6453 | ` */` |
|       16 |  6454 | ` case PH7_OP_STORE_REF: {` |
|       34 |  6455 | `	 SyString sName = { 0 , 0 };` |
|        - |  6456 | `	 VmFrame *pFrameLocal;` |
|        - |  6457 | `	SyHashEntry *pEntry;` |
|        - |  6458 | `	sxu32 nIdx;` |
|        - |  6459 | `#ifdef UNTRUST` |
|        - |  6460 | `	if( pTos < pStack ){` |
|        - |  6461 | `		goto Abort;` |
|        - |  6462 | `	}` |
|        - |  6463 | `#endif` |
|       34 |  6464 | `	if( pInstr->p3 == 0 ){` |
|        - |  6465 | `		char *zName;` |
|        - |  6466 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  6467 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6468 | `			/* Force a string cast */` |
|      ! 0 |  6469 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6470 | `		}` |
|      ! 0 |  6471 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6472 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  6473 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6474 | `			if( zName ){` |
|      ! 0 |  6475 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6476 | `			}` |
|      ! 0 |  6477 | `		}` |
|      ! 0 |  6478 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6479 | `		pTos--;` |
|      ! 0 |  6480 | `	}else{` |
|       34 |  6481 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  6482 | `	}` |
|       34 |  6483 | `	nIdx = pTos->nIdx;` |
|       34 |  6484 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  6485 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6486 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6487 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  6488 | `		}else{` |
|        - |  6489 | `			ph7_value *pObj;` |
|        - |  6490 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  6491 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  6492 | `			if( pObj == 0 ){` |
|      ! 0 |  6493 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6494 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  6495 | `				goto Abort;` |
|        - |  6496 | `			}` |
|        - |  6497 | `			/* Perform the store operation */` |
|      ! 0 |  6498 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  6499 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  6500 | `		}` |
|       34 |  6501 | `	}else if( sName.nByte > 0){` |
|       34 |  6502 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  6503 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  6504 | `		}else{` |
|       34 |  6505 | `			pFrameLocal = pVm->pFrame;` |
|       34 |  6506 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  6507 | `			/* Query the local frame */` |
|       34 |  6508 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       34 |  6509 | `			if( pEntry ){` |
|      ! 0 |  6510 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  6511 | `			}else{` |
|       34 |  6512 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       34 |  6513 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  6514 | `					/* Insert in the $GLOBALS array */` |
|       30 |  6515 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       14 |  6516 | `				}` |
|       34 |  6517 | `				if( rc == SXRET_OK ){` |
|       34 |  6518 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       16 |  6519 | `				}` |
|        - |  6520 | `			}` |
|        - |  6521 | `		}` |
|       16 |  6522 | `	}` |
|       34 |  6523 | `	break;` |
|        - |  6524 | `				 }` |
|        - |  6525 | `/*` |
|        - |  6526 | ` * OP_UPLINK P1 * *` |
|        - |  6527 | ` * Link a variable to the top active VM frame.` |
|        - |  6528 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  6529 | ` */` |
|       28 |  6530 | `case PH7_OP_UPLINK: {` |
|       58 |  6531 | `	if( pVm->pFrame->pParent ){` |
|       58 |  6532 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  6533 | `		SyString sName;` |
|        - |  6534 | `		/* Perform the link */` |
|      116 |  6535 | `		while( pLink <= pTos ){` |
|       60 |  6536 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6537 | `				/* Force a string cast */` |
|      ! 0 |  6538 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  6539 | `			}` |
|       60 |  6540 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       60 |  6541 | `			if( sName.nByte > 0 ){` |
|       60 |  6542 | `				VmFrameLink(&(*pVm),&sName);` |
|       29 |  6543 | `			}` |
|       60 |  6544 | `			pLink++;` |
|        2 |  6545 | `		}` |
|       28 |  6546 | `	}` |
|       58 |  6547 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       58 |  6548 | `	break;` |
|        - |  6549 | `					}` |
|        - |  6550 | `/*` |
|        - |  6551 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  6552 | ` * Push an exception in the corresponding container so that` |
|        - |  6553 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  6554 | ` */` |
|      150 |  6555 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      302 |  6556 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  6557 | `	VmFrame *pFrameLocal;` |
|        - |  6558 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      302 |  6559 | `	pException->iFinallyDone = 0;` |
|      302 |  6560 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  6561 | `	/* Create the exception frame */` |
|      302 |  6562 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      302 |  6563 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  6564 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  6565 | `		goto Abort;` |
|        - |  6566 | `	}` |
|        - |  6567 | `	/* Mark the special frame */` |
|      302 |  6568 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      302 |  6569 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  6570 | `	/* Point to the frame that trigger the exception */` |
|      302 |  6571 | `	pFrameLocal = pFrameLocal->pParent;` |
|      302 |  6572 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      302 |  6573 | `	pException->pFrame = pFrameLocal;` |
|      302 |  6574 | `	break;` |
|        - |  6575 | `							}` |
|        - |  6576 | `/*` |
|        - |  6577 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  6578 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  6579 | ` */` |
|      149 |  6580 | `case PH7_OP_POP_EXCEPTION: {` |
|      300 |  6581 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      300 |  6582 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  6583 | `		ph7_exception **apException;` |
|        - |  6584 | `		/* Pop the loaded exception */` |
|       32 |  6585 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       32 |  6586 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       30 |  6587 | `			(void)SySetPop(&pVm->aException);` |
|       14 |  6588 | `		}` |
|       15 |  6589 | `	}` |
|      300 |  6590 | `	pException->pFrame = 0;` |
|        - |  6591 | `	/* Leave the exception frame */` |
|      300 |  6592 | `	VmLeaveFrame(&(*pVm));` |
|        - |  6593 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      300 |  6594 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  6595 | `		sxi32 rcFinally;` |
|       20 |  6596 | `		pException->iFinallyDone = 1;` |
|       20 |  6597 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  6598 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  6599 | `			goto Abort;` |
|        - |  6600 | `		}` |
|        9 |  6601 | `	}` |
|      300 |  6602 | `	break;` |
|        - |  6603 | `							}` |
|        - |  6604 |  |
|        - |  6605 | `/*` |
|        - |  6606 | ` * OP_THROW * P2 *` |
|        - |  6607 | ` * Throw an user exception.` |
|        - |  6608 | ` */` |
|       58 |  6609 | `case PH7_OP_THROW: {` |
|      118 |  6610 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      118 |  6611 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  6612 | `#ifdef UNTRUST` |
|        - |  6613 | `	if( pTos < pStack ){` |
|        - |  6614 | `		goto Abort;` |
|        - |  6615 | `	}` |
|        - |  6616 | `#endif` |
|      118 |  6617 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  6618 | `	/* Tell the upper layer that an exception was thrown */` |
|      118 |  6619 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      118 |  6620 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      118 |  6621 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6622 | `		ph7_class *pThrowable;` |
|        - |  6623 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      118 |  6624 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      119 |  6625 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  6626 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  6627 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  6628 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  6629 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  6630 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  6631 | `			if( pErrorClass ){` |
|        3 |  6632 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  6633 | `			}` |
|        3 |  6634 | `			if( pErrInst ){` |
|        - |  6635 | `				ph7_class_method *pCons;` |
|        3 |  6636 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  6637 | `				if( pCons ){` |
|        - |  6638 | `					ph7_value sArg;` |
|        - |  6639 | `					ph7_value *apArg[1];` |
|        - |  6640 | `					SyString sMsgStr;` |
|        - |  6641 | `					static const char zErrMsg[] =` |
|        - |  6642 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  6643 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  6644 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  6645 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  6646 | `					apArg[0] = &sArg;` |
|        3 |  6647 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  6648 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  6649 | `				}` |
|        3 |  6650 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  6651 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  6652 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6653 | `					goto Abort;` |
|        - |  6654 | `				}` |
|        2 |  6655 | `			}else{` |
|        - |  6656 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  6657 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  6658 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6659 | `					goto Abort;` |
|        - |  6660 | `				}` |
|        - |  6661 | `			}` |
|        2 |  6662 | `		}else{` |
|        - |  6663 | `			/* Throw the exception */` |
|      116 |  6664 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      116 |  6665 | `			if( rc == SXERR_ABORT ){` |
|        - |  6666 | `				/* Abort processing immediately */` |
|       11 |  6667 | `				goto Abort;` |
|        - |  6668 | `			}` |
|        - |  6669 | `		}` |
|       55 |  6670 | `	}else{` |
|        - |  6671 | `		/* Expecting a class instance */` |
|      ! 0 |  6672 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  6673 | `		if( rc == SXERR_ABORT ){` |
|        - |  6674 | `			/* Abort processing immediately */` |
|      ! 0 |  6675 | `			goto Abort;` |
|        - |  6676 | `		}` |
|        - |  6677 | `	}` |
|        - |  6678 | `	/* Pop the top entry */` |
|      108 |  6679 | `	VmPopOperand(&pTos,1);` |
|        - |  6680 | `	/* Perform an unconditional jump */` |
|      108 |  6681 | `	pc = nJump - 1;` |
|      108 |  6682 | `	break;` |
|        - |  6683 | `				   }` |
|        - |  6684 | `/*` |
|        - |  6685 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  6686 | ` * Prepare a foreach step.` |
|        - |  6687 | ` */` |
|     5928 |  6688 | `case PH7_OP_FOREACH_INIT: {` |
|    11858 |  6689 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  6690 | `	void *pName;` |
|        - |  6691 | `#ifdef UNTRUST` |
|        - |  6692 | `	if( pTos < pStack ){` |
|        - |  6693 | `		goto Abort;` |
|        - |  6694 | `	}` |
|        - |  6695 | `#endif` |
|    11858 |  6696 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6697 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  6698 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6699 | `			/* Force a string cast */` |
|      ! 0 |  6700 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6701 | `		}` |
|        - |  6702 | `		/* Duplicate name */` |
|      ! 0 |  6703 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6704 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6705 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6706 | `		}` |
|      ! 0 |  6707 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6708 | `	}` |
|    11858 |  6709 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  6710 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6711 | `			/* Force a string cast */` |
|      ! 0 |  6712 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6713 | `		}` |
|        - |  6714 | `		/* Duplicate name */` |
|      ! 0 |  6715 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6716 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6717 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6718 | `		}` |
|      ! 0 |  6719 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6720 | `	}` |
|        - |  6721 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    11858 |  6722 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6723 | `		/* Jump out of the loop */` |
|      ! 0 |  6724 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6725 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  6726 | `		}` |
|      ! 0 |  6727 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  6728 | `	}else{` |
|        - |  6729 | `		ph7_foreach_step *pStep;` |
|    11858 |  6730 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    11858 |  6731 | `		if( pStep == 0 ){` |
|      ! 0 |  6732 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  6733 | `			/* Jump out of the loop */` |
|      ! 0 |  6734 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6735 | `		}else{` |
|        - |  6736 | `			/* Zero the structure */` |
|    11858 |  6737 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  6738 | `			/* Prepare the step */` |
|    11858 |  6739 | `			pStep->iFlags = pInfo->iFlags;` |
|    11858 |  6740 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6741 | `				ph7_hashmap *pMap;` |
|        - |  6742 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  6743 | `				 * source array so mutations don't affect other sharers. */` |
|    11826 |  6744 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  6745 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  6746 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  6747 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6748 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  6749 | `						 * variable still points at the same hashmap as` |
|        - |  6750 | `						 * the stack value. */` |
|        9 |  6751 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  6752 | `							pCur->iRef--;` |
|        9 |  6753 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  6754 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  6755 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  6756 | `						}` |
|        4 |  6757 | `					}` |
|        4 |  6758 | `				}` |
|    11826 |  6759 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6760 | `				/* Reset the internal loop cursor */` |
|    11826 |  6761 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6762 | `				/* Mark the step */` |
|    11826 |  6763 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    11826 |  6764 | `				pStep->xIter.pMap = pMap;` |
|    11826 |  6765 | `				pMap->iRef++;` |
|     5914 |  6766 | `			}else{` |
|       34 |  6767 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6768 | `				ph7_class *pIteratorClass;` |
|        - |  6769 | `				/* Check if the object implements Iterator */` |
|       34 |  6770 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       45 |  6771 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  6772 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  6773 | `					ph7_class_method *pRewind;` |
|       24 |  6774 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  6775 | `					pStep->xIter.pThis = pThis;` |
|       24 |  6776 | `					pThis->iRef++;` |
|       24 |  6777 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  6778 | `					if( pRewind ){` |
|       24 |  6779 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  6780 | `					}` |
|       13 |  6781 | `				}else{` |
|        - |  6782 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  6783 | `					ph7_class *pIterAggClass;` |
|       12 |  6784 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  6785 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  6786 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  6787 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  6788 | `						ph7_class_method *pGetIter;` |
|        3 |  6789 | `						int iterAggOk = 0;` |
|        3 |  6790 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  6791 | `						if( pGetIter ){` |
|        - |  6792 | `							ph7_value sResult;` |
|        3 |  6793 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  6794 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  6795 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  6796 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  6797 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  6798 | `									ph7_class_method *pRewind;` |
|        3 |  6799 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  6800 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  6801 | `									pIterObj->iRef++;` |
|        - |  6802 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  6803 | `									pStep->pOwner = pThis;` |
|        3 |  6804 | `									pThis->iRef++;` |
|        3 |  6805 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  6806 | `									if( pRewind ){` |
|        3 |  6807 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  6808 | `									}` |
|        3 |  6809 | `									iterAggOk = 1;` |
|        1 |  6810 | `								}` |
|        1 |  6811 | `							}` |
|        3 |  6812 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  6813 | `						}` |
|        3 |  6814 | `						if( !iterAggOk ){` |
|        - |  6815 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  6816 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6817 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  6818 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  6819 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  6820 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  6821 | `						}` |
|        2 |  6822 | `					}else{` |
|        - |  6823 | `						/* Plain object iteration via hAttr */` |
|        9 |  6824 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  6825 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  6826 | `						pStep->xIter.pThis = pThis;` |
|        9 |  6827 | `						pThis->iRef++;` |
|        - |  6828 | `					}` |
|        - |  6829 | `				}` |
|        - |  6830 | `			}` |
|        - |  6831 | `		}` |
|    11858 |  6832 | `		if( pStep ){` |
|    11858 |  6833 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  6834 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  6835 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  6836 | `				/* Jump out of the loop */` |
|      ! 0 |  6837 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  6838 | `			}` |
|     5928 |  6839 | `		}` |
|        - |  6840 | `	}` |
|    11858 |  6841 | `	VmPopOperand(&pTos,1);` |
|    11858 |  6842 | `	break;` |
|        - |  6843 | `						  }` |
|        - |  6844 | `/*` |
|        - |  6845 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  6846 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  6847 | ` */` |
|    96776 |  6848 | `case PH7_OP_FOREACH_STEP: {` |
|   193554 |  6849 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  6850 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  6851 | `	ph7_value *pValue;` |
|        - |  6852 | `	VmFrame *pFrameLocal;` |
|        - |  6853 | `	/* Peek the last step */` |
|   193554 |  6854 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   193554 |  6855 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   193554 |  6856 | `	pFrameLocal = pVm->pFrame;` |
|   193554 |  6857 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   193554 |  6858 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   193426 |  6859 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  6860 | `		ph7_hashmap_node *pNode;` |
|        - |  6861 | `		/* Extract the current node value */` |
|   193426 |  6862 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   193426 |  6863 | `		if( pNode == 0 ){` |
|        - |  6864 | `			/* No more entry to process */` |
|    11824 |  6865 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    11824 |  6866 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6867 | `				/* Break the reference with the last element */` |
|        7 |  6868 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  6869 | `			}` |
|        - |  6870 | `			/* Automatically reset the loop cursor */` |
|    11824 |  6871 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6872 | `			/* Cleanup the mess left behind */` |
|    11824 |  6873 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    11824 |  6874 | `			SySetPop(&pInfo->aStep);` |
|    11824 |  6875 | `			PH7_HashmapUnref(pMap);` |
|     5913 |  6876 | `		}else{` |
|   181604 |  6877 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      426 |  6878 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      426 |  6879 | `				if( pKey ){` |
|      426 |  6880 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      212 |  6881 | `				}` |
|      212 |  6882 | `			}` |
|   181604 |  6883 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6884 | `				SyHashEntry *pEntry;` |
|        - |  6885 | `				/* Pass by reference */` |
|       23 |  6886 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  6887 | `				if( pEntry ){` |
|       21 |  6888 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  6889 | `				}else{` |
|        4 |  6890 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  6891 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  6892 | `				}` |
|       12 |  6893 | `			}else{` |
|        - |  6894 | `				/* Make a copy of the entry value */` |
|   181582 |  6895 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   181582 |  6896 | `				if( pValue ){` |
|   181582 |  6897 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    90790 |  6898 | `				}` |
|        - |  6899 | `			}` |
|        2 |  6900 | `		}` |
|    96842 |  6901 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  6902 | `		/* Iterator-based iteration.` |
|        - |  6903 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  6904 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  6905 | `		 */` |
|      106 |  6906 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  6907 | `		ph7_class_method *pMethod;` |
|        - |  6908 | `		ph7_value sResult;` |
|      106 |  6909 | `		int isValid = 0;` |
|        - |  6910 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  6911 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  6912 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  6913 | `		}else{` |
|       82 |  6914 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  6915 | `			if( pMethod ){` |
|       82 |  6916 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  6917 | `			}` |
|        - |  6918 | `		}` |
|        - |  6919 | `		/* Call valid() */` |
|      106 |  6920 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  6921 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  6922 | `		if( pMethod ){` |
|      106 |  6923 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  6924 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  6925 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  6926 | `		}` |
|      106 |  6927 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  6928 | `		if( !isValid ){` |
|        - |  6929 | `			/* Iterator exhausted */` |
|       24 |  6930 | `			pc = pInstr->iP2 - 1;` |
|        - |  6931 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  6932 | `			if( pStep->pOwner ){` |
|        3 |  6933 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  6934 | `			}` |
|       24 |  6935 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  6936 | `			SySetPop(&pInfo->aStep);` |
|       24 |  6937 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  6938 | `		}else{` |
|        - |  6939 | `			/* Call current() to get value */` |
|       84 |  6940 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  6941 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  6942 | `			if( pMethod ){` |
|       84 |  6943 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  6944 | `			}` |
|       84 |  6945 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  6946 | `			if( pValue ){` |
|       84 |  6947 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  6948 | `			}` |
|       84 |  6949 | `			PH7_MemObjRelease(&sResult);` |
|        - |  6950 | `			/* Call key() if needed */` |
|       84 |  6951 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  6952 | `				ph7_value sKey;` |
|       35 |  6953 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  6954 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  6955 | `				if( pMethod ){` |
|       35 |  6956 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  6957 | `				}` |
|       35 |  6958 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  6959 | `				if( pValue ){` |
|       35 |  6960 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  6961 | `				}` |
|       35 |  6962 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  6963 | `			}` |
|        - |  6964 | `		}` |
|       54 |  6965 | `	}else{` |
|       25 |  6966 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  6967 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  6968 | `		SyHashEntry *pEntry;` |
|        - |  6969 | `		/* Point to the next attribute */` |
|       29 |  6970 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  6971 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  6972 | `			/* Check access permission */` |
|       31 |  6973 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  6974 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  6975 | `					break; /* Access is granted */` |
|        - |  6976 | `			}` |
|        1 |  6977 | `		}` |
|       25 |  6978 | `		if( pEntry == 0 ){` |
|        - |  6979 | `			/* Clean up the mess left behind */` |
|        9 |  6980 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  6981 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6982 | `				/* Break the reference with the last element */` |
|        3 |  6983 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  6984 | `			}` |
|        9 |  6985 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  6986 | `			SySetPop(&pInfo->aStep);` |
|        9 |  6987 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  6988 | `		}else{` |
|       17 |  6989 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  6990 | `			ph7_value *pAttrValue;` |
|       17 |  6991 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  6992 | `				/* Fill with the current attribute name */` |
|       17 |  6993 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  6994 | `				if( pKey ){` |
|       17 |  6995 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  6996 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  6997 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  6998 | `				}` |
|        8 |  6999 | `			}` |
|        - |  7000 | `			/* Extract attribute value */` |
|       17 |  7001 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  7002 | `			if( pAttrValue ){` |
|       17 |  7003 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7004 | `					/* Pass by reference */` |
|        3 |  7005 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  7006 | `					if( pEntry ){` |
|        3 |  7007 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  7008 | `					}else{` |
|      ! 0 |  7009 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  7010 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  7011 | `					}` |
|        2 |  7012 | `				}else{` |
|        - |  7013 | `					/* Make a copy of the attribute value */` |
|       15 |  7014 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  7015 | `					if( pValue ){` |
|       15 |  7016 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  7017 | `					}` |
|        - |  7018 | `				}` |
|        8 |  7019 | `			}` |
|        - |  7020 | `		}` |
|        - |  7021 | `	}` |
|   193554 |  7022 | `	break;` |
|        - |  7023 | `						  }` |
|        - |  7024 | `/*` |
|        - |  7025 | ` * OP_MEMBER P1 P2` |
|        - |  7026 | ` * Load class attribute/method on the stack.` |
|        - |  7027 | ` */` |
|     3629 |  7028 | `case PH7_OP_MEMBER: {` |
|        - |  7029 | `	ph7_class_instance *pThis;` |
|        - |  7030 | `	ph7_value *pNos;` |
|        - |  7031 | `	SyString sName;` |
|     7260 |  7032 | `	if( !pInstr->iP1 ){` |
|     7034 |  7033 | `		pNos = &pTos[-1];` |
|        - |  7034 | `#ifdef UNTRUST` |
|        - |  7035 | `		if( pNos < pStack ){` |
|        - |  7036 | `			goto Abort;` |
|        - |  7037 | `		}` |
|        - |  7038 | `#endif` |
|     7034 |  7039 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7040 | `			ph7_class *pClass;` |
|        - |  7041 | `			/* Class already instantiated */` |
|     7032 |  7042 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  7043 | `			/* Point to the instantiated class */` |
|     7032 |  7044 | `			pClass = pThis->pClass;` |
|        - |  7045 | `			/* Extract attribute name first */` |
|     7032 |  7046 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     7032 |  7047 | `			if( pInstr->iP2 ){` |
|        - |  7048 | `				/* Method call */` |
|      720 |  7049 | `				ph7_class_method *pMeth = 0;` |
|      720 |  7050 | `				if( sName.nByte > 0 ){` |
|        - |  7051 | `					/* Extract the target method */` |
|      720 |  7052 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      359 |  7053 | `				}` |
|      720 |  7054 | `				if( pMeth == 0 ){` |
|      ! 0 |  7055 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  7056 | `						&pClass->sName,&sName` |
|        - |  7057 | `						);` |
|        - |  7058 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  7059 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  7060 | `					/* Pop the method name from the stack */` |
|      ! 0 |  7061 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7062 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  7063 | `				}else{` |
|        - |  7064 | `					/* Push method name on the stack */` |
|      720 |  7065 | `					PH7_MemObjRelease(pTos);` |
|      720 |  7066 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      720 |  7067 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7068 | `				}` |
|      720 |  7069 | `				pTos->nIdx = SXU32_HIGH;` |
|      361 |  7070 | `			}else{` |
|        - |  7071 | `				/* Attribute access */` |
|     6314 |  7072 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  7073 | `				SyHashEntry *pEntry;` |
|        - |  7074 | `				/* Extract the target attribute */` |
|     6314 |  7075 | `				if( sName.nByte > 0 ){` |
|     6314 |  7076 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     6314 |  7077 | `					if( pEntry ){` |
|        - |  7078 | `						/* Point to the attribute value */` |
|     6312 |  7079 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3155 |  7080 | `					}` |
|     3156 |  7081 | `				}` |
|     6314 |  7082 | `				if( pObjAttr == 0 ){` |
|        - |  7083 | `					/* No such attribute,load null */` |
|        4 |  7084 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  7085 | `						&pClass->sName,&sName);` |
|        - |  7086 | `					/* Call the __get magic method if available */` |
|        3 |  7087 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  7088 | `				}` |
|     6314 |  7089 | `				VmPopOperand(&pTos,1);` |
|        - |  7090 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  7091 | `				 * This is due to the following case:` |
|        - |  7092 | `				 *     (new TestClass())->foo;` |
|        - |  7093 | `				 */` |
|     6314 |  7094 | `				pThis->iRef++;` |
|     6314 |  7095 | `				PH7_MemObjRelease(pTos);` |
|     6314 |  7096 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     6314 |  7097 | `				if( pObjAttr ){` |
|     6312 |  7098 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  7099 | `					/* Check attribute access */` |
|     6312 |  7100 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  7101 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  7102 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  7103 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  7104 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  7105 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     6310 |  7106 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     3194 |  7107 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       76 |  7108 | `							VmInstr *pNext = pInstr + 1;` |
|       76 |  7109 | `							int bIsLhs = 0;` |
|       76 |  7110 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       74 |  7111 | `								bIsLhs = 1;` |
|       36 |  7112 | `							}` |
|       76 |  7113 | `							if( !bIsLhs ){` |
|        3 |  7114 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  7115 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  7116 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  7117 | `									goto Abort;` |
|        - |  7118 | `								}` |
|        - |  7119 | `								{` |
|        3 |  7120 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7121 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7122 | `										pc = pFrm2->iExceptionJump - 1;` |
|     3629 |  7123 | `										break;` |
|        - |  7124 | `									}` |
|        - |  7125 | `								}` |
|      ! 0 |  7126 | `								goto Exception;` |
|        - |  7127 | `							}` |
|       36 |  7128 | `						}` |
|        - |  7129 | `						/* Load attribute */` |
|     6310 |  7130 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     6310 |  7131 | `						if( pValue ){` |
|     6310 |  7132 | `							if( pThis->iRef < 2 ){` |
|        - |  7133 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  7134 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  7135 | `								 */` |
|        7 |  7136 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  7137 | `							}else{` |
|        - |  7138 | `								/* Simple load */` |
|     6304 |  7139 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  7140 | `							}` |
|     6310 |  7141 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     6308 |  7142 | `								if( pThis->iRef > 1 ){` |
|        - |  7143 | `									/* Load attribute index */` |
|     6302 |  7144 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     3150 |  7145 | `								}` |
|     3153 |  7146 | `							}` |
|     3154 |  7147 | `						}` |
|     3156 |  7148 | `					}else{` |
|        - |  7149 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  7150 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  7151 | `						char zMsg[256];` |
|      ! 0 |  7152 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  7153 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7154 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7155 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  7156 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7157 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  7158 | `						goto Abort;` |
|        - |  7159 | `					}` |
|     3154 |  7160 | `				}` |
|        - |  7161 | `				/* Safely unreference the object */` |
|     6312 |  7162 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  7163 | `			}` |
|     3516 |  7164 | `		}else{` |
|        3 |  7165 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  7166 | `			VmPopOperand(&pTos,1);` |
|        3 |  7167 | `			PH7_MemObjRelease(pTos);` |
|        3 |  7168 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  7169 | `		}` |
|     3517 |  7170 | `	}else{` |
|        - |  7171 | `		/* Static member access using class name */` |
|      228 |  7172 | `		pNos = pTos;` |
|      228 |  7173 | `		pThis = 0;` |
|      228 |  7174 | `		if( !pInstr->p3 ){` |
|      190 |  7175 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      190 |  7176 | `			pNos--;` |
|        - |  7177 | `#ifdef UNTRUST` |
|        - |  7178 | `			if( pNos < pStack ){` |
|        - |  7179 | `				goto Abort;` |
|        - |  7180 | `			}` |
|        - |  7181 | `#endif` |
|       96 |  7182 | `		}else{` |
|        - |  7183 | `			/* Attribute name already computed */` |
|       40 |  7184 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7185 | `		}` |
|      228 |  7186 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      228 |  7187 | `			ph7_class *pClass = 0;` |
|      228 |  7188 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7189 | `				/* Class already instantiated */` |
|        5 |  7190 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  7191 | `				pClass = pThis->pClass;` |
|        5 |  7192 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  7193 | `			}else{` |
|        - |  7194 | `				/* Try to extract the target class */` |
|      224 |  7195 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      224 |  7196 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      224 |  7197 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  7198 | `					/* Handle self/static/parent keywords */` |
|      224 |  7199 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  7200 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  7201 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  7202 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  7203 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  7204 | `						}` |
|      194 |  7205 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  7206 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      164 |  7207 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       28 |  7208 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       28 |  7209 | `						if( pSelf && pSelf->pBase ){` |
|       28 |  7210 | `							pClass = pSelf->pBase;` |
|       13 |  7211 | `						}` |
|       15 |  7212 | `					}else{` |
|      112 |  7213 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  7214 | `					}` |
|      111 |  7215 | `				}` |
|        - |  7216 | `			}` |
|      228 |  7217 | `			if( pClass == 0 ){` |
|        - |  7218 | `				/* Undefined class */` |
|      ! 0 |  7219 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  7220 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  7221 | `					);` |
|      ! 0 |  7222 | `				if( !pInstr->p3 ){` |
|      ! 0 |  7223 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7224 | `				}` |
|      ! 0 |  7225 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  7226 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  7227 | `			}else{` |
|      228 |  7228 | `				if( pInstr->iP2 ){` |
|        - |  7229 | `					/* Method call */` |
|       86 |  7230 | `					ph7_class_method *pMeth = 0;` |
|       86 |  7231 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  7232 | `						/* Extract the target method */` |
|       86 |  7233 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  7234 | `					}` |
|       86 |  7235 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  7236 | `						if( pMeth ){` |
|      ! 0 |  7237 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  7238 | `								&pClass->sName,&sName` |
|        - |  7239 | `								);` |
|      ! 0 |  7240 | `						}else{` |
|      ! 0 |  7241 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7242 | `								&pClass->sName,&sName` |
|        - |  7243 | `								);` |
|        - |  7244 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  7245 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  7246 | `						}` |
|        - |  7247 | `						/* Pop the method name from the stack */` |
|      ! 0 |  7248 | `						if( !pInstr->p3 ){` |
|      ! 0 |  7249 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  7250 | `						}` |
|      ! 0 |  7251 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  7252 | `					}else{` |
|        - |  7253 | `						/* Push method name on the stack */` |
|       86 |  7254 | `						PH7_MemObjRelease(pTos);` |
|       86 |  7255 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       86 |  7256 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7257 | `					}` |
|       86 |  7258 | `					pTos->nIdx = SXU32_HIGH;` |
|       44 |  7259 | `				}else{` |
|        - |  7260 | `					/* Attribute access */` |
|      144 |  7261 | `					ph7_class_attr *pAttr = 0;` |
|        - |  7262 | `					/* Check for special ::class pseudo-constant */` |
|      190 |  7263 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  7264 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  7265 | `						/* ::class returns the fully qualified class name */` |
|        - |  7266 | `						/* Pop the attribute name from the stack */` |
|       60 |  7267 | `						if( !pInstr->p3 ){` |
|       60 |  7268 | `							VmPopOperand(&pTos,1);` |
|       29 |  7269 | `						}` |
|       60 |  7270 | `						PH7_MemObjRelease(pTos);` |
|        - |  7271 | `						/* Load the class name */` |
|       60 |  7272 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  7273 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  7274 | `					}else{` |
|        - |  7275 | `						/* Extract the target attribute */` |
|       86 |  7276 | `						if( sName.nByte > 0 ){` |
|       86 |  7277 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       42 |  7278 | `						}` |
|       86 |  7279 | `						if( pAttr == 0 ){` |
|        - |  7280 | `							/* No such attribute,load null */` |
|      ! 0 |  7281 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7282 | `								&pClass->sName,&sName);` |
|        - |  7283 | `							/* Call the __get magic method if available */` |
|      ! 0 |  7284 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  7285 | `						}` |
|        - |  7286 | `						/* Pop the attribute name from the stack */` |
|       86 |  7287 | `						if( !pInstr->p3 ){` |
|       48 |  7288 | `							VmPopOperand(&pTos,1);` |
|       23 |  7289 | `						}` |
|       86 |  7290 | `						PH7_MemObjRelease(pTos);` |
|       86 |  7291 | `						pTos->nIdx = SXU32_HIGH;` |
|       86 |  7292 | `						if( pAttr ){` |
|       86 |  7293 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  7294 | `								/* Access to a non static attribute */` |
|      ! 0 |  7295 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7296 | `									&pClass->sName,&pAttr->sName` |
|        - |  7297 | `									);` |
|      ! 0 |  7298 | `							}else{` |
|        - |  7299 | `								ph7_value *pValue;` |
|        - |  7300 | `								/* Check if the access to the attribute is allowed */` |
|       86 |  7301 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  7302 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  7303 | `									 * Same LHS-of-store peek as the instance path. */` |
|       80 |  7304 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       55 |  7305 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       41 |  7306 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       26 |  7307 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       28 |  7308 | `										if( pS ){` |
|       28 |  7309 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       28 |  7310 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  7311 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  7312 | `												int bIsLhs = 0;` |
|        8 |  7313 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  7314 | `													bIsLhs = 1;` |
|        2 |  7315 | `												}` |
|        8 |  7316 | `												if( !bIsLhs ){` |
|        3 |  7317 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  7318 | `													if( pThis ){` |
|      ! 0 |  7319 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7320 | `													}` |
|        3 |  7321 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  7322 | `														goto Abort;` |
|        - |  7323 | `													}` |
|        - |  7324 | `													{` |
|        3 |  7325 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7326 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7327 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  7328 | `															break;` |
|        - |  7329 | `														}` |
|        - |  7330 | `													}` |
|      ! 0 |  7331 | `													goto Exception;` |
|        - |  7332 | `												}` |
|        2 |  7333 | `											}` |
|       12 |  7334 | `										}` |
|       12 |  7335 | `									}` |
|        - |  7336 | `									/* Load the desired attribute */` |
|       80 |  7337 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       80 |  7338 | `									if( pValue ){` |
|       80 |  7339 | `										PH7_MemObjLoad(pValue,pTos);` |
|       80 |  7340 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  7341 | `											/* Load index number */` |
|       38 |  7342 | `											pTos->nIdx = pAttr->nIdx;` |
|       18 |  7343 | `										}` |
|       39 |  7344 | `									}` |
|       41 |  7345 | `								}else{` |
|        - |  7346 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  7347 | `									char zMsg[256];` |
|        5 |  7348 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  7349 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  7350 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  7351 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  7352 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  7353 | `									}else{` |
|      ! 0 |  7354 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7355 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7356 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  7357 | `									}` |
|        5 |  7358 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  7359 | `									goto Abort;` |
|        - |  7360 | `								}` |
|        - |  7361 | `							}` |
|       39 |  7362 | `						}` |
|        - |  7363 | `					}` |
|        - |  7364 | `				}` |
|      222 |  7365 | `				if( pThis ){` |
|        - |  7366 | `					/* Safely unreference the object */` |
|        5 |  7367 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  7368 | `				}` |
|        - |  7369 | `			}` |
|      112 |  7370 | `		}else{` |
|        - |  7371 | `			/* Pop operands */` |
|      ! 0 |  7372 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  7373 | `			if( !pInstr->p3 ){` |
|      ! 0 |  7374 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  7375 | `			}` |
|      ! 0 |  7376 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7377 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  7378 | `		}` |
|        - |  7379 | `	}` |
|     7252 |  7380 | `	break;` |
|        - |  7381 | `					}` |
|        - |  7382 | `/*` |
|        - |  7383 | ` * OP_NEW P1 * * *` |
|        - |  7384 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  7385 | ` */` |
|      568 |  7386 | `case PH7_OP_NEW: {` |
|     1138 |  7387 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1138 |  7388 | `	ph7_class *pClass = 0;` |
|        - |  7389 | `	ph7_class_instance *pNew;` |
|     1138 |  7390 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  7391 | `		/* Try to extract the desired class */` |
|     1706 |  7392 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1136 |  7393 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      568 |  7394 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7395 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  7396 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  7397 | `	}` |
|     1138 |  7398 | `	if( pClass == 0 ){` |
|        - |  7399 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  7400 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  7401 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  7402 | `			);` |
|        - |  7403 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  7404 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7405 | `		if( pInstr->iP1 > 0 ){` |
|        - |  7406 | `			/* Pop given arguments */` |
|      ! 0 |  7407 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  7408 | `		}` |
|      ! 0 |  7409 | `		goto Abort;` |
|      ! 0 |  7410 | `	}else{` |
|        - |  7411 | `		ph7_class_method *pCons;` |
|        - |  7412 | `		/* Create a new class instance */` |
|     1138 |  7413 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1138 |  7414 | `		if( pNew == 0 ){` |
|      ! 0 |  7415 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7416 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  7417 | `				&pClass->sName` |
|        - |  7418 | `			);` |
|      ! 0 |  7419 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7420 | `			if( pInstr->iP1 > 0 ){` |
|        - |  7421 | `				/* Pop given arguments */` |
|      ! 0 |  7422 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  7423 | `			}` |
|      ! 0 |  7424 | `			break;` |
|        - |  7425 | `		}` |
|        - |  7426 | `		/* Check if a constructor is available */` |
|     1138 |  7427 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1138 |  7428 | `		if( pCons == 0 ){` |
|      830 |  7429 | `			SyString *pName = &pClass->sName;` |
|        - |  7430 | `			/* Check for a constructor with the same base class name */` |
|      830 |  7431 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      414 |  7432 | `		}` |
|     1138 |  7433 | `		if( pCons ){` |
|        - |  7434 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  7435 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  7436 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  7437 | `			 * (including variadic string-key packing). */` |
|      310 |  7438 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|      310 |  7439 | `			SySetReset(&aArg);` |
|      608 |  7440 | `			while( pArg < pTos ){` |
|      300 |  7441 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      300 |  7442 | `				pArg++;` |
|        2 |  7443 | `			}` |
|      310 |  7444 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  7445 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  7446 | `				sxu32 n;` |
|       65 |  7447 | `				n = SySetUsed(&aArg);` |
|        - |  7448 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  7449 | `				 * for named args the missing-arg check happens downstream` |
|        - |  7450 | `				 * after resolution). */` |
|      113 |  7451 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       49 |  7452 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       49 |  7453 | `					if( pFuncArg ){` |
|       49 |  7454 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  7455 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  7456 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  7457 | `						}` |
|       24 |  7458 | `					}` |
|       49 |  7459 | `					n++;` |
|        1 |  7460 | `				}` |
|       32 |  7461 | `			}` |
|      310 |  7462 | `			VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  7463 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      310 |  7464 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  7465 | `				pNew->iRef = 1;` |
|      ! 0 |  7466 | `			}` |
|      154 |  7467 | `		}` |
|     1138 |  7468 | `		if( pInstr->iP1 > 0 ){` |
|        - |  7469 | `			/* Pop given arguments */` |
|      246 |  7470 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      122 |  7471 | `		}` |
|     1138 |  7472 | `		PH7_MemObjRelease(pTos);` |
|     1138 |  7473 | `		pTos->x.pOther = pNew;` |
|     1138 |  7474 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  7475 | `	}` |
|     1138 |  7476 | `	break;` |
|        - |  7477 | `				 }` |
|        - |  7478 | `/*` |
|        - |  7479 | ` * OP_CLONE * * *` |
|        - |  7480 | ` * Perfome a clone operation.` |
|        - |  7481 | ` */` |
|       24 |  7482 | `case PH7_OP_CLONE: {` |
|        - |  7483 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  7484 | `#ifdef UNTRUST` |
|        - |  7485 | `	if( pTos < pStack ){` |
|        - |  7486 | `		goto Abort;` |
|        - |  7487 | `	}` |
|        - |  7488 | `#endif` |
|        - |  7489 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  7490 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  7491 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7492 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  7493 | `		PH7_MemObjRelease(pTos);` |
|        5 |  7494 | `		break;` |
|        - |  7495 | `	}` |
|        - |  7496 | `	/* Point to the source */` |
|       46 |  7497 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7498 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  7499 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  7500 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7501 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  7502 | `			&pSrc->pClass->sName);` |
|      ! 0 |  7503 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7504 | `		break;` |
|        - |  7505 | `	}` |
|        - |  7506 | `	/* Perform the clone operation */` |
|       46 |  7507 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  7508 | `	PH7_MemObjRelease(pTos);` |
|       46 |  7509 | `	if( pClone == 0 ){` |
|      ! 0 |  7510 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7511 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  7512 | `	}else{` |
|        - |  7513 | `		/* Load the cloned object */` |
|       46 |  7514 | `		pTos->x.pOther = pClone;` |
|       46 |  7515 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  7516 | `	}` |
|       46 |  7517 | `	break;` |
|        - |  7518 | `				   }` |
|        - |  7519 | `/*` |
|        - |  7520 | ` * OP_SWITCH * * P3` |
|        - |  7521 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  7522 | ` */` |
|       26 |  7523 | `case PH7_OP_SWITCH: {` |
|       54 |  7524 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  7525 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  7526 | `	ph7_value sValue,sCaseValue;` |
|        - |  7527 | `	sxu32 n,nEntry;` |
|        - |  7528 | `#ifdef UNTRUST` |
|        - |  7529 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  7530 | `		goto Abort;` |
|        - |  7531 | `	}` |
|        - |  7532 | `#endif` |
|        - |  7533 | `	/* Point to the case table  */` |
|       54 |  7534 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  7535 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  7536 | `	/* Select the appropriate case block to execute */` |
|       54 |  7537 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  7538 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  7539 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  7540 | `		pCase = &aCase[n];` |
|      130 |  7541 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  7542 | `		/* Execute the case expression first */` |
|      130 |  7543 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  7544 | `		/* Compare the two expression */` |
|      130 |  7545 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  7546 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  7547 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  7548 | `		if( rc == 0 ){` |
|        - |  7549 | `			/* Value match,jump to this block */` |
|       52 |  7550 | `			pc = pCase->nStart - 1;` |
|       52 |  7551 | `			break;` |
|        - |  7552 | `		}` |
|       41 |  7553 | `	}` |
|       54 |  7554 | `	VmPopOperand(&pTos,1);` |
|       54 |  7555 | `	if( n >= nEntry ){` |
|        - |  7556 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  7557 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  7558 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  7559 | `		}else{` |
|        - |  7560 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  7561 | `			pc = pSwitch->nOut - 1;` |
|        - |  7562 | `		}` |
|        1 |  7563 | `	}` |
|       54 |  7564 | `	break;` |
|        - |  7565 | `					}` |
|        - |  7566 | `/*` |
|        - |  7567 | ` * OP_MATCH * * P3` |
|        - |  7568 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  7569 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  7570 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  7571 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  7572 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  7573 | ` */` |
|       54 |  7574 | `case PH7_OP_MATCH: {` |
|      110 |  7575 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      110 |  7576 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  7577 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  7578 | `	sxu32 i,j,nArm,nCond;` |
|      110 |  7579 | `	int matched = 0;` |
|        - |  7580 | `#ifdef UNTRUST` |
|        - |  7581 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  7582 | `		goto Abort;` |
|        - |  7583 | `	}` |
|        - |  7584 | `#endif` |
|      110 |  7585 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      110 |  7586 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      110 |  7587 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      110 |  7588 | `	PH7_MemObjInit(pVm,&sCond);` |
|      110 |  7589 | `	PH7_MemObjInit(pVm,&sResult);` |
|      110 |  7590 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      348 |  7591 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  7592 | `		pArm = &aArm[i];` |
|      240 |  7593 | `		if( pArm->bDefault ){` |
|       13 |  7594 | `			pDefault = pArm;` |
|       13 |  7595 | `			continue;` |
|        - |  7596 | `		}` |
|      228 |  7597 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  7598 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  7599 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  7600 | `			if( pCondBc == 0 ){` |
|      ! 0 |  7601 | `				continue;` |
|        - |  7602 | `			}` |
|      260 |  7603 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      260 |  7604 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  7605 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  7606 | `			if( rc == 0 ){` |
|       93 |  7607 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       93 |  7608 | `				matched = 1;` |
|       93 |  7609 | `				break;` |
|        - |  7610 | `			}` |
|       85 |  7611 | `		}` |
|      115 |  7612 | `	}` |
|      110 |  7613 | `	if( !matched && pDefault ){` |
|       13 |  7614 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       13 |  7615 | `		matched = 1;` |
|        6 |  7616 | `	}` |
|      110 |  7617 | `	if( !matched ){` |
|        5 |  7618 | `		const char *zType = "unknown";` |
|        - |  7619 | `		char zMsg[128];` |
|        - |  7620 | `		sxu32 nMsg;` |
|        5 |  7621 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  7622 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  7623 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  7624 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  7625 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  7626 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  7627 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  7628 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  7629 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  7630 | `		default: break;` |
|        - |  7631 | `		}` |
|        7 |  7632 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  7633 | `			"Unhandled match case of type %s",zType);` |
|        7 |  7634 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  7635 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  7636 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  7637 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  7638 | `		goto Abort;` |
|        - |  7639 | `	}` |
|      105 |  7640 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  7641 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  7642 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  7643 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  7644 | `	break;` |
|        - |  7645 | `					}` |
|        - |  7646 | `/*` |
|        - |  7647 | ` * OP_YIELD P1 P2 *` |
|        - |  7648 | ` *  Yield a value from a generator function.` |
|        - |  7649 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  7650 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  7651 | ` */` |
|       34 |  7652 | `case PH7_OP_YIELD: {` |
|        - |  7653 | `	ph7_generator *pGen;` |
|       70 |  7654 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  7655 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  7656 | `		goto Abort;` |
|        - |  7657 | `	}` |
|       70 |  7658 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  7659 | `	if( pInstr->iP2 ){` |
|        - |  7660 | `		/* yield $key => $value: value on top, key below */` |
|        - |  7661 | `#ifdef UNTRUST` |
|        - |  7662 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  7663 | `#endif` |
|        7 |  7664 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  7665 | `		VmPopOperand(&pTos, 1);` |
|        7 |  7666 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  7667 | `		VmPopOperand(&pTos, 1);` |
|        - |  7668 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  7669 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  7670 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  7671 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  7672 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  7673 | `			}` |
|        1 |  7674 | `		}` |
|       67 |  7675 | `	}else if( pInstr->iP1 ){` |
|        - |  7676 | `		/* yield $value */` |
|        - |  7677 | `#ifdef UNTRUST` |
|        - |  7678 | `		if( pTos < pStack ) goto Abort;` |
|        - |  7679 | `#endif` |
|       64 |  7680 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  7681 | `		VmPopOperand(&pTos, 1);` |
|        - |  7682 | `		/* Auto-increment key */` |
|       64 |  7683 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  7684 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  7685 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  7686 | `	}else{` |
|        - |  7687 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  7688 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7689 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7690 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  7691 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  7692 | `	}` |
|        - |  7693 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  7694 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  7695 | `	goto Suspend;` |
|        - |  7696 |  |
|        - |  7697 | `/*` |
|        - |  7698 | ` * OP_CALL P1 * *` |
|        - |  7699 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  7700 | ` *  function on the stack.` |
|        - |  7701 | ` */` |
|   344530 |  7702 | `case PH7_OP_CALL: {` |
|   689106 |  7703 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  7704 | `	ph7_value *pArg;` |
|   689106 |  7705 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   689106 |  7706 | `	pArg = &pTos[-nCallArgs];` |
|        - |  7707 | `	SyHashEntry *pEntry;` |
|        - |  7708 | `	SyString sName;` |
|        - |  7709 | `	/* Extract function name */` |
|   689106 |  7710 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       78 |  7711 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7712 | `			ph7_value sResult;` |
|      ! 0 |  7713 | `			SySetReset(&aArg);` |
|      ! 0 |  7714 | `			while( pArg < pTos ){` |
|      ! 0 |  7715 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  7716 | `				pArg++;` |
|      ! 0 |  7717 | `			}` |
|      ! 0 |  7718 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  7719 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  7720 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  7721 | `			SySetReset(&aArg);` |
|        - |  7722 | `			/* Pop given arguments */` |
|      ! 0 |  7723 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7724 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7725 | `			}` |
|        - |  7726 | `			/* Copy result */` |
|      ! 0 |  7727 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  7728 | `			PH7_MemObjRelease(&sResult);` |
|       78 |  7729 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       78 |  7730 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7731 | `			ph7_value sResult;` |
|        - |  7732 | `			sxi32 rcInv;` |
|       78 |  7733 | `			SySetReset(&aArg);` |
|      192 |  7734 | `			while( pArg < pTos ){` |
|      116 |  7735 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      116 |  7736 | `				pArg++;` |
|        2 |  7737 | `			}` |
|       78 |  7738 | `			PH7_MemObjInit(pVm,&sResult);` |
|      116 |  7739 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       76 |  7740 | `				(int)SySetUsed(&aArg),` |
|       76 |  7741 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - |  7742 | `				&sResult,` |
|       76 |  7743 | `				(VmCallArgMap *)pInstr->p3);` |
|       78 |  7744 | `			SySetReset(&aArg);` |
|       78 |  7745 | `			if( nCallArgs > 0 ){` |
|       74 |  7746 | `				VmPopOperand(&pTos,nCallArgs);` |
|       36 |  7747 | `			}` |
|       78 |  7748 | `			if( rcInv == SXERR_INVALID ){` |
|        - |  7749 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - |  7750 | `				 * sResult was already released by VmCallObjectInvoke.` |
|        - |  7751 | `				 * Pin pThis: the release below would otherwise drop the last ref` |
|        - |  7752 | `				 * to a temporary callable like (new Plain())(...), freeing the` |
|        - |  7753 | `				 * instance before VmRaiseNotCallable reads its class name. */` |
|       13 |  7754 | `				pThis->iRef++;` |
|       13 |  7755 | `				PH7_MemObjRelease(pTos);` |
|       13 |  7756 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 |  7757 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 |  7758 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7759 | `					goto Abort;` |
|        - |  7760 | `				}` |
|        - |  7761 | `				{` |
|       13 |  7762 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       12 |  7763 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION)` |
|       13 |  7764 | `					 && pFrm2->iExceptionJump > 0 ){` |
|       13 |  7765 | `						pc = pFrm2->iExceptionJump - 1;` |
|       13 |  7766 | `						break;` |
|        - |  7767 | `					}` |
|        - |  7768 | `				}` |
|      ! 0 |  7769 | `				goto Exception;` |
|        - |  7770 | `			}` |
|       66 |  7771 | `			PH7_MemObjStore(&sResult,pTos);` |
|       66 |  7772 | `			PH7_MemObjRelease(&sResult);` |
|       34 |  7773 | `		}else{` |
|        - |  7774 | `			/* Raise exception: Invalid function name */` |
|      ! 0 |  7775 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  7776 | `			/* Pop given arguments */` |
|      ! 0 |  7777 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7778 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7779 | `			}` |
|        - |  7780 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  7781 | `			PH7_MemObjRelease(pTos);` |
|        - |  7782 | `		}` |
|       66 |  7783 | `		break;` |
|        - |  7784 | `	}` |
|   689030 |  7785 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  7786 | `	/* Check for a compiled function first.` |
|        - |  7787 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  7788 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   689030 |  7789 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  7790 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  7791 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  7792 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  7793 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  7794 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  7795 | `	{` |
|   689030 |  7796 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   689030 |  7797 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  7798 | `		const char *zFunc;` |
|        - |  7799 | `		const char *zEnd;` |
|        - |  7800 | `		const char *z;` |
|        - |  7801 | `		SyString sGlobal;` |
|       22 |  7802 | `		zFunc = sName.zString;` |
|       22 |  7803 | `		zEnd  = zFunc + sName.nByte;` |
|       22 |  7804 | `		z = zEnd;` |
|        - |  7805 | `		/* Find last namespace separator */` |
|      194 |  7806 | `		while( z > zFunc ){` |
|      194 |  7807 | `			if( z[-1] == '\\' ){` |
|       22 |  7808 | `				break;` |
|        - |  7809 | `			}` |
|      174 |  7810 | `			z--;` |
|        2 |  7811 | `		}` |
|       22 |  7812 | `		if( z > zFunc && z < zEnd ){` |
|        - |  7813 | `			/* Retry lookup using the unqualified/global function name */` |
|       22 |  7814 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       22 |  7815 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  7816 | `		}` |
|       10 |  7817 | `	}` |
|        - |  7818 | `	} /* end VmCallArgMap namespace scope */` |
|   689030 |  7819 | `	if( pEntry ){` |
|        - |  7820 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  7821 | `		ph7_class_instance *pThis;` |
|        - |  7822 | `		ph7_value *pFrameStack;` |
|        - |  7823 | `		ph7_vm_func *pVmFunc;` |
|        - |  7824 | `		ph7_class *pSelf;` |
|        - |  7825 | `		VmFrame *pFrame;` |
|        - |  7826 | `		ph7_value *pObj;` |
|        - |  7827 | `		VmSlot sArg;` |
|        - |  7828 | `		sxu32 n;` |
|        - |  7829 | `		/* initialize fields */` |
|    17384 |  7830 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    17384 |  7831 | `		pThis = 0;` |
|    17384 |  7832 | `		pSelf = 0;` |
|    17384 |  7833 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  7834 | `			ph7_class_method *pMeth;` |
|        - |  7835 | `			/* Class method call */` |
|     2952 |  7836 | `			ph7_value *pTarget = &pTos[-1];` |
|     2952 |  7837 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  7838 | `				/* Extract the 'this' pointer */` |
|     2952 |  7839 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  7840 | `					/* Instance already loaded */` |
|     2862 |  7841 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     2862 |  7842 | `					pThis->iRef++;` |
|     2862 |  7843 | `					pSelf = pThis->pClass;` |
|     1430 |  7844 | `				}` |
|     2952 |  7845 | `				if( pSelf == 0 ){` |
|       92 |  7846 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  7847 | `						/* "Late Static Binding" class name */` |
|      128 |  7848 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  7849 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  7850 | `					}` |
|       92 |  7851 | `					if( pSelf == 0 ){` |
|       21 |  7852 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  7853 | `					}` |
|       45 |  7854 | `				}` |
|     2952 |  7855 | `				if( pThis == 0  ){` |
|       92 |  7856 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       92 |  7857 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       92 |  7858 | `					if( pFrameLocal->pParent ){` |
|        - |  7859 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       66 |  7860 | `						pThis = pFrameLocal->pThis;` |
|       66 |  7861 | `						if( pThis ){` |
|       21 |  7862 | `							pThis->iRef++;` |
|       10 |  7863 | `						}` |
|       32 |  7864 | `					}` |
|       45 |  7865 | `				}` |
|     2952 |  7866 | `				VmPopOperand(&pTos,1);` |
|     2952 |  7867 | `				PH7_MemObjRelease(pTos);` |
|        - |  7868 | `				/* Synchronize pointers */` |
|     2952 |  7869 | `				pArg = &pTos[-nCallArgs];` |
|        - |  7870 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  7871 | `				 * user have already computed the random generated unique class method name` |
|        - |  7872 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  7873 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  7874 | `				 */` |
|     2952 |  7875 | `				while( pArg < pStack ){` |
|      ! 0 |  7876 | `					pArg++;` |
|      ! 0 |  7877 | `				}` |
|     2952 |  7878 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  7879 | `					/* Check if the call is allowed */` |
|     2952 |  7880 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     2952 |  7881 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  7882 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  7883 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  7884 | `							char zMsg[256];` |
|      ! 0 |  7885 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  7886 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  7887 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  7888 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  7889 | `							/* Pop given arguments */` |
|      ! 0 |  7890 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  7891 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7892 | `							}` |
|      ! 0 |  7893 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  7894 | `							goto Abort;` |
|        - |  7895 | `						}` |
|        6 |  7896 | `					}` |
|     1475 |  7897 | `				}` |
|     1475 |  7898 | `			}` |
|     1475 |  7899 | `		}` |
|        - |  7900 | `		/* Check The recursion limit */` |
|    17384 |  7901 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  7902 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7903 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  7904 | `				&pVmFunc->sName);` |
|        - |  7905 | `			/* Pop given arguments */` |
|        3 |  7906 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7907 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7908 | `			}` |
|        - |  7909 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  7910 | `			PH7_MemObjRelease(pTos);` |
|       14 |  7911 | `			break;` |
|        - |  7912 | `		}` |
|    17382 |  7913 | `		if( pVmFunc->pNextName ){` |
|        - |  7914 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  7915 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  7916 | `		}` |
|    17382 |  7917 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  7918 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  7919 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  7920 | `			ph7_generator *pGenerator;` |
|        - |  7921 | `			ph7_class_instance *pGenObj;` |
|        - |  7922 | `			ph7_value *pCtxAttr;` |
|        - |  7923 | `			SyString sAttrName;` |
|        - |  7924 | `			ph7_value **apCallArgs;` |
|        - |  7925 | `			int nGenArgs, iArg;` |
|        - |  7926 | `			/* Collect arguments from the operand stack */` |
|       24 |  7927 | `			nGenArgs = (int)(pTos - pArg);` |
|       24 |  7928 | `			apCallArgs = 0;` |
|       24 |  7929 | `			if( nGenArgs > 0 ){` |
|       14 |  7930 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  7931 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  7932 | `				if( apCallArgs == 0 ){` |
|        - |  7933 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  7934 | `					nGenArgs = 0;` |
|      ! 0 |  7935 | `				}else{` |
|       10 |  7936 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  7937 | `					int didReorder = 0;` |
|       10 |  7938 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  7939 | `						/* Named-argument reordering for generator */` |
|        5 |  7940 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  7941 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  7942 | `						sxu32 nNV = nF;` |
|        5 |  7943 | `						sxi32 iVIdx = -1;` |
|        - |  7944 | `						sxi32 *aGSlot;` |
|        - |  7945 | `						sxu8 *aGUsed;` |
|        - |  7946 | `						sxu32 gi;` |
|       13 |  7947 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  7948 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  7949 | `						}` |
|        7 |  7950 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  7951 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  7952 | `						if( aGSlot ){` |
|        5 |  7953 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  7954 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  7955 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  7956 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  7957 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  7958 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7959 | `								goto Abort;` |
|        - |  7960 | `							}` |
|        - |  7961 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  7962 | `							 * append overflow (variadic / positional beyond` |
|        - |  7963 | `							 * formals) so downstream sees every argument. */` |
|        - |  7964 | `							{` |
|        5 |  7965 | `								int nOut = 0;` |
|       13 |  7966 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  7967 | `									sxu32 gj;` |
|       13 |  7968 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  7969 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  7970 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  7971 | `											break;` |
|        - |  7972 | `										}` |
|        3 |  7973 | `									}` |
|        5 |  7974 | `								}` |
|       13 |  7975 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  7976 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  7977 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  7978 | `									}` |
|        5 |  7979 | `								}` |
|        5 |  7980 | `								nGenArgs = nOut;` |
|        - |  7981 | `							}` |
|        5 |  7982 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  7983 | `							didReorder = 1;` |
|        2 |  7984 | `						}` |
|        - |  7985 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  7986 | `						 * positional fill below — preserves arg order rather` |
|        - |  7987 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  7988 | `					}` |
|       10 |  7989 | `					if( !didReorder ){` |
|       12 |  7990 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  7991 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  7992 | `						}` |
|        2 |  7993 | `					}` |
|        - |  7994 | `				}` |
|        4 |  7995 | `			}` |
|        - |  7996 | `			/* Create execution context and generator wrapper */` |
|       24 |  7997 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       24 |  7998 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  7999 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8000 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  8001 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  8002 | `				break;` |
|        - |  8003 | `			}` |
|       24 |  8004 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       24 |  8005 | `			if( pGenerator == 0 ){` |
|      ! 0 |  8006 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  8007 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8008 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  8009 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  8010 | `				break;` |
|        - |  8011 | `			}` |
|        - |  8012 | `			/* Set up the frame with arguments, closure env, $this */` |
|       24 |  8013 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       24 |  8014 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       24 |  8015 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       24 |  8016 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       24 |  8017 | `			pExecCtx->pFrame->pParent = 0;` |
|       24 |  8018 | `			if( apCallArgs ){` |
|       10 |  8019 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  8020 | `			}` |
|       24 |  8021 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  8022 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  8023 | `				if( pThis ){` |
|      ! 0 |  8024 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8025 | `				}` |
|      ! 0 |  8026 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8027 | `					goto Abort;` |
|        - |  8028 | `				}` |
|      ! 0 |  8029 | `				break;` |
|        - |  8030 | `			}` |
|        - |  8031 | `			/* Create Generator class instance */` |
|       24 |  8032 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       24 |  8033 | `			if( pGenObj == 0 ){` |
|      ! 0 |  8034 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  8035 | `				break;` |
|        - |  8036 | `			}` |
|        - |  8037 | `			/* Store generator in __ctx attribute */` |
|       24 |  8038 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       24 |  8039 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       24 |  8040 | `			if( pCtxAttr ){` |
|       24 |  8041 | `				pCtxAttr->x.pOther = pGenerator;` |
|       24 |  8042 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       11 |  8043 | `			}` |
|        - |  8044 | `			/* Pop args and function name, push Generator object */` |
|       24 |  8045 | `			PH7_MemObjRelease(pTos);` |
|       24 |  8046 | `			pTos = &pTos[-nCallArgs];` |
|       24 |  8047 | `			pTos->x.pOther = pGenObj;` |
|       24 |  8048 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       24 |  8049 | `			pGenObj->iRef++;` |
|       24 |  8050 | `			if( pThis ){` |
|      ! 0 |  8051 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8052 | `			}` |
|       24 |  8053 | `			break;` |
|        - |  8054 | `		}` |
|        - |  8055 | `		/* Extract the formal argument set */` |
|    17360 |  8056 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  8057 | `		/* Create a new VM frame  */` |
|    17360 |  8058 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    17360 |  8059 | `		if( rc != SXRET_OK ){` |
|        - |  8060 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  8061 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8062 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  8063 | `				&pVmFunc->sName);` |
|        - |  8064 | `			/* Pop given arguments */` |
|      ! 0 |  8065 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8066 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8067 | `			}` |
|        - |  8068 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8069 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8070 | `			break;` |
|        - |  8071 | `		}` |
|    17360 |  8072 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  8073 | `			/* Install the '$this' variable */` |
|        - |  8074 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     2880 |  8075 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     2880 |  8076 | `			if( pObj ){` |
|        - |  8077 | `				/* Reflect the change */` |
|     2880 |  8078 | `				pObj->x.pOther = pThis;` |
|     2880 |  8079 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1439 |  8080 | `			}` |
|     1439 |  8081 | `		}` |
|    17360 |  8082 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  8083 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  8084 | `			/* Install static variables */` |
|      ! 0 |  8085 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  8086 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  8087 | `				pStatic = &aStatic[n];` |
|      ! 0 |  8088 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  8089 | `					/* Initialize the static variables */` |
|      ! 0 |  8090 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  8091 | `					if( pObj ){` |
|        - |  8092 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  8093 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  8094 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  8095 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  8096 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  8097 | `						}` |
|      ! 0 |  8098 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  8099 | `					}else{` |
|      ! 0 |  8100 | `						continue;` |
|        - |  8101 | `					}` |
|      ! 0 |  8102 | `				}` |
|        - |  8103 | `				/* Install in the current frame */` |
|      ! 0 |  8104 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  8105 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  8106 | `			}` |
|      ! 0 |  8107 | `		}` |
|        - |  8108 | `		/* Push arguments in the local frame */` |
|        - |  8109 | `		{` |
|    17360 |  8110 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  8111 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  8112 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    17360 |  8113 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    17360 |  8114 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  8115 | `			/* ============================================================` |
|        - |  8116 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  8117 | `			 *` |
|        - |  8118 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  8119 | `			 * or position, then install them in the frame.` |
|        - |  8120 | `			 * ============================================================ */` |
|       96 |  8121 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       96 |  8122 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       96 |  8123 | `			sxi32 iVariadicIdx = -1;` |
|        - |  8124 | `			sxu32 nNonVariadic;` |
|        - |  8125 | `			sxi32 *aSlot;` |
|        - |  8126 | `			sxu8  *aUsed;` |
|        - |  8127 | `			sxu32 i;` |
|        - |  8128 | `			/* Find variadic parameter index */` |
|      292 |  8129 | `			for( i = 0; i < nFormal; i++ ){` |
|      206 |  8130 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  8131 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  8132 | `					break;` |
|        - |  8133 | `				}` |
|      100 |  8134 | `			}` |
|       96 |  8135 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  8136 | `			/* Allocate mapping arrays */` |
|      143 |  8137 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       94 |  8138 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       96 |  8139 | `			if( aSlot == 0 ){` |
|      ! 0 |  8140 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  8141 | `				goto Abort;` |
|        - |  8142 | `			}` |
|       96 |  8143 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  8144 | `			/* Resolve named arguments to formal parameters */` |
|      143 |  8145 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       47 |  8146 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       96 |  8147 | `			if( rc == PH7_ABORT ){` |
|        7 |  8148 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  8149 | `				goto Abort;` |
|        - |  8150 | `			}` |
|        - |  8151 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      275 |  8152 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  8153 | `				/* Find the stack arg mapped to formal n */` |
|      187 |  8154 | `				sxi32 iSrc = -1;` |
|      309 |  8155 | `				for( i = 0; i < nActual; i++ ){` |
|      291 |  8156 | `					if( aSlot[i] == (sxi32)n ){` |
|      169 |  8157 | `						iSrc = (sxi32)i;` |
|      169 |  8158 | `						break;` |
|        - |  8159 | `					}` |
|       62 |  8160 | `				}` |
|      187 |  8161 | `				if( iSrc >= 0 ){` |
|        - |  8162 | `					/* Argument was provided — install with type checking */` |
|      169 |  8163 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  8164 | `					/* NULL-to-default redirect (existing behavior) */` |
|      168 |  8165 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  8166 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  8167 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  8168 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8169 | `					}` |
|        - |  8170 | `					/* Type checking: union types */` |
|      169 |  8171 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  8172 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  8173 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  8174 | `							bCallIsStrict);` |
|       13 |  8175 | `						if( rcU != SXRET_OK ){` |
|        - |  8176 | `							const char *zGiven;` |
|      ! 0 |  8177 | `							const char *zExpected = "union";` |
|        - |  8178 | `							char zBuf[128];` |
|        - |  8179 | `							char zTypeBuf[128];` |
|      ! 0 |  8180 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8181 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  8182 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  8183 | `								zGiven = "null";` |
|      ! 0 |  8184 | `							}else{` |
|      ! 0 |  8185 | `								zGiven = ph7_type_name(pVal);` |
|        - |  8186 | `							}` |
|      ! 0 |  8187 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  8188 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  8189 | `							}` |
|      ! 0 |  8190 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8191 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  8192 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8193 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8194 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  8195 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8196 | `							pFrameStack = 0;` |
|      ! 0 |  8197 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  8198 | `							goto SkipFuncBody;` |
|        - |  8199 | `						}` |
|      171 |  8200 | `					}else if( aFormalArg[n].nType > 0` |
|       91 |  8201 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  8202 | `						/* Scalar/class type checking */` |
|       17 |  8203 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  8204 | `							SyString *pName = &aFormalArg[n].sClass;` |
|      ! 0 |  8205 | `							ph7_class *pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  8206 | `							if( pClass ){` |
|      ! 0 |  8207 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8208 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  8209 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8210 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8211 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8212 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8213 | `									}` |
|      ! 0 |  8214 | `								}else{` |
|      ! 0 |  8215 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  8216 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  8217 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8218 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8219 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8220 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8221 | `									}` |
|        - |  8222 | `								}` |
|      ! 0 |  8223 | `							}` |
|       17 |  8224 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  8225 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  8226 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8227 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  8228 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8229 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8230 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8231 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8232 | `								pFrameStack = 0;` |
|      ! 0 |  8233 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8234 | `								goto SkipFuncBody;` |
|        7 |  8235 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8236 | `								char zTypeBuf[128];` |
|      ! 0 |  8237 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8238 | `									&aFormalArg[n].sName,` |
|      ! 0 |  8239 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  8240 | `									ph7_type_name(pVal));` |
|      ! 0 |  8241 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8242 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8243 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8244 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8245 | `								pFrameStack = 0;` |
|      ! 0 |  8246 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8247 | `								goto SkipFuncBody;` |
|        - |  8248 | `							}` |
|        3 |  8249 | `						}` |
|        8 |  8250 | `					}` |
|        - |  8251 | `					/* Install: by reference or by value */` |
|      169 |  8252 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  8253 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  8254 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  8255 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8256 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  8257 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  8258 | `							}` |
|      ! 0 |  8259 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  8260 | `						}else{` |
|        7 |  8261 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  8262 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  8263 | `							if( pRefEntry == 0 ){` |
|        7 |  8264 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  8265 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  8266 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  8267 | `								sArg.pUserData = 0;` |
|        5 |  8268 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  8269 | `							}` |
|        5 |  8270 | `							pObj = 0;` |
|        - |  8271 | `						}` |
|        3 |  8272 | `					}else{` |
|      165 |  8273 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  8274 | `					}` |
|      169 |  8275 | `					if( pObj ){` |
|      165 |  8276 | `						PH7_MemObjStore(pVal,pObj);` |
|      165 |  8277 | `						sArg.nIdx = pObj->nIdx;` |
|      165 |  8278 | `						sArg.pUserData = 0;` |
|      165 |  8279 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       82 |  8280 | `					}` |
|       85 |  8281 | `				}else{` |
|        - |  8282 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  8283 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  8284 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  8285 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  8286 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  8287 | `						if( pObj ){` |
|       19 |  8288 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  8289 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  8290 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  8291 | `							sArg.pUserData = 0;` |
|       19 |  8292 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  8293 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  8294 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8295 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8296 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  8297 | `							}` |
|        9 |  8298 | `						}` |
|        9 |  8299 | `					}` |
|        - |  8300 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  8301 | `				}` |
|       94 |  8302 | `			}` |
|        - |  8303 | `			/* Handle variadic parameter */` |
|       89 |  8304 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  8305 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  8306 | `				if( pObj ){` |
|        9 |  8307 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  8308 | `					{` |
|        9 |  8309 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  8310 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  8311 | `							if( aSlot[i] == -1 ){` |
|       16 |  8312 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  8313 | `									/* Named variadic entry: insert with string key */` |
|        - |  8314 | `									ph7_value sKey;` |
|       11 |  8315 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  8316 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  8317 | `										pCallMap3->aNames[i].zString,` |
|       10 |  8318 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  8319 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  8320 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  8321 | `								}else{` |
|        - |  8322 | `									/* Positional variadic entry */` |
|      ! 0 |  8323 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  8324 | `								}` |
|        5 |  8325 | `							}` |
|       12 |  8326 | `						}` |
|        - |  8327 | `					}` |
|        9 |  8328 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  8329 | `					sArg.pUserData = 0;` |
|        9 |  8330 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  8331 | `				}` |
|        5 |  8332 | `			}else{` |
|        - |  8333 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  8334 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  8335 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  8336 | `				 * the positional-only path's behavior. */` |
|       81 |  8337 | `				sxu32 nAnon = nNonVariadic;` |
|      237 |  8338 | `				for( i = 0; i < nActual; i++ ){` |
|      157 |  8339 | `					if( aSlot[i] == -2 ){` |
|        - |  8340 | `						char zAnonBuf[32];` |
|        - |  8341 | `						SyString sAnonName;` |
|      ! 0 |  8342 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  8343 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  8344 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  8345 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  8346 | `						if( pObj ){` |
|      ! 0 |  8347 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  8348 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  8349 | `							sArg.pUserData = 0;` |
|      ! 0 |  8350 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  8351 | `						}` |
|      ! 0 |  8352 | `						nAnon++;` |
|      ! 0 |  8353 | `					}` |
|       79 |  8354 | `				}` |
|        - |  8355 | `			}` |
|        - |  8356 | `			/* Release all stack arguments */` |
|      267 |  8357 | `			for( i = 0; i < nActual; i++ ){` |
|      179 |  8358 | `				PH7_MemObjRelease(&pArg[i]);` |
|       90 |  8359 | `			}` |
|       89 |  8360 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  8361 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       89 |  8362 | `			n = nFormal;` |
|       45 |  8363 | `		}else{` |
|        - |  8364 | `		/* ============================================================` |
|        - |  8365 | `		 * Positional-only matching path (original)` |
|        - |  8366 | `		 * ============================================================ */` |
|    17266 |  8367 | `		n = 0;` |
|    46168 |  8368 | `		while( pArg < pTos ){` |
|    28974 |  8369 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  8370 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       40 |  8371 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       40 |  8372 | `				if( pObj ){` |
|        - |  8373 | `					/* Initialize as empty array */` |
|       40 |  8374 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  8375 | `					{` |
|       40 |  8376 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      150 |  8377 | `						while( pArg < pTos ){` |
|        - |  8378 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  8379 | `							 *` |
|        - |  8380 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  8381 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  8382 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  8383 | `							 * non-union variadic path below has the same limitation;` |
|        - |  8384 | `							 * fixing both wants a separate counter for elements` |
|        - |  8385 | `							 * already packed into the variadic array. */` |
|      114 |  8386 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  8387 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  8388 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  8389 | `									bCallIsStrict);` |
|       16 |  8390 | `								if( rcU != SXRET_OK ){` |
|        - |  8391 | `									const char *zGiven;` |
|        3 |  8392 | `									const char *zExpected = "union";` |
|        - |  8393 | `									char zBuf[128];` |
|        - |  8394 | `									char zTypeBuf[128];` |
|        3 |  8395 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8396 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  8397 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  8398 | `										zGiven = "null";` |
|      ! 0 |  8399 | `									}else{` |
|        3 |  8400 | `										zGiven = ph7_type_name(pArg);` |
|        - |  8401 | `									}` |
|        3 |  8402 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 |  8403 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 |  8404 | `									}` |
|        4 |  8405 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  8406 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 |  8407 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8408 | `										goto Abort;` |
|        - |  8409 | `									}` |
|        3 |  8410 | `									PH7_MemObjRelease(pTos);` |
|        3 |  8411 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  8412 | `									pFrameStack = 0;` |
|        3 |  8413 | `									rc = PH7_EXCEPTION;` |
|        3 |  8414 | `									goto SkipFuncBody;` |
|        - |  8415 | `								}` |
|       14 |  8416 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  8417 | `								pArg++;` |
|       14 |  8418 | `								continue;` |
|        - |  8419 | `							}` |
|        - |  8420 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  8421 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      114 |  8422 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  8423 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  8424 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  8425 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  8426 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  8427 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8428 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  8429 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8430 | `										goto Abort;` |
|        - |  8431 | `									}` |
|        - |  8432 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  8433 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  8434 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8435 | `									pFrameStack = 0;` |
|      ! 0 |  8436 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  8437 | `									goto SkipFuncBody;` |
|       13 |  8438 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8439 | `									char zTypeBuf[128];` |
|      ! 0 |  8440 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8441 | `										&aFormalArg[n].sName,` |
|      ! 0 |  8442 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  8443 | `										ph7_type_name(pArg));` |
|      ! 0 |  8444 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8445 | `										goto Abort;` |
|        - |  8446 | `									}` |
|      ! 0 |  8447 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  8448 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8449 | `									pFrameStack = 0;` |
|      ! 0 |  8450 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  8451 | `									goto SkipFuncBody;` |
|        - |  8452 | `								}` |
|        6 |  8453 | `							}` |
|      100 |  8454 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|      100 |  8455 | `							pArg++;` |
|        2 |  8456 | `						}` |
|        - |  8457 | `					}` |
|       38 |  8458 | `					sArg.nIdx = pObj->nIdx;` |
|       38 |  8459 | `					sArg.pUserData = 0;` |
|       38 |  8460 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  8461 | `				}` |
|       38 |  8462 | `				break; /* All remaining args consumed */` |
|        - |  8463 | `			}` |
|    28936 |  8464 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    28752 |  8465 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       33 |  8466 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  8467 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  8468 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  8469 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  8470 | `						goto Abort;` |
|        - |  8471 | `					}` |
|      ! 0 |  8472 | `				}` |
|        - |  8473 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    28754 |  8474 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       89 |  8475 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       58 |  8476 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       29 |  8477 | `						bCallIsStrict);` |
|       60 |  8478 | `					if( rcU != SXRET_OK ){` |
|        - |  8479 | `						const char *zGiven;` |
|       19 |  8480 | `						const char *zExpected = "union";` |
|        - |  8481 | `						char zBuf[128];` |
|        - |  8482 | `						char zTypeBuf[128];` |
|       19 |  8483 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  8484 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  8485 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  8486 | `							zGiven = "null";` |
|        5 |  8487 | `						}else{` |
|        5 |  8488 | `							zGiven = ph7_type_name(pArg);` |
|        - |  8489 | `						}` |
|       19 |  8490 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       19 |  8491 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 |  8492 | `						}` |
|       28 |  8493 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  8494 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       19 |  8495 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  8496 | `							goto Abort;` |
|        - |  8497 | `						}` |
|       19 |  8498 | `						PH7_MemObjRelease(pTos);` |
|       19 |  8499 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  8500 | `						pFrameStack = 0;` |
|       19 |  8501 | `						rc = PH7_EXCEPTION;` |
|       19 |  8502 | `						goto SkipFuncBody;` |
|        - |  8503 | `					}` |
|       21 |  8504 | `				}else` |
|        - |  8505 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  8506 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    28720 |  8507 | `				if( aFormalArg[n].nType > 0` |
|    15013 |  8508 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1304 |  8509 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  8510 | `						/* Argument must be a class instance [i.e: object] */` |
|       26 |  8511 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  8512 | `						ph7_class *pClass;` |
|        - |  8513 | `						/* Try to extract the desired class */` |
|       26 |  8514 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       26 |  8515 | `						if( pClass ){` |
|       22 |  8516 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8517 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  8518 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8519 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8520 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8521 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  8522 | `								}` |
|      ! 0 |  8523 | `							}else{` |
|        - |  8524 | `								/* reuse pThis declared in outer scope */` |
|       22 |  8525 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  8526 | `								/* Make sure the object is an instance of the given class */` |
|       22 |  8527 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  8528 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8529 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8530 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8531 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  8532 | `								}` |
|        - |  8533 | `							}` |
|       12 |  8534 | `						}` |
|     1292 |  8535 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       24 |  8536 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  8537 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  8538 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  8539 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  8540 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  8541 | `								goto Abort;` |
|        - |  8542 | `							}` |
|        - |  8543 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  8544 | `							PH7_MemObjRelease(pTos);` |
|       11 |  8545 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  8546 | `							pFrameStack = 0;` |
|       11 |  8547 | `							rc = PH7_EXCEPTION;` |
|       11 |  8548 | `							goto SkipFuncBody;` |
|       14 |  8549 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8550 | `							char zTypeBuf[128];` |
|        7 |  8551 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        4 |  8552 | `								&aFormalArg[n].sName,` |
|        4 |  8553 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        2 |  8554 | `								ph7_type_name(pArg));` |
|        5 |  8555 | `							if( rc == PH7_ABORT ){` |
|        5 |  8556 | `								goto Abort;` |
|        - |  8557 | `							}` |
|      ! 0 |  8558 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  8559 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8560 | `							pFrameStack = 0;` |
|      ! 0 |  8561 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  8562 | `							goto SkipFuncBody;` |
|        - |  8563 | `						}` |
|        4 |  8564 | `					}` |
|      644 |  8565 | `				}` |
|    28722 |  8566 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  8567 | `					/* Pass by reference */` |
|       58 |  8568 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  8569 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  8570 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  8571 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8572 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  8573 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  8574 | `						}` |
|        - |  8575 | `						/* Switch to pass by value */` |
|      ! 0 |  8576 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  8577 | `					}else{` |
|        - |  8578 | `						SyHashEntry *pRefEntry;` |
|        - |  8579 | `						/* Install the referenced variable in the private function frame */` |
|       58 |  8580 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       58 |  8581 | `						if( pRefEntry == 0 ){` |
|       86 |  8582 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       56 |  8583 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       58 |  8584 | `							sArg.nIdx = pArg->nIdx;` |
|       58 |  8585 | `							sArg.pUserData = 0;` |
|       58 |  8586 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       28 |  8587 | `						}` |
|       58 |  8588 | `						pObj = 0;` |
|        - |  8589 | `					}` |
|       30 |  8590 | `				}else{` |
|        - |  8591 | `					/* Pass by value,make a copy of the given argument */` |
|    28666 |  8592 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  8593 | `				}` |
|    14362 |  8594 | `			}else{` |
|        - |  8595 | `				char zName[32];` |
|        - |  8596 | `				SyString sArgName;` |
|        - |  8597 | `				/* Set a dummy name */` |
|      184 |  8598 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      184 |  8599 | `				sArgName.zString = zName;` |
|        - |  8600 | `				/* Annonymous argument */` |
|      184 |  8601 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  8602 | `			}` |
|    28904 |  8603 | `			if( pObj ){` |
|    28848 |  8604 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  8605 | `				/* Insert argument index  */` |
|    28848 |  8606 | `				sArg.nIdx = pObj->nIdx;` |
|    28848 |  8607 | `				sArg.pUserData = 0;` |
|    28848 |  8608 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    14423 |  8609 | `			}` |
|    28904 |  8610 | `			PH7_MemObjRelease(pArg);` |
|    28904 |  8611 | `			pArg++;` |
|    28904 |  8612 | `			++n;` |
|        2 |  8613 | `		}` |
|        - |  8614 | `		} /* end named vs positional branch */` |
|        - |  8615 | `		/* Set up closure environment */` |
|    17320 |  8616 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  8617 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  8618 | `			ph7_value *pValue;` |
|        - |  8619 | `			sxu32 iEnv;` |
|      120 |  8620 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      306 |  8621 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      188 |  8622 | `				pEnv = &aEnv[iEnv];` |
|      188 |  8623 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  8624 | `					/* Do not install null value */` |
|      114 |  8625 | `					continue;` |
|        - |  8626 | `				}` |
|       76 |  8627 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       76 |  8628 | `				if( pValue == 0 ){` |
|      ! 0 |  8629 | `					continue;` |
|        - |  8630 | `				}` |
|        - |  8631 | `				/* Invalidate any prior representation */` |
|       76 |  8632 | `				PH7_MemObjRelease(pValue);` |
|        - |  8633 | `				/* Duplicate bound variable value */` |
|       76 |  8634 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       39 |  8635 | `			}` |
|       59 |  8636 | `		}` |
|        - |  8637 | `		/* Process default values for remaining formal parameters */` |
|    19982 |  8638 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2710 |  8639 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  8640 | `				/* Variadic parameter with no extra args — create empty array */` |
|       48 |  8641 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       48 |  8642 | `				if( pObj ){` |
|       48 |  8643 | `					PH7_MemObjToHashmap(pObj);` |
|       48 |  8644 | `					sArg.nIdx = pObj->nIdx;` |
|       48 |  8645 | `					sArg.pUserData = 0;` |
|       48 |  8646 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  8647 | `				}` |
|       48 |  8648 | `				n++;` |
|       48 |  8649 | `				break; /* Variadic is always last */` |
|        - |  8650 | `			}` |
|     2664 |  8651 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2658 |  8652 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2658 |  8653 | `				if( pObj ){` |
|        - |  8654 | `					/* Evaluate the default value and extract it's result */` |
|     2658 |  8655 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2658 |  8656 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  8657 | `						goto Abort;` |
|        - |  8658 | `					}` |
|        - |  8659 | `					/* Insert argument index */` |
|     2658 |  8660 | `					sArg.nIdx = pObj->nIdx;` |
|     2658 |  8661 | `					sArg.pUserData = 0;` |
|     2658 |  8662 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  8663 | `					/* Make sure the default argument is of the correct type */` |
|     2656 |  8664 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1750 |  8665 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 |  8666 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  8667 | `						/* Cast to the desired type */` |
|        3 |  8668 | `						xCast(pObj);` |
|        1 |  8669 | `					}` |
|     1328 |  8670 | `				}` |
|     1328 |  8671 | `			}` |
|     2664 |  8672 | `			++n;` |
|        2 |  8673 | `		}` |
|        - |  8674 | `		} /* end VmCallArgMap scope */` |
|        - |  8675 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  8676 | `		 * does not return anything.` |
|        - |  8677 | `		 */` |
|    17320 |  8678 | `		PH7_MemObjRelease(pTos);` |
|    17320 |  8679 | `		pTos = &pTos[-nCallArgs];` |
|        - |  8680 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    17320 |  8681 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    17320 |  8682 | `		if( pFrameStack == 0 ){` |
|        - |  8683 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  8684 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  8685 | `				&pVmFunc->sName);` |
|      ! 0 |  8686 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8687 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8688 | `			}` |
|      ! 0 |  8689 | `			break;` |
|        - |  8690 | `		}` |
|     8659 |  8691 | `SkipFuncBody:` |
|    17350 |  8692 | `		if( pSelf ){` |
|        - |  8693 | `			/* Push class name */` |
|     2950 |  8694 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1474 |  8695 | `		}` |
|        - |  8696 | `		/* Increment nesting level */` |
|    17350 |  8697 | `		pVm->nRecursionDepth++;` |
|    17350 |  8698 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  8699 | `			/* Execute function body */` |
|    25979 |  8700 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    17318 |  8701 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0);` |
|     8659 |  8702 | `		}` |
|        - |  8703 | `		/* Decrement nesting level */` |
|    17350 |  8704 | `		pVm->nRecursionDepth--;` |
|    17350 |  8705 | `		if( pSelf ){` |
|        - |  8706 | `			/* Pop class name */` |
|     2950 |  8707 | `			(void)SySetPop(&pVm->aSelf);` |
|     1474 |  8708 | `		}` |
|        - |  8709 | `		/* Cleanup the mess left behind */` |
|    17350 |  8710 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  8711 | `			/* Return by reference,reflect that */` |
|        9 |  8712 | `			if( n != SXU32_HIGH ){` |
|        9 |  8713 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  8714 | `				sxu32 i;` |
|        - |  8715 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  8716 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  8717 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  8718 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  8719 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  8720 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  8721 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  8722 | `								&pVmFunc->sName);` |
|      ! 0 |  8723 | `						}` |
|      ! 0 |  8724 | `						n = SXU32_HIGH;` |
|      ! 0 |  8725 | `						break;` |
|        - |  8726 | `					}` |
|        3 |  8727 | `				}` |
|        5 |  8728 | `			}else{` |
|      ! 0 |  8729 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  8730 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  8731 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  8732 | `						&pVmFunc->sName);` |
|      ! 0 |  8733 | `				}` |
|        - |  8734 | `			}` |
|        9 |  8735 | `			pTos->nIdx = n;` |
|        4 |  8736 | `		}` |
|        - |  8737 | `		/* Cleanup the mess left behind */` |
|    17350 |  8738 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  8739 | `			/* An exception was throw in this frame */` |
|       48 |  8740 | `			pFrame = pFrame->pParent;` |
|       48 |  8741 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  8742 | `				/* Pop the resutlt */` |
|       46 |  8743 | `				VmPopOperand(&pTos,1);` |
|        - |  8744 | `				/* Jump to this destination */` |
|       46 |  8745 | `				pc = pFrame->iExceptionJump - 1;` |
|       46 |  8746 | `				rc = PH7_OK;` |
|       24 |  8747 | `			}else{` |
|        3 |  8748 | `				if( pFrame->pParent ){` |
|        3 |  8749 | `					rc = PH7_EXCEPTION;` |
|        2 |  8750 | `				}else{` |
|        - |  8751 | `					/* Continue normal execution */` |
|      ! 0 |  8752 | `					rc = PH7_OK;` |
|        - |  8753 | `				}` |
|        - |  8754 | `			}` |
|       23 |  8755 | `		}` |
|        - |  8756 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    17350 |  8757 | `		if( pFrameStack ){` |
|    17320 |  8758 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     8659 |  8759 | `		}` |
|        - |  8760 | `		/* Leave the frame */` |
|    17350 |  8761 | `		VmLeaveFrame(&(*pVm));` |
|    17350 |  8762 | `		if( rc == PH7_ABORT ){` |
|        - |  8763 | `			/* Abort processing immeditaley */` |
|       15 |  8764 | `			goto Abort;` |
|    17336 |  8765 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  8766 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  8767 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  8768 | `			 * overwriting the state saved by the inner level.` |
|        - |  8769 | `			 * pTos points to the result slot (not yet written).` |
|        - |  8770 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  8771 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  8772 | `			goto Suspend;` |
|    17298 |  8773 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  8774 | `			goto Exception;` |
|        - |  8775 | `		}` |
|     8649 |  8776 | `	}else{` |
|        - |  8777 | `		ph7_user_func *pFunc;` |
|        - |  8778 | `		ph7_context sCtx;` |
|        - |  8779 | `		ph7_value sRet;` |
|        - |  8780 | `		/* Look for an installed foreign function.` |
|        - |  8781 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  8782 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - |  8783 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - |  8784 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   671648 |  8785 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8786 | `		{` |
|   671648 |  8787 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   671648 |  8788 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - |  8789 | `			/* Compiler-qualified: try short name as global fallback */` |
|       22 |  8790 | `			const char *zShort = sName.zString;` |
|        - |  8791 | `			sxu32 i;` |
|      334 |  8792 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      314 |  8793 | `				if( sName.zString[i] == '\\' ){` |
|       28 |  8794 | `					zShort = &sName.zString[i + 1];` |
|       13 |  8795 | `				}` |
|      158 |  8796 | `			}` |
|       22 |  8797 | `			if( zShort != sName.zString ){` |
|       22 |  8798 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       22 |  8799 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 |  8800 | `			}` |
|       10 |  8801 | `		}` |
|        - |  8802 | `		} /* end VmCallArgMap namespace scope */` |
|   671648 |  8803 | `		if( pEntry == 0 ){` |
|        - |  8804 | `			/* Call to undefined function */` |
|        5 |  8805 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  8806 | `			/* Pop given arguments */` |
|        5 |  8807 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  8808 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8809 | `			}` |
|        - |  8810 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  8811 | `			PH7_MemObjRelease(pTos);` |
|       43 |  8812 | `			break;` |
|        - |  8813 | `		}` |
|   671644 |  8814 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  8815 | `		/* Start collecting function arguments */` |
|   671644 |  8816 | `		SySetReset(&aArg);` |
|  1807462 |  8817 | `		while( pArg < pTos ){` |
|  1135820 |  8818 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1135820 |  8819 | `			pArg++;` |
|        2 |  8820 | `		}` |
|        - |  8821 | `		/* Assume a null return value */` |
|   671644 |  8822 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  8823 | `		/* Init the call context */` |
|   671644 |  8824 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  8825 | `		/* Call the foreign function */` |
|   671644 |  8826 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  8827 | `		/* Release the call context */` |
|   671644 |  8828 | `		VmReleaseCallContext(&sCtx);` |
|   671644 |  8829 | `		if( rc == PH7_ABORT ){` |
|      489 |  8830 | `			goto Abort;` |
|   671156 |  8831 | `		}else if( rc == PH7_EXCEPTION ){` |
|       82 |  8832 | `			VmFrame *pFrm = pVm->pFrame;` |
|       82 |  8833 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       82 |  8834 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  8835 | `				/* Exception was NOT caught, propagate */` |
|        5 |  8836 | `				goto Exception;` |
|        - |  8837 | `			}` |
|        - |  8838 | `			/* Exception was caught: pop args and the result slot */` |
|       77 |  8839 | `			PH7_MemObjRelease(&sRet);` |
|       77 |  8840 | `			if( pInstr->iP1 > 0 ){` |
|       61 |  8841 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       30 |  8842 | `			}` |
|        - |  8843 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|       77 |  8844 | `			VmPopOperand(&pTos,1);` |
|        - |  8845 | `			/* Jump past the try/catch block via the exception frame */` |
|       77 |  8846 | `			pFrm = pVm->pFrame;` |
|       77 |  8847 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|       77 |  8848 | `				pc = pFrm->iExceptionJump - 1;` |
|       38 |  8849 | `			}` |
|       77 |  8850 | `			break;` |
|        - |  8851 | `		}` |
|   671076 |  8852 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  8853 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  8854 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  8855 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  8856 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  8857 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  8858 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  8859 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  8860 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  8861 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  8862 | `			}` |
|        - |  8863 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  8864 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  8865 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  8866 | `			goto Suspend;` |
|        - |  8867 | `		}` |
|   671038 |  8868 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8869 | `			/* Pop function name and arguments */` |
|   649762 |  8870 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   324902 |  8871 | `		}` |
|        - |  8872 | `		/* Save foreign function return value */` |
|   671038 |  8873 | `		PH7_MemObjStore(&sRet,pTos);` |
|   671038 |  8874 | `		PH7_MemObjRelease(&sRet);` |
|        - |  8875 | `	}` |
|   688332 |  8876 | `	break;` |
|        - |  8877 | `				  }` |
|        - |  8878 | `/*` |
|        - |  8879 | ` * OP_CONSUME: P1 * *` |
|        - |  8880 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  8881 | ` */` |
|    14434 |  8882 | `case PH7_OP_CONSUME: {` |
|    28870 |  8883 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    28870 |  8884 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  8885 |  |
|    28870 |  8886 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    28870 |  8887 | `	pCur = pOut;` |
|        - |  8888 | `	/* Start the consume process  */` |
|    57738 |  8889 | `	while( pOut <= pTos ){` |
|        - |  8890 | `		/* Force a string cast */` |
|    28870 |  8891 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      684 |  8892 | `			PH7_MemObjToString(pOut);` |
|      341 |  8893 | `		}` |
|    28870 |  8894 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  8895 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  8896 | `			/* Invoke the output consumer callback */` |
|    16906 |  8897 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    16906 |  8898 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    16906 |  8899 | `			SyBlobRelease(&pOut->sBlob);` |
|    16906 |  8900 | `			if( rc == SXERR_ABORT ){` |
|        - |  8901 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  8902 | `				goto Abort;` |
|        - |  8903 | `			}` |
|     8452 |  8904 | `		}` |
|    28870 |  8905 | `		pOut++;` |
|        2 |  8906 | `	}` |
|    28870 |  8907 | `	pTos = &pCur[-1];` |
|    28868 |  8908 | `	break;` |
|        - |  8909 | `					 }` |
|        - |  8910 |  |
|        - |  8911 | `		} /* Switch() */` |
| 11426740 |  8912 | `		pc++; /* Next instruction in the stream */` |
|        2 |  8913 | `	} /* For(;;) */` |
|    20819 |  8914 | `Done:` |
|    41640 |  8915 | `	SySetRelease(&aArg);` |
|    41640 |  8916 | `	return SXRET_OK;` |
|       72 |  8917 | `Suspend:` |
|      146 |  8918 | `	SySetRelease(&aArg);` |
|      146 |  8919 | `	return PH7_SUSPEND;` |
|      268 |  8920 | `Abort:` |
|      537 |  8921 | `	SySetRelease(&aArg);` |
|     1833 |  8922 | `	while( pTos >= pStack ){` |
|     1297 |  8923 | `		PH7_MemObjRelease(pTos);` |
|     1297 |  8924 | `		pTos--;` |
|        1 |  8925 | `	}` |
|      537 |  8926 | `	return PH7_ABORT;` |
|        3 |  8927 | `Exception:` |
|        8 |  8928 | `	SySetRelease(&aArg);` |
|       22 |  8929 | `	while( pTos >= pStack ){` |
|       16 |  8930 | `		PH7_MemObjRelease(pTos);` |
|       16 |  8931 | `		pTos--;` |
|        2 |  8932 | `	}` |
|        8 |  8933 | `	return PH7_EXCEPTION;` |
|    21164 |  8934 |  |
|        - |  8935 | `/*` |
|        - |  8936 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  8937 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  8938 | ` * See block-comment on that function for additional information.` |
|        - |  8939 | ` */` |
|    19552 |  8940 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  8941 |  |
|        - |  8942 | `	ph7_value *pStack;` |
|        - |  8943 | `	sxi32 rc;` |
|        - |  8944 | `	/* Allocate a new operand stack */` |
|    19554 |  8945 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    19554 |  8946 | `	if( pStack == 0 ){` |
|      ! 0 |  8947 | `		return SXERR_MEM;` |
|        - |  8948 | `	}` |
|        - |  8949 | `	/* Execute the program */` |
|    19554 |  8950 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0);` |
|        - |  8951 | `	/* Free the operand stack */` |
|    19554 |  8952 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  8953 | `	/* Execution result */` |
|    19554 |  8954 | `	return rc;` |
|     9778 |  8955 |  |
|        - |  8956 | `/*` |
|        - |  8957 | ` * Invoke any installed shutdown callbacks.` |
|        - |  8958 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  8959 | ` * or more calls to [register_shutdown_function()].` |
|        - |  8960 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  8961 | ` * execution ends.` |
|        - |  8962 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  8963 | ` * additional information.` |
|        - |  8964 | ` */` |
|     2670 |  8965 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  8966 |  |
|        - |  8967 | `	VmShutdownCB *pEntry;` |
|        - |  8968 | `	ph7_value *apArg[10];` |
|        - |  8969 | `	sxu32 n,nEntry;` |
|        - |  8970 | `	int i;` |
|        - |  8971 | `	/* Point to the stack of registered callbacks */` |
|     2672 |  8972 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    29372 |  8973 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    26702 |  8974 | `		apArg[i] = 0;` |
|    13352 |  8975 | `	}` |
|     2674 |  8976 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  8977 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  8978 | `		if( pEntry ){` |
|        - |  8979 | `			/* Prepare callback arguments if any */` |
|        3 |  8980 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  8981 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  8982 | `					break;` |
|        - |  8983 | `				}` |
|      ! 0 |  8984 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  8985 | `			}` |
|        - |  8986 | `			/* Invoke the callback */` |
|        3 |  8987 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  8988 | `			/*` |
|        - |  8989 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  8990 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  8991 | `			 */` |
|        3 |  8992 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  8993 | `			if( pEntry ){` |
|        3 |  8994 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  8995 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  8996 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  8997 | `				}` |
|        1 |  8998 | `			}` |
|        1 |  8999 | `		}` |
|        2 |  9000 | `	}` |
|     2672 |  9001 | `	SySetReset(&pVm->aShutdown);` |
|     2672 |  9002 |  |
|        - |  9003 | `/*` |
|        - |  9004 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  9005 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  9006 | ` * See block-comment on that function for additional information.` |
|        - |  9007 | ` */` |
|     2678 |  9008 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  9009 |  |
|        - |  9010 | `	/* Make sure we are ready to execute this program */` |
|     2680 |  9011 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  9012 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  9013 | `	}` |
|        - |  9014 | `	/* Set the execution magic number  */` |
|     2680 |  9015 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  9016 | `	/* Execute the program */` |
|     2680 |  9017 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0);` |
|        - |  9018 | `	/* Invoke any shutdown callbacks */` |
|     2676 |  9019 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  9020 | `	/*` |
|        - |  9021 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  9022 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  9023 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  9024 | `	 */` |
|     2676 |  9025 | `	return SXRET_OK;` |
|     1341 |  9026 |  |
|        - |  9027 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  9028 | `/*` |
|        - |  9029 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  9030 | ` * The context is in CREATED state and ready to be started.` |
|        - |  9031 | ` */` |
|       46 |  9032 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  9033 |  |
|        - |  9034 | `	ph7_exec_ctx *pCtx;` |
|        - |  9035 | `	ph7_value *pStack;` |
|        - |  9036 | `	VmFrame *pFrame;` |
|       48 |  9037 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       48 |  9038 | `	if( pCtx == 0 ){` |
|      ! 0 |  9039 | `		return 0;` |
|        - |  9040 | `	}` |
|       48 |  9041 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       48 |  9042 | `	pCtx->pVm = pVm;` |
|       48 |  9043 | `	pCtx->pFunc = pFunc;` |
|       48 |  9044 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       48 |  9045 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       48 |  9046 | `	pCtx->pc = 0;` |
|       48 |  9047 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       48 |  9048 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  9049 | `	/* Allocate a private operand stack */` |
|       48 |  9050 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       48 |  9051 | `	if( pStack == 0 ){` |
|      ! 0 |  9052 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9053 | `		return 0;` |
|        - |  9054 | `	}` |
|       48 |  9055 | `	pCtx->pStack = pStack;` |
|        - |  9056 | `	/* Create a detached frame for the fiber */` |
|       48 |  9057 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       48 |  9058 | `	if( pFrame == 0 ){` |
|      ! 0 |  9059 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  9060 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9061 | `		return 0;` |
|        - |  9062 | `	}` |
|       48 |  9063 | `	pCtx->pFrame = pFrame;` |
|       48 |  9064 | `	return pCtx;` |
|       25 |  9065 |  |
|        - |  9066 | `/*` |
|        - |  9067 | ` * Start executing a fiber context for the first time.` |
|        - |  9068 | ` */` |
|       46 |  9069 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  9070 |  |
|        - |  9071 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9072 | `	sxi32 rc;` |
|       48 |  9073 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9074 | `		return SXERR_INVALID;` |
|        - |  9075 | `	}` |
|        - |  9076 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 |  9077 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 |  9078 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9079 | `	/* Save and set the active context */` |
|       48 |  9080 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 |  9081 | `	pVm->pActiveCtx = pCtx;` |
|       48 |  9082 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 |  9083 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 |  9084 | `	pVm->nRecursionDepth++;` |
|        - |  9085 | `	/* Execute from the beginning */` |
|       48 |  9086 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 |  9087 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       46 |  9088 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|       48 |  9089 | `	pVm->nRecursionDepth--;` |
|        - |  9090 | `	/* Restore the previous context */` |
|       48 |  9091 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 |  9092 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9093 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 |  9094 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 |  9095 | `		pCtx->pFrame->pParent = 0;` |
|       46 |  9096 | `		if( pResult ){` |
|       24 |  9097 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  9098 | `		}` |
|       46 |  9099 | `		return SXRET_OK;` |
|        - |  9100 | `	}` |
|        - |  9101 | `	/* Detach frame */` |
|        3 |  9102 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  9103 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  9104 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  9105 | `	}` |
|        3 |  9106 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9107 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9108 | `		return PH7_ABORT;` |
|        - |  9109 | `	}` |
|        3 |  9110 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9111 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9112 | `		return PH7_EXCEPTION;` |
|        - |  9113 | `	}` |
|        - |  9114 | `	/* Normal completion */` |
|        3 |  9115 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  9116 | `	if( pResult ){` |
|        3 |  9117 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  9118 | `	}` |
|        3 |  9119 | `	return SXRET_OK;` |
|       25 |  9120 |  |
|        - |  9121 | `/*` |
|        - |  9122 | ` * Resume a suspended fiber context.` |
|        - |  9123 | ` */` |
|       98 |  9124 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  9125 |  |
|        - |  9126 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9127 | `	sxi32 rc;` |
|      100 |  9128 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  9129 | `		return SXERR_INVALID;` |
|        - |  9130 | `	}` |
|        - |  9131 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  9132 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  9133 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 |  9134 | `	if( pResumeValue ){` |
|       40 |  9135 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  9136 | `	}else{` |
|       62 |  9137 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  9138 | `	}` |
|      100 |  9139 | `	pCtx->nTos++;` |
|        - |  9140 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 |  9141 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 |  9142 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9143 | `	/* Save and set the active context */` |
|      100 |  9144 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 |  9145 | `	pVm->pActiveCtx = pCtx;` |
|      100 |  9146 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 |  9147 | `	pVm->nRecursionDepth++;` |
|        - |  9148 | `	/* Resume execution from saved PC */` |
|      100 |  9149 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 |  9150 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|       98 |  9151 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|      100 |  9152 | `	pVm->nRecursionDepth--;` |
|        - |  9153 | `	/* Restore the previous context */` |
|      100 |  9154 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 |  9155 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9156 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 |  9157 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 |  9158 | `		pCtx->pFrame->pParent = 0;` |
|       64 |  9159 | `		if( pResult ){` |
|       18 |  9160 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  9161 | `		}` |
|       64 |  9162 | `		return SXRET_OK;` |
|        - |  9163 | `	}` |
|        - |  9164 | `	/* Detach frame */` |
|       38 |  9165 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 |  9166 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 |  9167 | `		pCtx->pFrame->pParent = 0;` |
|       18 |  9168 | `	}` |
|       38 |  9169 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9170 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9171 | `		return PH7_ABORT;` |
|        - |  9172 | `	}` |
|       38 |  9173 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9174 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9175 | `		return PH7_EXCEPTION;` |
|        - |  9176 | `	}` |
|        - |  9177 | `	/* Normal completion */` |
|       38 |  9178 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 |  9179 | `	if( pResult ){` |
|       20 |  9180 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  9181 | `	}` |
|       38 |  9182 | `	return SXRET_OK;` |
|       51 |  9183 |  |
|        - |  9184 | `/*` |
|        - |  9185 | ` * Release an execution context and all its resources.` |
|        - |  9186 | ` */` |
|        4 |  9187 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  9188 |  |
|        5 |  9189 | `	if( pCtx == 0 ){` |
|      ! 0 |  9190 | `		return;` |
|        - |  9191 | `	}` |
|        5 |  9192 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  9193 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  9194 | `		return;` |
|        - |  9195 | `	}` |
|        5 |  9196 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  9197 | `	/* Release values */` |
|        5 |  9198 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  9199 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  9200 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  9201 | `	if( pCtx->pFrame ){` |
|        - |  9202 | `		VmSlot *aSlot;` |
|        - |  9203 | `		sxu32 n;` |
|        - |  9204 | `		/* Free local variables */` |
|        5 |  9205 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  9206 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  9207 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  9208 | `		}` |
|        - |  9209 | `		/* Remove local references */` |
|        5 |  9210 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  9211 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  9212 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  9213 | `		}` |
|        5 |  9214 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  9215 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  9216 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  9217 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  9218 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  9219 | `		pCtx->pFrame = 0;` |
|        2 |  9220 | `	}` |
|        - |  9221 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  9222 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  9223 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  9224 | `	if( pCtx->pStack ){` |
|        5 |  9225 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  9226 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  9227 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  9228 | `				PH7_MemObjRelease(pTos);` |
|        5 |  9229 | `				pTos--;` |
|        1 |  9230 | `			}` |
|        2 |  9231 | `		}` |
|        5 |  9232 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  9233 | `		pCtx->pStack = 0;` |
|        2 |  9234 | `	}` |
|        - |  9235 | `	/* Free the context itself */` |
|        5 |  9236 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  9237 |  |
|        - |  9238 | `/*` |
|        - |  9239 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  9240 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  9241 | ` */` |
|       90 |  9242 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  9243 |  |
|        - |  9244 | `	ph7_class_instance *pThis;` |
|        - |  9245 | `	SyString sAttr;` |
|        - |  9246 | `	ph7_value *pAttr;` |
|       92 |  9247 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9248 | `		return 0;` |
|        - |  9249 | `	}` |
|       92 |  9250 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  9251 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  9252 | `		return 0;` |
|        - |  9253 | `	}` |
|       92 |  9254 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  9255 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  9256 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  9257 | `		return 0;` |
|        - |  9258 | `	}` |
|       62 |  9259 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  9260 |  |
|        - |  9261 | `/*` |
|        - |  9262 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  9263 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  9264 | ` */` |
|       38 |  9265 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9266 |  |
|       40 |  9267 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  9268 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  9269 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9270 | `			"Cannot suspend outside of a fiber");` |
|        - |  9271 | `	}` |
|       40 |  9272 | `	if( nArg > 0 ){` |
|       40 |  9273 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  9274 | `	}else{` |
|      ! 0 |  9275 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  9276 | `	}` |
|       40 |  9277 | `	return PH7_SUSPEND;` |
|       21 |  9278 |  |
|        - |  9279 | `/*` |
|        - |  9280 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  9281 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  9282 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  9283 | ` */` |
|       24 |  9284 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9285 |  |
|        - |  9286 | `	ph7_class_instance *pThis;` |
|        - |  9287 | `	ph7_value *pAttr;` |
|        - |  9288 | `	SyString sAttrName;` |
|       26 |  9289 | `	if( nArg < 2 ){` |
|      ! 0 |  9290 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9291 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  9292 | `	}` |
|       26 |  9293 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9294 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9295 | `			"Fiber::__construct(): invalid $this");` |
|        - |  9296 | `	}` |
|       26 |  9297 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  9298 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  9299 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9300 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  9301 | `	}` |
|        - |  9302 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  9303 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  9304 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9305 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  9306 | `	}` |
|        - |  9307 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  9308 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  9309 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  9310 | `	if( pAttr ){` |
|       26 |  9311 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  9312 | `	}` |
|       26 |  9313 | `	return PH7_OK;` |
|       14 |  9314 |  |
|        - |  9315 | `/*` |
|        - |  9316 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  9317 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  9318 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  9319 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  9320 | ` */` |
|       24 |  9321 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  9322 | `	ph7_class_instance **ppThis)` |
|        2 |  9323 |  |
|       26 |  9324 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9325 | `	ph7_value *pCallable;` |
|        - |  9326 | `	SyString sAttrName;` |
|       26 |  9327 | `	*ppThis = 0;` |
|       26 |  9328 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  9329 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  9330 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  9331 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  9332 | `		return 0;` |
|        - |  9333 | `	}` |
|       26 |  9334 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  9335 | `		/* String callable — look up in user functions with overload support */` |
|        - |  9336 | `		SyString sName;` |
|        - |  9337 | `		SyHashEntry *pEntry;` |
|        - |  9338 | `		ph7_vm_func *pFunc;` |
|       26 |  9339 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  9340 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  9341 | `		if( pEntry == 0 ){` |
|      ! 0 |  9342 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  9343 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  9344 | `			return 0;` |
|        - |  9345 | `		}` |
|       26 |  9346 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  9347 | `		return pFunc;` |
|      ! 0 |  9348 | `	}else{` |
|        - |  9349 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  9350 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  9351 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  9352 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  9353 | `		if( pMethod == 0 ){` |
|      ! 0 |  9354 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9355 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  9356 | `			return 0;` |
|        - |  9357 | `		}` |
|      ! 0 |  9358 | `		*ppThis = pClosure;` |
|      ! 0 |  9359 | `		return &pMethod->sFunc;` |
|        - |  9360 | `	}` |
|       14 |  9361 |  |
|        - |  9362 | `/*` |
|        - |  9363 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  9364 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  9365 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  9366 | ` */` |
|       46 |  9367 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  9368 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  9369 |  |
|       48 |  9370 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  9371 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  9372 | `	sxu32 nFormal, n;` |
|        - |  9373 | `	VmSlot sSlot;` |
|        - |  9374 | `	sxi32 rc;` |
|        - |  9375 | `	/* Install $this for closure/method callables */` |
|       48 |  9376 | `	if( pClosureThis ){` |
|        - |  9377 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  9378 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  9379 | `		if( pObj ){` |
|      ! 0 |  9380 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  9381 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  9382 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  9383 | `		}` |
|      ! 0 |  9384 | `	}` |
|        - |  9385 | `	/* Install static variables */` |
|       48 |  9386 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  9387 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  9388 | `		ph7_value *pVal;` |
|      ! 0 |  9389 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  9390 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  9391 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  9392 | `			if( pVal ){` |
|      ! 0 |  9393 | `				sSlot.pUserData = 0;` |
|      ! 0 |  9394 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  9395 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  9396 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  9397 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  9398 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  9399 | `				}` |
|      ! 0 |  9400 | `			}` |
|      ! 0 |  9401 | `		}` |
|      ! 0 |  9402 | `	}` |
|        - |  9403 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       48 |  9404 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       48 |  9405 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       66 |  9406 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  9407 | `		ph7_value *pObj;` |
|       20 |  9408 | `		if( n < (sxu32)nArg ){` |
|        - |  9409 | `			/* Argument provided — install with type casting */` |
|       20 |  9410 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 |  9411 | `			if( pObj ){` |
|       20 |  9412 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  9413 | `				/* Type casting */` |
|       20 |  9414 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  9415 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9416 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9417 | `						if( xCast ){` |
|      ! 0 |  9418 | `							xCast(pObj);` |
|      ! 0 |  9419 | `						}` |
|      ! 0 |  9420 | `					}` |
|      ! 0 |  9421 | `				}` |
|       20 |  9422 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 |  9423 | `				sSlot.pUserData = 0;` |
|       20 |  9424 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 |  9425 | `			}` |
|        9 |  9426 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  9427 | `			/* Default value */` |
|      ! 0 |  9428 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  9429 | `			if( pObj ){` |
|      ! 0 |  9430 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  9431 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9432 | `					return rc;` |
|        - |  9433 | `				}` |
|      ! 0 |  9434 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  9435 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9436 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9437 | `						if( xCast ){` |
|      ! 0 |  9438 | `							xCast(pObj);` |
|      ! 0 |  9439 | `						}` |
|      ! 0 |  9440 | `					}` |
|      ! 0 |  9441 | `				}` |
|      ! 0 |  9442 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  9443 | `				sSlot.pUserData = 0;` |
|      ! 0 |  9444 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  9445 | `			}` |
|      ! 0 |  9446 | `		}` |
|       11 |  9447 | `	}` |
|        - |  9448 | `	/* Install closure environment (captured variables) */` |
|       48 |  9449 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  9450 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  9451 | `		ph7_value *pValue;` |
|        - |  9452 | `		sxu32 iEnv;` |
|        3 |  9453 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  9454 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  9455 | `			pEnv = &aEnv[iEnv];` |
|        7 |  9456 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  9457 | `				continue;` |
|        - |  9458 | `			}` |
|        5 |  9459 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  9460 | `			if( pValue == 0 ){` |
|      ! 0 |  9461 | `				continue;` |
|        - |  9462 | `			}` |
|        5 |  9463 | `			PH7_MemObjRelease(pValue);` |
|        5 |  9464 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  9465 | `		}` |
|        1 |  9466 | `	}` |
|       48 |  9467 | `	return SXRET_OK;` |
|       25 |  9468 |  |
|        - |  9469 | `/*` |
|        - |  9470 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  9471 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  9472 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  9473 | ` */` |
|       26 |  9474 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9475 |  |
|       28 |  9476 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9477 | `	ph7_class_instance *pThis;` |
|        - |  9478 | `	ph7_class_instance *pClosureThis;` |
|        - |  9479 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  9480 | `	ph7_vm_func *pFunc;` |
|        - |  9481 | `	ph7_value sResult;` |
|        - |  9482 | `	ph7_value *pCtxAttr;` |
|        - |  9483 | `	SyString sAttrName;` |
|        - |  9484 | `	sxi32 rc;` |
|       28 |  9485 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9486 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  9487 | `	}` |
|       28 |  9488 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  9489 | `	/* Check if already started (has a __ctx) */` |
|       28 |  9490 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 |  9491 | `	if( pExecCtx != 0 ){` |
|        3 |  9492 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9493 | `			"Cannot start a fiber that has already been started");` |
|        - |  9494 | `	}` |
|        - |  9495 | `	/* Resolve callable */` |
|       26 |  9496 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 |  9497 | `	if( pFunc == 0 ){` |
|      ! 0 |  9498 | `		return PH7_EXCEPTION;` |
|        - |  9499 | `	}` |
|        - |  9500 | `	/* Create execution context now that we know the function */` |
|       26 |  9501 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 |  9502 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  9503 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9504 | `			"Fiber::start(): out of memory");` |
|        - |  9505 | `	}` |
|        - |  9506 | `	/* Store context in $this->__ctx */` |
|       26 |  9507 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 |  9508 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  9509 | `	if( pCtxAttr ){` |
|       26 |  9510 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 |  9511 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  9512 | `	}` |
|        - |  9513 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  9514 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  9515 | `	 * into the fiber's frame, not the caller's. */` |
|       26 |  9516 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 |  9517 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  9518 | `	/* Unpack the args array and install into the frame */` |
|        - |  9519 | `	{` |
|       26 |  9520 | `		ph7_value **apValues = 0;` |
|       26 |  9521 | `		int nActual = 0;` |
|       26 |  9522 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 |  9523 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  9524 | `			ph7_hashmap_node *pNode;` |
|       26 |  9525 | `			sxu32 nCount = pMap->nEntry;` |
|       26 |  9526 | `			if( nCount > 0 ){` |
|        3 |  9527 | `				sxu32 idx = 0;` |
|        4 |  9528 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  9529 | `					nCount * sizeof(ph7_value *));` |
|        3 |  9530 | `				if( apValues ){` |
|        3 |  9531 | `					pNode = pMap->pFirst;` |
|        7 |  9532 | `					while( pNode && idx < nCount ){` |
|        5 |  9533 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  9534 | `						idx++;` |
|        5 |  9535 | `						pNode = pNode->pPrev;` |
|        1 |  9536 | `					}` |
|        3 |  9537 | `					nActual = (int)idx;` |
|        1 |  9538 | `				}` |
|        1 |  9539 | `			}` |
|       12 |  9540 | `		}` |
|       26 |  9541 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 |  9542 | `		if( apValues ){` |
|        3 |  9543 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  9544 | `		}` |
|        - |  9545 | `	}` |
|        - |  9546 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 |  9547 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 |  9548 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 |  9549 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9550 | `		return PH7_ABORT;` |
|        - |  9551 | `	}` |
|       26 |  9552 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 |  9553 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 |  9554 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9555 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9556 | `		return PH7_ABORT;` |
|        - |  9557 | `	}` |
|       26 |  9558 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9559 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9560 | `		return PH7_EXCEPTION;` |
|        - |  9561 | `	}` |
|       26 |  9562 | `	ph7_result_value(pCtx, &sResult);` |
|       26 |  9563 | `	PH7_MemObjRelease(&sResult);` |
|       26 |  9564 | `	return PH7_OK;` |
|       15 |  9565 |  |
|        - |  9566 | `/*` |
|        - |  9567 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  9568 | ` */` |
|       36 |  9569 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9570 |  |
|       38 |  9571 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9572 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  9573 | `	ph7_value sResult;` |
|        - |  9574 | `	ph7_value *pResumeVal;` |
|        - |  9575 | `	sxi32 rc;` |
|       38 |  9576 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9577 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  9578 | `		return PH7_OK;` |
|        - |  9579 | `	}` |
|       38 |  9580 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 |  9581 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  9582 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  9583 | `		return PH7_OK;` |
|        - |  9584 | `	}` |
|       38 |  9585 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  9586 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9587 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  9588 | `	}` |
|       36 |  9589 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 |  9590 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 |  9591 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 |  9592 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9593 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9594 | `		return PH7_ABORT;` |
|        - |  9595 | `	}` |
|       36 |  9596 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9597 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9598 | `		return PH7_EXCEPTION;` |
|        - |  9599 | `	}` |
|       36 |  9600 | `	ph7_result_value(pCtx, &sResult);` |
|       36 |  9601 | `	PH7_MemObjRelease(&sResult);` |
|       36 |  9602 | `	return PH7_OK;` |
|       20 |  9603 |  |
|        - |  9604 | `/*` |
|        - |  9605 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  9606 | ` */` |
|        6 |  9607 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9608 |  |
|        8 |  9609 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9610 | `	ph7_exec_ctx *pExecCtx;` |
|        8 |  9611 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9612 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9613 | `		return PH7_OK;` |
|        - |  9614 | `	}` |
|        8 |  9615 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 |  9616 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  9617 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9618 | `		return PH7_OK;` |
|        - |  9619 | `	}` |
|        8 |  9620 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  9621 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9622 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9623 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  9624 | `		}` |
|      ! 0 |  9625 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9626 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  9627 | `	}` |
|        8 |  9628 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 |  9629 | `	return PH7_OK;` |
|        5 |  9630 |  |
|        - |  9631 | `/*` |
|        - |  9632 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  9633 | ` */` |
|        6 |  9634 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9635 |  |
|        - |  9636 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9637 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9638 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9639 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  9640 | `	return PH7_OK;` |
|        4 |  9641 |  |
|      ! 0 |  9642 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  9643 |  |
|        - |  9644 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  9645 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  9646 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  9647 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  9648 | `	return PH7_OK;` |
|      ! 0 |  9649 |  |
|        6 |  9650 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9651 |  |
|        - |  9652 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9653 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9654 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9655 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  9656 | `	return PH7_OK;` |
|        4 |  9657 |  |
|        6 |  9658 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9659 |  |
|        - |  9660 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9661 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9662 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9663 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  9664 | `	return PH7_OK;` |
|        4 |  9665 |  |
|        - |  9666 | `/*` |
|        - |  9667 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  9668 | ` */` |
|        4 |  9669 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9670 |  |
|        5 |  9671 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9672 | `	ph7_exec_ctx *pExecCtx;` |
|        5 |  9673 | `	if( nArg < 1 ){` |
|      ! 0 |  9674 | `		return PH7_OK;` |
|        - |  9675 | `	}` |
|        5 |  9676 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 |  9677 | `	if( pExecCtx ){` |
|        5 |  9678 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  9679 | `		/* Clear the attribute so double-free is prevented */` |
|        5 |  9680 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 |  9681 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  9682 | `			SyString sAttrName;` |
|        - |  9683 | `			ph7_value *pAttr;` |
|        5 |  9684 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 |  9685 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 |  9686 | `			if( pAttr ){` |
|        5 |  9687 | `				PH7_MemObjRelease(pAttr);` |
|        2 |  9688 | `			}` |
|        2 |  9689 | `		}` |
|        2 |  9690 | `	}` |
|        5 |  9691 | `	return PH7_OK;` |
|        3 |  9692 |  |
|        - |  9693 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  9694 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  9695 |  |
|        - |  9696 | `	ph7_class_instance *pThis;` |
|      ! 0 |  9697 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  9698 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  9699 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  9700 |  |
|      ! 0 |  9701 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  9702 |  |
|        - |  9703 | `	ph7_class_instance *pThis;` |
|      ! 0 |  9704 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  9705 | `	ph7_exec_ctx *pCtx;` |
|        - |  9706 | `	ph7_vm_func *pFunc;` |
|        - |  9707 | `	ph7_value *pCallable;` |
|        - |  9708 | `	ph7_value *pCtxAttr;` |
|        - |  9709 | `	SyString sAttrName;` |
|        - |  9710 | `	/* Must not already be started */` |
|      ! 0 |  9711 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9712 | `	if( pCtx != 0 ){` |
|      ! 0 |  9713 | `		return SXERR_INVALID;` |
|        - |  9714 | `	}` |
|      ! 0 |  9715 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9716 | `		return SXERR_INVALID;` |
|        - |  9717 | `	}` |
|      ! 0 |  9718 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  9719 | `	/* Get the callable */` |
|      ! 0 |  9720 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  9721 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9722 | `	if( pCallable == 0 ){` |
|      ! 0 |  9723 | `		return SXERR_INVALID;` |
|        - |  9724 | `	}` |
|        - |  9725 | `	/* Resolve callable */` |
|      ! 0 |  9726 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  9727 | `		SyString sName;` |
|        - |  9728 | `		SyHashEntry *pEntry;` |
|      ! 0 |  9729 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  9730 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  9731 | `		if( pEntry == 0 ){` |
|      ! 0 |  9732 | `			return SXERR_NOTFOUND;` |
|        - |  9733 | `		}` |
|      ! 0 |  9734 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  9735 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9736 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  9737 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  9738 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  9739 | `		if( pMethod == 0 ){` |
|      ! 0 |  9740 | `			return SXERR_INVALID;` |
|        - |  9741 | `		}` |
|      ! 0 |  9742 | `		pClosureThis = pClosure;` |
|      ! 0 |  9743 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  9744 | `	}else{` |
|      ! 0 |  9745 | `		return SXERR_INVALID;` |
|        - |  9746 | `	}` |
|        - |  9747 | `	/* Create context */` |
|      ! 0 |  9748 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  9749 | `	if( pCtx == 0 ){` |
|      ! 0 |  9750 | `		return SXERR_MEM;` |
|        - |  9751 | `	}` |
|        - |  9752 | `	/* Store in __ctx */` |
|      ! 0 |  9753 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  9754 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9755 | `	if( pCtxAttr ){` |
|      ! 0 |  9756 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  9757 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  9758 | `	}` |
|        - |  9759 | `	/* Set up frame with args */` |
|      ! 0 |  9760 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  9761 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  9762 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  9763 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  9764 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  9765 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  9766 |  |
|      ! 0 |  9767 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  9768 |  |
|      ! 0 |  9769 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9770 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  9771 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  9772 |  |
|      ! 0 |  9773 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9774 |  |
|      ! 0 |  9775 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9776 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  9777 |  |
|      ! 0 |  9778 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9779 |  |
|      ! 0 |  9780 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9781 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  9782 |  |
|      ! 0 |  9783 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9784 |  |
|      ! 0 |  9785 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9786 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  9787 | `	return &pCtx->sRetValue;` |
|      ! 0 |  9788 |  |
|        - |  9789 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  9790 | `/*` |
|        - |  9791 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  9792 | ` */` |
|       22 |  9793 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 |  9794 |  |
|        - |  9795 | `	ph7_generator *pGen;` |
|       24 |  9796 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       24 |  9797 | `	if( pGen == 0 ){` |
|      ! 0 |  9798 | `		return 0;` |
|        - |  9799 | `	}` |
|       24 |  9800 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       24 |  9801 | `	pGen->pCtx = pCtx;` |
|       24 |  9802 | `	pGen->iImplicitKey = 0;` |
|       24 |  9803 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       24 |  9804 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  9805 | `	/* Link the generator back to the exec context */` |
|       24 |  9806 | `	pCtx->pPrivate = pGen;` |
|       24 |  9807 | `	return pGen;` |
|       13 |  9808 |  |
|        - |  9809 | `/*` |
|        - |  9810 | ` * Release a generator and its execution context.` |
|        - |  9811 | ` */` |
|      ! 0 |  9812 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  9813 |  |
|      ! 0 |  9814 | `	if( pGen == 0 ){` |
|      ! 0 |  9815 | `		return;` |
|        - |  9816 | `	}` |
|      ! 0 |  9817 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  9818 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  9819 | `	if( pGen->pCtx ){` |
|      ! 0 |  9820 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  9821 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  9822 | `		pGen->pCtx = 0;` |
|      ! 0 |  9823 | `	}` |
|      ! 0 |  9824 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  9825 |  |
|        - |  9826 | `/*` |
|        - |  9827 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  9828 | ` */` |
|      236 |  9829 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 |  9830 |  |
|        - |  9831 | `	ph7_class_instance *pThis;` |
|        - |  9832 | `	SyString sAttr;` |
|        - |  9833 | `	ph7_value *pAttr;` |
|      238 |  9834 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9835 | `		return 0;` |
|        - |  9836 | `	}` |
|      238 |  9837 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 |  9838 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  9839 | `		return 0;` |
|        - |  9840 | `	}` |
|      238 |  9841 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 |  9842 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 |  9843 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  9844 | `		return 0;` |
|        - |  9845 | `	}` |
|      238 |  9846 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 |  9847 |  |
|        - |  9848 | `/*` |
|        - |  9849 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  9850 | ` */` |
|       22 |  9851 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9852 |  |
|        - |  9853 | `	ph7_generator *pGen;` |
|        - |  9854 | `	sxi32 rc;` |
|       24 |  9855 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 |  9856 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 |  9857 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 |  9858 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 |  9859 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 |  9860 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 |  9861 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 |  9862 | `	}` |
|       24 |  9863 | `	return PH7_OK;` |
|       13 |  9864 |  |
|        - |  9865 | `/*` |
|        - |  9866 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  9867 | ` */` |
|       68 |  9868 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9869 |  |
|        - |  9870 | `	ph7_generator *pGen;` |
|       70 |  9871 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 |  9872 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 |  9873 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 |  9874 | `	return PH7_OK;` |
|       36 |  9875 |  |
|        - |  9876 | `/*` |
|        - |  9877 | ` * Generator::current() — return the last yielded value.` |
|        - |  9878 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  9879 | ` */` |
|       68 |  9880 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9881 |  |
|        - |  9882 | `	ph7_generator *pGen;` |
|        - |  9883 | `	sxi32 rc;` |
|       70 |  9884 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 |  9885 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 |  9886 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 |  9887 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9888 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  9889 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  9890 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  9891 | `	}` |
|       70 |  9892 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 |  9893 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 |  9894 | `	}else{` |
|      ! 0 |  9895 | `		ph7_result_null(pCtx);` |
|        - |  9896 | `	}` |
|       70 |  9897 | `	return PH7_OK;` |
|       36 |  9898 |  |
|        - |  9899 | `/*` |
|        - |  9900 | ` * Generator::key() — return the last yielded key.` |
|        - |  9901 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  9902 | ` */` |
|       12 |  9903 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9904 |  |
|        - |  9905 | `	ph7_generator *pGen;` |
|        - |  9906 | `	sxi32 rc;` |
|       13 |  9907 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  9908 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  9909 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  9910 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9911 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  9912 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  9913 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  9914 | `	}` |
|       13 |  9915 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  9916 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  9917 | `	}else{` |
|      ! 0 |  9918 | `		ph7_result_null(pCtx);` |
|        - |  9919 | `	}` |
|       13 |  9920 | `	return PH7_OK;` |
|        7 |  9921 |  |
|        - |  9922 | `/*` |
|        - |  9923 | ` * Generator::next() — advance to the next yield point.` |
|        - |  9924 | ` */` |
|       60 |  9925 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9926 |  |
|        - |  9927 | `	ph7_generator *pGen;` |
|        - |  9928 | `	sxi32 rc;` |
|       62 |  9929 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 |  9930 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 |  9931 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 |  9932 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9933 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 |  9934 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 |  9935 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 |  9936 | `	}else{` |
|      ! 0 |  9937 | `		return PH7_OK;` |
|        - |  9938 | `	}` |
|       62 |  9939 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 |  9940 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 |  9941 | `	return PH7_OK;` |
|       32 |  9942 |  |
|        - |  9943 | `/*` |
|        - |  9944 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  9945 | ` */` |
|        4 |  9946 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9947 |  |
|        - |  9948 | `	ph7_generator *pGen;` |
|        - |  9949 | `	ph7_value *pSendVal;` |
|        - |  9950 | `	sxi32 rc;` |
|        5 |  9951 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  9952 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  9953 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  9954 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  9955 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  9956 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  9957 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  9958 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  9959 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  9960 | `	}else{` |
|      ! 0 |  9961 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9962 | `		return PH7_OK;` |
|        - |  9963 | `	}` |
|        5 |  9964 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  9965 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  9966 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  9967 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  9968 | `	}else{` |
|        3 |  9969 | `		ph7_result_null(pCtx);` |
|        - |  9970 | `	}` |
|        5 |  9971 | `	return PH7_OK;` |
|        3 |  9972 |  |
|        - |  9973 | `/*` |
|        - |  9974 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  9975 | ` *` |
|        - |  9976 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  9977 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  9978 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  9979 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  9980 | ` * the exception to the caller.` |
|        - |  9981 | ` */` |
|      ! 0 |  9982 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  9983 |  |
|        - |  9984 | `	ph7_generator *pGen;` |
|        - |  9985 | `	const char *zMsg;` |
|        - |  9986 | `	int nLen;` |
|      ! 0 |  9987 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  9988 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  9989 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  9990 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  9991 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  9992 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  9993 | `			"Cannot throw into a closed generator");` |
|        - |  9994 | `	}` |
|        - |  9995 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  9996 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  9997 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  9998 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  9999 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10000 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 | 10001 | `	nLen = 0;` |
|      ! 0 | 10002 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - | 10003 | `		/* Try to get the exception's message */` |
|        - | 10004 | `		SyString sAttr;` |
|        - | 10005 | `		ph7_value *pMsgAttr;` |
|      ! 0 | 10006 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 | 10007 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 | 10008 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 | 10009 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 | 10010 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 | 10011 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 | 10012 | `		}` |
|      ! 0 | 10013 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 10014 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 | 10015 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 | 10016 | `	}` |
|      ! 0 | 10017 | `	(void)nLen;` |
|      ! 0 | 10018 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 | 10019 |  |
|        - | 10020 | `/*` |
|        - | 10021 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - | 10022 | ` */` |
|        2 | 10023 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10024 |  |
|        - | 10025 | `	ph7_generator *pGen;` |
|        3 | 10026 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 10027 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 | 10028 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 10029 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10030 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 10031 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - | 10032 | `	}` |
|        3 | 10033 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 | 10034 | `	return PH7_OK;` |
|        2 | 10035 |  |
|        - | 10036 | `/*` |
|        - | 10037 | ` * Generator::__destruct() — clean up.` |
|        - | 10038 | ` */` |
|      ! 0 | 10039 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10040 |  |
|        - | 10041 | `	ph7_generator *pGen;` |
|      ! 0 | 10042 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 | 10043 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10044 | `	if( pGen ){` |
|      ! 0 | 10045 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 | 10046 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10047 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10048 | `			SyString sAttrName;` |
|        - | 10049 | `			ph7_value *pAttr;` |
|      ! 0 | 10050 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10051 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10052 | `			if( pAttr ){` |
|      ! 0 | 10053 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 | 10054 | `			}` |
|      ! 0 | 10055 | `		}` |
|      ! 0 | 10056 | `	}` |
|      ! 0 | 10057 | `	return PH7_OK;` |
|      ! 0 | 10058 |  |
|        - | 10059 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 10060 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 10061 | `/*` |
|        - | 10062 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 10063 | ` * the desired message.` |
|        - | 10064 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 10065 | ` * in 'api.c' for additional information.` |
|        - | 10066 | ` */` |
|      370 | 10067 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 10068 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 10069 | `	SyString *pString /* Message to output */` |
|        - | 10070 | `	)` |
|        2 | 10071 |  |
|      372 | 10072 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 | 10073 | `	sxi32 rc = SXRET_OK;` |
|        - | 10074 | `	/* Call the output consumer */` |
|      372 | 10075 | `	if( pString->nByte > 0 ){` |
|      372 | 10076 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 | 10077 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 | 10078 | `	}` |
|      372 | 10079 | `	return rc;` |
|        2 | 10080 |  |
|        - | 10081 | `/*` |
|        - | 10082 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 10083 | ` * callback to consume the formatted message.` |
|        - | 10084 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 10085 | ` * in 'api.c' for additional information.` |
|        - | 10086 | ` */` |
|        2 | 10087 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 10088 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 10089 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 10090 | `	va_list ap           /* Variable list of arguments */` |
|        - | 10091 | `	)` |
|        1 | 10092 |  |
|        3 | 10093 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 10094 | `	sxi32 rc = SXRET_OK;` |
|        - | 10095 | `	SyBlob sWorker;` |
|        - | 10096 | `	/* Format the message and call the output consumer */` |
|        3 | 10097 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 10098 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 10099 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 10100 | `		/* Consume the formatted message */` |
|        3 | 10101 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 10102 | `	}` |
|        3 | 10103 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 10104 | `	/* Release the working buffer */` |
|        3 | 10105 | `	SyBlobRelease(&sWorker);` |
|        3 | 10106 | `	return rc;` |
|        1 | 10107 |  |
|        - | 10108 | `/*` |
|        - | 10109 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 10110 | ` * This function never fail and always return a pointer` |
|        - | 10111 | ` * to a null terminated string.` |
|        - | 10112 | ` */` |
|       12 | 10113 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 10114 |  |
|       13 | 10115 | `	const char *zOp = "Unknown     ";` |
|       13 | 10116 | `	switch(nOp){` |
|        3 | 10117 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 10118 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 10119 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 10120 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 10121 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 10122 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 10123 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 10124 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 10125 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 10126 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 10127 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 10128 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 10129 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 10130 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 10131 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 10132 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 10133 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 10134 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 10135 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 10136 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 10137 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 10138 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 10139 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 10140 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 10141 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 10142 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 10143 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 10144 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 10145 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 10146 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 10147 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 10148 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 10149 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 10150 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 10151 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 10152 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 10153 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 10154 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 10155 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 10156 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 10157 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 10158 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 10159 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 10160 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 10161 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 10162 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 10163 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 10164 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 10165 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 10166 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 10167 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 10168 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 10169 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 10170 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 10171 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 10172 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 10173 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 10174 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 10175 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 10176 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 10177 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 10178 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 10179 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 10180 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 10181 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 10182 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 10183 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 10184 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 | 10185 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 | 10186 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 10187 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 10188 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 10189 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 10190 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 10191 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 10192 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 10193 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 10194 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 10195 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 10196 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 10197 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 10198 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 10199 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 10200 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 10201 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 10202 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 10203 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 10204 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 10205 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 10206 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 10207 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 10208 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 10209 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 10210 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 10211 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 10212 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 10213 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 10214 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 10215 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 10216 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 10217 | `	default:` |
|      ! 0 | 10218 | `		break;` |
|        - | 10219 | `	}` |
|       13 | 10220 | `	return zOp;` |
|        1 | 10221 |  |
|        - | 10222 | `/*` |
|        - | 10223 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 10224 | ` * The xConsumer() callback which is an used defined function` |
|        - | 10225 | ` * is responsible of consuming the generated dump.` |
|        - | 10226 | ` */` |
|        2 | 10227 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 10228 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 10229 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 10230 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 10231 | `	)` |
|        1 | 10232 |  |
|        - | 10233 | `	sxi32 rc;` |
|        3 | 10234 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 10235 | `	return rc;` |
|        1 | 10236 |  |
|        - | 10237 | `/*` |
|        - | 10238 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 10239 | ` * outside a class body [i.e: global or function scope].` |
|        - | 10240 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 10241 | ` * in 'compile.c' for additional information.` |
|        - | 10242 | ` */` |
|       14 | 10243 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 10244 |  |
|       15 | 10245 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 10246 | `	/* Evaluate and expand constant value */` |
|       15 | 10247 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 | 10248 |  |
|        - | 10249 | `/*` |
|        - | 10250 | ` * Section:` |
|        - | 10251 | ` *  Function handling functions.` |
|        - | 10252 | ` * Status:` |
|        - | 10253 | ` *    Stable.` |
|        - | 10254 | ` */` |
|        - | 10255 | `/*` |
|        - | 10256 | ` * int func_num_args(void)` |
|        - | 10257 | ` *   Returns the number of arguments passed to the function.` |
|        - | 10258 | ` * Parameters` |
|        - | 10259 | ` *   None.` |
|        - | 10260 | ` * Return` |
|        - | 10261 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 10262 | ` *  or -1 if called from the globe scope.` |
|        - | 10263 | ` */` |
|      960 | 10264 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10265 |  |
|        - | 10266 | `	VmFrame *pFrame;` |
|        - | 10267 | `	ph7_vm *pVm;` |
|        - | 10268 | `	/* Point to the target VM */` |
|      962 | 10269 | `	pVm = pCtx->pVm;` |
|        - | 10270 | `	/* Current frame */` |
|      962 | 10271 | `	pFrame = pVm->pFrame;` |
|      962 | 10272 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      962 | 10273 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 10274 | `		SXUNUSED(nArg);` |
|      ! 0 | 10275 | `		SXUNUSED(apArg);` |
|        - | 10276 | `		/* Global frame,return -1 */` |
|      ! 0 | 10277 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 10278 | `		return SXRET_OK;` |
|        - | 10279 | `	}` |
|        - | 10280 | `	/* Total number of arguments passed to the enclosing function */` |
|      962 | 10281 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      962 | 10282 | `	ph7_result_int(pCtx,nArg);` |
|      962 | 10283 | `	return SXRET_OK;` |
|      482 | 10284 |  |
|        - | 10285 | `/*` |
|        - | 10286 | ` * value func_get_arg(int $arg_num)` |
|        - | 10287 | ` *   Return an item from the argument list.` |
|        - | 10288 | ` * Parameters` |
|        - | 10289 | ` *  Argument number(index start from zero).` |
|        - | 10290 | ` * Return` |
|        - | 10291 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 10292 | ` */` |
|       22 | 10293 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10294 |  |
|       24 | 10295 | `	ph7_value *pObj = 0;` |
|       24 | 10296 | `	VmSlot *pSlot = 0;` |
|        - | 10297 | `	VmFrame *pFrame;` |
|        - | 10298 | `	ph7_vm *pVm;` |
|        - | 10299 | `	/* Point to the target VM */` |
|       24 | 10300 | `	pVm = pCtx->pVm;` |
|        - | 10301 | `	/* Current frame */` |
|       24 | 10302 | `	pFrame = pVm->pFrame;` |
|       24 | 10303 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 10304 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 10305 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 10306 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 10307 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10308 | `		return SXRET_OK;` |
|        - | 10309 | `	}` |
|        - | 10310 | `	/* Extract the desired index */` |
|       21 | 10311 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 10312 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 10313 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 10314 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10315 | `		return SXRET_OK;` |
|        - | 10316 | `	}` |
|        - | 10317 | `	/* Extract the desired argument */` |
|       21 | 10318 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 10319 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 10320 | `			/* Return the desired argument */` |
|       21 | 10321 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 10322 | `		}else{` |
|        - | 10323 | `			/* No such argument,return false */` |
|      ! 0 | 10324 | `			ph7_result_bool(pCtx,0);` |
|        - | 10325 | `		}` |
|       11 | 10326 | `	}else{` |
|        - | 10327 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 10328 | `		ph7_result_bool(pCtx,0);` |
|        - | 10329 | `	}` |
|       21 | 10330 | `	return SXRET_OK;` |
|       13 | 10331 |  |
|        - | 10332 | `/*` |
|        - | 10333 | ` * array func_get_args_byref(void)` |
|        - | 10334 | ` *   Returns an array comprising a function's argument list.` |
|        - | 10335 | ` * Parameters` |
|        - | 10336 | ` *  None.` |
|        - | 10337 | ` * Return` |
|        - | 10338 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 10339 | ` *  member of the current user-defined function's argument list.` |
|        - | 10340 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 10341 | ` * NOTE:` |
|        - | 10342 | ` *  Arguments are returned to the array by reference.` |
|        - | 10343 | ` */` |
|        2 | 10344 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10345 |  |
|        - | 10346 | `	ph7_value *pArray;` |
|        - | 10347 | `	VmFrame *pFrame;` |
|        - | 10348 | `	VmSlot *aSlot;` |
|        - | 10349 | `	sxu32 n;` |
|        - | 10350 | `	/* Point to the current frame */` |
|        3 | 10351 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 10352 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 10353 | `	if( pFrame->pParent == 0 ){` |
|        - | 10354 | `		/* Global frame,return FALSE */` |
|      ! 0 | 10355 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 10356 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10357 | `		return SXRET_OK;` |
|        - | 10358 | `	}` |
|        - | 10359 | `	/* Create a new array */` |
|        3 | 10360 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10361 | `	if( pArray == 0 ){` |
|      ! 0 | 10362 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10363 | `		SXUNUSED(apArg);` |
|      ! 0 | 10364 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10365 | `		return SXRET_OK;` |
|        - | 10366 | `	}` |
|        - | 10367 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 10368 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 10369 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 10370 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 10371 | `	}` |
|        - | 10372 | `	/* Return the freshly created array */` |
|        3 | 10373 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10374 | `	return SXRET_OK;` |
|        2 | 10375 |  |
|        - | 10376 | `/*` |
|        - | 10377 | ` * array func_get_args(void)` |
|        - | 10378 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 10379 | ` * Parameters` |
|        - | 10380 | ` *  None.` |
|        - | 10381 | ` * Return` |
|        - | 10382 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 10383 | ` *  member of the current user-defined function's argument list.` |
|        - | 10384 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 10385 | ` */` |
|       88 | 10386 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10387 |  |
|       90 | 10388 | `	ph7_value *pObj = 0;` |
|        - | 10389 | `	ph7_value *pArray;` |
|        - | 10390 | `	VmFrame *pFrame;` |
|        - | 10391 | `	VmSlot *aSlot;` |
|        - | 10392 | `	sxu32 n;` |
|        - | 10393 | `	/* Point to the current frame */` |
|       90 | 10394 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 | 10395 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 | 10396 | `	if( pFrame->pParent == 0 ){` |
|        - | 10397 | `		/* Global frame,return FALSE */` |
|      ! 0 | 10398 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 10399 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10400 | `		return SXRET_OK;` |
|        - | 10401 | `	}` |
|        - | 10402 | `	/* Create a new array */` |
|       90 | 10403 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 | 10404 | `	if( pArray == 0 ){` |
|      ! 0 | 10405 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10406 | `		SXUNUSED(apArg);` |
|      ! 0 | 10407 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10408 | `		return SXRET_OK;` |
|        - | 10409 | `	}` |
|        - | 10410 | `	/* Start filling the array with the given arguments */` |
|       90 | 10411 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 | 10412 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 | 10413 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 | 10414 | `		if( pObj ){` |
|      134 | 10415 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 10416 | `		}` |
|       68 | 10417 | `	}` |
|        - | 10418 | `	/* Return the freshly created array */` |
|       90 | 10419 | `	ph7_result_value(pCtx,pArray);` |
|       90 | 10420 | `	return SXRET_OK;` |
|       46 | 10421 |  |
|        - | 10422 | `/*` |
|        - | 10423 | ` * bool function_exists(string $name)` |
|        - | 10424 | ` *  Return TRUE if the given function has been defined.` |
|        - | 10425 | ` * Parameters` |
|        - | 10426 | ` *  The name of the desired function.` |
|        - | 10427 | ` * Return` |
|        - | 10428 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 10429 | ` */` |
|     1712 | 10430 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10431 |  |
|        - | 10432 | `	const char *zName;` |
|        - | 10433 | `	ph7_vm *pVm;` |
|        - | 10434 | `	int nLen;` |
|        - | 10435 | `	int res;` |
|     1714 | 10436 | `	if( nArg < 1 ){` |
|        - | 10437 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 10438 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10439 | `		return SXRET_OK;` |
|        - | 10440 | `	}` |
|        - | 10441 | `	/* Point to the target VM */` |
|     1714 | 10442 | `	pVm = pCtx->pVm;` |
|        - | 10443 | `	/* Extract the function name */` |
|     1714 | 10444 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 10445 | `	/* Assume the function is not defined */` |
|     1714 | 10446 | `	res = 0;` |
|        - | 10447 | `	/* Perform the lookup */` |
|     2568 | 10448 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1708 | 10449 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 10450 | `			/* Function is defined */` |
|      238 | 10451 | `			res = 1;` |
|      118 | 10452 | `	}` |
|     1714 | 10453 | `	ph7_result_bool(pCtx,res);` |
|     1714 | 10454 | `	return SXRET_OK;` |
|      858 | 10455 |  |
|        - | 10456 | `/*` |
|        - | 10457 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 10458 | ` * [i.e: Whether it is callable or not].` |
|        - | 10459 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 10460 | ` */` |
|    21524 | 10461 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 | 10462 |  |
|    21526 | 10463 | `	int res = 0;` |
|    21526 | 10464 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 10465 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|        - | 10466 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|        - | 10467 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|        - | 10468 | `		 * standard PHP behavior. */` |
|       20 | 10469 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|       20 | 10470 | `		if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|       18 | 10471 | `			res = 1;` |
|       10 | 10472 | `		}` |
|        9 | 10473 | `		(void)CallInvoke;` |
|    21517 | 10474 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 | 10475 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 | 10476 | `		if( pMap->nEntry == 2 ){` |
|        - | 10477 | `			ph7_class *pClass;` |
|        - | 10478 | `			ph7_value *pV;` |
|        - | 10479 | `			/* Extract the target class */` |
|       12 | 10480 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 | 10481 | `			if( pV ){` |
|       12 | 10482 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 | 10483 | `				if( pClass ){` |
|        - | 10484 | `					ph7_class_method *pMethod;` |
|        - | 10485 | `					/* Extract the target method */` |
|       10 | 10486 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 10487 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 10488 | `						/* Perform the lookup */` |
|       10 | 10489 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 10490 | `						if( pMethod ){` |
|        - | 10491 | `							/* Method is callable */` |
|        5 | 10492 | `							res = 1;` |
|        2 | 10493 | `						}` |
|        4 | 10494 | `					}` |
|        4 | 10495 | `				}` |
|        5 | 10496 | `			}` |
|        7 | 10497 | `		}` |
|    21495 | 10498 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 10499 | `		const char *zName;` |
|        - | 10500 | `		int nLen;` |
|        - | 10501 | `		/* Extract the name */` |
|     5650 | 10502 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 10503 | `		/* Perform the lookup */` |
|     5665 | 10504 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 10505 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 10506 | `				/* Function is callable */` |
|     5632 | 10507 | `				res = 1;` |
|     2815 | 10508 | `		}` |
|     2824 | 10509 | `	}` |
|    21526 | 10510 | `	return res;` |
|        2 | 10511 |  |
|        - | 10512 | `/*` |
|        - | 10513 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 10514 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 10515 | ` * Parameters` |
|        - | 10516 | ` * $name` |
|        - | 10517 | ` *    The callback function to check` |
|        - | 10518 | ` * $syntax_only` |
|        - | 10519 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 10520 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 10521 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 10522 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 10523 | ` *    a string.` |
|        - | 10524 | ` * Return` |
|        - | 10525 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 10526 | ` */` |
|       20 | 10527 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10528 |  |
|        - | 10529 | `	ph7_vm *pVm;` |
|        - | 10530 | `	int res;` |
|       21 | 10531 | `	if( nArg < 1 ){` |
|        - | 10532 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 10533 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10534 | `		return SXRET_OK;` |
|        - | 10535 | `	}` |
|        - | 10536 | `	/* Point to the target VM */` |
|       21 | 10537 | `	pVm = pCtx->pVm;` |
|        - | 10538 | `	/* Perform the requested operation */` |
|       21 | 10539 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       21 | 10540 | `	ph7_result_bool(pCtx,res);` |
|       21 | 10541 | `	return SXRET_OK;` |
|       11 | 10542 |  |
|        - | 10543 | `/*` |
|        - | 10544 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 10545 | ` * defined below.` |
|        - | 10546 | ` */` |
|     1228 | 10547 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 10548 |  |
|     1229 | 10549 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 10550 | `	ph7_value sName;` |
|        - | 10551 | `	sxi32 rc;` |
|        - | 10552 | `	/* Prepare the function name for insertion */` |
|     1229 | 10553 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1229 | 10554 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 10555 | `	/* Perform the insertion */` |
|     1229 | 10556 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1229 | 10557 | `	PH7_MemObjRelease(&sName);` |
|     1229 | 10558 | `	return rc;` |
|        1 | 10559 |  |
|        - | 10560 | `/*` |
|        - | 10561 | ` * array get_defined_functions(void)` |
|        - | 10562 | ` *  Returns an array of all defined functions.` |
|        - | 10563 | ` * Parameter` |
|        - | 10564 | ` *  None.` |
|        - | 10565 | ` * Return` |
|        - | 10566 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 10567 | ` *  both built-in (internal) and user-defined.` |
|        - | 10568 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 10569 | ` *  defined ones using $arr["user"].` |
|        - | 10570 | ` * Note:` |
|        - | 10571 | ` *  NULL is returned on failure.` |
|        - | 10572 | ` */` |
|        2 | 10573 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10574 |  |
|        - | 10575 | `	ph7_value *pArray,*pEntry;` |
|        - | 10576 | `	/* NOTE:` |
|        - | 10577 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 10578 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 10579 | `	 */` |
|        3 | 10580 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10581 | ` 	if( pArray == 0 ){` |
|      ! 0 | 10582 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10583 | `		SXUNUSED(apArg);` |
|        - | 10584 | `		/* Return NULL */` |
|      ! 0 | 10585 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10586 | `		return SXRET_OK;` |
|        - | 10587 | `	}` |
|        3 | 10588 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 10589 | `	if( pEntry == 0 ){` |
|        - | 10590 | `		/* Return NULL */` |
|      ! 0 | 10591 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10592 | `		return SXRET_OK;` |
|        - | 10593 | `	}` |
|        - | 10594 | `	/* Fill with the appropriate information */` |
|        3 | 10595 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 10596 | `	/* Create the 'internal' index */` |
|        3 | 10597 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 10598 | `	/* Create the user-func array */` |
|        3 | 10599 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 10600 | `	if( pEntry == 0 ){` |
|        - | 10601 | `		/* Return NULL */` |
|      ! 0 | 10602 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10603 | `		return SXRET_OK;` |
|        - | 10604 | `	}` |
|        - | 10605 | `	/* Fill with the appropriate information */` |
|        3 | 10606 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 10607 | `	/* Create the 'user' index */` |
|        3 | 10608 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 10609 | `	/* Return the multi-dimensional array */` |
|        3 | 10610 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10611 | `	return SXRET_OK;` |
|        2 | 10612 |  |
|        - | 10613 | `/*` |
|        - | 10614 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 10615 | ` *  Register a function for execution on shutdown.` |
|        - | 10616 | ` * Note` |
|        - | 10617 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 10618 | ` *  be called in the same order as they were registered.` |
|        - | 10619 | ` * Parameters` |
|        - | 10620 | ` *  $callback` |
|        - | 10621 | ` *   The shutdown callback to register.` |
|        - | 10622 | ` * $param` |
|        - | 10623 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 10624 | ` * Return` |
|        - | 10625 | ` *  Nothing.` |
|        - | 10626 | ` */` |
|        2 | 10627 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10628 |  |
|        - | 10629 | `	VmShutdownCB sEntry;` |
|        - | 10630 | `	int i,j;` |
|        3 | 10631 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 10632 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 10633 | `		return PH7_OK;` |
|        - | 10634 | `	}` |
|        - | 10635 | `	/* Zero the Entry */` |
|        3 | 10636 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 10637 | `	/* Initialize fields */` |
|        3 | 10638 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 10639 | `	/* Save the callback name for later invocation name */` |
|        3 | 10640 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 | 10641 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 | 10642 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 | 10643 | `	}` |
|        - | 10644 | `	/* Copy arguments */` |
|        3 | 10645 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 10646 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 10647 | `			/* Limit reached */` |
|      ! 0 | 10648 | `			break;` |
|        - | 10649 | `		}` |
|      ! 0 | 10650 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 10651 | `	}` |
|        3 | 10652 | `	sEntry.nArg = j;` |
|        - | 10653 | `	/* Install the callback */` |
|        3 | 10654 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 | 10655 | `	return PH7_OK;` |
|        2 | 10656 |  |
|        - | 10657 | `/*` |
|        - | 10658 | ` * Section:` |
|        - | 10659 | ` *  Class handling functions.` |
|        - | 10660 | ` * Status:` |
|        - | 10661 | ` *    Stable.` |
|        - | 10662 | ` */` |
|        - | 10663 | `/*` |
|        - | 10664 | ` * Extract the top active class. NULL is returned` |
|        - | 10665 | ` * if the class stack is empty.` |
|        - | 10666 | ` */` |
|      890 | 10667 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 10668 |  |
|      892 | 10669 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 10670 | `	ph7_class **apClass;` |
|      892 | 10671 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 10672 | `		/* Empty stack,return NULL */` |
|       15 | 10673 | `		return 0;` |
|        - | 10674 | `	}` |
|        - | 10675 | `	/* Peek the last entry */` |
|      878 | 10676 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      878 | 10677 | `	return apClass[pSet->nUsed - 1];` |
|      447 | 10678 |  |
|        - | 10679 | `/*` |
|        - | 10680 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 10681 | ` *   Get the class that declared the currently executing method.` |
|        - | 10682 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 10683 | ` *` |
|        - | 10684 | ` * Parameters` |
|        - | 10685 | ` *   pVm: Target VM` |
|        - | 10686 | ` *` |
|        - | 10687 | ` * Return` |
|        - | 10688 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 10689 | ` *   - Not executing within a class method` |
|        - | 10690 | ` *` |
|        - | 10691 | ` * Note` |
|        - | 10692 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 10693 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 10694 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 10695 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 10696 | ` *   declaring class.` |
|        - | 10697 | ` */` |
|       98 | 10698 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 10699 |  |
|      100 | 10700 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10701 | `	ph7_vm_func *pVmFunc;` |
|        - | 10702 |  |
|        - | 10703 | `	/* Skip exception frames to find the actual method frame */` |
|      100 | 10704 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 10705 |  |
|        - | 10706 | `	/* Check if we're in a method context */` |
|      100 | 10707 | `	if( pFrame->pParent ){` |
|       96 | 10708 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       96 | 10709 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 10710 | `			/* Return the declaring class */` |
|       96 | 10711 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 10712 | `		}` |
|      ! 0 | 10713 | `	}` |
|        - | 10714 |  |
|        5 | 10715 | `	return 0;` |
|       51 | 10716 |  |
|        - | 10717 |  |
|        - | 10718 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 10719 | `/*` |
|        - | 10720 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 10721 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 10722 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 10723 | ` * return value indicates failure.` |
|        - | 10724 | ` */` |
|        - | 10725 | `/*` |
|        - | 10726 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 10727 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 10728 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 10729 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 10730 | ` */` |
|     2148 | 10731 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 10732 | `	ph7_vm *pVm,` |
|        - | 10733 | `	ph7_class_instance *pThis,` |
|        - | 10734 | `	ph7_class_method *pMethod,` |
|        - | 10735 | `	ph7_value *pResult,` |
|        - | 10736 | `	int nArg,` |
|        - | 10737 | `	ph7_value **apArg,` |
|        - | 10738 | `	VmCallArgMap *pMap` |
|        - | 10739 | `	)` |
|        2 | 10740 |  |
|        - | 10741 | `	ph7_value *aStack;` |
|        - | 10742 | `	VmInstr aInstr[2];` |
|        - | 10743 | `	int iCursor;` |
|        - | 10744 | `	int i;` |
|     2150 | 10745 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     2150 | 10746 | `	if( aStack == 0 ){` |
|      ! 0 | 10747 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 10748 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 10749 | `		return SXERR_MEM;` |
|        - | 10750 | `	}` |
|     3378 | 10751 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1230 | 10752 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     1230 | 10753 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      616 | 10754 | `	}` |
|     2150 | 10755 | `	iCursor = nArg + 1;` |
|     2150 | 10756 | `	if( pThis ){` |
|     2144 | 10757 | `		pThis->iRef++;` |
|     2144 | 10758 | `		aStack[i].x.pOther = pThis;` |
|     2144 | 10759 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|     1071 | 10760 | `	}` |
|     2150 | 10761 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2150 | 10762 | `	i++;` |
|     2150 | 10763 | `	SyBlobReset(&aStack[i].sBlob);` |
|     2150 | 10764 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     2150 | 10765 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     2150 | 10766 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2150 | 10767 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     2150 | 10768 | `	aInstr[0].iP1 = nArg;` |
|     2150 | 10769 | `	aInstr[0].iP2 = 0;` |
|     2150 | 10770 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     2150 | 10771 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     2150 | 10772 | `	aInstr[1].iP1 = 1;` |
|     2150 | 10773 | `	aInstr[1].iP2 = 0;` |
|     2150 | 10774 | `	aInstr[1].p3  = 0;` |
|     2150 | 10775 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0);` |
|     2150 | 10776 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     2150 | 10777 | `	return PH7_OK;` |
|     1076 | 10778 |  |
|     1686 | 10779 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 10780 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 10781 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 10782 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 10783 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 10784 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 10785 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 10786 | `	)` |
|        2 | 10787 |  |
|     1688 | 10788 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 10789 |  |
|        - | 10790 | `/*` |
|        - | 10791 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|        - | 10792 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|        - | 10793 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|        - | 10794 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|        - | 10795 | ` *` |
|        - | 10796 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|        - | 10797 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|        - | 10798 | ` * is_callable / closure-invoke paths follow the same rule.` |
|        - | 10799 | ` *` |
|        - | 10800 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|        - | 10801 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|        - | 10802 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|        - | 10803 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|        - | 10804 | ` *` |
|        - | 10805 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|        - | 10806 | ` */` |
|      166 | 10807 | `static sxi32 VmCallObjectInvoke(` |
|        - | 10808 | `	ph7_vm *pVm,` |
|        - | 10809 | `	ph7_class_instance *pThis,` |
|        - | 10810 | `	int nArg,` |
|        - | 10811 | `	ph7_value **apArg,` |
|        - | 10812 | `	ph7_value *pResult,` |
|        - | 10813 | `	VmCallArgMap *pMap` |
|        - | 10814 | `	)` |
|        2 | 10815 |  |
|        - | 10816 | `	ph7_class_method *pMethod;` |
|      168 | 10817 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      168 | 10818 | `	if( pMethod == 0 ){` |
|       13 | 10819 | `		if( pResult ){` |
|       13 | 10820 | `			PH7_MemObjRelease(pResult);` |
|        6 | 10821 | `		}` |
|       13 | 10822 | `		return SXERR_INVALID;` |
|        - | 10823 | `	}` |
|      156 | 10824 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       85 | 10825 |  |
|        - | 10826 | `/*` |
|        - | 10827 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|        - | 10828 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|        - | 10829 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|        - | 10830 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|        - | 10831 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|        - | 10832 | ` * lookup or 'goto Exception').` |
|        - | 10833 | ` *` |
|        - | 10834 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|        - | 10835 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|        - | 10836 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|        - | 10837 | ` * reported.` |
|        - | 10838 | ` */` |
|       12 | 10839 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        1 | 10840 |  |
|        - | 10841 | `	ph7_class *pErrorClass;` |
|       13 | 10842 | `	ph7_class_instance *pErrInst = 0;` |
|        - | 10843 | `	ph7_class_method *pCons;` |
|        - | 10844 | `	VmFrame *pThrowFrame;` |
|        - | 10845 | `	char zMsg[256];` |
|        - | 10846 | `	int nMsg;` |
|        - | 10847 | `	sxi32 rc;` |
|       25 | 10848 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - | 10849 | `		"Object of type %.*s is not callable",` |
|       12 | 10850 | `		(int)pThis->pClass->sName.nByte,` |
|       12 | 10851 | `		pThis->pClass->sName.zString);` |
|       13 | 10852 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|       13 | 10853 | `	if( pErrorClass ){` |
|       13 | 10854 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|        6 | 10855 | `	}` |
|       13 | 10856 | `	if( pErrInst == 0 ){` |
|        - | 10857 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|        - | 10858 | `		 * should always be available, so this branch is effectively unreachable.` |
|        - | 10859 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|        - | 10860 | `		 * visible to the user. */` |
|      ! 0 | 10861 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|      ! 0 | 10862 | `		return SXERR_ABORT;` |
|        - | 10863 | `	}` |
|       13 | 10864 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|       13 | 10865 | `	if( pCons ){` |
|        - | 10866 | `		ph7_value sArg;` |
|        - | 10867 | `		ph7_value *apMsg[1];` |
|        - | 10868 | `		SyString sMsgStr;` |
|       13 | 10869 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|       13 | 10870 | `		PH7_MemObjInit(pVm,&sArg);` |
|       13 | 10871 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|       13 | 10872 | `		apMsg[0] = &sArg;` |
|       13 | 10873 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|       13 | 10874 | `		PH7_MemObjRelease(&sArg);` |
|        6 | 10875 | `	}` |
|        - | 10876 | `	/* Else: Error::__construct is part of the built-in library and should` |
|        - | 10877 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|        - | 10878 | `	 * with an empty getMessage() rather than crashing. */` |
|       13 | 10879 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       13 | 10880 | `	if( pThrowFrame ){` |
|       13 | 10881 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 | 10882 | `	}` |
|       13 | 10883 | `	rc = VmThrowException(pVm,pErrInst);` |
|       13 | 10884 | `	PH7_ClassInstanceUnref(pErrInst);` |
|       13 | 10885 | `	return rc;` |
|        7 | 10886 |  |
|        - | 10887 | `/*` |
|        - | 10888 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 10889 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 10890 | ` * in the apArg[] array.` |
|        - | 10891 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 10892 | ` * return value indicates failure.` |
|        - | 10893 | ` */` |
|     1100 | 10894 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 10895 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 10896 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 10897 | `	int nArg,          /* Total number of given arguments */` |
|        - | 10898 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 10899 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 10900 | `	)` |
|        2 | 10901 |  |
|        - | 10902 | `	ph7_value *aStack;` |
|        - | 10903 | `	VmInstr aInstr[2];` |
|        - | 10904 | `	int i;` |
|     1102 | 10905 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|        - | 10906 | `		/* Object callable: dispatch through __invoke when available.` |
|        - | 10907 | `		 * No VmCallArgMap: call_user_func / array_map / usort and the C API` |
|        - | 10908 | `		 * pass arguments positionally and inherit no strict-types context. */` |
|      137 | 10909 | `		return VmCallObjectInvoke(&(*pVm),` |
|       90 | 10910 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|       45 | 10911 | `			nArg,apArg,pResult,0);` |
|        - | 10912 | `	}` |
|     1012 | 10913 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 10914 | `		/* Don't bother processing,it's invalid anyway */` |
|      509 | 10915 | `		if( pResult ){` |
|        - | 10916 | `			/* Assume a null return value */` |
|      ! 0 | 10917 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 10918 | `		}` |
|      509 | 10919 | `		return SXERR_INVALID;` |
|        - | 10920 | `	}` |
|      504 | 10921 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 10922 | `		/* Class method */` |
|       11 | 10923 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 | 10924 | `		ph7_class_method *pMethod = 0;` |
|       11 | 10925 | `		ph7_class_instance *pThis = 0;` |
|       11 | 10926 | `		ph7_class *pClass = 0;` |
|        - | 10927 | `		ph7_value *pValue;` |
|        - | 10928 | `		sxi32 rc;` |
|       11 | 10929 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 10930 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 10931 | `			if( pResult ){` |
|        - | 10932 | `				/* Assume a null return value */` |
|      ! 0 | 10933 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10934 | `			}` |
|      ! 0 | 10935 | `			return SXRET_OK;` |
|        - | 10936 | `		}` |
|        - | 10937 | `		/* Extract the class name or an instance of it */` |
|       11 | 10938 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 | 10939 | `		if( pValue ){` |
|       11 | 10940 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 | 10941 | `		}` |
|       11 | 10942 | `		if( pClass == 0 ){` |
|        - | 10943 | `			/* No such class,return NULL */` |
|      ! 0 | 10944 | `			if( pResult ){` |
|      ! 0 | 10945 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10946 | `			}` |
|      ! 0 | 10947 | `			return SXRET_OK;` |
|        - | 10948 | `		}` |
|       11 | 10949 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 10950 | `			/* Point to the class instance */` |
|        5 | 10951 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 | 10952 | `		}` |
|        - | 10953 | `		/* Try to extract the method */` |
|       11 | 10954 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 | 10955 | `		if( pValue ){` |
|       11 | 10956 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 | 10957 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 | 10958 | `					SyBlobLength(&pValue->sBlob));` |
|        5 | 10959 | `			}` |
|        5 | 10960 | `		}` |
|       11 | 10961 | `		if( pMethod == 0 ){` |
|        - | 10962 | `			/* No such method,return NULL */` |
|      ! 0 | 10963 | `			if( pResult ){` |
|      ! 0 | 10964 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10965 | `			}` |
|      ! 0 | 10966 | `			return SXRET_OK;` |
|        - | 10967 | `		}` |
|        - | 10968 | `		/* Call the class method */` |
|       11 | 10969 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 | 10970 | `		return rc;` |
|        - | 10971 | `	}` |
|        - | 10972 | `	/* Create a new operand stack */` |
|      494 | 10973 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      494 | 10974 | `	if( aStack == 0 ){` |
|      ! 0 | 10975 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 10976 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 10977 | `		if( pResult ){` |
|        - | 10978 | `			/* Assume a null return value */` |
|      ! 0 | 10979 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 10980 | `		}` |
|      ! 0 | 10981 | `		return SXERR_MEM;` |
|        - | 10982 | `	}` |
|        - | 10983 | `	/* Fill the operand stack with the given arguments */` |
|     1604 | 10984 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1112 | 10985 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 10986 | `		/*` |
|        - | 10987 | `		 * Symisc eXtension:` |
|        - | 10988 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 10989 | `		 */` |
|     1112 | 10990 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      557 | 10991 | `	}` |
|        - | 10992 | `	/* Push the function name */` |
|      494 | 10993 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      494 | 10994 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 10995 | `	/* Emit the CALL istruction */` |
|      494 | 10996 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      494 | 10997 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      494 | 10998 | `	aInstr[0].iP2 = 0;` |
|      494 | 10999 | `	aInstr[0].p3  = 0;` |
|        - | 11000 | `	/* Emit the DONE instruction */` |
|      494 | 11001 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      494 | 11002 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      494 | 11003 | `	aInstr[1].iP2 = 0;` |
|      494 | 11004 | `	aInstr[1].p3  = 0;` |
|        - | 11005 | `	/* Execute the function body (if available) */` |
|      494 | 11006 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0);` |
|        - | 11007 | `	/* Clean up the mess left behind */` |
|      494 | 11008 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      494 | 11009 | `	return PH7_OK;` |
|      552 | 11010 |  |
|        - | 11011 | `/*` |
|        - | 11012 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 11013 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 11014 | ` * parameter.` |
|        - | 11015 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 11016 | ` * return value indicates failure.` |
|        - | 11017 | ` */` |
|      236 | 11018 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 11019 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 11020 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 11021 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 11022 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 11023 | `	)` |
|        1 | 11024 |  |
|        - | 11025 | `	ph7_value *pArg;` |
|        - | 11026 | `	SySet aArg;` |
|        - | 11027 | `	va_list ap;` |
|        - | 11028 | `	sxi32 rc;` |
|      237 | 11029 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 11030 | `	/* Copy arguments one after one */` |
|      237 | 11031 | `	va_start(ap,pResult);` |
|      393 | 11032 | `	for(;;){` |
|      787 | 11033 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 | 11034 | `		if( pArg == 0 ){` |
|      237 | 11035 | `			break;` |
|        - | 11036 | `		}` |
|      551 | 11037 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 11038 | `	}` |
|        - | 11039 | `	/* Call the core routine */` |
|      237 | 11040 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 11041 | `	/* Cleanup */` |
|      237 | 11042 | `	SySetRelease(&aArg);` |
|      237 | 11043 | `	return rc;` |
|        1 | 11044 |  |
|        - | 11045 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 11046 | `/*` |
|        - | 11047 | ` * bool defined(string $name)` |
|        - | 11048 | ` *  Checks whether a given named constant exists.` |
|        - | 11049 | ` * Parameter:` |
|        - | 11050 | ` *  Name of the desired constant.` |
|        - | 11051 | ` * Return` |
|        - | 11052 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 11053 | ` */` |
|       14 | 11054 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11055 |  |
|        - | 11056 | `	const char *zName;` |
|       16 | 11057 | `	int nLen = 0;` |
|       16 | 11058 | `	int res = 0;` |
|       16 | 11059 | `	if( nArg < 1 ){` |
|        - | 11060 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 11061 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 11062 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11063 | `		return SXRET_OK;` |
|        - | 11064 | `	}` |
|        - | 11065 | `	/* Extract constant name */` |
|       16 | 11066 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11067 | `	/* Perform the lookup */` |
|       16 | 11068 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11069 | `		/* Already defined */` |
|       10 | 11070 | `		res = 1;` |
|        4 | 11071 | `	}` |
|       16 | 11072 | `	ph7_result_bool(pCtx,res);` |
|       16 | 11073 | `	return SXRET_OK;` |
|        9 | 11074 |  |
|        - | 11075 | `/*` |
|        - | 11076 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 11077 | ` * below.` |
|        - | 11078 | ` */` |
|       10 | 11079 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 11080 |  |
|       12 | 11081 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 11082 | `	/* Expand constant value */` |
|       12 | 11083 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 | 11084 |  |
|        - | 11085 | `/*` |
|        - | 11086 | ` * bool define(string $constant_name,expression value)` |
|        - | 11087 | ` *  Defines a named constant at runtime.` |
|        - | 11088 | ` * Parameter:` |
|        - | 11089 | ` *  $constant_name` |
|        - | 11090 | ` *   The name of the constant` |
|        - | 11091 | ` *  $value` |
|        - | 11092 | ` *   Constant value` |
|        - | 11093 | ` * Return:` |
|        - | 11094 | ` *   TRUE on success,FALSE on failure.` |
|        - | 11095 | ` */` |
|       12 | 11096 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11097 |  |
|        - | 11098 | `	const char *zName;  /* Constant name */` |
|        - | 11099 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 | 11100 | `	int nLen = 0;       /* Name length */` |
|        - | 11101 | `	sxi32 rc;` |
|       14 | 11102 | `	if( nArg < 2 ){` |
|        - | 11103 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 11104 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 11105 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11106 | `		return SXRET_OK;` |
|        - | 11107 | `	}` |
|       14 | 11108 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 11109 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 11110 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11111 | `		return SXRET_OK;` |
|        - | 11112 | `	}` |
|        - | 11113 | `	/* Extract constant name */` |
|       14 | 11114 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 | 11115 | `	if( nLen < 1 ){` |
|      ! 0 | 11116 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 11117 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11118 | `		return SXRET_OK;` |
|        - | 11119 | `	}` |
|        - | 11120 | `	/* Duplicate constant value */` |
|       14 | 11121 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 | 11122 | `	if( pValue == 0 ){` |
|      ! 0 | 11123 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 11124 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11125 | `		return SXRET_OK;` |
|        - | 11126 | `	}` |
|        - | 11127 | `	/* Initialize the memory object */` |
|       14 | 11128 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 11129 | `	/* Register the constant */` |
|       14 | 11130 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 | 11131 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11132 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 11133 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 11134 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11135 | `		return SXRET_OK;` |
|        - | 11136 | `	}` |
|        - | 11137 | `	/* Duplicate constant value */` |
|       14 | 11138 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 | 11139 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 11140 | `		/* Lower case the constant name */` |
|      ! 0 | 11141 | `		char *zCur = (char *)zName;` |
|      ! 0 | 11142 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 11143 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 11144 | `				/* UTF-8 stream */` |
|      ! 0 | 11145 | `				zCur++;` |
|      ! 0 | 11146 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 11147 | `					zCur++;` |
|      ! 0 | 11148 | `				}` |
|      ! 0 | 11149 | `				continue;` |
|        - | 11150 | `			}` |
|      ! 0 | 11151 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 11152 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 11153 | `				zCur[0] = (char)c;` |
|      ! 0 | 11154 | `			}` |
|      ! 0 | 11155 | `			zCur++;` |
|      ! 0 | 11156 | `		}` |
|        - | 11157 | `		/* Finally,register the constant */` |
|      ! 0 | 11158 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 | 11159 | `	}` |
|        - | 11160 | `	/* All done,return TRUE */` |
|       14 | 11161 | `	ph7_result_bool(pCtx,1);` |
|       14 | 11162 | `	return SXRET_OK;` |
|        8 | 11163 |  |
|        - | 11164 | `/*` |
|        - | 11165 | ` * value constant(string $name)` |
|        - | 11166 | ` *  Returns the value of a constant` |
|        - | 11167 | ` * Parameter` |
|        - | 11168 | ` *  $name` |
|        - | 11169 | ` *    Name of the constant.` |
|        - | 11170 | ` * Return` |
|        - | 11171 | ` *  Constant value or NULL if not defined.` |
|        - | 11172 | ` */` |
|        8 | 11173 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11174 |  |
|        - | 11175 | `	SyHashEntry *pEntry;` |
|        - | 11176 | `	ph7_constant *pCons;` |
|        - | 11177 | `	const char *zName; /* Constant name */` |
|        - | 11178 | `	ph7_value sVal;    /* Constant value */` |
|        - | 11179 | `	int nLen;` |
|       10 | 11180 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11181 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 11182 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 11183 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11184 | `		return SXRET_OK;` |
|        - | 11185 | `	}` |
|        - | 11186 | `	/* Extract the constant name */` |
|       10 | 11187 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11188 | `	/* Perform the query */` |
|       10 | 11189 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 11190 | `	if( pEntry == 0 ){` |
|        3 | 11191 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 11192 | `		ph7_result_null(pCtx);` |
|        3 | 11193 | `		return SXRET_OK;` |
|        - | 11194 | `	}` |
|        8 | 11195 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 11196 | `	/* Point to the structure that describe the constant */` |
|        8 | 11197 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 11198 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 11199 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 11200 | `	/* Return that value */` |
|        8 | 11201 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 11202 | `	/* Cleanup */` |
|        8 | 11203 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 11204 | `	return SXRET_OK;` |
|        6 | 11205 |  |
|        - | 11206 | `/*` |
|        - | 11207 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 11208 | ` * defined below.` |
|        - | 11209 | ` */` |
|      452 | 11210 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11211 |  |
|      453 | 11212 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11213 | `	ph7_value sName;` |
|        - | 11214 | `	sxi32 rc;` |
|        - | 11215 | `	/* Prepare the constant name for insertion */` |
|      453 | 11216 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      453 | 11217 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11218 | `	/* Perform the insertion */` |
|      453 | 11219 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      453 | 11220 | `	PH7_MemObjRelease(&sName);` |
|      453 | 11221 | `	return rc;` |
|        1 | 11222 |  |
|        - | 11223 | `/*` |
|        - | 11224 | ` * array get_defined_constants(void)` |
|        - | 11225 | ` *  Returns an associative array with the names of all defined` |
|        - | 11226 | ` *  constants.` |
|        - | 11227 | ` * Parameters` |
|        - | 11228 | ` *  NONE.` |
|        - | 11229 | ` * Returns` |
|        - | 11230 | ` *  Returns the names of all the constants currently defined.` |
|        - | 11231 | ` */` |
|        2 | 11232 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11233 |  |
|        - | 11234 | `	ph7_value *pArray;` |
|        - | 11235 | `	/* Create the array first*/` |
|        3 | 11236 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11237 | `	if( pArray == 0 ){` |
|      ! 0 | 11238 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11239 | `		SXUNUSED(apArg);` |
|        - | 11240 | `		/* Return NULL */` |
|      ! 0 | 11241 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11242 | `		return SXRET_OK;` |
|        - | 11243 | `	}` |
|        - | 11244 | `	/* Fill the array with the defined constants */` |
|        3 | 11245 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 11246 | `	/* Return the created array */` |
|        3 | 11247 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11248 | `	return SXRET_OK;` |
|        2 | 11249 |  |
|        - | 11250 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 11251 | `/*` |
|        - | 11252 | ` * Section:` |
|        - | 11253 | ` *  Random numbers/string generators.` |
|        - | 11254 | ` * Status:` |
|        - | 11255 | ` *    Stable.` |
|        - | 11256 | ` */` |
|        - | 11257 | `/*` |
|        - | 11258 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 11259 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - | 11260 | ` * used by te SQLite3 library.` |
|        - | 11261 | ` */` |
|     2749 | 11262 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 11263 |  |
|        - | 11264 | `	sxu32 iNum;` |
|     2751 | 11265 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2751 | 11266 | `	return iNum;` |
|        2 | 11267 |  |
|        - | 11268 | `/*` |
|        - | 11269 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 11270 | ` * Note that the generated string is NOT null terminated.` |
|        - | 11271 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - | 11272 | ` * by te SQLite3 library.` |
|        - | 11273 | ` */` |
|   195242 | 11274 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 11275 |  |
|        - | 11276 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 11277 | `	int i;` |
|        - | 11278 | `	/* Generate a binary string first */` |
|   195244 | 11279 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 11280 | `	/* Turn the binary string into english based alphabet */` |
|  2147832 | 11281 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1952590 | 11282 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   976296 | 11283 | `	 }` |
|   195244 | 11284 |  |
|        - | 11285 | `/*` |
|        - | 11286 | ` * int rand()` |
|        - | 11287 | ` * int mt_rand()` |
|        - | 11288 | ` * int rand(int $min,int $max)` |
|        - | 11289 | ` * int mt_rand(int $min,int $max)` |
|        - | 11290 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 11291 | ` * Parameter` |
|        - | 11292 | ` *  $min` |
|        - | 11293 | ` *    The lowest value to return (default: 0)` |
|        - | 11294 | ` *  $max` |
|        - | 11295 | ` *   The highest value to return (default: getrandmax())` |
|        - | 11296 | ` * Return` |
|        - | 11297 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 11298 | ` * Note:` |
|        - | 11299 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11300 | ` *  by te SQLite3 library.` |
|        - | 11301 | ` */` |
|       20 | 11302 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11303 |  |
|        - | 11304 | `	sxu32 iNum;` |
|        - | 11305 | `	/* Generate the random number */` |
|       21 | 11306 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 11307 | `	if( nArg > 1 ){` |
|        - | 11308 | `		sxu32 iMin,iMax;` |
|        3 | 11309 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 11310 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 11311 | `		if( iMin < iMax ){` |
|        3 | 11312 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 11313 | `			if( iDiv > 0 ){` |
|        3 | 11314 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 11315 | `			}` |
|        1 | 11316 | `		}else if(iMax > 0 ){` |
|      ! 0 | 11317 | `			iNum %= iMax;` |
|      ! 0 | 11318 | `		}` |
|        1 | 11319 | `	}` |
|        - | 11320 | `	/* Return the number */` |
|       21 | 11321 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 11322 | `	return SXRET_OK;` |
|        1 | 11323 |  |
|        - | 11324 | `/*` |
|        - | 11325 | ` * int getrandmax(void)` |
|        - | 11326 | ` * int mt_getrandmax(void)` |
|        - | 11327 | ` * int rc4_getrandmax(void)` |
|        - | 11328 | ` *   Show largest possible random value` |
|        - | 11329 | ` * Return` |
|        - | 11330 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 11331 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 11332 | ` * Note:` |
|        - | 11333 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11334 | ` *  by te SQLite3 library.` |
|        - | 11335 | ` */` |
|        4 | 11336 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11337 |  |
|        2 | 11338 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 11339 | `	SXUNUSED(apArg);` |
|        5 | 11340 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 11341 | `	return SXRET_OK;` |
|        1 | 11342 |  |
|        - | 11343 | `/*` |
|        - | 11344 | ` * string rand_str()` |
|        - | 11345 | ` * string rand_str(int $len)` |
|        - | 11346 | ` *  Generate a random string (English alphabet).` |
|        - | 11347 | ` * Parameter` |
|        - | 11348 | ` *  $len` |
|        - | 11349 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 11350 | ` * Return` |
|        - | 11351 | ` *   A pseudo random string.` |
|        - | 11352 | ` * Note:` |
|        - | 11353 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11354 | ` *  by te SQLite3 library.` |
|        - | 11355 | ` *  This function is a symisc extension.` |
|        - | 11356 | ` */` |
|      120 | 11357 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11358 |  |
|        - | 11359 | `	char zString[1024];` |
|      122 | 11360 | `	int iLen = 0x10;` |
|      122 | 11361 | `	if( nArg > 0 ){` |
|        - | 11362 | `		/* Get the desired length */` |
|      122 | 11363 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 11364 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 11365 | `			/* Default length */` |
|        3 | 11366 | `			iLen = 0x10;` |
|        1 | 11367 | `		}` |
|       60 | 11368 | `	}` |
|        - | 11369 | `	/* Generate the random string */` |
|      122 | 11370 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 11371 | `	/* Return the generated string */` |
|      122 | 11372 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 11373 | `	return SXRET_OK;` |
|        2 | 11374 |  |
|        - | 11375 | `/*` |
|        - | 11376 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - | 11377 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - | 11378 | ` * an int (PHP coerces float and numeric string silently).` |
|        - | 11379 | ` */` |
|      488 | 11380 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 | 11381 |  |
|      488 | 11382 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      483 | 11383 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       10 | 11384 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11385 | `			"TypeError",` |
|        - | 11386 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|        3 | 11387 | `			zFunc,iArgPos,zParamName,` |
|        3 | 11388 | `			ph7_type_name(pArg)` |
|        - | 11389 | `			);` |
|        - | 11390 | `	}` |
|      483 | 11391 | `	if( ph7_value_is_string(pArg) ){` |
|        - | 11392 | `		int len;` |
|        9 | 11393 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 | 11394 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 | 11395 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11396 | `				"TypeError",` |
|        - | 11397 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 | 11398 | `				zFunc,iArgPos,zParamName` |
|        - | 11399 | `				);` |
|        - | 11400 | `		}` |
|        2 | 11401 | `	}` |
|      479 | 11402 | `	return SXRET_OK;` |
|      245 | 11403 |  |
|        - | 11404 | `/*` |
|        - | 11405 | ` * int random_int(int $min, int $max)` |
|        - | 11406 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - | 11407 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - | 11408 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - | 11409 | ` *  power-of-two mask covering the range.` |
|        - | 11410 | ` */` |
|      242 | 11411 | `static int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11412 |  |
|        - | 11413 | `	sxi64 iMin,iMax;` |
|        - | 11414 | `	sxu64 uRange,uMask,uResult;` |
|        - | 11415 | `	unsigned int nAttempt;` |
|        - | 11416 | `	int rc;` |
|      243 | 11417 | `	if( nArg != 2 ){` |
|       10 | 11418 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11419 | `			"ArgumentCountError",` |
|        - | 11420 | `			"random_int() expects exactly 2 arguments, %d given",` |
|        3 | 11421 | `			nArg` |
|        - | 11422 | `			);` |
|        - | 11423 | `	}` |
|      237 | 11424 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      237 | 11425 | `	if( rc != SXRET_OK ){ return rc; }` |
|      233 | 11426 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      233 | 11427 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 11428 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 | 11429 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 | 11430 | `	if( iMin > iMax ){` |
|        3 | 11431 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11432 | `			"ValueError",` |
|        - | 11433 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - | 11434 | `			);` |
|        - | 11435 | `	}` |
|      229 | 11436 | `	if( iMin == iMax ){` |
|        5 | 11437 | `		ph7_result_int64(pCtx,iMin);` |
|        5 | 11438 | `		return SXRET_OK;` |
|        - | 11439 | `	}` |
|      225 | 11440 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 | 11441 | `	uMask = uRange;` |
|      225 | 11442 | `	uMask \|= uMask >> 1;` |
|      225 | 11443 | `	uMask \|= uMask >> 2;` |
|      225 | 11444 | `	uMask \|= uMask >> 4;` |
|      225 | 11445 | `	uMask \|= uMask >> 8;` |
|      225 | 11446 | `	uMask \|= uMask >> 16;` |
|      225 | 11447 | `	uMask \|= uMask >> 32;` |
|      225 | 11448 | `	uResult = 0;` |
|      360 | 11449 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 11450 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 11451 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 11452 | `		 * and the low-half mask would always read 0). */` |
|        - | 11453 | `		sxu64 uDraw;` |
|      360 | 11454 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|        - | 11455 | `			/* PHP 8.2+ would throw Random\RandomException here; that class` |
|        - | 11456 | `			 * is not yet registered in PHL (see PLAN.md item 6.13). */` |
|      ! 0 | 11457 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11458 | `				"Exception",` |
|        - | 11459 | `				"Cannot gather sufficient random data"` |
|        - | 11460 | `				);` |
|        - | 11461 | `		}` |
|      360 | 11462 | `		uDraw &= uMask;` |
|      360 | 11463 | `		if( uDraw <= uRange ){` |
|      225 | 11464 | `			uResult = uDraw;` |
|      225 | 11465 | `			break;` |
|        - | 11466 | `		}` |
|       75 | 11467 | `	}` |
|      225 | 11468 | `	if( nAttempt >= 50 ){` |
|      ! 0 | 11469 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11470 | `			"Exception",` |
|        - | 11471 | `			"Cannot gather sufficient random data"` |
|        - | 11472 | `			);` |
|        - | 11473 | `	}` |
|      225 | 11474 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 | 11475 | `	return SXRET_OK;` |
|      122 | 11476 |  |
|        - | 11477 | `/*` |
|        - | 11478 | ` * string random_bytes(int $length)` |
|        - | 11479 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - | 11480 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - | 11481 | ` */` |
|       24 | 11482 | `static int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11483 |  |
|        - | 11484 | `	sxi64 iLen;` |
|        - | 11485 | `	unsigned char zStack[256];` |
|        - | 11486 | `	void *pBuf;` |
|        - | 11487 | `	int rc;` |
|       25 | 11488 | `	int bHeap = 0;` |
|       25 | 11489 | `	if( nArg != 1 ){` |
|        7 | 11490 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11491 | `			"ArgumentCountError",` |
|        - | 11492 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|        2 | 11493 | `			nArg` |
|        - | 11494 | `			);` |
|        - | 11495 | `	}` |
|       21 | 11496 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       21 | 11497 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 | 11498 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 | 11499 | `	if( iLen < 1 ){` |
|        5 | 11500 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11501 | `			"ValueError",` |
|        - | 11502 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - | 11503 | `			);` |
|        - | 11504 | `	}` |
|        - | 11505 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - | 11506 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - | 11507 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 | 11508 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 | 11509 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11510 | `			"ValueError",` |
|        - | 11511 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - | 11512 | `			);` |
|        - | 11513 | `	}` |
|       13 | 11514 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 | 11515 | `		pBuf = zStack;` |
|        7 | 11516 | `	}else{` |
|      ! 0 | 11517 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 | 11518 | `		if( pBuf == 0 ){` |
|      ! 0 | 11519 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11520 | `				"Exception",` |
|        - | 11521 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 | 11522 | `				iLen` |
|        - | 11523 | `				);` |
|        - | 11524 | `		}` |
|      ! 0 | 11525 | `		bHeap = 1;` |
|        - | 11526 | `	}` |
|       13 | 11527 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 11528 | `		if( bHeap ){` |
|      ! 0 | 11529 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 11530 | `		}` |
|      ! 0 | 11531 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11532 | `			"Exception",` |
|        - | 11533 | `			"Cannot gather sufficient random data"` |
|        - | 11534 | `			);` |
|        - | 11535 | `	}` |
|       13 | 11536 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 11537 | `	if( bHeap ){` |
|      ! 0 | 11538 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 11539 | `	}` |
|       13 | 11540 | `	return SXRET_OK;` |
|       13 | 11541 |  |
|        - | 11542 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11543 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 11544 | `/* Unique ID private data */` |
|        - | 11545 | `struct unique_id_data` |
|        - | 11546 |  |
|        - | 11547 | `	ph7_context *pCtx; /* Call context */` |
|        - | 11548 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 11549 | `};` |
|        - | 11550 | `/*` |
|        - | 11551 | ` * Binary to hex consumer callback.` |
|        - | 11552 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 11553 | ` * defined below.` |
|        - | 11554 | ` */` |
|      192 | 11555 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 11556 |  |
|      193 | 11557 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 11558 | `	sxu32 nBuflen;` |
|        - | 11559 | `	/* Extract result buffer length */` |
|      193 | 11560 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 11561 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 11562 | `			/*` |
|        - | 11563 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 11564 | `			 * string will be 13 characters long` |
|        - | 11565 | `			 */` |
|       25 | 11566 | `		return SXERR_ABORT;` |
|        - | 11567 | `	}` |
|      169 | 11568 | `	if( nBuflen > 22 ){` |
|      ! 0 | 11569 | `		return SXERR_ABORT;` |
|        - | 11570 | `	}` |
|        - | 11571 | `	/* Safely Consume the hex stream */` |
|      169 | 11572 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 11573 | `	return SXRET_OK;` |
|       97 | 11574 |  |
|        - | 11575 | `/*` |
|        - | 11576 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 11577 | ` *  Generate a unique ID` |
|        - | 11578 | ` * Parameter` |
|        - | 11579 | ` * $prefix` |
|        - | 11580 | ` *  Append this prefix to the generated unique ID.` |
|        - | 11581 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 11582 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 11583 | ` * $more_entropy` |
|        - | 11584 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 11585 | ` *  that the result will be unique.` |
|        - | 11586 | ` * Return` |
|        - | 11587 | ` *  Returns the unique identifier, as a string.` |
|        - | 11588 | ` */` |
|       24 | 11589 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11590 |  |
|        - | 11591 | `	struct unique_id_data sUniq;` |
|        - | 11592 | `	unsigned char zDigest[20];` |
|       25 | 11593 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11594 | `	const char *zPrefix;` |
|        - | 11595 | `	SHA1Context sCtx;` |
|        - | 11596 | `	char zRandom[7];` |
|        - | 11597 | `	int nPrefix;` |
|        - | 11598 | `	int entropy;` |
|        - | 11599 | `	/* Generate a random string first */` |
|       25 | 11600 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 11601 | `	/* Initialize fields */` |
|       25 | 11602 | `	zPrefix = 0;` |
|       25 | 11603 | `	nPrefix = 0;` |
|       25 | 11604 | `	entropy = 0;` |
|       25 | 11605 | `	if( nArg > 0 ){` |
|        - | 11606 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 11607 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 11608 | `		if( nArg > 1 ){` |
|      ! 0 | 11609 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 11610 | `		}` |
|      ! 0 | 11611 | `	}` |
|       25 | 11612 | `	SHA1Init(&sCtx);` |
|        - | 11613 | `	/* Generate the random ID */` |
|       25 | 11614 | `	if( nPrefix > 0 ){` |
|      ! 0 | 11615 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 11616 | `	}` |
|        - | 11617 | `	/* Append the random ID */` |
|       25 | 11618 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 11619 | `	/* Append the random string */` |
|       25 | 11620 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 11621 | `	/* Increment the number */` |
|       25 | 11622 | `	pVm->unique_id++;` |
|       25 | 11623 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 11624 | `	/* Hexify the digest */` |
|       25 | 11625 | `	sUniq.pCtx = pCtx;` |
|       25 | 11626 | `	sUniq.entropy = entropy;` |
|       25 | 11627 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 11628 | `	/* All done */` |
|       25 | 11629 | `	return PH7_OK;` |
|        1 | 11630 |  |
|        - | 11631 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 11632 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 11633 | `/*` |
|        - | 11634 | ` * Section:` |
|        - | 11635 | ` *  Language construct implementation as foreign functions.` |
|        - | 11636 | ` * Status:` |
|        - | 11637 | ` *    Stable.` |
|        - | 11638 | ` */` |
|        - | 11639 | `/*` |
|        - | 11640 | ` * void echo($string...)` |
|        - | 11641 | ` *  Output one or more messages.` |
|        - | 11642 | ` * Parameters` |
|        - | 11643 | ` *  $string` |
|        - | 11644 | ` *   Message to output.` |
|        - | 11645 | ` * Return` |
|        - | 11646 | ` *  NULL.` |
|        - | 11647 | ` */` |
|      ! 0 | 11648 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 11649 |  |
|        - | 11650 | `	const char *zData;` |
|      ! 0 | 11651 | `	int nDataLen = 0;` |
|        - | 11652 | `	ph7_vm *pVm;` |
|        - | 11653 | `	int i,rc;` |
|        - | 11654 | `	/* Point to the target VM */` |
|      ! 0 | 11655 | `	pVm = pCtx->pVm;` |
|        - | 11656 | `	/* Output */` |
|      ! 0 | 11657 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 11658 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 11659 | `		if( nDataLen > 0 ){` |
|      ! 0 | 11660 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 11661 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 11662 | `			if( rc == SXERR_ABORT ){` |
|        - | 11663 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 11664 | `				return PH7_ABORT;` |
|        - | 11665 | `			}` |
|      ! 0 | 11666 | `		}` |
|      ! 0 | 11667 | `	}` |
|      ! 0 | 11668 | `	return SXRET_OK;` |
|      ! 0 | 11669 |  |
|        - | 11670 | `/*` |
|        - | 11671 | ` * int print($string...)` |
|        - | 11672 | ` *  Output one or more messages.` |
|        - | 11673 | ` * Parameters` |
|        - | 11674 | ` *  $string` |
|        - | 11675 | ` *   Message to output.` |
|        - | 11676 | ` * Return` |
|        - | 11677 | ` *  1 always.` |
|        - | 11678 | ` */` |
|        2 | 11679 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11680 |  |
|        - | 11681 | `	const char *zData;` |
|        3 | 11682 | `	int nDataLen = 0;` |
|        - | 11683 | `	ph7_vm *pVm;` |
|        - | 11684 | `	int i,rc;` |
|        - | 11685 | `	/* Point to the target VM */` |
|        3 | 11686 | `	pVm = pCtx->pVm;` |
|        - | 11687 | `	/* Output */` |
|        5 | 11688 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 11689 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 11690 | `		if( nDataLen > 0 ){` |
|        3 | 11691 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 11692 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 11693 | `			if( rc == SXERR_ABORT ){` |
|        - | 11694 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 11695 | `				return PH7_ABORT;` |
|        - | 11696 | `			}` |
|        1 | 11697 | `		}` |
|        2 | 11698 | `	}` |
|        - | 11699 | `	/* Return 1 */` |
|        3 | 11700 | `	ph7_result_int(pCtx,1);` |
|        3 | 11701 | `	return SXRET_OK;` |
|        2 | 11702 |  |
|        - | 11703 | `/*` |
|        - | 11704 | ` * void exit(string $msg)` |
|        - | 11705 | ` * void exit(int $status)` |
|        - | 11706 | ` * void die(string $ms)` |
|        - | 11707 | ` * void die(int $status)` |
|        - | 11708 | ` *   Output a message and terminate program execution.` |
|        - | 11709 | ` * Parameter` |
|        - | 11710 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 11711 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 11712 | ` *  and not printed` |
|        - | 11713 | ` * Return` |
|        - | 11714 | ` *  NULL` |
|        - | 11715 | ` */` |
|      ! 0 | 11716 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 11717 |  |
|      ! 0 | 11718 | `	if( nArg > 0 ){` |
|      ! 0 | 11719 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 11720 | `			const char *zData;` |
|      ! 0 | 11721 | `			int iLen = 0;` |
|        - | 11722 | `			/* Print exit message */` |
|      ! 0 | 11723 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 11724 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 11725 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 11726 | `			sxi32 iExitStatus;` |
|        - | 11727 | `			/* Record exit status code */` |
|      ! 0 | 11728 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 11729 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 11730 | `		}` |
|      ! 0 | 11731 | `	}` |
|        - | 11732 | `	/* Check if we are in an included file */` |
|      ! 0 | 11733 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - | 11734 | `		/* Exit the entire process */` |
|      ! 0 | 11735 | `		exit(pCtx->pVm->iExitStatus);` |
|        - | 11736 | `	}` |
|        - | 11737 | `	/* Abort processing immediately */` |
|      ! 0 | 11738 | `	return PH7_ABORT;` |
|      ! 0 | 11739 |  |
|        - | 11740 | `/*` |
|        - | 11741 | ` * bool isset($var,...)` |
|        - | 11742 | ` *  Finds out whether a variable is set.` |
|        - | 11743 | ` * Parameters` |
|        - | 11744 | ` *  One or more variable to check.` |
|        - | 11745 | ` * Return` |
|        - | 11746 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 11747 | ` */` |
|    89074 | 11748 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11749 |  |
|        - | 11750 | `	ph7_value *pObj;` |
|    89076 | 11751 | `	int res = 0;` |
|        - | 11752 | `	int i;` |
|    89076 | 11753 | `	if( nArg < 1 ){` |
|        - | 11754 | `		/* Missing arguments,return false */` |
|      ! 0 | 11755 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 11756 | `		return SXRET_OK;` |
|        - | 11757 | `	}` |
|        - | 11758 | `	/* Iterate over available arguments */` |
|   116540 | 11759 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    89076 | 11760 | `		pObj = apArg[i];` |
|    89076 | 11761 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    60740 | 11762 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 11763 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 11764 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 11765 | `			}` |
|    30369 | 11766 | `		}` |
|    89076 | 11767 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    89076 | 11768 | `		if( !res ){` |
|        - | 11769 | `			/* Variable not set,return FALSE */` |
|    61612 | 11770 | `			ph7_result_bool(pCtx,0);` |
|    61612 | 11771 | `			return SXRET_OK;` |
|        - | 11772 | `		}` |
|    13734 | 11773 | `	}` |
|        - | 11774 | `	/* All given variable are set,return TRUE */` |
|    27466 | 11775 | `	ph7_result_bool(pCtx,1);` |
|    27466 | 11776 | `	return SXRET_OK;` |
|    44539 | 11777 |  |
|        - | 11778 | `/*` |
|        - | 11779 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 11780 | ` * frame,the reference table and discard it's contents.` |
|        - | 11781 | ` * This function never fail and always return SXRET_OK.` |
|        - | 11782 | ` */` |
|  3119068 | 11783 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 11784 |  |
|        - | 11785 | `	ph7_value *pObj;` |
|        - | 11786 | `	VmRefObj *pRef;` |
|  3119070 | 11787 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3119070 | 11788 | `	if( pObj ){` |
|        - | 11789 | `		/* Release the object */` |
|  3119070 | 11790 | `		PH7_MemObjRelease(pObj);` |
|  1559534 | 11791 | `	}` |
|        - | 11792 | `	/* Remove old reference links */` |
|  3119070 | 11793 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3119070 | 11794 | `	if( pRef ){` |
|  3119064 | 11795 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 11796 | `		/* Unlink from the reference table */` |
|  3119064 | 11797 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3119064 | 11798 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 11799 | `			VmSlot sFree;` |
|        - | 11800 | `			/* Restore to the free list */` |
|  3119056 | 11801 | `			sFree.nIdx = nObjIdx;` |
|  3119056 | 11802 | `			sFree.pUserData = 0;` |
|  3119056 | 11803 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1559527 | 11804 | `		}` |
|  1559531 | 11805 | `	}` |
|  3119070 | 11806 | `	return SXRET_OK;` |
|        2 | 11807 |  |
|        - | 11808 | `/*` |
|        - | 11809 | ` * void unset($var,...)` |
|        - | 11810 | ` *   Unset one or more given variable.` |
|        - | 11811 | ` * Parameters` |
|        - | 11812 | ` *  One or more variable to unset.` |
|        - | 11813 | ` * Return` |
|        - | 11814 | ` *  Nothing.` |
|        - | 11815 | ` */` |
|     7328 | 11816 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11817 |  |
|        - | 11818 | `	ph7_value *pObj;` |
|        - | 11819 | `	ph7_vm *pVm;` |
|        - | 11820 | `	int i;` |
|        - | 11821 | `	/* Point to the target VM */` |
|     7330 | 11822 | `	pVm = pCtx->pVm;` |
|        - | 11823 | `	/* Iterate and unset */` |
|    14658 | 11824 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7330 | 11825 | `		pObj = apArg[i];` |
|     7330 | 11826 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 | 11827 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 11828 | `				/* Throw an error */` |
|      ! 0 | 11829 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 11830 | `			}` |
|      ! 0 | 11831 | `		}else{` |
|     7330 | 11832 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 11833 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     7330 | 11834 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     7324 | 11835 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3661 | 11836 | `			}` |
|        - | 11837 | `		}` |
|     3666 | 11838 | `	}` |
|     7330 | 11839 | `	return SXRET_OK;` |
|        2 | 11840 |  |
|        - | 11841 | `/*` |
|        - | 11842 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 11843 | ` */` |
|      110 | 11844 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11845 |  |
|      111 | 11846 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 | 11847 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 11848 | `	ph7_value *pObj;` |
|        - | 11849 | `	sxu32 nIdx;` |
|        - | 11850 | `	/* Extract the memory object */` |
|      111 | 11851 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 | 11852 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 | 11853 | `	if( pObj ){` |
|      111 | 11854 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 | 11855 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 11856 | `				SyString sName;` |
|        - | 11857 | `				ph7_value sKey;` |
|        - | 11858 | `				/* Perform the insertion */` |
|      109 | 11859 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 | 11860 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 | 11861 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 | 11862 | `				PH7_MemObjRelease(&sKey);` |
|       54 | 11863 | `			}` |
|       54 | 11864 | `		}` |
|       55 | 11865 | `	}` |
|      111 | 11866 | `	return SXRET_OK;` |
|        1 | 11867 |  |
|        - | 11868 | `/*` |
|        - | 11869 | ` * array get_defined_vars(void)` |
|        - | 11870 | ` *  Returns an array of all defined variables.` |
|        - | 11871 | ` * Parameter` |
|        - | 11872 | ` *  None` |
|        - | 11873 | ` * Return` |
|        - | 11874 | ` *  An array with all the variables defined in the current scope.` |
|        - | 11875 | ` */` |
|        2 | 11876 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11877 |  |
|        3 | 11878 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11879 | `	ph7_value *pArray;` |
|        - | 11880 | `	/* Create a new array */` |
|        3 | 11881 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11882 | ` 	if( pArray == 0 ){` |
|      ! 0 | 11883 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11884 | `		SXUNUSED(apArg);` |
|        - | 11885 | `		/* Return NULL */` |
|      ! 0 | 11886 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11887 | `		return SXRET_OK;` |
|        - | 11888 | `	}` |
|        - | 11889 | `	/* Superglobals first */` |
|        3 | 11890 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 11891 | `	/* Then variable defined in the current frame */` |
|        3 | 11892 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 11893 | `	/* Finally,return the created array */` |
|        3 | 11894 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11895 | `	return SXRET_OK;` |
|        2 | 11896 |  |
|        - | 11897 | `/*` |
|        - | 11898 | ` * bool gettype($var)` |
|        - | 11899 | ` *  Get the type of a variable` |
|        - | 11900 | ` * Parameters` |
|        - | 11901 | ` *   $var` |
|        - | 11902 | ` *    The variable being type checked.` |
|        - | 11903 | ` * Return` |
|        - | 11904 | ` *   String representation of the given variable type.` |
|        - | 11905 | ` */` |
|       32 | 11906 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11907 |  |
|       34 | 11908 | `	const char *zType = "Empty";` |
|       34 | 11909 | `	if( nArg > 0 ){` |
|       34 | 11910 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 11911 | `	}` |
|        - | 11912 | `	/* Return the variable type */` |
|       34 | 11913 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 11914 | `	return SXRET_OK;` |
|        2 | 11915 |  |
|        - | 11916 | `/*` |
|        - | 11917 | ` * string get_resource_type(resource $handle)` |
|        - | 11918 | ` *  This function gets the type of the given resource.` |
|        - | 11919 | ` * Parameters` |
|        - | 11920 | ` *  $handle` |
|        - | 11921 | ` *  The evaluated resource handle.` |
|        - | 11922 | ` * Return` |
|        - | 11923 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 11924 | ` *  representing its type. If the type is not identified by this function` |
|        - | 11925 | ` *  the return value will be the string Unknown.` |
|        - | 11926 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 11927 | ` *  is not a resource.` |
|        - | 11928 | ` */` |
|        2 | 11929 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11930 |  |
|        3 | 11931 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 11932 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 11933 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11934 | `		return PH7_OK;` |
|        - | 11935 | `	}` |
|        3 | 11936 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 11937 | `	return SXRET_OK;` |
|        2 | 11938 |  |
|        - | 11939 | `/*` |
|        - | 11940 | ` * void var_dump(expression,....)` |
|        - | 11941 | ` *   var_dump � Dumps information about a variable` |
|        - | 11942 | ` * Parameters` |
|        - | 11943 | ` *   One or more expression to dump.` |
|        - | 11944 | ` * Returns` |
|        - | 11945 | ` *  Nothing.` |
|        - | 11946 | ` */` |
|      218 | 11947 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11948 |  |
|        - | 11949 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 11950 | `	int i;` |
|      220 | 11951 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 11952 | `	/* Dump one or more expressions */` |
|      444 | 11953 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 11954 | `		ph7_value *pObj = apArg[i];` |
|        - | 11955 | `		/* Reset the working buffer */` |
|      226 | 11956 | `		SyBlobReset(&sDump);` |
|        - | 11957 | `		/* Dump the given expression */` |
|      226 | 11958 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 11959 | `		/* Output */` |
|      226 | 11960 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 11961 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 11962 | `		}` |
|      114 | 11963 | `	}` |
|        - | 11964 | `	/* Release the working buffer */` |
|      220 | 11965 | `	SyBlobRelease(&sDump);` |
|      220 | 11966 | `	return SXRET_OK;` |
|        2 | 11967 |  |
|        - | 11968 | `/*` |
|        - | 11969 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 11970 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 11971 | ` * Parameters` |
|        - | 11972 | ` *   expression: Expression to dump` |
|        - | 11973 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 11974 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 11975 | ` *            print_r() will return the information rather than print it.` |
|        - | 11976 | ` * Return` |
|        - | 11977 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 11978 | ` *  Otherwise, the return value is TRUE.` |
|        - | 11979 | ` */` |
|       16 | 11980 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11981 |  |
|       17 | 11982 | `	int ret_string = 0;` |
|        - | 11983 | `	SyBlob sDump;` |
|       17 | 11984 | `	if( nArg < 1 ){` |
|        - | 11985 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 11986 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11987 | `		return SXRET_OK;` |
|        - | 11988 | `	}` |
|       17 | 11989 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 11990 | `	if ( nArg > 1 ){` |
|        - | 11991 | `		/* Where to redirect output */` |
|       11 | 11992 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 11993 | `	}` |
|        - | 11994 | `	/* Generate dump */` |
|       17 | 11995 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 11996 | `	if( !ret_string ){` |
|        - | 11997 | `		/* Output dump */` |
|        7 | 11998 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11999 | `		/* Return true */` |
|        7 | 12000 | `		ph7_result_bool(pCtx,1);` |
|        4 | 12001 | `	}else{` |
|        - | 12002 | `		/* Generated dump as return value */` |
|       11 | 12003 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12004 | `	}` |
|        - | 12005 | `	/* Release the working buffer */` |
|       17 | 12006 | `	SyBlobRelease(&sDump);` |
|       17 | 12007 | `	return SXRET_OK;` |
|        9 | 12008 |  |
|        - | 12009 | `/*` |
|        - | 12010 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 12011 | ` * Same job as print_r. (see coment above)` |
|        - | 12012 | ` */` |
|        2 | 12013 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12014 |  |
|        3 | 12015 | `	int ret_string = 0;` |
|        - | 12016 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 12017 | `	if( nArg < 1 ){` |
|        - | 12018 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 12019 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12020 | `		return SXRET_OK;` |
|        - | 12021 | `	}` |
|        3 | 12022 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 12023 | `	if ( nArg > 1 ){` |
|        - | 12024 | `		/* Where to redirect output */` |
|        3 | 12025 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 12026 | `	}` |
|        - | 12027 | `	/* Generate dump */` |
|        3 | 12028 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 12029 | `	if( !ret_string ){` |
|        - | 12030 | `		/* Output dump */` |
|      ! 0 | 12031 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12032 | `		/* Return NULL */` |
|      ! 0 | 12033 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12034 | `	}else{` |
|        - | 12035 | `		/* Generated dump as return value */` |
|        3 | 12036 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12037 | `	}` |
|        - | 12038 | `	/* Release the working buffer */` |
|        3 | 12039 | `	SyBlobRelease(&sDump);` |
|        3 | 12040 | `	return SXRET_OK;` |
|        2 | 12041 |  |
|        - | 12042 | `/*` |
|        - | 12043 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 12044 | ` *  Set/get the various assert flags.` |
|        - | 12045 | ` * Parameter` |
|        - | 12046 | ` * $what` |
|        - | 12047 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 12048 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 12049 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 12050 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 12051 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 12052 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 12053 | ` * $value` |
|        - | 12054 | ` *   An optional new value for the option.` |
|        - | 12055 | ` * Return` |
|        - | 12056 | ` *  Old setting on success or FALSE on failure.` |
|        - | 12057 | ` */` |
|       28 | 12058 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12059 |  |
|       30 | 12060 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12061 | `	int iOption;` |
|        - | 12062 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 12063 | `	if( nArg < 1 ){` |
|        3 | 12064 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12065 | `			"ArgumentCountError",` |
|        - | 12066 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 12067 | `			);` |
|        - | 12068 | `	}` |
|        - | 12069 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 12070 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 12071 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 12072 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12073 | `			"TypeError",` |
|        - | 12074 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 12075 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 12076 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 12077 | `			);` |
|        - | 12078 | `	}` |
|       28 | 12079 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 12080 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 12081 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 12082 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 12083 | `	switch( iOption ){` |
|        5 | 12084 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 12085 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 12086 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 12087 | `		if( nArg > 1 ){` |
|        5 | 12088 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 12089 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 12090 | `			}else{` |
|        3 | 12091 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 12092 | `			}` |
|        2 | 12093 | `		}` |
|       12 | 12094 | `		break;` |
|        1 | 12095 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 12096 | `		/* Return old callback or null */` |
|        3 | 12097 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 12098 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 12099 | `		}else{` |
|        3 | 12100 | `			ph7_result_null(pCtx);` |
|        - | 12101 | `		}` |
|        3 | 12102 | `		if( nArg > 1 ){` |
|      ! 0 | 12103 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 12104 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 12105 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 12106 | `			}else{` |
|      ! 0 | 12107 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 12108 | `			}` |
|      ! 0 | 12109 | `		}` |
|        3 | 12110 | `		break;` |
|        5 | 12111 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 12112 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 12113 | `		if( nArg > 1 ){` |
|        5 | 12114 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 12115 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 12116 | `			}else{` |
|        3 | 12117 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 12118 | `			}` |
|        2 | 12119 | `		}` |
|       11 | 12120 | `		break;` |
|      ! 0 | 12121 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 12122 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 12123 | `		break;` |
|        1 | 12124 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 12125 | `		ph7_result_int(pCtx, 1);` |
|        3 | 12126 | `		break;` |
|      ! 0 | 12127 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 12128 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 12129 | `		break;` |
|        1 | 12130 | `	default:` |
|        - | 12131 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 12132 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12133 | `			"ValueError",` |
|        - | 12134 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 12135 | `			);` |
|        - | 12136 | `	}` |
|       26 | 12137 | `	return PH7_OK;` |
|       16 | 12138 |  |
|        - | 12139 | `/*` |
|        - | 12140 | ` * bool assert(mixed $assertion)` |
|        - | 12141 | ` *  Checks if assertion is FALSE.` |
|        - | 12142 | ` * Parameter` |
|        - | 12143 | ` *  $assertion` |
|        - | 12144 | ` *    The assertion to test.` |
|        - | 12145 | ` * Return` |
|        - | 12146 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 12147 | ` */` |
|       24 | 12148 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12149 |  |
|       26 | 12150 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12151 | `	int iFlags,iResult;` |
|        - | 12152 | `	const char *zDesc;` |
|        - | 12153 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 12154 | `	if( nArg < 1 ){` |
|        3 | 12155 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12156 | `			"ArgumentCountError",` |
|        - | 12157 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 12158 | `			);` |
|        - | 12159 | `	}` |
|       24 | 12160 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 12161 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 12162 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 12163 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 12164 | `		return PH7_OK;` |
|        - | 12165 | `	}` |
|        - | 12166 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 12167 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 12168 | `	if( !iResult ){` |
|        - | 12169 | `		/* Assertion failed */` |
|        - | 12170 | `		/* Extract optional description */` |
|       13 | 12171 | `		zDesc = 0;` |
|       13 | 12172 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 12173 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 12174 | `		}` |
|       13 | 12175 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 12176 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 12177 | `			ph7_value sFile,sLine;` |
|        - | 12178 | `			ph7_value *apCbArg[3];` |
|        - | 12179 | `			SyString *pFile;` |
|        - | 12180 | `			/* Extract the processed script */` |
|      ! 0 | 12181 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 12182 | `			if( pFile == 0 ){` |
|      ! 0 | 12183 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 12184 | `			}` |
|        - | 12185 | `			/* Invoke the callback */` |
|      ! 0 | 12186 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 12187 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 12188 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 12189 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 12190 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 12191 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 12192 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 12193 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 12194 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 12195 | `		}` |
|       13 | 12196 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 12197 | `			/* Abort VM execution immediately */` |
|      ! 0 | 12198 | `			return PH7_ABORT;` |
|        - | 12199 | `		}` |
|        - | 12200 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 12201 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 12202 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12203 | `				"AssertionError",` |
|        - | 12204 | `				"%s",` |
|        1 | 12205 | `				zDesc` |
|        - | 12206 | `				);` |
|      ! 0 | 12207 | `		}else{` |
|       11 | 12208 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12209 | `				"AssertionError",` |
|        - | 12210 | `				"assert(false)"` |
|        - | 12211 | `				);` |
|        - | 12212 | `		}` |
|        - | 12213 | `	}` |
|        - | 12214 | `	/* Assertion passed */` |
|       11 | 12215 | `	ph7_result_bool(pCtx,1);` |
|       11 | 12216 | `	return PH7_OK;` |
|       14 | 12217 |  |
|        - | 12218 | `/*` |
|        - | 12219 | ` * Section:` |
|        - | 12220 | ` *  Error reporting functions.` |
|        - | 12221 | ` * Status:` |
|        - | 12222 | ` *    Stable.` |
|        - | 12223 | ` */` |
|        - | 12224 | `/*` |
|        - | 12225 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 12226 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 12227 | ` * Parameters` |
|        - | 12228 | ` *  $error_msg` |
|        - | 12229 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 12230 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 12231 | ` * $error_type` |
|        - | 12232 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 12233 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 12234 | ` * Return` |
|        - | 12235 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 12236 | ` */` |
|       12 | 12237 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12238 |  |
|       14 | 12239 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 12240 | `	int rc = PH7_OK;` |
|       14 | 12241 | `	if( nArg > 0 ){` |
|        - | 12242 | `		const char *zErr;` |
|        - | 12243 | `		int nLen;` |
|        - | 12244 | `		/* Extract the error message */` |
|       12 | 12245 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 12246 | `		if( nArg > 1 ){` |
|        - | 12247 | `			/* Extract the error type */` |
|       12 | 12248 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 12249 | `			switch( nErr ){` |
|        1 | 12250 | `			case 1:   /* E_ERROR */` |
|        - | 12251 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 12252 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 12253 | `			case 256: /* E_USER_ERROR */` |
|        3 | 12254 | `				nErr = PH7_CTX_ERR;` |
|        3 | 12255 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 12256 | `				break;` |
|        1 | 12257 | `			case 2:   /* E_WARNING */` |
|        - | 12258 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 12259 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 12260 | `			case 512: /* E_USER_WARNING */` |
|        3 | 12261 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 12262 | `				break;` |
|        3 | 12263 | `			default:` |
|        8 | 12264 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 12265 | `				break;` |
|        - | 12266 | `			}` |
|        5 | 12267 | `		}` |
|        - | 12268 | `		/* Report error */` |
|       12 | 12269 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 12270 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 12271 | `			return rc;` |
|        - | 12272 | `		}` |
|        - | 12273 | `		/* Return true */` |
|       12 | 12274 | `		ph7_result_bool(pCtx,1);` |
|        7 | 12275 | `	}else{` |
|        - | 12276 | `		/* Missing arguments,return FALSE */` |
|        3 | 12277 | `		ph7_result_bool(pCtx,0);` |
|        - | 12278 | `	}` |
|       14 | 12279 | `	return rc;` |
|        8 | 12280 |  |
|        - | 12281 | `/*` |
|        - | 12282 | ` * int error_reporting([int $level])` |
|        - | 12283 | ` *  Sets which PHP errors are reported.` |
|        - | 12284 | ` * Parameters` |
|        - | 12285 | ` *  $level` |
|        - | 12286 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 12287 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 12288 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 12289 | ` *   levels will not always behave as expected.` |
|        - | 12290 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 12291 | ` *   in the predefined constants.` |
|        - | 12292 | ` * Return` |
|        - | 12293 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 12294 | ` *   parameter is given.` |
|        - | 12295 | ` */` |
|       38 | 12296 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12297 |  |
|       40 | 12298 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12299 | `	int nOld;` |
|        - | 12300 | `	/* Extract the old reporting level */` |
|       40 | 12301 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 | 12302 | `	if( nArg > 0 ){` |
|        - | 12303 | `		int nNew;` |
|        - | 12304 | `		/* Extract the desired error reporting level */` |
|       32 | 12305 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 | 12306 | `		if( !nNew ){` |
|        - | 12307 | `			/* Do not report errors at all */` |
|        5 | 12308 | `			pVm->bErrReport = 0;` |
|        3 | 12309 | `		}else{` |
|        - | 12310 | `			/* Report all errors */` |
|       28 | 12311 | `			pVm->bErrReport = 1;` |
|        - | 12312 | `		}` |
|       15 | 12313 | `	}` |
|        - | 12314 | `	/* Return the old level */` |
|       40 | 12315 | `	ph7_result_int(pCtx,nOld);` |
|       40 | 12316 | `	return PH7_OK;` |
|        2 | 12317 |  |
|        - | 12318 | `/*` |
|        - | 12319 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 12320 | ` *  Send an error message somewhere.` |
|        - | 12321 | ` * Parameter` |
|        - | 12322 | ` *  $message` |
|        - | 12323 | ` *   The error message that should be logged.` |
|        - | 12324 | ` *  $message_type` |
|        - | 12325 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 12326 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 12327 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 12328 | ` *       This is the default option.` |
|        - | 12329 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 12330 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 12331 | ` *    2  No longer an option.` |
|        - | 12332 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 12333 | ` *       to the end of the message string.` |
|        - | 12334 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 12335 | ` *  $destination` |
|        - | 12336 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 12337 | ` *  $extra_headers` |
|        - | 12338 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 12339 | ` * Return` |
|        - | 12340 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12341 | ` * NOTE:` |
|        - | 12342 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 12343 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 12344 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 12345 | ` *  Otherwise this function is no-op.` |
|        - | 12346 | ` */` |
|        4 | 12347 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12348 |  |
|        - | 12349 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 12350 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 12351 | `	int iType = 0;` |
|        5 | 12352 | `	if( nArg < 1 ){` |
|        - | 12353 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 12354 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12355 | `		return PH7_OK;` |
|        - | 12356 | `	}` |
|        5 | 12357 | `	if( pVm->xErrLog  ){` |
|        - | 12358 | `		/* Invoke the user callback */` |
|      ! 0 | 12359 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 12360 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 12361 | `		if( nArg > 1 ){` |
|      ! 0 | 12362 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 12363 | `			if( nArg > 2 ){` |
|      ! 0 | 12364 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 12365 | `				if( nArg > 3 ){` |
|      ! 0 | 12366 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 12367 | `				}` |
|      ! 0 | 12368 | `			}` |
|      ! 0 | 12369 | `		}` |
|      ! 0 | 12370 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 12371 | `	}` |
|        - | 12372 | `	/* Retun TRUE */` |
|        5 | 12373 | `	ph7_result_bool(pCtx,1);` |
|        5 | 12374 | `	return PH7_OK;` |
|        3 | 12375 |  |
|        - | 12376 | `/*` |
|        - | 12377 | ` * bool restore_exception_handler(void)` |
|        - | 12378 | ` *  Restores the previously defined exception handler function.` |
|        - | 12379 | ` * Parameter` |
|        - | 12380 | ` *  None` |
|        - | 12381 | ` * Return` |
|        - | 12382 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 12383 | ` */` |
|        4 | 12384 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12385 |  |
|        5 | 12386 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12387 | `	ph7_value *pOld,*pNew;` |
|        - | 12388 | `	/* Point to the old and the new handler */` |
|        5 | 12389 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 12390 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 12391 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 12392 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 12393 | `		SXUNUSED(apArg);` |
|        - | 12394 | `		/* No installed handler,return FALSE */` |
|        5 | 12395 | `		ph7_result_bool(pCtx,0);` |
|        5 | 12396 | `		return PH7_OK;` |
|        - | 12397 | `	}` |
|        - | 12398 | `	/* Copy the old handler */` |
|      ! 0 | 12399 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 12400 | `	PH7_MemObjRelease(pOld);` |
|        - | 12401 | `	/* Return TRUE */` |
|      ! 0 | 12402 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 12403 | `	return PH7_OK;` |
|        3 | 12404 |  |
|        - | 12405 | `/*` |
|        - | 12406 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 12407 | ` *  Sets a user-defined exception handler function.` |
|        - | 12408 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 12409 | ` * NOTE` |
|        - | 12410 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 12411 | ` *  the satndard PHP engine.` |
|        - | 12412 | ` * Parameters` |
|        - | 12413 | ` *  $exception_handler` |
|        - | 12414 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 12415 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 12416 | ` *   that was thrown.` |
|        - | 12417 | ` *  Note:` |
|        - | 12418 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 12419 | ` * Return` |
|        - | 12420 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 12421 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 12422 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 12423 | ` */` |
|        4 | 12424 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12425 |  |
|        6 | 12426 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12427 | `	ph7_value *pOld,*pNew;` |
|        - | 12428 | `	/* Point to the old and the new handler */` |
|        6 | 12429 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 12430 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 12431 | `	/* Return the old handler */` |
|        6 | 12432 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 12433 | `	if( nArg > 0 ){` |
|        6 | 12434 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 12435 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 12436 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 12437 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 12438 | `		}else{` |
|        6 | 12439 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 12440 | `			/* Install the new handler */` |
|        6 | 12441 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 12442 | `		}` |
|        2 | 12443 | `	}` |
|        6 | 12444 | `	return PH7_OK;` |
|        2 | 12445 |  |
|        - | 12446 | `/*` |
|        - | 12447 | ` * bool restore_error_handler(void)` |
|        - | 12448 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 12449 | ` * Parameters:` |
|        - | 12450 | ` *  None.` |
|        - | 12451 | ` * Return` |
|        - | 12452 | ` *  Always TRUE.` |
|        - | 12453 | ` */` |
|        6 | 12454 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12455 |  |
|        7 | 12456 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12457 | `	ph7_value *pOld,*pNew;` |
|        - | 12458 | `	/* Point to the old and the new handler */` |
|        7 | 12459 | `	pOld = &pVm->aErrCB[0];` |
|        7 | 12460 | `	pNew = &pVm->aErrCB[1];` |
|        7 | 12461 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 12462 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 12463 | `		SXUNUSED(apArg);` |
|        - | 12464 | `		/* No installed callback,return FALSE */` |
|        7 | 12465 | `		ph7_result_bool(pCtx,0);` |
|        7 | 12466 | `		return PH7_OK;` |
|        - | 12467 | `	}` |
|        - | 12468 | `	/* Copy the old callback */` |
|      ! 0 | 12469 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 12470 | `	PH7_MemObjRelease(pOld);` |
|        - | 12471 | `	/* Return TRUE */` |
|      ! 0 | 12472 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 12473 | `	return PH7_OK;` |
|        4 | 12474 |  |
|        - | 12475 | `/*` |
|        - | 12476 | ` * value set_error_handler(callable $error_handler)` |
|        - | 12477 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 12478 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 12479 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 12480 | ` *  Sets a user-defined error handler function.` |
|        - | 12481 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 12482 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 12483 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 12484 | ` *  conditions (using trigger_error()).` |
|        - | 12485 | ` * Parameters` |
|        - | 12486 | ` *  $error_handler` |
|        - | 12487 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 12488 | ` *   describing the error.` |
|        - | 12489 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 12490 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 12491 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 12492 | ` *   The function can be shown as:` |
|        - | 12493 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 12494 | ` *     errno` |
|        - | 12495 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 12496 | ` *   errstr` |
|        - | 12497 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 12498 | ` *   errfile` |
|        - | 12499 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 12500 | ` *     was raised in, as a string.` |
|        - | 12501 | ` *  Note:` |
|        - | 12502 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 12503 | ` * Return` |
|        - | 12504 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 12505 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 12506 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 12507 | ` */` |
|    10526 | 12508 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12509 |  |
|    10528 | 12510 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12511 | `	ph7_value *pOld,*pNew;` |
|        - | 12512 | `	/* Point to the old and the new handler */` |
|    10528 | 12513 | `	pOld = &pVm->aErrCB[0];` |
|    10528 | 12514 | `	pNew = &pVm->aErrCB[1];` |
|        - | 12515 | `	/* Return the old handler */` |
|    10528 | 12516 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    10528 | 12517 | `	if( nArg > 0 ){` |
|    10528 | 12518 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 12519 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5259 | 12520 | `			PH7_MemObjRelease(pNew);` |
|     5259 | 12521 | `			ph7_result_bool(pCtx,1);` |
|     2630 | 12522 | `		}else{` |
|     5270 | 12523 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 12524 | `			/* Install the new handler */` |
|     5270 | 12525 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 12526 | `		}` |
|     5263 | 12527 | `	}` |
|    10528 | 12528 | `	return PH7_OK;` |
|        2 | 12529 |  |
|        - | 12530 | `/*` |
|        - | 12531 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 12532 | ` *  Generates a backtrace.` |
|        - | 12533 | ` * Paramaeter` |
|        - | 12534 | ` *  $options` |
|        - | 12535 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 12536 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 12537 | ` *   all the function/method arguments, to save memory.` |
|        - | 12538 | ` * $limit` |
|        - | 12539 | ` *   (Not Used)` |
|        - | 12540 | ` * Return` |
|        - | 12541 | ` *  An array.The possible returned elements are as follows:` |
|        - | 12542 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 12543 | ` *          Name        Type      Description` |
|        - | 12544 | ` *          ------      ------     -----------` |
|        - | 12545 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 12546 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 12547 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 12548 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 12549 | ` *          object      object    The current object.` |
|        - | 12550 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 12551 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 12552 | ` */` |
|      832 | 12553 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12554 |  |
|      834 | 12555 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12556 | `	ph7_value *pArray;` |
|        - | 12557 | `	ph7_class *pClass;` |
|        - | 12558 | `	ph7_value *pValue;` |
|        - | 12559 | `	SyString *pFile;` |
|        - | 12560 | `	/* Create a new array */` |
|      834 | 12561 | `	pArray = ph7_context_new_array(pCtx);` |
|      834 | 12562 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      834 | 12563 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 12564 | `		/* Out of memory,return NULL */` |
|      ! 0 | 12565 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 12566 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12567 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12568 | `		SXUNUSED(apArg);` |
|      ! 0 | 12569 | `		return PH7_OK;` |
|        - | 12570 | `	}` |
|        - | 12571 | `	/* Dump running function name and it's arguments  */` |
|      834 | 12572 | `	if( pVm->pFrame->pParent ){` |
|      834 | 12573 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 12574 | `		ph7_vm_func *pFunc;` |
|        - | 12575 | `		ph7_value *pArg;` |
|      834 | 12576 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      834 | 12577 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      834 | 12578 | `		if( pFrame->pParent && pFunc ){` |
|      834 | 12579 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      834 | 12580 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      834 | 12581 | `			ph7_value_reset_string_cursor(pValue);` |
|      416 | 12582 | `		}` |
|        - | 12583 | `		/* Function arguments */` |
|      834 | 12584 | `		pArg = ph7_context_new_array(pCtx);` |
|      834 | 12585 | `		if( pArg  ){` |
|        - | 12586 | `			ph7_value *pObj;` |
|        - | 12587 | `			VmSlot *aSlot;` |
|        - | 12588 | `			sxu32 n;` |
|        - | 12589 | `			/* Start filling the array with the given arguments */` |
|      834 | 12590 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     3334 | 12591 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2502 | 12592 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2502 | 12593 | `				if( pObj ){` |
|     2502 | 12594 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1250 | 12595 | `				}` |
|     1252 | 12596 | `			}` |
|        - | 12597 | `			/* Save the array */` |
|      834 | 12598 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      416 | 12599 | `		}` |
|      416 | 12600 | `	}` |
|      834 | 12601 | `	ph7_value_int(pValue,1);` |
|        - | 12602 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 12603 | `	 * line numbers at run-time. )` |
|        - | 12604 | `	 */` |
|      834 | 12605 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 12606 | `	/* Current processed script */` |
|      834 | 12607 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      834 | 12608 | `	if( pFile ){` |
|      834 | 12609 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      834 | 12610 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      834 | 12611 | `		ph7_value_reset_string_cursor(pValue);` |
|      416 | 12612 | `	}` |
|        - | 12613 | `	/* Top class */` |
|      834 | 12614 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      834 | 12615 | `	if( pClass ){` |
|      830 | 12616 | `		ph7_value_reset_string_cursor(pValue);` |
|      830 | 12617 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      830 | 12618 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      414 | 12619 | `	}` |
|        - | 12620 | `	/* Return the freshly created array */` |
|      834 | 12621 | `	ph7_result_value(pCtx,pArray);` |
|        - | 12622 | `	/*` |
|        - | 12623 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 12624 | `	 * as soon we return from this function.` |
|        - | 12625 | `	 */` |
|      834 | 12626 | `	return PH7_OK;` |
|      418 | 12627 |  |
|        - | 12628 | `/*` |
|        - | 12629 | ` * Generate a small backtrace.` |
|        - | 12630 | ` * Store the generated dump in the given BLOB` |
|        - | 12631 | ` */` |
|        4 | 12632 | `static int VmMiniBacktrace(` |
|        - | 12633 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 12634 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 12635 | `	)` |
|        1 | 12636 |  |
|        5 | 12637 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12638 | `	ph7_vm_func *pFunc;` |
|        - | 12639 | `	ph7_class *pClass;` |
|        - | 12640 | `	SyString *pFile;` |
|        - | 12641 | `	/* Called function */` |
|        5 | 12642 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 12643 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 12644 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 12645 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 12646 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 12647 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 12648 | `	}else{` |
|      ! 0 | 12649 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 12650 | `	}` |
|        5 | 12651 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 12652 | `	/* Current processed script */` |
|        5 | 12653 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 12654 | `	if( pFile ){` |
|        5 | 12655 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 12656 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 12657 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 12658 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 12659 | `	}` |
|        - | 12660 | `	/* Top class */` |
|        5 | 12661 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 12662 | `	if( pClass ){` |
|      ! 0 | 12663 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 12664 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 12665 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 12666 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 12667 | `	}` |
|        5 | 12668 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 12669 | `	/* All done */` |
|        5 | 12670 | `	return SXRET_OK;` |
|        1 | 12671 |  |
|        - | 12672 | `/*` |
|        - | 12673 | ` * void debug_print_backtrace()` |
|        - | 12674 | ` *  Prints a backtrace` |
|        - | 12675 | ` * Parameters` |
|        - | 12676 | ` * None` |
|        - | 12677 | ` * Return` |
|        - | 12678 | ` * NULL` |
|        - | 12679 | ` */` |
|        2 | 12680 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12681 |  |
|        3 | 12682 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12683 | `	SyBlob sDump;` |
|        3 | 12684 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 12685 | `	/* Generate the backtrace */` |
|        3 | 12686 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 12687 | `	/* Output backtrace */` |
|        3 | 12688 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12689 | `	/* All done,cleanup */` |
|        3 | 12690 | `	SyBlobRelease(&sDump);` |
|        1 | 12691 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 12692 | `	SXUNUSED(apArg);` |
|        3 | 12693 | `	return PH7_OK;` |
|        1 | 12694 |  |
|        - | 12695 | `/*` |
|        - | 12696 | ` * string debug_string_backtrace()` |
|        - | 12697 | ` *  Generate a backtrace` |
|        - | 12698 | ` * Parameters` |
|        - | 12699 | ` * None` |
|        - | 12700 | ` * Return` |
|        - | 12701 | ` *  A mini backtrace().` |
|        - | 12702 | ` * Note that this is a symisc extension.` |
|        - | 12703 | ` */` |
|        2 | 12704 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12705 |  |
|        3 | 12706 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12707 | `	SyBlob sDump;` |
|        3 | 12708 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 12709 | `	/* Generate the backtrace */` |
|        3 | 12710 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 12711 | `	/* Return the backtrace */` |
|        3 | 12712 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 12713 | `	/* All done,cleanup */` |
|        3 | 12714 | `	SyBlobRelease(&sDump);` |
|        1 | 12715 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 12716 | `	SXUNUSED(apArg);` |
|        3 | 12717 | `	return PH7_OK;` |
|        1 | 12718 |  |
|        - | 12719 | `/*` |
|        - | 12720 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 12721 | ` * exception is triggered.` |
|        - | 12722 | ` */` |
|      510 | 12723 | `static sxi32 VmUncaughtException(` |
|        - | 12724 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 12725 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 12726 | `	)` |
|        1 | 12727 |  |
|        - | 12728 | `	ph7_value *apArg[2],sArg;` |
|      511 | 12729 | `	int nArg = 1;` |
|        - | 12730 | `	sxi32 rc;` |
|      511 | 12731 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 12732 | `		/* Nesting limit reached */` |
|      ! 0 | 12733 | `		return SXRET_OK;` |
|        - | 12734 | `	}` |
|        - | 12735 | `	/* Call any exception handler if available */` |
|      511 | 12736 | `	PH7_MemObjInit(pVm,&sArg);` |
|      511 | 12737 | `	if( pThis ){` |
|        - | 12738 | `		/* Load the exception instance */` |
|      511 | 12739 | `		sArg.x.pOther = pThis;` |
|      511 | 12740 | `		pThis->iRef++;` |
|      511 | 12741 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      256 | 12742 | `	}else{` |
|      ! 0 | 12743 | `		nArg = 0;` |
|        - | 12744 | `	}` |
|      511 | 12745 | `	apArg[0] = &sArg;` |
|        - | 12746 | `	/* Call the exception handler if available */` |
|      511 | 12747 | `	pVm->nExceptDepth++;` |
|      511 | 12748 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      511 | 12749 | `	pVm->nExceptDepth--;` |
|      511 | 12750 | `	if( rc != SXRET_OK ){` |
|        - | 12751 | `		SyBlob sMsgBuf;` |
|      509 | 12752 | `		const char *zClass = "Exception";` |
|      509 | 12753 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 12754 | `		const char *zMsg;` |
|        - | 12755 | `		sxu32 nMsg;` |
|        - | 12756 | `		const char *zFuncName;` |
|        - | 12757 | `		int nFuncLen;` |
|      509 | 12758 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      509 | 12759 | `		if( pThis ){` |
|        - | 12760 | `			ph7_class_method *pGetMessage;` |
|        - | 12761 | `			ph7_value sMsg;` |
|        - | 12762 | `			const char *zTmp;` |
|        - | 12763 | `			int nTmp;` |
|      509 | 12764 | `			zClass = pThis->pClass->sName.zString;` |
|      509 | 12765 | `			nClass = pThis->pClass->sName.nByte;` |
|      509 | 12766 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      509 | 12767 | `			if( pGetMessage ){` |
|      509 | 12768 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      509 | 12769 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      509 | 12770 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      509 | 12771 | `					if( zTmp && nTmp > 0 ){` |
|      509 | 12772 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      254 | 12773 | `					}` |
|      254 | 12774 | `				}` |
|      509 | 12775 | `				PH7_MemObjRelease(&sMsg);` |
|      254 | 12776 | `			}` |
|      254 | 12777 | `		}` |
|      509 | 12778 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      509 | 12779 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      509 | 12780 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      509 | 12781 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      509 | 12782 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 12783 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      509 | 12784 | `		rc = SXERR_ABORT;` |
|      254 | 12785 | `	}` |
|      511 | 12786 | `	PH7_MemObjRelease(&sArg);` |
|      511 | 12787 | `	return rc;` |
|      256 | 12788 |  |
|        - | 12789 | `/*` |
|        - | 12790 | ` * Throw a user exception.` |
|        - | 12791 | ` *` |
|        - | 12792 | ` * Exception dispatch follows this sequence:` |
|        - | 12793 | ` *` |
|        - | 12794 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 12795 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 12796 | ` *` |
|        - | 12797 | ` * 2. If NO catch matches:` |
|        - | 12798 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 12799 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 12800 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 12801 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 12802 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 12803 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 12804 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 12805 | ` *` |
|        - | 12806 | ` * 3. If a catch DOES match:` |
|        - | 12807 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 12808 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 12809 | ` *       inside the catch body from immediately propagating past our` |
|        - | 12810 | ` *       finally block.` |
|        - | 12811 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 12812 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 12813 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 12814 | ` *       in pPendingException (step 2c).` |
|        - | 12815 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 12816 | ` *    d. Run finally (if present).` |
|        - | 12817 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 12818 | ` *       that handlers are restored and finally has run.` |
|        - | 12819 | ` */` |
|      788 | 12820 | `static sxi32 VmThrowException(` |
|        - | 12821 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 12822 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 12823 | `	)` |
|        2 | 12824 |  |
|        - | 12825 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 12826 | `	ph7_exception **apException;` |
|        - | 12827 | `	ph7_exception *pException;` |
|        - | 12828 | `	/* Point to the stack of loaded exceptions */` |
|      790 | 12829 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      790 | 12830 | `	pException = 0;` |
|      790 | 12831 | `	pCatch = 0;` |
|      790 | 12832 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 12833 | `		ph7_exception_block *aCatch;` |
|        - | 12834 | `		ph7_class *pClass;` |
|        - | 12835 | `		SyString *aNames;` |
|        - | 12836 | `		sxu32 nNames;` |
|        - | 12837 | `		int matched;` |
|        - | 12838 | `		sxu32 j,k;` |
|        - | 12839 | `		/* Locate the appropriate block to execute */` |
|      272 | 12840 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      272 | 12841 | `		(void)SySetPop(&pVm->aException);` |
|      272 | 12842 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      280 | 12843 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 12844 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      278 | 12845 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      278 | 12846 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      278 | 12847 | `			matched = 0;` |
|      304 | 12848 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 12849 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 12850 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 12851 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      296 | 12852 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      296 | 12853 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 12854 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 12855 | `					continue;` |
|        - | 12856 | `				}` |
|      296 | 12857 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      270 | 12858 | `					matched = 1;` |
|      270 | 12859 | `					break;` |
|        - | 12860 | `				}` |
|       14 | 12861 | `			}` |
|      278 | 12862 | `			if( matched ){` |
|        - | 12863 | `				/* Catch block found,break immediately */` |
|      270 | 12864 | `				pCatch = &aCatch[j];` |
|      270 | 12865 | `				break;` |
|        - | 12866 | `			}` |
|        5 | 12867 | `		}` |
|      135 | 12868 | `	}` |
|        - | 12869 | `	/* Execute the cached block if available */` |
|      790 | 12870 | `	if( pCatch == 0 ){` |
|        - | 12871 | `		sxi32 rc;` |
|        - | 12872 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      522 | 12873 | `		if( pException && pException->iHasFinally ){` |
|        3 | 12874 | `			pException->iFinallyDone = 1;` |
|        3 | 12875 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 12876 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12877 | `				return SXERR_ABORT;` |
|        - | 12878 | `			}` |
|        1 | 12879 | `		}` |
|        - | 12880 | `		/* Check if there is an outer exception handler on the stack */` |
|      522 | 12881 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 12882 | `			/* Re-throw to the outer handler */` |
|        3 | 12883 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 12884 | `		}` |
|        - | 12885 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 12886 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 12887 | `		 * exception instead of reporting it uncaught.` |
|        - | 12888 | `		 */` |
|      520 | 12889 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 12890 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 12891 | `			 * by looking for a catch frame on the stack.` |
|        - | 12892 | `			 */` |
|      520 | 12893 | `			VmFrame *pF = pVm->pFrame;` |
|      520 | 12894 | `			int inCatch = 0;` |
|     1046 | 12895 | `			while( pF ){` |
|      536 | 12896 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        9 | 12897 | `					inCatch = 1;` |
|        9 | 12898 | `					break;` |
|        - | 12899 | `				}` |
|      527 | 12900 | `				pF = pF->pParent;` |
|        1 | 12901 | `			}` |
|      520 | 12902 | `			if( inCatch ){` |
|        - | 12903 | `				/* Defer — will be re-thrown after finally runs */` |
|        9 | 12904 | `				pThis->iRef++;` |
|        9 | 12905 | `				pVm->pPendingException = pThis;` |
|        9 | 12906 | `				return SXRET_OK;` |
|        - | 12907 | `			}` |
|      255 | 12908 | `		}` |
|        - | 12909 | `		/* Truly uncaught */` |
|      511 | 12910 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      511 | 12911 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 12912 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 12913 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 12914 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 12915 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 12916 | `			}` |
|      ! 0 | 12917 | `		}` |
|      511 | 12918 | `		return rc;` |
|      ! 0 | 12919 | `	}else{` |
|      270 | 12920 | `		VmFrame *pFrame = pVm->pFrame;` |
|      270 | 12921 | `		ph7_exception **apSaved = 0;` |
|        - | 12922 | `		sxu32 nSavedCount;` |
|        - | 12923 | `		sxi32 rc;` |
|      270 | 12924 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      270 | 12925 | `		if( pException->pFrame == pFrame ){` |
|      220 | 12926 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      109 | 12927 | `		}` |
|        - | 12928 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 12929 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 12930 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 12931 | `		 */` |
|      270 | 12932 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      270 | 12933 | `		if( nSavedCount > 0 ){` |
|       16 | 12934 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        5 | 12935 | `				nSavedCount * sizeof(ph7_exception *));` |
|       11 | 12936 | `			if( apSaved ){` |
|       16 | 12937 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        5 | 12938 | `					nSavedCount * sizeof(ph7_exception *));` |
|       11 | 12939 | `				SySetReset(&pVm->aException);` |
|        5 | 12940 | `			}` |
|        5 | 12941 | `		}` |
|        - | 12942 | `		/* Create a private frame first */` |
|      270 | 12943 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      270 | 12944 | `		if( rc == SXRET_OK ){` |
|      270 | 12945 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      270 | 12946 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      270 | 12947 | `			if( pObj ){` |
|      270 | 12948 | `				pThis->iRef++;` |
|      270 | 12949 | `				pObj->x.pOther = pThis;` |
|      270 | 12950 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      134 | 12951 | `			}` |
|        - | 12952 | `			/* Execute the catch block */` |
|      270 | 12953 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 12954 | `			/* Leave the frame */` |
|      270 | 12955 | `			VmLeaveFrame(&(*pVm));` |
|      134 | 12956 | `		}` |
|        - | 12957 | `		/* Restore the outer exception handlers */` |
|      270 | 12958 | `		if( apSaved ){` |
|        - | 12959 | `			sxu32 k;` |
|        - | 12960 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 12961 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 12962 | `			 * Restore the original outer entries.` |
|        - | 12963 | `			 */` |
|       11 | 12964 | `			SySetReset(&pVm->aException);` |
|       21 | 12965 | `			for(k = 0; k < nSavedCount; k++){` |
|       11 | 12966 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        6 | 12967 | `			}` |
|       11 | 12968 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        5 | 12969 | `		}` |
|        - | 12970 | `		/* Execute the finally block after catch */` |
|      270 | 12971 | `		if( pException->iHasFinally ){` |
|       16 | 12972 | `			pException->iFinallyDone = 1;` |
|        - | 12973 | `			{` |
|       16 | 12974 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 12975 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 12976 | `					return SXERR_ABORT;` |
|        - | 12977 | `				}` |
|        - | 12978 | `			}` |
|        7 | 12979 | `		}` |
|      270 | 12980 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12981 | `			return SXERR_ABORT;` |
|        - | 12982 | `		}` |
|        - | 12983 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 12984 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 12985 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 12986 | `		 */` |
|      270 | 12987 | `		if( pVm->pPendingException ){` |
|        9 | 12988 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 12989 | `			pVm->pPendingException = 0;` |
|        9 | 12990 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 12991 | `		}` |
|        - | 12992 | `	}` |
|        - | 12993 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 12994 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 12995 | `	 */` |
|      262 | 12996 | `	return SXRET_OK;` |
|      396 | 12997 |  |
|        - | 12998 | `/*` |
|        - | 12999 | ` * Section:` |
|        - | 13000 | ` *  Version,Credits and Copyright related functions.` |
|        - | 13001 | ` * Status:` |
|        - | 13002 | ` *    Stable.` |
|        - | 13003 | ` */` |
|        - | 13004 | `/*` |
|        - | 13005 | ` * string ph7version(void)` |
|        - | 13006 | ` *  Returns the running version of the PH7 version.` |
|        - | 13007 | ` * Parameters` |
|        - | 13008 | ` *  None` |
|        - | 13009 | ` * Return` |
|        - | 13010 | ` * Current PH7 version.` |
|        - | 13011 | ` */` |
|        2 | 13012 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13013 |  |
|        1 | 13014 | `	SXUNUSED(nArg);` |
|        1 | 13015 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 13016 | `	/* Current engine version */` |
|        3 | 13017 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 13018 | `	return PH7_OK;` |
|        1 | 13019 |  |
|        - | 13020 | `/*` |
|        - | 13021 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 13022 | ` */` |
|        - | 13023 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 13024 | ` "<html><head>"\` |
|        - | 13025 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 13026 | ` "<style type=\"text/css\">"\` |
|        - | 13027 | ` "div {"\` |
|        - | 13028 | `     "border: 1px solid #cccccc;"\` |
|        - | 13029 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 13030 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 13031 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 13032 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 13033 | `     "-webkit-border-radius: 10px;"\` |
|        - | 13034 | `     "-o-border-radius: 10px;"\` |
|        - | 13035 | `     "border-radius: 10px;"\` |
|        - | 13036 | `     "padding-left: 2em;"\` |
|        - | 13037 | `     "background-color: white;"\` |
|        - | 13038 | `     "margin-left: auto;"\` |
|        - | 13039 | `     "font-family: verdana;"\` |
|        - | 13040 | `     "padding-right: 2em;"\` |
|        - | 13041 | `     "margin-right: auto;"\` |
|        - | 13042 | `     "}"\` |
|        - | 13043 | `     "body {"\` |
|        - | 13044 | `     "padding: 0.2em;"\` |
|        - | 13045 | `     "font-style: normal;"\` |
|        - | 13046 | `     "font-size: medium;"\` |
|        - | 13047 | `     "background-color: #f2f2f2;"\` |
|        - | 13048 | `     "}"\` |
|        - | 13049 | `     "hr {"\` |
|        - | 13050 | `     "border-style: solid none none;"\` |
|        - | 13051 | `     "border-width: 1px medium medium;"\` |
|        - | 13052 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 13053 | `     "height: 1px;"\` |
|        - | 13054 | `     "}"\` |
|        - | 13055 | `     "a {"\` |
|        - | 13056 | `     "color: #3366cc;"\` |
|        - | 13057 | `     "text-decoration: none;"\` |
|        - | 13058 | `     "}"\` |
|        - | 13059 | `     "a:hover {"\` |
|        - | 13060 | `     "color: #999999;"\` |
|        - | 13061 | `     "}"\` |
|        - | 13062 | `     "a:active {"\` |
|        - | 13063 | `     "color: #663399;"\` |
|        - | 13064 | `     "}"\` |
|        - | 13065 | `     "h1 {"\` |
|        - | 13066 | `     "margin: 0;"\` |
|        - | 13067 | `     "padding: 0;"\` |
|        - | 13068 | `     "font-family: Verdana;"\` |
|        - | 13069 | `     "font-weight: bold;"\` |
|        - | 13070 | `     "font-style: normal;"\` |
|        - | 13071 | `     "font-size: medium;"\` |
|        - | 13072 | `     "text-transform: capitalize;"\` |
|        - | 13073 | `     "color: #0a328c;"\` |
|        - | 13074 | `     "}"\` |
|        - | 13075 | `     "p {"\` |
|        - | 13076 | `     "margin: 0 auto;"\` |
|        - | 13077 | `     "font-size: medium;"\` |
|        - | 13078 | `     "font-style: normal;"\` |
|        - | 13079 | `     "font-family: verdana;"\` |
|        - | 13080 | `     "}"\` |
|        - | 13081 | `"</style></head><body>"\` |
|        - | 13082 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 13083 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 13084 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 13085 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 13086 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 13087 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 13088 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 13089 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 13090 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 13091 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 13092 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 13093 |  |
|        - | 13094 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13095 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 13096 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 13097 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 13098 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13099 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 13100 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 13101 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 13102 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 13103 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 13104 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13105 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 13106 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 13107 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 13108 |  |
|        - | 13109 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 13110 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 13111 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 13112 | `"&nbsp;*<br>"\` |
|        - | 13113 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 13114 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 13115 | `"&nbsp;* are met:<br>"\` |
|        - | 13116 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 13117 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 13118 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 13119 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 13120 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 13121 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 13122 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 13123 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 13124 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 13125 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 13126 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 13127 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 13128 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 13129 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 13130 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 13131 | `"&nbsp;*<br>"\` |
|        - | 13132 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 13133 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 13134 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 13135 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 13136 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 13137 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 13138 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 13139 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 13140 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 13141 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 13142 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 13143 | `"&nbsp;*/<br>"\` |
|        - | 13144 | `"</span></small></small></p>"\` |
|        - | 13145 | `"</div></body></html>"` |
|        - | 13146 | `/*` |
|        - | 13147 | ` * bool ph7credits(void)` |
|        - | 13148 | ` * bool ph7info(void)` |
|        - | 13149 | ` * bool ph7copyright(void)` |
|        - | 13150 | ` *  Prints out the credits for PH7 engine` |
|        - | 13151 | ` * Parameters` |
|        - | 13152 | ` *  None` |
|        - | 13153 | ` * Return` |
|        - | 13154 | ` *  Always TRUE` |
|        - | 13155 | ` */` |
|        2 | 13156 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13157 |  |
|        3 | 13158 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 13159 | `	/* Expand the HTML page above*/` |
|        3 | 13160 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 13161 | `	ph7_context_output_format(` |
|        1 | 13162 | `		pCtx,` |
|        - | 13163 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 13164 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 13165 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 13166 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 13167 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 13168 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 13169 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 13170 | `#ifdef __WINNT__` |
|        - | 13171 | `		"Windows NT"` |
|        - | 13172 | `#elif defined(__UNIXES__)` |
|        - | 13173 | `		"UNIX-Like"` |
|        - | 13174 | `#else` |
|        - | 13175 | `		"Other OS"` |
|        - | 13176 | `#endif` |
|        - | 13177 | `		);` |
|        3 | 13178 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 13179 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13180 | `	SXUNUSED(apArg);` |
|        - | 13181 | `	/* Return TRUE */` |
|        - | 13182 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 13183 | `	return PH7_OK;` |
|        1 | 13184 |  |
|        - | 13185 | `/*` |
|        - | 13186 | ` * Section:` |
|        - | 13187 | ` *    URL related routines.` |
|        - | 13188 | ` * Status:` |
|        - | 13189 | ` *    Stable.` |
|        - | 13190 | ` */` |
|        - | 13191 | `/*` |
|        - | 13192 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 13193 | ` *  Parse a URL and return its fields.` |
|        - | 13194 | ` * Parameters` |
|        - | 13195 | ` *  $url` |
|        - | 13196 | ` *   The URL to parse.` |
|        - | 13197 | ` * $component` |
|        - | 13198 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 13199 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 13200 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 13201 | ` *  in which case the return value will be an integer).` |
|        - | 13202 | ` * Return` |
|        - | 13203 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 13204 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 13205 | ` *  this array are:` |
|        - | 13206 | ` *   scheme - e.g. http` |
|        - | 13207 | ` *   host` |
|        - | 13208 | ` *   port` |
|        - | 13209 | ` *   user` |
|        - | 13210 | ` *   pass` |
|        - | 13211 | ` *   path` |
|        - | 13212 | ` *   query - after the question mark ?` |
|        - | 13213 | ` *   fragment - after the hashmark #` |
|        - | 13214 | ` * Note:` |
|        - | 13215 | ` *  FALSE is returned on failure.` |
|        - | 13216 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 13217 | ` *  with the standard PHP engine.` |
|        - | 13218 | ` */` |
|       28 | 13219 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13220 |  |
|        - | 13221 | `	const char *zStr; /* Input string */` |
|        - | 13222 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 13223 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 13224 | `	int nLen;` |
|        - | 13225 | `	sxi32 rc;` |
|       29 | 13226 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 13227 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 13228 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13229 | `		return PH7_OK;` |
|        - | 13230 | `	}` |
|        - | 13231 | `	/* Extract the given URI */` |
|       29 | 13232 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 13233 | `	if( nLen < 1 ){` |
|        - | 13234 | `		/* Nothing to process,return FALSE */` |
|        3 | 13235 | `		ph7_result_bool(pCtx,0);` |
|        3 | 13236 | `		return PH7_OK;` |
|        - | 13237 | `	}` |
|        - | 13238 | `	/* Get a parse */` |
|       27 | 13239 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 13240 | `	if( rc != SXRET_OK ){` |
|        - | 13241 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 13242 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13243 | `		return PH7_OK;` |
|        - | 13244 | `	}` |
|       27 | 13245 | `	if( nArg > 1 ){` |
|      ! 0 | 13246 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 13247 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 13248 | `		switch(nComponent){` |
|      ! 0 | 13249 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 13250 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 13251 | `			if( pComp->nByte < 1 ){` |
|        - | 13252 | `				/* No available value,return NULL */` |
|      ! 0 | 13253 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13254 | `			}else{` |
|      ! 0 | 13255 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13256 | `			}` |
|      ! 0 | 13257 | `			break;` |
|      ! 0 | 13258 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 13259 | `			pComp = &sURI.sHost;` |
|      ! 0 | 13260 | `			if( pComp->nByte < 1 ){` |
|        - | 13261 | `				/* No available value,return NULL */` |
|      ! 0 | 13262 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13263 | `			}else{` |
|      ! 0 | 13264 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13265 | `			}` |
|      ! 0 | 13266 | `			break;` |
|      ! 0 | 13267 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 13268 | `			pComp = &sURI.sPort;` |
|      ! 0 | 13269 | `			if( pComp->nByte < 1 ){` |
|        - | 13270 | `				/* No available value,return NULL */` |
|      ! 0 | 13271 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13272 | `			}else{` |
|      ! 0 | 13273 | `				int iPort = 0;` |
|        - | 13274 | `				/* Cast the value to integer */` |
|      ! 0 | 13275 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 13276 | `				ph7_result_int(pCtx,iPort);` |
|        - | 13277 | `			}` |
|      ! 0 | 13278 | `			break;` |
|      ! 0 | 13279 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 13280 | `			pComp = &sURI.sUser;` |
|      ! 0 | 13281 | `			if( pComp->nByte < 1 ){` |
|        - | 13282 | `				/* No available value,return NULL */` |
|      ! 0 | 13283 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13284 | `			}else{` |
|      ! 0 | 13285 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13286 | `			}` |
|      ! 0 | 13287 | `			break;` |
|      ! 0 | 13288 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 13289 | `			pComp = &sURI.sPass;` |
|      ! 0 | 13290 | `			if( pComp->nByte < 1 ){` |
|        - | 13291 | `				/* No available value,return NULL */` |
|      ! 0 | 13292 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13293 | `			}else{` |
|      ! 0 | 13294 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13295 | `			}` |
|      ! 0 | 13296 | `			break;` |
|      ! 0 | 13297 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 13298 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 13299 | `			if( pComp->nByte < 1 ){` |
|        - | 13300 | `				/* No available value,return NULL */` |
|      ! 0 | 13301 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13302 | `			}else{` |
|      ! 0 | 13303 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13304 | `			}` |
|      ! 0 | 13305 | `			break;` |
|      ! 0 | 13306 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 13307 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 13308 | `			if( pComp->nByte < 1 ){` |
|        - | 13309 | `				/* No available value,return NULL */` |
|      ! 0 | 13310 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13311 | `			}else{` |
|      ! 0 | 13312 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13313 | `			}` |
|      ! 0 | 13314 | `			break;` |
|      ! 0 | 13315 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 13316 | `			pComp = &sURI.sPath;` |
|      ! 0 | 13317 | `			if( pComp->nByte < 1 ){` |
|        - | 13318 | `				/* No available value,return NULL */` |
|      ! 0 | 13319 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13320 | `			}else{` |
|      ! 0 | 13321 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13322 | `			}` |
|      ! 0 | 13323 | `			break;` |
|      ! 0 | 13324 | `		default:` |
|        - | 13325 | `			/* No such entry,return NULL */` |
|      ! 0 | 13326 | `			ph7_result_null(pCtx);` |
|      ! 0 | 13327 | `			break;` |
|        - | 13328 | `		}` |
|      ! 0 | 13329 | `	}else{` |
|        - | 13330 | `		ph7_value *pArray,*pValue;` |
|        - | 13331 | `		/* Return an associative array */` |
|       27 | 13332 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 13333 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 13334 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 13335 | `			/* Out of memory */` |
|      ! 0 | 13336 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 13337 | `			/* Return false */` |
|      ! 0 | 13338 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 13339 | `			return PH7_OK;` |
|        - | 13340 | `		}` |
|        - | 13341 | `		/* Fill the array */` |
|       27 | 13342 | `		pComp = &sURI.sScheme;` |
|       27 | 13343 | `		if( pComp->nByte > 0 ){` |
|       19 | 13344 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 13345 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 13346 | `		}` |
|        - | 13347 | `		/* Reset the string cursor */` |
|       27 | 13348 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13349 | `		pComp = &sURI.sHost;` |
|       27 | 13350 | `		if( pComp->nByte > 0 ){` |
|       25 | 13351 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 13352 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 13353 | `		}` |
|        - | 13354 | `		/* Reset the string cursor */` |
|       27 | 13355 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13356 | `		pComp = &sURI.sPort;` |
|       27 | 13357 | `		if( pComp->nByte > 0 ){` |
|       11 | 13358 | `			int iPort = 0;/* cc warning */` |
|        - | 13359 | `			/* Convert to integer */` |
|       11 | 13360 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 13361 | `			ph7_value_int(pValue,iPort);` |
|       11 | 13362 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 13363 | `		}` |
|        - | 13364 | `		/* Reset the string cursor */` |
|       27 | 13365 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13366 | `		pComp = &sURI.sUser;` |
|       27 | 13367 | `		if( pComp->nByte > 0 ){` |
|        7 | 13368 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 13369 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 13370 | `		}` |
|        - | 13371 | `		/* Reset the string cursor */` |
|       27 | 13372 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13373 | `		pComp = &sURI.sPass;` |
|       27 | 13374 | `		if( pComp->nByte > 0 ){` |
|        7 | 13375 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 13376 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 13377 | `		}` |
|        - | 13378 | `		/* Reset the string cursor */` |
|       27 | 13379 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13380 | `		pComp = &sURI.sPath;` |
|       27 | 13381 | `		if( pComp->nByte > 0 ){` |
|       17 | 13382 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 13383 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 13384 | `		}` |
|        - | 13385 | `		/* Reset the string cursor */` |
|       27 | 13386 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13387 | `		pComp = &sURI.sQuery;` |
|       27 | 13388 | `		if( pComp->nByte > 0 ){` |
|        5 | 13389 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 13390 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 13391 | `		}` |
|        - | 13392 | `		/* Reset the string cursor */` |
|       27 | 13393 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13394 | `		pComp = &sURI.sFragment;` |
|       27 | 13395 | `		if( pComp->nByte > 0 ){` |
|        5 | 13396 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 13397 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 13398 | `		}` |
|        - | 13399 | `		/* Return the created array */` |
|       27 | 13400 | `		ph7_result_value(pCtx,pArray);` |
|        - | 13401 | `		/* NOTE:` |
|        - | 13402 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 13403 | `		 * automatically as soon we return from this function.` |
|        - | 13404 | `		 */` |
|        - | 13405 | `	}` |
|        - | 13406 | `	/* All done */` |
|       27 | 13407 | `	return PH7_OK;` |
|       15 | 13408 |  |
|        - | 13409 | `/*` |
|        - | 13410 | ` * Section:` |
|        - | 13411 | ` *   Array related routines.` |
|        - | 13412 | ` * Status:` |
|        - | 13413 | ` *    Stable.` |
|        - | 13414 | ` * Note 2012-5-21 01:04:15:` |
|        - | 13415 | ` *  Array related functions that need access to the underlying` |
|        - | 13416 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 13417 | ` */` |
|        - | 13418 | `/*` |
|        - | 13419 | ` * The [compact()] function store it's state information in an instance` |
|        - | 13420 | ` * of the following structure.` |
|        - | 13421 | ` */` |
|        - | 13422 | `struct compact_data` |
|        - | 13423 |  |
|        - | 13424 | `	ph7_value *pArray;  /* Target array */` |
|        - | 13425 | `	int nRecCount;      /* Recursion count */` |
|        - | 13426 | `};` |
|        - | 13427 | `/*` |
|        - | 13428 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 13429 | ` */` |
|      ! 0 | 13430 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 13431 |  |
|      ! 0 | 13432 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 13433 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 13434 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 13435 | `	/* Act according to the hashmap value */` |
|      ! 0 | 13436 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 13437 | `		SyString sVar;` |
|      ! 0 | 13438 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 13439 | `		if( sVar.nByte > 0 ){` |
|        - | 13440 | `			/* Query the current frame */` |
|      ! 0 | 13441 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 13442 | `			/* ^` |
|        - | 13443 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 13444 | `			 */` |
|      ! 0 | 13445 | `			if( pKey ){` |
|        - | 13446 | `				/* Perform the insertion */` |
|      ! 0 | 13447 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 13448 | `			}` |
|      ! 0 | 13449 | `		}` |
|      ! 0 | 13450 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 13451 | `		int rc;` |
|        - | 13452 | `		/* Recursively traverse this array */` |
|      ! 0 | 13453 | `		pData->nRecCount++;` |
|      ! 0 | 13454 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 13455 | `		pData->nRecCount--;` |
|      ! 0 | 13456 | `		return rc;` |
|        - | 13457 | `	}` |
|      ! 0 | 13458 | `	return SXRET_OK;` |
|      ! 0 | 13459 |  |
|        - | 13460 | `/*` |
|        - | 13461 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 13462 | ` *  Create array containing variables and their values.` |
|        - | 13463 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 13464 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 13465 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 13466 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 13467 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 13468 | ` * Parameters` |
|        - | 13469 | ` *  $varname` |
|        - | 13470 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 13471 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 13472 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 13473 | ` *   it recursively.` |
|        - | 13474 | ` * Return` |
|        - | 13475 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 13476 | ` */` |
|        2 | 13477 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13478 |  |
|        - | 13479 | `	ph7_value *pArray,*pObj;` |
|        3 | 13480 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13481 | `	const char *zName;` |
|        - | 13482 | `	SyString sVar;` |
|        - | 13483 | `	int i,nLen;` |
|        3 | 13484 | `	if( nArg < 1 ){` |
|        - | 13485 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 13486 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13487 | `		return PH7_OK;` |
|        - | 13488 | `	}` |
|        - | 13489 | `	/* Create the array */` |
|        3 | 13490 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 13491 | `	if( pArray == 0 ){` |
|        - | 13492 | `		/* Out of memory */` |
|      ! 0 | 13493 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 13494 | `		/* Return NULL */` |
|      ! 0 | 13495 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13496 | `		return PH7_OK;` |
|        - | 13497 | `	}` |
|        - | 13498 | `	/* Perform the requested operation */` |
|        7 | 13499 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 13500 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 13501 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 13502 | `				struct compact_data sData;` |
|      ! 0 | 13503 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 13504 | `				/* Recursively walk the array */` |
|      ! 0 | 13505 | `				sData.nRecCount = 0;` |
|      ! 0 | 13506 | `				sData.pArray = pArray;` |
|      ! 0 | 13507 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 13508 | `			}` |
|      ! 0 | 13509 | `		}else{` |
|        - | 13510 | `			/* Extract variable name */` |
|        5 | 13511 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 13512 | `			if( nLen > 0 ){` |
|        5 | 13513 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 13514 | `				/* Check if the variable is available in the current frame */` |
|        5 | 13515 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 13516 | `				if( pObj ){` |
|        5 | 13517 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 13518 | `				}` |
|        2 | 13519 | `			}` |
|        - | 13520 | `		}` |
|        3 | 13521 | `	}` |
|        - | 13522 | `	/* Return the array */` |
|        3 | 13523 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 13524 | `	return PH7_OK;` |
|        2 | 13525 |  |
|        - | 13526 | `/*` |
|        - | 13527 | ` * The [extract()] function store it's state information in an instance` |
|        - | 13528 | ` * of the following structure.` |
|        - | 13529 | ` */` |
|        - | 13530 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 13531 | `struct extract_aux_data` |
|        - | 13532 |  |
|        - | 13533 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 13534 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 13535 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 13536 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 13537 | `	int iFlags;           /* Control flags */` |
|        - | 13538 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 13539 | `};` |
|        - | 13540 | `/* Forward declaration */` |
|        - | 13541 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 13542 | `/*` |
|        - | 13543 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 13544 | ` *   Import variables into the current symbol table from an array.` |
|        - | 13545 | ` * Parameters` |
|        - | 13546 | ` * $var_array` |
|        - | 13547 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 13548 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 13549 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 13550 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 13551 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 13552 | ` * $extract_type` |
|        - | 13553 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 13554 | ` *  It can be one of the following values:` |
|        - | 13555 | ` *   EXTR_OVERWRITE` |
|        - | 13556 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 13557 | ` *   EXTR_SKIP` |
|        - | 13558 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 13559 | ` *   EXTR_PREFIX_SAME` |
|        - | 13560 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 13561 | ` *   EXTR_PREFIX_ALL` |
|        - | 13562 | ` *       Prefix all variable names with prefix.` |
|        - | 13563 | ` *   EXTR_PREFIX_INVALID` |
|        - | 13564 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 13565 | ` *   EXTR_IF_EXISTS` |
|        - | 13566 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 13567 | ` *       otherwise do nothing.` |
|        - | 13568 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 13569 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 13570 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 13571 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 13572 | ` *      the current symbol table.` |
|        - | 13573 | ` * $prefix` |
|        - | 13574 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 13575 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 13576 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 13577 | ` *  underscore character.` |
|        - | 13578 | ` * Return` |
|        - | 13579 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 13580 | ` */` |
|        4 | 13581 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13582 |  |
|        - | 13583 | `	extract_aux_data sAux;` |
|        - | 13584 | `	ph7_hashmap *pMap;` |
|        5 | 13585 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 13586 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 13587 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 13588 | `		return PH7_OK;` |
|        - | 13589 | `	}` |
|        - | 13590 | `	/* Point to the target hashmap */` |
|        5 | 13591 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 13592 | `	if( pMap->nEntry < 1 ){` |
|        - | 13593 | `		/* Empty map,return  0 */` |
|      ! 0 | 13594 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 13595 | `		return PH7_OK;` |
|        - | 13596 | `	}` |
|        - | 13597 | `	/* Prepare the aux data */` |
|        5 | 13598 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 13599 | `	if( nArg > 1 ){` |
|        3 | 13600 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 13601 | `		if( nArg > 2 ){` |
|      ! 0 | 13602 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 13603 | `		}` |
|        1 | 13604 | `	}` |
|        5 | 13605 | `	sAux.pVm = pCtx->pVm;` |
|        - | 13606 | `	/* Invoke the worker callback */` |
|        5 | 13607 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 13608 | `	/* Number of variables successfully imported */` |
|        5 | 13609 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 13610 | `	return PH7_OK;` |
|        3 | 13611 |  |
|        - | 13612 | `/*` |
|        - | 13613 | ` * Worker callback for the [extract()] function defined` |
|        - | 13614 | ` * below.` |
|        - | 13615 | ` */` |
|        8 | 13616 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 13617 |  |
|        9 | 13618 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 13619 | `	int iFlags = pAux->iFlags;` |
|        9 | 13620 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 13621 | `	ph7_value *pObj;` |
|        - | 13622 | `	SyString sVar;` |
|        9 | 13623 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 13624 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 13625 | `	}` |
|        - | 13626 | `	/* Perform a string cast */` |
|        9 | 13627 | `	PH7_MemObjToString(pKey);` |
|        9 | 13628 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 13629 | `		/* Unavailable variable name */` |
|      ! 0 | 13630 | `		return SXRET_OK;` |
|        - | 13631 | `	}` |
|        9 | 13632 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 13633 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 13634 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 13635 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 13636 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 13637 | `			);` |
|      ! 0 | 13638 | `	}else{` |
|       13 | 13639 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 13640 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 13641 | `	}` |
|        9 | 13642 | `	sVar.zString = pAux->zWorker;` |
|        - | 13643 | `	/* Try to extract the variable */` |
|        9 | 13644 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 13645 | `	if( pObj ){` |
|        - | 13646 | `		/* Collision */` |
|        5 | 13647 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 13648 | `			return SXRET_OK;` |
|        - | 13649 | `		}` |
|        5 | 13650 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 13651 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 13652 | `				/* Already prefixed */` |
|      ! 0 | 13653 | `				return SXRET_OK;` |
|        - | 13654 | `			}` |
|      ! 0 | 13655 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 13656 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 13657 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 13658 | `				);` |
|      ! 0 | 13659 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 13660 | `		}` |
|        3 | 13661 | `	}else{` |
|        - | 13662 | `		/* Create the variable */` |
|        5 | 13663 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 13664 | `	}` |
|        9 | 13665 | `	if( pObj ){` |
|        - | 13666 | `		/* Overwrite the old value */` |
|        9 | 13667 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 13668 | `		/* Increment counter */` |
|        9 | 13669 | `		pAux->iCount++;` |
|        4 | 13670 | `	}` |
|        9 | 13671 | `	return SXRET_OK;` |
|        5 | 13672 |  |
|        - | 13673 | `/*` |
|        - | 13674 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 13675 | ` * defined below.` |
|        - | 13676 | ` */` |
|        2 | 13677 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 13678 |  |
|        3 | 13679 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 13680 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 13681 | `	ph7_value *pObj;` |
|        - | 13682 | `	SyString sVar;` |
|        - | 13683 | `	/* Perform a string cast */` |
|        3 | 13684 | `	PH7_MemObjToString(pKey);` |
|        3 | 13685 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 13686 | `		/* Unavailable variable name */` |
|      ! 0 | 13687 | `		return SXRET_OK;` |
|        - | 13688 | `	}` |
|        3 | 13689 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 13690 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 13691 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 13692 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 13693 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 13694 | `			);` |
|        2 | 13695 | `	}else{` |
|      ! 0 | 13696 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 13697 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 13698 | `	}` |
|        3 | 13699 | `	sVar.zString = pAux->zWorker;` |
|        - | 13700 | `	/* Extract the variable */` |
|        3 | 13701 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 13702 | `	if( pObj ){` |
|        3 | 13703 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 13704 | `	}` |
|        3 | 13705 | `	return SXRET_OK;` |
|        2 | 13706 |  |
|        - | 13707 | `/*` |
|        - | 13708 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 13709 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 13710 | ` * Parameters` |
|        - | 13711 | ` * $types` |
|        - | 13712 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 13713 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 13714 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 13715 | ` *  POST includes the POST uploaded file information.` |
|        - | 13716 | ` *  Note:` |
|        - | 13717 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 13718 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 13719 | ` * $prefix` |
|        - | 13720 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 13721 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 13722 | ` *  variable named $pref_userid.` |
|        - | 13723 | ` * Return` |
|        - | 13724 | ` *  TRUE on success or FALSE on failure.` |
|        - | 13725 | ` */` |
|        2 | 13726 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13727 |  |
|        - | 13728 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 13729 | `	extract_aux_data sAux;` |
|        - | 13730 | `	int nLen,nPrefixLen;` |
|        - | 13731 | `	ph7_value *pSuper;` |
|        - | 13732 | `	ph7_vm *pVm;` |
|        - | 13733 | `	/* By default import only $_GET variables  */` |
|        3 | 13734 | `	zImport = "G";` |
|        3 | 13735 | `	nLen = (int)sizeof(char);` |
|        3 | 13736 | `	zPrefix = 0;` |
|        3 | 13737 | `	nPrefixLen = 0;` |
|        3 | 13738 | `	if( nArg > 0 ){` |
|        3 | 13739 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 13740 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 13741 | `		}` |
|        3 | 13742 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 13743 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 13744 | `		}` |
|        1 | 13745 | `	}` |
|        - | 13746 | `	/* Point to the underlying VM */` |
|        3 | 13747 | `	pVm = pCtx->pVm;` |
|        - | 13748 | `	/* Initialize the aux data */` |
|        3 | 13749 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 13750 | `	sAux.zPrefix = zPrefix;` |
|        3 | 13751 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 13752 | `	sAux.pVm = pVm;` |
|        - | 13753 | `	/* Extract */` |
|        3 | 13754 | `	zEnd = &zImport[nLen];` |
|        5 | 13755 | `	while( zImport < zEnd ){` |
|        3 | 13756 | `		int c = zImport[0];` |
|        3 | 13757 | `		pSuper = 0;` |
|        3 | 13758 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 13759 | `			/* Import $_GET variables */` |
|        3 | 13760 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 13761 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 13762 | `			/* Import $_POST variables */` |
|      ! 0 | 13763 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 13764 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 13765 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 13766 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 13767 | `		}` |
|        3 | 13768 | `		if( pSuper ){` |
|        - | 13769 | `			/* Iterate throw array entries */` |
|        3 | 13770 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 13771 | `		}` |
|        - | 13772 | `		/* Advance the cursor */` |
|        3 | 13773 | `		zImport++;` |
|        1 | 13774 | `	}` |
|        - | 13775 | `	/* All done,return TRUE*/` |
|        3 | 13776 | `	ph7_result_bool(pCtx,0);` |
|        3 | 13777 | `	return PH7_OK;` |
|        1 | 13778 |  |
|        - | 13779 | `/*` |
|        - | 13780 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 13781 | ` * Refer to the eval() language construct implementation for more` |
|        - | 13782 | ` * information.` |
|        - | 13783 | ` */` |
|    12280 | 13784 | `static sxi32 VmEvalChunk(` |
|        - | 13785 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 13786 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 13787 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 13788 | `	int iFlags,         /* Compile flag */` |
|        - | 13789 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 13790 | `	)` |
|        2 | 13791 |  |
|        - | 13792 | `	SySet *pByteCode,aByteCode;` |
|        - | 13793 | `	SyBlob sSavedNs;` |
|    12282 | 13794 | `	ProcConsumer xErr = 0;` |
|    12282 | 13795 | `	void *pErrData = 0;` |
|        - | 13796 | `	/* Initialize bytecode container */` |
|    12282 | 13797 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    12282 | 13798 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 13799 | `	/* Reset the code generator */` |
|    12282 | 13800 | `	if( bTrueReturn ){` |
|        - | 13801 | `		/* Included file,log compile-time errors */` |
|     9270 | 13802 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9270 | 13803 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4634 | 13804 | `	}` |
|    12282 | 13805 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 13806 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 13807 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 13808 | `	 * the caller's namespace is restored. */` |
|    12282 | 13809 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    12282 | 13810 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    12282 | 13811 | `	if( bTrueReturn ){` |
|        - | 13812 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9270 | 13813 | `		SyBlobReset(&pVm->sNamespace);` |
|     4634 | 13814 | `	}` |
|        - | 13815 | `	/* Swap bytecode container */` |
|    12282 | 13816 | `	pByteCode = pVm->pByteContainer;` |
|    12282 | 13817 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 13818 | `	/* Compile the chunk */` |
|    12282 | 13819 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    18422 | 13820 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 13821 | `		/* Compilation error,return false */` |
|        3 | 13822 | `		if( pCtx ){` |
|        3 | 13823 | `			ph7_result_bool(pCtx,0);` |
|        1 | 13824 | `		}` |
|        2 | 13825 | `	}else{` |
|        - | 13826 | `		/* Mount any newly defined classes */` |
|        - | 13827 | `		SyHashEntry *pEntry;` |
|        - | 13828 | `		ph7_class *pClass;` |
|        - | 13829 | `		ph7_value sResult; /* Return value */` |
|        - | 13830 | `		sxi32 rc;` |
|    12280 | 13831 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   630037 | 13832 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   611620 | 13833 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 13834 | `			/* Only mount classes that haven't been mounted yet */` |
|   611620 | 13835 | `			if( !pClass->bMounted ){` |
|   111318 | 13836 | `				rc = VmMountUserClass(pVm,pClass);` |
|   111318 | 13837 | `				if( rc != SXRET_OK ){` |
|        - | 13838 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 13839 | `					if( pCtx ){` |
|      ! 0 | 13840 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 13841 | `					}` |
|      ! 0 | 13842 | `					goto Cleanup;` |
|        - | 13843 | `				}` |
|    55658 | 13844 | `			}` |
|        2 | 13845 | `		}` |
|    12280 | 13846 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 13847 | `			/* Out of memory */` |
|      ! 0 | 13848 | `			if( pCtx ){` |
|      ! 0 | 13849 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 13850 | `			}` |
|      ! 0 | 13851 | `			goto Cleanup;` |
|        - | 13852 | `		}` |
|    12280 | 13853 | `		if( bTrueReturn ){` |
|        - | 13854 | `			/* Assume a boolean true return value */` |
|     9270 | 13855 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4636 | 13856 | `		}else{` |
|        - | 13857 | `			/* Assume a null return value */` |
|     3012 | 13858 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 13859 | `		}` |
|        - | 13860 | `		/* Execute the compiled chunk */` |
|    12280 | 13861 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    12280 | 13862 | `		if( pCtx ){` |
|        - | 13863 | `			/* Set the execution result */` |
|     9288 | 13864 | `			ph7_result_value(pCtx,&sResult);` |
|     4643 | 13865 | `		}` |
|    12280 | 13866 | `		PH7_MemObjRelease(&sResult);` |
|        - | 13867 | `	}` |
|     6140 | 13868 | `Cleanup:` |
|        - | 13869 | `	/* Cleanup the mess left behind */` |
|    12282 | 13870 | `	pVm->pByteContainer = pByteCode;` |
|    12282 | 13871 | `	SySetRelease(&aByteCode);` |
|        - | 13872 | `	/* Restore caller's namespace state */` |
|    12282 | 13873 | `	SyBlobReset(&pVm->sNamespace);` |
|    12282 | 13874 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    12282 | 13875 | `	SyBlobRelease(&sSavedNs);` |
|    12282 | 13876 | `	return SXRET_OK;` |
|        2 | 13877 |  |
|        - | 13878 | `/*` |
|        - | 13879 | ` * value eval(string $code)` |
|        - | 13880 | ` *   Evaluate a string as PHP code.` |
|        - | 13881 | ` * Parameter` |
|        - | 13882 | ` *  code: PHP code to evaluate.` |
|        - | 13883 | ` * Return` |
|        - | 13884 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 13885 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 13886 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 13887 | ` */` |
|       22 | 13888 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13889 |  |
|        - | 13890 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 13891 | `	if( nArg < 1 ){` |
|        - | 13892 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13893 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13894 | `		return SXRET_OK;` |
|        - | 13895 | `	}` |
|        - | 13896 | `	/* Chunk to evaluate */` |
|       24 | 13897 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 13898 | `	if( sChunk.nByte < 1 ){` |
|        - | 13899 | `		/* Empty string,return NULL */` |
|        3 | 13900 | `		ph7_result_null(pCtx);` |
|        3 | 13901 | `		return SXRET_OK;` |
|        - | 13902 | `	}` |
|        - | 13903 | `	/* Eval the chunk */` |
|       22 | 13904 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 13905 | `	return SXRET_OK;` |
|       13 | 13906 |  |
|        - | 13907 | `/*` |
|        - | 13908 | ` * Check if a file path is already included.` |
|        - | 13909 | ` */` |
|    18532 | 13910 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 13911 |  |
|        - | 13912 | `	SyString *aEntries;` |
|        - | 13913 | `	sxu32 n;` |
|    18534 | 13914 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 13915 | `	/* Perform a linear search */` |
| 85804198 | 13916 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 85785672 | 13917 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 13918 | `			/* Already included */` |
|        7 | 13919 | `			return TRUE;` |
|        - | 13920 | `		}` |
| 42892834 | 13921 | `	}` |
|    18528 | 13922 | `	return FALSE;` |
|     9268 | 13923 |  |
|        - | 13924 | `/*` |
|        - | 13925 | ` * Push a file path in the appropriate VM container.` |
|        - | 13926 | ` */` |
|    21516 | 13927 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 13928 |  |
|        - | 13929 | `	SyString sPath;` |
|        - | 13930 | `	char *zDup;` |
|        - | 13931 | `#ifdef __WINNT__` |
|        - | 13932 | `	char *zCur;` |
|        - | 13933 | `#endif` |
|        - | 13934 | `	sxi32 rc;` |
|    21518 | 13935 | `	if( nLen < 0 ){` |
|     2986 | 13936 | `		nLen = SyStrlen(zPath);` |
|     1492 | 13937 | `	}` |
|        - | 13938 | `	/* Duplicate the file path first */` |
|    21518 | 13939 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    21518 | 13940 | `	if( zDup == 0 ){` |
|      ! 0 | 13941 | `		return SXERR_MEM;` |
|        - | 13942 | `	}` |
|        - | 13943 | `#ifdef __WINNT__` |
|        - | 13944 | `	/* Normalize path on windows` |
|        - | 13945 | `	 * Example:` |
|        - | 13946 | `	 *    Path/To/File.php` |
|        - | 13947 | `	 * becomes` |
|        - | 13948 | `	 *   path\to\file.php` |
|        - | 13949 | `	 */` |
|        2 | 13950 | `	zCur = zDup;` |
|        2 | 13951 | `	while( zCur[0] != 0 ){` |
|        2 | 13952 | `		if( zCur[0] == '/' ){` |
|        2 | 13953 | `			zCur[0] = '\\';` |
|        2 | 13954 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 13955 | `			int c = SyToLower(zCur[0]);` |
|        1 | 13956 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 13957 | `		}` |
|        2 | 13958 | `		zCur++;` |
|        2 | 13959 | `	}` |
|        - | 13960 | `#endif` |
|        - | 13961 | `	/* Install the file path */` |
|    21518 | 13962 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    21518 | 13963 | `	if( !bMain ){` |
|    18534 | 13964 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 13965 | `			/* Already included */` |
|        7 | 13966 | `			*pNew = 0;` |
|        4 | 13967 | `		}else{` |
|        - | 13968 | `			/* Insert in the corresponding container */` |
|    18528 | 13969 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    18528 | 13970 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 13971 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 13972 | `				return rc;` |
|        - | 13973 | `			}` |
|    18528 | 13974 | `			*pNew = 1;` |
|        - | 13975 | `		}` |
|     9266 | 13976 | `	}` |
|    21518 | 13977 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    21518 | 13978 | `	return SXRET_OK;` |
|    10760 | 13979 |  |
|        - | 13980 | `/*` |
|        - | 13981 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 13982 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 13983 | ` * indicates failure.` |
|        - | 13984 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 13985 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 13986 | ` * operations.` |
|        - | 13987 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 13988 | ` * this function is a no-op.` |
|        - | 13989 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 13990 | ` * constructs for more information.` |
|        - | 13991 | ` */` |
|     9278 | 13992 | `static sxi32 VmExecIncludedFile(` |
|        - | 13993 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 13994 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 13995 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 13996 | `	 )` |
|        2 | 13997 |  |
|        - | 13998 | `	sxi32 rc;` |
|        - | 13999 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14000 | `	const ph7_io_stream *pStream;` |
|        - | 14001 | `	SyBlob sContents;` |
|        - | 14002 | `	void *pHandle;` |
|        - | 14003 | `	ph7_vm *pVm;` |
|        - | 14004 | `	int isNew;` |
|        - | 14005 | `	/* Initialize fields */` |
|     9280 | 14006 | `	pVm = pCtx->pVm;` |
|     9280 | 14007 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9280 | 14008 | `	isNew = 0;` |
|        - | 14009 | `	/* Extract the associated stream */` |
|     9280 | 14010 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 14011 | `	/*` |
|        - | 14012 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 14013 | `	 * in a read-only mode.` |
|        - | 14014 | `	 */` |
|     9280 | 14015 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9280 | 14016 | `	if( pHandle == 0 ){` |
|        8 | 14017 | `		return SXERR_IO;` |
|        - | 14018 | `	}` |
|     9274 | 14019 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9274 | 14020 | `	if( IncludeOnce && !isNew ){` |
|        - | 14021 | `		/* Already included */` |
|        5 | 14022 | `		rc = SXERR_EXISTS;` |
|        3 | 14023 | `	}else{` |
|        - | 14024 | `		/* Read the whole file contents */` |
|     9270 | 14025 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9270 | 14026 | `		if( rc == SXRET_OK ){` |
|        - | 14027 | `			SyString sScript;` |
|        - | 14028 | `			/* Compile and execute the script */` |
|     9270 | 14029 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9270 | 14030 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4634 | 14031 | `		}` |
|        - | 14032 | `	}` |
|        - | 14033 | `	/* Pop from the set of included file */` |
|     9274 | 14034 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 14035 | `	/* Close the handle */` |
|     9274 | 14036 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 14037 | `	/* Release the working buffer */` |
|     9274 | 14038 | `	SyBlobRelease(&sContents);` |
|        - | 14039 | `#else` |
|        - | 14040 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 14041 | `	SXUNUSED(pPath);` |
|        - | 14042 | `	SXUNUSED(IncludeOnce);` |
|        - | 14043 | `	rc = SXERR_IO;` |
|        - | 14044 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9274 | 14045 | `	return rc;` |
|     4641 | 14046 |  |
|        - | 14047 | `/*` |
|        - | 14048 | ` * string get_include_path(void)` |
|        - | 14049 | ` *  Gets the current include_path configuration option.` |
|        - | 14050 | ` * Parameter` |
|        - | 14051 | ` *  None` |
|        - | 14052 | ` * Return` |
|        - | 14053 | ` *  Included paths as a string` |
|        - | 14054 | ` */` |
|        2 | 14055 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14056 |  |
|        3 | 14057 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14058 | `	SyString *aEntry;` |
|        - | 14059 | `	int dir_sep;` |
|        - | 14060 | `	sxu32 n;` |
|        - | 14061 | `#ifdef __WINNT__` |
|        1 | 14062 | `	dir_sep = ';';` |
|        - | 14063 | `#else` |
|        - | 14064 | `	/* Assume UNIX path separator */` |
|        2 | 14065 | `	dir_sep = ':';` |
|        - | 14066 | `#endif` |
|        1 | 14067 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14068 | `	SXUNUSED(apArg);` |
|        - | 14069 | `	/* Point to the list of import paths */` |
|        3 | 14070 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 14071 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 14072 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 14073 | `		if( n > 0 ){` |
|        - | 14074 | `			/* Append dir seprator */` |
|      ! 0 | 14075 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 14076 | `		}` |
|        - | 14077 | `		/* Append path */` |
|        3 | 14078 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 14079 | `	}` |
|        3 | 14080 | `	return PH7_OK;` |
|        1 | 14081 |  |
|        - | 14082 | `/*` |
|        - | 14083 | ` * string get_get_included_files(void)` |
|        - | 14084 | ` *  Gets the current include_path configuration option.` |
|        - | 14085 | ` * Parameter` |
|        - | 14086 | ` *  None` |
|        - | 14087 | ` * Return` |
|        - | 14088 | ` *  Included paths as a string` |
|        - | 14089 | ` */` |
|        2 | 14090 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14091 |  |
|        3 | 14092 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 14093 | `	ph7_value *pArray,*pWorker;` |
|        - | 14094 | `	SyString *pEntry;` |
|        - | 14095 | `	int c,d;` |
|        - | 14096 | `	/* Create an array and a working value */` |
|        3 | 14097 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 14098 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 14099 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 14100 | `		/* Out of memory,return null */` |
|      ! 0 | 14101 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14102 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 14103 | `		SXUNUSED(apArg);` |
|      ! 0 | 14104 | `		return PH7_OK;` |
|        - | 14105 | `	}` |
|        3 | 14106 | `	c = d = '/';` |
|        - | 14107 | `#ifdef __WINNT__` |
|        1 | 14108 | `	d = '\\';` |
|        - | 14109 | `#endif` |
|        - | 14110 | `	/* Iterate throw entries */` |
|        3 | 14111 | `	SySetResetCursor(pFiles);` |
|     3839 | 14112 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 14113 | `		const char *zBase,*zEnd;` |
|        - | 14114 | `		int iLen;` |
|        - | 14115 | `		/* reset the string cursor */` |
|     3837 | 14116 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 14117 | `		/* Extract base name */` |
|     3837 | 14118 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 14119 | `		/* Ignore trailing '/' */` |
|     5755 | 14120 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 14121 | `			zEnd--;` |
|      ! 0 | 14122 | `		}` |
|     3837 | 14123 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   118297 | 14124 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   112543 | 14125 | `			zEnd--;` |
|        1 | 14126 | `		}` |
|     3837 | 14127 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3837 | 14128 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 14129 | `		/* Copy entry name */` |
|     3837 | 14130 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 14131 | `		/* Perform the insertion */` |
|     3837 | 14132 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 14133 | `	}` |
|        - | 14134 | `	/* All done,return the created array */` |
|        3 | 14135 | `	ph7_result_value(pCtx,pArray);` |
|        - | 14136 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 14137 | `	 * by the engine as soon we return from this foreign` |
|        - | 14138 | `	 * function.` |
|        - | 14139 | `	 */` |
|        3 | 14140 | `	return PH7_OK;` |
|        2 | 14141 |  |
|        - | 14142 | `/*` |
|        - | 14143 | ` * include:` |
|        - | 14144 | ` * According to the PHP reference manual.` |
|        - | 14145 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 14146 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 14147 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 14148 | ` *  include() will finally check in the calling script's own directory` |
|        - | 14149 | ` *  and the current working directory before failing. The include()` |
|        - | 14150 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 14151 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 14152 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 14153 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 14154 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 14155 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 14156 | ` *  directory to find the requested file.` |
|        - | 14157 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 14158 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 14159 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 14160 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 14161 | ` */` |
|     9260 | 14162 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14163 |  |
|        - | 14164 | `	SyString sFile;` |
|        - | 14165 | `	sxi32 rc;` |
|     9262 | 14166 | `	if( nArg < 1 ){` |
|        - | 14167 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14168 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14169 | `		return SXRET_OK;` |
|        - | 14170 | `	}` |
|        - | 14171 | `	/* File to include */` |
|     9262 | 14172 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9262 | 14173 | `	if( sFile.nByte < 1 ){` |
|        - | 14174 | `		/* Empty string,return NULL */` |
|      ! 0 | 14175 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14176 | `		return SXRET_OK;` |
|        - | 14177 | `	}` |
|        - | 14178 | `	/* Open,compile and execute the desired script */` |
|     9262 | 14179 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9262 | 14180 | `	if( rc != SXRET_OK ){` |
|        - | 14181 | `		/* Emit a warning and return false */` |
|        3 | 14182 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 14183 | `		ph7_result_bool(pCtx,0);` |
|        1 | 14184 | `	}` |
|     9262 | 14185 | `	return SXRET_OK;` |
|     4632 | 14186 |  |
|        - | 14187 | `/*` |
|        - | 14188 | ` * include_once:` |
|        - | 14189 | ` *  According to the PHP reference manual.` |
|        - | 14190 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 14191 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 14192 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 14193 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 14194 | ` *   just once.` |
|        - | 14195 | ` */` |
|        4 | 14196 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14197 |  |
|        - | 14198 | `	SyString sFile;` |
|        - | 14199 | `	sxi32 rc;` |
|        5 | 14200 | `	if( nArg < 1 ){` |
|        - | 14201 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14202 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14203 | `		return SXRET_OK;` |
|        - | 14204 | `	}` |
|        - | 14205 | `	/* File to include */` |
|        5 | 14206 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 14207 | `	if( sFile.nByte < 1 ){` |
|        - | 14208 | `		/* Empty string,return NULL */` |
|      ! 0 | 14209 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14210 | `		return SXRET_OK;` |
|        - | 14211 | `	}` |
|        - | 14212 | `	/* Open,compile and execute the desired script */` |
|        5 | 14213 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 14214 | `	if( rc == SXERR_EXISTS ){` |
|        - | 14215 | `		/* File already included,return TRUE */` |
|        3 | 14216 | `		ph7_result_bool(pCtx,1);` |
|        3 | 14217 | `		return SXRET_OK;` |
|        - | 14218 | `	}` |
|        3 | 14219 | `	if( rc != SXRET_OK ){` |
|        - | 14220 | `		/* Emit a warning and return false */` |
|      ! 0 | 14221 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14222 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14223 | ` 	}` |
|        3 | 14224 | `	return SXRET_OK;` |
|        3 | 14225 |  |
|        - | 14226 | `/*` |
|        - | 14227 | ` * require.` |
|        - | 14228 | ` *  According to the PHP reference manual.` |
|        - | 14229 | ` *   require() is identical to include() except upon failure it will` |
|        - | 14230 | ` *   also produce a fatal level error.` |
|        - | 14231 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 14232 | ` *   emits a warning  which allows the script to continue.` |
|        - | 14233 | ` */` |
|        6 | 14234 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14235 |  |
|        - | 14236 | `	SyString sFile;` |
|        - | 14237 | `	sxi32 rc;` |
|        8 | 14238 | `	if( nArg < 1 ){` |
|        - | 14239 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14240 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14241 | `		return SXRET_OK;` |
|        - | 14242 | `	}` |
|        - | 14243 | `	/* File to include */` |
|        8 | 14244 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 14245 | `	if( sFile.nByte < 1 ){` |
|        - | 14246 | `		/* Empty string,return NULL */` |
|      ! 0 | 14247 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14248 | `		return SXRET_OK;` |
|        - | 14249 | `	}` |
|        - | 14250 | `	/* Open,compile and execute the desired script */` |
|        8 | 14251 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 14252 | `	if( rc != SXRET_OK ){` |
|        - | 14253 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 14254 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14255 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14256 | `		return PH7_ABORT;` |
|        - | 14257 | `	}` |
|        8 | 14258 | `	return SXRET_OK;` |
|        5 | 14259 |  |
|        - | 14260 | `/*` |
|        - | 14261 | ` * require_once:` |
|        - | 14262 | ` *  According to the PHP reference manual.` |
|        - | 14263 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 14264 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 14265 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 14266 | ` *   and how it differs from its non _once siblings.` |
|        - | 14267 | ` */` |
|        4 | 14268 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14269 |  |
|        - | 14270 | `	SyString sFile;` |
|        - | 14271 | `	sxi32 rc;` |
|        5 | 14272 | `	if( nArg < 1 ){` |
|        - | 14273 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14274 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14275 | `		return SXRET_OK;` |
|        - | 14276 | `	}` |
|        - | 14277 | `	/* File to include */` |
|        5 | 14278 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 14279 | `	if( sFile.nByte < 1 ){` |
|        - | 14280 | `		/* Empty string,return NULL */` |
|      ! 0 | 14281 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14282 | `		return SXRET_OK;` |
|        - | 14283 | `	}` |
|        - | 14284 | `	/* Open,compile and execute the desired script */` |
|        5 | 14285 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 14286 | `	if( rc == SXERR_EXISTS ){` |
|        - | 14287 | `		/* File already included,return TRUE */` |
|        3 | 14288 | `		ph7_result_bool(pCtx,1);` |
|        3 | 14289 | `		return SXRET_OK;` |
|        - | 14290 | `	}` |
|        3 | 14291 | `	if( rc != SXRET_OK ){` |
|        - | 14292 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 14293 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14294 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14295 | `		return PH7_ABORT;` |
|        - | 14296 | `	}` |
|        3 | 14297 | `	return SXRET_OK;` |
|        3 | 14298 |  |
|        - | 14299 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 14300 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 14301 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 14302 | `/*` |
|        - | 14303 | ` * Section:` |
|        - | 14304 | ` *  SPL Autoloading functions.` |
|        - | 14305 | ` * Status:` |
|        - | 14306 | ` *  Stable.` |
|        - | 14307 | ` */` |
|        - | 14308 | `/*` |
|        - | 14309 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 14310 | ` *  Register given function as __autoload() implementation.` |
|        - | 14311 | ` * Parameters` |
|        - | 14312 | ` *  callback` |
|        - | 14313 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 14314 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 14315 | ` *  throw` |
|        - | 14316 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 14317 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 14318 | ` *  prepend` |
|        - | 14319 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 14320 | ` *   autoload stack instead of appending it.` |
|        - | 14321 | ` * Return` |
|        - | 14322 | ` *  TRUE on success, FALSE on failure.` |
|        - | 14323 | ` */` |
|       34 | 14324 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14325 |  |
|        - | 14326 | `	VmAutoloadCB sEntry;` |
|       36 | 14327 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 14328 | `	int iPrepend = 0;` |
|        - | 14329 | `	sxu32 n;` |
|       36 | 14330 | `	if( nArg < 1 ){` |
|        - | 14331 | `		/* No callback provided — register default spl_autoload.` |
|        - | 14332 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 14333 | `		/* Check for duplicates first */` |
|        9 | 14334 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 14335 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 14336 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 14337 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 14338 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 14339 | `				ph7_result_bool(pCtx,1);` |
|        5 | 14340 | `				return SXRET_OK;` |
|        - | 14341 | `			}` |
|      ! 0 | 14342 | `		}` |
|        5 | 14343 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 14344 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 14345 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 14346 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 14347 | `		ph7_result_bool(pCtx,1);` |
|        5 | 14348 | `		return SXRET_OK;` |
|        - | 14349 | `	}` |
|        - | 14350 | `	/* Validate that the callback is callable */` |
|       28 | 14351 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 14352 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 14353 | `		if( nArg >= 2 ){` |
|      ! 0 | 14354 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 14355 | `		}` |
|      ! 0 | 14356 | `		if( iThrow ){` |
|      ! 0 | 14357 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 14358 | `				"Argument is not callable");` |
|      ! 0 | 14359 | `		}` |
|      ! 0 | 14360 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14361 | `		return SXRET_OK;` |
|        - | 14362 | `	}` |
|        - | 14363 | `	/* Check for duplicates */` |
|       46 | 14364 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 14365 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 14366 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 14367 | `			/* Already registered */` |
|      ! 0 | 14368 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 14369 | `			return SXRET_OK;` |
|        - | 14370 | `		}` |
|       11 | 14371 | `	}` |
|        - | 14372 | `	/* Check prepend flag */` |
|       28 | 14373 | `	if( nArg >= 3 ){` |
|        3 | 14374 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 14375 | `	}` |
|        - | 14376 | `	/* Store the callback */` |
|       28 | 14377 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 14378 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 14379 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 14380 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 14381 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 14382 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 14383 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 14384 | `		VmAutoloadCB *aBase;` |
|        3 | 14385 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 14386 | `		/* Rotate: move last entry to front */` |
|        3 | 14387 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 14388 | `		if( aBase ){` |
|        - | 14389 | `			VmAutoloadCB sTemp;` |
|        - | 14390 | `			sxu32 i;` |
|        3 | 14391 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 14392 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 14393 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 14394 | `			}` |
|        3 | 14395 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 14396 | `		}` |
|        2 | 14397 | `	}else{` |
|       26 | 14398 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 14399 | `	}` |
|       28 | 14400 | `	ph7_result_bool(pCtx,1);` |
|       28 | 14401 | `	return SXRET_OK;` |
|       19 | 14402 |  |
|        - | 14403 | `/*` |
|        - | 14404 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 14405 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 14406 | ` * Parameters` |
|        - | 14407 | ` *  callback` |
|        - | 14408 | ` *   The autoload function being unregistered.` |
|        - | 14409 | ` * Return` |
|        - | 14410 | ` *  TRUE on success, FALSE on failure.` |
|        - | 14411 | ` */` |
|       32 | 14412 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14413 |  |
|       34 | 14414 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14415 | `	sxu32 n,nEntry;` |
|       34 | 14416 | `	if( nArg < 1 ){` |
|      ! 0 | 14417 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14418 | `		return SXRET_OK;` |
|        - | 14419 | `	}` |
|       34 | 14420 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 14421 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 14422 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 14423 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 14424 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 14425 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 14426 | `			sxu32 i;` |
|       32 | 14427 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 14428 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 14429 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 14430 | `			}` |
|        - | 14431 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 14432 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 14433 | `			ph7_result_bool(pCtx,1);` |
|       32 | 14434 | `			return SXRET_OK;` |
|        - | 14435 | `		}` |
|        3 | 14436 | `	}` |
|        3 | 14437 | `	ph7_result_bool(pCtx,0);` |
|        3 | 14438 | `	return SXRET_OK;` |
|       18 | 14439 |  |
|        - | 14440 | `/*` |
|        - | 14441 | ` * array spl_autoload_functions(void)` |
|        - | 14442 | ` *  Return all registered __autoload() functions.` |
|        - | 14443 | ` * Return` |
|        - | 14444 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 14445 | ` *  an empty array is returned.` |
|        - | 14446 | ` */` |
|       20 | 14447 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14448 |  |
|       21 | 14449 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14450 | `	ph7_value *pArray;` |
|        - | 14451 | `	sxu32 n,nEntry;` |
|       10 | 14452 | `	SXUNUSED(nArg);` |
|       10 | 14453 | `	SXUNUSED(apArg);` |
|       21 | 14454 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 14455 | `	if( pArray == 0 ){` |
|      ! 0 | 14456 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14457 | `		return SXRET_OK;` |
|        - | 14458 | `	}` |
|       21 | 14459 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 14460 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 14461 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 14462 | `		if( pEntry ){` |
|       15 | 14463 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 14464 | `		}` |
|        8 | 14465 | `	}` |
|       21 | 14466 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 14467 | `	return SXRET_OK;` |
|       11 | 14468 |  |
|        - | 14469 | `/*` |
|        - | 14470 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 14471 | ` *  Default implementation of __autoload().` |
|        - | 14472 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 14473 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 14474 | ` * Parameters` |
|        - | 14475 | ` *  class` |
|        - | 14476 | ` *   The class name being searched.` |
|        - | 14477 | ` *  file_extensions` |
|        - | 14478 | ` *   Comma-separated list of file extensions to try.` |
|        - | 14479 | ` */` |
|        2 | 14480 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14481 |  |
|        - | 14482 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 14483 | `	SyBlob sPath;` |
|        - | 14484 | `	int nClass;` |
|        - | 14485 | `	sxi32 rc;` |
|        3 | 14486 | `	if( nArg < 1 ){` |
|      ! 0 | 14487 | `		return SXRET_OK;` |
|        - | 14488 | `	}` |
|        3 | 14489 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 14490 | `	if( nClass < 1 ){` |
|      ! 0 | 14491 | `		return SXRET_OK;` |
|        - | 14492 | `	}` |
|        - | 14493 | `	/* Default extensions */` |
|        3 | 14494 | `	zExt = ".php,.inc";` |
|        3 | 14495 | `	if( nArg >= 2 ){` |
|        - | 14496 | `		int nExt;` |
|      ! 0 | 14497 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 14498 | `		if( nExt < 1 ){` |
|      ! 0 | 14499 | `			zExt = ".php,.inc";` |
|      ! 0 | 14500 | `		}` |
|      ! 0 | 14501 | `	}` |
|        3 | 14502 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 14503 | `	/* Iterate over comma-separated extensions */` |
|        3 | 14504 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 14505 | `	zCur = zExt;` |
|        7 | 14506 | `	while( zCur < zEnd ){` |
|        - | 14507 | `		const char *zComma;` |
|        - | 14508 | `		SyString sFile;` |
|        - | 14509 | `		int i;` |
|        - | 14510 | `		/* Find next comma or end */` |
|        5 | 14511 | `		zComma = zCur;` |
|       21 | 14512 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 14513 | `			zComma++;` |
|        1 | 14514 | `		}` |
|        - | 14515 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 14516 | `		SyBlobReset(&sPath);` |
|       69 | 14517 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 14518 | `			char c = zClass[i];` |
|       65 | 14519 | `			if( c == '\\' ){` |
|      ! 0 | 14520 | `				c = '/';` |
|       65 | 14521 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 14522 | `				c = c + ('a' - 'A');` |
|        6 | 14523 | `			}` |
|       65 | 14524 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 14525 | `		}` |
|        - | 14526 | `		/* Append extension */` |
|        5 | 14527 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 14528 | `		/* Try to include the file */` |
|        5 | 14529 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 14530 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 14531 | `		if( rc == SXRET_OK ){` |
|        - | 14532 | `			/* File included successfully */` |
|      ! 0 | 14533 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 14534 | `			return SXRET_OK;` |
|        - | 14535 | `		}` |
|        - | 14536 | `		/* Move past the comma */` |
|        5 | 14537 | `		zCur = zComma;` |
|        5 | 14538 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 14539 | `			zCur++;` |
|        1 | 14540 | `		}` |
|        1 | 14541 | `	}` |
|        3 | 14542 | `	SyBlobRelease(&sPath);` |
|        3 | 14543 | `	return SXRET_OK;` |
|        2 | 14544 |  |
|        - | 14545 | `/* Table of built-in VM functions. */` |
|        - | 14546 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 14547 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 14548 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 14549 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 14550 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 14551 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 14552 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 14553 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 14554 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 14555 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 14556 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 14557 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 14558 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 14559 | `	    /* Constants management */` |
|        - | 14560 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 14561 | `	{ "define",   vm_builtin_define               },` |
|        - | 14562 | `	{ "constant", vm_builtin_constant             },` |
|        - | 14563 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 14564 | `	   /* Class/Object functions */` |
|        - | 14565 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 14566 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 14567 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 14568 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 14569 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 14570 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 14571 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 14572 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 14573 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 14574 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 14575 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 14576 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 14577 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 14578 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 14579 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 14580 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 14581 | `	   /* SPL Autoloading */` |
|        - | 14582 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 14583 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 14584 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 14585 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 14586 | `	   /* Random numbers/strings generators */` |
|        - | 14587 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 14588 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 14589 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 14590 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 14591 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 14592 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 14593 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 14594 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14595 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 14596 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 14597 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 14598 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 14599 | `	   /* Language constructs functions */` |
|        - | 14600 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 14601 | `	{ "print", vm_builtin_print                   },` |
|        - | 14602 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 14603 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 14604 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 14605 | `	  /* Variable handling functions */` |
|        - | 14606 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 14607 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 14608 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 14609 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 14610 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 14611 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 14612 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 14613 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 14614 | `	  /* Ouput control functions */` |
|        - | 14615 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 14616 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 14617 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 14618 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 14619 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 14620 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 14621 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 14622 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 14623 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 14624 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 14625 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 14626 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 14627 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 14628 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 14629 | `	  /* Assertion functions */` |
|        - | 14630 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 14631 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 14632 | `	  /* Error reporting functions */` |
|        - | 14633 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 14634 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 14635 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 14636 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 14637 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 14638 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 14639 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 14640 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 14641 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 14642 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 14643 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 14644 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 14645 | `	  /* Release info */` |
|        - | 14646 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 14647 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 14648 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 14649 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 14650 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 14651 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 14652 | `	  /* hashmap */` |
|        - | 14653 | `	{"compact",          vm_builtin_compact       },` |
|        - | 14654 | `	{"extract",          vm_builtin_extract       },` |
|        - | 14655 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 14656 | `	  /* URL related function */` |
|        - | 14657 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 14658 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 14659 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14660 | `	   /* XML processing functions */` |
|        - | 14661 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 14662 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 14663 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 14664 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 14665 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 14666 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 14667 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 14668 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 14669 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 14670 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 14671 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 14672 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 14673 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 14674 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 14675 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 14676 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 14677 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 14678 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 14679 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 14680 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 14681 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 14682 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 14683 | `	   /* UTF-8 encoding/decoding */` |
|        - | 14684 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 14685 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 14686 | `	   /* Command line processing */` |
|        - | 14687 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 14688 | `	   /* JSON encoding/decoding */` |
|        - | 14689 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 14690 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 14691 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 14692 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 14693 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 14694 | `	   /* Files/URI inclusion facility */` |
|        - | 14695 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 14696 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 14697 | `	{ "include",      vm_builtin_include          },` |
|        - | 14698 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 14699 | `	{ "require",      vm_builtin_require          },` |
|        - | 14700 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 14701 | `};` |
|        - | 14702 | `/*` |
|        - | 14703 | ` * Register the built-in VM functions defined above.` |
|        - | 14704 | ` */` |
|     2678 | 14705 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 14706 |  |
|        - | 14707 | `	sxi32 rc;` |
|        - | 14708 | `	sxu32 n;` |
|   350820 | 14709 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 14710 | `		/* Note that these special functions have access` |
|        - | 14711 | `		 * to the underlying virtual machine as their` |
|        - | 14712 | `		 * private data.` |
|        - | 14713 | `		 */` |
|   348142 | 14714 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   348142 | 14715 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 14716 | `			return rc;` |
|        - | 14717 | `		}` |
|   174072 | 14718 | `	}` |
|     2680 | 14719 | `	return SXRET_OK;` |
|     1341 | 14720 |  |
|        - | 14721 | `/*` |
|        - | 14722 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 14723 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 14724 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 14725 | ` */` |
|    41730 | 14726 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 14727 |  |
|    41732 | 14728 | `	if( !iLoadable ){` |
|    39866 | 14729 | `		return pClass;` |
|        - | 14730 | `	}` |
|     1872 | 14731 | `	while(pClass){` |
|     1868 | 14732 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1864 | 14733 | `			return pClass;` |
|        - | 14734 | `		}` |
|        5 | 14735 | `		pClass = pClass->pNextName;` |
|        1 | 14736 | `	}` |
|        5 | 14737 | `	return 0;` |
|    20867 | 14738 |  |
|        - | 14739 | `/*` |
|        - | 14740 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 14741 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 14742 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 14743 | ` * registered in the VM's class table.` |
|        - | 14744 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 14745 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 14746 | ` */` |
|       38 | 14747 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 14748 |  |
|        - | 14749 | `	VmAutoloadCB *pEntry;` |
|        - | 14750 | `	ph7_value sArg,sResult;` |
|        - | 14751 | `	SyHashEntry *pHashEntry;` |
|        - | 14752 | `	ph7_class *pClass;` |
|        - | 14753 | `	sxu32 n,nEntry;` |
|       40 | 14754 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       40 | 14755 | `	if( nEntry < 1 ){` |
|       26 | 14756 | `		return 0;` |
|        - | 14757 | `	}` |
|        - | 14758 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 14759 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 14760 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 14761 | `	}` |
|        - | 14762 | `	/* Mark this class as being autoloaded */` |
|       14 | 14763 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 14764 | `	/* Prepare the class name argument */` |
|       14 | 14765 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 14766 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 14767 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 14768 | `	pClass = 0;` |
|       28 | 14769 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 14770 | `		ph7_value *apArg[1];` |
|       24 | 14771 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 14772 | `		if( pEntry == 0 ){` |
|      ! 0 | 14773 | `			continue;` |
|        - | 14774 | `		}` |
|       24 | 14775 | `		apArg[0] = &sArg;` |
|       24 | 14776 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 14777 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 14778 | `			continue;` |
|        - | 14779 | `		}` |
|        - | 14780 | `		/* Check if the class is now available */` |
|       24 | 14781 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 14782 | `		if( pHashEntry ){` |
|       10 | 14783 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 14784 | `			if( pClass ){` |
|       10 | 14785 | `				break;` |
|        - | 14786 | `			}` |
|      ! 0 | 14787 | `		}` |
|        9 | 14788 | `	}` |
|       14 | 14789 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 14790 | `	PH7_MemObjRelease(&sResult);` |
|        - | 14791 | `	/* Remove reentrancy guard */` |
|       14 | 14792 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 14793 | `	return pClass;` |
|       21 | 14794 |  |
|        - | 14795 | `/*` |
|        - | 14796 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 14797 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 14798 | ` */` |
|       18 | 14799 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 14800 |  |
|       20 | 14801 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 14802 |  |
|        - | 14803 | `/*` |
|        - | 14804 | ` * Check if the given name refer to an installed class.` |
|        - | 14805 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 14806 | ` */` |
|    41742 | 14807 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 14808 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 14809 | `	const char *zName,  /* Name of the target class */` |
|        - | 14810 | `	sxu32 nByte,        /* zName length */` |
|        - | 14811 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 14812 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 14813 | `						 */` |
|        - | 14814 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 14815 | `	)` |
|        2 | 14816 |  |
|        - | 14817 | `	SyHashEntry *pEntry;` |
|        - | 14818 | `	ph7_class *pClass;` |
|    20871 | 14819 | `	SXUNUSED(iNest);` |
|        - | 14820 | `	/* Exact class lookup.` |
|        - | 14821 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 14822 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    41744 | 14823 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    41744 | 14824 | `	if( pEntry == 0 ){` |
|        - | 14825 | `		/* Class not found in hash table — try autoload before giving up */` |
|       22 | 14826 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 14827 | `	}` |
|    41724 | 14828 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    41724 | 14829 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    20873 | 14830 |  |
|        - | 14831 | `/*` |
|        - | 14832 | ` * Reference Table Implementation` |
|        - | 14833 | ` * Status: stable <chm@symisc.net>` |
|        - | 14834 | ` * Intro` |
|        - | 14835 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 14836 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 14837 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 14838 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 14839 | ` *  Refer to the official for more information on this powerful` |
|        - | 14840 | ` *  extension.` |
|        - | 14841 | ` */` |
|        - | 14842 | `/*` |
|        - | 14843 | ` * Allocate a new reference entry.` |
|        - | 14844 | ` */` |
|  3158136 | 14845 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 14846 |  |
|        - | 14847 | `	VmRefObj *pRef;` |
|        - | 14848 | `	/* Allocate a new instance */` |
|  3158138 | 14849 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3158138 | 14850 | `	if( pRef == 0 ){` |
|      ! 0 | 14851 | `		return 0;` |
|        - | 14852 | `	}` |
|        - | 14853 | `	/* Zero the structure */` |
|  3158138 | 14854 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 14855 | `	/* Initialize fields */` |
|  3158138 | 14856 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3158138 | 14857 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3158138 | 14858 | `	pRef->nIdx = nIdx;` |
|  3158138 | 14859 | `	return pRef;` |
|  1579070 | 14860 |  |
|        - | 14861 | `/*` |
|        - | 14862 | ` * Default hash function used by the reference table` |
|        - | 14863 | ` * for lookup/insertion operations.` |
|        - | 14864 | ` */` |
| 17344491 | 14865 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 14866 |  |
|        - | 14867 | `	/* Calculate the hash based on the memory object index */` |
| 17344493 | 14868 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 14869 |  |
|        - | 14870 | `/*` |
|        - | 14871 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 14872 | ` * in the reference table.` |
|        - | 14873 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 14874 | ` * otherwise.` |
|        - | 14875 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14876 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14877 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14878 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14879 | ` * Refer to the official for more information on this powerful` |
|        - | 14880 | ` * extension.` |
|        - | 14881 | ` */` |
|  9418110 | 14882 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 14883 |  |
|        - | 14884 | `	VmRefObj *pRef;` |
|        - | 14885 | `	sxu32 nBucket;` |
|        - | 14886 | `	/* Point to the appropriate bucket */` |
|  9418112 | 14887 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 14888 | `	/* Perform the lookup */` |
|  9418112 | 14889 | `	pRef = pVm->apRefObj[nBucket];` |
| 20573676 | 14890 | `	for(;;){` |
| 41140876 | 14891 | `		if( pRef == 0 ){` |
|  3258786 | 14892 | `			break;` |
|        - | 14893 | `		}` |
| 37882092 | 14894 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 14895 | `			/* Entry found */` |
|  6159328 | 14896 | `			return pRef;` |
|        - | 14897 | `		}` |
|        - | 14898 | `		/* Point to the next entry */` |
| 31722766 | 14899 | `		pRef = pRef->pNextCollide;` |
|        2 | 14900 | `	}` |
|        - | 14901 | `	/* No such entry,return NULL */` |
|  3258786 | 14902 | `	return 0;` |
|  4709057 | 14903 |  |
|        - | 14904 | `/*` |
|        - | 14905 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14906 | ` *` |
|        - | 14907 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14908 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14909 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14910 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14911 | ` * Refer to the official for more information on this powerful` |
|        - | 14912 | ` * extension.` |
|        - | 14913 | ` */` |
|  3158136 | 14914 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14915 |  |
|        - | 14916 | `	sxu32 nBucket;` |
|  3158138 | 14917 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 14918 | `		VmRefObj **apNew;` |
|        - | 14919 | `		sxu32 nNew;` |
|        - | 14920 | `		/* Allocate a larger table */` |
|     4572 | 14921 | `		nNew = pVm->nRefSize << 1;` |
|     4572 | 14922 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4572 | 14923 | `		if( apNew ){` |
|     4572 | 14924 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 14925 | `			sxu32 n;` |
|        - | 14926 | `			/* Zero the structure */` |
|     4572 | 14927 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 14928 | `			/* Rehash all referenced entries */` |
|  2846954 | 14929 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 14930 | `				/* Remove old collision links */` |
|  2842384 | 14931 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 14932 | `				/* Point to the appropriate bucket */` |
|  2842384 | 14933 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 14934 | `				/* Insert the entry  */` |
|  2842384 | 14935 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2842384 | 14936 | `				if( apNew[nBucket] ){` |
|  2298896 | 14937 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 14938 | `				}` |
|  2842384 | 14939 | `				apNew[nBucket] = pEntry;` |
|        - | 14940 | `				/* Point to the next entry */` |
|  2842384 | 14941 | `				pEntry = pEntry->pNext;` |
|  1421193 | 14942 | `			}` |
|        - | 14943 | `			/* Release the old table */` |
|     4572 | 14944 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 14945 | `			/* Install the new one */` |
|     4572 | 14946 | `			pVm->apRefObj = apNew;` |
|     4572 | 14947 | `			pVm->nRefSize = nNew;` |
|     2285 | 14948 | `		}` |
|     2285 | 14949 | `	}` |
|        - | 14950 | `	/* Point to the appropriate bucket */` |
|  3158138 | 14951 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 14952 | `	/* Insert the entry */` |
|  3158138 | 14953 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3158138 | 14954 | `	if( pVm->apRefObj[nBucket] ){` |
|  2582823 | 14955 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1291422 | 14956 | `	}` |
|  3158138 | 14957 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3158138 | 14958 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3158138 | 14959 | `	pVm->nRefUsed++;` |
|  3158138 | 14960 | `	return SXRET_OK;` |
|        2 | 14961 |  |
|        - | 14962 | `/*` |
|        - | 14963 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 14964 | ` * the reference table.` |
|        - | 14965 | ` * This function is invoked when the user perform an unset` |
|        - | 14966 | ` * call [i.e: unset($var); ].` |
|        - | 14967 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14968 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14969 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14970 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14971 | ` * Refer to the official for more information on this powerful` |
|        - | 14972 | ` * extension.` |
|        - | 14973 | ` */` |
|  3119062 | 14974 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14975 |  |
|        - | 14976 | `	ph7_hashmap_node **apNode;` |
|        - | 14977 | `	SyHashEntry **apEntry;` |
|        - | 14978 | `	sxu32 n;` |
|        - | 14979 | `	/* Point to the reference table */` |
|  3119064 | 14980 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3119064 | 14981 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 14982 | `	/* Unlink the entry from the reference table */` |
|  3226228 | 14983 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   107166 | 14984 | `		if( apEntry[n] ){` |
|   107116 | 14985 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    53557 | 14986 | `		}` |
|    53584 | 14987 | `	}` |
|  6132200 | 14988 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3013138 | 14989 | `		if( apNode[n] ){` |
|     7444 | 14990 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3721 | 14991 | `		}` |
|  1506570 | 14992 | `	}` |
|  3119064 | 14993 | `	if( pRef->pPrevCollide ){` |
|  1193201 | 14994 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   596690 | 14995 | `	}else{` |
|  1925865 | 14996 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 14997 | `	}` |
|  3119064 | 14998 | `	if( pRef->pNextCollide ){` |
|  1768504 | 14999 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   884176 | 15000 | `	}` |
|  3119064 | 15001 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 15002 | `	/* Release the node */` |
|  3119064 | 15003 | `	SySetRelease(&pRef->aReference);` |
|  3119064 | 15004 | `	SySetRelease(&pRef->aArrEntries);` |
|  3119064 | 15005 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3119064 | 15006 | `	pVm->nRefUsed--;` |
|  3119064 | 15007 | `	return SXRET_OK;` |
|        2 | 15008 |  |
|        - | 15009 | `/*` |
|        - | 15010 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 15011 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15012 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15013 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15014 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15015 | ` * Refer to the official for more information on this powerful` |
|        - | 15016 | ` * extension.` |
|        - | 15017 | ` */` |
|  3192640 | 15018 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 15019 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 15020 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 15021 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 15022 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 15023 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 15024 | `	)` |
|        2 | 15025 |  |
|  3192642 | 15026 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 15027 | `	VmRefObj *pRef;` |
|        - | 15028 | `	/* Check if the referenced object already exists */` |
|  3192642 | 15029 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3192642 | 15030 | `	if( pRef == 0 ){` |
|        - | 15031 | `		/* Create a new entry */` |
|  3158138 | 15032 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3158138 | 15033 | `		if( pRef == 0 ){` |
|      ! 0 | 15034 | `			return SXERR_MEM;` |
|        - | 15035 | `		}` |
|  3158138 | 15036 | `		pRef->iFlags = iFlags;` |
|        - | 15037 | `		/* Install the entry */` |
|  3158138 | 15038 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1579068 | 15039 | `	}` |
|  3192642 | 15040 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3192642 | 15041 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 15042 | `		VmSlot sRef;` |
|        - | 15043 | `		/* Local frame,record referenced entry so that it can` |
|        - | 15044 | `		 * be deleted when we leave this frame.` |
|        - | 15045 | `		 */` |
|   100746 | 15046 | `		sRef.nIdx = nIdx;` |
|   100746 | 15047 | `		sRef.pUserData = pEntry;` |
|   100746 | 15048 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 15049 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 15050 | `		}` |
|    50372 | 15051 | `	}` |
|  3192642 | 15052 | `	if( pEntry ){` |
|        - | 15053 | `		/* Address of the hash-entry */` |
|   135050 | 15054 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    67524 | 15055 | `	}` |
|  3192642 | 15056 | `	if( pMapEntry ){` |
|        - | 15057 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3049892 | 15058 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1524945 | 15059 | `	}` |
|  3192642 | 15060 | `	return SXRET_OK;` |
|  1596322 | 15061 |  |
|        - | 15062 | `/*` |
|        - | 15063 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 15064 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15065 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15066 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15067 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15068 | ` * Refer to the official for more information on this powerful` |
|        - | 15069 | ` * extension.` |
|        - | 15070 | ` */` |
|  3106402 | 15071 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 15072 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 15073 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 15074 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 15075 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 15076 | `	)` |
|        2 | 15077 |  |
|        - | 15078 | `	VmRefObj *pRef;` |
|        - | 15079 | `	sxu32 n;` |
|        - | 15080 | `	/* Check if the referenced object already exists */` |
|  3106404 | 15081 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3106404 | 15082 | `	if( pRef == 0 ){` |
|        - | 15083 | `		/* Not such entry */` |
|   100644 | 15084 | `		return SXERR_NOTFOUND;` |
|        - | 15085 | `	}` |
|        - | 15086 | `	/* Remove the desired entry */` |
|  3005762 | 15087 | `	if( pEntry ){` |
|        - | 15088 | `		SyHashEntry **apEntry;` |
|       62 | 15089 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      228 | 15090 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      168 | 15091 | `			if( apEntry[n] == pEntry ){` |
|        - | 15092 | `				/* Nullify the entry */` |
|       62 | 15093 | `				apEntry[n] = 0;` |
|        - | 15094 | `				/*` |
|        - | 15095 | `				 * NOTE:` |
|        - | 15096 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 15097 | `				 * we avoid wasting spaces.` |
|        - | 15098 | `				 */` |
|       30 | 15099 | `			}` |
|       85 | 15100 | `		}` |
|       30 | 15101 | `	}` |
|  3005762 | 15102 | `	if( pMapEntry ){` |
|        - | 15103 | `		ph7_hashmap_node **apNode;` |
|  3005702 | 15104 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  6011496 | 15105 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3005796 | 15106 | `			if( apNode[n] == pMapEntry ){` |
|        - | 15107 | `				/* nullify the entry */` |
|  3005702 | 15108 | `				apNode[n] = 0;` |
|  1502850 | 15109 | `			}` |
|  1502899 | 15110 | `		}` |
|  1502850 | 15111 | `	}` |
|  3005762 | 15112 | `	return SXRET_OK;` |
|  1553203 | 15113 |  |
|        - | 15114 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 15115 | `/*` |
|        - | 15116 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 15117 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 15118 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 15119 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 15120 | ` * For more information on how to register IO stream devices,please` |
|        - | 15121 | ` * refer to the official documentation.` |
|        - | 15122 | ` */` |
|    28118 | 15123 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 15124 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 15125 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 15126 | `	int nByte              /* *pzDevice length*/` |
|        - | 15127 | `	)` |
|        2 | 15128 |  |
|        - | 15129 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 15130 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 15131 | `	SyString sDev,sCur;` |
|        - | 15132 | `	sxu32 n,nEntry;` |
|        - | 15133 | `	int rc;` |
|        - | 15134 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    28120 | 15135 | `	zNext = zCur = zIn = *pzDevice;` |
|    28120 | 15136 | `	zEnd = &zIn[nByte];` |
|  1793824 | 15137 | `	while( zIn < zEnd ){` |
|  1765708 | 15138 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 15139 | `			/* Got one */` |
|        3 | 15140 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 15141 | `			break;` |
|        - | 15142 | `		}` |
|        - | 15143 | `		/* Advance the cursor */` |
|  1765706 | 15144 | `		zIn++;` |
|        2 | 15145 | `	}` |
|    28120 | 15146 | `	if( zIn >= zEnd ){` |
|        - | 15147 | `		/* No such scheme,return the default stream */` |
|    28118 | 15148 | `		return pVm->pDefStream;` |
|        - | 15149 | `	}` |
|        3 | 15150 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 15151 | `	/* Remove leading and trailing white spaces */` |
|        3 | 15152 | `	SyStringFullTrim(&sDev);` |
|        - | 15153 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 15154 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 15155 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 15156 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 15157 | `		pStream = apStream[n];` |
|        3 | 15158 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 15159 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 15160 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 15161 | `		if( rc == 0 ){` |
|        - | 15162 | `			/* Stream device found */` |
|        3 | 15163 | `			*pzDevice = zNext;` |
|        3 | 15164 | `			return pStream;` |
|        - | 15165 | `		}` |
|      ! 0 | 15166 | `	}` |
|        - | 15167 | `	/* No such stream,return NULL */` |
|      ! 0 | 15168 | `	return 0;` |
|    14061 | 15169 |  |
|        - | 15170 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 15171 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 15172 |  |
