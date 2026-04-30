# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6261/8100 lines (77.30%)

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
|   901052 |   142 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   143 |  |
|   901054 |   144 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |   145 | `		return TRUE;` |
|        - |   146 | `	}` |
|   901020 |   147 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   148 | `		return TRUE;` |
|        - |   149 | `	}` |
|   901010 |   150 | `	return FALSE;` |
|   450550 |   151 |  |
|        - |   152 | `/*` |
|        - |   153 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   154 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   155 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   156 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   157 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   158 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   159 | ` * still go through the existing numeric coercion.` |
|        - |   160 | ` */` |
|   334072 |   161 | `static int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        2 |   162 |  |
|        - |   163 | `	SyString sStr;` |
|   334074 |   164 | `	sxu8 bReal = FALSE;` |
|   334074 |   165 | `	const char *zTail = 0;` |
|        - |   166 | `	const char *zEnd;` |
|   334074 |   167 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   334016 |   168 | `		return FALSE;` |
|        - |   169 | `	}` |
|       59 |   170 | `	SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|       59 |   171 | `	if( sStr.nByte == 0 ){` |
|        5 |   172 | `		return TRUE;` |
|        - |   173 | `	}` |
|       55 |   174 | `	if( SyStrIsNumeric(sStr.zString,sStr.nByte,&bReal,&zTail) != SXRET_OK ){` |
|       43 |   175 | `		return TRUE;` |
|        - |   176 | `	}` |
|        - |   177 | `	/* SyStrIsNumeric accepts a leading numeric prefix; require the` |
|        - |   178 | `	 * remainder to be whitespace only so leading-numeric junk like "5foo"` |
|        - |   179 | `	 * still takes the Perl path. */` |
|       13 |   180 | `	zEnd = sStr.zString + sStr.nByte;` |
|       17 |   181 | `	while( zTail < zEnd && (unsigned char)*zTail < 0xc0 && SyisSpace(*zTail) ){` |
|        5 |   182 | `		zTail++;` |
|        1 |   183 | `	}` |
|       13 |   184 | `	return zTail < zEnd;` |
|   167060 |   185 |  |
|        - |   186 | `/* SyhttpUri, SyhttpHeader and HTTP method/protocol defines moved to ph7int.h */` |
|        - |   187 | `/*` |
|        - |   188 | ` * Register a constant and it's associated expansion callback so that` |
|        - |   189 | ` * it can be expanded from the target PHP program.` |
|        - |   190 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|        - |   191 | ` * simple and work as follows:` |
|        - |   192 | ` * Each registered constant have a C procedure associated with it.` |
|        - |   193 | ` * This procedure known as the constant expansion callback is responsible` |
|        - |   194 | ` * of expanding the invoked constant to the desired value,for example:` |
|        - |   195 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|        - |   196 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|        - |   197 | ` * (Windows,Linux,...) and so on.` |
|        - |   198 | ` * Please refer to the official documentation for additional information.` |
|        - |   199 | ` */` |
|   584190 |   200 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |   201 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   202 | `	const SyString *pName,  /* Constant name */` |
|        - |   203 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   204 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |   205 | `	)` |
|        2 |   206 |  |
|        - |   207 | `	ph7_constant *pCons;` |
|        - |   208 | `	SyHashEntry *pEntry;` |
|        - |   209 | `	char *zDupName;` |
|        - |   210 | `	sxi32 rc;` |
|   584192 |   211 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   584192 |   212 | `	if( pEntry ){` |
|        - |   213 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   214 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   215 | `		pCons->xExpand = xExpand;` |
|        6 |   216 | `		pCons->pUserData = pUserData;` |
|        6 |   217 | `		return SXRET_OK;` |
|        - |   218 | `	}` |
|        - |   219 | `	/* Allocate a new constant instance */` |
|   584188 |   220 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   584188 |   221 | `	if( pCons == 0 ){` |
|      ! 0 |   222 | `		return 0;` |
|        - |   223 | `	}` |
|        - |   224 | `	/* Duplicate constant name */` |
|   584188 |   225 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   584188 |   226 | `	if( zDupName == 0 ){` |
|      ! 0 |   227 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   228 | `		return 0;` |
|        - |   229 | `	}` |
|        - |   230 | `	/* Install the constant */` |
|   584188 |   231 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   584188 |   232 | `	pCons->xExpand = xExpand;` |
|   584188 |   233 | `	pCons->pUserData = pUserData;` |
|   584188 |   234 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   584188 |   235 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   236 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   237 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   238 | `		return rc;` |
|        - |   239 | `	}` |
|        - |   240 | `	/* All done,constant can be invoked from PHP code */` |
|   584188 |   241 | `	return SXRET_OK;` |
|   292097 |   242 |  |
|        - |   243 | `/*` |
|        - |   244 | ` * Allocate a new foreign function instance.` |
|        - |   245 | ` * This function return SXRET_OK on success. Any other` |
|        - |   246 | ` * return value indicates failure.` |
|        - |   247 | ` * Please refer to the official documentation for an introduction to` |
|        - |   248 | ` * the foreign function mechanism.` |
|        - |   249 | ` */` |
|  1298126 |   250 | `static sxi32 PH7_NewForeignFunction(` |
|        - |   251 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   252 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   253 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   254 | `	void *pUserData,          /* Foreign function private data */` |
|        - |   255 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |   256 | `	)` |
|        2 |   257 |  |
|        - |   258 | `	ph7_user_func *pFunc;` |
|        - |   259 | `	char *zDup;` |
|        - |   260 | `	/* Allocate a new user function */` |
|  1298128 |   261 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1298128 |   262 | `	if( pFunc == 0 ){` |
|      ! 0 |   263 | `		return SXERR_MEM;` |
|        - |   264 | `	}` |
|        - |   265 | `	/* Duplicate function name */` |
|  1298128 |   266 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1298128 |   267 | `	if( zDup == 0 ){` |
|      ! 0 |   268 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   269 | `		return SXERR_MEM;` |
|        - |   270 | `	}` |
|        - |   271 | `	/* Zero the structure */` |
|  1298128 |   272 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   273 | `	/* Initialize structure fields */` |
|  1298128 |   274 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1298128 |   275 | `	pFunc->pVm   = pVm;` |
|  1298128 |   276 | `	pFunc->xFunc = xFunc;` |
|  1298128 |   277 | `	pFunc->pUserData = pUserData;` |
|  1298128 |   278 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   279 | `	/* Write a pointer to the new function */` |
|  1298128 |   280 | `	*ppOut = pFunc;` |
|  1298128 |   281 | `	return SXRET_OK;` |
|   649065 |   282 |  |
|        - |   283 | `/*` |
|        - |   284 | ` * Install a foreign function and it's associated callback so that` |
|        - |   285 | ` * it can be invoked from the target PHP code.` |
|        - |   286 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   287 | ` * return value indicates failure.` |
|        - |   288 | ` * Please refer to the official documentation for an introduction to` |
|        - |   289 | ` * the foreign function mechanism.` |
|        - |   290 | ` */` |
|  1300818 |   291 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |   292 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   293 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   294 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   295 | `	void *pUserData           /* Foreign function private data */` |
|        - |   296 | `	)` |
|        2 |   297 |  |
|        - |   298 | `	ph7_user_func *pFunc;` |
|        - |   299 | `	SyHashEntry *pEntry;` |
|        - |   300 | `	sxi32 rc;` |
|        - |   301 | `	/* Overwrite any previously registered function with the same name */` |
|  1300820 |   302 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1300820 |   303 | `	if( pEntry ){` |
|     2694 |   304 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2694 |   305 | `		pFunc->pUserData = pUserData;` |
|     2694 |   306 | `		pFunc->xFunc = xFunc;` |
|     2694 |   307 | `		SySetReset(&pFunc->aAux);` |
|     2694 |   308 | `		return SXRET_OK;` |
|        - |   309 | `	}` |
|        - |   310 | `	/* Create a new user function */` |
|  1298128 |   311 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1298128 |   312 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   313 | `		return rc;` |
|        - |   314 | `	}` |
|        - |   315 | `	/* Install the function in the corresponding hashtable */` |
|  1298128 |   316 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1298128 |   317 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   318 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   319 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   320 | `		return rc;` |
|        - |   321 | `	}` |
|        - |   322 | `	/* User function successfully installed */` |
|  1298128 |   323 | `	return SXRET_OK;` |
|   650411 |   324 |  |
|        - |   325 | `/*` |
|        - |   326 | ` * Initialize a VM function.` |
|        - |   327 | ` */` |
|   236064 |   328 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   329 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   330 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   331 | `	const char *zName,  /* Function name */` |
|        - |   332 | `	sxu32 nByte,        /* zName length */` |
|        - |   333 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   334 | `	void *pUserData     /* Function private data */` |
|        - |   335 | `	)` |
|        2 |   336 |  |
|        - |   337 | `	/* Zero the structure */` |
|   236066 |   338 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   339 | `	/* Initialize structure fields */` |
|        - |   340 | `	/* Arguments container */` |
|   236066 |   341 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   342 | `	/* Static variable container */` |
|   236066 |   343 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   344 | `	/* Bytecode container */` |
|   236066 |   345 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   346 | `    /* Preallocate some instruction slots */` |
|   236066 |   347 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   348 | `	/* Closure environment */` |
|   236066 |   349 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   350 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   236066 |   351 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   236066 |   352 | `	pFunc->iFlags = iFlags;` |
|   236066 |   353 | `	pFunc->pUserData = pUserData;` |
|        - |   354 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |   355 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   236066 |   356 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   236066 |   357 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   236066 |   358 | `	return SXRET_OK;` |
|        2 |   359 |  |
|        - |   360 | `/*` |
|        - |   361 | ` * Namespace-aware function lookup.` |
|        - |   362 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   363 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   364 | ` */` |
|        - |   365 | `/*` |
|        - |   366 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   367 | ` */` |
|   724884 |   368 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   369 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   370 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   371 | `	SyString *pName     /* Function name */` |
|        - |   372 | `	)` |
|        2 |   373 |  |
|        - |   374 | `	SyHashEntry *pEntry;` |
|        - |   375 | `	sxi32 rc;` |
|   724886 |   376 | `	if( pName == 0 ){` |
|        - |   377 | `		/* Use the built-in name */` |
|    40046 |   378 | `		pName = &pFunc->sName;` |
|    20022 |   379 | `	}` |
|        - |   380 | `	/* Check for duplicates (functions with the same name) first */` |
|   724886 |   381 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   724886 |   382 | `	if( pEntry ){` |
|   537206 |   383 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   537206 |   384 | `		if( pLink != pFunc ){` |
|        - |   385 | `			/* Link */` |
|      188 |   386 | `			pFunc->pNextName = pLink;` |
|      188 |   387 | `			pEntry->pUserData = pFunc;` |
|       93 |   388 | `		}` |
|   537206 |   389 | `		return SXRET_OK;` |
|        - |   390 | `	}` |
|        - |   391 | `	/* First time seen */` |
|   187682 |   392 | `	pFunc->pNextName = 0;` |
|   187682 |   393 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   187682 |   394 | `	return rc;` |
|   362444 |   395 |  |
|        - |   396 | `/*` |
|        - |   397 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   398 | ` */` |
|    55010 |   399 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   400 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   401 | `	ph7_class *pClass /* Target Class */` |
|        - |   402 | `	)` |
|        2 |   403 |  |
|    55012 |   404 | `	SyString *pName = &pClass->sName;` |
|        - |   405 | `	SyHashEntry *pEntry;` |
|        - |   406 | `	sxi32 rc;` |
|        - |   407 | `	/* Check for duplicates */` |
|    55012 |   408 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    55012 |   409 | `	if( pEntry ){` |
|       31 |   410 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   411 | `		/* Link entry with the same name */` |
|       31 |   412 | `		pClass->pNextName = pLink;` |
|       31 |   413 | `		pEntry->pUserData = pClass;` |
|       31 |   414 | `		return SXRET_OK;` |
|        - |   415 | `	}` |
|    54982 |   416 | `	pClass->pNextName = 0;` |
|        - |   417 | `	/* Perform a simple hashtable insertion */` |
|    54982 |   418 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    54982 |   419 | `	return rc;` |
|    27507 |   420 |  |
|        - |   421 | `/*` |
|        - |   422 | ` * Instruction builder interface.` |
|        - |   423 | ` */` |
|  4073654 |   424 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   425 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   426 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   427 | `	sxi32 iP1,    /* First operand */` |
|        - |   428 | `	sxu32 iP2,    /* Second operand */` |
|        - |   429 | `	void *p3,     /* Third operand */` |
|        - |   430 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   431 | `	)` |
|        2 |   432 |  |
|        - |   433 | `	VmInstr sInstr;` |
|        - |   434 | `	sxi32 rc;` |
|        - |   435 | `	/* Fill the VM instruction */` |
|  4073656 |   436 | `	sInstr.iOp = (sxu8)iOp;` |
|  4073656 |   437 | `	sInstr.iP1 = iP1;` |
|  4073656 |   438 | `	sInstr.iP2 = iP2;` |
|  4073656 |   439 | `	sInstr.p3  = p3;` |
|  4073656 |   440 | `	if( pIndex ){` |
|        - |   441 | `		/* Instruction index in the bytecode array */` |
|   221212 |   442 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   110605 |   443 | `	}` |
|        - |   444 | `	/* Finally,record the instruction */` |
|  4073656 |   445 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  4073656 |   446 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   447 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   448 | `		/* Fall throw */` |
|      ! 0 |   449 | `	}` |
|  4073656 |   450 | `	return rc;` |
|        2 |   451 |  |
|        - |   452 | `/*` |
|        - |   453 | ` * Swap the current bytecode container with the given one.` |
|        - |   454 | ` */` |
|   528416 |   455 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   456 |  |
|   528418 |   457 | `	if( pContainer == 0 ){` |
|        - |   458 | `		/* Point to the default container */` |
|      ! 0 |   459 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   460 | `	}else{` |
|        - |   461 | `		/* Change container */` |
|   528418 |   462 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   463 | `	}` |
|   528418 |   464 | `	return SXRET_OK;` |
|        2 |   465 |  |
|        - |   466 | `/*` |
|        - |   467 | ` * Return the current bytecode container.` |
|        - |   468 | ` */` |
|   264208 |   469 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   470 |  |
|   264210 |   471 | `	return pVm->pByteContainer;` |
|        2 |   472 |  |
|        - |   473 | `/*` |
|        - |   474 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   475 | ` */` |
|   218128 |   476 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   477 |  |
|        - |   478 | `	VmInstr *pInstr;` |
|   218130 |   479 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   218130 |   480 | `	return pInstr;` |
|        2 |   481 |  |
|        - |   482 | `/*` |
|        - |   483 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   484 | ` */` |
|  1223820 |   485 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   486 |  |
|  1223822 |   487 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   488 |  |
|        - |   489 | `/*` |
|        - |   490 | ` * Pop the last VM instruction.` |
|        - |   491 | ` */` |
|   201788 |   492 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   493 |  |
|   201790 |   494 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   495 |  |
|        - |   496 | `/*` |
|        - |   497 | ` * Peek the last VM instruction.` |
|        - |   498 | ` */` |
|   802106 |   499 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   500 |  |
|   802108 |   501 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   502 |  |
|    31764 |   503 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   504 |  |
|        - |   505 | `	VmInstr *aInstr;` |
|        - |   506 | `	sxu32 n;` |
|    31766 |   507 | `	n = SySetUsed(pVm->pByteContainer);` |
|    31766 |   508 | `	if( n < 2 ){` |
|      ! 0 |   509 | `		return 0;` |
|        - |   510 | `	}` |
|    31766 |   511 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    31766 |   512 | `	return &aInstr[n - 2];` |
|    15884 |   513 |  |
|        - |   514 | `/*` |
|        - |   515 | ` * Allocate a new virtual machine frame.` |
|        - |   516 | ` */` |
|    21176 |   517 | `static VmFrame * VmNewFrame(` |
|        - |   518 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   519 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   520 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   521 | `	)` |
|        2 |   522 |  |
|        - |   523 | `	VmFrame *pFrame;` |
|        - |   524 | `	/* Allocate a new vm frame */` |
|    21178 |   525 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    21178 |   526 | `	if( pFrame == 0 ){` |
|      ! 0 |   527 | `		return 0;` |
|        - |   528 | `	}` |
|        - |   529 | `	/* Zero the structure */` |
|    21178 |   530 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   531 | `	/* Initialize frame fields */` |
|    21178 |   532 | `	pFrame->pUserData = pUserData;` |
|    21178 |   533 | `	pFrame->pThis = pThis;` |
|    21178 |   534 | `	pFrame->pVm = pVm;` |
|    21178 |   535 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    21178 |   536 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    21178 |   537 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    21178 |   538 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    21178 |   539 | `	return pFrame;` |
|    10590 |   540 |  |
|        - |   541 | `/* Forward declaration */` |
|        - |   542 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   543 | `/*` |
|        - |   544 | ` * Enter a VM frame.` |
|        - |   545 | ` */` |
|    21130 |   546 | `static sxi32 VmEnterFrame(` |
|        - |   547 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   548 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   549 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   550 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   551 | `	)` |
|        2 |   552 |  |
|        - |   553 | `	VmFrame *pFrame;` |
|        - |   554 | `	/* Allocate a new frame */` |
|    21132 |   555 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    21132 |   556 | `	if( pFrame == 0 ){` |
|      ! 0 |   557 | `		return SXERR_MEM;` |
|        - |   558 | `	}` |
|        - |   559 | `	/* Link to the list of active VM frame */` |
|    21132 |   560 | `	pFrame->pParent = pVm->pFrame;` |
|    21132 |   561 | `	pVm->pFrame = pFrame;` |
|    21132 |   562 | `	if( ppFrame ){` |
|        - |   563 | `		/* Write a pointer to the new VM frame */` |
|    18126 |   564 | `		*ppFrame = pFrame;` |
|     9062 |   565 | `	}` |
|    21132 |   566 | `	return SXRET_OK;` |
|    10567 |   567 |  |
|        - |   568 | `/*` |
|        - |   569 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   570 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   571 | ` * information.` |
|        - |   572 | ` */` |
|       58 |   573 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        2 |   574 |  |
|        - |   575 | `	VmFrame *pTarget,*pFrame;` |
|       60 |   576 | `	SyHashEntry *pEntry = 0;` |
|        - |   577 | `	sxi32 rc;` |
|        - |   578 | `	/* Point to the upper frame */` |
|       60 |   579 | `	pFrame = pVm->pFrame;` |
|       60 |   580 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       60 |   581 | `	pTarget = pFrame;` |
|       60 |   582 | `	pFrame = pTarget->pParent;` |
|       60 |   583 | `	while( pFrame ){` |
|       60 |   584 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   585 | `			/* Query the current frame */` |
|       60 |   586 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       60 |   587 | `			if( pEntry ){` |
|        - |   588 | `				/* Variable found */` |
|       60 |   589 | `				break;` |
|        - |   590 | `			}` |
|      ! 0 |   591 | `		}` |
|        - |   592 | `		/* Point to the upper frame */` |
|      ! 0 |   593 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   594 | `	}` |
|       60 |   595 | `	if( pEntry == 0 ){` |
|        - |   596 | `		/* Inexistant variable */` |
|      ! 0 |   597 | `		return SXERR_NOTFOUND;` |
|        - |   598 | `	}` |
|        - |   599 | `	/* Link to the current frame */` |
|       60 |   600 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       60 |   601 | `	if( rc == SXRET_OK ){` |
|        - |   602 | `		sxu32 nIdx;` |
|       60 |   603 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       60 |   604 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       29 |   605 | `	}` |
|       60 |   606 | `	return rc;` |
|       31 |   607 |  |
|        - |   608 | `/*` |
|        - |   609 | ` * Leave the top-most active frame.` |
|        - |   610 | ` */` |
|    18114 |   611 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   612 |  |
|    18116 |   613 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    18116 |   614 | `	if( pCurFrame ){` |
|        - |   615 | `		/* Unlink from the list of active VM frame */` |
|    18116 |   616 | `		pVm->pFrame = pCurFrame->pParent;` |
|    18116 |   617 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   618 | `			VmSlot  *aSlot;` |
|        - |   619 | `			sxu32 n;` |
|        - |   620 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    17802 |   621 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   119140 |   622 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   623 | `				/* Unset the local variable */` |
|   101340 |   624 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    50671 |   625 | `			}` |
|        - |   626 | `			/* Remove local reference */` |
|    17802 |   627 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   119202 |   628 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   101402 |   629 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    50702 |   630 | `			}` |
|     8900 |   631 | `		}` |
|        - |   632 | `		/* Release internal containers */` |
|    18116 |   633 | `		SyHashRelease(&pCurFrame->hVar);` |
|    18116 |   634 | `		SySetRelease(&pCurFrame->sArg);` |
|    18116 |   635 | `		SySetRelease(&pCurFrame->sLocal);` |
|    18116 |   636 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   637 | `		/* Release the whole structure */` |
|    18116 |   638 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     9057 |   639 | `	}` |
|    18116 |   640 |  |
|        - |   641 | `/*` |
|        - |   642 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   643 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   644 | ` * should be skipped when looking for the real execution context.` |
|        - |   645 | ` */` |
|  6981922 |   646 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   647 |  |
|  6983880 |   648 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     1958 |   649 | `		pFrame = pFrame->pParent;` |
|        2 |   650 | `	}` |
|  6981924 |   651 | `	return pFrame;` |
|        2 |   652 |  |
|        - |   653 | `/*` |
|        - |   654 | ` * Compare two functions signature and return the comparison result.` |
|        - |   655 | ` */` |
|      836 |   656 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   657 |  |
|      837 |   658 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      837 |   659 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      837 |   660 | `	const char *zSin = pSecond->zString;` |
|      837 |   661 | `	const char *zFin = pFirst->zString;` |
|      837 |   662 | `	const char *zPtr = zFin;` |
|      421 |   663 | `	for(;;){` |
|      843 |   664 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      413 |   665 | `			break;` |
|        - |   666 | `		}` |
|       19 |   667 | `		if( zFin[0] != zSin[0] ){` |
|        - |   668 | `			/* mismatch */` |
|       13 |   669 | `			break;` |
|        - |   670 | `		}` |
|        7 |   671 | `		zFin++;` |
|        7 |   672 | `		zSin++;` |
|        1 |   673 | `	}` |
|      837 |   674 | `	return (int)(zFin-zPtr);` |
|        1 |   675 |  |
|        - |   676 | `/*` |
|        - |   677 | ` * Select the appropriate VM function for the current call context.` |
|        - |   678 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   679 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   680 | ` * Refer to the official documentation for more information.` |
|        - |   681 | ` */` |
|      138 |   682 | `static ph7_vm_func * VmOverload(` |
|        - |   683 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   684 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   685 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   686 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   687 | `	)` |
|        2 |   688 |  |
|        - |   689 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   690 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   691 | `	ph7_vm_func *pLink;` |
|        - |   692 | `	SyString sArgSig;` |
|        - |   693 | `	SyBlob sSig;` |
|        - |   694 |  |
|      140 |   695 | `	pLink = pList;` |
|      140 |   696 | `	i = 0;` |
|        - |   697 | `	/* Put functions expecting the same number of passed arguments */` |
|     1086 |   698 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1024 |   699 | `		if( pLink == 0 ){` |
|       78 |   700 | `			break;` |
|        - |   701 | `		}` |
|      948 |   702 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   703 | `			/* Candidate for overloading */` |
|      902 |   704 | `			apSet[i++] = pLink;` |
|      450 |   705 | `		}` |
|        - |   706 | `		/* Point to the next entry */` |
|      948 |   707 | `		pLink = pLink->pNextName;` |
|        2 |   708 | `	}` |
|      140 |   709 | `	if( i < 1 ){` |
|        - |   710 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   711 | `		return pList;` |
|        - |   712 | `	}` |
|      140 |   713 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   714 | `		/* Return the only candidate */` |
|       32 |   715 | `		return apSet[0];` |
|        - |   716 | `	}` |
|        - |   717 | `	/* Calculate function signature */` |
|      109 |   718 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      367 |   719 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      259 |   720 | `		int c = 'n'; /* null */` |
|      259 |   721 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   722 | `			/* Hashmap */` |
|       45 |   723 | `			c = 'h';` |
|      237 |   724 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   725 | `			/* bool */` |
|      ! 0 |   726 | `			c = 'b';` |
|      215 |   727 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   728 | `			/* int */` |
|        7 |   729 | `			c = 'i';` |
|      212 |   730 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   731 | `			/* String */` |
|      107 |   732 | `			c = 's';` |
|      156 |   733 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   734 | `			/* Float */` |
|      ! 0 |   735 | `			c = 'f';` |
|      103 |   736 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   737 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|        3 |   738 | `			int marker = 'o';` |
|        3 |   739 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|        3 |   740 | `			SyString *pName = &pClass->sName;` |
|        3 |   741 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|        3 |   742 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|        3 |   743 | `			c = -1;` |
|        1 |   744 | `		}` |
|      259 |   745 | `		if( c > 0 ){` |
|      257 |   746 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      128 |   747 | `		}` |
|      130 |   748 | `	}` |
|      109 |   749 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      109 |   750 | `	iTarget = 0;` |
|      109 |   751 | `	iMax = -1;` |
|        - |   752 | `	/* Select the appropriate function */` |
|      945 |   753 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   754 | `		/* Compare the two signatures */` |
|      837 |   755 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      837 |   756 | `		if( iCur > iMax ){` |
|      113 |   757 | `			iMax = iCur;` |
|      113 |   758 | `			iTarget = j;` |
|       56 |   759 | `		}` |
|      419 |   760 | `	}` |
|      109 |   761 | `	SyBlobRelease(&sSig);` |
|        - |   762 | `	/* Appropriate function for the current call context */` |
|      109 |   763 | `	return apSet[iTarget];` |
|       71 |   764 |  |
|        - |   765 | `/* Forward declaration */` |
|        - |   766 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   767 | `/*` |
|        - |   768 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   769 | ` * it can be instanciated from the executed PHP script.` |
|        - |   770 | ` */` |
|   160656 |   771 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   772 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   773 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   774 | `	)` |
|        2 |   775 |  |
|        - |   776 | `	ph7_class_method *pMeth;` |
|        - |   777 | `	ph7_class_attr *pAttr;` |
|        - |   778 | `	SyHashEntry *pEntry;` |
|        - |   779 | `	sxi32 rc;` |
|        - |   780 | `	/* Reset the loop cursor */` |
|   160658 |   781 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   782 | `	/* Process only static and constant attribute */` |
|   626300 |   783 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   784 | `		/* Extract the current attribute */` |
|   385316 |   785 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   385316 |   786 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   787 | `			ph7_value *pMemObj;` |
|        - |   788 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1776 |   789 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1776 |   790 | `			if( pMemObj == 0 ){` |
|      ! 0 |   791 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   792 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   793 | `					&pClass->sName,&pAttr->sName` |
|        - |   794 | `					);` |
|      ! 0 |   795 | `				return SXERR_MEM;` |
|        - |   796 | `			}` |
|     1776 |   797 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   798 | `				/* Initialize attribute default value (any complex expression) */` |
|     1772 |   799 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      885 |   800 | `			}` |
|        - |   801 | `			/* Record attribute index */` |
|     1776 |   802 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   803 | `			/* Install static attribute in the reference table */` |
|     1776 |   804 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   805 | `			/* If this is a typed static property, register the slot so the` |
|        - |   806 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |   807 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |   808 | `			 * points at its own nIdx field (stable for the VM lifetime). */` |
|     1776 |   809 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       10 |   810 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       10 |   811 | `				if( pVmAttrS == 0 ){` |
|      ! 0 |   812 | `					return SXERR_MEM;` |
|        - |   813 | `				}` |
|       10 |   814 | `				pVmAttrS->pAttr = pAttr;` |
|       10 |   815 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|       10 |   816 | `				pVmAttrS->iState = 0;` |
|       10 |   817 | `				pVmAttrS->pOwner = pClass;` |
|        - |   818 | `				/* Static typed property with no default starts uninitialized */` |
|        8 |   819 | `				if( SySetUsed(&pAttr->aByteCode) == 0` |
|        8 |   820 | `				 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        6 |   821 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        2 |   822 | `				}` |
|       10 |   823 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|      ! 0 |   824 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|      ! 0 |   825 | `					return SXERR_MEM;` |
|        - |   826 | `				}` |
|        4 |   827 | `			}` |
|      887 |   828 | `		}` |
|        2 |   829 | `	}` |
|        - |   830 | `	/* Install class methods */` |
|   160658 |   831 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   832 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   833 | `		 */` |
|    80140 |   834 | `		return SXRET_OK;` |
|        - |   835 | `	}` |
|        - |   836 | `	/* Create constructor alias if not yet done */` |
|    80520 |   837 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   838 | `		/* User constructor with the same base class name */` |
|     6308 |   839 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     6308 |   840 | `		if( pEntry ){` |
|      ! 0 |   841 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   842 | `			/* Create the alias */` |
|      ! 0 |   843 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   844 | `		}` |
|     3153 |   845 | `	}` |
|        - |   846 | `	/* Install the methods now */` |
|    80520 |   847 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   805627 |   848 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   684850 |   849 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   684850 |   850 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   684842 |   851 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   684842 |   852 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   853 | `				return rc;` |
|        - |   854 | `			}` |
|   342420 |   855 | `		}` |
|        2 |   856 | `	}` |
|        - |   857 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    80520 |   858 | `	pClass->bMounted = TRUE;` |
|    80520 |   859 | `	return SXRET_OK;` |
|    80330 |   860 |  |
|        - |   861 | `/*` |
|        - |   862 | ` * Allocate a private frame for attributes of the given` |
|        - |   863 | ` * class instance (Object in the PHP jargon).` |
|        - |   864 | ` */` |
|     1900 |   865 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   866 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   867 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   868 | `	)` |
|        2 |   869 |  |
|     1902 |   870 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   871 | `	ph7_class_attr *pAttr;` |
|        - |   872 | `	SyHashEntry *pEntry;` |
|        - |   873 | `	sxi32 rc;` |
|        - |   874 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1902 |   875 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     7940 |   876 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   877 | `		VmClassAttr *pVmAttr;` |
|        - |   878 | `		/* Extract the current attribute */` |
|     6040 |   879 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     6040 |   880 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     6040 |   881 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   882 | `			return SXERR_MEM;` |
|        - |   883 | `		}` |
|     6040 |   884 | `		pVmAttr->pAttr = pAttr;` |
|     6040 |   885 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   886 | `			ph7_value *pMemObj;` |
|        - |   887 | `			/* Reserve a memory object for this attribute */` |
|     6016 |   888 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     6016 |   889 | `			if( pMemObj == 0 ){` |
|      ! 0 |   890 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   891 | `				return SXERR_MEM;` |
|        - |   892 | `			}` |
|     6016 |   893 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     6016 |   894 | `			pVmAttr->iState = 0;` |
|     6016 |   895 | `			pVmAttr->pOwner = pClass;` |
|     6016 |   896 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   897 | `				/* Initialize attribute default value (any complex expression) */` |
|     2054 |   898 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     4990 |   899 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   900 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   901 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       68 |   902 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       33 |   903 | `			}` |
|     6016 |   904 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     6016 |   905 | `			if( rc != SXRET_OK ){` |
|        - |   906 | `				VmSlot sSlot;` |
|        - |   907 | `				/* Restore memory object */` |
|      ! 0 |   908 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   909 | `				sSlot.pUserData = 0;` |
|      ! 0 |   910 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   911 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   912 | `				return SXERR_MEM;` |
|        - |   913 | `			}` |
|        - |   914 | `			/* Install attribute in the reference table */` |
|     6016 |   915 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   916 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   917 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   918 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     6016 |   919 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      162 |   920 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      162 |   921 | `				if( rc != SXRET_OK ){` |
|        - |   922 | `					VmSlot sSlot;` |
|      ! 0 |   923 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 |   924 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   925 | `					sSlot.pUserData = 0;` |
|      ! 0 |   926 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   927 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   928 | `					return SXERR_MEM;` |
|        - |   929 | `				}` |
|       80 |   930 | `			}` |
|     3009 |   931 | `		}else{` |
|        - |   932 | `			/* Install static/constant attribute */` |
|       26 |   933 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       26 |   934 | `			pVmAttr->iState = 0;` |
|       26 |   935 | `			pVmAttr->pOwner = pClass;` |
|       26 |   936 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       26 |   937 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   938 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   939 | `				return SXERR_MEM;` |
|        - |   940 | `			}` |
|        - |   941 | `		}` |
|        2 |   942 | `	}` |
|     1902 |   943 | `	return SXRET_OK;` |
|      952 |   944 |  |
|        - |   945 | `/* Forward declaration */` |
|        - |   946 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   947 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   948 | `/*` |
|        - |   949 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   950 | ` */` |
|        - |   951 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   952 | `/*` |
|        - |   953 | ` * Reserve a constant memory object.` |
|        - |   954 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   955 | ` */` |
|   435652 |   956 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   957 |  |
|        - |   958 | `	ph7_value *pObj;` |
|        - |   959 | `	sxi32 rc;` |
|   435654 |   960 | `	if( pIndex ){` |
|        - |   961 | `		/* Object index in the object table */` |
|   426636 |   962 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   213317 |   963 | `	}` |
|        - |   964 | `	/* Reserve a slot for the new object */` |
|   435654 |   965 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   435654 |   966 | `	if( rc != SXRET_OK ){` |
|        - |   967 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   968 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   969 | `		 */` |
|      ! 0 |   970 | `		return 0;` |
|        - |   971 | `	}` |
|   435654 |   972 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   435654 |   973 | `	return pObj;` |
|   217828 |   974 |  |
|        - |   975 | `/*` |
|        - |   976 | ` * Reserve a memory object.` |
|        - |   977 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   978 | ` */` |
|  2149588 |   979 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   980 |  |
|        - |   981 | `	ph7_value *pObj;` |
|        - |   982 | `	sxi32 rc;` |
|  2149590 |   983 | `	if( pIndex ){` |
|        - |   984 | `		/* Object index in the object table */` |
|  2149590 |   985 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1074794 |   986 | `	}` |
|        - |   987 | `	/* Reserve a slot for the new object */` |
|  2149590 |   988 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2149590 |   989 | `	if( rc != SXRET_OK ){` |
|        - |   990 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   991 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   992 | `		 */` |
|      ! 0 |   993 | `		return 0;` |
|        - |   994 | `	}` |
|  2149590 |   995 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2149590 |   996 | `	return pObj;` |
|  1074796 |   997 |  |
|        - |   998 | `/* Forward declaration */` |
|        - |   999 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |  1000 | `/* Forward declarations for Fiber C functions */` |
|        - |  1001 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1002 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1003 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1004 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1005 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1006 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1007 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1008 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1009 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1010 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1011 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |  1012 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |  1013 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1014 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  1015 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |  1016 | `static sxi32 VmCallClassMethodWithMap(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1017 | `	ph7_class_method *pMethod, ph7_value *pResult, int nArg,` |
|        - |  1018 | `	ph7_value **apArg, VmCallArgMap *pMap);` |
|        - |  1019 | `static sxi32 VmCallObjectInvoke(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1020 | `	int nArg, ph7_value **apArg, ph7_value *pResult, VmCallArgMap *pMap);` |
|        - |  1021 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis);` |
|        - |  1022 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |  1023 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1024 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |  1025 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1026 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1027 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1028 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1029 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1030 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1031 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1032 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1033 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1034 | `/*` |
|        - |  1035 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |  1036 | ` * directly as foreign functions.` |
|        - |  1037 | ` */` |
|        - |  1038 | `#define PH7_BUILTIN_LIB \` |
|        - |  1039 | `	"interface Throwable {"\` |
|        - |  1040 | `	"public function getMessage();"\` |
|        - |  1041 | `	"public function getCode();"\` |
|        - |  1042 | `	"public function getFile();"\` |
|        - |  1043 | `	"public function getLine();"\` |
|        - |  1044 | `	"public function getTrace();"\` |
|        - |  1045 | `	"public function getTraceAsString();"\` |
|        - |  1046 | `	"public function getPrevious();"\` |
|        - |  1047 | `	"public function __toString();"\` |
|        - |  1048 | `	"}"\` |
|        - |  1049 | `	"class Exception implements Throwable { "\` |
|        - |  1050 | `    "protected $message = '';"\` |
|        - |  1051 | `    "protected $code = 0;"\` |
|        - |  1052 | `    "protected $file;"\` |
|        - |  1053 | `    "protected $line;"\` |
|        - |  1054 | `    "protected $trace;"\` |
|        - |  1055 | `    "protected $previous;"\` |
|        - |  1056 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1057 | `	"   if( isset($message) ){"\` |
|        - |  1058 | `	"	  $this->message = $message;"\` |
|        - |  1059 | `	"   }"\` |
|        - |  1060 | `	"   $this->code = $code;"\` |
|        - |  1061 | `	"   $this->file = __FILE__;"\` |
|        - |  1062 | `	"   $this->line = __LINE__;"\` |
|        - |  1063 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1064 | `	"   if( isset($previous) ){"\` |
|        - |  1065 | `	"     $this->previous = $previous;"\` |
|        - |  1066 | `	"   }"\` |
|        - |  1067 | `	"}"\` |
|        - |  1068 | `	"public function getMessage(){"\` |
|        - |  1069 | `	"   return $this->message;"\` |
|        - |  1070 | `	"}"\` |
|        - |  1071 | `	" public function getCode(){"\` |
|        - |  1072 | `	"  return $this->code;"\` |
|        - |  1073 | `	"}"\` |
|        - |  1074 | `	"public function getFile(){"\` |
|        - |  1075 | `	"  return $this->file;"\` |
|        - |  1076 | `	"}"\` |
|        - |  1077 | `	"public function getLine(){"\` |
|        - |  1078 | `	"  return $this->line;"\` |
|        - |  1079 | `	"}"\` |
|        - |  1080 | `	"public function getTrace(){"\` |
|        - |  1081 | `	"   return $this->trace;"\` |
|        - |  1082 | `	"}"\` |
|        - |  1083 | `	"public function getTraceAsString(){"\` |
|        - |  1084 | `	"  return debug_string_backtrace();"\` |
|        - |  1085 | `	"}"\` |
|        - |  1086 | `	"public function getPrevious(){"\` |
|        - |  1087 | `	"    return $this->previous;"\` |
|        - |  1088 | `	"}"\` |
|        - |  1089 | `	"public function __toString(){"\` |
|        - |  1090 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1091 | `    "}"\` |
|        - |  1092 | `	"}"\` |
|        - |  1093 | `	"class Error implements Throwable { "\` |
|        - |  1094 | `    "protected $message = '';"\` |
|        - |  1095 | `    "protected $code = 0;"\` |
|        - |  1096 | `    "protected $file;"\` |
|        - |  1097 | `    "protected $line;"\` |
|        - |  1098 | `    "protected $trace;"\` |
|        - |  1099 | `    "protected $previous;"\` |
|        - |  1100 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1101 | `	"   if( isset($message) ){"\` |
|        - |  1102 | `	"	  $this->message = $message;"\` |
|        - |  1103 | `	"   }"\` |
|        - |  1104 | `	"   $this->code = $code;"\` |
|        - |  1105 | `	"   $this->file = __FILE__;"\` |
|        - |  1106 | `	"   $this->line = __LINE__;"\` |
|        - |  1107 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1108 | `	"   if( isset($previous) ){"\` |
|        - |  1109 | `	"     $this->previous = $previous;"\` |
|        - |  1110 | `	"   }"\` |
|        - |  1111 | `	"}"\` |
|        - |  1112 | `	"public function getMessage(){"\` |
|        - |  1113 | `	"   return $this->message;"\` |
|        - |  1114 | `	"}"\` |
|        - |  1115 | `	"public function getCode(){"\` |
|        - |  1116 | `	"  return $this->code;"\` |
|        - |  1117 | `	"}"\` |
|        - |  1118 | `	"public function getFile(){"\` |
|        - |  1119 | `	"  return $this->file;"\` |
|        - |  1120 | `	"}"\` |
|        - |  1121 | `	"public function getLine(){"\` |
|        - |  1122 | `	"  return $this->line;"\` |
|        - |  1123 | `	"}"\` |
|        - |  1124 | `	"public function getTrace(){"\` |
|        - |  1125 | `	"   return $this->trace;"\` |
|        - |  1126 | `	"}"\` |
|        - |  1127 | `	"public function getTraceAsString(){"\` |
|        - |  1128 | `	"  return debug_string_backtrace();"\` |
|        - |  1129 | `	"}"\` |
|        - |  1130 | `	"public function getPrevious(){"\` |
|        - |  1131 | `	"    return $this->previous;"\` |
|        - |  1132 | `	"}"\` |
|        - |  1133 | `	"public function __toString(){"\` |
|        - |  1134 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1135 | `	"}"\` |
|        - |  1136 | `	"}"\` |
|        - |  1137 | `	"class TypeError extends Error { }"\` |
|        - |  1138 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |  1139 | `	"class ValueError extends Error { }"\` |
|        - |  1140 | `	"class FiberError extends Error { }"\` |
|        - |  1141 | `	"class AssertionError extends Error { }"\` |
|        - |  1142 | `	"class ArithmeticError extends Error { }"\` |
|        - |  1143 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1144 | `	"class ErrorException extends Exception { "\` |
|        - |  1145 | `	"protected $severity;"\` |
|        - |  1146 | `	"public function __construct(string $message = null,"\` |
|        - |  1147 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Throwable $previous = null){"\` |
|        - |  1148 | `	"   if( isset($message) ){"\` |
|        - |  1149 | `	"	  $this->message = $message;"\` |
|        - |  1150 | `	"   }"\` |
|        - |  1151 | `	"   $this->severity = $severity;"\` |
|        - |  1152 | `	"   $this->code = $code;"\` |
|        - |  1153 | `	"   $this->file = $filename;"\` |
|        - |  1154 | `	"   $this->line = $lineno;"\` |
|        - |  1155 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1156 | `	"   if( isset($previous) ){"\` |
|        - |  1157 | `	"     $this->previous = $previous;"\` |
|        - |  1158 | `	"   }"\` |
|        - |  1159 | `	"}"\` |
|        - |  1160 | `	"public function getSeverity(){"\` |
|        - |  1161 | `	"   return $this->severity;"\` |
|        - |  1162 | `    "}"\` |
|        - |  1163 | `	"}"\` |
|        - |  1164 | `	"interface Iterator {"\` |
|        - |  1165 | `	"public function current();"\` |
|        - |  1166 | `	"public function key();"\` |
|        - |  1167 | `	"public function next();"\` |
|        - |  1168 | `	"public function rewind();"\` |
|        - |  1169 | `	"public function valid();"\` |
|        - |  1170 | `	"}"\` |
|        - |  1171 | `	"interface IteratorAggregate {"\` |
|        - |  1172 | `	"public function getIterator();"\` |
|        - |  1173 | `	"}"\` |
|        - |  1174 | `	"interface Serializable {"\` |
|        - |  1175 | `	"public function serialize();"\` |
|        - |  1176 | `	"public function unserialize(string $serialized);"\` |
|        - |  1177 | `	"}"\` |
|        - |  1178 | `	"/* Directory releated IO */"\` |
|        - |  1179 | `	"class Directory {"\` |
|        - |  1180 | `	"public $handle = null;"\` |
|        - |  1181 | `	"public $path  = null;"\` |
|        - |  1182 | `	"public function __construct(string $path)"\` |
|        - |  1183 | `	"{"\` |
|        - |  1184 | `	"   $this->handle = opendir($path);"\` |
|        - |  1185 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1186 | `	"      $this->path = $path;"\` |
|        - |  1187 | `	"   }"\` |
|        - |  1188 | `	"}"\` |
|        - |  1189 | `	"public function __destruct()"\` |
|        - |  1190 | `	"{"\` |
|        - |  1191 | `	"  if( $this->handle != null ){"\` |
|        - |  1192 | `	"       closedir($this->handle);"\` |
|        - |  1193 | `	"  }"\` |
|        - |  1194 | `	"}"\` |
|        - |  1195 | `	"public function read()"\` |
|        - |  1196 | `	"{"\` |
|        - |  1197 | `	"    return readdir($this->handle);"\` |
|        - |  1198 | `	"}"\` |
|        - |  1199 | `	"public function rewind()"\` |
|        - |  1200 | `	"{"\` |
|        - |  1201 | `	"    rewinddir($this->handle);"\` |
|        - |  1202 | `	"}"\` |
|        - |  1203 | `	"public function close()"\` |
|        - |  1204 | `	"{"\` |
|        - |  1205 | `	"    closedir($this->handle);"\` |
|        - |  1206 | `	"    $this->handle = null;"\` |
|        - |  1207 | `	"}"\` |
|        - |  1208 | `	"}"\` |
|        - |  1209 | `	"class Fiber {"\` |
|        - |  1210 | `	"  private $__ctx;"\` |
|        - |  1211 | `	"  private $__callable;"\` |
|        - |  1212 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1213 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1214 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1215 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1216 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1217 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1218 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1219 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1220 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1221 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1222 | `	"}"\` |
|        - |  1223 | `	"class Generator implements Iterator {"\` |
|        - |  1224 | `	"  private $__ctx;"\` |
|        - |  1225 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1226 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1227 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1228 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1229 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1230 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1231 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1232 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1233 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1234 | `	"}"\` |
|        - |  1235 | `	"class stdClass{"\` |
|        - |  1236 | `	"  public $value;"\` |
|        - |  1237 | `	" /* Magic methods */"\` |
|        - |  1238 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1239 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1240 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1241 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1242 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1243 | `	"}"\` |
|        - |  1244 | `	"function dir(string $path){"\` |
|        - |  1245 | `	"   return new Directory($path);"\` |
|        - |  1246 | `	"}"\` |
|        - |  1247 | `	"function Dir(string $path){"\` |
|        - |  1248 | `	"   return new Directory($path);"\` |
|        - |  1249 | `	"}"\` |
|        - |  1250 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1251 | `    "{"\` |
|        - |  1252 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1253 | `	"  $aDir = array();"\` |
|        - |  1254 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1255 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1256 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1257 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1258 | `	"   }"\` |
|        - |  1259 | `	"  closedir($pHandle);"\` |
|        - |  1260 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1261 | `	"      rsort($aDir);"\` |
|        - |  1262 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1263 | `	"      sort($aDir);"\` |
|        - |  1264 | `	"  }"\` |
|        - |  1265 | `	"  return $aDir;"\` |
|        - |  1266 | `	"}"\` |
|        - |  1267 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1268 | `	"/* Open the target directory */"\` |
|        - |  1269 | `	"$zDir = dirname($pattern);"\` |
|        - |  1270 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1271 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1272 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1273 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1274 | `	"	return FALSE;"\` |
|        - |  1275 | `	"}"\` |
|        - |  1276 | `	"$pattern = basename($pattern);"\` |
|        - |  1277 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1278 | `	"/* Loop throw available entries */"\` |
|        - |  1279 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1280 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1281 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1282 | `	"	if( $rc ){"\` |
|        - |  1283 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1284 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1285 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1286 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1287 | `	"		  }"\` |
|        - |  1288 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1289 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1290 | `	"		 continue;"\` |
|        - |  1291 | `	"	   }"\` |
|        - |  1292 | `	"	   /* Add the entry */"\` |
|        - |  1293 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1294 | `	"	}"\` |
|        - |  1295 | `	" }"\` |
|        - |  1296 | `	"/* Close the handle */"\` |
|        - |  1297 | `	"closedir($pHandle);"\` |
|        - |  1298 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1299 | `	"  /* Sort the array */"\` |
|        - |  1300 | `	"  sort($pArray);"\` |
|        - |  1301 | `	"}"\` |
|        - |  1302 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1303 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1304 | `	"  $pArray[] = $pattern;"\` |
|        - |  1305 | `	"}"\` |
|        - |  1306 | `	"/* Return the created array */"\` |
|        - |  1307 | `	"return $pArray;"\` |
|        - |  1308 | `   "}"\` |
|        - |  1309 | `   "/* Creates a temporary file */"\` |
|        - |  1310 | `   "function tmpfile(){"\` |
|        - |  1311 | `   "  /* Extract the temp directory */"\` |
|        - |  1312 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1313 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1314 | `   "    /* Use the current dir */"\` |
|        - |  1315 | `   "    $zTempDir = '.';"\` |
|        - |  1316 | `   "  }"\` |
|        - |  1317 | `   "  /* Create the file */"\` |
|        - |  1318 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1319 | `   "  return $pHandle;"\` |
|        - |  1320 | `   "}"\` |
|        - |  1321 | `   "/* Creates a temporary filename */"\` |
|        - |  1322 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1323 | `   "{"\` |
|        - |  1324 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1325 | `   "}"\` |
|        - |  1326 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1327 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1328 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1329 | `   "/* Copy arguments */"\` |
|        - |  1330 | `   "$nArgs = func_num_args();"\` |
|        - |  1331 | `   "$pNew = array();"\` |
|        - |  1332 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1333 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1334 | `    "}"\` |
|        - |  1335 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1336 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1337 | `	"/* Erase */"\` |
|        - |  1338 | `	"array_erase($pArray);"\` |
|        - |  1339 | `	"/* Unshift */"\` |
|        - |  1340 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1341 | `	"return sizeof($pArray);"\` |
|        - |  1342 | `    "}"\` |
|        - |  1343 | `	"function array_merge_recursive(){"\` |
|        - |  1344 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1345 | `    "$arrays = func_get_args();"\` |
|        - |  1346 | `    "$narrays = count($arrays);"\` |
|        - |  1347 | `    "$ret = array();"\` |
|        - |  1348 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1349 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1350 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1351 | `	 " }"\` |
|        - |  1352 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1353 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1354 | `     "  if( $keyIsInt ) {"\` |
|        - |  1355 | `     "   $ret[] = $value;"\` |
|        - |  1356 | `     "  } else {"\` |
|        - |  1357 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1358 | `     "    $cur = $ret[$key];"\` |
|        - |  1359 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1360 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1361 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1362 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1363 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1364 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1365 | `     "    } else {"\` |
|        - |  1366 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1367 | `     "    }"\` |
|        - |  1368 | `     "   } else {"\` |
|        - |  1369 | `     "    $ret[$key] = $value;"\` |
|        - |  1370 | `     "   }"\` |
|        - |  1371 | `     "  }"\` |
|        - |  1372 | `     " }"\` |
|        - |  1373 | `	 " }"\` |
|        - |  1374 | `	 " return $ret;"\` |
|        - |  1375 | `    "}"\` |
|        - |  1376 | `	"function max(){"\` |
|        - |  1377 | `    "  $pArgs = func_get_args();"\` |
|        - |  1378 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1379 | `	"  return null;"\` |
|        - |  1380 | `    " }"\` |
|        - |  1381 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1382 | `    " $pArg = $pArgs[0];"\` |
|        - |  1383 | `	" if( !is_array($pArg) ){"\` |
|        - |  1384 | `	"   return $pArg; "\` |
|        - |  1385 | `	" }"\` |
|        - |  1386 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1387 | `	"   return null;"\` |
|        - |  1388 | `	" }"\` |
|        - |  1389 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1390 | `	" reset($pArg);"\` |
|        - |  1391 | `	" $max = current($pArg);"\` |
|        - |  1392 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1393 | `	"   if( $val > $max ){"\` |
|        - |  1394 | `	"     $max = $val;"\` |
|        - |  1395 | `    " }"\` |
|        - |  1396 | `	" }"\` |
|        - |  1397 | `	" return $max;"\` |
|        - |  1398 | `    " }"\` |
|        - |  1399 | `    " $max = $pArgs[0];"\` |
|        - |  1400 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1401 | `    " $val = $pArgs[$i];"\` |
|        - |  1402 | `	"if( $val > $max ){"\` |
|        - |  1403 | `	" $max = $val;"\` |
|        - |  1404 | `	"}"\` |
|        - |  1405 | `    " }"\` |
|        - |  1406 | `	" return $max;"\` |
|        - |  1407 | `    "}"\` |
|        - |  1408 | `	"function min(){"\` |
|        - |  1409 | `    "  $pArgs = func_get_args();"\` |
|        - |  1410 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1411 | `	"  return null;"\` |
|        - |  1412 | `    " }"\` |
|        - |  1413 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1414 | `    " $pArg = $pArgs[0];"\` |
|        - |  1415 | `	" if( !is_array($pArg) ){"\` |
|        - |  1416 | `	"   return $pArg; "\` |
|        - |  1417 | `	" }"\` |
|        - |  1418 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1419 | `	"   return null;"\` |
|        - |  1420 | `	" }"\` |
|        - |  1421 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1422 | `	" reset($pArg);"\` |
|        - |  1423 | `	" $min = current($pArg);"\` |
|        - |  1424 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1425 | `	"   if( $val < $min ){"\` |
|        - |  1426 | `	"     $min = $val;"\` |
|        - |  1427 | `    " }"\` |
|        - |  1428 | `	" }"\` |
|        - |  1429 | `	" return $min;"\` |
|        - |  1430 | `    " }"\` |
|        - |  1431 | `    " $min = $pArgs[0];"\` |
|        - |  1432 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1433 | `    " $val = $pArgs[$i];"\` |
|        - |  1434 | `	"if( $val < $min ){"\` |
|        - |  1435 | `	" $min = $val;"\` |
|        - |  1436 | `	" }"\` |
|        - |  1437 | `    " }"\` |
|        - |  1438 | `	" return $min;"\` |
|        - |  1439 | `	"}"\` |
|        - |  1440 | `	"function fileowner(string $file){"\` |
|        - |  1441 | `    " $a = stat($file);"\` |
|        - |  1442 | `	" if( !is_array($a) ){"\` |
|        - |  1443 | `	"	return false;"\` |
|        - |  1444 | `	" }"\` |
|        - |  1445 | `	" return $a['uid'];"\` |
|        - |  1446 | `    "}"\` |
|        - |  1447 | `    "function filegroup(string $file){"\` |
|        - |  1448 | `	" $a = stat($file);"\` |
|        - |  1449 | `	" if( !is_array($a) ){"\` |
|        - |  1450 | `	"	return false;"\` |
|        - |  1451 | `	" }"\` |
|        - |  1452 | `	" return $a['gid'];"\` |
|        - |  1453 | `    "}"\` |
|        - |  1454 | `	 "function fileinode(string $file){"\` |
|        - |  1455 | `	" $a = stat($file);"\` |
|        - |  1456 | `	" if( !is_array($a) ){"\` |
|        - |  1457 | `	"	return false;"\` |
|        - |  1458 | `	" }"\` |
|        - |  1459 | `	" return $a['ino'];"\` |
|        - |  1460 | `    "}"` |
|        - |  1461 |  |
|        - |  1462 | `/*` |
|        - |  1463 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1464 | ` * start compiling the target PHP program.` |
|        - |  1465 | ` */` |
|     3006 |  1466 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1467 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1468 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1469 | `	 )` |
|        2 |  1470 |  |
|        - |  1471 | `	SyString sBuiltin;` |
|        - |  1472 | `	ph7_value *pObj;` |
|        - |  1473 | `	sxi32 rc;` |
|        - |  1474 | `	/* Zero the structure */` |
|     3008 |  1475 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1476 | `	/* Initialize VM fields */` |
|     3008 |  1477 | `	pVm->pEngine = &(*pEngine);` |
|     3008 |  1478 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1479 | `	/* Instructions containers */` |
|     3008 |  1480 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     3008 |  1481 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     3008 |  1482 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1483 | `	/* Object containers */` |
|     3008 |  1484 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3008 |  1485 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1486 | `	/* Virtual machine internal containers */` |
|     3008 |  1487 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     3008 |  1488 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     3008 |  1489 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     3008 |  1490 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3008 |  1491 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     3008 |  1492 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     3008 |  1493 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     3008 |  1494 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     3008 |  1495 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     3008 |  1496 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     3008 |  1497 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     3008 |  1498 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     3008 |  1499 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     3008 |  1500 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     3008 |  1501 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     3008 |  1502 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     3008 |  1503 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     3008 |  1504 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     3008 |  1505 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     3008 |  1506 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     3008 |  1507 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     3008 |  1508 | `	pVm->pPendingException = 0;` |
|        - |  1509 | `	/* Configuration containers */` |
|     3008 |  1510 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     3008 |  1511 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     3008 |  1512 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     3008 |  1513 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     3008 |  1514 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     3008 |  1515 | `	pVm->iResponseStatus = 200;` |
|     3008 |  1516 | `	pVm->bHeadersSent = 0;` |
|     3008 |  1517 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1518 | `	/* Error callbacks containers */` |
|     3008 |  1519 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     3008 |  1520 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     3008 |  1521 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     3008 |  1522 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     3008 |  1523 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1524 | `	/* Set a default recursion limit */` |
|        - |  1525 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     3008 |  1526 | `	pVm->nMaxDepth = 32;` |
|        - |  1527 | `#else` |
|        - |  1528 | `	pVm->nMaxDepth = 16;` |
|        - |  1529 | `#endif` |
|        - |  1530 | `	/* Default assertion flags */` |
|     3008 |  1531 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1532 | `	/* JSON return status */` |
|     3008 |  1533 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1534 | `	/* PRNG context */` |
|     3008 |  1535 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1536 | `	/* Install the null constant */` |
|     3008 |  1537 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3008 |  1538 | `	if( pObj == 0 ){` |
|      ! 0 |  1539 | `		rc = SXERR_MEM;` |
|      ! 0 |  1540 | `		goto Err;` |
|        - |  1541 | `	}` |
|     3008 |  1542 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1543 | `	/* Install the boolean TRUE constant */` |
|     3008 |  1544 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3008 |  1545 | `	if( pObj == 0 ){` |
|      ! 0 |  1546 | `		rc = SXERR_MEM;` |
|      ! 0 |  1547 | `		goto Err;` |
|        - |  1548 | `	}` |
|     3008 |  1549 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1550 | `	/* Install the boolean FALSE constant */` |
|     3008 |  1551 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3008 |  1552 | `	if( pObj == 0 ){` |
|      ! 0 |  1553 | `		rc = SXERR_MEM;` |
|      ! 0 |  1554 | `		goto Err;` |
|        - |  1555 | `	}` |
|     3008 |  1556 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1557 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1558 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1559 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     3008 |  1560 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     3008 |  1561 | `	if( pObj == 0 ){` |
|      ! 0 |  1562 | `		rc = SXERR_MEM;` |
|      ! 0 |  1563 | `		goto Err;` |
|        - |  1564 | `	}` |
|     3008 |  1565 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1566 | `	/* Create the global frame */` |
|     3008 |  1567 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     3008 |  1568 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1569 | `		goto Err;` |
|        - |  1570 | `	}` |
|        - |  1571 | `	/* Initialize the code generator */` |
|     3008 |  1572 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3008 |  1573 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1574 | `		goto Err;` |
|        - |  1575 | `	}` |
|        - |  1576 | `	/* VM correctly initialized,set the magic number */` |
|     3008 |  1577 | `	pVm->nMagic = PH7_VM_INIT;` |
|     3008 |  1578 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1579 | `	/* Compile the built-in library */` |
|     3008 |  1580 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1581 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     3008 |  1582 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1583 | `	/* Register Fiber internal C functions */` |
|     3008 |  1584 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     3008 |  1585 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     3008 |  1586 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     3008 |  1587 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     3008 |  1588 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     3008 |  1589 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     3008 |  1590 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     3008 |  1591 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     3008 |  1592 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     3008 |  1593 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1594 | `	/* Cache the Generator class pointer and register generator functions */` |
|     3008 |  1595 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     3008 |  1596 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     3008 |  1597 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     3008 |  1598 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     3008 |  1599 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     3008 |  1600 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     3008 |  1601 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     3008 |  1602 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     3008 |  1603 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     3008 |  1604 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1605 | `	/* Reset the code generator */` |
|     3008 |  1606 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3008 |  1607 | `	return SXRET_OK;` |
|      ! 0 |  1608 | `Err:` |
|      ! 0 |  1609 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1610 | `	return rc;` |
|     1505 |  1611 |  |
|        - |  1612 | `/*` |
|        - |  1613 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1614 | ` * routine which store the output in an internal blob.` |
|        - |  1615 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1616 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1617 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1618 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1619 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1620 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1621 | ` * to finish executing and extracting the output.` |
|        - |  1622 | ` */` |
|       38 |  1623 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1624 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1625 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1626 | `	void *pUserData     /* User private data */` |
|        - |  1627 | `	)` |
|      ! 0 |  1628 |  |
|        - |  1629 | `	 sxi32 rc;` |
|        - |  1630 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1631 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1632 | `	 return rc;` |
|      ! 0 |  1633 |  |
|        - |  1634 | `/*` |
|        - |  1635 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1636 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1637 | ` */` |
|    18552 |  1638 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1639 |  |
|    18554 |  1640 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    18554 |  1641 | `	if( xCons != VmObConsumer ){` |
|     7534 |  1642 | `		pVm->nOutputLen += nLen;` |
|     7534 |  1643 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      952 |  1644 | `			pVm->bHeadersSent = 1;` |
|      475 |  1645 | `		}` |
|     3766 |  1646 | `	}` |
|    18554 |  1647 |  |
|        - |  1648 | `#define VM_STACK_GUARD 16` |
|        - |  1649 | `/*` |
|        - |  1650 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1651 | ` * our compiled PHP program.` |
|        - |  1652 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1653 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1654 | ` */` |
|    42562 |  1655 | `static ph7_value * VmNewOperandStack(` |
|        - |  1656 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1657 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1658 | `	)` |
|        2 |  1659 |  |
|        - |  1660 | `	ph7_value *pStack;` |
|        - |  1661 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1662 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1663 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1664 | `  ** on the maximum stack depth required.` |
|        - |  1665 | `  **` |
|        - |  1666 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1667 | `  */` |
|    42564 |  1668 | `	nInstr += VM_STACK_GUARD;` |
|    42564 |  1669 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    42564 |  1670 | `	if( pStack == 0 ){` |
|      ! 0 |  1671 | `		return 0;` |
|        - |  1672 | `	}` |
|        - |  1673 | `	/* Initialize the operand stack */` |
|  2931112 |  1674 | `	while( nInstr > 0 ){` |
|  2888550 |  1675 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2888550 |  1676 | `		--nInstr;` |
|        2 |  1677 | `	}` |
|        - |  1678 | `	/* Ready for bytecode execution */` |
|    42564 |  1679 | `	return pStack;` |
|    21283 |  1680 |  |
|        - |  1681 | `/* Forward declaration */` |
|        - |  1682 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1683 | `/*` |
|        - |  1684 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1685 | ` * This routine gets called by the PH7 engine after` |
|        - |  1686 | ` * successful compilation of the target PHP program.` |
|        - |  1687 | ` */` |
|     2692 |  1688 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1689 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1690 | `	)` |
|        2 |  1691 |  |
|        - |  1692 | `	SyHashEntry *pEntry;` |
|        - |  1693 | `	sxi32 rc;` |
|     2694 |  1694 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1695 | `		/* Initialize your VM first */` |
|      ! 0 |  1696 | `		return SXERR_CORRUPT;` |
|        - |  1697 | `	}` |
|        - |  1698 | `	/* Mark the VM ready for byte-code execution */` |
|     2694 |  1699 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1700 | `	/* Release the code generator now we have compiled our program */` |
|     2694 |  1701 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1702 | `	/* Emit the DONE instruction */` |
|     2694 |  1703 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2694 |  1704 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1705 | `		return SXERR_MEM;` |
|        - |  1706 | `	}` |
|        - |  1707 | `	/* Script return value */` |
|     2694 |  1708 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1709 | `	/* Allocate a new operand stack */` |
|     2694 |  1710 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2694 |  1711 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1712 | `		return SXERR_MEM;` |
|        - |  1713 | `	}` |
|        - |  1714 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1715 | `	 * private data. */` |
|     2694 |  1716 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2694 |  1717 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1718 | `	/* Allocate the reference table */` |
|     2694 |  1719 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2694 |  1720 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2694 |  1721 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1722 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1723 | `		return SXERR_MEM;` |
|        - |  1724 | `	}` |
|        - |  1725 | `	/* Zero the reference table */` |
|     2694 |  1726 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1727 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2694 |  1728 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2694 |  1729 | `	if( rc != SXRET_OK ){` |
|        - |  1730 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1731 | `		return rc;` |
|        - |  1732 | `	}` |
|        - |  1733 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2694 |  1734 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2694 |  1735 | `	if( rc != SXRET_OK ){` |
|        - |  1736 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1737 | `		return rc;` |
|        - |  1738 | `	}` |
|        - |  1739 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2694 |  1740 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1741 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2694 |  1742 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1743 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2694 |  1744 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1745 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1746 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2694 |  1747 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2694 |  1748 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1749 | `#endif` |
|        - |  1750 | `	/* Initialize and install static and constants class attributes */` |
|     2694 |  1751 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    51428 |  1752 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    48736 |  1753 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    48736 |  1754 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1755 | `			return rc;` |
|        - |  1756 | `		}` |
|        2 |  1757 | `	}` |
|        - |  1758 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2694 |  1759 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1760 | `	/* VM is ready for bytecode execution */` |
|     2694 |  1761 | `	return SXRET_OK;` |
|     1348 |  1762 |  |
|        - |  1763 | `/*` |
|        - |  1764 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1765 | ` */` |
|      ! 0 |  1766 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1767 |  |
|      ! 0 |  1768 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1769 | `		return SXERR_CORRUPT;` |
|        - |  1770 | `	}` |
|        - |  1771 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1772 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1773 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1774 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1775 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1776 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1777 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1778 | `	pVm->bHttpContext = 0;` |
|        - |  1779 | `	/* Set the ready flag */` |
|      ! 0 |  1780 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1781 | `	return SXRET_OK;` |
|      ! 0 |  1782 |  |
|        - |  1783 | `/*` |
|        - |  1784 | ` * Release a Virtual Machine.` |
|        - |  1785 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1786 | ` */` |
|     2684 |  1787 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1788 |  |
|        - |  1789 | `	/* Set the stale magic number */` |
|     2686 |  1790 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1791 | `	/* Release the private memory subsystem */` |
|     2686 |  1792 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2686 |  1793 | `	return SXRET_OK;` |
|        2 |  1794 |  |
|        - |  1795 | `/*` |
|        - |  1796 | ` * Initialize a foreign function call context.` |
|        - |  1797 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1798 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1799 | ` * functions.` |
|        - |  1800 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1801 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1802 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1803 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1804 | ` */` |
|   675434 |  1805 | `static sxi32 VmInitCallContext(` |
|        - |  1806 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1807 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1808 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1809 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1810 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1811 | `	)` |
|        2 |  1812 |  |
|   675436 |  1813 | `	pOut->pFunc = pFunc;` |
|   675436 |  1814 | `	pOut->pVm   = pVm;` |
|   675436 |  1815 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   675436 |  1816 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1817 | `	/* Assume a null return value */` |
|   675436 |  1818 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   675436 |  1819 | `	pOut->pRet = pRet;` |
|   675436 |  1820 | `	pOut->iFlags = iFlags;` |
|   675436 |  1821 | `	return SXRET_OK;` |
|        2 |  1822 |  |
|        - |  1823 | `/*` |
|        - |  1824 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1825 | ` * left behind.` |
|        - |  1826 | ` */` |
|   675434 |  1827 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1828 |  |
|        - |  1829 | `	sxu32 n;` |
|   675436 |  1830 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     8270 |  1831 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    24090 |  1832 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    15822 |  1833 | `			if( apObj[n] == 0 ){` |
|        - |  1834 | `				/* Already released */` |
|      318 |  1835 | `				continue;` |
|        - |  1836 | `			}` |
|    15506 |  1837 | `			PH7_MemObjRelease(apObj[n]);` |
|    15506 |  1838 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     7754 |  1839 | `		}` |
|     8270 |  1840 | `		SySetRelease(&pCtx->sVar);` |
|     4134 |  1841 | `	}` |
|   675436 |  1842 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1843 | `		ph7_aux_data *aAux;` |
|        - |  1844 | `		void *pChunk;` |
|        - |  1845 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1846 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1847 | `		 */` |
|        9 |  1848 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1849 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1850 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1851 | `			/* Release the chunk */` |
|       25 |  1852 | `			if( pChunk ){` |
|       25 |  1853 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1854 | `			}` |
|       13 |  1855 | `		}` |
|        9 |  1856 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1857 | `	}` |
|   675436 |  1858 |  |
|        - |  1859 | `/*` |
|        - |  1860 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1861 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1862 | ` */` |
|      316 |  1863 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1864 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1865 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1866 | `	)` |
|        2 |  1867 |  |
|      318 |  1868 | `	if( pValue == 0 ){` |
|        - |  1869 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1870 | `		return;` |
|        - |  1871 | `	}` |
|      318 |  1872 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      318 |  1873 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1874 | `		sxu32 n;` |
|     1116 |  1875 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1116 |  1876 | `			if( apObj[n] == pValue ){` |
|      318 |  1877 | `				PH7_MemObjRelease(pValue);` |
|      318 |  1878 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1879 | `				/* Mark as released */` |
|      318 |  1880 | `				apObj[n] = 0;` |
|      318 |  1881 | `				break;` |
|        - |  1882 | `			}` |
|      401 |  1883 | `		}` |
|      158 |  1884 | `	}` |
|      160 |  1885 |  |
|        - |  1886 | `/*` |
|        - |  1887 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1888 | ` */` |
|  3853840 |  1889 | `static void VmPopOperand(` |
|        - |  1890 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1891 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1892 | `	)` |
|        2 |  1893 |  |
|  3853842 |  1894 | `	ph7_value *pTos = *ppTos;` |
|  8204372 |  1895 | `	while( nPop > 0 ){` |
|  4350532 |  1896 | `		PH7_MemObjRelease(pTos);` |
|  4350532 |  1897 | `		pTos--;` |
|  4350532 |  1898 | `		nPop--;` |
|        2 |  1899 | `	}` |
|        - |  1900 | `	/* Top of the stack */` |
|  3853842 |  1901 | `	*ppTos = pTos;` |
|  3853842 |  1902 |  |
|        - |  1903 | `/*` |
|        - |  1904 | ` * Reserve a memory object.` |
|        - |  1905 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1906 | ` */` |
|  3164722 |  1907 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1908 |  |
|  3164724 |  1909 | `	ph7_value *pObj = 0;` |
|        - |  1910 | `	VmSlot *pSlot;` |
|        - |  1911 | `	sxu32 nIdx;` |
|        - |  1912 | `	/* Check for a free slot */` |
|  3164724 |  1913 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3164724 |  1914 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3164724 |  1915 | `	if( pSlot ){` |
|  1015136 |  1916 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  1015136 |  1917 | `		nIdx = pSlot->nIdx;` |
|   507567 |  1918 | `	}` |
|  3164724 |  1919 | `	if( pObj == 0 ){` |
|        - |  1920 | `		/* Reserve a new memory object */` |
|  2149590 |  1921 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2149590 |  1922 | `		if( pObj == 0 ){` |
|      ! 0 |  1923 | `			return 0;` |
|        - |  1924 | `		}` |
|  1074794 |  1925 | `	}` |
|        - |  1926 | `	/* Set a null default value */` |
|  3164724 |  1927 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3164724 |  1928 | `	pObj->nIdx = nIdx;` |
|  3164724 |  1929 | `	return pObj;` |
|  1582363 |  1930 |  |
|        - |  1931 | `/*` |
|        - |  1932 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1933 | ` */` |
|    34548 |  1934 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1935 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1936 | `	const char *zKey,  /* Entry key */` |
|        - |  1937 | `	sxu32 nByte,       /* Key length */` |
|        - |  1938 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1939 | `	)` |
|        2 |  1940 |  |
|        - |  1941 | `	ph7_value sKey;` |
|        - |  1942 | `	sxi32 rc;` |
|    34550 |  1943 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    34550 |  1944 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1945 | `	/* Perform the insertion */` |
|    34550 |  1946 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    34550 |  1947 | `	PH7_MemObjRelease(&sKey);` |
|    34550 |  1948 | `	return rc;` |
|        2 |  1949 |  |
|        - |  1950 | `/*` |
|        - |  1951 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1952 | ` * Return a pointer to the variable value on success.` |
|        - |  1953 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1954 | ` */` |
|  3585266 |  1955 | `static ph7_value * VmExtractMemObj(` |
|        - |  1956 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1957 | `	const SyString *pName, /* Variable name */` |
|        - |  1958 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1959 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1960 | `	)` |
|        2 |  1961 |  |
|  3585268 |  1962 | `	int bNullify = FALSE;` |
|        - |  1963 | `	SyHashEntry *pEntry;` |
|        - |  1964 | `	VmFrame *pFrame;` |
|        - |  1965 | `	ph7_value *pObj;` |
|        - |  1966 | `	sxu32 nIdx;` |
|        - |  1967 | `	sxi32 rc;` |
|        - |  1968 | `	/* Point to the top active frame */` |
|  3585268 |  1969 | `	pFrame = pVm->pFrame;` |
|  3585268 |  1970 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1971 | `	/* Perform the lookup */` |
|  3585268 |  1972 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1973 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1974 | `		pName = &sAnnon;` |
|        - |  1975 | `		/* Always nullify the object */` |
|      ! 0 |  1976 | `		bNullify = TRUE;` |
|      ! 0 |  1977 | `		bDup = FALSE;` |
|      ! 0 |  1978 | `	}` |
|        - |  1979 | `	/* Check the superglobals table first */` |
|  3585268 |  1980 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3585268 |  1981 | `	if( pEntry == 0 ){` |
|        - |  1982 | `		/* Query the top active frame */` |
|  3585228 |  1983 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3585228 |  1984 | `		if( pEntry == 0 ){` |
|   109108 |  1985 | `			char *zName = (char *)pName->zString;` |
|        - |  1986 | `			VmSlot sLocal;` |
|   109108 |  1987 | `			if( !bCreate ){` |
|        - |  1988 | `				/* Do not create the variable,return NULL instead */` |
|      122 |  1989 | `				return 0;` |
|        - |  1990 | `			}` |
|        - |  1991 | `			/* No such variable,automatically create a new one and install` |
|        - |  1992 | `			 * it in the current frame.` |
|        - |  1993 | `			 */` |
|   108988 |  1994 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   108988 |  1995 | `			if( pObj == 0 ){` |
|      ! 0 |  1996 | `				return 0;` |
|        - |  1997 | `			}` |
|   108988 |  1998 | `			nIdx = pObj->nIdx;` |
|   108988 |  1999 | `			if( bDup ){` |
|        - |  2000 | `				/* Duplicate name */` |
|      196 |  2001 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      196 |  2002 | `				if( zName == 0 ){` |
|      ! 0 |  2003 | `					return 0;` |
|        - |  2004 | `				}` |
|       97 |  2005 | `			}` |
|        - |  2006 | `			/* Link to the top active VM frame */` |
|   108988 |  2007 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   108988 |  2008 | `			if( rc != SXRET_OK ){` |
|        - |  2009 | `				/* Return the slot to the free pool */` |
|      ! 0 |  2010 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  2011 | `				sLocal.pUserData = 0;` |
|      ! 0 |  2012 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  2013 | `				return 0;` |
|        - |  2014 | `			}` |
|   108988 |  2015 | `			if( pFrame->pParent != 0 ){` |
|        - |  2016 | `				/* Local variable */` |
|   101388 |  2017 | `				sLocal.nIdx = nIdx;` |
|   101388 |  2018 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    50695 |  2019 | `			}else{` |
|        - |  2020 | `				/* Register in the $GLOBALS array */` |
|     7602 |  2021 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  2022 | `			}` |
|        - |  2023 | `			/* Install in the reference table */` |
|   108988 |  2024 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  2025 | `			/* Save object index */` |
|   108988 |  2026 | `			pObj->nIdx = nIdx;` |
|    54495 |  2027 | `		}else{` |
|        - |  2028 | `			/* Extract variable contents */` |
|  3476122 |  2029 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3476122 |  2030 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3476122 |  2031 | `			if( bNullify && pObj ){` |
|      ! 0 |  2032 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  2033 | `			}` |
|        - |  2034 | `		}` |
|  1792665 |  2035 | `	}else{` |
|        - |  2036 | `		/* Superglobal */` |
|       42 |  2037 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  2038 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  2039 | `	}` |
|  3585148 |  2040 | `	return pObj;` |
|  1792745 |  2041 |  |
|        - |  2042 | `/*` |
|        - |  2043 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  2044 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  2045 | ` */` |
|     2996 |  2046 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  2047 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  2048 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  2049 | `	sxu32 nByte        /* zName length */` |
|        - |  2050 | `	)` |
|        2 |  2051 |  |
|        - |  2052 | `	SyHashEntry *pEntry;` |
|        - |  2053 | `	ph7_value *pValue;` |
|        - |  2054 | `	sxu32 nIdx;` |
|        - |  2055 | `	/* Query the superglobal table */` |
|     2998 |  2056 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2998 |  2057 | `	if( pEntry == 0 ){` |
|        - |  2058 | `		/* No such entry */` |
|      ! 0 |  2059 | `		return 0;` |
|        - |  2060 | `	}` |
|        - |  2061 | `	/* Extract the superglobal index in the global object pool */` |
|     2998 |  2062 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2063 | `	/* Extract the variable value  */` |
|     2998 |  2064 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2998 |  2065 | `	return pValue;` |
|     1500 |  2066 |  |
|        - |  2067 | `/*` |
|        - |  2068 | ` * Perform a raw hashmap insertion.` |
|        - |  2069 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  2070 | ` */` |
|     3026 |  2071 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  2072 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  2073 | `	const char *zKey,   /* Entry key */` |
|        - |  2074 | `	int nKeylen,        /* zKey length*/` |
|        - |  2075 | `	const char *zData,  /* Entry data */` |
|        - |  2076 | `	int nLen            /* zData length */` |
|        - |  2077 | `	)` |
|        2 |  2078 |  |
|        - |  2079 | `	ph7_value sKey,sValue;` |
|        - |  2080 | `	sxi32 rc;` |
|     3028 |  2081 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     3028 |  2082 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     3028 |  2083 | `	if( zKey ){` |
|     3006 |  2084 | `		if( nKeylen < 0 ){` |
|     2954 |  2085 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1476 |  2086 | `		}` |
|     3006 |  2087 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1502 |  2088 | `	}` |
|     3028 |  2089 | `	if( zData ){` |
|     3028 |  2090 | `		if( nLen < 0 ){` |
|        - |  2091 | `			/* Compute length automatically */` |
|      144 |  2092 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  2093 | `		}` |
|     3028 |  2094 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1513 |  2095 | `	}` |
|        - |  2096 | `	/* Perform the insertion */` |
|     3028 |  2097 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     3028 |  2098 | `	PH7_MemObjRelease(&sKey);` |
|     3028 |  2099 | `	PH7_MemObjRelease(&sValue);` |
|     3028 |  2100 | `	return rc;` |
|        2 |  2101 |  |
|        - |  2102 | `/*` |
|        - |  2103 | ` * Configure a working virtual machine instance.` |
|        - |  2104 | ` *` |
|        - |  2105 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  2106 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  2107 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  2108 | ` * The second argument to this function is an integer configuration option` |
|        - |  2109 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  2110 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  2111 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  2112 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  2113 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  2114 | ` */` |
|    43402 |  2115 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  2116 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  2117 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  2118 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  2119 | `	)` |
|        2 |  2120 |  |
|    43404 |  2121 | `	sxi32 rc = SXRET_OK;` |
|    43404 |  2122 | `	switch(nOp){` |
|     1338 |  2123 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2678 |  2124 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2678 |  2125 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  2126 | `		/* VM output consumer callback */` |
|        - |  2127 | `#ifdef UNTRUST` |
|        - |  2128 | `		if( xConsumer == 0 ){` |
|        - |  2129 | `			rc = SXERR_CORRUPT;` |
|        - |  2130 | `			break;` |
|        - |  2131 | `		}` |
|        - |  2132 | `#endif` |
|        - |  2133 | `		/* Install the output consumer */` |
|     2678 |  2134 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2678 |  2135 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2678 |  2136 | `		break;` |
|        - |  2137 | `							   }` |
|     1346 |  2138 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2139 | `		/* Import path */` |
|        - |  2140 | `		  const char *zPath;` |
|        - |  2141 | `		  SyString sPath;` |
|     2694 |  2142 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2143 | `#if defined(UNTRUST)` |
|        - |  2144 | `		  if( zPath == 0 ){` |
|        - |  2145 | `			  rc = SXERR_EMPTY;` |
|        - |  2146 | `			  break;` |
|        - |  2147 | `		  }` |
|        - |  2148 | `#endif` |
|     2694 |  2149 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2150 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2151 | `#ifdef __WINNT__` |
|        2 |  2152 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2153 | `#endif` |
|     5386 |  2154 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2155 | `		  /* Remove leading and trailing white spaces */` |
|     2694 |  2156 | `		  SyStringFullTrim(&sPath);` |
|     2694 |  2157 | `		  if( sPath.nByte > 0 ){` |
|        - |  2158 | `			  /* Store the path in the corresponding conatiner */` |
|     2694 |  2159 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1346 |  2160 | `		  }` |
|     2694 |  2161 | `		  break;` |
|        - |  2162 | `									 }` |
|     1346 |  2163 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2164 | `		/* Run-Time Error report */` |
|     2694 |  2165 | `		pVm->bErrReport = 1;` |
|     2694 |  2166 | `		break;` |
|      ! 0 |  2167 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2168 | `		/* Recursion depth */` |
|      ! 0 |  2169 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2170 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2171 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2172 | `		}` |
|      ! 0 |  2173 | `		break;` |
|        - |  2174 | `									   }` |
|      ! 0 |  2175 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2176 | `		/* VM output length in bytes */` |
|      ! 0 |  2177 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2178 | `#ifdef UNTRUST` |
|        - |  2179 | `		if( pOut == 0 ){` |
|        - |  2180 | `			rc = SXERR_CORRUPT;` |
|        - |  2181 | `			break;` |
|        - |  2182 | `		}` |
|        - |  2183 | `#endif` |
|      ! 0 |  2184 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2185 | `		break;` |
|        - |  2186 | `							   }` |
|        - |  2187 |  |
|    13460 |  2188 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2189 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2190 | `		/* Create a new superglobal/global variable */` |
|    26922 |  2191 | `		const char *zName = va_arg(ap,const char *);` |
|    26922 |  2192 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2193 | `		SyHashEntry *pEntry;` |
|        - |  2194 | `		ph7_value *pObj;` |
|        - |  2195 | `		sxu32 nByte;` |
|        - |  2196 | `		sxu32 nIdx;` |
|        - |  2197 | `#ifdef UNTRUST` |
|        - |  2198 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2199 | `			rc = SXERR_CORRUPT;` |
|        - |  2200 | `			break;` |
|        - |  2201 | `		}` |
|        - |  2202 | `#endif` |
|    26922 |  2203 | `		nByte = SyStrlen(zName);` |
|    26922 |  2204 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2205 | `			/* Check if the superglobal is already installed */` |
|    26922 |  2206 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    13462 |  2207 | `		}else{` |
|        - |  2208 | `			/* Query the top active VM frame */` |
|      ! 0 |  2209 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2210 | `		}` |
|    26922 |  2211 | `		if( pEntry ){` |
|        - |  2212 | `			/* Variable already installed */` |
|      ! 0 |  2213 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2214 | `			/* Extract contents */` |
|      ! 0 |  2215 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2216 | `			if( pObj ){` |
|        - |  2217 | `				/* Overwrite old contents */` |
|      ! 0 |  2218 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2219 | `			}` |
|      ! 0 |  2220 | `		}else{` |
|        - |  2221 | `			/* Install a new variable */` |
|    26922 |  2222 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    26922 |  2223 | `			if( pObj == 0 ){` |
|      ! 0 |  2224 | `				rc = SXERR_MEM;` |
|      ! 0 |  2225 | `				break;` |
|        - |  2226 | `			}` |
|    26922 |  2227 | `			nIdx = pObj->nIdx;` |
|        - |  2228 | `			/* Copy value */` |
|    26922 |  2229 | `			PH7_MemObjStore(pValue,pObj);` |
|    26922 |  2230 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2231 | `				/* Install the superglobal */` |
|    26922 |  2232 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    13462 |  2233 | `			}else{` |
|        - |  2234 | `				/* Install in the current frame */` |
|      ! 0 |  2235 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2236 | `			}` |
|    26922 |  2237 | `			if( rc == SXRET_OK ){` |
|        - |  2238 | `				SyHashEntry *pRef;` |
|    26922 |  2239 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    26922 |  2240 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    13462 |  2241 | `				}else{` |
|      ! 0 |  2242 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2243 | `				}` |
|        - |  2244 | `				/* Install in the reference table */` |
|    26922 |  2245 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    26922 |  2246 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2247 | `					/* Register in the $GLOBALS array */` |
|    26922 |  2248 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    13460 |  2249 | `				}` |
|    13460 |  2250 | `			}` |
|        - |  2251 | `		}` |
|    26922 |  2252 | `		break;` |
|        - |  2253 | `									}` |
|     1476 |  2254 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2255 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2256 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2257 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2258 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2259 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2260 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2954 |  2261 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2954 |  2262 | `		const char *zValue = va_arg(ap,const char *);` |
|     2954 |  2263 | `		int nLen = va_arg(ap,int);` |
|        - |  2264 | `		ph7_hashmap *pMap;` |
|        - |  2265 | `		ph7_value *pValue;` |
|     2954 |  2266 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2267 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2268 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2953 |  2269 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2270 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2271 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2952 |  2272 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2273 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2274 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2952 |  2275 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2276 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2277 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2952 |  2278 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2279 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2280 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2952 |  2281 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2282 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2283 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2284 | `		}else{` |
|        - |  2285 | `			/* Extract the $_SERVER superglobal */` |
|     2952 |  2286 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2287 | `		}` |
|     2954 |  2288 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2289 | `			/* No such entry */` |
|      ! 0 |  2290 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2291 | `			break;` |
|        - |  2292 | `		}` |
|        - |  2293 | `		/* Point to the hashmap */` |
|     2954 |  2294 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2295 | `		/* Perform the insertion */` |
|     2954 |  2296 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2954 |  2297 | `		break;` |
|        - |  2298 | `								   }` |
|       11 |  2299 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2300 | `		/* Script arguments */` |
|       24 |  2301 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2302 | `		ph7_hashmap *pMap;` |
|        - |  2303 | `		ph7_value *pValue;` |
|        - |  2304 | `		sxu32 n;` |
|       24 |  2305 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2306 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2307 | `			break;` |
|        - |  2308 | `		}` |
|        - |  2309 | `		/* Extract the $argv array */` |
|       24 |  2310 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2311 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2312 | `			/* No such entry */` |
|      ! 0 |  2313 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2314 | `			break;` |
|        - |  2315 | `		}` |
|        - |  2316 | `		/* Point to the hashmap */` |
|       24 |  2317 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2318 | `		/* Perform the insertion */` |
|       24 |  2319 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2320 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2321 | `		if( rc == SXRET_OK ){` |
|       24 |  2322 | `			if( pMap->nEntry > 1 ){` |
|        - |  2323 | `				/* Append space separator first */` |
|       18 |  2324 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2325 | `			}` |
|       24 |  2326 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2327 | `		}` |
|       24 |  2328 | `		break;` |
|        - |  2329 | `								  }` |
|      ! 0 |  2330 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2331 | `		/* error_log() consumer */` |
|      ! 0 |  2332 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2333 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2334 | `		break;` |
|        - |  2335 | `										}` |
|      ! 0 |  2336 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2337 | `		/* Script return value */` |
|      ! 0 |  2338 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2339 | `#ifdef UNTRUST` |
|        - |  2340 | `		if( ppValue == 0 ){` |
|        - |  2341 | `			rc = SXERR_CORRUPT;` |
|        - |  2342 | `			break;` |
|        - |  2343 | `		}` |
|        - |  2344 | `#endif` |
|      ! 0 |  2345 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2346 | `		break;` |
|        - |  2347 | `								   }` |
|     2692 |  2348 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2349 | `		/* Register an IO stream device */` |
|     5386 |  2350 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2351 | `		/* Make sure we are dealing with a valid IO stream */` |
|     8076 |  2352 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5386 |  2353 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2354 | `				/* Invalid stream */` |
|      ! 0 |  2355 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2356 | `				break;` |
|        - |  2357 | `		}` |
|     5386 |  2358 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2359 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2694 |  2360 | `			pVm->pDefStream = pStream;` |
|     1346 |  2361 | `		}` |
|        - |  2362 | `		/* Insert in the appropriate container */` |
|     5386 |  2363 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5386 |  2364 | `		break;` |
|        - |  2365 | `								  }` |
|        8 |  2366 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2367 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2368 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2369 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2370 | `#ifdef UNTRUST` |
|        - |  2371 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2372 | `			rc = SXERR_CORRUPT;` |
|        - |  2373 | `			break;` |
|        - |  2374 | `		}` |
|        - |  2375 | `#endif` |
|       16 |  2376 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2377 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2378 | `		break;` |
|        - |  2379 | `									   }` |
|        8 |  2380 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2381 | `		/* Raw HTTP request*/` |
|       16 |  2382 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2383 | `		int nByte = va_arg(ap,int);` |
|       16 |  2384 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2385 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2386 | `			break;` |
|        - |  2387 | `		}` |
|       16 |  2388 | `		if( nByte < 0 ){` |
|        - |  2389 | `			/* Compute length automatically */` |
|      ! 0 |  2390 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2391 | `		}` |
|        - |  2392 | `		/* Process the request */` |
|       16 |  2393 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2394 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2395 | `		if( rc == SXRET_OK ){` |
|       16 |  2396 | `			pVm->bHttpContext = 1;` |
|        8 |  2397 | `		}` |
|       16 |  2398 | `		break;` |
|        - |  2399 | `									}` |
|        8 |  2400 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2401 | `		/* Extract HTTP response status code */` |
|       16 |  2402 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2403 | `		if( pStatus ){` |
|       16 |  2404 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2405 | `		}` |
|       16 |  2406 | `		break;` |
|        - |  2407 | `										}` |
|        8 |  2408 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2409 | `		/* Iterate response headers via callback */` |
|        - |  2410 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2411 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2412 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2413 | `		if( xCallback ){` |
|       16 |  2414 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2415 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2416 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2417 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2418 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2419 | `							   pUserData);` |
|       12 |  2420 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2421 | `					break;` |
|        - |  2422 | `				}` |
|        6 |  2423 | `			}` |
|        8 |  2424 | `		}` |
|       16 |  2425 | `		break;` |
|        - |  2426 | `										 }` |
|      ! 0 |  2427 | `	default:` |
|        - |  2428 | `		/* Unknown configuration option */` |
|      ! 0 |  2429 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2430 | `		break;` |
|        - |  2431 | `	}` |
|    43404 |  2432 | `	return rc;` |
|        2 |  2433 |  |
|        - |  2434 | `/* Forward declaration */` |
|        - |  2435 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2436 | `/*` |
|        - |  2437 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2438 | ` * format.` |
|        - |  2439 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2440 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2441 | ` * (STDOUT).` |
|        - |  2442 | ` */` |
|        2 |  2443 | `static sxi32 VmByteCodeDump(` |
|        - |  2444 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2445 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2446 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2447 | `	)` |
|        1 |  2448 |  |
|        - |  2449 | `	static const char zDump[] = {` |
|        - |  2450 | `		"====================================================\n"` |
|        - |  2451 | `		"PH7 VM Dump\n"` |
|        - |  2452 | `		"====================================================\n"` |
|        - |  2453 | `	};` |
|        - |  2454 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2455 | `	sxi32 rc = SXRET_OK;` |
|        - |  2456 | `	sxu32 n;` |
|        - |  2457 | `	/* Point to the PH7 instructions */` |
|        3 |  2458 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2459 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2460 | `	n = 0;` |
|        3 |  2461 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2462 | `	/* Dump instructions */` |
|        7 |  2463 | `	for(;;){` |
|       15 |  2464 | `		if( pInstr >= pEnd ){` |
|        - |  2465 | `			/* No more instructions */` |
|        3 |  2466 | `			break;` |
|        - |  2467 | `		}` |
|        - |  2468 | `		/* Format and call the consumer callback */` |
|       19 |  2469 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2470 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2471 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2472 | `		if( rc != SXRET_OK ){` |
|        - |  2473 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2474 | `			return rc;` |
|        - |  2475 | `		}` |
|       13 |  2476 | `		++n;` |
|       13 |  2477 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2478 | `	}` |
|        3 |  2479 | `	return rc;` |
|        2 |  2480 |  |
|        - |  2481 | `/* Forward declaration */` |
|        - |  2482 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2483 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2484 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2485 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2486 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2487 | `/*` |
|        - |  2488 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2489 | ` * consumer callback.` |
|        - |  2490 | ` */` |
|      598 |  2491 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2492 |  |
|      599 |  2493 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      599 |  2494 | `	sxi32 rc = SXRET_OK;` |
|        - |  2495 | `	/* Append a new line */` |
|        - |  2496 | `#ifdef __WINNT__` |
|        1 |  2497 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2498 | `#else` |
|      598 |  2499 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2500 | `#endif` |
|        - |  2501 | `	/* Invoke the output consumer callback */` |
|      599 |  2502 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      599 |  2503 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      599 |  2504 | `	return rc;` |
|        1 |  2505 |  |
|        - |  2506 | `/*` |
|        - |  2507 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2508 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2509 | ` * information.` |
|        - |  2510 | ` */` |
|      148 |  2511 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2512 |  |
|      150 |  2513 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2514 | `		ph7_value apArg[4];` |
|        - |  2515 | `		ph7_value *apArgPtr[4];` |
|        - |  2516 | `		ph7_value sResult;` |
|        - |  2517 | `		SyString sErr;` |
|        - |  2518 | `		/* Prepare arguments */` |
|       76 |  2519 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2520 | `			/* use explicit message length to avoid reading past buffer */` |
|       76 |  2521 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       76 |  2522 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       76 |  2523 | `		if( pFile ){` |
|       76 |  2524 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       76 |  2525 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       39 |  2526 | `		}else{` |
|      ! 0 |  2527 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2528 | `		}` |
|       76 |  2529 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       76 |  2530 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2531 | `		/* Set up pointer array */` |
|       76 |  2532 | `		apArgPtr[0] = &apArg[0];` |
|       76 |  2533 | `		apArgPtr[1] = &apArg[1];` |
|       76 |  2534 | `		apArgPtr[2] = &apArg[2];` |
|       76 |  2535 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2536 | `		/* Call the handler */` |
|       76 |  2537 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2538 | `		/* Check return value */` |
|       76 |  2539 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2540 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2541 | `		}` |
|        - |  2542 | `		/* Release */` |
|       76 |  2543 | `		PH7_MemObjRelease(&apArg[0]);` |
|       76 |  2544 | `		PH7_MemObjRelease(&apArg[1]);` |
|       76 |  2545 | `		PH7_MemObjRelease(&apArg[2]);` |
|       76 |  2546 | `		PH7_MemObjRelease(&apArg[3]);` |
|       76 |  2547 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2548 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2549 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       76 |  2550 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2551 | `	}` |
|        - |  2552 | `	/* No handler, always call error handler */` |
|       75 |  2553 | `	return TRUE;` |
|       76 |  2554 |  |
|      110 |  2555 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2556 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2557 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2558 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2559 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2560 | `	)` |
|        2 |  2561 |  |
|      112 |  2562 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2563 | `	SyString *pFile;` |
|        - |  2564 | `	char *zErr;` |
|      112 |  2565 | `	sxi32 rc = SXRET_OK;` |
|      112 |  2566 | `	if( !pVm->bErrReport ){` |
|        - |  2567 | `		/* Don't bother reporting errors */` |
|        3 |  2568 | `		return SXRET_OK;` |
|        - |  2569 | `	}` |
|        - |  2570 | `	/* Reset the working buffer */` |
|      110 |  2571 | `	SyBlobReset(pWorker);` |
|        - |  2572 | `	/* Peek the processed file if available */` |
|      110 |  2573 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      110 |  2574 | `	if( pFile ){` |
|        - |  2575 | `		/* Append file name */` |
|      110 |  2576 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|      110 |  2577 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       54 |  2578 | `	}` |
|        - |  2579 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2580 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2581 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2582 | `	 * E_DEPRECATED). */` |
|      110 |  2583 | `	zErr = "Error:  ";` |
|      110 |  2584 | `	switch(iErr){` |
|       19 |  2585 | `	case PH7_CTX_WARNING:` |
|       40 |  2586 | `		zErr = "Warning:  ";` |
|       40 |  2587 | `		break;` |
|        6 |  2588 | `	case PH7_CTX_NOTICE:` |
|       14 |  2589 | `		zErr = "Notice:  ";` |
|       12 |  2590 | `		break;` |
|       29 |  2591 | `	default:` |
|        - |  2592 | `		/* keep iErr unchanged */` |
|       58 |  2593 | `		break;` |
|        - |  2594 | `	}` |
|      110 |  2595 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|      110 |  2596 | `	if( pFuncName ){` |
|        - |  2597 | `		/* Append function name first */` |
|       23 |  2598 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2599 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2600 | `	}` |
|      110 |  2601 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2602 | `	/* Check for user error handler.  compute length of C string */` |
|      110 |  2603 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2604 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2605 | `	}` |
|      110 |  2606 | `	return rc;` |
|       57 |  2607 |  |
|        - |  2608 | `/*` |
|        - |  2609 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2610 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2611 | ` * information.` |
|        - |  2612 | ` */` |
|       40 |  2613 | `static sxi32 VmThrowErrorAp(` |
|        - |  2614 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2615 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2616 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2617 | `	const char *zFormat, /* Format message */` |
|        - |  2618 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2619 | `	)` |
|        2 |  2620 |  |
|       42 |  2621 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2622 | `	SyBlob sMsg;` |
|        - |  2623 | `	SyString *pFile;` |
|        - |  2624 | `	char *zErr;` |
|       42 |  2625 | `	sxi32 rc = SXRET_OK;` |
|       42 |  2626 | `	if( !pVm->bErrReport ){` |
|        - |  2627 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2628 | `		return SXRET_OK;` |
|        - |  2629 | `	}` |
|        - |  2630 | `	/* Reset the working buffer */` |
|       42 |  2631 | `	SyBlobReset(pWorker);` |
|        - |  2632 | `	/* Peek the processed file if available */` |
|       42 |  2633 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       42 |  2634 | `	if( pFile ){` |
|        - |  2635 | `		/* Append file name */` |
|       42 |  2636 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       42 |  2637 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       20 |  2638 | `	}` |
|        - |  2639 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2640 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2641 | `	 * the correct errno value. */` |
|       42 |  2642 | `	zErr = "Error:  ";` |
|       42 |  2643 | `	switch(iErr){` |
|        4 |  2644 | `	case PH7_CTX_WARNING:` |
|        9 |  2645 | `		zErr = "Warning:  ";` |
|        9 |  2646 | `		break;` |
|        3 |  2647 | `	case PH7_CTX_NOTICE:` |
|        7 |  2648 | `		zErr = "Notice:  ";` |
|        6 |  2649 | `		break;` |
|       13 |  2650 | `	default:` |
|        - |  2651 | `		/* do not change iErr */` |
|       26 |  2652 | `		break;` |
|        - |  2653 | `	}` |
|       42 |  2654 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       42 |  2655 | `	if( pFuncName ){` |
|        - |  2656 | `		/* Append function name first */` |
|       26 |  2657 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2658 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2659 | `	}` |
|        - |  2660 | `	/* Format the raw message */` |
|       42 |  2661 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       42 |  2662 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2663 | `	/* Check if a user error handler is installed */` |
|       42 |  2664 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2665 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2666 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2667 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2668 | `	}` |
|       42 |  2669 | `	SyBlobRelease(&sMsg);` |
|       42 |  2670 | `	return rc;` |
|       22 |  2671 |  |
|        - |  2672 | `/*` |
|        - |  2673 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  2674 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  2675 | ` * possible.` |
|        - |  2676 | ` */` |
|       38 |  2677 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        1 |  2678 |  |
|        - |  2679 | `	ph7_class *pClass;` |
|       39 |  2680 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  2681 | `	ph7_class_instance *pThis;` |
|        - |  2682 | `	ph7_class_method *pCons;` |
|        - |  2683 | `	ph7_value sArg;` |
|        - |  2684 | `	ph7_value *apArg[1];` |
|        - |  2685 | `	SyBlob sMsg;` |
|        - |  2686 | `	SyString sMsgStr;` |
|        - |  2687 | `	VmFrame *pFrame;` |
|        - |  2688 | `	sxi32 rc;` |
|       39 |  2689 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       39 |  2690 | `	if( pClass == 0 ){` |
|      ! 0 |  2691 | `		return PH7_ABORT;` |
|        - |  2692 | `	}` |
|       39 |  2693 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       39 |  2694 | `	if( pThis == 0 ){` |
|      ! 0 |  2695 | `		return PH7_ABORT;` |
|        - |  2696 | `	}` |
|       39 |  2697 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2698 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  2699 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  2700 | `	{` |
|       39 |  2701 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       39 |  2702 | `		if( pOwner ){` |
|       39 |  2703 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       19 |  2704 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       20 |  2705 | `		}else{` |
|      ! 0 |  2706 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  2707 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  2708 | `		}` |
|        - |  2709 | `	}` |
|       39 |  2710 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       39 |  2711 | `	if( pCons ){` |
|       39 |  2712 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       39 |  2713 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       39 |  2714 | `		apArg[0] = &sArg;` |
|       39 |  2715 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       39 |  2716 | `		PH7_MemObjRelease(&sArg);` |
|       19 |  2717 | `	}` |
|       39 |  2718 | `	SyBlobRelease(&sMsg);` |
|       39 |  2719 | `	pFrame = pVm->pFrame;` |
|       39 |  2720 | `	if( pFrame ){` |
|       39 |  2721 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       39 |  2722 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       19 |  2723 | `	}` |
|       39 |  2724 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       39 |  2725 | `	PH7_ClassInstanceUnref(pThis);` |
|       39 |  2726 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2727 | `		return PH7_ABORT;` |
|        - |  2728 | `	}` |
|       39 |  2729 | `	return PH7_EXCEPTION;` |
|       20 |  2730 |  |
|        - |  2731 |  |
|        - |  2732 | `/*` |
|        - |  2733 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  2734 | ` */` |
|        4 |  2735 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  2736 |  |
|        - |  2737 | `	ph7_class *pErrClass;` |
|        - |  2738 | `	ph7_class_instance *pThis;` |
|        - |  2739 | `	ph7_class_method *pCons;` |
|        - |  2740 | `	ph7_value sArg;` |
|        - |  2741 | `	ph7_value *apArg[1];` |
|        - |  2742 | `	SyBlob sMsg;` |
|        - |  2743 | `	SyString sMsgStr;` |
|        - |  2744 | `	VmFrame *pFrame;` |
|        - |  2745 | `	sxi32 rc;` |
|        5 |  2746 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        5 |  2747 | `	if( pErrClass == 0 ){` |
|      ! 0 |  2748 | `		return PH7_ABORT;` |
|        - |  2749 | `	}` |
|        5 |  2750 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        5 |  2751 | `	if( pThis == 0 ){` |
|      ! 0 |  2752 | `		return PH7_ABORT;` |
|        - |  2753 | `	}` |
|        5 |  2754 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2755 | `	{` |
|        5 |  2756 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        5 |  2757 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        5 |  2758 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  2759 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  2760 | `	}` |
|        5 |  2761 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        5 |  2762 | `	if( pCons ){` |
|        5 |  2763 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        5 |  2764 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        5 |  2765 | `		apArg[0] = &sArg;` |
|        5 |  2766 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        5 |  2767 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  2768 | `	}` |
|        5 |  2769 | `	SyBlobRelease(&sMsg);` |
|        5 |  2770 | `	pFrame = pVm->pFrame;` |
|        5 |  2771 | `	if( pFrame ){` |
|        5 |  2772 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  2773 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  2774 | `	}` |
|        5 |  2775 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        5 |  2776 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  2777 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2778 | `		return PH7_ABORT;` |
|        - |  2779 | `	}` |
|        5 |  2780 | `	return PH7_EXCEPTION;` |
|        3 |  2781 |  |
|        - |  2782 |  |
|        - |  2783 | `/*` |
|        - |  2784 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  2785 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  2786 | ` * For class types, instanceof is verified.` |
|        - |  2787 | ` *` |
|        - |  2788 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  2789 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  2790 | ` */` |
|        - |  2791 | `/*` |
|        - |  2792 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  2793 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  2794 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  2795 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  2796 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  2797 | ` */` |
|       20 |  2798 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  2799 |  |
|        - |  2800 | `	const char *z, *zEnd, *zTail;` |
|        - |  2801 | `	sxu32 n;` |
|        - |  2802 | `	sxu8 bReal;` |
|        - |  2803 | `	sxi32 rc;` |
|       22 |  2804 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2805 | `		return 0;` |
|        - |  2806 | `	}` |
|       22 |  2807 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       22 |  2808 | `	n = SyBlobLength(&pValue->sBlob);` |
|       22 |  2809 | `	zEnd = z + n;` |
|       22 |  2810 | `	if( n == 0 ){` |
|      ! 0 |  2811 | `		return 0;` |
|        - |  2812 | `	}` |
|       22 |  2813 | `	zTail = 0;` |
|       22 |  2814 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       22 |  2815 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        7 |  2816 | `		return 0;` |
|        - |  2817 | `	}` |
|        - |  2818 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       16 |  2819 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  2820 | `		zTail++;` |
|      ! 0 |  2821 | `	}` |
|       16 |  2822 | `	return zTail == zEnd ? 1 : 0;` |
|       12 |  2823 |  |
|        - |  2824 |  |
|        - |  2825 | `/*` |
|        - |  2826 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  2827 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  2828 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  2829 | ` *   0 if it's not strictly numeric.` |
|        - |  2830 | ` */` |
|       16 |  2831 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  2832 |  |
|        - |  2833 | `	const char *z, *zEnd, *zTail;` |
|        - |  2834 | `	sxu32 n;` |
|       18 |  2835 | `	sxu8 bReal = 0;` |
|        - |  2836 | `	sxi32 rc;` |
|       18 |  2837 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2838 | `		return 0;` |
|        - |  2839 | `	}` |
|       18 |  2840 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2841 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2842 | `	zEnd = z + n;` |
|       18 |  2843 | `	if( n == 0 ) return 0;` |
|       18 |  2844 | `	zTail = 0;` |
|       18 |  2845 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2846 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  2847 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  2848 | `	if( zTail != zEnd ) return 0;` |
|       15 |  2849 | `	return bReal ? 2 : 1;` |
|       10 |  2850 |  |
|        - |  2851 |  |
|        - |  2852 | `/*` |
|        - |  2853 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - |  2854 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - |  2855 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - |  2856 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - |  2857 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - |  2858 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - |  2859 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - |  2860 | ` * throw.` |
|        - |  2861 | ` *` |
|        - |  2862 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  2863 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  2864 | ` */` |
|       98 |  2865 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)` |
|        2 |  2866 |  |
|        - |  2867 | `	sxu32 i;` |
|        - |  2868 | `	ph7_type_alt *aAlts;` |
|        - |  2869 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  2870 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|      100 |  2871 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  2872 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  2873 | `	}` |
|       88 |  2874 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|       88 |  2875 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       88 |  2876 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      260 |  2877 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      174 |  2878 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      150 |  2879 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      150 |  2880 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      150 |  2881 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       76 |  2882 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       48 |  2883 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  2884 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       88 |  2885 | `	}` |
|        - |  2886 | `	/* Object handling */` |
|       88 |  2887 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       18 |  2888 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       18 |  2889 | `		if( bHasClassAlt ){` |
|       14 |  2890 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  2891 | `			ph7_class *pSelfNow = 0;` |
|       14 |  2892 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  2893 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  2894 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  2895 | `			}` |
|       26 |  2896 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  2897 | `				ph7_class *pExpected;` |
|        - |  2898 | `				SyString *pCN;` |
|       22 |  2899 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  2900 | `				pCN = &aAlts[i].sClass;` |
|       22 |  2901 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  2902 | `					pExpected = pSelfNow;` |
|       22 |  2903 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  2904 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2905 | `				}else{` |
|       22 |  2906 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  2907 | `				}` |
|       22 |  2908 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  2909 | `					return SXRET_OK;` |
|        - |  2910 | `				}` |
|        8 |  2911 | `			}` |
|        2 |  2912 | `		}` |
|        9 |  2913 | `		return SXERR_INVALID;` |
|        - |  2914 | `	}` |
|        - |  2915 | `	/* Array handling */` |
|       72 |  2916 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  2917 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  2918 | `	}` |
|        - |  2919 | `	/* Scalar handling — exact match first */` |
|       66 |  2920 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       26 |  2921 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  2922 | `	}` |
|       42 |  2923 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  2924 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  2925 | `	}` |
|       38 |  2926 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       38 |  2927 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  2928 | `	}` |
|       18 |  2929 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2930 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  2931 | `	}` |
|       18 |  2932 | `	if( bStrict ){` |
|        - |  2933 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|      ! 0 |  2934 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 |  2935 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 |  2936 | `			return SXRET_OK;` |
|        - |  2937 | `		}` |
|      ! 0 |  2938 | `		return SXERR_INVALID;` |
|        - |  2939 | `	}` |
|        - |  2940 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  2941 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  2942 | `	 * to match PHP's union RFC. */` |
|        - |  2943 | `	{` |
|       18 |  2944 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  2945 | `		if( bHasInt ){` |
|        - |  2946 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  2947 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  2948 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2949 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2950 | `				return SXRET_OK;` |
|        - |  2951 | `			}` |
|       18 |  2952 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  2953 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  2954 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  2955 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2956 | `					return SXRET_OK;` |
|        - |  2957 | `				}` |
|      ! 0 |  2958 | `			}` |
|       18 |  2959 | `			if( kind == 1 ){` |
|        9 |  2960 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  2961 | `				return SXRET_OK;` |
|        - |  2962 | `			}` |
|        4 |  2963 | `		}` |
|       10 |  2964 | `		if( bHasFloat ){` |
|       10 |  2965 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  2966 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  2967 | `				return SXRET_OK;` |
|        - |  2968 | `			}` |
|       10 |  2969 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  2970 | `				PH7_MemObjToReal(pValue);` |
|        7 |  2971 | `				return SXRET_OK;` |
|        - |  2972 | `			}` |
|        1 |  2973 | `		}` |
|        3 |  2974 | `		if( bHasString ){` |
|      ! 0 |  2975 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  2976 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  2977 | `				return SXRET_OK;` |
|        - |  2978 | `			}` |
|      ! 0 |  2979 | `		}` |
|        3 |  2980 | `		if( bHasBool ){` |
|      ! 0 |  2981 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  2982 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  2983 | `				return SXRET_OK;` |
|        - |  2984 | `			}` |
|      ! 0 |  2985 | `		}` |
|        - |  2986 | `	}` |
|        3 |  2987 | `	return SXERR_INVALID;` |
|       51 |  2988 |  |
|        - |  2989 |  |
|        - |  2990 | `/*` |
|        - |  2991 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - |  2992 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - |  2993 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - |  2994 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - |  2995 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - |  2996 | ` */` |
|       34 |  2997 | `static sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        2 |  2998 |  |
|       36 |  2999 | `	if( bStrict ){` |
|        - |  3000 | `		/* Only int -> float widening is allowed implicitly. */` |
|       10 |  3001 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 |  3002 | `			PH7_MemObjToReal(pVal);` |
|        3 |  3003 | `			return SXRET_OK;` |
|        - |  3004 | `		}` |
|        7 |  3005 | `		return SXERR_INVALID;` |
|        - |  3006 | `	}` |
|        - |  3007 | `	{` |
|       28 |  3008 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|       28 |  3009 | `		if( xCast ) xCast(pVal);` |
|        - |  3010 | `	}` |
|       28 |  3011 | `	return SXRET_OK;` |
|       19 |  3012 |  |
|        - |  3013 |  |
|        - |  3014 | `/*` |
|        - |  3015 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - |  3016 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - |  3017 | ` *` |
|        - |  3018 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - |  3019 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - |  3020 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - |  3021 | ` * back to a static literal and ignore zBuf entirely.` |
|        - |  3022 | ` */` |
|        8 |  3023 | `static const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        1 |  3024 |  |
|        9 |  3025 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|        9 |  3026 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|        9 |  3027 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|        9 |  3028 | `		if( pDeclared->zString && nCopy > 0 ){` |
|        9 |  3029 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|        4 |  3030 | `		}` |
|        9 |  3031 | `		zBuf[nCopy] = 0;` |
|        9 |  3032 | `		return zBuf;` |
|        - |  3033 | `	}` |
|      ! 0 |  3034 | `	switch( nType ){` |
|      ! 0 |  3035 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 |  3036 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 |  3037 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 |  3038 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 |  3039 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 |  3040 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 |  3041 | `		default:             return "scalar";` |
|        - |  3042 | `	}` |
|        5 |  3043 |  |
|        - |  3044 |  |
|        - |  3045 | `/*` |
|        - |  3046 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  3047 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  3048 | ` */` |
|       18 |  3049 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        1 |  3050 |  |
|       19 |  3051 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       28 |  3052 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       18 |  3053 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       19 |  3054 | `	return zBuf;` |
|        1 |  3055 |  |
|        - |  3056 |  |
|    13806 |  3057 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  3058 |  |
|        - |  3059 | `	SyHashEntry *pSlot;` |
|        - |  3060 | `	VmClassAttr *pVmAttr;` |
|        - |  3061 | `	ph7_class_attr *pAttr;` |
|    13808 |  3062 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    13808 |  3063 | `	if( pSlot == 0 ){` |
|    13606 |  3064 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  3065 | `	}` |
|      204 |  3066 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      204 |  3067 | `	pAttr = pVmAttr->pAttr;` |
|      204 |  3068 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  3069 | `		return SXRET_OK;` |
|        - |  3070 | `	}` |
|        - |  3071 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|        - |  3072 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|        - |  3073 | `	 * matching PHP's documented behavior. */` |
|      204 |  3074 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       23 |  3075 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  3076 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|        - |  3077 |  |
|       16 |  3078 | `		if( rc == SXRET_OK ){` |
|        9 |  3079 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  3080 | `			return SXRET_OK;` |
|        - |  3081 | `		}` |
|        7 |  3082 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3083 | `			char zBuf[128];` |
|        4 |  3084 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  3085 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3086 | `		}` |
|        5 |  3087 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3088 | `	}` |
|        - |  3089 | `	/* NULL handling: allowed only if the type is nullable. */` |
|      190 |  3090 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  3091 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       12 |  3092 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       12 |  3093 | `			return SXRET_OK;` |
|        - |  3094 | `		}` |
|        3 |  3095 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  3096 | `	}` |
|        - |  3097 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  3098 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  3099 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      178 |  3100 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  3101 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3102 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  3103 | `			return SXRET_OK;` |
|        - |  3104 | `		}` |
|        7 |  3105 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3106 | `	}` |
|      168 |  3107 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  3108 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  3109 | `		 * currently active on the self-stack. */` |
|       26 |  3110 | `		ph7_class *pExpected = 0;` |
|       26 |  3111 | `		SyString *pClassName = &pAttr->sClass;` |
|       26 |  3112 | `		ph7_class *pSelfNow = 0;` |
|       26 |  3113 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        3 |  3114 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        3 |  3115 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        1 |  3116 | `		}` |
|       26 |  3117 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  3118 | `			pExpected = pSelfNow;` |
|       24 |  3119 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3120 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3121 | `		}else{` |
|       22 |  3122 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3123 | `		}` |
|       26 |  3124 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3125 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3126 | `		}` |
|       26 |  3127 | `		if( pExpected ){` |
|       22 |  3128 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       22 |  3129 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  3130 | `				char zBuf[128];` |
|        7 |  3131 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  3132 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3133 | `			}` |
|        8 |  3134 | `		}` |
|       22 |  3135 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       22 |  3136 | `		return SXRET_OK;` |
|        - |  3137 | `	}` |
|        - |  3138 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  3139 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|      144 |  3140 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3141 | `		char zBuf[128];` |
|       10 |  3142 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 |  3143 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3144 | `	}` |
|      138 |  3145 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       26 |  3146 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       26 |  3147 | `		if( xCast ){` |
|        - |  3148 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       26 |  3149 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3150 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3151 | `			}` |
|       24 |  3152 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  3153 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3154 | `			}` |
|        - |  3155 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  3156 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  3157 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       26 |  3158 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       17 |  3159 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       19 |  3160 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|        9 |  3161 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  3162 | `			}` |
|       12 |  3163 | `			xCast(pValue);` |
|        5 |  3164 | `		}` |
|        5 |  3165 | `	}` |
|      124 |  3166 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      124 |  3167 | `	return SXRET_OK;` |
|     6905 |  3168 |  |
|        - |  3169 |  |
|        - |  3170 | `/*` |
|        - |  3171 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3172 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3173 | ` * information.` |
|        - |  3174 | ` * ------------------------------------` |
|        - |  3175 | ` * Simple boring wrapper function.` |
|        - |  3176 | ` * ------------------------------------` |
|        - |  3177 | ` */` |
|       16 |  3178 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  3179 |  |
|        - |  3180 | `	va_list ap;` |
|        - |  3181 | `	sxi32 rc;` |
|       17 |  3182 | `	va_start(ap,zFormat);` |
|       17 |  3183 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       17 |  3184 | `	va_end(ap);` |
|       17 |  3185 | `	return rc;` |
|        1 |  3186 |  |
|        - |  3187 | `/*` |
|        - |  3188 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  3189 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  3190 | ` */` |
|       34 |  3191 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        1 |  3192 |  |
|        - |  3193 | `	ph7_class *pClass;` |
|        - |  3194 | `	ph7_class_instance *pThis;` |
|        - |  3195 | `	ph7_class_method *pCons;` |
|        - |  3196 | `	ph7_value sArg;` |
|        - |  3197 | `	ph7_value *apArg[1];` |
|        - |  3198 | `	SyBlob sMsg;` |
|        - |  3199 | `	SyString sMsgStr;` |
|        - |  3200 | `	VmFrame *pFrame;` |
|        - |  3201 | `	sxi32 rc;` |
|       35 |  3202 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       35 |  3203 | `	if( pClass == 0 ){` |
|      ! 0 |  3204 | `		return PH7_ABORT;` |
|        - |  3205 | `	}` |
|       35 |  3206 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       35 |  3207 | `	if( pThis == 0 ){` |
|      ! 0 |  3208 | `		return PH7_ABORT;` |
|        - |  3209 | `	}` |
|       35 |  3210 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       35 |  3211 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       17 |  3212 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       35 |  3213 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       35 |  3214 | `	if( pCons ){` |
|       35 |  3215 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       35 |  3216 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       35 |  3217 | `		apArg[0] = &sArg;` |
|       35 |  3218 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       35 |  3219 | `		PH7_MemObjRelease(&sArg);` |
|       17 |  3220 | `	}` |
|       35 |  3221 | `	SyBlobRelease(&sMsg);` |
|       35 |  3222 | `	pFrame = pVm->pFrame;` |
|       35 |  3223 | `	if( pFrame ){` |
|       35 |  3224 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       35 |  3225 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       17 |  3226 | `	}` |
|       35 |  3227 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       35 |  3228 | `	PH7_ClassInstanceUnref(pThis);` |
|       35 |  3229 | `	if( rc == SXERR_ABORT ){` |
|        5 |  3230 | `		return PH7_ABORT;` |
|        - |  3231 | `	}` |
|       31 |  3232 | `	return PH7_EXCEPTION;` |
|       18 |  3233 |  |
|        - |  3234 | `/*` |
|        - |  3235 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - |  3236 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - |  3237 | ` */` |
|        6 |  3238 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|        1 |  3239 |  |
|        - |  3240 | `	ph7_class *pClass;` |
|        - |  3241 | `	ph7_class_instance *pThis;` |
|        - |  3242 | `	ph7_class_method *pCons;` |
|        - |  3243 | `	ph7_value sArg;` |
|        - |  3244 | `	ph7_value *apArg[1];` |
|        - |  3245 | `	SyBlob sMsg;` |
|        - |  3246 | `	SyString sMsgStr;` |
|        - |  3247 | `	VmFrame *pFrame;` |
|        - |  3248 | `	sxi32 rc;` |
|        7 |  3249 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|        7 |  3250 | `	if( pClass == 0 ){` |
|      ! 0 |  3251 | `		return PH7_ABORT;` |
|        - |  3252 | `	}` |
|        7 |  3253 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|        7 |  3254 | `	if( pThis == 0 ){` |
|      ! 0 |  3255 | `		return PH7_ABORT;` |
|        - |  3256 | `	}` |
|        7 |  3257 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        7 |  3258 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|        3 |  3259 | `		pFuncName,zExpected,zGiven);` |
|        7 |  3260 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        7 |  3261 | `	if( pCons ){` |
|        7 |  3262 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        7 |  3263 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        7 |  3264 | `		apArg[0] = &sArg;` |
|        7 |  3265 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        7 |  3266 | `		PH7_MemObjRelease(&sArg);` |
|        3 |  3267 | `	}` |
|        7 |  3268 | `	SyBlobRelease(&sMsg);` |
|        7 |  3269 | `	pFrame = pVm->pFrame;` |
|        7 |  3270 | `	if( pFrame ){` |
|        7 |  3271 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        7 |  3272 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        3 |  3273 | `	}` |
|        7 |  3274 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        7 |  3275 | `	PH7_ClassInstanceUnref(pThis);` |
|        7 |  3276 | `	if( rc == SXERR_ABORT ){` |
|        7 |  3277 | `		return PH7_ABORT;` |
|        - |  3278 | `	}` |
|      ! 0 |  3279 | `	return PH7_EXCEPTION;` |
|        4 |  3280 |  |
|        - |  3281 | `/*` |
|        - |  3282 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|        - |  3283 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|        - |  3284 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|        - |  3285 | ` */` |
|       14 |  3286 | `static const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|        1 |  3287 |  |
|       15 |  3288 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        5 |  3289 | `		return pVal->x.iVal ? "true" : "false";` |
|        - |  3290 | `	}` |
|       11 |  3291 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        3 |  3292 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|        3 |  3293 | `		if( pThis && pThis->pClass ){` |
|        3 |  3294 | `			SyString *pName = &pThis->pClass->sName;` |
|        3 |  3295 | `			sxu32 n = pName->nByte;` |
|        3 |  3296 | `			if( n >= nBuf ){` |
|      ! 0 |  3297 | `				n = nBuf - 1;` |
|      ! 0 |  3298 | `			}` |
|        3 |  3299 | `			SyMemcpy(pName->zString,zBuf,n);` |
|        3 |  3300 | `			zBuf[n] = 0;` |
|        3 |  3301 | `			return zBuf;` |
|        - |  3302 | `		}` |
|      ! 0 |  3303 | `		return "object";` |
|        - |  3304 | `	}` |
|        9 |  3305 | `	return ph7_type_name(pVal);` |
|        8 |  3306 |  |
|        - |  3307 | `/*` |
|        - |  3308 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|        - |  3309 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|        - |  3310 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|        - |  3311 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|        - |  3312 | ` */` |
|       14 |  3313 | `static sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|        1 |  3314 |  |
|        - |  3315 | `	ph7_class *pClass;` |
|        - |  3316 | `	ph7_class_instance *pThis;` |
|        - |  3317 | `	ph7_class_method *pCons;` |
|        - |  3318 | `	ph7_value sArg;` |
|        - |  3319 | `	ph7_value *apArg[1];` |
|        - |  3320 | `	SyBlob sMsg;` |
|        - |  3321 | `	SyString sMsgStr;` |
|        - |  3322 | `	VmFrame *pFrame;` |
|        - |  3323 | `	sxi32 rc;` |
|       15 |  3324 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|        - |  3325 | `	char zNameBuf[64];` |
|       15 |  3326 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|       15 |  3327 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|       15 |  3328 | `	if( pClass == 0 ){` |
|      ! 0 |  3329 | `		return PH7_ABORT;` |
|        - |  3330 | `	}` |
|       15 |  3331 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       15 |  3332 | `	if( pThis == 0 ){` |
|      ! 0 |  3333 | `		return PH7_ABORT;` |
|        - |  3334 | `	}` |
|       15 |  3335 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       15 |  3336 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|       15 |  3337 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       15 |  3338 | `	if( pCons ){` |
|       15 |  3339 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       15 |  3340 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       15 |  3341 | `		apArg[0] = &sArg;` |
|       15 |  3342 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       15 |  3343 | `		PH7_MemObjRelease(&sArg);` |
|        7 |  3344 | `	}` |
|       15 |  3345 | `	SyBlobRelease(&sMsg);` |
|       15 |  3346 | `	pFrame = pVm->pFrame;` |
|       15 |  3347 | `	if( pFrame ){` |
|       15 |  3348 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       15 |  3349 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        7 |  3350 | `	}` |
|       15 |  3351 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       15 |  3352 | `	PH7_ClassInstanceUnref(pThis);` |
|       15 |  3353 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3354 | `		return PH7_ABORT;` |
|        - |  3355 | `	}` |
|       15 |  3356 | `	return PH7_EXCEPTION;` |
|        8 |  3357 |  |
|        - |  3358 | `/*` |
|        - |  3359 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - |  3360 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - |  3361 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - |  3362 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - |  3363 | ` */` |
|        - |  3364 | `/*` |
|        - |  3365 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|        - |  3366 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|        - |  3367 | ` * null SyString yields an empty C string. Returns zBuf.` |
|        - |  3368 | ` */` |
|       24 |  3369 | `static const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|        2 |  3370 |  |
|        - |  3371 | `	sxu32 nCopy;` |
|       26 |  3372 | `	if( nBuf == 0 ) return "";` |
|       26 |  3373 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|      ! 0 |  3374 | `		zBuf[0] = 0;` |
|      ! 0 |  3375 | `		return zBuf;` |
|        - |  3376 | `	}` |
|       26 |  3377 | `	nCopy = SyStringLength(pStr);` |
|       26 |  3378 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       26 |  3379 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|       26 |  3380 | `	zBuf[nCopy] = 0;` |
|       26 |  3381 | `	return zBuf;` |
|       14 |  3382 |  |
|        - |  3383 |  |
|      262 |  3384 | `static sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        2 |  3385 |  |
|      264 |  3386 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|        - |  3387 | `	const char *zGiven;` |
|        - |  3388 | `	char zBuf[128];` |
|        - |  3389 | `	char zTypeBuf[128];` |
|        - |  3390 | `	/* Untyped function: no enforcement. */` |
|      264 |  3391 | `	if( pFunc->nReturnType == 0 ){` |
|      ! 0 |  3392 | `		return SXRET_OK;` |
|        - |  3393 | `	}` |
|        - |  3394 | `	/* void return type: the function must not produce a value. */` |
|      264 |  3395 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|       94 |  3396 | `		if( pValue == 0 ){` |
|       92 |  3397 | `			return SXRET_OK;` |
|        - |  3398 | `		}` |
|        - |  3399 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - |  3400 | `		 * still counts as "returned a value" here. */` |
|        3 |  3401 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|        3 |  3402 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|        - |  3403 | `	}` |
|        - |  3404 | `	/* Function fell off the end without an explicit return: PHP implicitly` |
|        - |  3405 | `	 * returns null. For a typed non-nullable return, that's a TypeError. */` |
|      172 |  3406 | `	if( pValue == 0 ){` |
|      ! 0 |  3407 | `		const char *zExpected = "value";` |
|      ! 0 |  3408 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3409 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3410 | `		}` |
|      ! 0 |  3411 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|        - |  3412 | `	}` |
|        - |  3413 | `	/* Union return type — delegate. The function has no flag for nullable` |
|        - |  3414 | `	 * unions; a null alternative is represented inside aReturnUnion, so pass` |
|        - |  3415 | `	 * bNullable=0 here. */` |
|      172 |  3416 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - |  3417 | `		sxi32 rcU;` |
|      ! 0 |  3418 | `		int bNullable = 0;` |
|      ! 0 |  3419 | `		const char *zExpected = "union";` |
|        - |  3420 | ``		/* Scan alternatives for MEMOBJ_NULL, which serves as `T\|null`. */`` |
|        - |  3421 | `		{` |
|        - |  3422 | `			sxu32 i;` |
|      ! 0 |  3423 | `			ph7_type_alt *aAlts = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  3424 | `			for( i = 0; i < SySetUsed(&pFunc->aReturnUnion); i++ ){` |
|      ! 0 |  3425 | `				if( aAlts[i].nType == MEMOBJ_NULL ){ bNullable = 1; break; }` |
|      ! 0 |  3426 | `			}` |
|        - |  3427 | `		}` |
|      ! 0 |  3428 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|      ! 0 |  3429 | `		if( rcU == SXRET_OK ){` |
|      ! 0 |  3430 | `			return SXRET_OK;` |
|        - |  3431 | `		}` |
|      ! 0 |  3432 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3433 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3434 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  3435 | `			zGiven = "null";` |
|      ! 0 |  3436 | `		}else{` |
|      ! 0 |  3437 | `			zGiven = ph7_type_name(pValue);` |
|        - |  3438 | `		}` |
|      ! 0 |  3439 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3440 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3441 | `		}` |
|      ! 0 |  3442 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3443 | `	}` |
|        - |  3444 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - |  3445 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - |  3446 | `	 * it into the TypeError message. */` |
|      172 |  3447 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|        6 |  3448 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|        - |  3449 | `		const char *zExpected;` |
|        - |  3450 | `		ph7_class *pExpected;` |
|        6 |  3451 | `		ph7_class *pSelfNow = 0;` |
|        6 |  3452 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        6 |  3453 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        6 |  3454 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        2 |  3455 | `		}` |
|        6 |  3456 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        3 |  3457 | `			pExpected = pSelfNow;` |
|        4 |  3458 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3459 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3460 | `		}else{` |
|        3 |  3461 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3462 | `		}` |
|        6 |  3463 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|        6 |  3464 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3465 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 |  3466 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3467 | `		}` |
|        6 |  3468 | `		if( pExpected ){` |
|        6 |  3469 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        6 |  3470 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  3471 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3472 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3473 | `			}` |
|        2 |  3474 | `		}` |
|        6 |  3475 | `		return SXRET_OK;` |
|        - |  3476 | `	}` |
|        - |  3477 | `	/* Scalar return type. Allow null pass-through if the function is` |
|        - |  3478 | `	 * nullable (textual "?T" gets that flag, though union+null is handled` |
|        - |  3479 | `	 * above). There's no explicit nullable flag on ph7_vm_func, so detect` |
|        - |  3480 | `	 * via the type-text leading '?'. */` |
|      168 |  3481 | `	if( (pValue->iFlags & MEMOBJ_NULL) ){` |
|        6 |  3482 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0` |
|        8 |  3483 | `		 && pFunc->sReturnTypeName.zString[0] == '?' ){` |
|        8 |  3484 | `			return SXRET_OK;` |
|        - |  3485 | `		}` |
|      ! 0 |  3486 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3487 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3488 | `			"null");` |
|        - |  3489 | `	}` |
|        - |  3490 | `	/* Exact match? Done. */` |
|      162 |  3491 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|      156 |  3492 | `		return SXRET_OK;` |
|        - |  3493 | `	}` |
|        - |  3494 | `	/* Object->scalar is never compatible. */` |
|        8 |  3495 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3496 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3497 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3498 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3499 | `			zGiven);` |
|        - |  3500 | `	}` |
|        - |  3501 | `	/* Array <-> scalar is never compatible. */` |
|        8 |  3502 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      ! 0 |  3503 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3504 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3505 | `			ph7_type_name(pValue));` |
|        - |  3506 | `	}` |
|        - |  3507 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - |  3508 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - |  3509 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - |  3510 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|        8 |  3511 | `	if( !bStrict` |
|        5 |  3512 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|        4 |  3513 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|        6 |  3514 | `	 && !VmStringIsStrictNumeric(pValue) ){` |
|        4 |  3515 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3516 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3517 | `			"string");` |
|        - |  3518 | `	}` |
|        6 |  3519 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|        3 |  3520 | `		return SXRET_OK;` |
|        - |  3521 | `	}` |
|        4 |  3522 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3523 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 |  3524 | `		ph7_type_name(pValue));` |
|      133 |  3525 |  |
|        - |  3526 | `/*` |
|        - |  3527 | ` * Report a fatal named-argument error.` |
|        - |  3528 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  3529 | ` */` |
|        6 |  3530 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        1 |  3531 |  |
|        7 |  3532 | `	const char *zFunc = 0;` |
|        7 |  3533 | `	int nFunc = 0;` |
|        7 |  3534 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        7 |  3535 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        1 |  3536 |  |
|        - |  3537 | `/*` |
|        - |  3538 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3539 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3540 | ` * information.` |
|        - |  3541 | ` * ------------------------------------` |
|        - |  3542 | ` * Simple boring wrapper function.` |
|        - |  3543 | ` * ------------------------------------` |
|        - |  3544 | ` */` |
|       24 |  3545 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  3546 |  |
|        - |  3547 | `	sxi32 rc;` |
|       26 |  3548 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  3549 | `	return rc;` |
|        2 |  3550 |  |
|        - |  3551 | `/*` |
|        - |  3552 | ` * Resolve function context from the current frame.` |
|        - |  3553 | ` */` |
|     1014 |  3554 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  3555 |  |
|        - |  3556 | `	VmFrame *pFrame;` |
|        - |  3557 | `	ph7_vm_func *pFunc;` |
|     1015 |  3558 | `	*pzFuncName = 0;` |
|     1015 |  3559 | `	*pnFuncLen = 0;` |
|     1015 |  3560 | `	pFrame = pVm->pFrame;` |
|     1015 |  3561 | `	if( pFrame == 0 ){` |
|      ! 0 |  3562 | `		return;` |
|        - |  3563 | `	}` |
|     1015 |  3564 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1015 |  3565 | `	if( pFrame->pParent == 0 ){` |
|      991 |  3566 | `		return;` |
|        - |  3567 | `	}` |
|       25 |  3568 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       25 |  3569 | `	if( pFunc == 0 ){` |
|      ! 0 |  3570 | `		return;` |
|        - |  3571 | `	}` |
|       25 |  3572 | `	*pzFuncName = pFunc->sName.zString;` |
|       25 |  3573 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      508 |  3574 |  |
|        - |  3575 | `/*` |
|        - |  3576 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  3577 | ` */` |
|      522 |  3578 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  3579 |  |
|        - |  3580 | `	SyBlob sOut;` |
|        - |  3581 | `	SyString *pFile;` |
|      523 |  3582 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  3583 | `		return PH7_OK;` |
|        - |  3584 | `	}` |
|      523 |  3585 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  3586 | `		zClass = "Exception";` |
|      ! 0 |  3587 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  3588 | `	}` |
|      523 |  3589 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      501 |  3590 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      250 |  3591 | `	}` |
|      523 |  3592 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      523 |  3593 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      523 |  3594 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      523 |  3595 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      523 |  3596 | `	if( zMsg && nMsg > 0 ){` |
|      523 |  3597 | `		SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      523 |  3598 | `		SyBlobAppend(&sOut,zMsg,nMsg);` |
|      261 |  3599 | `	}` |
|      523 |  3600 | `	if( pFile ){` |
|      523 |  3601 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      523 |  3602 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      523 |  3603 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      261 |  3604 | `	}` |
|      523 |  3605 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      523 |  3606 | `	if( pFile ){` |
|      523 |  3607 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      523 |  3608 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      523 |  3609 | `		if( zFuncName && nFuncLen > 0 ){` |
|       25 |  3610 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|       13 |  3611 | `		}else{` |
|      499 |  3612 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  3613 | `		}` |
|      261 |  3614 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  3615 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  3616 | `	}else{` |
|      ! 0 |  3617 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  3618 | `	}` |
|      523 |  3619 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      523 |  3620 | `	if( pFile ){` |
|      523 |  3621 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      523 |  3622 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      523 |  3623 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      523 |  3624 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      261 |  3625 | `	}` |
|      523 |  3626 | `	VmCallErrorHandler(pVm,&sOut);` |
|      523 |  3627 | `	SyBlobRelease(&sOut);` |
|      523 |  3628 | `	return PH7_ABORT;` |
|      262 |  3629 |  |
|        - |  3630 | `/*` |
|        - |  3631 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  3632 | ` */` |
|      568 |  3633 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  3634 |  |
|        - |  3635 | `	ph7_vm *pVm;` |
|        - |  3636 | `	ph7_class *pClass;` |
|        - |  3637 | `	ph7_class_instance *pThis;` |
|        - |  3638 | `	ph7_class_method *pCons;` |
|        - |  3639 | `	ph7_value sArg;` |
|        - |  3640 | `	ph7_value *apArg[1];` |
|        - |  3641 | `	SyBlob sMsg;` |
|        - |  3642 | `	SyString sMsgStr;` |
|        - |  3643 | `	VmFrame *pFrame;` |
|        - |  3644 | `	va_list ap;` |
|        - |  3645 | `	sxi32 rc;` |
|        - |  3646 |  |
|      570 |  3647 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3648 | `		return PH7_ABORT;` |
|        - |  3649 | `	}` |
|      570 |  3650 | `	pVm = pCtx->pVm;` |
|      570 |  3651 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3652 | `		zClass = "Error";` |
|      ! 0 |  3653 | `	}` |
|      570 |  3654 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      570 |  3655 | `	if( pClass == 0 ){` |
|      ! 0 |  3656 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3657 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  3658 | `			zClass` |
|        - |  3659 | `			);` |
|        - |  3660 | `	}` |
|      570 |  3661 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      570 |  3662 | `	if( pThis == 0 ){` |
|      ! 0 |  3663 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3664 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  3665 | `			);` |
|        - |  3666 | `	}` |
|        - |  3667 |  |
|      570 |  3668 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      570 |  3669 | `	va_start(ap,zFormat);` |
|      570 |  3670 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      570 |  3671 | `	va_end(ap);` |
|        - |  3672 |  |
|      570 |  3673 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      570 |  3674 | `	if( pCons ){` |
|      570 |  3675 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      570 |  3676 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      570 |  3677 | `		apArg[0] = &sArg;` |
|      570 |  3678 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      570 |  3679 | `		PH7_MemObjRelease(&sArg);` |
|      284 |  3680 | `	}` |
|      570 |  3681 | `	SyBlobRelease(&sMsg);` |
|        - |  3682 |  |
|      570 |  3683 | `	pFrame = pVm->pFrame;` |
|      570 |  3684 | `	if( pFrame ){` |
|      570 |  3685 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      570 |  3686 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      284 |  3687 | `	}` |
|      570 |  3688 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      570 |  3689 | `	PH7_ClassInstanceUnref(pThis);` |
|      570 |  3690 | `	if( rc == SXERR_ABORT ){` |
|      489 |  3691 | `		return PH7_ABORT;` |
|        - |  3692 | `	}` |
|       82 |  3693 | `	return PH7_EXCEPTION;` |
|      286 |  3694 |  |
|        - |  3695 | `/*` |
|        - |  3696 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  3697 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  3698 | ` */` |
|      ! 0 |  3699 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  3700 |  |
|        - |  3701 | `	ph7_vm *pVm;` |
|        - |  3702 | `	SyBlob sMsg;` |
|      ! 0 |  3703 | `	const char *zFuncName = 0;` |
|      ! 0 |  3704 | `	int nFuncLen = 0;` |
|        - |  3705 | `	va_list ap;` |
|        - |  3706 | `	sxi32 rc;` |
|        - |  3707 |  |
|      ! 0 |  3708 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3709 | `		return PH7_OK;` |
|        - |  3710 | `	}` |
|      ! 0 |  3711 | `	pVm = pCtx->pVm;` |
|      ! 0 |  3712 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3713 | `		zClass = "Error";` |
|      ! 0 |  3714 | `	}` |
|        - |  3715 |  |
|      ! 0 |  3716 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3717 |  |
|      ! 0 |  3718 | `	va_start(ap,zFormat);` |
|      ! 0 |  3719 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  3720 | `	va_end(ap);` |
|        - |  3721 |  |
|      ! 0 |  3722 | `	if( pCtx->pFunc ){` |
|      ! 0 |  3723 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  3724 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  3725 | `	}` |
|      ! 0 |  3726 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  3727 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  3728 | `	}` |
|      ! 0 |  3729 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  3730 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  3731 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  3732 | `	return rc;` |
|      ! 0 |  3733 |  |
|        - |  3734 | `/*` |
|        - |  3735 | ` * Save the execution state of a fiber/generator context.` |
|        - |  3736 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  3737 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  3738 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  3739 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  3740 | ` * when VmByteCodeExec returns.` |
|        - |  3741 | ` */` |
|      144 |  3742 | `static sxi32 VmSuspendCtx(` |
|        - |  3743 | `	ph7_vm *pVm,` |
|        - |  3744 | `	ph7_exec_ctx *pCtx,` |
|        - |  3745 | `	sxi32 pc,` |
|        - |  3746 | `	sxi32 nTos` |
|        - |  3747 | `	)` |
|        2 |  3748 |  |
|       72 |  3749 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      146 |  3750 | `	pCtx->pc = pc;` |
|      146 |  3751 | `	pCtx->nTos = nTos;` |
|      146 |  3752 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      146 |  3753 | `	return PH7_SUSPEND;` |
|        2 |  3754 |  |
|        - |  3755 | `/*` |
|        - |  3756 | ` * Resolve named-argument mapping.` |
|        - |  3757 | ` *` |
|        - |  3758 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  3759 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  3760 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  3761 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  3762 | ` * every formal parameter that received a value.` |
|        - |  3763 | ` *` |
|        - |  3764 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  3765 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  3766 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  3767 | ` */` |
|       98 |  3768 | `static sxi32 VmResolveNamedArgs(` |
|        - |  3769 | `	ph7_vm *pVm,` |
|        - |  3770 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  3771 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  3772 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  3773 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  3774 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  3775 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  3776 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  3777 |  |
|        2 |  3778 |  |
|      100 |  3779 | `	sxi32 posIdx = 0;` |
|        - |  3780 | `	sxu32 i;` |
|        - |  3781 | `	char zErrMsg[256];` |
|      100 |  3782 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      296 |  3783 | `	for( i = 0; i < nActual; i++ ){` |
|      198 |  3784 | `		aSlot[i] = -2;` |
|      100 |  3785 | `	}` |
|      290 |  3786 | `	for( i = 0; i < nActual; i++ ){` |
|      286 |  3787 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  3788 | `			/* Named argument — find formal by name */` |
|      184 |  3789 | `			int found = 0;` |
|        - |  3790 | `			sxu32 k;` |
|      304 |  3791 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      290 |  3792 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      281 |  3793 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      268 |  3794 | `						pMap->aNames[i].zString,` |
|      402 |  3795 | `						pMap->aNames[i].nByte) == 0 ){` |
|      172 |  3796 | `					if( aUsed[k] ){` |
|        7 |  3797 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3798 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  3799 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        5 |  3800 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        5 |  3801 | `						return PH7_ABORT;` |
|        - |  3802 | `					}` |
|      168 |  3803 | `					aSlot[i] = (sxi32)k;` |
|      168 |  3804 | `					aUsed[k] = 1;` |
|      168 |  3805 | `					found = 1;` |
|      168 |  3806 | `					break;` |
|        - |  3807 | `				}` |
|       62 |  3808 | `			}` |
|      180 |  3809 | `			if( !found ){` |
|       14 |  3810 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  3811 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  3812 | `				}else{` |
|        4 |  3813 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3814 | `						"Unknown named parameter $%.*s",` |
|        2 |  3815 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  3816 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  3817 | `					return PH7_ABORT;` |
|        - |  3818 | `				}` |
|        5 |  3819 | `			}` |
|       90 |  3820 | `		}else{` |
|        - |  3821 | `			/* Positional argument */` |
|       16 |  3822 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       16 |  3823 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  3824 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3825 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  3826 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  3827 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  3828 | `					return PH7_ABORT;` |
|        - |  3829 | `				}` |
|       16 |  3830 | `				aSlot[i] = posIdx;` |
|       16 |  3831 | `				aUsed[posIdx] = 1;` |
|        7 |  3832 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  3833 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  3834 | `			}` |
|       16 |  3835 | `			posIdx++;` |
|        - |  3836 | `		}` |
|       97 |  3837 | `	}` |
|       93 |  3838 | `	return SXRET_OK;` |
|       51 |  3839 |  |
|        - |  3840 | `/*` |
|        - |  3841 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  3842 | ` *` |
|        - |  3843 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  3844 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  3845 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  3846 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  3847 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  3848 | ` * then the program execution is halted.` |
|        - |  3849 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  3850 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  3851 | ` * or to reset the VM to it's initial state.` |
|        - |  3852 | ` */` |
|    42660 |  3853 | `static sxi32 VmByteCodeExec(` |
|        - |  3854 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3855 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  3856 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  3857 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  3858 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  3859 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  3860 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  3861 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  3862 | `	ph7_vm_func *pEnforceRetFunc /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  3863 | `	)` |
|        2 |  3864 |  |
|        - |  3865 | `	VmInstr *pInstr;` |
|        - |  3866 | `	ph7_value *pTos;` |
|        - |  3867 | `	SySet aArg;` |
|        - |  3868 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  3869 | `	sxi32 pc;` |
|        - |  3870 | `	sxi32 rc;` |
|        - |  3871 | `	/* Argument container */` |
|    42662 |  3872 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    42662 |  3873 | `	if( nTos < 0 ){` |
|    39910 |  3874 | `		pTos = &pStack[-1];` |
|    19956 |  3875 | `	}else{` |
|     2754 |  3876 | `		pTos = &pStack[nTos];` |
|        - |  3877 | `	}` |
|    42662 |  3878 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    42662 |  3879 | `	pc = nPc;` |
|        - |  3880 | `/*` |
|        - |  3881 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  3882 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  3883 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  3884 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  3885 | ` */` |
|        - |  3886 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  3887 | `	{ \` |
|        - |  3888 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  3889 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  3890 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  3891 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  3892 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  3893 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  3894 | `				break; \` |
|        - |  3895 | `			} \` |
|        - |  3896 | `			goto Exception; \` |
|        - |  3897 | `		} \` |
|        - |  3898 | `	}` |
|        - |  3899 | `	/* Execute as much as we can */` |
|  5764375 |  3900 | `	for(;;){` |
|        - |  3901 | `		/* Fetch the instruction to execute */` |
| 11528048 |  3902 | `		pInstr = &aInstr[pc];` |
| 11528048 |  3903 | `		rc = SXRET_OK;` |
|        - |  3904 | `/*` |
|        - |  3905 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  3906 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  3907 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  3908 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  3909 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  3910 | ` */` |
| 11528048 |  3911 | `		switch(pInstr->iOp){` |
|        - |  3912 | `/*` |
|        - |  3913 | ` * DONE: P1 * *` |
|        - |  3914 | ` *` |
|        - |  3915 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  3916 | ` * and return immediately.` |
|        - |  3917 | ` */` |
|    20979 |  3918 | `case PH7_OP_DONE:` |
|        - |  3919 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  3920 | `	 * the fiber start/resume paths) set pEnforceRetFunc, so this branch is` |
|        - |  3921 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  3922 | `	 * callback trampolines, and the main script. */` |
|    41960 |  3923 | `	if( pEnforceRetFunc && pEnforceRetFunc->nReturnType > 0 ){` |
|      264 |  3924 | `		ph7_value *pRetVal = 0;` |
|      264 |  3925 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|      174 |  3926 | `			pRetVal = pTos;` |
|       86 |  3927 | `		}` |
|      264 |  3928 | `		rc = VmEnforceReturnType(&(*pVm), pEnforceRetFunc, pRetVal);` |
|      264 |  3929 | `		if( rc == PH7_ABORT ) goto Abort;` |
|      258 |  3930 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  3931 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      ! 0 |  3932 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3933 | `				pTos--;` |
|      ! 0 |  3934 | `			}` |
|      ! 0 |  3935 | `			goto Exception;` |
|        - |  3936 | `		}` |
|        - |  3937 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  3938 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  3939 | `		 * defensively we clear the pointer after a successful check). */` |
|      258 |  3940 | `		pEnforceRetFunc = 0;` |
|      128 |  3941 | `	}` |
|    41954 |  3942 | `	if( pInstr->iP1 ){` |
|        - |  3943 | `#ifdef UNTRUST` |
|        - |  3944 | `		if( pTos < pStack ){` |
|        - |  3945 | `			goto Abort;` |
|        - |  3946 | `		}` |
|        - |  3947 | `#endif` |
|    25390 |  3948 | `		if( pLastRef ){` |
|    15690 |  3949 | `			*pLastRef = pTos->nIdx;` |
|     7844 |  3950 | `		}` |
|    25390 |  3951 | `		if( pResult ){` |
|        - |  3952 | `			/* Execution result */` |
|    24032 |  3953 | `			PH7_MemObjStore(pTos,pResult);` |
|    12015 |  3954 | `		}` |
|    25390 |  3955 | `		VmPopOperand(&pTos,1);` |
|    29260 |  3956 | `	}else if( pLastRef ){` |
|        - |  3957 | `		/* Nothing referenced */` |
|     1732 |  3958 | `		*pLastRef = SXU32_HIGH;` |
|      865 |  3959 | `	}` |
|        - |  3960 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  3961 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  3962 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  3963 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  3964 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  3965 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  3966 | `	 * block can override it.` |
|        - |  3967 | `	 */` |
|    41956 |  3968 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  3969 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  3970 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  3971 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  3972 | `		pExc->pFrame = 0;` |
|        3 |  3973 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  3974 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  3975 | `			pExc->iFinallyDone = 1;` |
|        - |  3976 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  3977 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  3978 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  3979 | `				goto Abort;` |
|        - |  3980 | `			}` |
|        1 |  3981 | `		}` |
|        1 |  3982 | `	}` |
|    41954 |  3983 | `	goto Done;` |
|        - |  3984 | `/*` |
|        - |  3985 | ` * HALT: P1 * *` |
|        - |  3986 | ` *` |
|        - |  3987 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  3988 | ` * and abort immediately.` |
|        - |  3989 | ` */` |
|        4 |  3990 | `case PH7_OP_HALT:` |
|        9 |  3991 | `	if( pInstr->iP1 ){` |
|        - |  3992 | `#ifdef UNTRUST` |
|        - |  3993 | `		if( pTos < pStack ){` |
|        - |  3994 | `			goto Abort;` |
|        - |  3995 | `		}` |
|        - |  3996 | `#endif` |
|        9 |  3997 | `		if( pLastRef ){` |
|      ! 0 |  3998 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  3999 | `		}` |
|        9 |  4000 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  4001 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  4002 | `				/* Output the exit message */` |
|        7 |  4003 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  4004 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  4005 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  4006 | `			}` |
|        7 |  4007 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  4008 | `			/* Record exit status */` |
|        5 |  4009 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  4010 | `		}` |
|        9 |  4011 | `		VmPopOperand(&pTos,1);` |
|        4 |  4012 | `	}else if( pLastRef ){` |
|        - |  4013 | `		/* Nothing referenced */` |
|      ! 0 |  4014 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  4015 | `	}` |
|        - |  4016 | `	/* Check if we're in an included file context */` |
|        9 |  4017 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  4018 | `		/* Terminate the entire process */` |
|        9 |  4019 | `		exit(pVm->iExitStatus);` |
|        - |  4020 | `	}` |
|      ! 0 |  4021 | `	goto Abort;` |
|        - |  4022 | `/*` |
|        - |  4023 | ` * JMP: * P2 *` |
|        - |  4024 | ` *` |
|        - |  4025 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  4026 | ` * the one at index P2 from the beginning of the program.` |
|        - |  4027 | ` */` |
|   246030 |  4028 | `case PH7_OP_JMP:` |
|   492106 |  4029 | `	pc = pInstr->iP2 - 1;` |
|   492106 |  4030 | `	break;` |
|        - |  4031 | `/*` |
|        - |  4032 | ` * JZ: P1 P2 *` |
|        - |  4033 | ` *` |
|        - |  4034 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  4035 | ` * entry in the stack if P1 is zero.` |
|        - |  4036 | ` */` |
|   583406 |  4037 | `case PH7_OP_JZ:` |
|        - |  4038 | `#ifdef UNTRUST` |
|        - |  4039 | `	if( pTos < pStack ){` |
|        - |  4040 | `		goto Abort;` |
|        - |  4041 | `	}` |
|        - |  4042 | `#endif` |
|        - |  4043 | `	/* Get a boolean value */` |
|  1166902 |  4044 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      172 |  4045 | `		PH7_MemObjToBool(pTos);` |
|       85 |  4046 | `	}` |
|  1166902 |  4047 | `	if( !pTos->x.iVal ){` |
|        - |  4048 | `		/* Take the jump */` |
|   598550 |  4049 | `		pc = pInstr->iP2 - 1;` |
|   299274 |  4050 | `	}` |
|  1166902 |  4051 | `	if( !pInstr->iP1 ){` |
|   926164 |  4052 | `		VmPopOperand(&pTos,1);` |
|   463103 |  4053 | `	}` |
|  1166902 |  4054 | `	break;` |
|        - |  4055 | `/*` |
|        - |  4056 | ` * JNZ: P1 P2 *` |
|        - |  4057 | ` *` |
|        - |  4058 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  4059 | ` * entry in the stack if P1 is zero.` |
|        - |  4060 | ` */` |
|    60933 |  4061 | `case PH7_OP_JNZ:` |
|        - |  4062 | `#ifdef UNTRUST` |
|        - |  4063 | `	if( pTos < pStack ){` |
|        - |  4064 | `		goto Abort;` |
|        - |  4065 | `	}` |
|        - |  4066 | `#endif` |
|        - |  4067 | `	/* Get a boolean value */` |
|   121868 |  4068 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4069 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4070 | `	}` |
|   121868 |  4071 | `	if( pTos->x.iVal ){` |
|        - |  4072 | `		/* Take the jump */` |
|     5410 |  4073 | `		pc = pInstr->iP2 - 1;` |
|     2704 |  4074 | `	}` |
|   121868 |  4075 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  4076 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4077 | `	}` |
|   121868 |  4078 | `	break;` |
|        - |  4079 | `/*` |
|        - |  4080 | ` * NOOP: * * *` |
|        - |  4081 | ` *` |
|        - |  4082 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  4083 | ` * destination.` |
|        - |  4084 | ` */` |
|      ! 0 |  4085 | `case PH7_OP_NOOP:` |
|      ! 0 |  4086 | `	break;` |
|        - |  4087 | `/*` |
|        - |  4088 | ` * POP: P1 * *` |
|        - |  4089 | ` *` |
|        - |  4090 | ` * Pop P1 elements from the operand stack.` |
|        - |  4091 | ` */` |
|   450346 |  4092 | `case PH7_OP_POP: {` |
|   900738 |  4093 | `	sxi32 n = pInstr->iP1;` |
|   900738 |  4094 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  4095 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       17 |  4096 | `		n = (sxi32)(pTos - pStack);` |
|        8 |  4097 | `	}` |
|   900738 |  4098 | `	VmPopOperand(&pTos,n);` |
|   900738 |  4099 | `	break;` |
|        - |  4100 | `				 }` |
|        - |  4101 | `/*` |
|        - |  4102 | ` * DUP: * * *` |
|        - |  4103 | ` *` |
|        - |  4104 | ` * Duplicate the top of the stack.` |
|        - |  4105 | ` */` |
|       41 |  4106 | `case PH7_OP_DUP:` |
|        - |  4107 | `#ifdef UNTRUST` |
|        - |  4108 | `	if( pTos < pStack ){` |
|        - |  4109 | `		goto Abort;` |
|        - |  4110 | `	}` |
|        - |  4111 | `#endif` |
|       84 |  4112 | `	pTos++;` |
|       84 |  4113 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  4114 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  4115 | `	break;` |
|        - |  4116 | `/*` |
|        - |  4117 | ` * NSSWITCH: * * P3` |
|        - |  4118 | ` *` |
|        - |  4119 | ` * Switch the active namespace at runtime.` |
|        - |  4120 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  4121 | ` */` |
|     7563 |  4122 | `case PH7_OP_NSSWITCH:` |
|    15128 |  4123 | `	SyBlobReset(&pVm->sNamespace);` |
|    15128 |  4124 | `	if( pInstr->p3 ){` |
|       98 |  4125 | `		const char *zNs = (const char *)pInstr->p3;` |
|       98 |  4126 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       48 |  4127 | `	}` |
|        - |  4128 | `	/* Clear namespace-scoped use-const imports */` |
|    15128 |  4129 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    15128 |  4130 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    15128 |  4131 | `	break;` |
|        - |  4132 | `/* OP_USECONST P1 * P3` |
|        - |  4133 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  4134 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  4135 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  4136 | ` */` |
|        7 |  4137 | `case PH7_OP_USECONST: {` |
|       16 |  4138 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  4139 | `	if( azPair ){` |
|       16 |  4140 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  4141 | `	}` |
|       16 |  4142 | `	break;` |
|        - |  4143 | `				}` |
|        - |  4144 | `/*` |
|        - |  4145 | ` * CVT_INT: * * *` |
|        - |  4146 | ` *` |
|        - |  4147 | ` * Force the top of the stack to be an integer.` |
|        - |  4148 | ` */` |
|       78 |  4149 | `case PH7_OP_CVT_INT:` |
|        - |  4150 | `#ifdef UNTRUST` |
|        - |  4151 | `	if( pTos < pStack ){` |
|        - |  4152 | `		goto Abort;` |
|        - |  4153 | `	}` |
|        - |  4154 | `#endif` |
|      158 |  4155 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      111 |  4156 | `		PH7_MemObjToInteger(pTos);` |
|       55 |  4157 | `	}` |
|        - |  4158 | `	/* Invalidate any prior representation */` |
|      158 |  4159 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      158 |  4160 | `	break;` |
|        - |  4161 | `/*` |
|        - |  4162 | ` * CVT_REAL: * * *` |
|        - |  4163 | ` *` |
|        - |  4164 | ` * Force the top of the stack to be a real.` |
|        - |  4165 | ` */` |
|        5 |  4166 | `case PH7_OP_CVT_REAL:` |
|        - |  4167 | `#ifdef UNTRUST` |
|        - |  4168 | `	if( pTos < pStack ){` |
|        - |  4169 | `		goto Abort;` |
|        - |  4170 | `	}` |
|        - |  4171 | `#endif` |
|       11 |  4172 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4173 | `		PH7_MemObjToReal(pTos);` |
|        3 |  4174 | `	}` |
|        - |  4175 | `	/* Invalidate any prior representation */` |
|       11 |  4176 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       11 |  4177 | `	break;` |
|        - |  4178 | `/*` |
|        - |  4179 | ` * CVT_STR: * * *` |
|        - |  4180 | ` *` |
|        - |  4181 | ` * Force the top of the stack to be a string.` |
|        - |  4182 | ` */` |
|      146 |  4183 | `case PH7_OP_CVT_STR:` |
|        - |  4184 | `#ifdef UNTRUST` |
|        - |  4185 | `	if( pTos < pStack ){` |
|        - |  4186 | `		goto Abort;` |
|        - |  4187 | `	}` |
|        - |  4188 | `#endif` |
|      294 |  4189 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  4190 | `		PH7_MemObjToString(pTos);` |
|      146 |  4191 | `	}` |
|      294 |  4192 | `	break;` |
|        - |  4193 | `/*` |
|        - |  4194 | ` * CVT_BOOL: * * *` |
|        - |  4195 | ` *` |
|        - |  4196 | ` * Force the top of the stack to be a boolean.` |
|        - |  4197 | ` */` |
|        5 |  4198 | `case PH7_OP_CVT_BOOL:` |
|        - |  4199 | `#ifdef UNTRUST` |
|        - |  4200 | `	if( pTos < pStack ){` |
|        - |  4201 | `		goto Abort;` |
|        - |  4202 | `	}` |
|        - |  4203 | `#endif` |
|       11 |  4204 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  4205 | `		PH7_MemObjToBool(pTos);` |
|        3 |  4206 | `	}` |
|       11 |  4207 | `	break;` |
|        - |  4208 | `/*` |
|        - |  4209 | ` * CVT_NULL: * * *` |
|        - |  4210 | ` *` |
|        - |  4211 | ` * Nullify the top of the stack.` |
|        - |  4212 | ` */` |
|        3 |  4213 | `case PH7_OP_CVT_NULL:` |
|        - |  4214 | `#ifdef UNTRUST` |
|        - |  4215 | `	if( pTos < pStack ){` |
|        - |  4216 | `		goto Abort;` |
|        - |  4217 | `	}` |
|        - |  4218 | `#endif` |
|        7 |  4219 | `	PH7_MemObjRelease(pTos);` |
|        7 |  4220 | `	break;` |
|        - |  4221 | `/*` |
|        - |  4222 | ` * CVT_NUMC: * * *` |
|        - |  4223 | ` *` |
|        - |  4224 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  4225 | ` */` |
|      ! 0 |  4226 | `case PH7_OP_CVT_NUMC:` |
|        - |  4227 | `#ifdef UNTRUST` |
|        - |  4228 | `	if( pTos < pStack ){` |
|        - |  4229 | `		goto Abort;` |
|        - |  4230 | `	}` |
|        - |  4231 | `#endif` |
|        - |  4232 | `	/* Force a numeric cast */` |
|      ! 0 |  4233 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  4234 | `	break;` |
|        - |  4235 | `/*` |
|        - |  4236 | ` * CVT_ARRAY: * * *` |
|        - |  4237 | ` *` |
|        - |  4238 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  4239 | ` */` |
|       10 |  4240 | `case PH7_OP_CVT_ARRAY:` |
|        - |  4241 | `#ifdef UNTRUST` |
|        - |  4242 | `	if( pTos < pStack ){` |
|        - |  4243 | `		goto Abort;` |
|        - |  4244 | `	}` |
|        - |  4245 | `#endif` |
|        - |  4246 | `	/* Force a hashmap cast */` |
|       21 |  4247 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  4248 | `	if( rc != SXRET_OK ){` |
|        - |  4249 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  4250 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4251 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  4252 | `	}` |
|       21 |  4253 | `	break;` |
|        - |  4254 | `/*` |
|        - |  4255 | ` * CVT_OBJ: * * *` |
|        - |  4256 | ` *` |
|        - |  4257 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  4258 | ` */` |
|        8 |  4259 | `case PH7_OP_CVT_OBJ:` |
|        - |  4260 | `#ifdef UNTRUST` |
|        - |  4261 | `	if( pTos < pStack ){` |
|        - |  4262 | `		goto Abort;` |
|        - |  4263 | `	}` |
|        - |  4264 | `#endif` |
|       17 |  4265 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  4266 | `		/* Force a 'stdClass()' cast */` |
|       17 |  4267 | `		PH7_MemObjToObject(pTos);` |
|        8 |  4268 | `	}` |
|       17 |  4269 | `	break;` |
|        - |  4270 | `/*` |
|        - |  4271 | ` * ERR_CTRL * * *` |
|        - |  4272 | ` *` |
|        - |  4273 | ` * Error control operator.` |
|        - |  4274 | ` */` |
|    15537 |  4275 | `case PH7_OP_ERR_CTRL:` |
|        - |  4276 | `	/*` |
|        - |  4277 | `	 * TICKET 1433-038:` |
|        - |  4278 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  4279 | `	 * use the public API,to control error output.` |
|        - |  4280 | `	 */` |
|    31074 |  4281 | `	break;` |
|        - |  4282 | `/*` |
|        - |  4283 | ` * IS_A * * *` |
|        - |  4284 | ` *` |
|        - |  4285 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  4286 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  4287 | ` * holding a class name or an object).` |
|        - |  4288 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  4289 | ` */` |
|       42 |  4290 | `case PH7_OP_IS_A:{` |
|       86 |  4291 | `	ph7_value *pNos = &pTos[-1];` |
|       86 |  4292 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  4293 | `#ifdef UNTRUST` |
|        - |  4294 | `	if( pNos < pStack ){` |
|        - |  4295 | `		goto Abort;` |
|        - |  4296 | `	}` |
|        - |  4297 | `#endif` |
|       86 |  4298 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       84 |  4299 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       84 |  4300 | `		ph7_class *pClass = 0;` |
|        - |  4301 | `		/* Extract the target class */` |
|       84 |  4302 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4303 | `			/* Instance already loaded */` |
|      ! 0 |  4304 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       84 |  4305 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       84 |  4306 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       84 |  4307 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  4308 | `			/* Handle self/static/parent keywords */` |
|       84 |  4309 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  4310 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       82 |  4311 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  4312 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       81 |  4313 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  4314 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  4315 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  4316 | `					pClass = pSelf->pBase;` |
|        2 |  4317 | `				}` |
|        3 |  4318 | `			}else{` |
|       74 |  4319 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  4320 | `			}` |
|       41 |  4321 | `		}` |
|       84 |  4322 | `		if( pClass ){` |
|        - |  4323 | `			/* Perform the query */` |
|       84 |  4324 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       41 |  4325 | `		}` |
|       41 |  4326 | `	}` |
|        - |  4327 | `	/* Push result */` |
|       86 |  4328 | `	VmPopOperand(&pTos,1);` |
|       86 |  4329 | `	PH7_MemObjRelease(pTos);` |
|       86 |  4330 | `	pTos->x.iVal = iRes;` |
|       86 |  4331 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       86 |  4332 | `	break;` |
|        - |  4333 | `				 }` |
|        - |  4334 |  |
|        - |  4335 | `/*` |
|        - |  4336 | ` * LOADC P1 P2 *` |
|        - |  4337 | ` *` |
|        - |  4338 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  4339 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  4340 | ` */` |
|   983890 |  4341 | `case PH7_OP_LOADC: {` |
|        - |  4342 | `	ph7_value *pObj;` |
|        - |  4343 | `	/* Reserve a room */` |
|  1967826 |  4344 | `	pTos++;` |
|  2942245 |  4345 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1967826 |  4346 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  4347 | `			SyHashEntry *pEntry;` |
|        - |  4348 | `			/* Check use const imports first — imports take precedence */` |
|        - |  4349 | `			{` |
|        - |  4350 | `				SyHashEntry *pConstImport;` |
|    28550 |  4351 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    19032 |  4352 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19034 |  4353 | `				if( pConstImport ){` |
|       11 |  4354 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  4355 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  4356 | `					if( pEntry ){` |
|       11 |  4357 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  4358 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  4359 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  4360 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  4361 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  4362 | `						break;` |
|        - |  4363 | `					}` |
|        - |  4364 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  4365 | `				}` |
|        - |  4366 | `			}` |
|        - |  4367 | `			/* Candidate for expansion via user defined callbacks */` |
|    19024 |  4368 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19024 |  4369 | `			if( pEntry ){` |
|    19020 |  4370 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  4371 | `				/* Set a NULL default value */` |
|    19020 |  4372 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    19020 |  4373 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  4374 | `				/* Invoke the callback and deal with the expanded value */` |
|    19020 |  4375 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  4376 | `				/* Mark as constant */` |
|    19020 |  4377 | `				pTos->nIdx = SXU32_HIGH;` |
|    19020 |  4378 | `				break;` |
|        - |  4379 | `			}` |
|        - |  4380 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  4381 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  4382 | `			 * use-const imports → current NS → global → string fallback).` |
|        - |  4383 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - |  4384 | `			{` |
|        6 |  4385 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  4386 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  4387 | `				sxu32 j;` |
|        6 |  4388 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       14 |  4389 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|        9 |  4390 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|        5 |  4391 | `				}` |
|        6 |  4392 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  4393 | `					/* Try current_namespace\name */` |
|      ! 0 |  4394 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  4395 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  4396 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  4397 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  4398 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  4399 | `					if( pEntry ){` |
|      ! 0 |  4400 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  4401 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4402 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  4403 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  4404 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4405 | `						break;` |
|        - |  4406 | `					}` |
|        - |  4407 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  4408 | `				}` |
|        6 |  4409 | `				if( isQualified ){` |
|        - |  4410 | `					/* Qualified name: must be a real constant. */` |
|        3 |  4411 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  4412 | `					SyBlob sErr;` |
|        3 |  4413 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  4414 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  4415 | `					if( pErrFile ){` |
|        3 |  4416 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  4417 | `					}` |
|        3 |  4418 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  4419 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  4420 | `					SyBlobRelease(&sErr);` |
|        3 |  4421 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  4422 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  4423 | `					goto LoadC_Done;` |
|        - |  4424 | `				}` |
|        - |  4425 | `			}` |
|        1 |  4426 | `		}` |
|  1948796 |  4427 | `		PH7_MemObjLoad(pObj,pTos);` |
|   974421 |  4428 | `	}else{` |
|        - |  4429 | `		/* Set a NULL value */` |
|      ! 0 |  4430 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4431 | `	}` |
|   974376 |  4432 | `LoadC_Done:` |
|        - |  4433 | `	/* Mark as constant */` |
|  1948798 |  4434 | `	pTos->nIdx = SXU32_HIGH;` |
|  1948798 |  4435 | `	break;` |
|        - |  4436 | `				  }` |
|        - |  4437 | `/*` |
|        - |  4438 | ` * LOAD: P1 * P3` |
|        - |  4439 | ` *` |
|        - |  4440 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  4441 | ` * from the P3 operand.` |
|        - |  4442 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  4443 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  4444 | ` */` |
|  1546588 |  4445 | `case PH7_OP_LOAD:{` |
|        - |  4446 | `	ph7_value *pObj;` |
|        - |  4447 | `	SyString sName;` |
|  3093398 |  4448 | `	if( pInstr->p3 == 0 ){` |
|        - |  4449 | `		/* Take the variable name from the top of the stack */` |
|        - |  4450 | `#ifdef UNTRUST` |
|        - |  4451 | `		if( pTos < pStack ){` |
|        - |  4452 | `			goto Abort;` |
|        - |  4453 | `		}` |
|        - |  4454 | `#endif` |
|        - |  4455 | `		/* Force a string cast */` |
|       19 |  4456 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4457 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4458 | `		}` |
|       19 |  4459 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  4460 | `	}else{` |
|  3093380 |  4461 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4462 | `		/* Reserve a room for the target object */` |
|  3093380 |  4463 | `		pTos++;` |
|        - |  4464 | `	}` |
|        - |  4465 | `	/* Extract the requested memory object */` |
|  3093398 |  4466 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  3093398 |  4467 | `	if( pObj == 0 ){` |
|       28 |  4468 | `		if( pInstr->iP1 ){` |
|        - |  4469 | `			/* Variable not found,load NULL */` |
|       28 |  4470 | `			if( !pInstr->p3 ){` |
|      ! 0 |  4471 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4472 | `			}else{` |
|       28 |  4473 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4474 | `			}` |
|       28 |  4475 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1546603 |  4476 | `			break;` |
|      ! 0 |  4477 | `		}else{` |
|        - |  4478 | `			/* Fatal error */` |
|      ! 0 |  4479 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4480 | `			goto Abort;` |
|        - |  4481 | `		}` |
|        - |  4482 | `	}` |
|        - |  4483 | `	/* Load variable contents */` |
|  3093372 |  4484 | `	PH7_MemObjLoad(pObj,pTos);` |
|  3093372 |  4485 | `	pTos->nIdx = pObj->nIdx;` |
|  3093372 |  4486 | `	break;` |
|        - |  4487 | `				   }` |
|        - |  4488 | `/*` |
|        - |  4489 | ` * LOAD_MAP P1 * *` |
|        - |  4490 | ` *` |
|        - |  4491 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  4492 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  4493 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  4494 | ` */` |
|    21977 |  4495 | `case PH7_OP_LOAD_MAP: {` |
|        - |  4496 | `	ph7_hashmap *pMap;` |
|        - |  4497 | `	/* Allocate a new hashmap instance */` |
|    43956 |  4498 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    43956 |  4499 | `	if( pMap == 0 ){` |
|      ! 0 |  4500 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4501 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  4502 | `		goto Abort;` |
|        - |  4503 | `	}` |
|    43956 |  4504 | `	if( pInstr->iP1 > 0 ){` |
|     2542 |  4505 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|     2542 |  4506 | `		sxi32 rcSpread = SXRET_OK;` |
|        - |  4507 | `		/* Perform the insertion */` |
|     7756 |  4508 | `		while( pEntry < pTos ){` |
|     5230 |  4509 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|        - |  4510 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|        - |  4511 | `				 * semantics — string keys preserved (later wins), int keys` |
|        - |  4512 | `				 * renumbered. Same routine that backs array_merge. */` |
|       62 |  4513 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|       47 |  4514 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|       47 |  4515 | `					if( rcMerge != SXRET_OK ){` |
|        - |  4516 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|        - |  4517 | `						 * path — emit fatal and abort, leaving no partial` |
|        - |  4518 | `						 * map dangling. */` |
|      ! 0 |  4519 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4520 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|      ! 0 |  4521 | `						rcSpread = PH7_ABORT;` |
|      ! 0 |  4522 | `						break;` |
|        - |  4523 | `					}` |
|       24 |  4524 | `				}else{` |
|        - |  4525 | `					/* Throw a catchable Error matching PHP semantics. */` |
|       15 |  4526 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|       15 |  4527 | `					break;` |
|        1 |  4528 | `				}` |
|     5193 |  4529 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  4530 | `				/* Insertion by reference */` |
|      142 |  4531 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  4532 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  4533 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  4534 | `					);` |
|       48 |  4535 | `			}else{` |
|        - |  4536 | `				/* Standard insertion */` |
|     7613 |  4537 | `				PH7_HashmapInsert(pMap,` |
|     5074 |  4538 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2537 |  4539 | `					&pEntry[1]` |
|        - |  4540 | `				);` |
|        - |  4541 | `			}` |
|        - |  4542 | `			/* Next pair on the stack */` |
|     5216 |  4543 | `			pEntry += 2;` |
|        2 |  4544 | `		}` |
|        - |  4545 | `		/* Pop P1 elements */` |
|     2542 |  4546 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     2542 |  4547 | `		if( rcSpread != SXRET_OK ){` |
|        - |  4548 | `			/* Discard the partially-built map and propagate the exception. */` |
|       15 |  4549 | `			PH7_HashmapRelease(pMap,TRUE);` |
|       15 |  4550 | `			if( rcSpread == PH7_ABORT ){` |
|      ! 0 |  4551 | `				goto Abort;` |
|        - |  4552 | `			}` |
|        - |  4553 | `			{` |
|       15 |  4554 | `				VmFrame *pFrm2 = pVm->pFrame;` |
|       15 |  4555 | `				if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|      ! 0 |  4556 | `					pc = pFrm2->iExceptionJump - 1;` |
|      ! 0 |  4557 | `					break;` |
|        - |  4558 | `				}` |
|        - |  4559 | `			}` |
|       15 |  4560 | `			goto Exception;` |
|        - |  4561 | `		}` |
|     1263 |  4562 | `	}` |
|        - |  4563 | `	/* Push the hashmap */` |
|    43942 |  4564 | `	pTos++;` |
|    43942 |  4565 | `	pTos->nIdx = SXU32_HIGH;` |
|    43942 |  4566 | `	pTos->x.pOther = pMap;` |
|    43942 |  4567 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    43942 |  4568 | `	break;` |
|        - |  4569 | `					  }` |
|        - |  4570 | `/*` |
|        - |  4571 | ` * LOAD_LIST: P1 * *` |
|        - |  4572 | ` *` |
|        - |  4573 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  4574 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  4575 | ` * Caveats:` |
|        - |  4576 | ` *  This implementation support only a single nesting level.` |
|        - |  4577 | ` */` |
|       48 |  4578 | `case PH7_OP_LOAD_LIST: {` |
|        - |  4579 | `	ph7_value *pEntry;` |
|       98 |  4580 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  4581 | `		/* Empty list,break immediately */` |
|      ! 0 |  4582 | `		break;` |
|        - |  4583 | `	}` |
|       98 |  4584 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  4585 | `#ifdef UNTRUST` |
|        - |  4586 | `	if( &pEntry[-1] < pStack ){` |
|        - |  4587 | `		goto Abort;` |
|        - |  4588 | `	}` |
|        - |  4589 | `#endif` |
|       98 |  4590 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  4591 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  4592 | `		ph7_hashmap_node *pNode;` |
|        - |  4593 | `		ph7_value sKey,*pObj;` |
|        - |  4594 | `		/* Start Copying */` |
|       91 |  4595 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  4596 | `		while( pEntry <= pTos ){` |
|      193 |  4597 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  4598 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  4599 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  4600 | `					if( rc == SXRET_OK ){` |
|        - |  4601 | `						/* Store node value */` |
|      165 |  4602 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  4603 | `					}else{` |
|        - |  4604 | `						/* Undefined array key */` |
|        - |  4605 | `						char zMsg[128];` |
|      ! 0 |  4606 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  4607 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4608 | `						PH7_MemObjRelease(pObj);` |
|        - |  4609 | `					}` |
|       82 |  4610 | `				}` |
|       82 |  4611 | `			}` |
|      193 |  4612 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  4613 | `			pEntry++;` |
|        1 |  4614 | `		}` |
|       46 |  4615 | `	}else{` |
|        - |  4616 | `		/* Source is not an array */` |
|        - |  4617 | `		ph7_value *pObj;` |
|       18 |  4618 | `		while( pEntry <= pTos ){` |
|       12 |  4619 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  4620 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  4621 | `					PH7_MemObjRelease(pObj);` |
|        5 |  4622 | `				}` |
|        5 |  4623 | `			}` |
|       12 |  4624 | `			pEntry++;` |
|        2 |  4625 | `		}` |
|        8 |  4626 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  4627 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  4628 | `			const char *zType = "unknown";` |
|        3 |  4629 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  4630 | `			char zMsg[256];` |
|        3 |  4631 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  4632 | `				zType = "string";` |
|        1 |  4633 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  4634 | `				zType = "int";` |
|      ! 0 |  4635 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4636 | `				zType = "float";` |
|      ! 0 |  4637 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4638 | `				zType = "object";` |
|      ! 0 |  4639 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  4640 | `				zType = "resource";` |
|      ! 0 |  4641 | `			}` |
|        3 |  4642 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  4643 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  4644 | `		}` |
|        - |  4645 | `	}` |
|       98 |  4646 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  4647 | `	break;` |
|        - |  4648 | `					   }` |
|        - |  4649 | `/*` |
|        - |  4650 | ` * LOAD_IDX: P1 P2 *` |
|        - |  4651 | ` *` |
|        - |  4652 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  4653 | ` * from the stack.` |
|        - |  4654 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  4655 | ` * instead.` |
|        - |  4656 | ` */` |
|   248045 |  4657 | `case PH7_OP_LOAD_IDX: {` |
|   496136 |  4658 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   496136 |  4659 | `	ph7_hashmap *pMap = 0;` |
|        - |  4660 | `	ph7_value *pIdx;` |
|   496136 |  4661 | `	pIdx = 0;` |
|   496136 |  4662 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  4663 | `		if( !pInstr->iP2){` |
|        - |  4664 | `			/* No available index,load NULL */` |
|      ! 0 |  4665 | `			if( pTos >= pStack ){` |
|      ! 0 |  4666 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4667 | `			}else{` |
|        - |  4668 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  4669 | `				pTos++;` |
|      ! 0 |  4670 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4671 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  4672 | `			}` |
|        - |  4673 | `			/* Emit a notice */` |
|      ! 0 |  4674 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  4675 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  4676 | `			break;` |
|        - |  4677 | `		}` |
|      ! 0 |  4678 | `	}else{` |
|   496136 |  4679 | `		pIdx = pTos;` |
|   496136 |  4680 | `		pTos--;` |
|        - |  4681 | `	}` |
|   496136 |  4682 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  4683 | `		/* String access */` |
|   386252 |  4684 | `		if( pIdx ){` |
|        - |  4685 | `			sxu32 nOfft;` |
|   386252 |  4686 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  4687 | `				/* Force an int cast */` |
|      ! 0 |  4688 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4689 | `			}` |
|   386252 |  4690 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   386252 |  4691 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  4692 | `				/* Invalid offset,load null */` |
|      ! 0 |  4693 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4694 | `			}else{` |
|   386252 |  4695 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   386252 |  4696 | `				int c = zData[nOfft];` |
|   386252 |  4697 | `				PH7_MemObjRelease(pTos);` |
|   386252 |  4698 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   386252 |  4699 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  4700 | `			}` |
|   193149 |  4701 | `		}else{` |
|        - |  4702 | `			/* No available index,load NULL */` |
|      ! 0 |  4703 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4704 | `		}` |
|   386252 |  4705 | `		break;` |
|        - |  4706 | `	}` |
|   109886 |  4707 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  4708 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4709 | `			ph7_value *pObj;` |
|        3 |  4710 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4711 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  4712 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  4713 | `			}` |
|        1 |  4714 | `		}` |
|        1 |  4715 | `	}` |
|   109886 |  4716 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   109886 |  4717 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   109886 |  4718 | `		if( pInstr->iP2 == 1 ){` |
|        - |  4719 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  4720 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  4721 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  4722 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      885 |  4723 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      442 |  4724 | `		}` |
|        - |  4725 | `		/* Point to the hashmap */` |
|   109886 |  4726 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   109886 |  4727 | `		if( pIdx ){` |
|        - |  4728 | `			/* Load the desired entry */` |
|   109886 |  4729 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    54942 |  4730 | `		}` |
|   109886 |  4731 | `		if( pInstr->iP2 == 3 ){` |
|        - |  4732 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  4733 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  4734 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  4735 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  4736 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  4737 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  4738 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  4739 | `			 * correct for the outermost write. */` |
|       19 |  4740 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  4741 | `			if( !needWrite && pNode ){` |
|       13 |  4742 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  4743 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  4744 | `					needWrite = 1;` |
|        3 |  4745 | `				}` |
|        6 |  4746 | `			}` |
|       19 |  4747 | `			if( needWrite ){` |
|       13 |  4748 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  4749 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  4750 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  4751 | `					 * into the new map's storage. */` |
|        7 |  4752 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  4753 | `					if( pIdx ){` |
|        7 |  4754 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  4755 | `					}` |
|        3 |  4756 | `				}` |
|        6 |  4757 | `			}` |
|        9 |  4758 | `		}` |
|   109886 |  4759 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) ){` |
|        - |  4760 | `			/* Create a new empty entry */` |
|      273 |  4761 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  4762 | `			if( rc == SXRET_OK ){` |
|        - |  4763 | `				/* Point to the last inserted entry */` |
|      273 |  4764 | `				pNode = pMap->pLast;` |
|      136 |  4765 | `			}` |
|      136 |  4766 | `		}` |
|    54942 |  4767 | `	}` |
|   109886 |  4768 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  4769 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  4770 | `		char zMsg[128];` |
|      ! 0 |  4771 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4772 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4773 | `		}` |
|      ! 0 |  4774 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  4775 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4776 | `	}` |
|   109886 |  4777 | `	if( pIdx ){` |
|   109886 |  4778 | `		PH7_MemObjRelease(pIdx);` |
|    54942 |  4779 | `	}` |
|   109886 |  4780 | `	if( rc == SXRET_OK ){` |
|        - |  4781 | `		/* Load entry contents */` |
|    48810 |  4782 | `		if( pMap->iRef < 2 ){` |
|        - |  4783 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  4784 | `			 * of the entry value,rather than pointing to it.` |
|        - |  4785 | `			 */` |
|       28 |  4786 | `			pTos->nIdx = SXU32_HIGH;` |
|       28 |  4787 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       15 |  4788 | `		}else{` |
|    48784 |  4789 | `			pTos->nIdx = pNode->nValIdx;` |
|    48784 |  4790 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    48784 |  4791 | `			PH7_HashmapUnref(pMap);` |
|        - |  4792 | `		}` |
|    24406 |  4793 | `	}else{` |
|        - |  4794 | `		/* No such entry,load NULL */` |
|    61078 |  4795 | `		PH7_MemObjRelease(pTos);` |
|    61078 |  4796 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  4797 | `	}` |
|   109886 |  4798 | `	break;` |
|        - |  4799 | `					  }` |
|        - |  4800 | `/*` |
|        - |  4801 | ` * LOAD_CLOSURE * * P3` |
|        - |  4802 | ` *` |
|        - |  4803 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  4804 | ` * name in the stack.` |
|        - |  4805 | ` */` |
|       47 |  4806 | `case PH7_OP_LOAD_CLOSURE:{` |
|       96 |  4807 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       96 |  4808 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  4809 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  4810 | `		ph7_vm_func *pClosure;` |
|        - |  4811 | `		char *zName;` |
|        - |  4812 | `		sxu32 mLen;` |
|        - |  4813 | `		sxu32 n;` |
|        - |  4814 | `		/* Create a new VM function */` |
|       96 |  4815 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  4816 | `		/* Generate an unique closure name */` |
|       96 |  4817 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|       96 |  4818 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  4819 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  4820 | `			goto Abort;` |
|        - |  4821 | `		}` |
|       96 |  4822 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|       96 |  4823 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  4824 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  4825 | `		}` |
|        - |  4826 | `		/* Zero the stucture */` |
|       96 |  4827 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  4828 | `		/* Perform a structure assignment on read-only items */` |
|       96 |  4829 | `		pClosure->aArgs = pFunc->aArgs;` |
|       96 |  4830 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|       96 |  4831 | `		pClosure->aStatic = pFunc->aStatic;` |
|       96 |  4832 | `		pClosure->iFlags = pFunc->iFlags;` |
|       96 |  4833 | `		pClosure->pUserData = pFunc->pUserData;` |
|       96 |  4834 | `		pClosure->sSignature = pFunc->sSignature;` |
|       96 |  4835 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|       96 |  4836 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|       96 |  4837 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|       96 |  4838 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|       96 |  4839 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  4840 | `		/* Register the closure */` |
|       96 |  4841 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  4842 | `		/* Set up closure environment */` |
|       96 |  4843 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|       96 |  4844 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      256 |  4845 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  4846 | `			ph7_value *pValue;` |
|      162 |  4847 | `			pEnv = &aEnv[n];` |
|      162 |  4848 | `			sEnv.sName  = pEnv->sName;` |
|      162 |  4849 | `			sEnv.iFlags = pEnv->iFlags;` |
|      162 |  4850 | `			sEnv.nIdx = SXU32_HIGH;` |
|      162 |  4851 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      162 |  4852 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  4853 | `				/* Pass by reference */` |
|      ! 0 |  4854 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  4855 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  4856 | `					);` |
|      ! 0 |  4857 | `			}` |
|        - |  4858 | `			/* Standard pass by value */` |
|      162 |  4859 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      162 |  4860 | `			if( pValue ){` |
|        - |  4861 | `				/* Copy imported value */` |
|       72 |  4862 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       35 |  4863 | `			}` |
|        - |  4864 | `			/* Insert the imported variable */` |
|      162 |  4865 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       82 |  4866 | `		}` |
|        - |  4867 | `		/* Finally,load the closure name on the stack */` |
|       96 |  4868 | `		pTos++;` |
|       96 |  4869 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       47 |  4870 | `	}` |
|       96 |  4871 | `	break;` |
|        - |  4872 | `						 }` |
|        - |  4873 | `/*` |
|        - |  4874 | ` * STORE * P2 P3` |
|        - |  4875 | ` *` |
|        - |  4876 | ` * Perform a store (Assignment) operation.` |
|        - |  4877 | ` */` |
|   138662 |  4878 | `case PH7_OP_STORE: {` |
|        - |  4879 | `	ph7_value *pObj;` |
|        - |  4880 | `	SyString sName;` |
|        - |  4881 | `#ifdef UNTRUST` |
|        - |  4882 | `	if( pTos < pStack ){` |
|        - |  4883 | `		goto Abort;` |
|        - |  4884 | `	}` |
|        - |  4885 | `#endif` |
|   277326 |  4886 | `	if( pInstr->iP2 ){` |
|        - |  4887 | `		sxu32 nIdx;` |
|        - |  4888 | `		sxi32 rcT;` |
|        - |  4889 | `		/* Member store operation */` |
|     4856 |  4890 | `		nIdx = pTos->nIdx;` |
|     4856 |  4891 | `		VmPopOperand(&pTos,1);` |
|     4856 |  4892 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  4893 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4894 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  4895 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  4896 | `		}else{` |
|        - |  4897 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  4898 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     4852 |  4899 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     4852 |  4900 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  4901 | `				goto Abort;` |
|        - |  4902 | `			}` |
|     4852 |  4903 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  4904 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  4905 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  4906 | `				 * propagate out of the VM loop. */` |
|       37 |  4907 | `				VmPopOperand(&pTos,1);` |
|        - |  4908 | `				{` |
|       37 |  4909 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       37 |  4910 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       37 |  4911 | `						pc = pFrm2->iExceptionJump - 1;` |
|   138681 |  4912 | `						break;` |
|        - |  4913 | `					}` |
|        - |  4914 | `				}` |
|      ! 0 |  4915 | `				goto Exception;` |
|        - |  4916 | `			}` |
|        - |  4917 | `			/* Point to the desired memory object */` |
|     4816 |  4918 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     4816 |  4919 | `			if( pObj ){` |
|        - |  4920 | `				/* Perform the store operation */` |
|     4816 |  4921 | `				PH7_MemObjStore(pTos,pObj);` |
|     2407 |  4922 | `			}` |
|        - |  4923 | `		}` |
|     4820 |  4924 | `		break;` |
|   272472 |  4925 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  4926 | `		/* Take the variable name from the next on the stack */` |
|        7 |  4927 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4928 | `			/* Force a string cast */` |
|      ! 0 |  4929 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4930 | `		}` |
|        7 |  4931 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  4932 | `		pTos--;` |
|        - |  4933 | `#ifdef UNTRUST` |
|        - |  4934 | `		if( pTos < pStack  ){` |
|        - |  4935 | `			goto Abort;` |
|        - |  4936 | `		}` |
|        - |  4937 | `#endif` |
|        4 |  4938 | `	}else{` |
|   272466 |  4939 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4940 | `	}` |
|        - |  4941 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   272472 |  4942 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   272472 |  4943 | `	if( pObj == 0 ){` |
|      ! 0 |  4944 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4945 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4946 | `		goto Abort;` |
|        - |  4947 | `	}` |
|   272472 |  4948 | `	if( !pInstr->p3 ){` |
|        7 |  4949 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  4950 | `	}` |
|        - |  4951 | `	/* Perform the store operation */` |
|   272472 |  4952 | `	PH7_MemObjStore(pTos,pObj);` |
|   272472 |  4953 | `	break;` |
|        - |  4954 | `				   }` |
|        - |  4955 | `/*` |
|        - |  4956 | ` * STORE_IDX:   P1 * P3` |
|        - |  4957 | ` * STORE_IDX_R: P1 * P3` |
|        - |  4958 | ` *` |
|        - |  4959 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  4960 | ` */` |
|    94236 |  4961 | `case PH7_OP_STORE_IDX:` |
|        - |  4962 | `case PH7_OP_STORE_IDX_REF: {` |
|   188474 |  4963 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  4964 | `	ph7_value *pKey;` |
|        - |  4965 | `	sxu32 nIdx;` |
|   188474 |  4966 | `	if( pInstr->iP1 ){` |
|        - |  4967 | `		/* Key is next on stack */` |
|    62312 |  4968 | `		pKey = pTos;` |
|    62312 |  4969 | `		pTos--;` |
|    31157 |  4970 | `	}else{` |
|   126164 |  4971 | `		pKey = 0;` |
|        - |  4972 | `	}` |
|   188474 |  4973 | `	nIdx = pTos->nIdx;` |
|   188474 |  4974 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  4975 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  4976 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  4977 | `		 * checking true sharing count, then re-add after separation. */` |
|   188422 |  4978 | `		if( nIdx != SXU32_HIGH ){` |
|   188422 |  4979 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   282632 |  4980 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   188422 |  4981 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4982 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  4983 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  4984 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  4985 | `				 * refcounts if the backing array was already separated. */` |
|   188422 |  4986 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   188422 |  4987 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   188422 |  4988 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   188422 |  4989 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   188422 |  4990 | `					pTos->x.pOther = pMap;` |
|    94212 |  4991 | `				}else{` |
|        - |  4992 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  4993 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  4994 | `					pMap = pCur;` |
|        - |  4995 | `				}` |
|    94212 |  4996 | `			}else{` |
|      ! 0 |  4997 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4998 | `			}` |
|    94212 |  4999 | `		}else{` |
|      ! 0 |  5000 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5001 | `		}` |
|   188422 |  5002 | `		if( pMap->iRef < 2 ){` |
|        - |  5003 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  5004 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  5005 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  5006 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  5007 | `			pMap->iRef = 2;` |
|      ! 0 |  5008 | `		}` |
|    94212 |  5009 | `	}else{` |
|        - |  5010 | `		ph7_value *pObj;` |
|       53 |  5011 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  5012 | `		if( pObj == 0 ){` |
|      ! 0 |  5013 | `			if( pKey ){` |
|      ! 0 |  5014 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  5015 | `			}` |
|      ! 0 |  5016 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5017 | `			break;` |
|        - |  5018 | `		}` |
|        - |  5019 | `		/* Phase#1: Load the array */` |
|       53 |  5020 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  5021 | `			VmPopOperand(&pTos,1);` |
|       53 |  5022 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  5023 | `				/* Force a string cast */` |
|      ! 0 |  5024 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  5025 | `			}` |
|       53 |  5026 | `			if( pKey == 0 ){` |
|        - |  5027 | `				/* Append string */` |
|        3 |  5028 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  5029 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  5030 | `				}` |
|        2 |  5031 | `			}else{` |
|        - |  5032 | `				sxu32 nOfft;` |
|       51 |  5033 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  5034 | `					/* Force an int cast */` |
|       51 |  5035 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  5036 | `				}` |
|       51 |  5037 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  5038 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  5039 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  5040 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  5041 | `					zData[nOfft] = zBlob[0];` |
|       26 |  5042 | `				}else{` |
|      ! 0 |  5043 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  5044 | `						/* Perform an append operation */` |
|      ! 0 |  5045 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  5046 | `					}` |
|        - |  5047 | `				}` |
|        - |  5048 | `			}` |
|       53 |  5049 | `			if( pKey ){` |
|       51 |  5050 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  5051 | `			}` |
|       53 |  5052 | `			break;` |
|      ! 0 |  5053 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  5054 | `			/* Force a hashmap cast  */` |
|      ! 0 |  5055 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  5056 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  5057 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  5058 | `				goto Abort;` |
|        - |  5059 | `			}` |
|      ! 0 |  5060 | `		}` |
|        - |  5061 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  5062 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  5063 | `	}` |
|   188422 |  5064 | `	VmPopOperand(&pTos,1);` |
|        - |  5065 | `	/* Phase#2: Perform the insertion */` |
|   188422 |  5066 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  5067 | `		/* Insertion by reference */` |
|       15 |  5068 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  5069 | `	}else{` |
|   188408 |  5070 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  5071 | `	}` |
|   188422 |  5072 | `	if( pKey ){` |
|    62262 |  5073 | `		PH7_MemObjRelease(pKey);` |
|    31130 |  5074 | `	}` |
|   188422 |  5075 | `	break;` |
|        - |  5076 | `					   }` |
|        - |  5077 | `/*` |
|        - |  5078 | ` * INCR: P1 * *` |
|        - |  5079 | ` *` |
|        - |  5080 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  5081 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  5082 | ` * the stack and increment after that.` |
|        - |  5083 | ` */` |
|   167014 |  5084 | `case PH7_OP_INCR:` |
|        - |  5085 | `#ifdef UNTRUST` |
|        - |  5086 | `	if( pTos < pStack ){` |
|        - |  5087 | `		goto Abort;` |
|        - |  5088 | `	}` |
|        - |  5089 | `#endif` |
|   334074 |  5090 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   334074 |  5091 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5092 | `			ph7_value *pObj;` |
|   334074 |  5093 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   334074 |  5094 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  5095 | `					/* Perl-style string increment.` |
|        - |  5096 | `					 * Post-increment: pTos may alias pObj's buffer via SXBLOB_RDONLY` |
|        - |  5097 | `					 * (set by PH7_MemObjLoad).  Force ownership so the upcoming` |
|        - |  5098 | `					 * mutation of pObj doesn't bleed into pTos's old-value view. */` |
|       49 |  5099 | `					if( pInstr->iP1 == 0 ){` |
|       45 |  5100 | `						SyBlobNullAppend(&pTos->sBlob);` |
|       22 |  5101 | `					}` |
|       49 |  5102 | `					PH7_MemObjStringIncrement(pObj);` |
|       49 |  5103 | `					if( pInstr->iP1 ){` |
|        - |  5104 | `						/* Pre-increment: deep-copy pObj into pTos. */` |
|        5 |  5105 | `						PH7_MemObjStore(pObj,pTos);` |
|        2 |  5106 | `					}` |
|       25 |  5107 | `				}else{` |
|        - |  5108 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - |  5109 | `					 * original value: pTos may alias pObj's blob via` |
|        - |  5110 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - |  5111 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - |  5112 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - |  5113 | `					 * so its old-value view survives the coercion. */` |
|   334026 |  5114 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       11 |  5115 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        5 |  5116 | `					}` |
|        - |  5117 | `					/* Force a numeric cast on the variable */` |
|   334026 |  5118 | `					PH7_MemObjToNumeric(pObj);` |
|   334026 |  5119 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        5 |  5120 | `						pObj->rVal++;` |
|        3 |  5121 | `					}else{` |
|   334022 |  5122 | `						pObj->x.iVal++;` |
|        - |  5123 | `					}` |
|   334026 |  5124 | `					if( pInstr->iP1 ){` |
|        - |  5125 | `						/* Pre-increment: result is the new value. */` |
|       77 |  5126 | `						PH7_MemObjStore(pObj,pTos);` |
|       38 |  5127 | `					}` |
|        - |  5128 | `					/* Post-increment: pTos retains the old value (a string` |
|        - |  5129 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - |  5130 | `				}` |
|   167058 |  5131 | `			}` |
|   167060 |  5132 | `		}else{` |
|      ! 0 |  5133 | `			if( pInstr->iP1 ){` |
|      ! 0 |  5134 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 |  5135 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 |  5136 | `				}else{` |
|        - |  5137 | `					/* Force a numeric cast */` |
|      ! 0 |  5138 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  5139 | `					/* Pre-increment */` |
|      ! 0 |  5140 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5141 | `						pTos->rVal++;` |
|        - |  5142 | `						/* Try to get an integer representation */` |
|      ! 0 |  5143 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5144 | `					}else{` |
|      ! 0 |  5145 | `						pTos->x.iVal++;` |
|      ! 0 |  5146 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5147 | `					}` |
|        - |  5148 | `				}` |
|      ! 0 |  5149 | `			}` |
|        - |  5150 | `		}` |
|   167058 |  5151 | `	}` |
|   334074 |  5152 | `	break;` |
|        - |  5153 | `/*` |
|        - |  5154 | ` * DECR: P1 * *` |
|        - |  5155 | ` *` |
|        - |  5156 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  5157 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  5158 | ` * and decrement after that.` |
|        - |  5159 | ` */` |
|        2 |  5160 | `case PH7_OP_DECR:` |
|        - |  5161 | `#ifdef UNTRUST` |
|        - |  5162 | `	if( pTos < pStack ){` |
|        - |  5163 | `		goto Abort;` |
|        - |  5164 | `	}` |
|        - |  5165 | `#endif` |
|        5 |  5166 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  5167 | `		/* Force a numeric cast */` |
|        5 |  5168 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  5169 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5170 | `			ph7_value *pObj;` |
|        5 |  5171 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  5172 | `				/* Force a numeric cast */` |
|        5 |  5173 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  5174 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5175 | `					pObj->rVal--;` |
|        - |  5176 | `					/* Try to get an integer representation */` |
|      ! 0 |  5177 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5178 | `				}else{` |
|        5 |  5179 | `					pObj->x.iVal--;` |
|        5 |  5180 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5181 | `				}` |
|        5 |  5182 | `				if( pInstr->iP1 ){` |
|        - |  5183 | `					/* Pre-icrement */` |
|      ! 0 |  5184 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  5185 | `				}` |
|        2 |  5186 | `			}` |
|        3 |  5187 | `		}else{` |
|      ! 0 |  5188 | `			if( pInstr->iP1 ){` |
|        - |  5189 | `				/* Pre-increment */` |
|      ! 0 |  5190 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5191 | `					pTos->rVal--;` |
|        - |  5192 | `					/* Try to get an integer representation */` |
|      ! 0 |  5193 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5194 | `				}else{` |
|      ! 0 |  5195 | `					pTos->x.iVal--;` |
|      ! 0 |  5196 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5197 | `				}` |
|      ! 0 |  5198 | `			}` |
|        - |  5199 | `		}` |
|        2 |  5200 | `	}` |
|        5 |  5201 | `	break;` |
|        - |  5202 | `/*` |
|        - |  5203 | ` * UMINUS: * * *` |
|        - |  5204 | ` *` |
|        - |  5205 | ` * Perform a unary minus operation.` |
|        - |  5206 | ` */` |
|    28756 |  5207 | `case PH7_OP_UMINUS:` |
|        - |  5208 | `#ifdef UNTRUST` |
|        - |  5209 | `	if( pTos < pStack ){` |
|        - |  5210 | `		goto Abort;` |
|        - |  5211 | `	}` |
|        - |  5212 | `#endif` |
|        - |  5213 | `	/* Force a numeric (integer,real or both) cast */` |
|    57514 |  5214 | `	PH7_MemObjToNumeric(pTos);` |
|    57514 |  5215 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  5216 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  5217 | `	}` |
|    57514 |  5218 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    57484 |  5219 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    28741 |  5220 | `	}` |
|    57514 |  5221 | `	break;` |
|        - |  5222 | `/*` |
|        - |  5223 | ` * UPLUS: * * *` |
|        - |  5224 | ` *` |
|        - |  5225 | ` * Perform a unary plus operation.` |
|        - |  5226 | ` */` |
|       18 |  5227 | `case PH7_OP_UPLUS:` |
|        - |  5228 | `#ifdef UNTRUST` |
|        - |  5229 | `	if( pTos < pStack ){` |
|        - |  5230 | `		goto Abort;` |
|        - |  5231 | `	}` |
|        - |  5232 | `#endif` |
|        - |  5233 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  5234 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  5235 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5236 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  5237 | `	}` |
|       37 |  5238 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  5239 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  5240 | `	}` |
|       37 |  5241 | `	break;` |
|        - |  5242 | `/*` |
|        - |  5243 | ` * OP_LNOT: * * *` |
|        - |  5244 | ` *` |
|        - |  5245 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  5246 | ` * with its complement.` |
|        - |  5247 | ` */` |
|    44297 |  5248 | `case PH7_OP_LNOT:` |
|        - |  5249 | `#ifdef UNTRUST` |
|        - |  5250 | `	if( pTos < pStack ){` |
|        - |  5251 | `		goto Abort;` |
|        - |  5252 | `	}` |
|        - |  5253 | `#endif` |
|        - |  5254 | `	/* Force a boolean cast */` |
|    88640 |  5255 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       23 |  5256 | `		PH7_MemObjToBool(pTos);` |
|       11 |  5257 | `	}` |
|    88640 |  5258 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    88640 |  5259 | `	break;` |
|        - |  5260 | `/*` |
|        - |  5261 | ` * OP_BITNOT: * * *` |
|        - |  5262 | ` *` |
|        - |  5263 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  5264 | ` * with its ones-complement.` |
|        - |  5265 | ` */` |
|       15 |  5266 | `case PH7_OP_BITNOT:` |
|        - |  5267 | `#ifdef UNTRUST` |
|        - |  5268 | `	if( pTos < pStack ){` |
|        - |  5269 | `		goto Abort;` |
|        - |  5270 | `	}` |
|        - |  5271 | `#endif` |
|        - |  5272 | `	/* Force an integer cast */` |
|       32 |  5273 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5274 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5275 | `	}` |
|       32 |  5276 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       32 |  5277 | `	break;` |
|        - |  5278 | `/* OP_MUL * * *` |
|        - |  5279 | ` * OP_MUL_STORE * * *` |
|        - |  5280 | ` *` |
|        - |  5281 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  5282 | ` * and push the result back onto the stack.` |
|        - |  5283 | ` */` |
|     1287 |  5284 | `case PH7_OP_MUL:` |
|        - |  5285 | `case PH7_OP_MUL_STORE: {` |
|     2576 |  5286 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5287 | `	/* Force the operand to be numeric */` |
|        - |  5288 | `#ifdef UNTRUST` |
|        - |  5289 | `	if( pNos < pStack ){` |
|        - |  5290 | `		goto Abort;` |
|        - |  5291 | `	}` |
|        - |  5292 | `#endif` |
|     2576 |  5293 | `	PH7_MemObjToNumeric(pTos);` |
|     2576 |  5294 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5295 | `	/* Perform the requested operation */` |
|     2576 |  5296 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5297 | `		/* Floating point arithemic */` |
|        - |  5298 | `		ph7_real a,b,r;` |
|       19 |  5299 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  5300 | `			PH7_MemObjToReal(pTos);` |
|        4 |  5301 | `		}` |
|       19 |  5302 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  5303 | `			PH7_MemObjToReal(pNos);` |
|        3 |  5304 | `		}` |
|       19 |  5305 | `		a = pNos->rVal;` |
|       19 |  5306 | `		b = pTos->rVal;` |
|       19 |  5307 | `		r = a * b;` |
|        - |  5308 | `		/* Push the result */` |
|       19 |  5309 | `		pNos->rVal = r;` |
|       19 |  5310 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5311 | `		/* Try to get an integer representation */` |
|       19 |  5312 | `		PH7_MemObjTryInteger(pNos);` |
|       10 |  5313 | `	}else{` |
|        - |  5314 | `		/* Integer arithmetic */` |
|        - |  5315 | `		sxi64 a,b,r;` |
|     2558 |  5316 | `		a = pNos->x.iVal;` |
|     2558 |  5317 | `		b = pTos->x.iVal;` |
|     2558 |  5318 | `		r = a * b;` |
|        - |  5319 | `		/* Push the result */` |
|     2558 |  5320 | `		pNos->x.iVal = r;` |
|     2558 |  5321 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5322 | `	}` |
|     2576 |  5323 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  5324 | `		ph7_value *pObj;` |
|       32 |  5325 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5326 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  5327 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  5328 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  5329 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  5330 | `		}` |
|       15 |  5331 | `	}` |
|     2576 |  5332 | `	VmPopOperand(&pTos,1);` |
|     2576 |  5333 | `	break;` |
|        - |  5334 | `				 }` |
|        - |  5335 | `/* OP_POW * * *` |
|        - |  5336 | ` * OP_POW_STORE * * *` |
|        - |  5337 | ` *` |
|        - |  5338 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - |  5339 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - |  5340 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - |  5341 | ` * fits in sxi64; otherwise the result is a double.` |
|        - |  5342 | ` */` |
|       66 |  5343 | `case PH7_OP_POW:` |
|        - |  5344 | `case PH7_OP_POW_STORE: {` |
|      133 |  5345 | `	ph7_value *pNos = &pTos[-1];` |
|      133 |  5346 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|        - |  5347 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|        - |  5348 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|        - |  5349 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|        - |  5350 | `	 */` |
|      133 |  5351 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|      133 |  5352 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|        - |  5353 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  5354 | `	int bBothInt;` |
|      133 |  5355 | `	int usedInt = 0;` |
|        - |  5356 | `	ph7_real a, b, r;` |
|        - |  5357 | `#endif` |
|      133 |  5358 | `	sxi64 base_i = 0, exp_i = 0;` |
|        - |  5359 | `#ifdef UNTRUST` |
|        - |  5360 | `	if( pNos < pStack ){` |
|        - |  5361 | `		goto Abort;` |
|        - |  5362 | `	}` |
|        - |  5363 | `#endif` |
|      133 |  5364 | `	PH7_MemObjToNumeric(pTos);` |
|      133 |  5365 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5366 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      261 |  5367 | `	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&` |
|      128 |  5368 | `	           ((pNos->iFlags & MEMOBJ_REAL) == 0);` |
|      133 |  5369 | `	if( bBothInt ){` |
|      123 |  5370 | `		base_i = pBase->x.iVal;` |
|      123 |  5371 | `		exp_i  = pExp->x.iVal;` |
|       61 |  5372 | `	}` |
|      133 |  5373 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|      125 |  5374 | `		PH7_MemObjToReal(pBase);` |
|       62 |  5375 | `	}` |
|      133 |  5376 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|      131 |  5377 | `		PH7_MemObjToReal(pExp);` |
|       65 |  5378 | `	}` |
|      133 |  5379 | `	a = pBase->rVal;` |
|      133 |  5380 | `	b = pExp->rVal;` |
|      133 |  5381 | `	r = pow(a, b);` |
|        - |  5382 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|        - |  5383 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|        - |  5384 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|        - |  5385 | `	 * representable as double but not as signed int64. */` |
|      133 |  5386 | `	if( bBothInt && exp_i >= 0 ){` |
|      117 |  5387 | `		sxi64 result_i = 1;` |
|      117 |  5388 | `		sxi64 cur_base = base_i;` |
|      117 |  5389 | `		sxi64 cur_exp  = exp_i;` |
|      117 |  5390 | `		int overflow = 0;` |
|      401 |  5391 | `		while( cur_exp > 0 ){` |
|      289 |  5392 | `			if( cur_exp & 1 ){` |
|      189 |  5393 | `				if( VmMulOverflow64(result_i, cur_base, &result_i) ){` |
|        3 |  5394 | `					overflow = 1;` |
|        3 |  5395 | `					break;` |
|        - |  5396 | `				}` |
|       93 |  5397 | `			}` |
|      287 |  5398 | `			cur_exp >>= 1;` |
|      287 |  5399 | `			if( cur_exp > 0 ){` |
|      181 |  5400 | `				if( VmMulOverflow64(cur_base, cur_base, &cur_base) ){` |
|        3 |  5401 | `					overflow = 1;` |
|        3 |  5402 | `					break;` |
|        - |  5403 | `				}` |
|       89 |  5404 | `			}` |
|        1 |  5405 | `		}` |
|      117 |  5406 | `		if( !overflow ){` |
|      113 |  5407 | `			pNos->x.iVal = result_i;` |
|      113 |  5408 | `			MemObjSetType(pNos, MEMOBJ_INT);` |
|      113 |  5409 | `			usedInt = 1;` |
|       56 |  5410 | `		}` |
|       58 |  5411 | `	}` |
|      133 |  5412 | `	if( !usedInt ){` |
|       21 |  5413 | `		pNos->rVal = r;` |
|       21 |  5414 | `		MemObjSetType(pNos, MEMOBJ_REAL);` |
|       10 |  5415 | `	}` |
|        - |  5416 | `#else` |
|        - |  5417 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|        - |  5418 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|        - |  5419 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|        - |  5420 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|        - |  5421 | `	 * represented. */` |
|        - |  5422 | `	base_i = pBase->x.iVal;` |
|        - |  5423 | `	exp_i  = pExp->x.iVal;` |
|        - |  5424 | `	{` |
|        - |  5425 | `		sxi64 result_i = 1;` |
|        - |  5426 | `		sxi64 cur_base = base_i;` |
|        - |  5427 | `		sxi64 cur_exp  = exp_i;` |
|        - |  5428 | `		if( cur_exp < 0 ){` |
|        - |  5429 | `			result_i = 0;` |
|        - |  5430 | `		}else{` |
|        - |  5431 | `			while( cur_exp > 0 ){` |
|        - |  5432 | `				if( cur_exp & 1 ){` |
|        - |  5433 | `					result_i *= cur_base;` |
|        - |  5434 | `				}` |
|        - |  5435 | `				cur_exp >>= 1;` |
|        - |  5436 | `				if( cur_exp > 0 ){` |
|        - |  5437 | `					cur_base *= cur_base;` |
|        - |  5438 | `				}` |
|        - |  5439 | `			}` |
|        - |  5440 | `		}` |
|        - |  5441 | `		pNos->x.iVal = result_i;` |
|        - |  5442 | `		MemObjSetType(pNos, MEMOBJ_INT);` |
|        - |  5443 | `	}` |
|        - |  5444 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      133 |  5445 | `	if( bStore ){` |
|        - |  5446 | `		ph7_value *pObj;` |
|       23 |  5447 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5448 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       23 |  5449 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       23 |  5450 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       23 |  5451 | `			PH7_MemObjStore(pNos,pObj);` |
|       11 |  5452 | `		}` |
|       11 |  5453 | `	}` |
|      133 |  5454 | `	VmPopOperand(&pTos,1);` |
|      133 |  5455 | `	break;` |
|        - |  5456 | `				 }` |
|        - |  5457 | `/* OP_ADD * * *` |
|        - |  5458 | ` *` |
|        - |  5459 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5460 | ` * and push the result back onto the stack.` |
|        - |  5461 | ` */` |
|      513 |  5462 | `case PH7_OP_ADD:{` |
|     1028 |  5463 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5464 | `#ifdef UNTRUST` |
|        - |  5465 | `	if( pNos < pStack ){` |
|        - |  5466 | `		goto Abort;` |
|        - |  5467 | `	}` |
|        - |  5468 | `#endif` |
|        - |  5469 | `	/* Perform the addition */` |
|     1028 |  5470 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|     1028 |  5471 | `	VmPopOperand(&pTos,1);` |
|     1028 |  5472 | `	break;` |
|        - |  5473 | `				}` |
|        - |  5474 | `/*` |
|        - |  5475 | ` * OP_ADD_STORE * * *` |
|        - |  5476 | ` *` |
|        - |  5477 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5478 | ` * and push the result back onto the stack.` |
|        - |  5479 | ` */` |
|      502 |  5480 | `case PH7_OP_ADD_STORE:{` |
|     1006 |  5481 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5482 | `	ph7_value *pObj;` |
|        - |  5483 | `	sxu32 nIdx;` |
|        - |  5484 | `#ifdef UNTRUST` |
|        - |  5485 | `	if( pNos < pStack ){` |
|        - |  5486 | `		goto Abort;` |
|        - |  5487 | `	}` |
|        - |  5488 | `#endif` |
|        - |  5489 | `	/* Perform the addition */` |
|     1006 |  5490 | `	nIdx = pTos->nIdx;` |
|     1006 |  5491 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  5492 | `	/* Peform the store operation */` |
|     1006 |  5493 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  5494 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1006 |  5495 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1006 |  5496 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1006 |  5497 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  5498 | `	}` |
|        - |  5499 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1006 |  5500 | `	PH7_MemObjStore(pTos,pNos);` |
|     1006 |  5501 | `	VmPopOperand(&pTos,1);` |
|     1006 |  5502 | `	break;` |
|        - |  5503 | `				}` |
|        - |  5504 | `/* OP_SUB * * *` |
|        - |  5505 | ` *` |
|        - |  5506 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5507 | ` * first (what was next on the stack) from the second (the` |
|        - |  5508 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5509 | ` */` |
|      348 |  5510 | `case PH7_OP_SUB: {` |
|      698 |  5511 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5512 | `#ifdef UNTRUST` |
|        - |  5513 | `	if( pNos < pStack ){` |
|        - |  5514 | `		goto Abort;` |
|        - |  5515 | `	}` |
|        - |  5516 | `#endif` |
|      698 |  5517 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5518 | `		/* Floating point arithemic */` |
|        - |  5519 | `		ph7_real a,b,r;` |
|       95 |  5520 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5521 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5522 | `		}` |
|       95 |  5523 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5524 | `			PH7_MemObjToReal(pNos);` |
|        2 |  5525 | `		}` |
|       95 |  5526 | `		a = pNos->rVal;` |
|       95 |  5527 | `		b = pTos->rVal;` |
|       95 |  5528 | `		r = a - b;` |
|        - |  5529 | `		/* Push the result */` |
|       95 |  5530 | `		pNos->rVal = r;` |
|       95 |  5531 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5532 | `		/* Try to get an integer representation */` |
|       95 |  5533 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  5534 | `	}else{` |
|        - |  5535 | `		/* Integer arithmetic */` |
|        - |  5536 | `		sxi64 a,b,r;` |
|      604 |  5537 | `		a = pNos->x.iVal;` |
|      604 |  5538 | `		b = pTos->x.iVal;` |
|      604 |  5539 | `		r = a - b;` |
|        - |  5540 | `		/* Push the result */` |
|      604 |  5541 | `		pNos->x.iVal = r;` |
|      604 |  5542 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5543 | `	}` |
|      698 |  5544 | `	VmPopOperand(&pTos,1);` |
|      698 |  5545 | `	break;` |
|        - |  5546 | `				 }` |
|        - |  5547 | `/* OP_SUB_STORE * * *` |
|        - |  5548 | ` *` |
|        - |  5549 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5550 | ` * first (what was next on the stack) from the second (the` |
|        - |  5551 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5552 | ` */` |
|        4 |  5553 | `case PH7_OP_SUB_STORE: {` |
|       10 |  5554 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5555 | `	ph7_value *pObj;` |
|        - |  5556 | `#ifdef UNTRUST` |
|        - |  5557 | `	if( pNos < pStack ){` |
|        - |  5558 | `		goto Abort;` |
|        - |  5559 | `	}` |
|        - |  5560 | `#endif` |
|       10 |  5561 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5562 | `		/* Floating point arithemic */` |
|        - |  5563 | `		ph7_real a,b,r;` |
|      ! 0 |  5564 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5565 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5566 | `		}` |
|      ! 0 |  5567 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5568 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  5569 | `		}` |
|      ! 0 |  5570 | `		a = pTos->rVal;` |
|      ! 0 |  5571 | `		b = pNos->rVal;` |
|      ! 0 |  5572 | `		r = a - b;` |
|        - |  5573 | `		/* Push the result */` |
|      ! 0 |  5574 | `		pNos->rVal = r;` |
|      ! 0 |  5575 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5576 | `		/* Try to get an integer representation */` |
|      ! 0 |  5577 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  5578 | `	}else{` |
|        - |  5579 | `		/* Integer arithmetic */` |
|        - |  5580 | `		sxi64 a,b,r;` |
|       10 |  5581 | `		a = pTos->x.iVal;` |
|       10 |  5582 | `		b = pNos->x.iVal;` |
|       10 |  5583 | `		r = a - b;` |
|        - |  5584 | `		/* Push the result */` |
|       10 |  5585 | `		pNos->x.iVal = r;` |
|       10 |  5586 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5587 | `	}` |
|       10 |  5588 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5589 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  5590 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  5591 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  5592 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  5593 | `	}` |
|       10 |  5594 | `	VmPopOperand(&pTos,1);` |
|       10 |  5595 | `	break;` |
|        - |  5596 | `				 }` |
|        - |  5597 |  |
|        - |  5598 | `/*` |
|        - |  5599 | ` * OP_MOD * * *` |
|        - |  5600 | ` *` |
|        - |  5601 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5602 | ` * first (what was next on the stack) from the second (the` |
|        - |  5603 | ` * top of the stack) and push the remainder after division` |
|        - |  5604 | ` * onto the stack.` |
|        - |  5605 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  5606 | ` */` |
|      308 |  5607 | `case PH7_OP_MOD:{` |
|      618 |  5608 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5609 | `	sxi64 a,b,r;` |
|        - |  5610 | `#ifdef UNTRUST` |
|        - |  5611 | `	if( pNos < pStack ){` |
|        - |  5612 | `		goto Abort;` |
|        - |  5613 | `	}` |
|        - |  5614 | `#endif` |
|        - |  5615 | `	/* Force the operands to be integer */` |
|      618 |  5616 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5617 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5618 | `	}` |
|      618 |  5619 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  5620 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  5621 | `	}` |
|        - |  5622 | `	/* Perform the requested operation */` |
|      618 |  5623 | `	a = pNos->x.iVal;` |
|      618 |  5624 | `	b = pTos->x.iVal;` |
|      618 |  5625 | `	if( b == 0 ){` |
|        3 |  5626 | `		r = 0;` |
|        3 |  5627 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  5628 | `		/* goto Abort; */` |
|        2 |  5629 | `	}else{` |
|      615 |  5630 | `		r = a%b;` |
|        - |  5631 | `	}` |
|        - |  5632 | `	/* Push the result */` |
|      618 |  5633 | `	pNos->x.iVal = r;` |
|      618 |  5634 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      618 |  5635 | `	VmPopOperand(&pTos,1);` |
|      618 |  5636 | `	break;` |
|        - |  5637 | `				}` |
|        - |  5638 | `/*` |
|        - |  5639 | ` * OP_MOD_STORE * * *` |
|        - |  5640 | ` *` |
|        - |  5641 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5642 | ` * first (what was next on the stack) from the second (the` |
|        - |  5643 | ` * top of the stack) and push the remainder after division` |
|        - |  5644 | ` * onto the stack.` |
|        - |  5645 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  5646 | ` */` |
|        1 |  5647 | `case PH7_OP_MOD_STORE: {` |
|        3 |  5648 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5649 | `	ph7_value *pObj;` |
|        - |  5650 | `	sxi64 a,b,r;` |
|        - |  5651 | `#ifdef UNTRUST` |
|        - |  5652 | `	if( pNos < pStack ){` |
|        - |  5653 | `		goto Abort;` |
|        - |  5654 | `	}` |
|        - |  5655 | `#endif` |
|        - |  5656 | `	/* Force the operands to be integer */` |
|        3 |  5657 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5658 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5659 | `	}` |
|        3 |  5660 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5661 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5662 | `	}` |
|        - |  5663 | `	/* Perform the requested operation */` |
|        3 |  5664 | `	a = pTos->x.iVal;` |
|        3 |  5665 | `	b = pNos->x.iVal;` |
|        3 |  5666 | `	if( b == 0 ){` |
|      ! 0 |  5667 | `		r = 0;` |
|      ! 0 |  5668 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  5669 | `		/* goto Abort; */` |
|      ! 0 |  5670 | `	}else{` |
|        3 |  5671 | `		r = a%b;` |
|        - |  5672 | `	}` |
|        - |  5673 | `	/* Push the result */` |
|        3 |  5674 | `	pNos->x.iVal = r;` |
|        3 |  5675 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  5676 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5677 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  5678 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  5679 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  5680 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  5681 | `	}` |
|        3 |  5682 | `	VmPopOperand(&pTos,1);` |
|        3 |  5683 | `	break;` |
|        - |  5684 | `				}` |
|        - |  5685 | `/*` |
|        - |  5686 | ` * OP_DIV * * *` |
|        - |  5687 | ` *` |
|        - |  5688 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5689 | ` * first (what was next on the stack) from the second (the` |
|        - |  5690 | ` * top of the stack) and push the result onto the stack.` |
|        - |  5691 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  5692 | ` */` |
|       31 |  5693 | `case PH7_OP_DIV:{` |
|       64 |  5694 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5695 | `	ph7_real a,b,r;` |
|        - |  5696 | `#ifdef UNTRUST` |
|        - |  5697 | `	if( pNos < pStack ){` |
|        - |  5698 | `		goto Abort;` |
|        - |  5699 | `	}` |
|        - |  5700 | `#endif` |
|        - |  5701 | `	/* Force the operands to be real */` |
|       64 |  5702 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       60 |  5703 | `		PH7_MemObjToReal(pTos);` |
|       29 |  5704 | `	}` |
|       64 |  5705 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       26 |  5706 | `		PH7_MemObjToReal(pNos);` |
|       12 |  5707 | `	}` |
|        - |  5708 | `	/* Perform the requested operation */` |
|       64 |  5709 | `	a = pNos->rVal;` |
|       64 |  5710 | `	b = pTos->rVal;` |
|       64 |  5711 | `	if( b == 0 ){` |
|        - |  5712 | `		/* Division by zero */` |
|        3 |  5713 | `		pNos->rVal = 0;` |
|        3 |  5714 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  5715 | `		/* goto Abort; */` |
|        2 |  5716 | `	}else{` |
|       61 |  5717 | `		r = a/b;` |
|        - |  5718 | `		/* Push the result */` |
|       61 |  5719 | `		pNos->rVal = r;` |
|       61 |  5720 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5721 | `		/* Try to get an integer representation */` |
|       61 |  5722 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  5723 | `	}` |
|       64 |  5724 | `	VmPopOperand(&pTos,1);` |
|       64 |  5725 | `	break;` |
|        - |  5726 | `				}` |
|        - |  5727 | `/*` |
|        - |  5728 | ` * OP_DIV_STORE * * *` |
|        - |  5729 | ` *` |
|        - |  5730 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5731 | ` * first (what was next on the stack) from the second (the` |
|        - |  5732 | ` * top of the stack) and push the result onto the stack.` |
|        - |  5733 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  5734 | ` */` |
|        2 |  5735 | `case PH7_OP_DIV_STORE:{` |
|        5 |  5736 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5737 | `	ph7_value *pObj;` |
|        - |  5738 | `	ph7_real a,b,r;` |
|        - |  5739 | `#ifdef UNTRUST` |
|        - |  5740 | `	if( pNos < pStack ){` |
|        - |  5741 | `		goto Abort;` |
|        - |  5742 | `	}` |
|        - |  5743 | `#endif` |
|        - |  5744 | `	/* Force the operands to be real */` |
|        5 |  5745 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5746 | `		PH7_MemObjToReal(pTos);` |
|        2 |  5747 | `	}` |
|        5 |  5748 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5749 | `		PH7_MemObjToReal(pNos);` |
|        2 |  5750 | `	}` |
|        - |  5751 | `	/* Perform the requested operation */` |
|        5 |  5752 | `	a = pTos->rVal;` |
|        5 |  5753 | `	b = pNos->rVal;` |
|        5 |  5754 | `	if( b == 0 ){` |
|        - |  5755 | `		/* Division by zero */` |
|      ! 0 |  5756 | `		r = 0;` |
|      ! 0 |  5757 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  5758 | `		/* goto Abort; */` |
|      ! 0 |  5759 | `	}else{` |
|        5 |  5760 | `		r = a/b;` |
|        - |  5761 | `		/* Push the result */` |
|        5 |  5762 | `		pNos->rVal = r;` |
|        5 |  5763 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5764 | `		/* Try to get an integer representation */` |
|        5 |  5765 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  5766 | `	}` |
|        5 |  5767 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5768 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  5769 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  5770 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  5771 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  5772 | `	}` |
|        5 |  5773 | `	VmPopOperand(&pTos,1);` |
|        5 |  5774 | `	break;` |
|        - |  5775 | `				}` |
|        - |  5776 | `/* OP_BAND * * *` |
|        - |  5777 | ` *` |
|        - |  5778 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5779 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  5780 | ` * two elements.` |
|        - |  5781 | `*/` |
|        - |  5782 | `/* OP_BOR * * *` |
|        - |  5783 | ` *` |
|        - |  5784 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5785 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  5786 | ` * two elements.` |
|        - |  5787 | ` */` |
|        - |  5788 | `/* OP_BXOR * * *` |
|        - |  5789 | ` *` |
|        - |  5790 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5791 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  5792 | ` * two elements.` |
|        - |  5793 | ` */` |
|       44 |  5794 | `case PH7_OP_BAND:` |
|        - |  5795 | `case PH7_OP_BOR:` |
|        - |  5796 | `case PH7_OP_BXOR:{` |
|       90 |  5797 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5798 | `	sxi64 a,b,r;` |
|        - |  5799 | `#ifdef UNTRUST` |
|        - |  5800 | `	if( pNos < pStack ){` |
|        - |  5801 | `		goto Abort;` |
|        - |  5802 | `	}` |
|        - |  5803 | `#endif` |
|        - |  5804 | `	/* Force the operands to be integer */` |
|       90 |  5805 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5806 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5807 | `	}` |
|       90 |  5808 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5809 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5810 | `	}` |
|        - |  5811 | `	/* Perform the requested operation */` |
|       90 |  5812 | `	a = pNos->x.iVal;` |
|       90 |  5813 | `	b = pTos->x.iVal;` |
|       90 |  5814 | `	switch(pInstr->iOp){` |
|        7 |  5815 | `	case PH7_OP_BOR_STORE:` |
|       15 |  5816 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  5817 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  5818 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       30 |  5819 | `	case PH7_OP_BAND_STORE:` |
|       30 |  5820 | `	case PH7_OP_BAND:` |
|       62 |  5821 | `	default:          r = a&b; break;` |
|        - |  5822 | `	}` |
|        - |  5823 | `	/* Push the result */` |
|       90 |  5824 | `	pNos->x.iVal = r;` |
|       90 |  5825 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       90 |  5826 | `	VmPopOperand(&pTos,1);` |
|       90 |  5827 | `	break;` |
|        - |  5828 | `				 }` |
|        - |  5829 | `/* OP_BAND_STORE * * *` |
|        - |  5830 | ` *` |
|        - |  5831 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5832 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  5833 | ` * two elements.` |
|        - |  5834 | `*/` |
|        - |  5835 | `/* OP_BOR_STORE * * *` |
|        - |  5836 | ` *` |
|        - |  5837 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5838 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  5839 | ` * two elements.` |
|        - |  5840 | ` */` |
|        - |  5841 | `/* OP_BXOR_STORE * * *` |
|        - |  5842 | ` *` |
|        - |  5843 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5844 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  5845 | ` * two elements.` |
|        - |  5846 | ` */` |
|       10 |  5847 | `case PH7_OP_BAND_STORE:` |
|        - |  5848 | `case PH7_OP_BOR_STORE:` |
|        - |  5849 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  5850 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5851 | `	ph7_value *pObj;` |
|        - |  5852 | `	sxi64 a,b,r;` |
|        - |  5853 | `#ifdef UNTRUST` |
|        - |  5854 | `	if( pNos < pStack ){` |
|        - |  5855 | `		goto Abort;` |
|        - |  5856 | `	}` |
|        - |  5857 | `#endif` |
|        - |  5858 | `	/* Force the operands to be integer */` |
|       21 |  5859 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5860 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5861 | `	}` |
|       21 |  5862 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5863 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5864 | `	}` |
|        - |  5865 | `	/* Perform the requested operation */` |
|       21 |  5866 | `	a = pTos->x.iVal;` |
|       21 |  5867 | `	b = pNos->x.iVal;` |
|       21 |  5868 | `	switch(pInstr->iOp){` |
|        3 |  5869 | `	case PH7_OP_BOR_STORE:` |
|        7 |  5870 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  5871 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  5872 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  5873 | `	case PH7_OP_BAND_STORE:` |
|        3 |  5874 | `	case PH7_OP_BAND:` |
|        7 |  5875 | `	default:          r = a&b; break;` |
|        - |  5876 | `	}` |
|        - |  5877 | `	/* Push the result */` |
|       21 |  5878 | `	pNos->x.iVal = r;` |
|       21 |  5879 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  5880 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5881 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  5882 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  5883 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  5884 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  5885 | `	}` |
|       21 |  5886 | `	VmPopOperand(&pTos,1);` |
|       21 |  5887 | `	break;` |
|        - |  5888 | `				 }` |
|        - |  5889 | `/* OP_SHL * * *` |
|        - |  5890 | ` *` |
|        - |  5891 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5892 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5893 | ` * left by N bits where N is the top element on the stack.` |
|        - |  5894 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5895 | ` */` |
|        - |  5896 | `/* OP_SHR * * *` |
|        - |  5897 | ` *` |
|        - |  5898 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5899 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5900 | ` * right by N bits where N is the top element on the stack.` |
|        - |  5901 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5902 | ` */` |
|       12 |  5903 | `case PH7_OP_SHL:` |
|        - |  5904 | `case PH7_OP_SHR: {` |
|       25 |  5905 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5906 | `	sxi64 a,r;` |
|        - |  5907 | `	sxi32 b;` |
|        - |  5908 | `#ifdef UNTRUST` |
|        - |  5909 | `	if( pNos < pStack ){` |
|        - |  5910 | `		goto Abort;` |
|        - |  5911 | `	}` |
|        - |  5912 | `#endif` |
|        - |  5913 | `	/* Force the operands to be integer */` |
|       25 |  5914 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5915 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5916 | `	}` |
|       25 |  5917 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5918 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5919 | `	}` |
|        - |  5920 | `	/* Perform the requested operation */` |
|       25 |  5921 | `	a = pNos->x.iVal;` |
|       25 |  5922 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  5923 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  5924 | `		r = a << b;` |
|        8 |  5925 | `	}else{` |
|       11 |  5926 | `		r = a >> b;` |
|        - |  5927 | `	}` |
|        - |  5928 | `	/* Push the result */` |
|       25 |  5929 | `	pNos->x.iVal = r;` |
|       25 |  5930 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  5931 | `	VmPopOperand(&pTos,1);` |
|       25 |  5932 | `	break;` |
|        - |  5933 | `				 }` |
|        - |  5934 | `/*  OP_SHL_STORE * * *` |
|        - |  5935 | ` *` |
|        - |  5936 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5937 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5938 | ` * left by N bits where N is the top element on the stack.` |
|        - |  5939 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5940 | ` */` |
|        - |  5941 | `/* OP_SHR_STORE * * *` |
|        - |  5942 | ` *` |
|        - |  5943 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5944 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5945 | ` * right by N bits where N is the top element on the stack.` |
|        - |  5946 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5947 | ` */` |
|        9 |  5948 | `case PH7_OP_SHL_STORE:` |
|        - |  5949 | `case PH7_OP_SHR_STORE: {` |
|       19 |  5950 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5951 | `	ph7_value *pObj;` |
|        - |  5952 | `	sxi64 a,r;` |
|        - |  5953 | `	sxi32 b;` |
|        - |  5954 | `#ifdef UNTRUST` |
|        - |  5955 | `	if( pNos < pStack ){` |
|        - |  5956 | `		goto Abort;` |
|        - |  5957 | `	}` |
|        - |  5958 | `#endif` |
|        - |  5959 | `	/* Force the operands to be integer */` |
|       19 |  5960 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5961 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5962 | `	}` |
|       19 |  5963 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5964 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5965 | `	}` |
|        - |  5966 | `	/* Perform the requested operation */` |
|       19 |  5967 | `	a = pTos->x.iVal;` |
|       19 |  5968 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  5969 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  5970 | `		r = a << b;` |
|        5 |  5971 | `	}else{` |
|       11 |  5972 | `		r = a >> b;` |
|        - |  5973 | `	}` |
|        - |  5974 | `	/* Push the result */` |
|       19 |  5975 | `	pNos->x.iVal = r;` |
|       19 |  5976 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  5977 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5978 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  5979 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  5980 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  5981 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  5982 | `	}` |
|       19 |  5983 | `	VmPopOperand(&pTos,1);` |
|       19 |  5984 | `	break;` |
|        - |  5985 | `				 }` |
|        - |  5986 | `/* CAT:  P1 * *` |
|        - |  5987 | ` *` |
|        - |  5988 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  5989 | ` * back.` |
|        - |  5990 | ` */` |
|    70229 |  5991 | `case PH7_OP_CAT:{` |
|        - |  5992 | `	ph7_value *pNos,*pCur;` |
|   140460 |  5993 | `	if( pInstr->iP1 < 1 ){` |
|   113092 |  5994 | `		pNos = &pTos[-1];` |
|    56547 |  5995 | `	}else{` |
|    27370 |  5996 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  5997 | `	}` |
|        - |  5998 | `#ifdef UNTRUST` |
|        - |  5999 | `	if( pNos < pStack ){` |
|        - |  6000 | `		goto Abort;` |
|        - |  6001 | `	}` |
|        - |  6002 | `#endif` |
|        - |  6003 | `	/* Force a string cast */` |
|   140460 |  6004 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1640 |  6005 | `		PH7_MemObjToString(pNos);` |
|      819 |  6006 | `	}` |
|   140460 |  6007 | `	pCur = &pNos[1];` |
|   283500 |  6008 | `	while( pCur <= pTos ){` |
|   143042 |  6009 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50900 |  6010 | `			PH7_MemObjToString(pCur);` |
|    25449 |  6011 | `		}` |
|        - |  6012 | `		/* Perform the concatenation */` |
|   143042 |  6013 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   143000 |  6014 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    71499 |  6015 | `		}` |
|   143042 |  6016 | `		SyBlobRelease(&pCur->sBlob);` |
|   143042 |  6017 | `		pCur++;` |
|        2 |  6018 | `	}` |
|   140460 |  6019 | `	pTos = pNos;` |
|   140460 |  6020 | `	break;` |
|        - |  6021 | `				}` |
|        - |  6022 | `/*  CAT_STORE: * * *` |
|        - |  6023 | ` *` |
|        - |  6024 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  6025 | ` * back.` |
|        - |  6026 | ` */` |
|     3910 |  6027 | `case PH7_OP_CAT_STORE:{` |
|     7822 |  6028 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6029 | `	ph7_value *pObj;` |
|        - |  6030 | `#ifdef UNTRUST` |
|        - |  6031 | `	if( pNos < pStack ){` |
|        - |  6032 | `		goto Abort;` |
|        - |  6033 | `	}` |
|        - |  6034 | `#endif` |
|     7822 |  6035 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6036 | `		/* Force a string cast */` |
|        3 |  6037 | `		PH7_MemObjToString(pTos);` |
|        1 |  6038 | `	}` |
|     7822 |  6039 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6040 | `		/* Force a string cast */` |
|      ! 0 |  6041 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6042 | `	}` |
|        - |  6043 | `	/* Perform the concatenation (Reverse order) */` |
|     7822 |  6044 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     7822 |  6045 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3910 |  6046 | `	}` |
|        - |  6047 | `	/* Perform the store operation */` |
|     7822 |  6048 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6049 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     7822 |  6050 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     7822 |  6051 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|     7820 |  6052 | `		PH7_MemObjStore(pTos,pObj);` |
|     3909 |  6053 | `	}` |
|     7820 |  6054 | `	PH7_MemObjStore(pTos,pNos);` |
|     7820 |  6055 | `	VmPopOperand(&pTos,1);` |
|     7820 |  6056 | `	break;` |
|        - |  6057 | `				}` |
|        - |  6058 | `/* OP_AND: * * *` |
|        - |  6059 | ` *` |
|        - |  6060 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  6061 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6062 | ` * stack.` |
|        - |  6063 | ` */` |
|        - |  6064 | `/* OP_OR: * * *` |
|        - |  6065 | ` *` |
|        - |  6066 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  6067 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6068 | ` * stack.` |
|        - |  6069 | ` */` |
|   107211 |  6070 | `case PH7_OP_LAND:` |
|        - |  6071 | `case PH7_OP_LOR: {` |
|   214468 |  6072 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6073 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  6074 | `#ifdef UNTRUST` |
|        - |  6075 | `	if( pNos < pStack ){` |
|        - |  6076 | `		goto Abort;` |
|        - |  6077 | `	}` |
|        - |  6078 | `#endif` |
|        - |  6079 | `	/* Force a boolean cast */` |
|   214468 |  6080 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  6081 | `		PH7_MemObjToBool(pTos);` |
|        1 |  6082 | `	}` |
|   214468 |  6083 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6084 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6085 | `	}` |
|   214468 |  6086 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   214468 |  6087 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   214468 |  6088 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  6089 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    98012 |  6090 | `		v1 = and_logic[v1*3+v2];` |
|    49029 |  6091 | `	}else{` |
|        - |  6092 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   116458 |  6093 | `		v1 = or_logic[v1*3+v2];` |
|        - |  6094 | `	}` |
|   214468 |  6095 | `	if( v1 == 2 ){` |
|      ! 0 |  6096 | `		v1 = 1;` |
|      ! 0 |  6097 | `	}` |
|   214468 |  6098 | `	VmPopOperand(&pTos,1);` |
|   214468 |  6099 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   214468 |  6100 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   214468 |  6101 | `	break;` |
|        - |  6102 | `				 }` |
|        - |  6103 | `/*` |
|        - |  6104 | ` * OP_NULLC: * * *` |
|        - |  6105 | ` * Null coalescing operator '??'.` |
|        - |  6106 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  6107 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  6108 | ` */` |
|        - |  6109 | `/*` |
|        - |  6110 | ` * OP_NULLC: * P2 *` |
|        - |  6111 | ` * Short-circuit null coalescing '??'.` |
|        - |  6112 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  6113 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  6114 | ` */` |
|       52 |  6115 | `case PH7_OP_NULLC: {` |
|        - |  6116 | `#ifdef UNTRUST` |
|        - |  6117 | `	if( pTos < pStack ){` |
|        - |  6118 | `		goto Abort;` |
|        - |  6119 | `	}` |
|        - |  6120 | `#endif` |
|      106 |  6121 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  6122 | `		/* Left is not null — keep it and skip the RHS */` |
|       42 |  6123 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       22 |  6124 | `	}else{` |
|        - |  6125 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       66 |  6126 | `		VmPopOperand(&pTos, 1);` |
|        - |  6127 | `	}` |
|      106 |  6128 | `	break;` |
|        - |  6129 |  |
|        - |  6130 | `/*` |
|        - |  6131 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  6132 | ` * Null coalescing assignment short-circuit.` |
|        - |  6133 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  6134 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  6135 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  6136 | ` */` |
|       23 |  6137 | `case PH7_OP_NULLC_JMP: {` |
|        - |  6138 | `#ifdef UNTRUST` |
|        - |  6139 | `	if( pTos < pStack ){` |
|        - |  6140 | `		goto Abort;` |
|        - |  6141 | `	}` |
|        - |  6142 | `#endif` |
|       47 |  6143 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       19 |  6144 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|        9 |  6145 | `	}` |
|       47 |  6146 | `	break;` |
|        - |  6147 |  |
|        - |  6148 | `/*` |
|        - |  6149 | ` * OP_NULLC_STORE: * * *` |
|        - |  6150 | ` * Null coalescing assignment store.` |
|        - |  6151 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  6152 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  6153 | ` * expression result.` |
|        - |  6154 | ` */` |
|        - |  6155 | `/*` |
|        - |  6156 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  6157 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  6158 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  6159 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  6160 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  6161 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  6162 | ` */` |
|       51 |  6163 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  6164 | `#ifdef UNTRUST` |
|        - |  6165 | `	if( pTos < pStack ){` |
|        - |  6166 | `		goto Abort;` |
|        - |  6167 | `	}` |
|        - |  6168 | `#endif` |
|      104 |  6169 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  6170 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  6171 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  6172 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  6173 | `	}` |
|      104 |  6174 | `	break;` |
|        - |  6175 |  |
|       14 |  6176 | `case PH7_OP_NULLC_STORE: {` |
|       29 |  6177 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6178 | `	ph7_value *pObj;` |
|        - |  6179 | `	sxu32 nIdx;` |
|        - |  6180 | `#ifdef UNTRUST` |
|        - |  6181 | `	if( pNos < pStack ){` |
|        - |  6182 | `		goto Abort;` |
|        - |  6183 | `	}` |
|        - |  6184 | `#endif` |
|       29 |  6185 | `	nIdx = pNos->nIdx;` |
|       29 |  6186 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  6187 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6188 | `			"Cannot perform assignment on a constant class attribute");` |
|       29 |  6189 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       29 |  6190 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       29 |  6191 | `		PH7_MemObjStore(pTos,pObj);` |
|       14 |  6192 | `	}` |
|       29 |  6193 | `	PH7_MemObjStore(pTos,pNos);` |
|       29 |  6194 | `	VmPopOperand(&pTos,1);` |
|       29 |  6195 | `	break;` |
|        - |  6196 |  |
|        - |  6197 | `/*` |
|        - |  6198 | ` * OP_SPREAD: * * *` |
|        - |  6199 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  6200 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  6201 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  6202 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  6203 | ` */` |
|        9 |  6204 | `case PH7_OP_SPREAD: {` |
|        - |  6205 | `#ifdef UNTRUST` |
|        - |  6206 | `	if( pTos < pStack ){` |
|        - |  6207 | `		goto Abort;` |
|        - |  6208 | `	}` |
|        - |  6209 | `#endif` |
|       20 |  6210 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  6211 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  6212 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  6213 | `		if( nEntry == 0 ){` |
|        - |  6214 | `			/* Empty array — remove from stack */` |
|        3 |  6215 | `			VmPopOperand(&pTos, 1);` |
|        3 |  6216 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  6217 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  6218 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  6219 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  6220 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  6221 | `				VM_STACK_GUARD);` |
|      ! 0 |  6222 | `		}else{` |
|        - |  6223 | `			ph7_hashmap_node *pNode2;` |
|        - |  6224 | `			ph7_value *pElem;` |
|        - |  6225 | `			sxu32 i;` |
|        - |  6226 | `			/* Overwrite TOS with first element */` |
|       18 |  6227 | `			pNode2 = pMap->pFirst;` |
|       18 |  6228 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  6229 | `			PH7_MemObjRelease(pTos);` |
|       18 |  6230 | `			if( pElem ){` |
|       18 |  6231 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  6232 | `			}` |
|       18 |  6233 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  6234 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  6235 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  6236 | `			pNode2 = pNode2->pPrev;` |
|        - |  6237 | `			/* Push remaining elements */` |
|       44 |  6238 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  6239 | `				pTos++;` |
|       28 |  6240 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  6241 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  6242 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  6243 | `				if( pElem ){` |
|       28 |  6244 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  6245 | `				}` |
|       28 |  6246 | `				pNode2 = pNode2->pPrev;` |
|       15 |  6247 | `			}` |
|       18 |  6248 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  6249 | `		}` |
|        9 |  6250 | `	}` |
|        - |  6251 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  6252 | `	break;` |
|        - |  6253 |  |
|        - |  6254 | `/*` |
|        - |  6255 | ` * OP_FLAG_SPREAD: * * *` |
|        - |  6256 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - |  6257 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - |  6258 | ` */` |
|       30 |  6259 | `case PH7_OP_FLAG_SPREAD: {` |
|        - |  6260 | `#ifdef UNTRUST` |
|        - |  6261 | `	if( pTos < pStack ){` |
|        - |  6262 | `		goto Abort;` |
|        - |  6263 | `	}` |
|        - |  6264 | `#endif` |
|       62 |  6265 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|       62 |  6266 | `	break;` |
|        - |  6267 |  |
|        - |  6268 | `/* OP_LXOR: * * *` |
|        - |  6269 | ` *` |
|        - |  6270 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  6271 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6272 | ` * stack.` |
|        - |  6273 | ` * According to the PHP language reference manual:` |
|        - |  6274 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  6275 | ` *  TRUE,but not both.` |
|        - |  6276 | ` */` |
|        5 |  6277 | `case PH7_OP_LXOR:{` |
|       11 |  6278 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  6279 | `	sxi32 v = 0;` |
|        - |  6280 | `#ifdef UNTRUST` |
|        - |  6281 | `	if( pNos < pStack ){` |
|        - |  6282 | `		goto Abort;` |
|        - |  6283 | `	}` |
|        - |  6284 | `#endif` |
|        - |  6285 | `	/* Force a boolean cast */` |
|       11 |  6286 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6287 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  6288 | `	}` |
|       11 |  6289 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6290 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6291 | `	}` |
|       11 |  6292 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  6293 | `		v = 1;` |
|        3 |  6294 | `	}` |
|       11 |  6295 | `	VmPopOperand(&pTos,1);` |
|       11 |  6296 | `	pTos->x.iVal = v;` |
|       11 |  6297 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  6298 | `	break;` |
|        - |  6299 | `				 }` |
|        - |  6300 | `/* OP_EQ P1 P2 P3` |
|        - |  6301 | ` *` |
|        - |  6302 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  6303 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6304 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6305 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6306 | ` */` |
|        - |  6307 | `/* OP_NEQ P1 P2 P3` |
|        - |  6308 | ` *` |
|        - |  6309 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  6310 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6311 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6312 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6313 | ` */` |
|     4459 |  6314 | `case PH7_OP_EQ:` |
|        - |  6315 | `case PH7_OP_NEQ: {` |
|     8920 |  6316 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6317 | `	/* Perform the comparison and act accordingly */` |
|        - |  6318 | `#ifdef UNTRUST` |
|        - |  6319 | `	if( pNos < pStack ){` |
|        - |  6320 | `		goto Abort;` |
|        - |  6321 | `	}` |
|        - |  6322 | `#endif` |
|     8920 |  6323 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     8920 |  6324 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  6325 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     8911 |  6326 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     8876 |  6327 | `		rc = rc == 0;` |
|     4439 |  6328 | `	}else{` |
|       28 |  6329 | `		rc = rc != 0;` |
|        - |  6330 | `	}` |
|     8920 |  6331 | `	VmPopOperand(&pTos,1);` |
|     8920 |  6332 | `	if( !pInstr->iP2 ){` |
|        - |  6333 | `		/* Push comparison result without taking the jump */` |
|     8920 |  6334 | `		PH7_MemObjRelease(pTos);` |
|     8920 |  6335 | `		pTos->x.iVal = rc;` |
|        - |  6336 | `		/* Invalidate any prior representation */` |
|     8920 |  6337 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4461 |  6338 | `	}else{` |
|      ! 0 |  6339 | `		if( rc ){` |
|        - |  6340 | `			/* Jump to the desired location */` |
|      ! 0 |  6341 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6342 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6343 | `		}` |
|        - |  6344 | `	}` |
|     8920 |  6345 | `	break;` |
|        - |  6346 | `				 }` |
|        - |  6347 | `/* OP_TEQ P1 P2 *` |
|        - |  6348 | ` *` |
|        - |  6349 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  6350 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6351 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6352 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6353 | ` */` |
|   157220 |  6354 | `case PH7_OP_TEQ: {` |
|   314442 |  6355 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6356 | `	/* Perform the comparison and act accordingly */` |
|        - |  6357 | `#ifdef UNTRUST` |
|        - |  6358 | `	if( pNos < pStack ){` |
|        - |  6359 | `		goto Abort;` |
|        - |  6360 | `	}` |
|        - |  6361 | `#endif` |
|   314442 |  6362 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   314442 |  6363 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6364 | `		rc = 0;` |
|        2 |  6365 | `	}else{` |
|   314440 |  6366 | `		rc = rc == 0;` |
|        - |  6367 | `	}` |
|   314442 |  6368 | `	VmPopOperand(&pTos,1);` |
|   314442 |  6369 | `	if( !pInstr->iP2 ){` |
|        - |  6370 | `		/* Push comparison result without taking the jump */` |
|   314442 |  6371 | `		PH7_MemObjRelease(pTos);` |
|   314442 |  6372 | `		pTos->x.iVal = rc;` |
|        - |  6373 | `		/* Invalidate any prior representation */` |
|   314442 |  6374 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   157222 |  6375 | `	}else{` |
|      ! 0 |  6376 | `		if( rc ){` |
|        - |  6377 | `			/* Jump to the desired location */` |
|      ! 0 |  6378 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6379 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6380 | `		}` |
|        - |  6381 | `	}` |
|   314442 |  6382 | `	break;` |
|        - |  6383 | `				 }` |
|        - |  6384 | `/* OP_TNE P1 P2 *` |
|        - |  6385 | ` *` |
|        - |  6386 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  6387 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  6388 | ` * instruction.` |
|        - |  6389 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6390 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6391 | ` *` |
|        - |  6392 | ` */` |
|   121253 |  6393 | `case PH7_OP_TNE: {` |
|   242508 |  6394 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6395 | `	/* Perform the comparison and act accordingly */` |
|        - |  6396 | `#ifdef UNTRUST` |
|        - |  6397 | `	if( pNos < pStack ){` |
|        - |  6398 | `		goto Abort;` |
|        - |  6399 | `	}` |
|        - |  6400 | `#endif` |
|   242508 |  6401 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   242508 |  6402 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6403 | `		rc = 1;` |
|        2 |  6404 | `	}else{` |
|   242506 |  6405 | `		rc = rc != 0;` |
|        - |  6406 | `	}` |
|   242508 |  6407 | `	VmPopOperand(&pTos,1);` |
|   242508 |  6408 | `	if( !pInstr->iP2 ){` |
|        - |  6409 | `		/* Push comparison result without taking the jump */` |
|   242508 |  6410 | `		PH7_MemObjRelease(pTos);` |
|   242508 |  6411 | `		pTos->x.iVal = rc;` |
|        - |  6412 | `		/* Invalidate any prior representation */` |
|   242508 |  6413 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   121255 |  6414 | `	}else{` |
|      ! 0 |  6415 | `		if( rc ){` |
|        - |  6416 | `			/* Jump to the desired location */` |
|      ! 0 |  6417 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6418 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6419 | `		}` |
|        - |  6420 | `	}` |
|   242508 |  6421 | `	break;` |
|        - |  6422 | `				 }` |
|        - |  6423 | `/* OP_LT P1 P2 P3` |
|        - |  6424 | ` *` |
|        - |  6425 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6426 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6427 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6428 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6429 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6430 | ` *` |
|        - |  6431 | ` */` |
|        - |  6432 | `/* OP_LE P1 P2 P3` |
|        - |  6433 | ` *` |
|        - |  6434 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6435 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6436 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6437 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6438 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6439 | ` *` |
|        - |  6440 | ` */` |
|   112153 |  6441 | `case PH7_OP_LT:` |
|        - |  6442 | `case PH7_OP_LE: {` |
|   224352 |  6443 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6444 | `	/* Perform the comparison and act accordingly */` |
|        - |  6445 | `#ifdef UNTRUST` |
|        - |  6446 | `	if( pNos < pStack ){` |
|        - |  6447 | `		goto Abort;` |
|        - |  6448 | `	}` |
|        - |  6449 | `#endif` |
|   224352 |  6450 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   224352 |  6451 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6452 | `		rc = 0;` |
|   224348 |  6453 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1606 |  6454 | `		rc = rc < 1;` |
|      804 |  6455 | `	}else{` |
|   222740 |  6456 | `		rc = rc < 0;` |
|        - |  6457 | `	}` |
|   224352 |  6458 | `	VmPopOperand(&pTos,1);` |
|   224352 |  6459 | `	if( !pInstr->iP2 ){` |
|        - |  6460 | `		/* Push comparison result without taking the jump */` |
|   224352 |  6461 | `		PH7_MemObjRelease(pTos);` |
|   224352 |  6462 | `		pTos->x.iVal = rc;` |
|        - |  6463 | `		/* Invalidate any prior representation */` |
|   224352 |  6464 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   112199 |  6465 | `	}else{` |
|      ! 0 |  6466 | `		if( rc ){` |
|        - |  6467 | `			/* Jump to the desired location */` |
|      ! 0 |  6468 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6469 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6470 | `		}` |
|        - |  6471 | `	}` |
|   224352 |  6472 | `	break;` |
|        - |  6473 | `				}` |
|        - |  6474 | `/* OP_GT P1 P2 P3` |
|        - |  6475 | ` *` |
|        - |  6476 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6477 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6478 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6479 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6480 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6481 | ` *` |
|        - |  6482 | ` */` |
|        - |  6483 | `/* OP_GE P1 P2 P3` |
|        - |  6484 | ` *` |
|        - |  6485 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6486 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6487 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6488 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6489 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6490 | ` *` |
|        - |  6491 | ` */` |
|    55394 |  6492 | `case PH7_OP_GT:` |
|        - |  6493 | `case PH7_OP_GE: {` |
|   110790 |  6494 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6495 | `	/* Perform the comparison and act accordingly */` |
|        - |  6496 | `#ifdef UNTRUST` |
|        - |  6497 | `	if( pNos < pStack ){` |
|        - |  6498 | `		goto Abort;` |
|        - |  6499 | `	}` |
|        - |  6500 | `#endif` |
|   110790 |  6501 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   110790 |  6502 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6503 | `		rc = 0;` |
|   110786 |  6504 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   110398 |  6505 | `		rc = rc >= 0;` |
|    55200 |  6506 | `	}else{` |
|      386 |  6507 | `		rc = rc > 0;` |
|        - |  6508 | `	}` |
|   110790 |  6509 | `	VmPopOperand(&pTos,1);` |
|   110790 |  6510 | `	if( !pInstr->iP2 ){` |
|        - |  6511 | `		/* Push comparison result without taking the jump */` |
|   110790 |  6512 | `		PH7_MemObjRelease(pTos);` |
|   110790 |  6513 | `		pTos->x.iVal = rc;` |
|        - |  6514 | `		/* Invalidate any prior representation */` |
|   110790 |  6515 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    55396 |  6516 | `	}else{` |
|      ! 0 |  6517 | `		if( rc ){` |
|        - |  6518 | `			/* Jump to the desired location */` |
|      ! 0 |  6519 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6520 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6521 | `		}` |
|        - |  6522 | `	}` |
|   110790 |  6523 | `	break;` |
|        - |  6524 | `				}` |
|        - |  6525 | `/* OP_SPACESHIP * * *` |
|        - |  6526 | ` *` |
|        - |  6527 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  6528 | ` *   -1 if left < right` |
|        - |  6529 | ` *    0 if left == right` |
|        - |  6530 | ` *    1 if left > right` |
|        - |  6531 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  6532 | ` */` |
|       25 |  6533 | `case PH7_OP_SPACESHIP: {` |
|       51 |  6534 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6535 | `#ifdef UNTRUST` |
|        - |  6536 | `	if( pNos < pStack ){` |
|        - |  6537 | `		goto Abort;` |
|        - |  6538 | `	}` |
|        - |  6539 | `#endif` |
|       51 |  6540 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  6541 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  6542 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  6543 | `		rc = 1;` |
|        4 |  6544 | `	}else{` |
|        - |  6545 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  6546 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  6547 | `	}` |
|       51 |  6548 | `	VmPopOperand(&pTos,1);` |
|       51 |  6549 | `	PH7_MemObjRelease(pTos);` |
|       51 |  6550 | `	pTos->x.iVal = rc;` |
|       51 |  6551 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  6552 | `	break;` |
|        - |  6553 | `				}` |
|        - |  6554 | `/* OP_SEQ P1 P2 *` |
|        - |  6555 | ` * Strict string comparison.` |
|        - |  6556 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  6557 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6558 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  6559 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  6560 | ` * use PH7_OP_EQ.` |
|        - |  6561 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6562 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6563 | ` */` |
|        - |  6564 | `/* OP_SNE P1 P2 *` |
|        - |  6565 | ` * Strict string comparison.` |
|        - |  6566 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  6567 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6568 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  6569 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  6570 | ` * use PH7_OP_EQ.` |
|        - |  6571 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6572 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6573 | ` */` |
|       18 |  6574 | `case PH7_OP_SEQ:` |
|        - |  6575 | `case PH7_OP_SNE: {` |
|       38 |  6576 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6577 | `	SyString s1,s2;` |
|        - |  6578 | `	/* Perform the comparison and act accordingly */` |
|        - |  6579 | `#ifdef UNTRUST` |
|        - |  6580 | `	if( pNos < pStack ){` |
|        - |  6581 | `		goto Abort;` |
|        - |  6582 | `	}` |
|        - |  6583 | `#endif` |
|        - |  6584 | `	/* Force a string cast */` |
|       38 |  6585 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  6586 | `		PH7_MemObjToString(pTos);` |
|        2 |  6587 | `	}` |
|       38 |  6588 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  6589 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6590 | `	}` |
|       38 |  6591 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  6592 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  6593 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  6594 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  6595 | `		rc = rc != 0;` |
|      ! 0 |  6596 | `	}else{` |
|       38 |  6597 | `		rc = rc == 0;` |
|        - |  6598 | `	}` |
|       38 |  6599 | `	VmPopOperand(&pTos,1);` |
|       38 |  6600 | `	if( !pInstr->iP2 ){` |
|        - |  6601 | `		/* Push comparison result without taking the jump */` |
|       38 |  6602 | `		PH7_MemObjRelease(pTos);` |
|       38 |  6603 | `		pTos->x.iVal = rc;` |
|        - |  6604 | `		/* Invalidate any prior representation */` |
|       38 |  6605 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  6606 | `	}else{` |
|      ! 0 |  6607 | `		if( rc ){` |
|        - |  6608 | `			/* Jump to the desired location */` |
|      ! 0 |  6609 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6610 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6611 | `		}` |
|        - |  6612 | `	}` |
|       38 |  6613 | `	break;` |
|        - |  6614 | `				 }` |
|        - |  6615 | `/*` |
|        - |  6616 | ` * OP_LOAD_REF * * *` |
|        - |  6617 | ` * Push the index of a referenced object on the stack.` |
|        - |  6618 | ` */` |
|       57 |  6619 | `case PH7_OP_LOAD_REF: {` |
|        - |  6620 | `	sxu32 nIdx;` |
|        - |  6621 | `#ifdef UNTRUST` |
|        - |  6622 | `	if( pTos < pStack ){` |
|        - |  6623 | `		goto Abort;` |
|        - |  6624 | `	}` |
|        - |  6625 | `#endif` |
|        - |  6626 | `	/* Extract memory object index */` |
|      115 |  6627 | `	nIdx = pTos->nIdx;` |
|      115 |  6628 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  6629 | `		/* Nullify the object */` |
|       95 |  6630 | `		PH7_MemObjRelease(pTos);` |
|        - |  6631 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  6632 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  6633 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  6634 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  6635 | `	}` |
|      115 |  6636 | `	break;` |
|        - |  6637 | `					  }` |
|        - |  6638 | `/*` |
|        - |  6639 | ` * OP_STORE_REF * * P3` |
|        - |  6640 | ` * Perform an assignment operation by reference.` |
|        - |  6641 | ` */` |
|       16 |  6642 | ` case PH7_OP_STORE_REF: {` |
|       34 |  6643 | `	 SyString sName = { 0 , 0 };` |
|        - |  6644 | `	 VmFrame *pFrameLocal;` |
|        - |  6645 | `	SyHashEntry *pEntry;` |
|        - |  6646 | `	sxu32 nIdx;` |
|        - |  6647 | `#ifdef UNTRUST` |
|        - |  6648 | `	if( pTos < pStack ){` |
|        - |  6649 | `		goto Abort;` |
|        - |  6650 | `	}` |
|        - |  6651 | `#endif` |
|       34 |  6652 | `	if( pInstr->p3 == 0 ){` |
|        - |  6653 | `		char *zName;` |
|        - |  6654 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  6655 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6656 | `			/* Force a string cast */` |
|      ! 0 |  6657 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6658 | `		}` |
|      ! 0 |  6659 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6660 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  6661 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6662 | `			if( zName ){` |
|      ! 0 |  6663 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6664 | `			}` |
|      ! 0 |  6665 | `		}` |
|      ! 0 |  6666 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6667 | `		pTos--;` |
|      ! 0 |  6668 | `	}else{` |
|       34 |  6669 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  6670 | `	}` |
|       34 |  6671 | `	nIdx = pTos->nIdx;` |
|       34 |  6672 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  6673 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6674 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6675 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  6676 | `		}else{` |
|        - |  6677 | `			ph7_value *pObj;` |
|        - |  6678 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  6679 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  6680 | `			if( pObj == 0 ){` |
|      ! 0 |  6681 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6682 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  6683 | `				goto Abort;` |
|        - |  6684 | `			}` |
|        - |  6685 | `			/* Perform the store operation */` |
|      ! 0 |  6686 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  6687 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  6688 | `		}` |
|       34 |  6689 | `	}else if( sName.nByte > 0){` |
|       34 |  6690 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  6691 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  6692 | `		}else{` |
|       34 |  6693 | `			pFrameLocal = pVm->pFrame;` |
|       34 |  6694 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  6695 | `			/* Query the local frame */` |
|       34 |  6696 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       34 |  6697 | `			if( pEntry ){` |
|      ! 0 |  6698 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  6699 | `			}else{` |
|       34 |  6700 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       34 |  6701 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  6702 | `					/* Insert in the $GLOBALS array */` |
|       30 |  6703 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       14 |  6704 | `				}` |
|       34 |  6705 | `				if( rc == SXRET_OK ){` |
|       34 |  6706 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       16 |  6707 | `				}` |
|        - |  6708 | `			}` |
|        - |  6709 | `		}` |
|       16 |  6710 | `	}` |
|       34 |  6711 | `	break;` |
|        - |  6712 | `				 }` |
|        - |  6713 | `/*` |
|        - |  6714 | ` * OP_UPLINK P1 * *` |
|        - |  6715 | ` * Link a variable to the top active VM frame.` |
|        - |  6716 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  6717 | ` */` |
|       28 |  6718 | `case PH7_OP_UPLINK: {` |
|       58 |  6719 | `	if( pVm->pFrame->pParent ){` |
|       58 |  6720 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  6721 | `		SyString sName;` |
|        - |  6722 | `		/* Perform the link */` |
|      116 |  6723 | `		while( pLink <= pTos ){` |
|       60 |  6724 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6725 | `				/* Force a string cast */` |
|      ! 0 |  6726 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  6727 | `			}` |
|       60 |  6728 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       60 |  6729 | `			if( sName.nByte > 0 ){` |
|       60 |  6730 | `				VmFrameLink(&(*pVm),&sName);` |
|       29 |  6731 | `			}` |
|       60 |  6732 | `			pLink++;` |
|        2 |  6733 | `		}` |
|       28 |  6734 | `	}` |
|       58 |  6735 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       58 |  6736 | `	break;` |
|        - |  6737 | `					}` |
|        - |  6738 | `/*` |
|        - |  6739 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  6740 | ` * Push an exception in the corresponding container so that` |
|        - |  6741 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  6742 | ` */` |
|      157 |  6743 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      316 |  6744 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  6745 | `	VmFrame *pFrameLocal;` |
|        - |  6746 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      316 |  6747 | `	pException->iFinallyDone = 0;` |
|      316 |  6748 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  6749 | `	/* Create the exception frame */` |
|      316 |  6750 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      316 |  6751 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  6752 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  6753 | `		goto Abort;` |
|        - |  6754 | `	}` |
|        - |  6755 | `	/* Mark the special frame */` |
|      316 |  6756 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      316 |  6757 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  6758 | `	/* Point to the frame that trigger the exception */` |
|      316 |  6759 | `	pFrameLocal = pFrameLocal->pParent;` |
|      316 |  6760 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      316 |  6761 | `	pException->pFrame = pFrameLocal;` |
|      316 |  6762 | `	break;` |
|        - |  6763 | `							}` |
|        - |  6764 | `/*` |
|        - |  6765 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  6766 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  6767 | ` */` |
|      156 |  6768 | `case PH7_OP_POP_EXCEPTION: {` |
|      314 |  6769 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      314 |  6770 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  6771 | `		ph7_exception **apException;` |
|        - |  6772 | `		/* Pop the loaded exception */` |
|       32 |  6773 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       32 |  6774 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       30 |  6775 | `			(void)SySetPop(&pVm->aException);` |
|       14 |  6776 | `		}` |
|       15 |  6777 | `	}` |
|      314 |  6778 | `	pException->pFrame = 0;` |
|        - |  6779 | `	/* Leave the exception frame */` |
|      314 |  6780 | `	VmLeaveFrame(&(*pVm));` |
|        - |  6781 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      314 |  6782 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  6783 | `		sxi32 rcFinally;` |
|       20 |  6784 | `		pException->iFinallyDone = 1;` |
|       20 |  6785 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  6786 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  6787 | `			goto Abort;` |
|        - |  6788 | `		}` |
|        9 |  6789 | `	}` |
|      314 |  6790 | `	break;` |
|        - |  6791 | `							}` |
|        - |  6792 |  |
|        - |  6793 | `/*` |
|        - |  6794 | ` * OP_THROW * P2 *` |
|        - |  6795 | ` * Throw an user exception.` |
|        - |  6796 | ` */` |
|       58 |  6797 | `case PH7_OP_THROW: {` |
|      118 |  6798 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      118 |  6799 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  6800 | `#ifdef UNTRUST` |
|        - |  6801 | `	if( pTos < pStack ){` |
|        - |  6802 | `		goto Abort;` |
|        - |  6803 | `	}` |
|        - |  6804 | `#endif` |
|      118 |  6805 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  6806 | `	/* Tell the upper layer that an exception was thrown */` |
|      118 |  6807 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      118 |  6808 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      118 |  6809 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6810 | `		ph7_class *pThrowable;` |
|        - |  6811 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      118 |  6812 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      119 |  6813 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  6814 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  6815 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  6816 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  6817 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  6818 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  6819 | `			if( pErrorClass ){` |
|        3 |  6820 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  6821 | `			}` |
|        3 |  6822 | `			if( pErrInst ){` |
|        - |  6823 | `				ph7_class_method *pCons;` |
|        3 |  6824 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  6825 | `				if( pCons ){` |
|        - |  6826 | `					ph7_value sArg;` |
|        - |  6827 | `					ph7_value *apArg[1];` |
|        - |  6828 | `					SyString sMsgStr;` |
|        - |  6829 | `					static const char zErrMsg[] =` |
|        - |  6830 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  6831 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  6832 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  6833 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  6834 | `					apArg[0] = &sArg;` |
|        3 |  6835 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  6836 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  6837 | `				}` |
|        3 |  6838 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  6839 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  6840 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6841 | `					goto Abort;` |
|        - |  6842 | `				}` |
|        2 |  6843 | `			}else{` |
|        - |  6844 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  6845 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  6846 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6847 | `					goto Abort;` |
|        - |  6848 | `				}` |
|        - |  6849 | `			}` |
|        2 |  6850 | `		}else{` |
|        - |  6851 | `			/* Throw the exception */` |
|      116 |  6852 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      116 |  6853 | `			if( rc == SXERR_ABORT ){` |
|        - |  6854 | `				/* Abort processing immediately */` |
|       11 |  6855 | `				goto Abort;` |
|        - |  6856 | `			}` |
|        - |  6857 | `		}` |
|       55 |  6858 | `	}else{` |
|        - |  6859 | `		/* Expecting a class instance */` |
|      ! 0 |  6860 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  6861 | `		if( rc == SXERR_ABORT ){` |
|        - |  6862 | `			/* Abort processing immediately */` |
|      ! 0 |  6863 | `			goto Abort;` |
|        - |  6864 | `		}` |
|        - |  6865 | `	}` |
|        - |  6866 | `	/* Pop the top entry */` |
|      108 |  6867 | `	VmPopOperand(&pTos,1);` |
|        - |  6868 | `	/* Perform an unconditional jump */` |
|      108 |  6869 | `	pc = nJump - 1;` |
|      108 |  6870 | `	break;` |
|        - |  6871 | `				   }` |
|        - |  6872 | `/*` |
|        - |  6873 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  6874 | ` * Prepare a foreach step.` |
|        - |  6875 | ` */` |
|     5973 |  6876 | `case PH7_OP_FOREACH_INIT: {` |
|    11948 |  6877 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  6878 | `	void *pName;` |
|        - |  6879 | `#ifdef UNTRUST` |
|        - |  6880 | `	if( pTos < pStack ){` |
|        - |  6881 | `		goto Abort;` |
|        - |  6882 | `	}` |
|        - |  6883 | `#endif` |
|    11948 |  6884 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6885 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  6886 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6887 | `			/* Force a string cast */` |
|      ! 0 |  6888 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6889 | `		}` |
|        - |  6890 | `		/* Duplicate name */` |
|      ! 0 |  6891 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6892 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6893 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6894 | `		}` |
|      ! 0 |  6895 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6896 | `	}` |
|    11948 |  6897 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  6898 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6899 | `			/* Force a string cast */` |
|      ! 0 |  6900 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6901 | `		}` |
|        - |  6902 | `		/* Duplicate name */` |
|      ! 0 |  6903 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6904 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6905 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6906 | `		}` |
|      ! 0 |  6907 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6908 | `	}` |
|        - |  6909 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    11948 |  6910 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6911 | `		/* Jump out of the loop */` |
|      ! 0 |  6912 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6913 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  6914 | `		}` |
|      ! 0 |  6915 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  6916 | `	}else{` |
|        - |  6917 | `		ph7_foreach_step *pStep;` |
|    11948 |  6918 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    11948 |  6919 | `		if( pStep == 0 ){` |
|      ! 0 |  6920 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  6921 | `			/* Jump out of the loop */` |
|      ! 0 |  6922 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6923 | `		}else{` |
|        - |  6924 | `			/* Zero the structure */` |
|    11948 |  6925 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  6926 | `			/* Prepare the step */` |
|    11948 |  6927 | `			pStep->iFlags = pInfo->iFlags;` |
|    11948 |  6928 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6929 | `				ph7_hashmap *pMap;` |
|        - |  6930 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  6931 | `				 * source array so mutations don't affect other sharers. */` |
|    11916 |  6932 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  6933 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  6934 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  6935 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6936 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  6937 | `						 * variable still points at the same hashmap as` |
|        - |  6938 | `						 * the stack value. */` |
|        9 |  6939 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  6940 | `							pCur->iRef--;` |
|        9 |  6941 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  6942 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  6943 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  6944 | `						}` |
|        4 |  6945 | `					}` |
|        4 |  6946 | `				}` |
|    11916 |  6947 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6948 | `				/* Reset the internal loop cursor */` |
|    11916 |  6949 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6950 | `				/* Mark the step */` |
|    11916 |  6951 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    11916 |  6952 | `				pStep->xIter.pMap = pMap;` |
|    11916 |  6953 | `				pMap->iRef++;` |
|     5959 |  6954 | `			}else{` |
|       34 |  6955 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6956 | `				ph7_class *pIteratorClass;` |
|        - |  6957 | `				/* Check if the object implements Iterator */` |
|       34 |  6958 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       45 |  6959 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  6960 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  6961 | `					ph7_class_method *pRewind;` |
|       24 |  6962 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  6963 | `					pStep->xIter.pThis = pThis;` |
|       24 |  6964 | `					pThis->iRef++;` |
|       24 |  6965 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  6966 | `					if( pRewind ){` |
|       24 |  6967 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  6968 | `					}` |
|       13 |  6969 | `				}else{` |
|        - |  6970 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  6971 | `					ph7_class *pIterAggClass;` |
|       12 |  6972 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  6973 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  6974 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  6975 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  6976 | `						ph7_class_method *pGetIter;` |
|        3 |  6977 | `						int iterAggOk = 0;` |
|        3 |  6978 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  6979 | `						if( pGetIter ){` |
|        - |  6980 | `							ph7_value sResult;` |
|        3 |  6981 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  6982 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  6983 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  6984 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  6985 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  6986 | `									ph7_class_method *pRewind;` |
|        3 |  6987 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  6988 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  6989 | `									pIterObj->iRef++;` |
|        - |  6990 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  6991 | `									pStep->pOwner = pThis;` |
|        3 |  6992 | `									pThis->iRef++;` |
|        3 |  6993 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  6994 | `									if( pRewind ){` |
|        3 |  6995 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  6996 | `									}` |
|        3 |  6997 | `									iterAggOk = 1;` |
|        1 |  6998 | `								}` |
|        1 |  6999 | `							}` |
|        3 |  7000 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  7001 | `						}` |
|        3 |  7002 | `						if( !iterAggOk ){` |
|        - |  7003 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  7004 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7005 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  7006 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  7007 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  7008 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  7009 | `						}` |
|        2 |  7010 | `					}else{` |
|        - |  7011 | `						/* Plain object iteration via hAttr */` |
|        9 |  7012 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  7013 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  7014 | `						pStep->xIter.pThis = pThis;` |
|        9 |  7015 | `						pThis->iRef++;` |
|        - |  7016 | `					}` |
|        - |  7017 | `				}` |
|        - |  7018 | `			}` |
|        - |  7019 | `		}` |
|    11948 |  7020 | `		if( pStep ){` |
|    11948 |  7021 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  7022 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  7023 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  7024 | `				/* Jump out of the loop */` |
|      ! 0 |  7025 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  7026 | `			}` |
|     5973 |  7027 | `		}` |
|        - |  7028 | `	}` |
|    11948 |  7029 | `	VmPopOperand(&pTos,1);` |
|    11948 |  7030 | `	break;` |
|        - |  7031 | `						  }` |
|        - |  7032 | `/*` |
|        - |  7033 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  7034 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  7035 | ` */` |
|    97598 |  7036 | `case PH7_OP_FOREACH_STEP: {` |
|   195198 |  7037 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7038 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  7039 | `	ph7_value *pValue;` |
|        - |  7040 | `	VmFrame *pFrameLocal;` |
|        - |  7041 | `	/* Peek the last step */` |
|   195198 |  7042 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   195198 |  7043 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   195198 |  7044 | `	pFrameLocal = pVm->pFrame;` |
|   195198 |  7045 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   195198 |  7046 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   195070 |  7047 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  7048 | `		ph7_hashmap_node *pNode;` |
|        - |  7049 | `		/* Extract the current node value */` |
|   195070 |  7050 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   195070 |  7051 | `		if( pNode == 0 ){` |
|        - |  7052 | `			/* No more entry to process */` |
|    11914 |  7053 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    11914 |  7054 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7055 | `				/* Break the reference with the last element */` |
|        7 |  7056 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  7057 | `			}` |
|        - |  7058 | `			/* Automatically reset the loop cursor */` |
|    11914 |  7059 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7060 | `			/* Cleanup the mess left behind */` |
|    11914 |  7061 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    11914 |  7062 | `			SySetPop(&pInfo->aStep);` |
|    11914 |  7063 | `			PH7_HashmapUnref(pMap);` |
|     5958 |  7064 | `		}else{` |
|   183158 |  7065 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      506 |  7066 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      506 |  7067 | `				if( pKey ){` |
|      506 |  7068 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      252 |  7069 | `				}` |
|      252 |  7070 | `			}` |
|   183158 |  7071 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7072 | `				SyHashEntry *pEntry;` |
|        - |  7073 | `				/* Pass by reference */` |
|       23 |  7074 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  7075 | `				if( pEntry ){` |
|       21 |  7076 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  7077 | `				}else{` |
|        4 |  7078 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  7079 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  7080 | `				}` |
|       12 |  7081 | `			}else{` |
|        - |  7082 | `				/* Make a copy of the entry value */` |
|   183136 |  7083 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   183136 |  7084 | `				if( pValue ){` |
|   183136 |  7085 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    91567 |  7086 | `				}` |
|        - |  7087 | `			}` |
|        2 |  7088 | `		}` |
|    97664 |  7089 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  7090 | `		/* Iterator-based iteration.` |
|        - |  7091 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  7092 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  7093 | `		 */` |
|      106 |  7094 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  7095 | `		ph7_class_method *pMethod;` |
|        - |  7096 | `		ph7_value sResult;` |
|      106 |  7097 | `		int isValid = 0;` |
|        - |  7098 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  7099 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  7100 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  7101 | `		}else{` |
|       82 |  7102 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  7103 | `			if( pMethod ){` |
|       82 |  7104 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  7105 | `			}` |
|        - |  7106 | `		}` |
|        - |  7107 | `		/* Call valid() */` |
|      106 |  7108 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  7109 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  7110 | `		if( pMethod ){` |
|      106 |  7111 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  7112 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  7113 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  7114 | `		}` |
|      106 |  7115 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  7116 | `		if( !isValid ){` |
|        - |  7117 | `			/* Iterator exhausted */` |
|       24 |  7118 | `			pc = pInstr->iP2 - 1;` |
|        - |  7119 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  7120 | `			if( pStep->pOwner ){` |
|        3 |  7121 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  7122 | `			}` |
|       24 |  7123 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  7124 | `			SySetPop(&pInfo->aStep);` |
|       24 |  7125 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  7126 | `		}else{` |
|        - |  7127 | `			/* Call current() to get value */` |
|       84 |  7128 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  7129 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  7130 | `			if( pMethod ){` |
|       84 |  7131 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  7132 | `			}` |
|       84 |  7133 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  7134 | `			if( pValue ){` |
|       84 |  7135 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  7136 | `			}` |
|       84 |  7137 | `			PH7_MemObjRelease(&sResult);` |
|        - |  7138 | `			/* Call key() if needed */` |
|       84 |  7139 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  7140 | `				ph7_value sKey;` |
|       35 |  7141 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  7142 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  7143 | `				if( pMethod ){` |
|       35 |  7144 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  7145 | `				}` |
|       35 |  7146 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  7147 | `				if( pValue ){` |
|       35 |  7148 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  7149 | `				}` |
|       35 |  7150 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  7151 | `			}` |
|        - |  7152 | `		}` |
|       54 |  7153 | `	}else{` |
|       25 |  7154 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  7155 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  7156 | `		SyHashEntry *pEntry;` |
|        - |  7157 | `		/* Point to the next attribute */` |
|       29 |  7158 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  7159 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  7160 | `			/* Check access permission */` |
|       31 |  7161 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  7162 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  7163 | `					break; /* Access is granted */` |
|        - |  7164 | `			}` |
|        1 |  7165 | `		}` |
|       25 |  7166 | `		if( pEntry == 0 ){` |
|        - |  7167 | `			/* Clean up the mess left behind */` |
|        9 |  7168 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  7169 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7170 | `				/* Break the reference with the last element */` |
|        3 |  7171 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  7172 | `			}` |
|        9 |  7173 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  7174 | `			SySetPop(&pInfo->aStep);` |
|        9 |  7175 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  7176 | `		}else{` |
|       17 |  7177 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  7178 | `			ph7_value *pAttrValue;` |
|       17 |  7179 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  7180 | `				/* Fill with the current attribute name */` |
|       17 |  7181 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  7182 | `				if( pKey ){` |
|       17 |  7183 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  7184 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  7185 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  7186 | `				}` |
|        8 |  7187 | `			}` |
|        - |  7188 | `			/* Extract attribute value */` |
|       17 |  7189 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  7190 | `			if( pAttrValue ){` |
|       17 |  7191 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7192 | `					/* Pass by reference */` |
|        3 |  7193 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  7194 | `					if( pEntry ){` |
|        3 |  7195 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  7196 | `					}else{` |
|      ! 0 |  7197 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  7198 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  7199 | `					}` |
|        2 |  7200 | `				}else{` |
|        - |  7201 | `					/* Make a copy of the attribute value */` |
|       15 |  7202 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  7203 | `					if( pValue ){` |
|       15 |  7204 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  7205 | `					}` |
|        - |  7206 | `				}` |
|        8 |  7207 | `			}` |
|        - |  7208 | `		}` |
|        - |  7209 | `	}` |
|   195198 |  7210 | `	break;` |
|        - |  7211 | `						  }` |
|        - |  7212 | `/*` |
|        - |  7213 | ` * OP_MEMBER P1 P2` |
|        - |  7214 | ` * Load class attribute/method on the stack.` |
|        - |  7215 | ` */` |
|     3679 |  7216 | `case PH7_OP_MEMBER: {` |
|        - |  7217 | `	ph7_class_instance *pThis;` |
|        - |  7218 | `	ph7_value *pNos;` |
|        - |  7219 | `	SyString sName;` |
|     7360 |  7220 | `	if( !pInstr->iP1 ){` |
|     7134 |  7221 | `		pNos = &pTos[-1];` |
|        - |  7222 | `#ifdef UNTRUST` |
|        - |  7223 | `		if( pNos < pStack ){` |
|        - |  7224 | `			goto Abort;` |
|        - |  7225 | `		}` |
|        - |  7226 | `#endif` |
|     7134 |  7227 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7228 | `			ph7_class *pClass;` |
|        - |  7229 | `			/* Class already instantiated */` |
|     7132 |  7230 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  7231 | `			/* Point to the instantiated class */` |
|     7132 |  7232 | `			pClass = pThis->pClass;` |
|        - |  7233 | `			/* Extract attribute name first */` |
|     7132 |  7234 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     7132 |  7235 | `			if( pInstr->iP2 ){` |
|        - |  7236 | `				/* Method call */` |
|      734 |  7237 | `				ph7_class_method *pMeth = 0;` |
|      734 |  7238 | `				if( sName.nByte > 0 ){` |
|        - |  7239 | `					/* Extract the target method */` |
|      734 |  7240 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      366 |  7241 | `				}` |
|      734 |  7242 | `				if( pMeth == 0 ){` |
|      ! 0 |  7243 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  7244 | `						&pClass->sName,&sName` |
|        - |  7245 | `						);` |
|        - |  7246 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  7247 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  7248 | `					/* Pop the method name from the stack */` |
|      ! 0 |  7249 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7250 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  7251 | `				}else{` |
|        - |  7252 | `					/* Push method name on the stack */` |
|      734 |  7253 | `					PH7_MemObjRelease(pTos);` |
|      734 |  7254 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      734 |  7255 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7256 | `				}` |
|      734 |  7257 | `				pTos->nIdx = SXU32_HIGH;` |
|      368 |  7258 | `			}else{` |
|        - |  7259 | `				/* Attribute access */` |
|     6400 |  7260 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  7261 | `				SyHashEntry *pEntry;` |
|        - |  7262 | `				/* Extract the target attribute */` |
|     6400 |  7263 | `				if( sName.nByte > 0 ){` |
|     6400 |  7264 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     6400 |  7265 | `					if( pEntry ){` |
|        - |  7266 | `						/* Point to the attribute value */` |
|     6398 |  7267 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3198 |  7268 | `					}` |
|     3199 |  7269 | `				}` |
|     6400 |  7270 | `				if( pObjAttr == 0 ){` |
|        - |  7271 | `					/* No such attribute,load null */` |
|        4 |  7272 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  7273 | `						&pClass->sName,&sName);` |
|        - |  7274 | `					/* Call the __get magic method if available */` |
|        3 |  7275 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  7276 | `				}` |
|     6400 |  7277 | `				VmPopOperand(&pTos,1);` |
|        - |  7278 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  7279 | `				 * This is due to the following case:` |
|        - |  7280 | `				 *     (new TestClass())->foo;` |
|        - |  7281 | `				 */` |
|     6400 |  7282 | `				pThis->iRef++;` |
|     6400 |  7283 | `				PH7_MemObjRelease(pTos);` |
|     6400 |  7284 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     6400 |  7285 | `				if( pObjAttr ){` |
|     6398 |  7286 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  7287 | `					/* Check attribute access */` |
|     6398 |  7288 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  7289 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  7290 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  7291 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  7292 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  7293 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     6396 |  7294 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     3237 |  7295 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       76 |  7296 | `							VmInstr *pNext = pInstr + 1;` |
|       76 |  7297 | `							int bIsLhs = 0;` |
|       76 |  7298 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       74 |  7299 | `								bIsLhs = 1;` |
|       36 |  7300 | `							}` |
|       76 |  7301 | `							if( !bIsLhs ){` |
|        3 |  7302 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  7303 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  7304 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  7305 | `									goto Abort;` |
|        - |  7306 | `								}` |
|        - |  7307 | `								{` |
|        3 |  7308 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7309 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7310 | `										pc = pFrm2->iExceptionJump - 1;` |
|     3679 |  7311 | `										break;` |
|        - |  7312 | `									}` |
|        - |  7313 | `								}` |
|      ! 0 |  7314 | `								goto Exception;` |
|        - |  7315 | `							}` |
|       36 |  7316 | `						}` |
|        - |  7317 | `						/* Load attribute */` |
|     6396 |  7318 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     6396 |  7319 | `						if( pValue ){` |
|     6396 |  7320 | `							if( pThis->iRef < 2 ){` |
|        - |  7321 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  7322 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  7323 | `								 */` |
|        7 |  7324 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  7325 | `							}else{` |
|        - |  7326 | `								/* Simple load */` |
|     6390 |  7327 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  7328 | `							}` |
|     6396 |  7329 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     6394 |  7330 | `								if( pThis->iRef > 1 ){` |
|        - |  7331 | `									/* Load attribute index */` |
|     6388 |  7332 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     3193 |  7333 | `								}` |
|     3196 |  7334 | `							}` |
|     3197 |  7335 | `						}` |
|     3199 |  7336 | `					}else{` |
|        - |  7337 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  7338 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  7339 | `						char zMsg[256];` |
|      ! 0 |  7340 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  7341 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7342 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7343 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  7344 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7345 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  7346 | `						goto Abort;` |
|        - |  7347 | `					}` |
|     3197 |  7348 | `				}` |
|        - |  7349 | `				/* Safely unreference the object */` |
|     6398 |  7350 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  7351 | `			}` |
|     3566 |  7352 | `		}else{` |
|        3 |  7353 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  7354 | `			VmPopOperand(&pTos,1);` |
|        3 |  7355 | `			PH7_MemObjRelease(pTos);` |
|        3 |  7356 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  7357 | `		}` |
|     3567 |  7358 | `	}else{` |
|        - |  7359 | `		/* Static member access using class name */` |
|      228 |  7360 | `		pNos = pTos;` |
|      228 |  7361 | `		pThis = 0;` |
|      228 |  7362 | `		if( !pInstr->p3 ){` |
|      190 |  7363 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      190 |  7364 | `			pNos--;` |
|        - |  7365 | `#ifdef UNTRUST` |
|        - |  7366 | `			if( pNos < pStack ){` |
|        - |  7367 | `				goto Abort;` |
|        - |  7368 | `			}` |
|        - |  7369 | `#endif` |
|       96 |  7370 | `		}else{` |
|        - |  7371 | `			/* Attribute name already computed */` |
|       40 |  7372 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7373 | `		}` |
|      228 |  7374 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      228 |  7375 | `			ph7_class *pClass = 0;` |
|      228 |  7376 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7377 | `				/* Class already instantiated */` |
|        5 |  7378 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  7379 | `				pClass = pThis->pClass;` |
|        5 |  7380 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  7381 | `			}else{` |
|        - |  7382 | `				/* Try to extract the target class */` |
|      224 |  7383 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      224 |  7384 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      224 |  7385 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  7386 | `					/* Handle self/static/parent keywords */` |
|      224 |  7387 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  7388 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  7389 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  7390 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  7391 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  7392 | `						}` |
|      194 |  7393 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  7394 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      164 |  7395 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       28 |  7396 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       28 |  7397 | `						if( pSelf && pSelf->pBase ){` |
|       28 |  7398 | `							pClass = pSelf->pBase;` |
|       13 |  7399 | `						}` |
|       15 |  7400 | `					}else{` |
|      112 |  7401 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  7402 | `					}` |
|      111 |  7403 | `				}` |
|        - |  7404 | `			}` |
|      228 |  7405 | `			if( pClass == 0 ){` |
|        - |  7406 | `				/* Undefined class */` |
|      ! 0 |  7407 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  7408 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  7409 | `					);` |
|      ! 0 |  7410 | `				if( !pInstr->p3 ){` |
|      ! 0 |  7411 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7412 | `				}` |
|      ! 0 |  7413 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  7414 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  7415 | `			}else{` |
|      228 |  7416 | `				if( pInstr->iP2 ){` |
|        - |  7417 | `					/* Method call */` |
|       86 |  7418 | `					ph7_class_method *pMeth = 0;` |
|       86 |  7419 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  7420 | `						/* Extract the target method */` |
|       86 |  7421 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  7422 | `					}` |
|       86 |  7423 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  7424 | `						if( pMeth ){` |
|      ! 0 |  7425 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  7426 | `								&pClass->sName,&sName` |
|        - |  7427 | `								);` |
|      ! 0 |  7428 | `						}else{` |
|      ! 0 |  7429 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7430 | `								&pClass->sName,&sName` |
|        - |  7431 | `								);` |
|        - |  7432 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  7433 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  7434 | `						}` |
|        - |  7435 | `						/* Pop the method name from the stack */` |
|      ! 0 |  7436 | `						if( !pInstr->p3 ){` |
|      ! 0 |  7437 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  7438 | `						}` |
|      ! 0 |  7439 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  7440 | `					}else{` |
|        - |  7441 | `						/* Push method name on the stack */` |
|       86 |  7442 | `						PH7_MemObjRelease(pTos);` |
|       86 |  7443 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       86 |  7444 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7445 | `					}` |
|       86 |  7446 | `					pTos->nIdx = SXU32_HIGH;` |
|       44 |  7447 | `				}else{` |
|        - |  7448 | `					/* Attribute access */` |
|      144 |  7449 | `					ph7_class_attr *pAttr = 0;` |
|        - |  7450 | `					/* Check for special ::class pseudo-constant */` |
|      190 |  7451 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  7452 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  7453 | `						/* ::class returns the fully qualified class name */` |
|        - |  7454 | `						/* Pop the attribute name from the stack */` |
|       60 |  7455 | `						if( !pInstr->p3 ){` |
|       60 |  7456 | `							VmPopOperand(&pTos,1);` |
|       29 |  7457 | `						}` |
|       60 |  7458 | `						PH7_MemObjRelease(pTos);` |
|        - |  7459 | `						/* Load the class name */` |
|       60 |  7460 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  7461 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  7462 | `					}else{` |
|        - |  7463 | `						/* Extract the target attribute */` |
|       86 |  7464 | `						if( sName.nByte > 0 ){` |
|       86 |  7465 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       42 |  7466 | `						}` |
|       86 |  7467 | `						if( pAttr == 0 ){` |
|        - |  7468 | `							/* No such attribute,load null */` |
|      ! 0 |  7469 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7470 | `								&pClass->sName,&sName);` |
|        - |  7471 | `							/* Call the __get magic method if available */` |
|      ! 0 |  7472 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  7473 | `						}` |
|        - |  7474 | `						/* Pop the attribute name from the stack */` |
|       86 |  7475 | `						if( !pInstr->p3 ){` |
|       48 |  7476 | `							VmPopOperand(&pTos,1);` |
|       23 |  7477 | `						}` |
|       86 |  7478 | `						PH7_MemObjRelease(pTos);` |
|       86 |  7479 | `						pTos->nIdx = SXU32_HIGH;` |
|       86 |  7480 | `						if( pAttr ){` |
|       86 |  7481 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  7482 | `								/* Access to a non static attribute */` |
|      ! 0 |  7483 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7484 | `									&pClass->sName,&pAttr->sName` |
|        - |  7485 | `									);` |
|      ! 0 |  7486 | `							}else{` |
|        - |  7487 | `								ph7_value *pValue;` |
|        - |  7488 | `								/* Check if the access to the attribute is allowed */` |
|       86 |  7489 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  7490 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  7491 | `									 * Same LHS-of-store peek as the instance path. */` |
|       80 |  7492 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       55 |  7493 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       41 |  7494 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       26 |  7495 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       28 |  7496 | `										if( pS ){` |
|       28 |  7497 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       28 |  7498 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  7499 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  7500 | `												int bIsLhs = 0;` |
|        8 |  7501 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  7502 | `													bIsLhs = 1;` |
|        2 |  7503 | `												}` |
|        8 |  7504 | `												if( !bIsLhs ){` |
|        3 |  7505 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  7506 | `													if( pThis ){` |
|      ! 0 |  7507 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7508 | `													}` |
|        3 |  7509 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  7510 | `														goto Abort;` |
|        - |  7511 | `													}` |
|        - |  7512 | `													{` |
|        3 |  7513 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7514 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7515 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  7516 | `															break;` |
|        - |  7517 | `														}` |
|        - |  7518 | `													}` |
|      ! 0 |  7519 | `													goto Exception;` |
|        - |  7520 | `												}` |
|        2 |  7521 | `											}` |
|       12 |  7522 | `										}` |
|       12 |  7523 | `									}` |
|        - |  7524 | `									/* Load the desired attribute */` |
|       80 |  7525 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       80 |  7526 | `									if( pValue ){` |
|       80 |  7527 | `										PH7_MemObjLoad(pValue,pTos);` |
|       80 |  7528 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  7529 | `											/* Load index number */` |
|       38 |  7530 | `											pTos->nIdx = pAttr->nIdx;` |
|       18 |  7531 | `										}` |
|       39 |  7532 | `									}` |
|       41 |  7533 | `								}else{` |
|        - |  7534 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  7535 | `									char zMsg[256];` |
|        5 |  7536 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  7537 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  7538 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  7539 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  7540 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  7541 | `									}else{` |
|      ! 0 |  7542 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7543 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7544 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  7545 | `									}` |
|        5 |  7546 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  7547 | `									goto Abort;` |
|        - |  7548 | `								}` |
|        - |  7549 | `							}` |
|       39 |  7550 | `						}` |
|        - |  7551 | `					}` |
|        - |  7552 | `				}` |
|      222 |  7553 | `				if( pThis ){` |
|        - |  7554 | `					/* Safely unreference the object */` |
|        5 |  7555 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  7556 | `				}` |
|        - |  7557 | `			}` |
|      112 |  7558 | `		}else{` |
|        - |  7559 | `			/* Pop operands */` |
|      ! 0 |  7560 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  7561 | `			if( !pInstr->p3 ){` |
|      ! 0 |  7562 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  7563 | `			}` |
|      ! 0 |  7564 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7565 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  7566 | `		}` |
|        - |  7567 | `	}` |
|     7352 |  7568 | `	break;` |
|        - |  7569 | `					}` |
|        - |  7570 | `/*` |
|        - |  7571 | ` * OP_NEW P1 * * *` |
|        - |  7572 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  7573 | ` */` |
|      570 |  7574 | `case PH7_OP_NEW: {` |
|     1142 |  7575 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1142 |  7576 | `	ph7_class *pClass = 0;` |
|        - |  7577 | `	ph7_class_instance *pNew;` |
|     1142 |  7578 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  7579 | `		/* Try to extract the desired class */` |
|     1712 |  7580 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1140 |  7581 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      570 |  7582 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7583 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  7584 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  7585 | `	}` |
|     1142 |  7586 | `	if( pClass == 0 ){` |
|        - |  7587 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  7588 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  7589 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  7590 | `			);` |
|        - |  7591 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  7592 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7593 | `		if( pInstr->iP1 > 0 ){` |
|        - |  7594 | `			/* Pop given arguments */` |
|      ! 0 |  7595 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  7596 | `		}` |
|      ! 0 |  7597 | `		goto Abort;` |
|      ! 0 |  7598 | `	}else{` |
|        - |  7599 | `		ph7_class_method *pCons;` |
|        - |  7600 | `		/* Create a new class instance */` |
|     1142 |  7601 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1142 |  7602 | `		if( pNew == 0 ){` |
|      ! 0 |  7603 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7604 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  7605 | `				&pClass->sName` |
|        - |  7606 | `			);` |
|      ! 0 |  7607 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7608 | `			if( pInstr->iP1 > 0 ){` |
|        - |  7609 | `				/* Pop given arguments */` |
|      ! 0 |  7610 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  7611 | `			}` |
|      ! 0 |  7612 | `			break;` |
|        - |  7613 | `		}` |
|        - |  7614 | `		/* Check if a constructor is available */` |
|     1142 |  7615 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1142 |  7616 | `		if( pCons == 0 ){` |
|      834 |  7617 | `			SyString *pName = &pClass->sName;` |
|        - |  7618 | `			/* Check for a constructor with the same base class name */` |
|      834 |  7619 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      416 |  7620 | `		}` |
|     1142 |  7621 | `		if( pCons ){` |
|        - |  7622 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  7623 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  7624 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  7625 | `			 * (including variadic string-key packing). */` |
|      310 |  7626 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|      310 |  7627 | `			SySetReset(&aArg);` |
|      608 |  7628 | `			while( pArg < pTos ){` |
|      300 |  7629 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      300 |  7630 | `				pArg++;` |
|        2 |  7631 | `			}` |
|      310 |  7632 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  7633 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  7634 | `				sxu32 n;` |
|       65 |  7635 | `				n = SySetUsed(&aArg);` |
|        - |  7636 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  7637 | `				 * for named args the missing-arg check happens downstream` |
|        - |  7638 | `				 * after resolution). */` |
|      113 |  7639 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       49 |  7640 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       49 |  7641 | `					if( pFuncArg ){` |
|       49 |  7642 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  7643 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  7644 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  7645 | `						}` |
|       24 |  7646 | `					}` |
|       49 |  7647 | `					n++;` |
|        1 |  7648 | `				}` |
|       32 |  7649 | `			}` |
|      310 |  7650 | `			VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  7651 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      310 |  7652 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  7653 | `				pNew->iRef = 1;` |
|      ! 0 |  7654 | `			}` |
|      154 |  7655 | `		}` |
|     1142 |  7656 | `		if( pInstr->iP1 > 0 ){` |
|        - |  7657 | `			/* Pop given arguments */` |
|      246 |  7658 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      122 |  7659 | `		}` |
|     1142 |  7660 | `		PH7_MemObjRelease(pTos);` |
|     1142 |  7661 | `		pTos->x.pOther = pNew;` |
|     1142 |  7662 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  7663 | `	}` |
|     1142 |  7664 | `	break;` |
|        - |  7665 | `				 }` |
|        - |  7666 | `/*` |
|        - |  7667 | ` * OP_CLONE * * *` |
|        - |  7668 | ` * Perfome a clone operation.` |
|        - |  7669 | ` */` |
|       24 |  7670 | `case PH7_OP_CLONE: {` |
|        - |  7671 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  7672 | `#ifdef UNTRUST` |
|        - |  7673 | `	if( pTos < pStack ){` |
|        - |  7674 | `		goto Abort;` |
|        - |  7675 | `	}` |
|        - |  7676 | `#endif` |
|        - |  7677 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  7678 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  7679 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7680 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  7681 | `		PH7_MemObjRelease(pTos);` |
|        5 |  7682 | `		break;` |
|        - |  7683 | `	}` |
|        - |  7684 | `	/* Point to the source */` |
|       46 |  7685 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7686 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  7687 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  7688 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7689 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  7690 | `			&pSrc->pClass->sName);` |
|      ! 0 |  7691 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7692 | `		break;` |
|        - |  7693 | `	}` |
|        - |  7694 | `	/* Perform the clone operation */` |
|       46 |  7695 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  7696 | `	PH7_MemObjRelease(pTos);` |
|       46 |  7697 | `	if( pClone == 0 ){` |
|      ! 0 |  7698 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7699 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  7700 | `	}else{` |
|        - |  7701 | `		/* Load the cloned object */` |
|       46 |  7702 | `		pTos->x.pOther = pClone;` |
|       46 |  7703 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  7704 | `	}` |
|       46 |  7705 | `	break;` |
|        - |  7706 | `				   }` |
|        - |  7707 | `/*` |
|        - |  7708 | ` * OP_SWITCH * * P3` |
|        - |  7709 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  7710 | ` */` |
|       26 |  7711 | `case PH7_OP_SWITCH: {` |
|       54 |  7712 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  7713 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  7714 | `	ph7_value sValue,sCaseValue;` |
|        - |  7715 | `	sxu32 n,nEntry;` |
|        - |  7716 | `#ifdef UNTRUST` |
|        - |  7717 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  7718 | `		goto Abort;` |
|        - |  7719 | `	}` |
|        - |  7720 | `#endif` |
|        - |  7721 | `	/* Point to the case table  */` |
|       54 |  7722 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  7723 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  7724 | `	/* Select the appropriate case block to execute */` |
|       54 |  7725 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  7726 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  7727 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  7728 | `		pCase = &aCase[n];` |
|      130 |  7729 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  7730 | `		/* Execute the case expression first */` |
|      130 |  7731 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  7732 | `		/* Compare the two expression */` |
|      130 |  7733 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  7734 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  7735 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  7736 | `		if( rc == 0 ){` |
|        - |  7737 | `			/* Value match,jump to this block */` |
|       52 |  7738 | `			pc = pCase->nStart - 1;` |
|       52 |  7739 | `			break;` |
|        - |  7740 | `		}` |
|       41 |  7741 | `	}` |
|       54 |  7742 | `	VmPopOperand(&pTos,1);` |
|       54 |  7743 | `	if( n >= nEntry ){` |
|        - |  7744 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  7745 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  7746 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  7747 | `		}else{` |
|        - |  7748 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  7749 | `			pc = pSwitch->nOut - 1;` |
|        - |  7750 | `		}` |
|        1 |  7751 | `	}` |
|       54 |  7752 | `	break;` |
|        - |  7753 | `					}` |
|        - |  7754 | `/*` |
|        - |  7755 | ` * OP_MATCH * * P3` |
|        - |  7756 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  7757 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  7758 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  7759 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  7760 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  7761 | ` */` |
|       54 |  7762 | `case PH7_OP_MATCH: {` |
|      110 |  7763 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      110 |  7764 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  7765 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  7766 | `	sxu32 i,j,nArm,nCond;` |
|      110 |  7767 | `	int matched = 0;` |
|        - |  7768 | `#ifdef UNTRUST` |
|        - |  7769 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  7770 | `		goto Abort;` |
|        - |  7771 | `	}` |
|        - |  7772 | `#endif` |
|      110 |  7773 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      110 |  7774 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      110 |  7775 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      110 |  7776 | `	PH7_MemObjInit(pVm,&sCond);` |
|      110 |  7777 | `	PH7_MemObjInit(pVm,&sResult);` |
|      110 |  7778 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      348 |  7779 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  7780 | `		pArm = &aArm[i];` |
|      240 |  7781 | `		if( pArm->bDefault ){` |
|       13 |  7782 | `			pDefault = pArm;` |
|       13 |  7783 | `			continue;` |
|        - |  7784 | `		}` |
|      228 |  7785 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  7786 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  7787 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  7788 | `			if( pCondBc == 0 ){` |
|      ! 0 |  7789 | `				continue;` |
|        - |  7790 | `			}` |
|      260 |  7791 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      260 |  7792 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  7793 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  7794 | `			if( rc == 0 ){` |
|       93 |  7795 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       93 |  7796 | `				matched = 1;` |
|       93 |  7797 | `				break;` |
|        - |  7798 | `			}` |
|       85 |  7799 | `		}` |
|      115 |  7800 | `	}` |
|      110 |  7801 | `	if( !matched && pDefault ){` |
|       13 |  7802 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       13 |  7803 | `		matched = 1;` |
|        6 |  7804 | `	}` |
|      110 |  7805 | `	if( !matched ){` |
|        5 |  7806 | `		const char *zType = "unknown";` |
|        - |  7807 | `		char zMsg[128];` |
|        - |  7808 | `		sxu32 nMsg;` |
|        5 |  7809 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  7810 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  7811 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  7812 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  7813 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  7814 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  7815 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  7816 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  7817 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  7818 | `		default: break;` |
|        - |  7819 | `		}` |
|        7 |  7820 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  7821 | `			"Unhandled match case of type %s",zType);` |
|        7 |  7822 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  7823 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  7824 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  7825 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  7826 | `		goto Abort;` |
|        - |  7827 | `	}` |
|      105 |  7828 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  7829 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  7830 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  7831 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  7832 | `	break;` |
|        - |  7833 | `					}` |
|        - |  7834 | `/*` |
|        - |  7835 | ` * OP_YIELD P1 P2 *` |
|        - |  7836 | ` *  Yield a value from a generator function.` |
|        - |  7837 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  7838 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  7839 | ` */` |
|       34 |  7840 | `case PH7_OP_YIELD: {` |
|        - |  7841 | `	ph7_generator *pGen;` |
|       70 |  7842 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  7843 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  7844 | `		goto Abort;` |
|        - |  7845 | `	}` |
|       70 |  7846 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  7847 | `	if( pInstr->iP2 ){` |
|        - |  7848 | `		/* yield $key => $value: value on top, key below */` |
|        - |  7849 | `#ifdef UNTRUST` |
|        - |  7850 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  7851 | `#endif` |
|        7 |  7852 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  7853 | `		VmPopOperand(&pTos, 1);` |
|        7 |  7854 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  7855 | `		VmPopOperand(&pTos, 1);` |
|        - |  7856 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  7857 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  7858 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  7859 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  7860 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  7861 | `			}` |
|        1 |  7862 | `		}` |
|       67 |  7863 | `	}else if( pInstr->iP1 ){` |
|        - |  7864 | `		/* yield $value */` |
|        - |  7865 | `#ifdef UNTRUST` |
|        - |  7866 | `		if( pTos < pStack ) goto Abort;` |
|        - |  7867 | `#endif` |
|       64 |  7868 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  7869 | `		VmPopOperand(&pTos, 1);` |
|        - |  7870 | `		/* Auto-increment key */` |
|       64 |  7871 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  7872 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  7873 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  7874 | `	}else{` |
|        - |  7875 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  7876 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7877 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7878 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  7879 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  7880 | `	}` |
|        - |  7881 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  7882 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  7883 | `	goto Suspend;` |
|        - |  7884 |  |
|        - |  7885 | `/*` |
|        - |  7886 | ` * OP_CALL P1 * *` |
|        - |  7887 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  7888 | ` *  function on the stack.` |
|        - |  7889 | ` */` |
|   346511 |  7890 | `case PH7_OP_CALL: {` |
|   693068 |  7891 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  7892 | `	ph7_value *pArg;` |
|   693068 |  7893 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   693068 |  7894 | `	pArg = &pTos[-nCallArgs];` |
|        - |  7895 | `	SyHashEntry *pEntry;` |
|        - |  7896 | `	SyString sName;` |
|        - |  7897 | `	/* Extract function name */` |
|   693068 |  7898 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       78 |  7899 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7900 | `			ph7_value sResult;` |
|      ! 0 |  7901 | `			SySetReset(&aArg);` |
|      ! 0 |  7902 | `			while( pArg < pTos ){` |
|      ! 0 |  7903 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  7904 | `				pArg++;` |
|      ! 0 |  7905 | `			}` |
|      ! 0 |  7906 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  7907 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  7908 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  7909 | `			SySetReset(&aArg);` |
|        - |  7910 | `			/* Pop given arguments */` |
|      ! 0 |  7911 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7912 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7913 | `			}` |
|        - |  7914 | `			/* Copy result */` |
|      ! 0 |  7915 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  7916 | `			PH7_MemObjRelease(&sResult);` |
|       78 |  7917 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       78 |  7918 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7919 | `			ph7_value sResult;` |
|        - |  7920 | `			sxi32 rcInv;` |
|       78 |  7921 | `			SySetReset(&aArg);` |
|      192 |  7922 | `			while( pArg < pTos ){` |
|      116 |  7923 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      116 |  7924 | `				pArg++;` |
|        2 |  7925 | `			}` |
|       78 |  7926 | `			PH7_MemObjInit(pVm,&sResult);` |
|      116 |  7927 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       76 |  7928 | `				(int)SySetUsed(&aArg),` |
|       76 |  7929 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - |  7930 | `				&sResult,` |
|       76 |  7931 | `				(VmCallArgMap *)pInstr->p3);` |
|       78 |  7932 | `			SySetReset(&aArg);` |
|       78 |  7933 | `			if( nCallArgs > 0 ){` |
|       74 |  7934 | `				VmPopOperand(&pTos,nCallArgs);` |
|       36 |  7935 | `			}` |
|       78 |  7936 | `			if( rcInv == SXERR_INVALID ){` |
|        - |  7937 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - |  7938 | `				 * sResult was already released by VmCallObjectInvoke.` |
|        - |  7939 | `				 * Pin pThis: the release below would otherwise drop the last ref` |
|        - |  7940 | `				 * to a temporary callable like (new Plain())(...), freeing the` |
|        - |  7941 | `				 * instance before VmRaiseNotCallable reads its class name. */` |
|       13 |  7942 | `				pThis->iRef++;` |
|       13 |  7943 | `				PH7_MemObjRelease(pTos);` |
|       13 |  7944 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 |  7945 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 |  7946 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7947 | `					goto Abort;` |
|        - |  7948 | `				}` |
|        - |  7949 | `				{` |
|       13 |  7950 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       12 |  7951 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION)` |
|       13 |  7952 | `					 && pFrm2->iExceptionJump > 0 ){` |
|       13 |  7953 | `						pc = pFrm2->iExceptionJump - 1;` |
|       13 |  7954 | `						break;` |
|        - |  7955 | `					}` |
|        - |  7956 | `				}` |
|      ! 0 |  7957 | `				goto Exception;` |
|        - |  7958 | `			}` |
|       66 |  7959 | `			PH7_MemObjStore(&sResult,pTos);` |
|       66 |  7960 | `			PH7_MemObjRelease(&sResult);` |
|       34 |  7961 | `		}else{` |
|        - |  7962 | `			/* Raise exception: Invalid function name */` |
|      ! 0 |  7963 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  7964 | `			/* Pop given arguments */` |
|      ! 0 |  7965 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7966 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7967 | `			}` |
|        - |  7968 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  7969 | `			PH7_MemObjRelease(pTos);` |
|        - |  7970 | `		}` |
|       66 |  7971 | `		break;` |
|        - |  7972 | `	}` |
|   692992 |  7973 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  7974 | `	/* Check for a compiled function first.` |
|        - |  7975 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  7976 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   692992 |  7977 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  7978 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  7979 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  7980 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  7981 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  7982 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  7983 | `	{` |
|   692992 |  7984 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   692992 |  7985 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  7986 | `		const char *zFunc;` |
|        - |  7987 | `		const char *zEnd;` |
|        - |  7988 | `		const char *z;` |
|        - |  7989 | `		SyString sGlobal;` |
|       22 |  7990 | `		zFunc = sName.zString;` |
|       22 |  7991 | `		zEnd  = zFunc + sName.nByte;` |
|       22 |  7992 | `		z = zEnd;` |
|        - |  7993 | `		/* Find last namespace separator */` |
|      194 |  7994 | `		while( z > zFunc ){` |
|      194 |  7995 | `			if( z[-1] == '\\' ){` |
|       22 |  7996 | `				break;` |
|        - |  7997 | `			}` |
|      174 |  7998 | `			z--;` |
|        2 |  7999 | `		}` |
|       22 |  8000 | `		if( z > zFunc && z < zEnd ){` |
|        - |  8001 | `			/* Retry lookup using the unqualified/global function name */` |
|       22 |  8002 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       22 |  8003 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  8004 | `		}` |
|       10 |  8005 | `	}` |
|        - |  8006 | `	} /* end VmCallArgMap namespace scope */` |
|   692992 |  8007 | `	if( pEntry ){` |
|        - |  8008 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  8009 | `		ph7_class_instance *pThis;` |
|        - |  8010 | `		ph7_value *pFrameStack;` |
|        - |  8011 | `		ph7_vm_func *pVmFunc;` |
|        - |  8012 | `		ph7_class *pSelf;` |
|        - |  8013 | `		VmFrame *pFrame;` |
|        - |  8014 | `		ph7_value *pObj;` |
|        - |  8015 | `		VmSlot sArg;` |
|        - |  8016 | `		sxu32 n;` |
|        - |  8017 | `		/* initialize fields */` |
|    17554 |  8018 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    17554 |  8019 | `		pThis = 0;` |
|    17554 |  8020 | `		pSelf = 0;` |
|    17554 |  8021 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  8022 | `			ph7_class_method *pMeth;` |
|        - |  8023 | `			/* Class method call */` |
|     2980 |  8024 | `			ph7_value *pTarget = &pTos[-1];` |
|     2980 |  8025 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  8026 | `				/* Extract the 'this' pointer */` |
|     2980 |  8027 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  8028 | `					/* Instance already loaded */` |
|     2890 |  8029 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     2890 |  8030 | `					pThis->iRef++;` |
|     2890 |  8031 | `					pSelf = pThis->pClass;` |
|     1444 |  8032 | `				}` |
|     2980 |  8033 | `				if( pSelf == 0 ){` |
|       92 |  8034 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  8035 | `						/* "Late Static Binding" class name */` |
|      128 |  8036 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  8037 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  8038 | `					}` |
|       92 |  8039 | `					if( pSelf == 0 ){` |
|       21 |  8040 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  8041 | `					}` |
|       45 |  8042 | `				}` |
|     2980 |  8043 | `				if( pThis == 0  ){` |
|       92 |  8044 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       92 |  8045 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       92 |  8046 | `					if( pFrameLocal->pParent ){` |
|        - |  8047 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       66 |  8048 | `						pThis = pFrameLocal->pThis;` |
|       66 |  8049 | `						if( pThis ){` |
|       21 |  8050 | `							pThis->iRef++;` |
|       10 |  8051 | `						}` |
|       32 |  8052 | `					}` |
|       45 |  8053 | `				}` |
|     2980 |  8054 | `				VmPopOperand(&pTos,1);` |
|     2980 |  8055 | `				PH7_MemObjRelease(pTos);` |
|        - |  8056 | `				/* Synchronize pointers */` |
|     2980 |  8057 | `				pArg = &pTos[-nCallArgs];` |
|        - |  8058 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  8059 | `				 * user have already computed the random generated unique class method name` |
|        - |  8060 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  8061 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  8062 | `				 */` |
|     2980 |  8063 | `				while( pArg < pStack ){` |
|      ! 0 |  8064 | `					pArg++;` |
|      ! 0 |  8065 | `				}` |
|     2980 |  8066 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  8067 | `					/* Check if the call is allowed */` |
|     2980 |  8068 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     2980 |  8069 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  8070 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  8071 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  8072 | `							char zMsg[256];` |
|      ! 0 |  8073 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8074 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  8075 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  8076 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  8077 | `							/* Pop given arguments */` |
|      ! 0 |  8078 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  8079 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8080 | `							}` |
|      ! 0 |  8081 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8082 | `							goto Abort;` |
|        - |  8083 | `						}` |
|        6 |  8084 | `					}` |
|     1489 |  8085 | `				}` |
|     1489 |  8086 | `			}` |
|     1489 |  8087 | `		}` |
|        - |  8088 | `		/* Check The recursion limit */` |
|    17554 |  8089 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  8090 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8091 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  8092 | `				&pVmFunc->sName);` |
|        - |  8093 | `			/* Pop given arguments */` |
|        3 |  8094 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8095 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8096 | `			}` |
|        - |  8097 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  8098 | `			PH7_MemObjRelease(pTos);` |
|       14 |  8099 | `			break;` |
|        - |  8100 | `		}` |
|    17552 |  8101 | `		if( pVmFunc->pNextName ){` |
|        - |  8102 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  8103 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  8104 | `		}` |
|    17552 |  8105 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  8106 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  8107 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  8108 | `			ph7_generator *pGenerator;` |
|        - |  8109 | `			ph7_class_instance *pGenObj;` |
|        - |  8110 | `			ph7_value *pCtxAttr;` |
|        - |  8111 | `			SyString sAttrName;` |
|        - |  8112 | `			ph7_value **apCallArgs;` |
|        - |  8113 | `			int nGenArgs, iArg;` |
|        - |  8114 | `			/* Collect arguments from the operand stack */` |
|       24 |  8115 | `			nGenArgs = (int)(pTos - pArg);` |
|       24 |  8116 | `			apCallArgs = 0;` |
|       24 |  8117 | `			if( nGenArgs > 0 ){` |
|       14 |  8118 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  8119 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  8120 | `				if( apCallArgs == 0 ){` |
|        - |  8121 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  8122 | `					nGenArgs = 0;` |
|      ! 0 |  8123 | `				}else{` |
|       10 |  8124 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  8125 | `					int didReorder = 0;` |
|       10 |  8126 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  8127 | `						/* Named-argument reordering for generator */` |
|        5 |  8128 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  8129 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  8130 | `						sxu32 nNV = nF;` |
|        5 |  8131 | `						sxi32 iVIdx = -1;` |
|        - |  8132 | `						sxi32 *aGSlot;` |
|        - |  8133 | `						sxu8 *aGUsed;` |
|        - |  8134 | `						sxu32 gi;` |
|       13 |  8135 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  8136 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  8137 | `						}` |
|        7 |  8138 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  8139 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  8140 | `						if( aGSlot ){` |
|        5 |  8141 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  8142 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  8143 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  8144 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  8145 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  8146 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8147 | `								goto Abort;` |
|        - |  8148 | `							}` |
|        - |  8149 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  8150 | `							 * append overflow (variadic / positional beyond` |
|        - |  8151 | `							 * formals) so downstream sees every argument. */` |
|        - |  8152 | `							{` |
|        5 |  8153 | `								int nOut = 0;` |
|       13 |  8154 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  8155 | `									sxu32 gj;` |
|       13 |  8156 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  8157 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  8158 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  8159 | `											break;` |
|        - |  8160 | `										}` |
|        3 |  8161 | `									}` |
|        5 |  8162 | `								}` |
|       13 |  8163 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  8164 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  8165 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  8166 | `									}` |
|        5 |  8167 | `								}` |
|        5 |  8168 | `								nGenArgs = nOut;` |
|        - |  8169 | `							}` |
|        5 |  8170 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  8171 | `							didReorder = 1;` |
|        2 |  8172 | `						}` |
|        - |  8173 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  8174 | `						 * positional fill below — preserves arg order rather` |
|        - |  8175 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  8176 | `					}` |
|       10 |  8177 | `					if( !didReorder ){` |
|       12 |  8178 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  8179 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  8180 | `						}` |
|        2 |  8181 | `					}` |
|        - |  8182 | `				}` |
|        4 |  8183 | `			}` |
|        - |  8184 | `			/* Create execution context and generator wrapper */` |
|       24 |  8185 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       24 |  8186 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  8187 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8188 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  8189 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  8190 | `				break;` |
|        - |  8191 | `			}` |
|       24 |  8192 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       24 |  8193 | `			if( pGenerator == 0 ){` |
|      ! 0 |  8194 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  8195 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8196 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  8197 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  8198 | `				break;` |
|        - |  8199 | `			}` |
|        - |  8200 | `			/* Set up the frame with arguments, closure env, $this */` |
|       24 |  8201 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       24 |  8202 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       24 |  8203 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       24 |  8204 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       24 |  8205 | `			pExecCtx->pFrame->pParent = 0;` |
|       24 |  8206 | `			if( apCallArgs ){` |
|       10 |  8207 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  8208 | `			}` |
|       24 |  8209 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  8210 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  8211 | `				if( pThis ){` |
|      ! 0 |  8212 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8213 | `				}` |
|      ! 0 |  8214 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8215 | `					goto Abort;` |
|        - |  8216 | `				}` |
|      ! 0 |  8217 | `				break;` |
|        - |  8218 | `			}` |
|        - |  8219 | `			/* Create Generator class instance */` |
|       24 |  8220 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       24 |  8221 | `			if( pGenObj == 0 ){` |
|      ! 0 |  8222 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  8223 | `				break;` |
|        - |  8224 | `			}` |
|        - |  8225 | `			/* Store generator in __ctx attribute */` |
|       24 |  8226 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       24 |  8227 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       24 |  8228 | `			if( pCtxAttr ){` |
|       24 |  8229 | `				pCtxAttr->x.pOther = pGenerator;` |
|       24 |  8230 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       11 |  8231 | `			}` |
|        - |  8232 | `			/* Pop args and function name, push Generator object */` |
|       24 |  8233 | `			PH7_MemObjRelease(pTos);` |
|       24 |  8234 | `			pTos = &pTos[-nCallArgs];` |
|       24 |  8235 | `			pTos->x.pOther = pGenObj;` |
|       24 |  8236 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       24 |  8237 | `			pGenObj->iRef++;` |
|       24 |  8238 | `			if( pThis ){` |
|      ! 0 |  8239 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8240 | `			}` |
|       24 |  8241 | `			break;` |
|        - |  8242 | `		}` |
|        - |  8243 | `		/* Extract the formal argument set */` |
|    17530 |  8244 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  8245 | `		/* Create a new VM frame  */` |
|    17530 |  8246 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    17530 |  8247 | `		if( rc != SXRET_OK ){` |
|        - |  8248 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  8249 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8250 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  8251 | `				&pVmFunc->sName);` |
|        - |  8252 | `			/* Pop given arguments */` |
|      ! 0 |  8253 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8254 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8255 | `			}` |
|        - |  8256 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8257 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8258 | `			break;` |
|        - |  8259 | `		}` |
|    17530 |  8260 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  8261 | `			/* Install the '$this' variable */` |
|        - |  8262 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     2908 |  8263 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     2908 |  8264 | `			if( pObj ){` |
|        - |  8265 | `				/* Reflect the change */` |
|     2908 |  8266 | `				pObj->x.pOther = pThis;` |
|     2908 |  8267 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1453 |  8268 | `			}` |
|     1453 |  8269 | `		}` |
|    17530 |  8270 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  8271 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  8272 | `			/* Install static variables */` |
|      ! 0 |  8273 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  8274 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  8275 | `				pStatic = &aStatic[n];` |
|      ! 0 |  8276 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  8277 | `					/* Initialize the static variables */` |
|      ! 0 |  8278 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  8279 | `					if( pObj ){` |
|        - |  8280 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  8281 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  8282 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  8283 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  8284 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  8285 | `						}` |
|      ! 0 |  8286 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  8287 | `					}else{` |
|      ! 0 |  8288 | `						continue;` |
|        - |  8289 | `					}` |
|      ! 0 |  8290 | `				}` |
|        - |  8291 | `				/* Install in the current frame */` |
|      ! 0 |  8292 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  8293 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  8294 | `			}` |
|      ! 0 |  8295 | `		}` |
|        - |  8296 | `		/* Push arguments in the local frame */` |
|        - |  8297 | `		{` |
|    17530 |  8298 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  8299 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  8300 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    17530 |  8301 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    17530 |  8302 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  8303 | `			/* ============================================================` |
|        - |  8304 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  8305 | `			 *` |
|        - |  8306 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  8307 | `			 * or position, then install them in the frame.` |
|        - |  8308 | `			 * ============================================================ */` |
|       96 |  8309 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       96 |  8310 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       96 |  8311 | `			sxi32 iVariadicIdx = -1;` |
|        - |  8312 | `			sxu32 nNonVariadic;` |
|        - |  8313 | `			sxi32 *aSlot;` |
|        - |  8314 | `			sxu8  *aUsed;` |
|        - |  8315 | `			sxu32 i;` |
|        - |  8316 | `			/* Find variadic parameter index */` |
|      292 |  8317 | `			for( i = 0; i < nFormal; i++ ){` |
|      206 |  8318 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  8319 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  8320 | `					break;` |
|        - |  8321 | `				}` |
|      100 |  8322 | `			}` |
|       96 |  8323 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  8324 | `			/* Allocate mapping arrays */` |
|      143 |  8325 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       94 |  8326 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       96 |  8327 | `			if( aSlot == 0 ){` |
|      ! 0 |  8328 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  8329 | `				goto Abort;` |
|        - |  8330 | `			}` |
|       96 |  8331 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  8332 | `			/* Resolve named arguments to formal parameters */` |
|      143 |  8333 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       47 |  8334 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       96 |  8335 | `			if( rc == PH7_ABORT ){` |
|        7 |  8336 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  8337 | `				goto Abort;` |
|        - |  8338 | `			}` |
|        - |  8339 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      275 |  8340 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  8341 | `				/* Find the stack arg mapped to formal n */` |
|      187 |  8342 | `				sxi32 iSrc = -1;` |
|      309 |  8343 | `				for( i = 0; i < nActual; i++ ){` |
|      291 |  8344 | `					if( aSlot[i] == (sxi32)n ){` |
|      169 |  8345 | `						iSrc = (sxi32)i;` |
|      169 |  8346 | `						break;` |
|        - |  8347 | `					}` |
|       62 |  8348 | `				}` |
|      187 |  8349 | `				if( iSrc >= 0 ){` |
|        - |  8350 | `					/* Argument was provided — install with type checking */` |
|      169 |  8351 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  8352 | `					/* NULL-to-default redirect (existing behavior) */` |
|      168 |  8353 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  8354 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  8355 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  8356 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8357 | `					}` |
|        - |  8358 | `					/* Type checking: union types */` |
|      169 |  8359 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  8360 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  8361 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  8362 | `							bCallIsStrict);` |
|       13 |  8363 | `						if( rcU != SXRET_OK ){` |
|        - |  8364 | `							const char *zGiven;` |
|      ! 0 |  8365 | `							const char *zExpected = "union";` |
|        - |  8366 | `							char zBuf[128];` |
|        - |  8367 | `							char zTypeBuf[128];` |
|      ! 0 |  8368 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8369 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  8370 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  8371 | `								zGiven = "null";` |
|      ! 0 |  8372 | `							}else{` |
|      ! 0 |  8373 | `								zGiven = ph7_type_name(pVal);` |
|        - |  8374 | `							}` |
|      ! 0 |  8375 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  8376 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  8377 | `							}` |
|      ! 0 |  8378 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8379 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  8380 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8381 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8382 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  8383 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8384 | `							pFrameStack = 0;` |
|      ! 0 |  8385 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  8386 | `							goto SkipFuncBody;` |
|        - |  8387 | `						}` |
|      171 |  8388 | `					}else if( aFormalArg[n].nType > 0` |
|       91 |  8389 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  8390 | `						/* Scalar/class type checking */` |
|       17 |  8391 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  8392 | `							SyString *pName = &aFormalArg[n].sClass;` |
|      ! 0 |  8393 | `							ph7_class *pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  8394 | `							if( pClass ){` |
|      ! 0 |  8395 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8396 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  8397 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8398 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8399 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8400 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8401 | `									}` |
|      ! 0 |  8402 | `								}else{` |
|      ! 0 |  8403 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  8404 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  8405 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8406 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8407 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8408 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8409 | `									}` |
|        - |  8410 | `								}` |
|      ! 0 |  8411 | `							}` |
|       17 |  8412 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  8413 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  8414 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8415 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  8416 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8417 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8418 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8419 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8420 | `								pFrameStack = 0;` |
|      ! 0 |  8421 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8422 | `								goto SkipFuncBody;` |
|        7 |  8423 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8424 | `								char zTypeBuf[128];` |
|      ! 0 |  8425 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8426 | `									&aFormalArg[n].sName,` |
|      ! 0 |  8427 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  8428 | `									ph7_type_name(pVal));` |
|      ! 0 |  8429 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8430 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8431 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8432 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8433 | `								pFrameStack = 0;` |
|      ! 0 |  8434 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8435 | `								goto SkipFuncBody;` |
|        - |  8436 | `							}` |
|        3 |  8437 | `						}` |
|        8 |  8438 | `					}` |
|        - |  8439 | `					/* Install: by reference or by value */` |
|      169 |  8440 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  8441 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  8442 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  8443 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8444 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  8445 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  8446 | `							}` |
|      ! 0 |  8447 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  8448 | `						}else{` |
|        7 |  8449 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  8450 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  8451 | `							if( pRefEntry == 0 ){` |
|        7 |  8452 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  8453 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  8454 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  8455 | `								sArg.pUserData = 0;` |
|        5 |  8456 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  8457 | `							}` |
|        5 |  8458 | `							pObj = 0;` |
|        - |  8459 | `						}` |
|        3 |  8460 | `					}else{` |
|      165 |  8461 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  8462 | `					}` |
|      169 |  8463 | `					if( pObj ){` |
|      165 |  8464 | `						PH7_MemObjStore(pVal,pObj);` |
|      165 |  8465 | `						sArg.nIdx = pObj->nIdx;` |
|      165 |  8466 | `						sArg.pUserData = 0;` |
|      165 |  8467 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       82 |  8468 | `					}` |
|       85 |  8469 | `				}else{` |
|        - |  8470 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  8471 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  8472 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  8473 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  8474 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  8475 | `						if( pObj ){` |
|       19 |  8476 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  8477 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  8478 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  8479 | `							sArg.pUserData = 0;` |
|       19 |  8480 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  8481 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  8482 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8483 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8484 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  8485 | `							}` |
|        9 |  8486 | `						}` |
|        9 |  8487 | `					}` |
|        - |  8488 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  8489 | `				}` |
|       94 |  8490 | `			}` |
|        - |  8491 | `			/* Handle variadic parameter */` |
|       89 |  8492 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  8493 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  8494 | `				if( pObj ){` |
|        9 |  8495 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  8496 | `					{` |
|        9 |  8497 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  8498 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  8499 | `							if( aSlot[i] == -1 ){` |
|       16 |  8500 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  8501 | `									/* Named variadic entry: insert with string key */` |
|        - |  8502 | `									ph7_value sKey;` |
|       11 |  8503 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  8504 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  8505 | `										pCallMap3->aNames[i].zString,` |
|       10 |  8506 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  8507 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  8508 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  8509 | `								}else{` |
|        - |  8510 | `									/* Positional variadic entry */` |
|      ! 0 |  8511 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  8512 | `								}` |
|        5 |  8513 | `							}` |
|       12 |  8514 | `						}` |
|        - |  8515 | `					}` |
|        9 |  8516 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  8517 | `					sArg.pUserData = 0;` |
|        9 |  8518 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  8519 | `				}` |
|        5 |  8520 | `			}else{` |
|        - |  8521 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  8522 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  8523 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  8524 | `				 * the positional-only path's behavior. */` |
|       81 |  8525 | `				sxu32 nAnon = nNonVariadic;` |
|      237 |  8526 | `				for( i = 0; i < nActual; i++ ){` |
|      157 |  8527 | `					if( aSlot[i] == -2 ){` |
|        - |  8528 | `						char zAnonBuf[32];` |
|        - |  8529 | `						SyString sAnonName;` |
|      ! 0 |  8530 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  8531 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  8532 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  8533 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  8534 | `						if( pObj ){` |
|      ! 0 |  8535 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  8536 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  8537 | `							sArg.pUserData = 0;` |
|      ! 0 |  8538 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  8539 | `						}` |
|      ! 0 |  8540 | `						nAnon++;` |
|      ! 0 |  8541 | `					}` |
|       79 |  8542 | `				}` |
|        - |  8543 | `			}` |
|        - |  8544 | `			/* Release all stack arguments */` |
|      267 |  8545 | `			for( i = 0; i < nActual; i++ ){` |
|      179 |  8546 | `				PH7_MemObjRelease(&pArg[i]);` |
|       90 |  8547 | `			}` |
|       89 |  8548 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  8549 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       89 |  8550 | `			n = nFormal;` |
|       45 |  8551 | `		}else{` |
|        - |  8552 | `		/* ============================================================` |
|        - |  8553 | `		 * Positional-only matching path (original)` |
|        - |  8554 | `		 * ============================================================ */` |
|    17436 |  8555 | `		n = 0;` |
|    46688 |  8556 | `		while( pArg < pTos ){` |
|    29324 |  8557 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  8558 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       40 |  8559 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       40 |  8560 | `				if( pObj ){` |
|        - |  8561 | `					/* Initialize as empty array */` |
|       40 |  8562 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  8563 | `					{` |
|       40 |  8564 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      150 |  8565 | `						while( pArg < pTos ){` |
|        - |  8566 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  8567 | `							 *` |
|        - |  8568 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  8569 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  8570 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  8571 | `							 * non-union variadic path below has the same limitation;` |
|        - |  8572 | `							 * fixing both wants a separate counter for elements` |
|        - |  8573 | `							 * already packed into the variadic array. */` |
|      114 |  8574 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  8575 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  8576 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  8577 | `									bCallIsStrict);` |
|       16 |  8578 | `								if( rcU != SXRET_OK ){` |
|        - |  8579 | `									const char *zGiven;` |
|        3 |  8580 | `									const char *zExpected = "union";` |
|        - |  8581 | `									char zBuf[128];` |
|        - |  8582 | `									char zTypeBuf[128];` |
|        3 |  8583 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8584 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  8585 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  8586 | `										zGiven = "null";` |
|      ! 0 |  8587 | `									}else{` |
|        3 |  8588 | `										zGiven = ph7_type_name(pArg);` |
|        - |  8589 | `									}` |
|        3 |  8590 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 |  8591 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 |  8592 | `									}` |
|        4 |  8593 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  8594 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 |  8595 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8596 | `										goto Abort;` |
|        - |  8597 | `									}` |
|        3 |  8598 | `									PH7_MemObjRelease(pTos);` |
|        3 |  8599 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  8600 | `									pFrameStack = 0;` |
|        3 |  8601 | `									rc = PH7_EXCEPTION;` |
|        3 |  8602 | `									goto SkipFuncBody;` |
|        - |  8603 | `								}` |
|       14 |  8604 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  8605 | `								pArg++;` |
|       14 |  8606 | `								continue;` |
|        - |  8607 | `							}` |
|        - |  8608 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  8609 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      114 |  8610 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  8611 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  8612 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  8613 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  8614 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  8615 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8616 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  8617 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8618 | `										goto Abort;` |
|        - |  8619 | `									}` |
|        - |  8620 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  8621 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  8622 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8623 | `									pFrameStack = 0;` |
|      ! 0 |  8624 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  8625 | `									goto SkipFuncBody;` |
|       13 |  8626 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8627 | `									char zTypeBuf[128];` |
|      ! 0 |  8628 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8629 | `										&aFormalArg[n].sName,` |
|      ! 0 |  8630 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  8631 | `										ph7_type_name(pArg));` |
|      ! 0 |  8632 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8633 | `										goto Abort;` |
|        - |  8634 | `									}` |
|      ! 0 |  8635 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  8636 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8637 | `									pFrameStack = 0;` |
|      ! 0 |  8638 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  8639 | `									goto SkipFuncBody;` |
|        - |  8640 | `								}` |
|        6 |  8641 | `							}` |
|      100 |  8642 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|      100 |  8643 | `							pArg++;` |
|        2 |  8644 | `						}` |
|        - |  8645 | `					}` |
|       38 |  8646 | `					sArg.nIdx = pObj->nIdx;` |
|       38 |  8647 | `					sArg.pUserData = 0;` |
|       38 |  8648 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  8649 | `				}` |
|       38 |  8650 | `				break; /* All remaining args consumed */` |
|        - |  8651 | `			}` |
|    29286 |  8652 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    29102 |  8653 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       34 |  8654 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  8655 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  8656 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  8657 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  8658 | `						goto Abort;` |
|        - |  8659 | `					}` |
|      ! 0 |  8660 | `				}` |
|        - |  8661 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    29104 |  8662 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       89 |  8663 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       58 |  8664 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       29 |  8665 | `						bCallIsStrict);` |
|       60 |  8666 | `					if( rcU != SXRET_OK ){` |
|        - |  8667 | `						const char *zGiven;` |
|       19 |  8668 | `						const char *zExpected = "union";` |
|        - |  8669 | `						char zBuf[128];` |
|        - |  8670 | `						char zTypeBuf[128];` |
|       19 |  8671 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  8672 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  8673 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  8674 | `							zGiven = "null";` |
|        5 |  8675 | `						}else{` |
|        5 |  8676 | `							zGiven = ph7_type_name(pArg);` |
|        - |  8677 | `						}` |
|       19 |  8678 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       19 |  8679 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 |  8680 | `						}` |
|       28 |  8681 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  8682 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       19 |  8683 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  8684 | `							goto Abort;` |
|        - |  8685 | `						}` |
|       19 |  8686 | `						PH7_MemObjRelease(pTos);` |
|       19 |  8687 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  8688 | `						pFrameStack = 0;` |
|       19 |  8689 | `						rc = PH7_EXCEPTION;` |
|       19 |  8690 | `						goto SkipFuncBody;` |
|        - |  8691 | `					}` |
|       21 |  8692 | `				}else` |
|        - |  8693 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  8694 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    29070 |  8695 | `				if( aFormalArg[n].nType > 0` |
|    15225 |  8696 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1378 |  8697 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  8698 | `						/* Argument must be a class instance [i.e: object] */` |
|       26 |  8699 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  8700 | `						ph7_class *pClass;` |
|        - |  8701 | `						/* Try to extract the desired class */` |
|       26 |  8702 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       26 |  8703 | `						if( pClass ){` |
|       22 |  8704 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8705 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  8706 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8707 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8708 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8709 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  8710 | `								}` |
|      ! 0 |  8711 | `							}else{` |
|        - |  8712 | `								/* reuse pThis declared in outer scope */` |
|       22 |  8713 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  8714 | `								/* Make sure the object is an instance of the given class */` |
|       22 |  8715 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  8716 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8717 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8718 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8719 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  8720 | `								}` |
|        - |  8721 | `							}` |
|       12 |  8722 | `						}` |
|     1366 |  8723 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       24 |  8724 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  8725 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  8726 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  8727 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  8728 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  8729 | `								goto Abort;` |
|        - |  8730 | `							}` |
|        - |  8731 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  8732 | `							PH7_MemObjRelease(pTos);` |
|       11 |  8733 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  8734 | `							pFrameStack = 0;` |
|       11 |  8735 | `							rc = PH7_EXCEPTION;` |
|       11 |  8736 | `							goto SkipFuncBody;` |
|       14 |  8737 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8738 | `							char zTypeBuf[128];` |
|        7 |  8739 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        4 |  8740 | `								&aFormalArg[n].sName,` |
|        4 |  8741 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        2 |  8742 | `								ph7_type_name(pArg));` |
|        5 |  8743 | `							if( rc == PH7_ABORT ){` |
|        5 |  8744 | `								goto Abort;` |
|        - |  8745 | `							}` |
|      ! 0 |  8746 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  8747 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8748 | `							pFrameStack = 0;` |
|      ! 0 |  8749 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  8750 | `							goto SkipFuncBody;` |
|        - |  8751 | `						}` |
|        4 |  8752 | `					}` |
|      681 |  8753 | `				}` |
|    29072 |  8754 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  8755 | `					/* Pass by reference */` |
|       58 |  8756 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  8757 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  8758 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  8759 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8760 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  8761 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  8762 | `						}` |
|        - |  8763 | `						/* Switch to pass by value */` |
|      ! 0 |  8764 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  8765 | `					}else{` |
|        - |  8766 | `						SyHashEntry *pRefEntry;` |
|        - |  8767 | `						/* Install the referenced variable in the private function frame */` |
|       58 |  8768 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       58 |  8769 | `						if( pRefEntry == 0 ){` |
|       86 |  8770 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       56 |  8771 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       58 |  8772 | `							sArg.nIdx = pArg->nIdx;` |
|       58 |  8773 | `							sArg.pUserData = 0;` |
|       58 |  8774 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       28 |  8775 | `						}` |
|       58 |  8776 | `						pObj = 0;` |
|        - |  8777 | `					}` |
|       30 |  8778 | `				}else{` |
|        - |  8779 | `					/* Pass by value,make a copy of the given argument */` |
|    29016 |  8780 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  8781 | `				}` |
|    14537 |  8782 | `			}else{` |
|        - |  8783 | `				char zName[32];` |
|        - |  8784 | `				SyString sArgName;` |
|        - |  8785 | `				/* Set a dummy name */` |
|      184 |  8786 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      184 |  8787 | `				sArgName.zString = zName;` |
|        - |  8788 | `				/* Annonymous argument */` |
|      184 |  8789 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  8790 | `			}` |
|    29254 |  8791 | `			if( pObj ){` |
|    29198 |  8792 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  8793 | `				/* Insert argument index  */` |
|    29198 |  8794 | `				sArg.nIdx = pObj->nIdx;` |
|    29198 |  8795 | `				sArg.pUserData = 0;` |
|    29198 |  8796 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    14598 |  8797 | `			}` |
|    29254 |  8798 | `			PH7_MemObjRelease(pArg);` |
|    29254 |  8799 | `			pArg++;` |
|    29254 |  8800 | `			++n;` |
|        2 |  8801 | `		}` |
|        - |  8802 | `		} /* end named vs positional branch */` |
|        - |  8803 | `		/* Set up closure environment */` |
|    17490 |  8804 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  8805 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  8806 | `			ph7_value *pValue;` |
|        - |  8807 | `			sxu32 iEnv;` |
|      120 |  8808 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      306 |  8809 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      188 |  8810 | `				pEnv = &aEnv[iEnv];` |
|      188 |  8811 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  8812 | `					/* Do not install null value */` |
|      114 |  8813 | `					continue;` |
|        - |  8814 | `				}` |
|       76 |  8815 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       76 |  8816 | `				if( pValue == 0 ){` |
|      ! 0 |  8817 | `					continue;` |
|        - |  8818 | `				}` |
|        - |  8819 | `				/* Invalidate any prior representation */` |
|       76 |  8820 | `				PH7_MemObjRelease(pValue);` |
|        - |  8821 | `				/* Duplicate bound variable value */` |
|       76 |  8822 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       39 |  8823 | `			}` |
|       59 |  8824 | `		}` |
|        - |  8825 | `		/* Process default values for remaining formal parameters */` |
|    20180 |  8826 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2738 |  8827 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  8828 | `				/* Variadic parameter with no extra args — create empty array */` |
|       48 |  8829 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       48 |  8830 | `				if( pObj ){` |
|       48 |  8831 | `					PH7_MemObjToHashmap(pObj);` |
|       48 |  8832 | `					sArg.nIdx = pObj->nIdx;` |
|       48 |  8833 | `					sArg.pUserData = 0;` |
|       48 |  8834 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  8835 | `				}` |
|       48 |  8836 | `				n++;` |
|       48 |  8837 | `				break; /* Variadic is always last */` |
|        - |  8838 | `			}` |
|     2692 |  8839 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2686 |  8840 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2686 |  8841 | `				if( pObj ){` |
|        - |  8842 | `					/* Evaluate the default value and extract it's result */` |
|     2686 |  8843 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2686 |  8844 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  8845 | `						goto Abort;` |
|        - |  8846 | `					}` |
|        - |  8847 | `					/* Insert argument index */` |
|     2686 |  8848 | `					sArg.nIdx = pObj->nIdx;` |
|     2686 |  8849 | `					sArg.pUserData = 0;` |
|     2686 |  8850 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  8851 | `					/* Make sure the default argument is of the correct type */` |
|     2684 |  8852 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1764 |  8853 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 |  8854 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  8855 | `						/* Cast to the desired type */` |
|        3 |  8856 | `						xCast(pObj);` |
|        1 |  8857 | `					}` |
|     1342 |  8858 | `				}` |
|     1342 |  8859 | `			}` |
|     2692 |  8860 | `			++n;` |
|        2 |  8861 | `		}` |
|        - |  8862 | `		} /* end VmCallArgMap scope */` |
|        - |  8863 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  8864 | `		 * does not return anything.` |
|        - |  8865 | `		 */` |
|    17490 |  8866 | `		PH7_MemObjRelease(pTos);` |
|    17490 |  8867 | `		pTos = &pTos[-nCallArgs];` |
|        - |  8868 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    17490 |  8869 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    17490 |  8870 | `		if( pFrameStack == 0 ){` |
|        - |  8871 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  8872 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  8873 | `				&pVmFunc->sName);` |
|      ! 0 |  8874 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8875 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8876 | `			}` |
|      ! 0 |  8877 | `			break;` |
|        - |  8878 | `		}` |
|     8744 |  8879 | `SkipFuncBody:` |
|    17520 |  8880 | `		if( pSelf ){` |
|        - |  8881 | `			/* Push class name */` |
|     2978 |  8882 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1488 |  8883 | `		}` |
|        - |  8884 | `		/* Increment nesting level */` |
|    17520 |  8885 | `		pVm->nRecursionDepth++;` |
|    17520 |  8886 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  8887 | `			/* Execute function body */` |
|    26234 |  8888 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    17488 |  8889 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0);` |
|     8744 |  8890 | `		}` |
|        - |  8891 | `		/* Decrement nesting level */` |
|    17520 |  8892 | `		pVm->nRecursionDepth--;` |
|    17520 |  8893 | `		if( pSelf ){` |
|        - |  8894 | `			/* Pop class name */` |
|     2978 |  8895 | `			(void)SySetPop(&pVm->aSelf);` |
|     1488 |  8896 | `		}` |
|        - |  8897 | `		/* Cleanup the mess left behind */` |
|    17520 |  8898 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  8899 | `			/* Return by reference,reflect that */` |
|        9 |  8900 | `			if( n != SXU32_HIGH ){` |
|        9 |  8901 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  8902 | `				sxu32 i;` |
|        - |  8903 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  8904 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  8905 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  8906 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  8907 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  8908 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  8909 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  8910 | `								&pVmFunc->sName);` |
|      ! 0 |  8911 | `						}` |
|      ! 0 |  8912 | `						n = SXU32_HIGH;` |
|      ! 0 |  8913 | `						break;` |
|        - |  8914 | `					}` |
|        3 |  8915 | `				}` |
|        5 |  8916 | `			}else{` |
|      ! 0 |  8917 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  8918 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  8919 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  8920 | `						&pVmFunc->sName);` |
|      ! 0 |  8921 | `				}` |
|        - |  8922 | `			}` |
|        9 |  8923 | `			pTos->nIdx = n;` |
|        4 |  8924 | `		}` |
|        - |  8925 | `		/* Cleanup the mess left behind */` |
|    17520 |  8926 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  8927 | `			/* An exception was throw in this frame */` |
|       62 |  8928 | `			pFrame = pFrame->pParent;` |
|       62 |  8929 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  8930 | `				/* Pop the resutlt */` |
|       60 |  8931 | `				VmPopOperand(&pTos,1);` |
|        - |  8932 | `				/* Jump to this destination */` |
|       60 |  8933 | `				pc = pFrame->iExceptionJump - 1;` |
|       60 |  8934 | `				rc = PH7_OK;` |
|       31 |  8935 | `			}else{` |
|        3 |  8936 | `				if( pFrame->pParent ){` |
|        3 |  8937 | `					rc = PH7_EXCEPTION;` |
|        2 |  8938 | `				}else{` |
|        - |  8939 | `					/* Continue normal execution */` |
|      ! 0 |  8940 | `					rc = PH7_OK;` |
|        - |  8941 | `				}` |
|        - |  8942 | `			}` |
|       30 |  8943 | `		}` |
|        - |  8944 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    17520 |  8945 | `		if( pFrameStack ){` |
|    17490 |  8946 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     8744 |  8947 | `		}` |
|        - |  8948 | `		/* Leave the frame */` |
|    17520 |  8949 | `		VmLeaveFrame(&(*pVm));` |
|    17520 |  8950 | `		if( rc == PH7_ABORT ){` |
|        - |  8951 | `			/* Abort processing immeditaley */` |
|       15 |  8952 | `			goto Abort;` |
|    17506 |  8953 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  8954 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  8955 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  8956 | `			 * overwriting the state saved by the inner level.` |
|        - |  8957 | `			 * pTos points to the result slot (not yet written).` |
|        - |  8958 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  8959 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  8960 | `			goto Suspend;` |
|    17468 |  8961 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  8962 | `			goto Exception;` |
|        - |  8963 | `		}` |
|     8734 |  8964 | `	}else{` |
|        - |  8965 | `		ph7_user_func *pFunc;` |
|        - |  8966 | `		ph7_context sCtx;` |
|        - |  8967 | `		ph7_value sRet;` |
|        - |  8968 | `		/* Look for an installed foreign function.` |
|        - |  8969 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  8970 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - |  8971 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - |  8972 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   675440 |  8973 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8974 | `		{` |
|   675440 |  8975 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   675440 |  8976 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - |  8977 | `			/* Compiler-qualified: try short name as global fallback */` |
|       22 |  8978 | `			const char *zShort = sName.zString;` |
|        - |  8979 | `			sxu32 i;` |
|      334 |  8980 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      314 |  8981 | `				if( sName.zString[i] == '\\' ){` |
|       28 |  8982 | `					zShort = &sName.zString[i + 1];` |
|       13 |  8983 | `				}` |
|      158 |  8984 | `			}` |
|       22 |  8985 | `			if( zShort != sName.zString ){` |
|       22 |  8986 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       22 |  8987 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 |  8988 | `			}` |
|       10 |  8989 | `		}` |
|        - |  8990 | `		} /* end VmCallArgMap namespace scope */` |
|   675440 |  8991 | `		if( pEntry == 0 ){` |
|        - |  8992 | `			/* Call to undefined function */` |
|        5 |  8993 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  8994 | `			/* Pop given arguments */` |
|        5 |  8995 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  8996 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8997 | `			}` |
|        - |  8998 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  8999 | `			PH7_MemObjRelease(pTos);` |
|       43 |  9000 | `			break;` |
|        - |  9001 | `		}` |
|   675436 |  9002 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  9003 | `		/* Start collecting function arguments */` |
|   675436 |  9004 | `		SySetReset(&aArg);` |
|  1818324 |  9005 | `		while( pArg < pTos ){` |
|  1142890 |  9006 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1142890 |  9007 | `			pArg++;` |
|        2 |  9008 | `		}` |
|        - |  9009 | `		/* Assume a null return value */` |
|   675436 |  9010 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  9011 | `		/* Init the call context */` |
|   675436 |  9012 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  9013 | `		/* Call the foreign function */` |
|   675436 |  9014 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  9015 | `		/* Release the call context */` |
|   675436 |  9016 | `		VmReleaseCallContext(&sCtx);` |
|   675436 |  9017 | `		if( rc == PH7_ABORT ){` |
|      489 |  9018 | `			goto Abort;` |
|   674948 |  9019 | `		}else if( rc == PH7_EXCEPTION ){` |
|       82 |  9020 | `			VmFrame *pFrm = pVm->pFrame;` |
|       82 |  9021 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       82 |  9022 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  9023 | `				/* Exception was NOT caught, propagate */` |
|        5 |  9024 | `				goto Exception;` |
|        - |  9025 | `			}` |
|        - |  9026 | `			/* Exception was caught: pop args and the result slot */` |
|       77 |  9027 | `			PH7_MemObjRelease(&sRet);` |
|       77 |  9028 | `			if( pInstr->iP1 > 0 ){` |
|       61 |  9029 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       30 |  9030 | `			}` |
|        - |  9031 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|       77 |  9032 | `			VmPopOperand(&pTos,1);` |
|        - |  9033 | `			/* Jump past the try/catch block via the exception frame */` |
|       77 |  9034 | `			pFrm = pVm->pFrame;` |
|       77 |  9035 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|       77 |  9036 | `				pc = pFrm->iExceptionJump - 1;` |
|       38 |  9037 | `			}` |
|       77 |  9038 | `			break;` |
|        - |  9039 | `		}` |
|   674868 |  9040 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9041 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  9042 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  9043 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  9044 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  9045 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  9046 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  9047 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  9048 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  9049 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  9050 | `			}` |
|        - |  9051 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  9052 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  9053 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  9054 | `			goto Suspend;` |
|        - |  9055 | `		}` |
|   674830 |  9056 | `		if( pInstr->iP1 > 0 ){` |
|        - |  9057 | `			/* Pop function name and arguments */` |
|   653452 |  9058 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   326747 |  9059 | `		}` |
|        - |  9060 | `		/* Save foreign function return value */` |
|   674830 |  9061 | `		PH7_MemObjStore(&sRet,pTos);` |
|   674830 |  9062 | `		PH7_MemObjRelease(&sRet);` |
|        - |  9063 | `	}` |
|   692294 |  9064 | `	break;` |
|        - |  9065 | `				  }` |
|        - |  9066 | `/*` |
|        - |  9067 | ` * OP_CONSUME: P1 * *` |
|        - |  9068 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  9069 | ` */` |
|    14799 |  9070 | `case PH7_OP_CONSUME: {` |
|    29600 |  9071 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    29600 |  9072 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  9073 |  |
|    29600 |  9074 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    29600 |  9075 | `	pCur = pOut;` |
|        - |  9076 | `	/* Start the consume process  */` |
|    59198 |  9077 | `	while( pOut <= pTos ){` |
|        - |  9078 | `		/* Force a string cast */` |
|    29600 |  9079 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      892 |  9080 | `			PH7_MemObjToString(pOut);` |
|      445 |  9081 | `		}` |
|    29600 |  9082 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  9083 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  9084 | `			/* Invoke the output consumer callback */` |
|    17578 |  9085 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    17578 |  9086 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    17578 |  9087 | `			SyBlobRelease(&pOut->sBlob);` |
|    17578 |  9088 | `			if( rc == SXERR_ABORT ){` |
|        - |  9089 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  9090 | `				goto Abort;` |
|        - |  9091 | `			}` |
|     8788 |  9092 | `		}` |
|    29600 |  9093 | `		pOut++;` |
|        2 |  9094 | `	}` |
|    29600 |  9095 | `	pTos = &pCur[-1];` |
|    29598 |  9096 | `	break;` |
|        - |  9097 | `					 }` |
|        - |  9098 |  |
|        - |  9099 | `		} /* Switch() */` |
| 11485388 |  9100 | `		pc++; /* Next instruction in the stream */` |
|        2 |  9101 | `	} /* For(;;) */` |
|    20976 |  9102 | `Done:` |
|    41954 |  9103 | `	SySetRelease(&aArg);` |
|    41954 |  9104 | `	return SXRET_OK;` |
|       72 |  9105 | `Suspend:` |
|      146 |  9106 | `	SySetRelease(&aArg);` |
|      146 |  9107 | `	return PH7_SUSPEND;` |
|      268 |  9108 | `Abort:` |
|      537 |  9109 | `	SySetRelease(&aArg);` |
|     1833 |  9110 | `	while( pTos >= pStack ){` |
|     1297 |  9111 | `		PH7_MemObjRelease(pTos);` |
|     1297 |  9112 | `		pTos--;` |
|        1 |  9113 | `	}` |
|      537 |  9114 | `	return PH7_ABORT;` |
|       10 |  9115 | `Exception:` |
|       22 |  9116 | `	SySetRelease(&aArg);` |
|       36 |  9117 | `	while( pTos >= pStack ){` |
|       16 |  9118 | `		PH7_MemObjRelease(pTos);` |
|       16 |  9119 | `		pTos--;` |
|        2 |  9120 | `	}` |
|       22 |  9121 | `	return PH7_EXCEPTION;` |
|    21328 |  9122 |  |
|        - |  9123 | `/*` |
|        - |  9124 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  9125 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  9126 | ` * See block-comment on that function for additional information.` |
|        - |  9127 | ` */` |
|    19682 |  9128 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  9129 |  |
|        - |  9130 | `	ph7_value *pStack;` |
|        - |  9131 | `	sxi32 rc;` |
|        - |  9132 | `	/* Allocate a new operand stack */` |
|    19684 |  9133 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    19684 |  9134 | `	if( pStack == 0 ){` |
|      ! 0 |  9135 | `		return SXERR_MEM;` |
|        - |  9136 | `	}` |
|        - |  9137 | `	/* Execute the program */` |
|    19684 |  9138 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0);` |
|        - |  9139 | `	/* Free the operand stack */` |
|    19684 |  9140 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  9141 | `	/* Execution result */` |
|    19684 |  9142 | `	return rc;` |
|     9843 |  9143 |  |
|        - |  9144 | `/*` |
|        - |  9145 | ` * Invoke any installed shutdown callbacks.` |
|        - |  9146 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  9147 | ` * or more calls to [register_shutdown_function()].` |
|        - |  9148 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  9149 | ` * execution ends.` |
|        - |  9150 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  9151 | ` * additional information.` |
|        - |  9152 | ` */` |
|     2684 |  9153 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  9154 |  |
|        - |  9155 | `	VmShutdownCB *pEntry;` |
|        - |  9156 | `	ph7_value *apArg[10];` |
|        - |  9157 | `	sxu32 n,nEntry;` |
|        - |  9158 | `	int i;` |
|        - |  9159 | `	/* Point to the stack of registered callbacks */` |
|     2686 |  9160 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    29526 |  9161 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    26842 |  9162 | `		apArg[i] = 0;` |
|    13422 |  9163 | `	}` |
|     2688 |  9164 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  9165 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  9166 | `		if( pEntry ){` |
|        - |  9167 | `			/* Prepare callback arguments if any */` |
|        3 |  9168 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  9169 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  9170 | `					break;` |
|        - |  9171 | `				}` |
|      ! 0 |  9172 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  9173 | `			}` |
|        - |  9174 | `			/* Invoke the callback */` |
|        3 |  9175 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  9176 | `			/*` |
|        - |  9177 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  9178 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  9179 | `			 */` |
|        3 |  9180 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  9181 | `			if( pEntry ){` |
|        3 |  9182 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  9183 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  9184 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  9185 | `				}` |
|        1 |  9186 | `			}` |
|        1 |  9187 | `		}` |
|        2 |  9188 | `	}` |
|     2686 |  9189 | `	SySetReset(&pVm->aShutdown);` |
|     2686 |  9190 |  |
|        - |  9191 | `/*` |
|        - |  9192 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  9193 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  9194 | ` * See block-comment on that function for additional information.` |
|        - |  9195 | ` */` |
|     2692 |  9196 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  9197 |  |
|        - |  9198 | `	/* Make sure we are ready to execute this program */` |
|     2694 |  9199 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  9200 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  9201 | `	}` |
|        - |  9202 | `	/* Set the execution magic number  */` |
|     2694 |  9203 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  9204 | `	/* Execute the program */` |
|     2694 |  9205 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0);` |
|        - |  9206 | `	/* Invoke any shutdown callbacks */` |
|     2690 |  9207 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  9208 | `	/*` |
|        - |  9209 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  9210 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  9211 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  9212 | `	 */` |
|     2690 |  9213 | `	return SXRET_OK;` |
|     1348 |  9214 |  |
|        - |  9215 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  9216 | `/*` |
|        - |  9217 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  9218 | ` * The context is in CREATED state and ready to be started.` |
|        - |  9219 | ` */` |
|       46 |  9220 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  9221 |  |
|        - |  9222 | `	ph7_exec_ctx *pCtx;` |
|        - |  9223 | `	ph7_value *pStack;` |
|        - |  9224 | `	VmFrame *pFrame;` |
|       48 |  9225 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       48 |  9226 | `	if( pCtx == 0 ){` |
|      ! 0 |  9227 | `		return 0;` |
|        - |  9228 | `	}` |
|       48 |  9229 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       48 |  9230 | `	pCtx->pVm = pVm;` |
|       48 |  9231 | `	pCtx->pFunc = pFunc;` |
|       48 |  9232 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       48 |  9233 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       48 |  9234 | `	pCtx->pc = 0;` |
|       48 |  9235 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       48 |  9236 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  9237 | `	/* Allocate a private operand stack */` |
|       48 |  9238 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       48 |  9239 | `	if( pStack == 0 ){` |
|      ! 0 |  9240 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9241 | `		return 0;` |
|        - |  9242 | `	}` |
|       48 |  9243 | `	pCtx->pStack = pStack;` |
|        - |  9244 | `	/* Create a detached frame for the fiber */` |
|       48 |  9245 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       48 |  9246 | `	if( pFrame == 0 ){` |
|      ! 0 |  9247 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  9248 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9249 | `		return 0;` |
|        - |  9250 | `	}` |
|       48 |  9251 | `	pCtx->pFrame = pFrame;` |
|       48 |  9252 | `	return pCtx;` |
|       25 |  9253 |  |
|        - |  9254 | `/*` |
|        - |  9255 | ` * Start executing a fiber context for the first time.` |
|        - |  9256 | ` */` |
|       46 |  9257 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  9258 |  |
|        - |  9259 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9260 | `	sxi32 rc;` |
|       48 |  9261 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9262 | `		return SXERR_INVALID;` |
|        - |  9263 | `	}` |
|        - |  9264 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 |  9265 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 |  9266 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9267 | `	/* Save and set the active context */` |
|       48 |  9268 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 |  9269 | `	pVm->pActiveCtx = pCtx;` |
|       48 |  9270 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 |  9271 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 |  9272 | `	pVm->nRecursionDepth++;` |
|        - |  9273 | `	/* Execute from the beginning */` |
|       48 |  9274 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 |  9275 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       46 |  9276 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|       48 |  9277 | `	pVm->nRecursionDepth--;` |
|        - |  9278 | `	/* Restore the previous context */` |
|       48 |  9279 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 |  9280 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9281 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 |  9282 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 |  9283 | `		pCtx->pFrame->pParent = 0;` |
|       46 |  9284 | `		if( pResult ){` |
|       24 |  9285 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  9286 | `		}` |
|       46 |  9287 | `		return SXRET_OK;` |
|        - |  9288 | `	}` |
|        - |  9289 | `	/* Detach frame */` |
|        3 |  9290 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  9291 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  9292 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  9293 | `	}` |
|        3 |  9294 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9295 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9296 | `		return PH7_ABORT;` |
|        - |  9297 | `	}` |
|        3 |  9298 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9299 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9300 | `		return PH7_EXCEPTION;` |
|        - |  9301 | `	}` |
|        - |  9302 | `	/* Normal completion */` |
|        3 |  9303 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  9304 | `	if( pResult ){` |
|        3 |  9305 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  9306 | `	}` |
|        3 |  9307 | `	return SXRET_OK;` |
|       25 |  9308 |  |
|        - |  9309 | `/*` |
|        - |  9310 | ` * Resume a suspended fiber context.` |
|        - |  9311 | ` */` |
|       98 |  9312 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  9313 |  |
|        - |  9314 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9315 | `	sxi32 rc;` |
|      100 |  9316 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  9317 | `		return SXERR_INVALID;` |
|        - |  9318 | `	}` |
|        - |  9319 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  9320 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  9321 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 |  9322 | `	if( pResumeValue ){` |
|       40 |  9323 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  9324 | `	}else{` |
|       62 |  9325 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  9326 | `	}` |
|      100 |  9327 | `	pCtx->nTos++;` |
|        - |  9328 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 |  9329 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 |  9330 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9331 | `	/* Save and set the active context */` |
|      100 |  9332 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 |  9333 | `	pVm->pActiveCtx = pCtx;` |
|      100 |  9334 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 |  9335 | `	pVm->nRecursionDepth++;` |
|        - |  9336 | `	/* Resume execution from saved PC */` |
|      100 |  9337 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 |  9338 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|       98 |  9339 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|      100 |  9340 | `	pVm->nRecursionDepth--;` |
|        - |  9341 | `	/* Restore the previous context */` |
|      100 |  9342 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 |  9343 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9344 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 |  9345 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 |  9346 | `		pCtx->pFrame->pParent = 0;` |
|       64 |  9347 | `		if( pResult ){` |
|       18 |  9348 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  9349 | `		}` |
|       64 |  9350 | `		return SXRET_OK;` |
|        - |  9351 | `	}` |
|        - |  9352 | `	/* Detach frame */` |
|       38 |  9353 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 |  9354 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 |  9355 | `		pCtx->pFrame->pParent = 0;` |
|       18 |  9356 | `	}` |
|       38 |  9357 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9358 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9359 | `		return PH7_ABORT;` |
|        - |  9360 | `	}` |
|       38 |  9361 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9362 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9363 | `		return PH7_EXCEPTION;` |
|        - |  9364 | `	}` |
|        - |  9365 | `	/* Normal completion */` |
|       38 |  9366 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 |  9367 | `	if( pResult ){` |
|       20 |  9368 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  9369 | `	}` |
|       38 |  9370 | `	return SXRET_OK;` |
|       51 |  9371 |  |
|        - |  9372 | `/*` |
|        - |  9373 | ` * Release an execution context and all its resources.` |
|        - |  9374 | ` */` |
|        4 |  9375 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  9376 |  |
|        5 |  9377 | `	if( pCtx == 0 ){` |
|      ! 0 |  9378 | `		return;` |
|        - |  9379 | `	}` |
|        5 |  9380 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  9381 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  9382 | `		return;` |
|        - |  9383 | `	}` |
|        5 |  9384 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  9385 | `	/* Release values */` |
|        5 |  9386 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  9387 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  9388 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  9389 | `	if( pCtx->pFrame ){` |
|        - |  9390 | `		VmSlot *aSlot;` |
|        - |  9391 | `		sxu32 n;` |
|        - |  9392 | `		/* Free local variables */` |
|        5 |  9393 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  9394 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  9395 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  9396 | `		}` |
|        - |  9397 | `		/* Remove local references */` |
|        5 |  9398 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  9399 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  9400 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  9401 | `		}` |
|        5 |  9402 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  9403 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  9404 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  9405 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  9406 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  9407 | `		pCtx->pFrame = 0;` |
|        2 |  9408 | `	}` |
|        - |  9409 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  9410 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  9411 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  9412 | `	if( pCtx->pStack ){` |
|        5 |  9413 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  9414 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  9415 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  9416 | `				PH7_MemObjRelease(pTos);` |
|        5 |  9417 | `				pTos--;` |
|        1 |  9418 | `			}` |
|        2 |  9419 | `		}` |
|        5 |  9420 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  9421 | `		pCtx->pStack = 0;` |
|        2 |  9422 | `	}` |
|        - |  9423 | `	/* Free the context itself */` |
|        5 |  9424 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  9425 |  |
|        - |  9426 | `/*` |
|        - |  9427 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  9428 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  9429 | ` */` |
|       90 |  9430 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  9431 |  |
|        - |  9432 | `	ph7_class_instance *pThis;` |
|        - |  9433 | `	SyString sAttr;` |
|        - |  9434 | `	ph7_value *pAttr;` |
|       92 |  9435 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9436 | `		return 0;` |
|        - |  9437 | `	}` |
|       92 |  9438 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  9439 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  9440 | `		return 0;` |
|        - |  9441 | `	}` |
|       92 |  9442 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  9443 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  9444 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  9445 | `		return 0;` |
|        - |  9446 | `	}` |
|       62 |  9447 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  9448 |  |
|        - |  9449 | `/*` |
|        - |  9450 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  9451 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  9452 | ` */` |
|       38 |  9453 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9454 |  |
|       40 |  9455 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  9456 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  9457 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9458 | `			"Cannot suspend outside of a fiber");` |
|        - |  9459 | `	}` |
|       40 |  9460 | `	if( nArg > 0 ){` |
|       40 |  9461 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  9462 | `	}else{` |
|      ! 0 |  9463 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  9464 | `	}` |
|       40 |  9465 | `	return PH7_SUSPEND;` |
|       21 |  9466 |  |
|        - |  9467 | `/*` |
|        - |  9468 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  9469 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  9470 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  9471 | ` */` |
|       24 |  9472 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9473 |  |
|        - |  9474 | `	ph7_class_instance *pThis;` |
|        - |  9475 | `	ph7_value *pAttr;` |
|        - |  9476 | `	SyString sAttrName;` |
|       26 |  9477 | `	if( nArg < 2 ){` |
|      ! 0 |  9478 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9479 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  9480 | `	}` |
|       26 |  9481 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9482 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9483 | `			"Fiber::__construct(): invalid $this");` |
|        - |  9484 | `	}` |
|       26 |  9485 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  9486 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  9487 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9488 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  9489 | `	}` |
|        - |  9490 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  9491 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  9492 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9493 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  9494 | `	}` |
|        - |  9495 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  9496 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  9497 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  9498 | `	if( pAttr ){` |
|       26 |  9499 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  9500 | `	}` |
|       26 |  9501 | `	return PH7_OK;` |
|       14 |  9502 |  |
|        - |  9503 | `/*` |
|        - |  9504 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  9505 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  9506 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  9507 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  9508 | ` */` |
|       24 |  9509 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  9510 | `	ph7_class_instance **ppThis)` |
|        2 |  9511 |  |
|       26 |  9512 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9513 | `	ph7_value *pCallable;` |
|        - |  9514 | `	SyString sAttrName;` |
|       26 |  9515 | `	*ppThis = 0;` |
|       26 |  9516 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  9517 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  9518 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  9519 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  9520 | `		return 0;` |
|        - |  9521 | `	}` |
|       26 |  9522 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  9523 | `		/* String callable — look up in user functions with overload support */` |
|        - |  9524 | `		SyString sName;` |
|        - |  9525 | `		SyHashEntry *pEntry;` |
|        - |  9526 | `		ph7_vm_func *pFunc;` |
|       26 |  9527 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  9528 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  9529 | `		if( pEntry == 0 ){` |
|      ! 0 |  9530 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  9531 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  9532 | `			return 0;` |
|        - |  9533 | `		}` |
|       26 |  9534 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  9535 | `		return pFunc;` |
|      ! 0 |  9536 | `	}else{` |
|        - |  9537 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  9538 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  9539 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  9540 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  9541 | `		if( pMethod == 0 ){` |
|      ! 0 |  9542 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9543 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  9544 | `			return 0;` |
|        - |  9545 | `		}` |
|      ! 0 |  9546 | `		*ppThis = pClosure;` |
|      ! 0 |  9547 | `		return &pMethod->sFunc;` |
|        - |  9548 | `	}` |
|       14 |  9549 |  |
|        - |  9550 | `/*` |
|        - |  9551 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  9552 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  9553 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  9554 | ` */` |
|       46 |  9555 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  9556 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  9557 |  |
|       48 |  9558 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  9559 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  9560 | `	sxu32 nFormal, n;` |
|        - |  9561 | `	VmSlot sSlot;` |
|        - |  9562 | `	sxi32 rc;` |
|        - |  9563 | `	/* Install $this for closure/method callables */` |
|       48 |  9564 | `	if( pClosureThis ){` |
|        - |  9565 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  9566 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  9567 | `		if( pObj ){` |
|      ! 0 |  9568 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  9569 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  9570 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  9571 | `		}` |
|      ! 0 |  9572 | `	}` |
|        - |  9573 | `	/* Install static variables */` |
|       48 |  9574 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  9575 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  9576 | `		ph7_value *pVal;` |
|      ! 0 |  9577 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  9578 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  9579 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  9580 | `			if( pVal ){` |
|      ! 0 |  9581 | `				sSlot.pUserData = 0;` |
|      ! 0 |  9582 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  9583 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  9584 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  9585 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  9586 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  9587 | `				}` |
|      ! 0 |  9588 | `			}` |
|      ! 0 |  9589 | `		}` |
|      ! 0 |  9590 | `	}` |
|        - |  9591 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       48 |  9592 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       48 |  9593 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       66 |  9594 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  9595 | `		ph7_value *pObj;` |
|       20 |  9596 | `		if( n < (sxu32)nArg ){` |
|        - |  9597 | `			/* Argument provided — install with type casting */` |
|       20 |  9598 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 |  9599 | `			if( pObj ){` |
|       20 |  9600 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  9601 | `				/* Type casting */` |
|       20 |  9602 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  9603 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9604 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9605 | `						if( xCast ){` |
|      ! 0 |  9606 | `							xCast(pObj);` |
|      ! 0 |  9607 | `						}` |
|      ! 0 |  9608 | `					}` |
|      ! 0 |  9609 | `				}` |
|       20 |  9610 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 |  9611 | `				sSlot.pUserData = 0;` |
|       20 |  9612 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 |  9613 | `			}` |
|        9 |  9614 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  9615 | `			/* Default value */` |
|      ! 0 |  9616 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  9617 | `			if( pObj ){` |
|      ! 0 |  9618 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  9619 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9620 | `					return rc;` |
|        - |  9621 | `				}` |
|      ! 0 |  9622 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  9623 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9624 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9625 | `						if( xCast ){` |
|      ! 0 |  9626 | `							xCast(pObj);` |
|      ! 0 |  9627 | `						}` |
|      ! 0 |  9628 | `					}` |
|      ! 0 |  9629 | `				}` |
|      ! 0 |  9630 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  9631 | `				sSlot.pUserData = 0;` |
|      ! 0 |  9632 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  9633 | `			}` |
|      ! 0 |  9634 | `		}` |
|       11 |  9635 | `	}` |
|        - |  9636 | `	/* Install closure environment (captured variables) */` |
|       48 |  9637 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  9638 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  9639 | `		ph7_value *pValue;` |
|        - |  9640 | `		sxu32 iEnv;` |
|        3 |  9641 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  9642 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  9643 | `			pEnv = &aEnv[iEnv];` |
|        7 |  9644 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  9645 | `				continue;` |
|        - |  9646 | `			}` |
|        5 |  9647 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  9648 | `			if( pValue == 0 ){` |
|      ! 0 |  9649 | `				continue;` |
|        - |  9650 | `			}` |
|        5 |  9651 | `			PH7_MemObjRelease(pValue);` |
|        5 |  9652 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  9653 | `		}` |
|        1 |  9654 | `	}` |
|       48 |  9655 | `	return SXRET_OK;` |
|       25 |  9656 |  |
|        - |  9657 | `/*` |
|        - |  9658 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  9659 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  9660 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  9661 | ` */` |
|       26 |  9662 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9663 |  |
|       28 |  9664 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9665 | `	ph7_class_instance *pThis;` |
|        - |  9666 | `	ph7_class_instance *pClosureThis;` |
|        - |  9667 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  9668 | `	ph7_vm_func *pFunc;` |
|        - |  9669 | `	ph7_value sResult;` |
|        - |  9670 | `	ph7_value *pCtxAttr;` |
|        - |  9671 | `	SyString sAttrName;` |
|        - |  9672 | `	sxi32 rc;` |
|       28 |  9673 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9674 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  9675 | `	}` |
|       28 |  9676 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  9677 | `	/* Check if already started (has a __ctx) */` |
|       28 |  9678 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 |  9679 | `	if( pExecCtx != 0 ){` |
|        3 |  9680 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9681 | `			"Cannot start a fiber that has already been started");` |
|        - |  9682 | `	}` |
|        - |  9683 | `	/* Resolve callable */` |
|       26 |  9684 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 |  9685 | `	if( pFunc == 0 ){` |
|      ! 0 |  9686 | `		return PH7_EXCEPTION;` |
|        - |  9687 | `	}` |
|        - |  9688 | `	/* Create execution context now that we know the function */` |
|       26 |  9689 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 |  9690 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  9691 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9692 | `			"Fiber::start(): out of memory");` |
|        - |  9693 | `	}` |
|        - |  9694 | `	/* Store context in $this->__ctx */` |
|       26 |  9695 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 |  9696 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  9697 | `	if( pCtxAttr ){` |
|       26 |  9698 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 |  9699 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  9700 | `	}` |
|        - |  9701 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  9702 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  9703 | `	 * into the fiber's frame, not the caller's. */` |
|       26 |  9704 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 |  9705 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  9706 | `	/* Unpack the args array and install into the frame */` |
|        - |  9707 | `	{` |
|       26 |  9708 | `		ph7_value **apValues = 0;` |
|       26 |  9709 | `		int nActual = 0;` |
|       26 |  9710 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 |  9711 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  9712 | `			ph7_hashmap_node *pNode;` |
|       26 |  9713 | `			sxu32 nCount = pMap->nEntry;` |
|       26 |  9714 | `			if( nCount > 0 ){` |
|        3 |  9715 | `				sxu32 idx = 0;` |
|        4 |  9716 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  9717 | `					nCount * sizeof(ph7_value *));` |
|        3 |  9718 | `				if( apValues ){` |
|        3 |  9719 | `					pNode = pMap->pFirst;` |
|        7 |  9720 | `					while( pNode && idx < nCount ){` |
|        5 |  9721 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  9722 | `						idx++;` |
|        5 |  9723 | `						pNode = pNode->pPrev;` |
|        1 |  9724 | `					}` |
|        3 |  9725 | `					nActual = (int)idx;` |
|        1 |  9726 | `				}` |
|        1 |  9727 | `			}` |
|       12 |  9728 | `		}` |
|       26 |  9729 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 |  9730 | `		if( apValues ){` |
|        3 |  9731 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  9732 | `		}` |
|        - |  9733 | `	}` |
|        - |  9734 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 |  9735 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 |  9736 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 |  9737 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9738 | `		return PH7_ABORT;` |
|        - |  9739 | `	}` |
|       26 |  9740 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 |  9741 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 |  9742 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9743 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9744 | `		return PH7_ABORT;` |
|        - |  9745 | `	}` |
|       26 |  9746 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9747 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9748 | `		return PH7_EXCEPTION;` |
|        - |  9749 | `	}` |
|       26 |  9750 | `	ph7_result_value(pCtx, &sResult);` |
|       26 |  9751 | `	PH7_MemObjRelease(&sResult);` |
|       26 |  9752 | `	return PH7_OK;` |
|       15 |  9753 |  |
|        - |  9754 | `/*` |
|        - |  9755 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  9756 | ` */` |
|       36 |  9757 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9758 |  |
|       38 |  9759 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9760 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  9761 | `	ph7_value sResult;` |
|        - |  9762 | `	ph7_value *pResumeVal;` |
|        - |  9763 | `	sxi32 rc;` |
|       38 |  9764 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9765 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  9766 | `		return PH7_OK;` |
|        - |  9767 | `	}` |
|       38 |  9768 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 |  9769 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  9770 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  9771 | `		return PH7_OK;` |
|        - |  9772 | `	}` |
|       38 |  9773 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  9774 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9775 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  9776 | `	}` |
|       36 |  9777 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 |  9778 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 |  9779 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 |  9780 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9781 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9782 | `		return PH7_ABORT;` |
|        - |  9783 | `	}` |
|       36 |  9784 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9785 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9786 | `		return PH7_EXCEPTION;` |
|        - |  9787 | `	}` |
|       36 |  9788 | `	ph7_result_value(pCtx, &sResult);` |
|       36 |  9789 | `	PH7_MemObjRelease(&sResult);` |
|       36 |  9790 | `	return PH7_OK;` |
|       20 |  9791 |  |
|        - |  9792 | `/*` |
|        - |  9793 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  9794 | ` */` |
|        6 |  9795 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9796 |  |
|        8 |  9797 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9798 | `	ph7_exec_ctx *pExecCtx;` |
|        8 |  9799 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9800 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9801 | `		return PH7_OK;` |
|        - |  9802 | `	}` |
|        8 |  9803 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 |  9804 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  9805 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9806 | `		return PH7_OK;` |
|        - |  9807 | `	}` |
|        8 |  9808 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  9809 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9810 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9811 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  9812 | `		}` |
|      ! 0 |  9813 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9814 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  9815 | `	}` |
|        8 |  9816 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 |  9817 | `	return PH7_OK;` |
|        5 |  9818 |  |
|        - |  9819 | `/*` |
|        - |  9820 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  9821 | ` */` |
|        6 |  9822 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9823 |  |
|        - |  9824 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9825 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9826 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9827 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  9828 | `	return PH7_OK;` |
|        4 |  9829 |  |
|      ! 0 |  9830 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  9831 |  |
|        - |  9832 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  9833 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  9834 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  9835 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  9836 | `	return PH7_OK;` |
|      ! 0 |  9837 |  |
|        6 |  9838 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9839 |  |
|        - |  9840 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9841 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9842 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9843 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  9844 | `	return PH7_OK;` |
|        4 |  9845 |  |
|        6 |  9846 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9847 |  |
|        - |  9848 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9849 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9850 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9851 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  9852 | `	return PH7_OK;` |
|        4 |  9853 |  |
|        - |  9854 | `/*` |
|        - |  9855 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  9856 | ` */` |
|        4 |  9857 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9858 |  |
|        5 |  9859 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9860 | `	ph7_exec_ctx *pExecCtx;` |
|        5 |  9861 | `	if( nArg < 1 ){` |
|      ! 0 |  9862 | `		return PH7_OK;` |
|        - |  9863 | `	}` |
|        5 |  9864 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 |  9865 | `	if( pExecCtx ){` |
|        5 |  9866 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  9867 | `		/* Clear the attribute so double-free is prevented */` |
|        5 |  9868 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 |  9869 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  9870 | `			SyString sAttrName;` |
|        - |  9871 | `			ph7_value *pAttr;` |
|        5 |  9872 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 |  9873 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 |  9874 | `			if( pAttr ){` |
|        5 |  9875 | `				PH7_MemObjRelease(pAttr);` |
|        2 |  9876 | `			}` |
|        2 |  9877 | `		}` |
|        2 |  9878 | `	}` |
|        5 |  9879 | `	return PH7_OK;` |
|        3 |  9880 |  |
|        - |  9881 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  9882 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  9883 |  |
|        - |  9884 | `	ph7_class_instance *pThis;` |
|      ! 0 |  9885 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  9886 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  9887 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  9888 |  |
|      ! 0 |  9889 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  9890 |  |
|        - |  9891 | `	ph7_class_instance *pThis;` |
|      ! 0 |  9892 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  9893 | `	ph7_exec_ctx *pCtx;` |
|        - |  9894 | `	ph7_vm_func *pFunc;` |
|        - |  9895 | `	ph7_value *pCallable;` |
|        - |  9896 | `	ph7_value *pCtxAttr;` |
|        - |  9897 | `	SyString sAttrName;` |
|        - |  9898 | `	/* Must not already be started */` |
|      ! 0 |  9899 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9900 | `	if( pCtx != 0 ){` |
|      ! 0 |  9901 | `		return SXERR_INVALID;` |
|        - |  9902 | `	}` |
|      ! 0 |  9903 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9904 | `		return SXERR_INVALID;` |
|        - |  9905 | `	}` |
|      ! 0 |  9906 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  9907 | `	/* Get the callable */` |
|      ! 0 |  9908 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  9909 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9910 | `	if( pCallable == 0 ){` |
|      ! 0 |  9911 | `		return SXERR_INVALID;` |
|        - |  9912 | `	}` |
|        - |  9913 | `	/* Resolve callable */` |
|      ! 0 |  9914 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  9915 | `		SyString sName;` |
|        - |  9916 | `		SyHashEntry *pEntry;` |
|      ! 0 |  9917 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  9918 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  9919 | `		if( pEntry == 0 ){` |
|      ! 0 |  9920 | `			return SXERR_NOTFOUND;` |
|        - |  9921 | `		}` |
|      ! 0 |  9922 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  9923 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9924 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  9925 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  9926 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  9927 | `		if( pMethod == 0 ){` |
|      ! 0 |  9928 | `			return SXERR_INVALID;` |
|        - |  9929 | `		}` |
|      ! 0 |  9930 | `		pClosureThis = pClosure;` |
|      ! 0 |  9931 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  9932 | `	}else{` |
|      ! 0 |  9933 | `		return SXERR_INVALID;` |
|        - |  9934 | `	}` |
|        - |  9935 | `	/* Create context */` |
|      ! 0 |  9936 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  9937 | `	if( pCtx == 0 ){` |
|      ! 0 |  9938 | `		return SXERR_MEM;` |
|        - |  9939 | `	}` |
|        - |  9940 | `	/* Store in __ctx */` |
|      ! 0 |  9941 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  9942 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9943 | `	if( pCtxAttr ){` |
|      ! 0 |  9944 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  9945 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  9946 | `	}` |
|        - |  9947 | `	/* Set up frame with args */` |
|      ! 0 |  9948 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  9949 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  9950 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  9951 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  9952 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  9953 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  9954 |  |
|      ! 0 |  9955 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  9956 |  |
|      ! 0 |  9957 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9958 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  9959 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  9960 |  |
|      ! 0 |  9961 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9962 |  |
|      ! 0 |  9963 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9964 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  9965 |  |
|      ! 0 |  9966 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9967 |  |
|      ! 0 |  9968 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9969 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  9970 |  |
|      ! 0 |  9971 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9972 |  |
|      ! 0 |  9973 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9974 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  9975 | `	return &pCtx->sRetValue;` |
|      ! 0 |  9976 |  |
|        - |  9977 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  9978 | `/*` |
|        - |  9979 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  9980 | ` */` |
|       22 |  9981 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 |  9982 |  |
|        - |  9983 | `	ph7_generator *pGen;` |
|       24 |  9984 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       24 |  9985 | `	if( pGen == 0 ){` |
|      ! 0 |  9986 | `		return 0;` |
|        - |  9987 | `	}` |
|       24 |  9988 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       24 |  9989 | `	pGen->pCtx = pCtx;` |
|       24 |  9990 | `	pGen->iImplicitKey = 0;` |
|       24 |  9991 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       24 |  9992 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  9993 | `	/* Link the generator back to the exec context */` |
|       24 |  9994 | `	pCtx->pPrivate = pGen;` |
|       24 |  9995 | `	return pGen;` |
|       13 |  9996 |  |
|        - |  9997 | `/*` |
|        - |  9998 | ` * Release a generator and its execution context.` |
|        - |  9999 | ` */` |
|      ! 0 | 10000 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 | 10001 |  |
|      ! 0 | 10002 | `	if( pGen == 0 ){` |
|      ! 0 | 10003 | `		return;` |
|        - | 10004 | `	}` |
|      ! 0 | 10005 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 10006 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 10007 | `	if( pGen->pCtx ){` |
|      ! 0 | 10008 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 | 10009 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 | 10010 | `		pGen->pCtx = 0;` |
|      ! 0 | 10011 | `	}` |
|      ! 0 | 10012 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 | 10013 |  |
|        - | 10014 | `/*` |
|        - | 10015 | ` * Extract ph7_generator from a Generator class instance.` |
|        - | 10016 | ` */` |
|      236 | 10017 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 | 10018 |  |
|        - | 10019 | `	ph7_class_instance *pThis;` |
|        - | 10020 | `	SyString sAttr;` |
|        - | 10021 | `	ph7_value *pAttr;` |
|      238 | 10022 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10023 | `		return 0;` |
|        - | 10024 | `	}` |
|      238 | 10025 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 | 10026 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 | 10027 | `		return 0;` |
|        - | 10028 | `	}` |
|      238 | 10029 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 | 10030 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 | 10031 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 | 10032 | `		return 0;` |
|        - | 10033 | `	}` |
|      238 | 10034 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 | 10035 |  |
|        - | 10036 | `/*` |
|        - | 10037 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - | 10038 | ` */` |
|       22 | 10039 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10040 |  |
|        - | 10041 | `	ph7_generator *pGen;` |
|        - | 10042 | `	sxi32 rc;` |
|       24 | 10043 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 | 10044 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 | 10045 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 | 10046 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 | 10047 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 | 10048 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 | 10049 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 | 10050 | `	}` |
|       24 | 10051 | `	return PH7_OK;` |
|       13 | 10052 |  |
|        - | 10053 | `/*` |
|        - | 10054 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - | 10055 | ` */` |
|       68 | 10056 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10057 |  |
|        - | 10058 | `	ph7_generator *pGen;` |
|       70 | 10059 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 | 10060 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10061 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 | 10062 | `	return PH7_OK;` |
|       36 | 10063 |  |
|        - | 10064 | `/*` |
|        - | 10065 | ` * Generator::current() — return the last yielded value.` |
|        - | 10066 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10067 | ` */` |
|       68 | 10068 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10069 |  |
|        - | 10070 | `	ph7_generator *pGen;` |
|        - | 10071 | `	sxi32 rc;` |
|       70 | 10072 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10073 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10074 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10075 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10076 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 10077 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 10078 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 10079 | `	}` |
|       70 | 10080 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 | 10081 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 | 10082 | `	}else{` |
|      ! 0 | 10083 | `		ph7_result_null(pCtx);` |
|        - | 10084 | `	}` |
|       70 | 10085 | `	return PH7_OK;` |
|       36 | 10086 |  |
|        - | 10087 | `/*` |
|        - | 10088 | ` * Generator::key() — return the last yielded key.` |
|        - | 10089 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10090 | ` */` |
|       12 | 10091 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10092 |  |
|        - | 10093 | `	ph7_generator *pGen;` |
|        - | 10094 | `	sxi32 rc;` |
|       13 | 10095 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 10096 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 | 10097 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 10098 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10099 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 10100 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 10101 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 10102 | `	}` |
|       13 | 10103 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 | 10104 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 | 10105 | `	}else{` |
|      ! 0 | 10106 | `		ph7_result_null(pCtx);` |
|        - | 10107 | `	}` |
|       13 | 10108 | `	return PH7_OK;` |
|        7 | 10109 |  |
|        - | 10110 | `/*` |
|        - | 10111 | ` * Generator::next() — advance to the next yield point.` |
|        - | 10112 | ` */` |
|       60 | 10113 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10114 |  |
|        - | 10115 | `	ph7_generator *pGen;` |
|        - | 10116 | `	sxi32 rc;` |
|       62 | 10117 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 | 10118 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 | 10119 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 | 10120 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10121 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 | 10122 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 | 10123 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 | 10124 | `	}else{` |
|      ! 0 | 10125 | `		return PH7_OK;` |
|        - | 10126 | `	}` |
|       62 | 10127 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 | 10128 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 | 10129 | `	return PH7_OK;` |
|       32 | 10130 |  |
|        - | 10131 | `/*` |
|        - | 10132 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - | 10133 | ` */` |
|        4 | 10134 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10135 |  |
|        - | 10136 | `	ph7_generator *pGen;` |
|        - | 10137 | `	ph7_value *pSendVal;` |
|        - | 10138 | `	sxi32 rc;` |
|        5 | 10139 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 | 10140 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 | 10141 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 | 10142 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 | 10143 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - | 10144 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 | 10145 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 | 10146 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 | 10147 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 | 10148 | `	}else{` |
|      ! 0 | 10149 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10150 | `		return PH7_OK;` |
|        - | 10151 | `	}` |
|        5 | 10152 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 | 10153 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 | 10154 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10155 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 | 10156 | `	}else{` |
|        3 | 10157 | `		ph7_result_null(pCtx);` |
|        - | 10158 | `	}` |
|        5 | 10159 | `	return PH7_OK;` |
|        3 | 10160 |  |
|        - | 10161 | `/*` |
|        - | 10162 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - | 10163 | ` *` |
|        - | 10164 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - | 10165 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - | 10166 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - | 10167 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - | 10168 | ` * the exception to the caller.` |
|        - | 10169 | ` */` |
|      ! 0 | 10170 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10171 |  |
|        - | 10172 | `	ph7_generator *pGen;` |
|        - | 10173 | `	const char *zMsg;` |
|        - | 10174 | `	int nLen;` |
|      ! 0 | 10175 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 | 10176 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10177 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 | 10178 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 | 10179 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 | 10180 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 10181 | `			"Cannot throw into a closed generator");` |
|        - | 10182 | `	}` |
|        - | 10183 | `	/* Close the generator. Re-throw the exception properly via` |
|        - | 10184 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - | 10185 | `	 * exception dispatch path works correctly. Extract the message` |
|        - | 10186 | `	 * from the passed exception object if possible. */` |
|      ! 0 | 10187 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10188 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 | 10189 | `	nLen = 0;` |
|      ! 0 | 10190 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - | 10191 | `		/* Try to get the exception's message */` |
|        - | 10192 | `		SyString sAttr;` |
|        - | 10193 | `		ph7_value *pMsgAttr;` |
|      ! 0 | 10194 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 | 10195 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 | 10196 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 | 10197 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 | 10198 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 | 10199 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 | 10200 | `		}` |
|      ! 0 | 10201 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 10202 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 | 10203 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 | 10204 | `	}` |
|      ! 0 | 10205 | `	(void)nLen;` |
|      ! 0 | 10206 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 | 10207 |  |
|        - | 10208 | `/*` |
|        - | 10209 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - | 10210 | ` */` |
|        2 | 10211 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10212 |  |
|        - | 10213 | `	ph7_generator *pGen;` |
|        3 | 10214 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 10215 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 | 10216 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 10217 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10218 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 10219 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - | 10220 | `	}` |
|        3 | 10221 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 | 10222 | `	return PH7_OK;` |
|        2 | 10223 |  |
|        - | 10224 | `/*` |
|        - | 10225 | ` * Generator::__destruct() — clean up.` |
|        - | 10226 | ` */` |
|      ! 0 | 10227 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10228 |  |
|        - | 10229 | `	ph7_generator *pGen;` |
|      ! 0 | 10230 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 | 10231 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10232 | `	if( pGen ){` |
|      ! 0 | 10233 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 | 10234 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10235 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10236 | `			SyString sAttrName;` |
|        - | 10237 | `			ph7_value *pAttr;` |
|      ! 0 | 10238 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10239 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10240 | `			if( pAttr ){` |
|      ! 0 | 10241 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 | 10242 | `			}` |
|      ! 0 | 10243 | `		}` |
|      ! 0 | 10244 | `	}` |
|      ! 0 | 10245 | `	return PH7_OK;` |
|      ! 0 | 10246 |  |
|        - | 10247 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 10248 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 10249 | `/*` |
|        - | 10250 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 10251 | ` * the desired message.` |
|        - | 10252 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 10253 | ` * in 'api.c' for additional information.` |
|        - | 10254 | ` */` |
|      370 | 10255 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 10256 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 10257 | `	SyString *pString /* Message to output */` |
|        - | 10258 | `	)` |
|        2 | 10259 |  |
|      372 | 10260 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 | 10261 | `	sxi32 rc = SXRET_OK;` |
|        - | 10262 | `	/* Call the output consumer */` |
|      372 | 10263 | `	if( pString->nByte > 0 ){` |
|      372 | 10264 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 | 10265 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 | 10266 | `	}` |
|      372 | 10267 | `	return rc;` |
|        2 | 10268 |  |
|        - | 10269 | `/*` |
|        - | 10270 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 10271 | ` * callback to consume the formatted message.` |
|        - | 10272 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 10273 | ` * in 'api.c' for additional information.` |
|        - | 10274 | ` */` |
|        2 | 10275 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 10276 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 10277 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 10278 | `	va_list ap           /* Variable list of arguments */` |
|        - | 10279 | `	)` |
|        1 | 10280 |  |
|        3 | 10281 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 10282 | `	sxi32 rc = SXRET_OK;` |
|        - | 10283 | `	SyBlob sWorker;` |
|        - | 10284 | `	/* Format the message and call the output consumer */` |
|        3 | 10285 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 10286 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 10287 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 10288 | `		/* Consume the formatted message */` |
|        3 | 10289 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 10290 | `	}` |
|        3 | 10291 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 10292 | `	/* Release the working buffer */` |
|        3 | 10293 | `	SyBlobRelease(&sWorker);` |
|        3 | 10294 | `	return rc;` |
|        1 | 10295 |  |
|        - | 10296 | `/*` |
|        - | 10297 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 10298 | ` * This function never fail and always return a pointer` |
|        - | 10299 | ` * to a null terminated string.` |
|        - | 10300 | ` */` |
|       12 | 10301 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 10302 |  |
|       13 | 10303 | `	const char *zOp = "Unknown     ";` |
|       13 | 10304 | `	switch(nOp){` |
|        3 | 10305 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 10306 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 10307 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 10308 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 10309 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 10310 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 10311 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 10312 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 10313 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 10314 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 10315 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 10316 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 10317 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 10318 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 10319 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 10320 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 10321 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 10322 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 10323 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 10324 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 10325 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 10326 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 10327 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 10328 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 10329 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 10330 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 10331 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 10332 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 10333 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 10334 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 10335 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 10336 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 10337 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 10338 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 10339 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 10340 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 10341 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 10342 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 10343 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 10344 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 10345 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 10346 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 10347 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 10348 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 10349 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 10350 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 10351 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 10352 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 10353 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 10354 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 10355 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 10356 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 10357 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 10358 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 10359 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 10360 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 10361 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 10362 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 10363 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 10364 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 10365 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 10366 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 10367 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 10368 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 10369 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 10370 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 10371 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 10372 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 10373 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 | 10374 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 | 10375 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 10376 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 10377 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 10378 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 10379 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 10380 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 10381 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 10382 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 10383 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 10384 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 10385 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 10386 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 10387 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 10388 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 10389 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 10390 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 10391 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 10392 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 10393 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 10394 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 10395 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 10396 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 10397 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 10398 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 10399 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 10400 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 10401 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 10402 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 10403 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 10404 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 10405 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 10406 | `	default:` |
|      ! 0 | 10407 | `		break;` |
|        - | 10408 | `	}` |
|       13 | 10409 | `	return zOp;` |
|        1 | 10410 |  |
|        - | 10411 | `/*` |
|        - | 10412 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 10413 | ` * The xConsumer() callback which is an used defined function` |
|        - | 10414 | ` * is responsible of consuming the generated dump.` |
|        - | 10415 | ` */` |
|        2 | 10416 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 10417 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 10418 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 10419 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 10420 | `	)` |
|        1 | 10421 |  |
|        - | 10422 | `	sxi32 rc;` |
|        3 | 10423 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 10424 | `	return rc;` |
|        1 | 10425 |  |
|        - | 10426 | `/*` |
|        - | 10427 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 10428 | ` * outside a class body [i.e: global or function scope].` |
|        - | 10429 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 10430 | ` * in 'compile.c' for additional information.` |
|        - | 10431 | ` */` |
|       14 | 10432 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 10433 |  |
|       15 | 10434 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 10435 | `	/* Evaluate and expand constant value */` |
|       15 | 10436 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 | 10437 |  |
|        - | 10438 | `/*` |
|        - | 10439 | ` * Section:` |
|        - | 10440 | ` *  Function handling functions.` |
|        - | 10441 | ` * Status:` |
|        - | 10442 | ` *    Stable.` |
|        - | 10443 | ` */` |
|        - | 10444 | `/*` |
|        - | 10445 | ` * int func_num_args(void)` |
|        - | 10446 | ` *   Returns the number of arguments passed to the function.` |
|        - | 10447 | ` * Parameters` |
|        - | 10448 | ` *   None.` |
|        - | 10449 | ` * Return` |
|        - | 10450 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 10451 | ` *  or -1 if called from the globe scope.` |
|        - | 10452 | ` */` |
|      960 | 10453 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10454 |  |
|        - | 10455 | `	VmFrame *pFrame;` |
|        - | 10456 | `	ph7_vm *pVm;` |
|        - | 10457 | `	/* Point to the target VM */` |
|      962 | 10458 | `	pVm = pCtx->pVm;` |
|        - | 10459 | `	/* Current frame */` |
|      962 | 10460 | `	pFrame = pVm->pFrame;` |
|      962 | 10461 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      962 | 10462 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 10463 | `		SXUNUSED(nArg);` |
|      ! 0 | 10464 | `		SXUNUSED(apArg);` |
|        - | 10465 | `		/* Global frame,return -1 */` |
|      ! 0 | 10466 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 10467 | `		return SXRET_OK;` |
|        - | 10468 | `	}` |
|        - | 10469 | `	/* Total number of arguments passed to the enclosing function */` |
|      962 | 10470 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      962 | 10471 | `	ph7_result_int(pCtx,nArg);` |
|      962 | 10472 | `	return SXRET_OK;` |
|      482 | 10473 |  |
|        - | 10474 | `/*` |
|        - | 10475 | ` * value func_get_arg(int $arg_num)` |
|        - | 10476 | ` *   Return an item from the argument list.` |
|        - | 10477 | ` * Parameters` |
|        - | 10478 | ` *  Argument number(index start from zero).` |
|        - | 10479 | ` * Return` |
|        - | 10480 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 10481 | ` */` |
|       22 | 10482 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10483 |  |
|       24 | 10484 | `	ph7_value *pObj = 0;` |
|       24 | 10485 | `	VmSlot *pSlot = 0;` |
|        - | 10486 | `	VmFrame *pFrame;` |
|        - | 10487 | `	ph7_vm *pVm;` |
|        - | 10488 | `	/* Point to the target VM */` |
|       24 | 10489 | `	pVm = pCtx->pVm;` |
|        - | 10490 | `	/* Current frame */` |
|       24 | 10491 | `	pFrame = pVm->pFrame;` |
|       24 | 10492 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 10493 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 10494 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 10495 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 10496 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10497 | `		return SXRET_OK;` |
|        - | 10498 | `	}` |
|        - | 10499 | `	/* Extract the desired index */` |
|       21 | 10500 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 10501 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 10502 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 10503 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10504 | `		return SXRET_OK;` |
|        - | 10505 | `	}` |
|        - | 10506 | `	/* Extract the desired argument */` |
|       21 | 10507 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 10508 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 10509 | `			/* Return the desired argument */` |
|       21 | 10510 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 10511 | `		}else{` |
|        - | 10512 | `			/* No such argument,return false */` |
|      ! 0 | 10513 | `			ph7_result_bool(pCtx,0);` |
|        - | 10514 | `		}` |
|       11 | 10515 | `	}else{` |
|        - | 10516 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 10517 | `		ph7_result_bool(pCtx,0);` |
|        - | 10518 | `	}` |
|       21 | 10519 | `	return SXRET_OK;` |
|       13 | 10520 |  |
|        - | 10521 | `/*` |
|        - | 10522 | ` * array func_get_args_byref(void)` |
|        - | 10523 | ` *   Returns an array comprising a function's argument list.` |
|        - | 10524 | ` * Parameters` |
|        - | 10525 | ` *  None.` |
|        - | 10526 | ` * Return` |
|        - | 10527 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 10528 | ` *  member of the current user-defined function's argument list.` |
|        - | 10529 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 10530 | ` * NOTE:` |
|        - | 10531 | ` *  Arguments are returned to the array by reference.` |
|        - | 10532 | ` */` |
|        2 | 10533 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10534 |  |
|        - | 10535 | `	ph7_value *pArray;` |
|        - | 10536 | `	VmFrame *pFrame;` |
|        - | 10537 | `	VmSlot *aSlot;` |
|        - | 10538 | `	sxu32 n;` |
|        - | 10539 | `	/* Point to the current frame */` |
|        3 | 10540 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 10541 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 10542 | `	if( pFrame->pParent == 0 ){` |
|        - | 10543 | `		/* Global frame,return FALSE */` |
|      ! 0 | 10544 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 10545 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10546 | `		return SXRET_OK;` |
|        - | 10547 | `	}` |
|        - | 10548 | `	/* Create a new array */` |
|        3 | 10549 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10550 | `	if( pArray == 0 ){` |
|      ! 0 | 10551 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10552 | `		SXUNUSED(apArg);` |
|      ! 0 | 10553 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10554 | `		return SXRET_OK;` |
|        - | 10555 | `	}` |
|        - | 10556 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 10557 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 10558 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 10559 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 10560 | `	}` |
|        - | 10561 | `	/* Return the freshly created array */` |
|        3 | 10562 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10563 | `	return SXRET_OK;` |
|        2 | 10564 |  |
|        - | 10565 | `/*` |
|        - | 10566 | ` * array func_get_args(void)` |
|        - | 10567 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 10568 | ` * Parameters` |
|        - | 10569 | ` *  None.` |
|        - | 10570 | ` * Return` |
|        - | 10571 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 10572 | ` *  member of the current user-defined function's argument list.` |
|        - | 10573 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 10574 | ` */` |
|       88 | 10575 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10576 |  |
|       90 | 10577 | `	ph7_value *pObj = 0;` |
|        - | 10578 | `	ph7_value *pArray;` |
|        - | 10579 | `	VmFrame *pFrame;` |
|        - | 10580 | `	VmSlot *aSlot;` |
|        - | 10581 | `	sxu32 n;` |
|        - | 10582 | `	/* Point to the current frame */` |
|       90 | 10583 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 | 10584 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 | 10585 | `	if( pFrame->pParent == 0 ){` |
|        - | 10586 | `		/* Global frame,return FALSE */` |
|      ! 0 | 10587 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 10588 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10589 | `		return SXRET_OK;` |
|        - | 10590 | `	}` |
|        - | 10591 | `	/* Create a new array */` |
|       90 | 10592 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 | 10593 | `	if( pArray == 0 ){` |
|      ! 0 | 10594 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10595 | `		SXUNUSED(apArg);` |
|      ! 0 | 10596 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10597 | `		return SXRET_OK;` |
|        - | 10598 | `	}` |
|        - | 10599 | `	/* Start filling the array with the given arguments */` |
|       90 | 10600 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 | 10601 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 | 10602 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 | 10603 | `		if( pObj ){` |
|      134 | 10604 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 10605 | `		}` |
|       68 | 10606 | `	}` |
|        - | 10607 | `	/* Return the freshly created array */` |
|       90 | 10608 | `	ph7_result_value(pCtx,pArray);` |
|       90 | 10609 | `	return SXRET_OK;` |
|       46 | 10610 |  |
|        - | 10611 | `/*` |
|        - | 10612 | ` * bool function_exists(string $name)` |
|        - | 10613 | ` *  Return TRUE if the given function has been defined.` |
|        - | 10614 | ` * Parameters` |
|        - | 10615 | ` *  The name of the desired function.` |
|        - | 10616 | ` * Return` |
|        - | 10617 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 10618 | ` */` |
|     1714 | 10619 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10620 |  |
|        - | 10621 | `	const char *zName;` |
|        - | 10622 | `	ph7_vm *pVm;` |
|        - | 10623 | `	int nLen;` |
|        - | 10624 | `	int res;` |
|     1716 | 10625 | `	if( nArg < 1 ){` |
|        - | 10626 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 10627 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10628 | `		return SXRET_OK;` |
|        - | 10629 | `	}` |
|        - | 10630 | `	/* Point to the target VM */` |
|     1716 | 10631 | `	pVm = pCtx->pVm;` |
|        - | 10632 | `	/* Extract the function name */` |
|     1716 | 10633 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 10634 | `	/* Assume the function is not defined */` |
|     1716 | 10635 | `	res = 0;` |
|        - | 10636 | `	/* Perform the lookup */` |
|     2571 | 10637 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1710 | 10638 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 10639 | `			/* Function is defined */` |
|      238 | 10640 | `			res = 1;` |
|      118 | 10641 | `	}` |
|     1716 | 10642 | `	ph7_result_bool(pCtx,res);` |
|     1716 | 10643 | `	return SXRET_OK;` |
|      859 | 10644 |  |
|        - | 10645 | `/*` |
|        - | 10646 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 10647 | ` * [i.e: Whether it is callable or not].` |
|        - | 10648 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 10649 | ` */` |
|    22156 | 10650 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 | 10651 |  |
|    22158 | 10652 | `	int res = 0;` |
|    22158 | 10653 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 10654 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|        - | 10655 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|        - | 10656 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|        - | 10657 | `		 * standard PHP behavior. */` |
|       20 | 10658 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|       20 | 10659 | `		if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|       18 | 10660 | `			res = 1;` |
|       10 | 10661 | `		}` |
|        9 | 10662 | `		(void)CallInvoke;` |
|    22149 | 10663 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 | 10664 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 | 10665 | `		if( pMap->nEntry == 2 ){` |
|        - | 10666 | `			ph7_class *pClass;` |
|        - | 10667 | `			ph7_value *pV;` |
|        - | 10668 | `			/* Extract the target class */` |
|       12 | 10669 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 | 10670 | `			if( pV ){` |
|       12 | 10671 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 | 10672 | `				if( pClass ){` |
|        - | 10673 | `					ph7_class_method *pMethod;` |
|        - | 10674 | `					/* Extract the target method */` |
|       10 | 10675 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 10676 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 10677 | `						/* Perform the lookup */` |
|       10 | 10678 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 10679 | `						if( pMethod ){` |
|        - | 10680 | `							/* Method is callable */` |
|        5 | 10681 | `							res = 1;` |
|        2 | 10682 | `						}` |
|        4 | 10683 | `					}` |
|        4 | 10684 | `				}` |
|        5 | 10685 | `			}` |
|        7 | 10686 | `		}` |
|    22127 | 10687 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 10688 | `		const char *zName;` |
|        - | 10689 | `		int nLen;` |
|        - | 10690 | `		/* Extract the name */` |
|     5674 | 10691 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 10692 | `		/* Perform the lookup */` |
|     5689 | 10693 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 10694 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 10695 | `				/* Function is callable */` |
|     5656 | 10696 | `				res = 1;` |
|     2827 | 10697 | `		}` |
|     2836 | 10698 | `	}` |
|    22158 | 10699 | `	return res;` |
|        2 | 10700 |  |
|        - | 10701 | `/*` |
|        - | 10702 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 10703 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 10704 | ` * Parameters` |
|        - | 10705 | ` * $name` |
|        - | 10706 | ` *    The callback function to check` |
|        - | 10707 | ` * $syntax_only` |
|        - | 10708 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 10709 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 10710 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 10711 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 10712 | ` *    a string.` |
|        - | 10713 | ` * Return` |
|        - | 10714 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 10715 | ` */` |
|       20 | 10716 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10717 |  |
|        - | 10718 | `	ph7_vm *pVm;` |
|        - | 10719 | `	int res;` |
|       21 | 10720 | `	if( nArg < 1 ){` |
|        - | 10721 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 10722 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10723 | `		return SXRET_OK;` |
|        - | 10724 | `	}` |
|        - | 10725 | `	/* Point to the target VM */` |
|       21 | 10726 | `	pVm = pCtx->pVm;` |
|        - | 10727 | `	/* Perform the requested operation */` |
|       21 | 10728 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       21 | 10729 | `	ph7_result_bool(pCtx,res);` |
|       21 | 10730 | `	return SXRET_OK;` |
|       11 | 10731 |  |
|        - | 10732 | `/*` |
|        - | 10733 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 10734 | ` * defined below.` |
|        - | 10735 | ` */` |
|     1228 | 10736 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 10737 |  |
|     1229 | 10738 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 10739 | `	ph7_value sName;` |
|        - | 10740 | `	sxi32 rc;` |
|        - | 10741 | `	/* Prepare the function name for insertion */` |
|     1229 | 10742 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1229 | 10743 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 10744 | `	/* Perform the insertion */` |
|     1229 | 10745 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1229 | 10746 | `	PH7_MemObjRelease(&sName);` |
|     1229 | 10747 | `	return rc;` |
|        1 | 10748 |  |
|        - | 10749 | `/*` |
|        - | 10750 | ` * array get_defined_functions(void)` |
|        - | 10751 | ` *  Returns an array of all defined functions.` |
|        - | 10752 | ` * Parameter` |
|        - | 10753 | ` *  None.` |
|        - | 10754 | ` * Return` |
|        - | 10755 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 10756 | ` *  both built-in (internal) and user-defined.` |
|        - | 10757 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 10758 | ` *  defined ones using $arr["user"].` |
|        - | 10759 | ` * Note:` |
|        - | 10760 | ` *  NULL is returned on failure.` |
|        - | 10761 | ` */` |
|        2 | 10762 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10763 |  |
|        - | 10764 | `	ph7_value *pArray,*pEntry;` |
|        - | 10765 | `	/* NOTE:` |
|        - | 10766 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 10767 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 10768 | `	 */` |
|        3 | 10769 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10770 | ` 	if( pArray == 0 ){` |
|      ! 0 | 10771 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10772 | `		SXUNUSED(apArg);` |
|        - | 10773 | `		/* Return NULL */` |
|      ! 0 | 10774 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10775 | `		return SXRET_OK;` |
|        - | 10776 | `	}` |
|        3 | 10777 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 10778 | `	if( pEntry == 0 ){` |
|        - | 10779 | `		/* Return NULL */` |
|      ! 0 | 10780 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10781 | `		return SXRET_OK;` |
|        - | 10782 | `	}` |
|        - | 10783 | `	/* Fill with the appropriate information */` |
|        3 | 10784 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 10785 | `	/* Create the 'internal' index */` |
|        3 | 10786 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 10787 | `	/* Create the user-func array */` |
|        3 | 10788 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 10789 | `	if( pEntry == 0 ){` |
|        - | 10790 | `		/* Return NULL */` |
|      ! 0 | 10791 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10792 | `		return SXRET_OK;` |
|        - | 10793 | `	}` |
|        - | 10794 | `	/* Fill with the appropriate information */` |
|        3 | 10795 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 10796 | `	/* Create the 'user' index */` |
|        3 | 10797 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 10798 | `	/* Return the multi-dimensional array */` |
|        3 | 10799 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10800 | `	return SXRET_OK;` |
|        2 | 10801 |  |
|        - | 10802 | `/*` |
|        - | 10803 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 10804 | ` *  Register a function for execution on shutdown.` |
|        - | 10805 | ` * Note` |
|        - | 10806 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 10807 | ` *  be called in the same order as they were registered.` |
|        - | 10808 | ` * Parameters` |
|        - | 10809 | ` *  $callback` |
|        - | 10810 | ` *   The shutdown callback to register.` |
|        - | 10811 | ` * $param` |
|        - | 10812 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 10813 | ` * Return` |
|        - | 10814 | ` *  Nothing.` |
|        - | 10815 | ` */` |
|        2 | 10816 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10817 |  |
|        - | 10818 | `	VmShutdownCB sEntry;` |
|        - | 10819 | `	int i,j;` |
|        3 | 10820 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 10821 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 10822 | `		return PH7_OK;` |
|        - | 10823 | `	}` |
|        - | 10824 | `	/* Zero the Entry */` |
|        3 | 10825 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 10826 | `	/* Initialize fields */` |
|        3 | 10827 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 10828 | `	/* Save the callback name for later invocation name */` |
|        3 | 10829 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 | 10830 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 | 10831 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 | 10832 | `	}` |
|        - | 10833 | `	/* Copy arguments */` |
|        3 | 10834 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 10835 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 10836 | `			/* Limit reached */` |
|      ! 0 | 10837 | `			break;` |
|        - | 10838 | `		}` |
|      ! 0 | 10839 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 10840 | `	}` |
|        3 | 10841 | `	sEntry.nArg = j;` |
|        - | 10842 | `	/* Install the callback */` |
|        3 | 10843 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 | 10844 | `	return PH7_OK;` |
|        2 | 10845 |  |
|        - | 10846 | `/*` |
|        - | 10847 | ` * Section:` |
|        - | 10848 | ` *  Class handling functions.` |
|        - | 10849 | ` * Status:` |
|        - | 10850 | ` *    Stable.` |
|        - | 10851 | ` */` |
|        - | 10852 | `/*` |
|        - | 10853 | ` * Extract the top active class. NULL is returned` |
|        - | 10854 | ` * if the class stack is empty.` |
|        - | 10855 | ` */` |
|      904 | 10856 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 10857 |  |
|      906 | 10858 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 10859 | `	ph7_class **apClass;` |
|      906 | 10860 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 10861 | `		/* Empty stack,return NULL */` |
|       15 | 10862 | `		return 0;` |
|        - | 10863 | `	}` |
|        - | 10864 | `	/* Peek the last entry */` |
|      892 | 10865 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      892 | 10866 | `	return apClass[pSet->nUsed - 1];` |
|      454 | 10867 |  |
|        - | 10868 | `/*` |
|        - | 10869 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 10870 | ` *   Get the class that declared the currently executing method.` |
|        - | 10871 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 10872 | ` *` |
|        - | 10873 | ` * Parameters` |
|        - | 10874 | ` *   pVm: Target VM` |
|        - | 10875 | ` *` |
|        - | 10876 | ` * Return` |
|        - | 10877 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 10878 | ` *   - Not executing within a class method` |
|        - | 10879 | ` *` |
|        - | 10880 | ` * Note` |
|        - | 10881 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 10882 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 10883 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 10884 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 10885 | ` *   declaring class.` |
|        - | 10886 | ` */` |
|       98 | 10887 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 10888 |  |
|      100 | 10889 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10890 | `	ph7_vm_func *pVmFunc;` |
|        - | 10891 |  |
|        - | 10892 | `	/* Skip exception frames to find the actual method frame */` |
|      100 | 10893 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 10894 |  |
|        - | 10895 | `	/* Check if we're in a method context */` |
|      100 | 10896 | `	if( pFrame->pParent ){` |
|       96 | 10897 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       96 | 10898 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 10899 | `			/* Return the declaring class */` |
|       96 | 10900 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 10901 | `		}` |
|      ! 0 | 10902 | `	}` |
|        - | 10903 |  |
|        5 | 10904 | `	return 0;` |
|       51 | 10905 |  |
|        - | 10906 |  |
|        - | 10907 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 10908 | `/*` |
|        - | 10909 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 10910 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 10911 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 10912 | ` * return value indicates failure.` |
|        - | 10913 | ` */` |
|        - | 10914 | `/*` |
|        - | 10915 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 10916 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 10917 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 10918 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 10919 | ` */` |
|     2162 | 10920 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 10921 | `	ph7_vm *pVm,` |
|        - | 10922 | `	ph7_class_instance *pThis,` |
|        - | 10923 | `	ph7_class_method *pMethod,` |
|        - | 10924 | `	ph7_value *pResult,` |
|        - | 10925 | `	int nArg,` |
|        - | 10926 | `	ph7_value **apArg,` |
|        - | 10927 | `	VmCallArgMap *pMap` |
|        - | 10928 | `	)` |
|        2 | 10929 |  |
|        - | 10930 | `	ph7_value *aStack;` |
|        - | 10931 | `	VmInstr aInstr[2];` |
|        - | 10932 | `	int iCursor;` |
|        - | 10933 | `	int i;` |
|     2164 | 10934 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     2164 | 10935 | `	if( aStack == 0 ){` |
|      ! 0 | 10936 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 10937 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 10938 | `		return SXERR_MEM;` |
|        - | 10939 | `	}` |
|     3406 | 10940 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1244 | 10941 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     1244 | 10942 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      623 | 10943 | `	}` |
|     2164 | 10944 | `	iCursor = nArg + 1;` |
|     2164 | 10945 | `	if( pThis ){` |
|     2158 | 10946 | `		pThis->iRef++;` |
|     2158 | 10947 | `		aStack[i].x.pOther = pThis;` |
|     2158 | 10948 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|     1078 | 10949 | `	}` |
|     2164 | 10950 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2164 | 10951 | `	i++;` |
|     2164 | 10952 | `	SyBlobReset(&aStack[i].sBlob);` |
|     2164 | 10953 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     2164 | 10954 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     2164 | 10955 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2164 | 10956 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     2164 | 10957 | `	aInstr[0].iP1 = nArg;` |
|     2164 | 10958 | `	aInstr[0].iP2 = 0;` |
|     2164 | 10959 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     2164 | 10960 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     2164 | 10961 | `	aInstr[1].iP1 = 1;` |
|     2164 | 10962 | `	aInstr[1].iP2 = 0;` |
|     2164 | 10963 | `	aInstr[1].p3  = 0;` |
|     2164 | 10964 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0);` |
|     2164 | 10965 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     2164 | 10966 | `	return PH7_OK;` |
|     1083 | 10967 |  |
|     1700 | 10968 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 10969 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 10970 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 10971 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 10972 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 10973 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 10974 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 10975 | `	)` |
|        2 | 10976 |  |
|     1702 | 10977 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 10978 |  |
|        - | 10979 | `/*` |
|        - | 10980 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|        - | 10981 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|        - | 10982 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|        - | 10983 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|        - | 10984 | ` *` |
|        - | 10985 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|        - | 10986 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|        - | 10987 | ` * is_callable / closure-invoke paths follow the same rule.` |
|        - | 10988 | ` *` |
|        - | 10989 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|        - | 10990 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|        - | 10991 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|        - | 10992 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|        - | 10993 | ` *` |
|        - | 10994 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|        - | 10995 | ` */` |
|      166 | 10996 | `static sxi32 VmCallObjectInvoke(` |
|        - | 10997 | `	ph7_vm *pVm,` |
|        - | 10998 | `	ph7_class_instance *pThis,` |
|        - | 10999 | `	int nArg,` |
|        - | 11000 | `	ph7_value **apArg,` |
|        - | 11001 | `	ph7_value *pResult,` |
|        - | 11002 | `	VmCallArgMap *pMap` |
|        - | 11003 | `	)` |
|        2 | 11004 |  |
|        - | 11005 | `	ph7_class_method *pMethod;` |
|      168 | 11006 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      168 | 11007 | `	if( pMethod == 0 ){` |
|       13 | 11008 | `		if( pResult ){` |
|       13 | 11009 | `			PH7_MemObjRelease(pResult);` |
|        6 | 11010 | `		}` |
|       13 | 11011 | `		return SXERR_INVALID;` |
|        - | 11012 | `	}` |
|      156 | 11013 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       85 | 11014 |  |
|        - | 11015 | `/*` |
|        - | 11016 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|        - | 11017 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|        - | 11018 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|        - | 11019 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|        - | 11020 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|        - | 11021 | ` * lookup or 'goto Exception').` |
|        - | 11022 | ` *` |
|        - | 11023 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|        - | 11024 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|        - | 11025 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|        - | 11026 | ` * reported.` |
|        - | 11027 | ` */` |
|       12 | 11028 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        1 | 11029 |  |
|        - | 11030 | `	ph7_class *pErrorClass;` |
|       13 | 11031 | `	ph7_class_instance *pErrInst = 0;` |
|        - | 11032 | `	ph7_class_method *pCons;` |
|        - | 11033 | `	VmFrame *pThrowFrame;` |
|        - | 11034 | `	char zMsg[256];` |
|        - | 11035 | `	int nMsg;` |
|        - | 11036 | `	sxi32 rc;` |
|       25 | 11037 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - | 11038 | `		"Object of type %.*s is not callable",` |
|       12 | 11039 | `		(int)pThis->pClass->sName.nByte,` |
|       12 | 11040 | `		pThis->pClass->sName.zString);` |
|       13 | 11041 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|       13 | 11042 | `	if( pErrorClass ){` |
|       13 | 11043 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|        6 | 11044 | `	}` |
|       13 | 11045 | `	if( pErrInst == 0 ){` |
|        - | 11046 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|        - | 11047 | `		 * should always be available, so this branch is effectively unreachable.` |
|        - | 11048 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|        - | 11049 | `		 * visible to the user. */` |
|      ! 0 | 11050 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|      ! 0 | 11051 | `		return SXERR_ABORT;` |
|        - | 11052 | `	}` |
|       13 | 11053 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|       13 | 11054 | `	if( pCons ){` |
|        - | 11055 | `		ph7_value sArg;` |
|        - | 11056 | `		ph7_value *apMsg[1];` |
|        - | 11057 | `		SyString sMsgStr;` |
|       13 | 11058 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|       13 | 11059 | `		PH7_MemObjInit(pVm,&sArg);` |
|       13 | 11060 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|       13 | 11061 | `		apMsg[0] = &sArg;` |
|       13 | 11062 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|       13 | 11063 | `		PH7_MemObjRelease(&sArg);` |
|        6 | 11064 | `	}` |
|        - | 11065 | `	/* Else: Error::__construct is part of the built-in library and should` |
|        - | 11066 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|        - | 11067 | `	 * with an empty getMessage() rather than crashing. */` |
|       13 | 11068 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       13 | 11069 | `	if( pThrowFrame ){` |
|       13 | 11070 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 | 11071 | `	}` |
|       13 | 11072 | `	rc = VmThrowException(pVm,pErrInst);` |
|       13 | 11073 | `	PH7_ClassInstanceUnref(pErrInst);` |
|       13 | 11074 | `	return rc;` |
|        7 | 11075 |  |
|        - | 11076 | `/*` |
|        - | 11077 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 11078 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 11079 | ` * in the apArg[] array.` |
|        - | 11080 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 11081 | ` * return value indicates failure.` |
|        - | 11082 | ` */` |
|     1100 | 11083 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 11084 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 11085 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 11086 | `	int nArg,          /* Total number of given arguments */` |
|        - | 11087 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 11088 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 11089 | `	)` |
|        2 | 11090 |  |
|        - | 11091 | `	ph7_value *aStack;` |
|        - | 11092 | `	VmInstr aInstr[2];` |
|        - | 11093 | `	int i;` |
|     1102 | 11094 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|        - | 11095 | `		/* Object callable: dispatch through __invoke when available.` |
|        - | 11096 | `		 * No VmCallArgMap: call_user_func / array_map / usort and the C API` |
|        - | 11097 | `		 * pass arguments positionally and inherit no strict-types context. */` |
|      137 | 11098 | `		return VmCallObjectInvoke(&(*pVm),` |
|       90 | 11099 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|       45 | 11100 | `			nArg,apArg,pResult,0);` |
|        - | 11101 | `	}` |
|     1012 | 11102 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11103 | `		/* Don't bother processing,it's invalid anyway */` |
|      509 | 11104 | `		if( pResult ){` |
|        - | 11105 | `			/* Assume a null return value */` |
|      ! 0 | 11106 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 11107 | `		}` |
|      509 | 11108 | `		return SXERR_INVALID;` |
|        - | 11109 | `	}` |
|      504 | 11110 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 11111 | `		/* Class method */` |
|       11 | 11112 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 | 11113 | `		ph7_class_method *pMethod = 0;` |
|       11 | 11114 | `		ph7_class_instance *pThis = 0;` |
|       11 | 11115 | `		ph7_class *pClass = 0;` |
|        - | 11116 | `		ph7_value *pValue;` |
|        - | 11117 | `		sxi32 rc;` |
|       11 | 11118 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 11119 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 11120 | `			if( pResult ){` |
|        - | 11121 | `				/* Assume a null return value */` |
|      ! 0 | 11122 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11123 | `			}` |
|      ! 0 | 11124 | `			return SXRET_OK;` |
|        - | 11125 | `		}` |
|        - | 11126 | `		/* Extract the class name or an instance of it */` |
|       11 | 11127 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 | 11128 | `		if( pValue ){` |
|       11 | 11129 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 | 11130 | `		}` |
|       11 | 11131 | `		if( pClass == 0 ){` |
|        - | 11132 | `			/* No such class,return NULL */` |
|      ! 0 | 11133 | `			if( pResult ){` |
|      ! 0 | 11134 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11135 | `			}` |
|      ! 0 | 11136 | `			return SXRET_OK;` |
|        - | 11137 | `		}` |
|       11 | 11138 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11139 | `			/* Point to the class instance */` |
|        5 | 11140 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 | 11141 | `		}` |
|        - | 11142 | `		/* Try to extract the method */` |
|       11 | 11143 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 | 11144 | `		if( pValue ){` |
|       11 | 11145 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 | 11146 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 | 11147 | `					SyBlobLength(&pValue->sBlob));` |
|        5 | 11148 | `			}` |
|        5 | 11149 | `		}` |
|       11 | 11150 | `		if( pMethod == 0 ){` |
|        - | 11151 | `			/* No such method,return NULL */` |
|      ! 0 | 11152 | `			if( pResult ){` |
|      ! 0 | 11153 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11154 | `			}` |
|      ! 0 | 11155 | `			return SXRET_OK;` |
|        - | 11156 | `		}` |
|        - | 11157 | `		/* Call the class method */` |
|       11 | 11158 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 | 11159 | `		return rc;` |
|        - | 11160 | `	}` |
|        - | 11161 | `	/* Create a new operand stack */` |
|      494 | 11162 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      494 | 11163 | `	if( aStack == 0 ){` |
|      ! 0 | 11164 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 11165 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 11166 | `		if( pResult ){` |
|        - | 11167 | `			/* Assume a null return value */` |
|      ! 0 | 11168 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 11169 | `		}` |
|      ! 0 | 11170 | `		return SXERR_MEM;` |
|        - | 11171 | `	}` |
|        - | 11172 | `	/* Fill the operand stack with the given arguments */` |
|     1604 | 11173 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1112 | 11174 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 11175 | `		/*` |
|        - | 11176 | `		 * Symisc eXtension:` |
|        - | 11177 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 11178 | `		 */` |
|     1112 | 11179 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      557 | 11180 | `	}` |
|        - | 11181 | `	/* Push the function name */` |
|      494 | 11182 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      494 | 11183 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 11184 | `	/* Emit the CALL istruction */` |
|      494 | 11185 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      494 | 11186 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      494 | 11187 | `	aInstr[0].iP2 = 0;` |
|      494 | 11188 | `	aInstr[0].p3  = 0;` |
|        - | 11189 | `	/* Emit the DONE instruction */` |
|      494 | 11190 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      494 | 11191 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      494 | 11192 | `	aInstr[1].iP2 = 0;` |
|      494 | 11193 | `	aInstr[1].p3  = 0;` |
|        - | 11194 | `	/* Execute the function body (if available) */` |
|      494 | 11195 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0);` |
|        - | 11196 | `	/* Clean up the mess left behind */` |
|      494 | 11197 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      494 | 11198 | `	return PH7_OK;` |
|      552 | 11199 |  |
|        - | 11200 | `/*` |
|        - | 11201 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 11202 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 11203 | ` * parameter.` |
|        - | 11204 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 11205 | ` * return value indicates failure.` |
|        - | 11206 | ` */` |
|      236 | 11207 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 11208 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 11209 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 11210 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 11211 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 11212 | `	)` |
|        1 | 11213 |  |
|        - | 11214 | `	ph7_value *pArg;` |
|        - | 11215 | `	SySet aArg;` |
|        - | 11216 | `	va_list ap;` |
|        - | 11217 | `	sxi32 rc;` |
|      237 | 11218 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 11219 | `	/* Copy arguments one after one */` |
|      237 | 11220 | `	va_start(ap,pResult);` |
|      393 | 11221 | `	for(;;){` |
|      787 | 11222 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 | 11223 | `		if( pArg == 0 ){` |
|      237 | 11224 | `			break;` |
|        - | 11225 | `		}` |
|      551 | 11226 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 11227 | `	}` |
|        - | 11228 | `	/* Call the core routine */` |
|      237 | 11229 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 11230 | `	/* Cleanup */` |
|      237 | 11231 | `	SySetRelease(&aArg);` |
|      237 | 11232 | `	return rc;` |
|        1 | 11233 |  |
|        - | 11234 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 11235 | `/*` |
|        - | 11236 | ` * bool defined(string $name)` |
|        - | 11237 | ` *  Checks whether a given named constant exists.` |
|        - | 11238 | ` * Parameter:` |
|        - | 11239 | ` *  Name of the desired constant.` |
|        - | 11240 | ` * Return` |
|        - | 11241 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 11242 | ` */` |
|       16 | 11243 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11244 |  |
|        - | 11245 | `	const char *zName;` |
|       18 | 11246 | `	int nLen = 0;` |
|       18 | 11247 | `	int res = 0;` |
|       18 | 11248 | `	if( nArg < 1 ){` |
|        - | 11249 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 11250 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 11251 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11252 | `		return SXRET_OK;` |
|        - | 11253 | `	}` |
|        - | 11254 | `	/* Extract constant name */` |
|       18 | 11255 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11256 | `	/* Perform the lookup */` |
|       18 | 11257 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11258 | `		/* Already defined */` |
|       12 | 11259 | `		res = 1;` |
|        5 | 11260 | `	}` |
|       18 | 11261 | `	ph7_result_bool(pCtx,res);` |
|       18 | 11262 | `	return SXRET_OK;` |
|       10 | 11263 |  |
|        - | 11264 | `/*` |
|        - | 11265 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 11266 | ` * below.` |
|        - | 11267 | ` */` |
|       10 | 11268 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 11269 |  |
|       12 | 11270 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 11271 | `	/* Expand constant value */` |
|       12 | 11272 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 | 11273 |  |
|        - | 11274 | `/*` |
|        - | 11275 | ` * bool define(string $constant_name,expression value)` |
|        - | 11276 | ` *  Defines a named constant at runtime.` |
|        - | 11277 | ` * Parameter:` |
|        - | 11278 | ` *  $constant_name` |
|        - | 11279 | ` *   The name of the constant` |
|        - | 11280 | ` *  $value` |
|        - | 11281 | ` *   Constant value` |
|        - | 11282 | ` * Return:` |
|        - | 11283 | ` *   TRUE on success,FALSE on failure.` |
|        - | 11284 | ` */` |
|       12 | 11285 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11286 |  |
|        - | 11287 | `	const char *zName;  /* Constant name */` |
|        - | 11288 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 | 11289 | `	int nLen = 0;       /* Name length */` |
|        - | 11290 | `	sxi32 rc;` |
|       14 | 11291 | `	if( nArg < 2 ){` |
|        - | 11292 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 11293 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 11294 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11295 | `		return SXRET_OK;` |
|        - | 11296 | `	}` |
|       14 | 11297 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 11298 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 11299 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11300 | `		return SXRET_OK;` |
|        - | 11301 | `	}` |
|        - | 11302 | `	/* Extract constant name */` |
|       14 | 11303 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 | 11304 | `	if( nLen < 1 ){` |
|      ! 0 | 11305 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 11306 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11307 | `		return SXRET_OK;` |
|        - | 11308 | `	}` |
|        - | 11309 | `	/* Duplicate constant value */` |
|       14 | 11310 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 | 11311 | `	if( pValue == 0 ){` |
|      ! 0 | 11312 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 11313 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11314 | `		return SXRET_OK;` |
|        - | 11315 | `	}` |
|        - | 11316 | `	/* Initialize the memory object */` |
|       14 | 11317 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 11318 | `	/* Register the constant */` |
|       14 | 11319 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 | 11320 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11321 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 11322 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 11323 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11324 | `		return SXRET_OK;` |
|        - | 11325 | `	}` |
|        - | 11326 | `	/* Duplicate constant value */` |
|       14 | 11327 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 | 11328 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 11329 | `		/* Lower case the constant name */` |
|      ! 0 | 11330 | `		char *zCur = (char *)zName;` |
|      ! 0 | 11331 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 11332 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 11333 | `				/* UTF-8 stream */` |
|      ! 0 | 11334 | `				zCur++;` |
|      ! 0 | 11335 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 11336 | `					zCur++;` |
|      ! 0 | 11337 | `				}` |
|      ! 0 | 11338 | `				continue;` |
|        - | 11339 | `			}` |
|      ! 0 | 11340 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 11341 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 11342 | `				zCur[0] = (char)c;` |
|      ! 0 | 11343 | `			}` |
|      ! 0 | 11344 | `			zCur++;` |
|      ! 0 | 11345 | `		}` |
|        - | 11346 | `		/* Finally,register the constant */` |
|      ! 0 | 11347 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 | 11348 | `	}` |
|        - | 11349 | `	/* All done,return TRUE */` |
|       14 | 11350 | `	ph7_result_bool(pCtx,1);` |
|       14 | 11351 | `	return SXRET_OK;` |
|        8 | 11352 |  |
|        - | 11353 | `/*` |
|        - | 11354 | ` * value constant(string $name)` |
|        - | 11355 | ` *  Returns the value of a constant` |
|        - | 11356 | ` * Parameter` |
|        - | 11357 | ` *  $name` |
|        - | 11358 | ` *    Name of the constant.` |
|        - | 11359 | ` * Return` |
|        - | 11360 | ` *  Constant value or NULL if not defined.` |
|        - | 11361 | ` */` |
|        8 | 11362 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11363 |  |
|        - | 11364 | `	SyHashEntry *pEntry;` |
|        - | 11365 | `	ph7_constant *pCons;` |
|        - | 11366 | `	const char *zName; /* Constant name */` |
|        - | 11367 | `	ph7_value sVal;    /* Constant value */` |
|        - | 11368 | `	int nLen;` |
|       10 | 11369 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11370 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 11371 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 11372 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11373 | `		return SXRET_OK;` |
|        - | 11374 | `	}` |
|        - | 11375 | `	/* Extract the constant name */` |
|       10 | 11376 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11377 | `	/* Perform the query */` |
|       10 | 11378 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 11379 | `	if( pEntry == 0 ){` |
|        3 | 11380 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 11381 | `		ph7_result_null(pCtx);` |
|        3 | 11382 | `		return SXRET_OK;` |
|        - | 11383 | `	}` |
|        8 | 11384 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 11385 | `	/* Point to the structure that describe the constant */` |
|        8 | 11386 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 11387 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 11388 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 11389 | `	/* Return that value */` |
|        8 | 11390 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 11391 | `	/* Cleanup */` |
|        8 | 11392 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 11393 | `	return SXRET_OK;` |
|        6 | 11394 |  |
|        - | 11395 | `/*` |
|        - | 11396 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 11397 | ` * defined below.` |
|        - | 11398 | ` */` |
|      452 | 11399 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11400 |  |
|      453 | 11401 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11402 | `	ph7_value sName;` |
|        - | 11403 | `	sxi32 rc;` |
|        - | 11404 | `	/* Prepare the constant name for insertion */` |
|      453 | 11405 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      453 | 11406 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11407 | `	/* Perform the insertion */` |
|      453 | 11408 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      453 | 11409 | `	PH7_MemObjRelease(&sName);` |
|      453 | 11410 | `	return rc;` |
|        1 | 11411 |  |
|        - | 11412 | `/*` |
|        - | 11413 | ` * array get_defined_constants(void)` |
|        - | 11414 | ` *  Returns an associative array with the names of all defined` |
|        - | 11415 | ` *  constants.` |
|        - | 11416 | ` * Parameters` |
|        - | 11417 | ` *  NONE.` |
|        - | 11418 | ` * Returns` |
|        - | 11419 | ` *  Returns the names of all the constants currently defined.` |
|        - | 11420 | ` */` |
|        2 | 11421 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11422 |  |
|        - | 11423 | `	ph7_value *pArray;` |
|        - | 11424 | `	/* Create the array first*/` |
|        3 | 11425 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11426 | `	if( pArray == 0 ){` |
|      ! 0 | 11427 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11428 | `		SXUNUSED(apArg);` |
|        - | 11429 | `		/* Return NULL */` |
|      ! 0 | 11430 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11431 | `		return SXRET_OK;` |
|        - | 11432 | `	}` |
|        - | 11433 | `	/* Fill the array with the defined constants */` |
|        3 | 11434 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 11435 | `	/* Return the created array */` |
|        3 | 11436 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11437 | `	return SXRET_OK;` |
|        2 | 11438 |  |
|        - | 11439 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 11440 | `/*` |
|        - | 11441 | ` * Section:` |
|        - | 11442 | ` *  Random numbers/string generators.` |
|        - | 11443 | ` * Status:` |
|        - | 11444 | ` *    Stable.` |
|        - | 11445 | ` */` |
|        - | 11446 | `/*` |
|        - | 11447 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 11448 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - | 11449 | ` * used by te SQLite3 library.` |
|        - | 11450 | ` */` |
|     2763 | 11451 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 11452 |  |
|        - | 11453 | `	sxu32 iNum;` |
|     2765 | 11454 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2765 | 11455 | `	return iNum;` |
|        2 | 11456 |  |
|        - | 11457 | `/*` |
|        - | 11458 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 11459 | ` * Note that the generated string is NOT null terminated.` |
|        - | 11460 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - | 11461 | ` * by te SQLite3 library.` |
|        - | 11462 | ` */` |
|   196152 | 11463 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 11464 |  |
|        - | 11465 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 11466 | `	int i;` |
|        - | 11467 | `	/* Generate a binary string first */` |
|   196154 | 11468 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 11469 | `	/* Turn the binary string into english based alphabet */` |
|  2157842 | 11470 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1961690 | 11471 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   980846 | 11472 | `	 }` |
|   196154 | 11473 |  |
|        - | 11474 | `/*` |
|        - | 11475 | ` * int rand()` |
|        - | 11476 | ` * int mt_rand()` |
|        - | 11477 | ` * int rand(int $min,int $max)` |
|        - | 11478 | ` * int mt_rand(int $min,int $max)` |
|        - | 11479 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 11480 | ` * Parameter` |
|        - | 11481 | ` *  $min` |
|        - | 11482 | ` *    The lowest value to return (default: 0)` |
|        - | 11483 | ` *  $max` |
|        - | 11484 | ` *   The highest value to return (default: getrandmax())` |
|        - | 11485 | ` * Return` |
|        - | 11486 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 11487 | ` * Note:` |
|        - | 11488 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11489 | ` *  by te SQLite3 library.` |
|        - | 11490 | ` */` |
|       20 | 11491 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11492 |  |
|        - | 11493 | `	sxu32 iNum;` |
|        - | 11494 | `	/* Generate the random number */` |
|       21 | 11495 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 11496 | `	if( nArg > 1 ){` |
|        - | 11497 | `		sxu32 iMin,iMax;` |
|        3 | 11498 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 11499 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 11500 | `		if( iMin < iMax ){` |
|        3 | 11501 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 11502 | `			if( iDiv > 0 ){` |
|        3 | 11503 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 11504 | `			}` |
|        1 | 11505 | `		}else if(iMax > 0 ){` |
|      ! 0 | 11506 | `			iNum %= iMax;` |
|      ! 0 | 11507 | `		}` |
|        1 | 11508 | `	}` |
|        - | 11509 | `	/* Return the number */` |
|       21 | 11510 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 11511 | `	return SXRET_OK;` |
|        1 | 11512 |  |
|        - | 11513 | `/*` |
|        - | 11514 | ` * int getrandmax(void)` |
|        - | 11515 | ` * int mt_getrandmax(void)` |
|        - | 11516 | ` * int rc4_getrandmax(void)` |
|        - | 11517 | ` *   Show largest possible random value` |
|        - | 11518 | ` * Return` |
|        - | 11519 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 11520 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 11521 | ` * Note:` |
|        - | 11522 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11523 | ` *  by te SQLite3 library.` |
|        - | 11524 | ` */` |
|        4 | 11525 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11526 |  |
|        2 | 11527 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 11528 | `	SXUNUSED(apArg);` |
|        5 | 11529 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 11530 | `	return SXRET_OK;` |
|        1 | 11531 |  |
|        - | 11532 | `/*` |
|        - | 11533 | ` * string rand_str()` |
|        - | 11534 | ` * string rand_str(int $len)` |
|        - | 11535 | ` *  Generate a random string (English alphabet).` |
|        - | 11536 | ` * Parameter` |
|        - | 11537 | ` *  $len` |
|        - | 11538 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 11539 | ` * Return` |
|        - | 11540 | ` *   A pseudo random string.` |
|        - | 11541 | ` * Note:` |
|        - | 11542 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11543 | ` *  by te SQLite3 library.` |
|        - | 11544 | ` *  This function is a symisc extension.` |
|        - | 11545 | ` */` |
|      120 | 11546 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11547 |  |
|        - | 11548 | `	char zString[1024];` |
|      122 | 11549 | `	int iLen = 0x10;` |
|      122 | 11550 | `	if( nArg > 0 ){` |
|        - | 11551 | `		/* Get the desired length */` |
|      122 | 11552 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 11553 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 11554 | `			/* Default length */` |
|        3 | 11555 | `			iLen = 0x10;` |
|        1 | 11556 | `		}` |
|       60 | 11557 | `	}` |
|        - | 11558 | `	/* Generate the random string */` |
|      122 | 11559 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 11560 | `	/* Return the generated string */` |
|      122 | 11561 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 11562 | `	return SXRET_OK;` |
|        2 | 11563 |  |
|        - | 11564 | `/*` |
|        - | 11565 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - | 11566 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - | 11567 | ` * an int (PHP coerces float and numeric string silently).` |
|        - | 11568 | ` */` |
|      488 | 11569 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 | 11570 |  |
|      488 | 11571 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      483 | 11572 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       10 | 11573 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11574 | `			"TypeError",` |
|        - | 11575 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|        3 | 11576 | `			zFunc,iArgPos,zParamName,` |
|        3 | 11577 | `			ph7_type_name(pArg)` |
|        - | 11578 | `			);` |
|        - | 11579 | `	}` |
|      483 | 11580 | `	if( ph7_value_is_string(pArg) ){` |
|        - | 11581 | `		int len;` |
|        9 | 11582 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 | 11583 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 | 11584 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11585 | `				"TypeError",` |
|        - | 11586 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 | 11587 | `				zFunc,iArgPos,zParamName` |
|        - | 11588 | `				);` |
|        - | 11589 | `		}` |
|        2 | 11590 | `	}` |
|      479 | 11591 | `	return SXRET_OK;` |
|      245 | 11592 |  |
|        - | 11593 | `/*` |
|        - | 11594 | ` * int random_int(int $min, int $max)` |
|        - | 11595 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - | 11596 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - | 11597 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - | 11598 | ` *  power-of-two mask covering the range.` |
|        - | 11599 | ` */` |
|      242 | 11600 | `static int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11601 |  |
|        - | 11602 | `	sxi64 iMin,iMax;` |
|        - | 11603 | `	sxu64 uRange,uMask,uResult;` |
|        - | 11604 | `	unsigned int nAttempt;` |
|        - | 11605 | `	int rc;` |
|      243 | 11606 | `	if( nArg != 2 ){` |
|       10 | 11607 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11608 | `			"ArgumentCountError",` |
|        - | 11609 | `			"random_int() expects exactly 2 arguments, %d given",` |
|        3 | 11610 | `			nArg` |
|        - | 11611 | `			);` |
|        - | 11612 | `	}` |
|      237 | 11613 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      237 | 11614 | `	if( rc != SXRET_OK ){ return rc; }` |
|      233 | 11615 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      233 | 11616 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 11617 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 | 11618 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 | 11619 | `	if( iMin > iMax ){` |
|        3 | 11620 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11621 | `			"ValueError",` |
|        - | 11622 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - | 11623 | `			);` |
|        - | 11624 | `	}` |
|      229 | 11625 | `	if( iMin == iMax ){` |
|        5 | 11626 | `		ph7_result_int64(pCtx,iMin);` |
|        5 | 11627 | `		return SXRET_OK;` |
|        - | 11628 | `	}` |
|      225 | 11629 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 | 11630 | `	uMask = uRange;` |
|      225 | 11631 | `	uMask \|= uMask >> 1;` |
|      225 | 11632 | `	uMask \|= uMask >> 2;` |
|      225 | 11633 | `	uMask \|= uMask >> 4;` |
|      225 | 11634 | `	uMask \|= uMask >> 8;` |
|      225 | 11635 | `	uMask \|= uMask >> 16;` |
|      225 | 11636 | `	uMask \|= uMask >> 32;` |
|      225 | 11637 | `	uResult = 0;` |
|      366 | 11638 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 11639 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 11640 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 11641 | `		 * and the low-half mask would always read 0). */` |
|        - | 11642 | `		sxu64 uDraw;` |
|      366 | 11643 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|        - | 11644 | `			/* PHP 8.2+ would throw Random\RandomException here; that class` |
|        - | 11645 | `			 * is not yet registered in PHL (see PLAN.md item 6.13). */` |
|      ! 0 | 11646 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11647 | `				"Exception",` |
|        - | 11648 | `				"Cannot gather sufficient random data"` |
|        - | 11649 | `				);` |
|        - | 11650 | `		}` |
|      366 | 11651 | `		uDraw &= uMask;` |
|      366 | 11652 | `		if( uDraw <= uRange ){` |
|      225 | 11653 | `			uResult = uDraw;` |
|      225 | 11654 | `			break;` |
|        - | 11655 | `		}` |
|       66 | 11656 | `	}` |
|      225 | 11657 | `	if( nAttempt >= 50 ){` |
|      ! 0 | 11658 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11659 | `			"Exception",` |
|        - | 11660 | `			"Cannot gather sufficient random data"` |
|        - | 11661 | `			);` |
|        - | 11662 | `	}` |
|      225 | 11663 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 | 11664 | `	return SXRET_OK;` |
|      122 | 11665 |  |
|        - | 11666 | `/*` |
|        - | 11667 | ` * string random_bytes(int $length)` |
|        - | 11668 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - | 11669 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - | 11670 | ` */` |
|       24 | 11671 | `static int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11672 |  |
|        - | 11673 | `	sxi64 iLen;` |
|        - | 11674 | `	unsigned char zStack[256];` |
|        - | 11675 | `	void *pBuf;` |
|        - | 11676 | `	int rc;` |
|       25 | 11677 | `	int bHeap = 0;` |
|       25 | 11678 | `	if( nArg != 1 ){` |
|        7 | 11679 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11680 | `			"ArgumentCountError",` |
|        - | 11681 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|        2 | 11682 | `			nArg` |
|        - | 11683 | `			);` |
|        - | 11684 | `	}` |
|       21 | 11685 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       21 | 11686 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 | 11687 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 | 11688 | `	if( iLen < 1 ){` |
|        5 | 11689 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11690 | `			"ValueError",` |
|        - | 11691 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - | 11692 | `			);` |
|        - | 11693 | `	}` |
|        - | 11694 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - | 11695 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - | 11696 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 | 11697 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 | 11698 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11699 | `			"ValueError",` |
|        - | 11700 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - | 11701 | `			);` |
|        - | 11702 | `	}` |
|       13 | 11703 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 | 11704 | `		pBuf = zStack;` |
|        7 | 11705 | `	}else{` |
|      ! 0 | 11706 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 | 11707 | `		if( pBuf == 0 ){` |
|      ! 0 | 11708 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11709 | `				"Exception",` |
|        - | 11710 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 | 11711 | `				iLen` |
|        - | 11712 | `				);` |
|        - | 11713 | `		}` |
|      ! 0 | 11714 | `		bHeap = 1;` |
|        - | 11715 | `	}` |
|       13 | 11716 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 11717 | `		if( bHeap ){` |
|      ! 0 | 11718 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 11719 | `		}` |
|      ! 0 | 11720 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11721 | `			"Exception",` |
|        - | 11722 | `			"Cannot gather sufficient random data"` |
|        - | 11723 | `			);` |
|        - | 11724 | `	}` |
|       13 | 11725 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 11726 | `	if( bHeap ){` |
|      ! 0 | 11727 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 11728 | `	}` |
|       13 | 11729 | `	return SXRET_OK;` |
|       13 | 11730 |  |
|        - | 11731 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11732 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 11733 | `/* Unique ID private data */` |
|        - | 11734 | `struct unique_id_data` |
|        - | 11735 |  |
|        - | 11736 | `	ph7_context *pCtx; /* Call context */` |
|        - | 11737 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 11738 | `};` |
|        - | 11739 | `/*` |
|        - | 11740 | ` * Binary to hex consumer callback.` |
|        - | 11741 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 11742 | ` * defined below.` |
|        - | 11743 | ` */` |
|      192 | 11744 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 11745 |  |
|      193 | 11746 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 11747 | `	sxu32 nBuflen;` |
|        - | 11748 | `	/* Extract result buffer length */` |
|      193 | 11749 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 11750 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 11751 | `			/*` |
|        - | 11752 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 11753 | `			 * string will be 13 characters long` |
|        - | 11754 | `			 */` |
|       25 | 11755 | `		return SXERR_ABORT;` |
|        - | 11756 | `	}` |
|      169 | 11757 | `	if( nBuflen > 22 ){` |
|      ! 0 | 11758 | `		return SXERR_ABORT;` |
|        - | 11759 | `	}` |
|        - | 11760 | `	/* Safely Consume the hex stream */` |
|      169 | 11761 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 11762 | `	return SXRET_OK;` |
|       97 | 11763 |  |
|        - | 11764 | `/*` |
|        - | 11765 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 11766 | ` *  Generate a unique ID` |
|        - | 11767 | ` * Parameter` |
|        - | 11768 | ` * $prefix` |
|        - | 11769 | ` *  Append this prefix to the generated unique ID.` |
|        - | 11770 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 11771 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 11772 | ` * $more_entropy` |
|        - | 11773 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 11774 | ` *  that the result will be unique.` |
|        - | 11775 | ` * Return` |
|        - | 11776 | ` *  Returns the unique identifier, as a string.` |
|        - | 11777 | ` */` |
|       24 | 11778 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11779 |  |
|        - | 11780 | `	struct unique_id_data sUniq;` |
|        - | 11781 | `	unsigned char zDigest[20];` |
|       25 | 11782 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11783 | `	const char *zPrefix;` |
|        - | 11784 | `	SHA1Context sCtx;` |
|        - | 11785 | `	char zRandom[7];` |
|        - | 11786 | `	int nPrefix;` |
|        - | 11787 | `	int entropy;` |
|        - | 11788 | `	/* Generate a random string first */` |
|       25 | 11789 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 11790 | `	/* Initialize fields */` |
|       25 | 11791 | `	zPrefix = 0;` |
|       25 | 11792 | `	nPrefix = 0;` |
|       25 | 11793 | `	entropy = 0;` |
|       25 | 11794 | `	if( nArg > 0 ){` |
|        - | 11795 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 11796 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 11797 | `		if( nArg > 1 ){` |
|      ! 0 | 11798 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 11799 | `		}` |
|      ! 0 | 11800 | `	}` |
|       25 | 11801 | `	SHA1Init(&sCtx);` |
|        - | 11802 | `	/* Generate the random ID */` |
|       25 | 11803 | `	if( nPrefix > 0 ){` |
|      ! 0 | 11804 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 11805 | `	}` |
|        - | 11806 | `	/* Append the random ID */` |
|       25 | 11807 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 11808 | `	/* Append the random string */` |
|       25 | 11809 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 11810 | `	/* Increment the number */` |
|       25 | 11811 | `	pVm->unique_id++;` |
|       25 | 11812 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 11813 | `	/* Hexify the digest */` |
|       25 | 11814 | `	sUniq.pCtx = pCtx;` |
|       25 | 11815 | `	sUniq.entropy = entropy;` |
|       25 | 11816 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 11817 | `	/* All done */` |
|       25 | 11818 | `	return PH7_OK;` |
|        1 | 11819 |  |
|        - | 11820 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 11821 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 11822 | `/*` |
|        - | 11823 | ` * Section:` |
|        - | 11824 | ` *  Language construct implementation as foreign functions.` |
|        - | 11825 | ` * Status:` |
|        - | 11826 | ` *    Stable.` |
|        - | 11827 | ` */` |
|        - | 11828 | `/*` |
|        - | 11829 | ` * void echo($string...)` |
|        - | 11830 | ` *  Output one or more messages.` |
|        - | 11831 | ` * Parameters` |
|        - | 11832 | ` *  $string` |
|        - | 11833 | ` *   Message to output.` |
|        - | 11834 | ` * Return` |
|        - | 11835 | ` *  NULL.` |
|        - | 11836 | ` */` |
|      ! 0 | 11837 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 11838 |  |
|        - | 11839 | `	const char *zData;` |
|      ! 0 | 11840 | `	int nDataLen = 0;` |
|        - | 11841 | `	ph7_vm *pVm;` |
|        - | 11842 | `	int i,rc;` |
|        - | 11843 | `	/* Point to the target VM */` |
|      ! 0 | 11844 | `	pVm = pCtx->pVm;` |
|        - | 11845 | `	/* Output */` |
|      ! 0 | 11846 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 11847 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 11848 | `		if( nDataLen > 0 ){` |
|      ! 0 | 11849 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 11850 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 11851 | `			if( rc == SXERR_ABORT ){` |
|        - | 11852 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 11853 | `				return PH7_ABORT;` |
|        - | 11854 | `			}` |
|      ! 0 | 11855 | `		}` |
|      ! 0 | 11856 | `	}` |
|      ! 0 | 11857 | `	return SXRET_OK;` |
|      ! 0 | 11858 |  |
|        - | 11859 | `/*` |
|        - | 11860 | ` * int print($string...)` |
|        - | 11861 | ` *  Output one or more messages.` |
|        - | 11862 | ` * Parameters` |
|        - | 11863 | ` *  $string` |
|        - | 11864 | ` *   Message to output.` |
|        - | 11865 | ` * Return` |
|        - | 11866 | ` *  1 always.` |
|        - | 11867 | ` */` |
|        2 | 11868 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11869 |  |
|        - | 11870 | `	const char *zData;` |
|        3 | 11871 | `	int nDataLen = 0;` |
|        - | 11872 | `	ph7_vm *pVm;` |
|        - | 11873 | `	int i,rc;` |
|        - | 11874 | `	/* Point to the target VM */` |
|        3 | 11875 | `	pVm = pCtx->pVm;` |
|        - | 11876 | `	/* Output */` |
|        5 | 11877 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 11878 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 11879 | `		if( nDataLen > 0 ){` |
|        3 | 11880 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 11881 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 11882 | `			if( rc == SXERR_ABORT ){` |
|        - | 11883 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 11884 | `				return PH7_ABORT;` |
|        - | 11885 | `			}` |
|        1 | 11886 | `		}` |
|        2 | 11887 | `	}` |
|        - | 11888 | `	/* Return 1 */` |
|        3 | 11889 | `	ph7_result_int(pCtx,1);` |
|        3 | 11890 | `	return SXRET_OK;` |
|        2 | 11891 |  |
|        - | 11892 | `/*` |
|        - | 11893 | ` * void exit(string $msg)` |
|        - | 11894 | ` * void exit(int $status)` |
|        - | 11895 | ` * void die(string $ms)` |
|        - | 11896 | ` * void die(int $status)` |
|        - | 11897 | ` *   Output a message and terminate program execution.` |
|        - | 11898 | ` * Parameter` |
|        - | 11899 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 11900 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 11901 | ` *  and not printed` |
|        - | 11902 | ` * Return` |
|        - | 11903 | ` *  NULL` |
|        - | 11904 | ` */` |
|      ! 0 | 11905 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 11906 |  |
|      ! 0 | 11907 | `	if( nArg > 0 ){` |
|      ! 0 | 11908 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 11909 | `			const char *zData;` |
|      ! 0 | 11910 | `			int iLen = 0;` |
|        - | 11911 | `			/* Print exit message */` |
|      ! 0 | 11912 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 11913 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 11914 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 11915 | `			sxi32 iExitStatus;` |
|        - | 11916 | `			/* Record exit status code */` |
|      ! 0 | 11917 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 11918 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 11919 | `		}` |
|      ! 0 | 11920 | `	}` |
|        - | 11921 | `	/* Check if we are in an included file */` |
|      ! 0 | 11922 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - | 11923 | `		/* Exit the entire process */` |
|      ! 0 | 11924 | `		exit(pCtx->pVm->iExitStatus);` |
|        - | 11925 | `	}` |
|        - | 11926 | `	/* Abort processing immediately */` |
|      ! 0 | 11927 | `	return PH7_ABORT;` |
|      ! 0 | 11928 |  |
|        - | 11929 | `/*` |
|        - | 11930 | ` * bool isset($var,...)` |
|        - | 11931 | ` *  Finds out whether a variable is set.` |
|        - | 11932 | ` * Parameters` |
|        - | 11933 | ` *  One or more variable to check.` |
|        - | 11934 | ` * Return` |
|        - | 11935 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 11936 | ` */` |
|    89550 | 11937 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11938 |  |
|        - | 11939 | `	ph7_value *pObj;` |
|    89552 | 11940 | `	int res = 0;` |
|        - | 11941 | `	int i;` |
|    89552 | 11942 | `	if( nArg < 1 ){` |
|        - | 11943 | `		/* Missing arguments,return false */` |
|      ! 0 | 11944 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 11945 | `		return SXRET_OK;` |
|        - | 11946 | `	}` |
|        - | 11947 | `	/* Iterate over available arguments */` |
|   117146 | 11948 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    89552 | 11949 | `		pObj = apArg[i];` |
|    89552 | 11950 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    61072 | 11951 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 11952 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 11953 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 11954 | `			}` |
|    30535 | 11955 | `		}` |
|    89552 | 11956 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    89552 | 11957 | `		if( !res ){` |
|        - | 11958 | `			/* Variable not set,return FALSE */` |
|    61958 | 11959 | `			ph7_result_bool(pCtx,0);` |
|    61958 | 11960 | `			return SXRET_OK;` |
|        - | 11961 | `		}` |
|    13799 | 11962 | `	}` |
|        - | 11963 | `	/* All given variable are set,return TRUE */` |
|    27596 | 11964 | `	ph7_result_bool(pCtx,1);` |
|    27596 | 11965 | `	return SXRET_OK;` |
|    44777 | 11966 |  |
|        - | 11967 | `/*` |
|        - | 11968 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 11969 | ` * frame,the reference table and discard it's contents.` |
|        - | 11970 | ` * This function never fail and always return SXRET_OK.` |
|        - | 11971 | ` */` |
|  3122740 | 11972 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 11973 |  |
|        - | 11974 | `	ph7_value *pObj;` |
|        - | 11975 | `	VmRefObj *pRef;` |
|  3122742 | 11976 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3122742 | 11977 | `	if( pObj ){` |
|        - | 11978 | `		/* Release the object */` |
|  3122742 | 11979 | `		PH7_MemObjRelease(pObj);` |
|  1561370 | 11980 | `	}` |
|        - | 11981 | `	/* Remove old reference links */` |
|  3122742 | 11982 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3122742 | 11983 | `	if( pRef ){` |
|  3122736 | 11984 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 11985 | `		/* Unlink from the reference table */` |
|  3122736 | 11986 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3122736 | 11987 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 11988 | `			VmSlot sFree;` |
|        - | 11989 | `			/* Restore to the free list */` |
|  3122728 | 11990 | `			sFree.nIdx = nObjIdx;` |
|  3122728 | 11991 | `			sFree.pUserData = 0;` |
|  3122728 | 11992 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1561363 | 11993 | `		}` |
|  1561367 | 11994 | `	}` |
|  3122742 | 11995 | `	return SXRET_OK;` |
|        2 | 11996 |  |
|        - | 11997 | `/*` |
|        - | 11998 | ` * void unset($var,...)` |
|        - | 11999 | ` *   Unset one or more given variable.` |
|        - | 12000 | ` * Parameters` |
|        - | 12001 | ` *  One or more variable to unset.` |
|        - | 12002 | ` * Return` |
|        - | 12003 | ` *  Nothing.` |
|        - | 12004 | ` */` |
|     7418 | 12005 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12006 |  |
|        - | 12007 | `	ph7_value *pObj;` |
|        - | 12008 | `	ph7_vm *pVm;` |
|        - | 12009 | `	int i;` |
|        - | 12010 | `	/* Point to the target VM */` |
|     7420 | 12011 | `	pVm = pCtx->pVm;` |
|        - | 12012 | `	/* Iterate and unset */` |
|    14838 | 12013 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7420 | 12014 | `		pObj = apArg[i];` |
|     7420 | 12015 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 | 12016 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 12017 | `				/* Throw an error */` |
|      ! 0 | 12018 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 12019 | `			}` |
|      ! 0 | 12020 | `		}else{` |
|     7420 | 12021 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 12022 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     7420 | 12023 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     7414 | 12024 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3706 | 12025 | `			}` |
|        - | 12026 | `		}` |
|     3711 | 12027 | `	}` |
|     7420 | 12028 | `	return SXRET_OK;` |
|        2 | 12029 |  |
|        - | 12030 | `/*` |
|        - | 12031 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 12032 | ` */` |
|      110 | 12033 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12034 |  |
|      111 | 12035 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 | 12036 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 12037 | `	ph7_value *pObj;` |
|        - | 12038 | `	sxu32 nIdx;` |
|        - | 12039 | `	/* Extract the memory object */` |
|      111 | 12040 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 | 12041 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 | 12042 | `	if( pObj ){` |
|      111 | 12043 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 | 12044 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 12045 | `				SyString sName;` |
|        - | 12046 | `				ph7_value sKey;` |
|        - | 12047 | `				/* Perform the insertion */` |
|      109 | 12048 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 | 12049 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 | 12050 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 | 12051 | `				PH7_MemObjRelease(&sKey);` |
|       54 | 12052 | `			}` |
|       54 | 12053 | `		}` |
|       55 | 12054 | `	}` |
|      111 | 12055 | `	return SXRET_OK;` |
|        1 | 12056 |  |
|        - | 12057 | `/*` |
|        - | 12058 | ` * array get_defined_vars(void)` |
|        - | 12059 | ` *  Returns an array of all defined variables.` |
|        - | 12060 | ` * Parameter` |
|        - | 12061 | ` *  None` |
|        - | 12062 | ` * Return` |
|        - | 12063 | ` *  An array with all the variables defined in the current scope.` |
|        - | 12064 | ` */` |
|        2 | 12065 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12066 |  |
|        3 | 12067 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12068 | `	ph7_value *pArray;` |
|        - | 12069 | `	/* Create a new array */` |
|        3 | 12070 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12071 | ` 	if( pArray == 0 ){` |
|      ! 0 | 12072 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12073 | `		SXUNUSED(apArg);` |
|        - | 12074 | `		/* Return NULL */` |
|      ! 0 | 12075 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12076 | `		return SXRET_OK;` |
|        - | 12077 | `	}` |
|        - | 12078 | `	/* Superglobals first */` |
|        3 | 12079 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 12080 | `	/* Then variable defined in the current frame */` |
|        3 | 12081 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 12082 | `	/* Finally,return the created array */` |
|        3 | 12083 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12084 | `	return SXRET_OK;` |
|        2 | 12085 |  |
|        - | 12086 | `/*` |
|        - | 12087 | ` * bool gettype($var)` |
|        - | 12088 | ` *  Get the type of a variable` |
|        - | 12089 | ` * Parameters` |
|        - | 12090 | ` *   $var` |
|        - | 12091 | ` *    The variable being type checked.` |
|        - | 12092 | ` * Return` |
|        - | 12093 | ` *   String representation of the given variable type.` |
|        - | 12094 | ` */` |
|       32 | 12095 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12096 |  |
|       34 | 12097 | `	const char *zType = "Empty";` |
|       34 | 12098 | `	if( nArg > 0 ){` |
|       34 | 12099 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 12100 | `	}` |
|        - | 12101 | `	/* Return the variable type */` |
|       34 | 12102 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 12103 | `	return SXRET_OK;` |
|        2 | 12104 |  |
|        - | 12105 | `/*` |
|        - | 12106 | ` * string get_resource_type(resource $handle)` |
|        - | 12107 | ` *  This function gets the type of the given resource.` |
|        - | 12108 | ` * Parameters` |
|        - | 12109 | ` *  $handle` |
|        - | 12110 | ` *  The evaluated resource handle.` |
|        - | 12111 | ` * Return` |
|        - | 12112 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 12113 | ` *  representing its type. If the type is not identified by this function` |
|        - | 12114 | ` *  the return value will be the string Unknown.` |
|        - | 12115 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 12116 | ` *  is not a resource.` |
|        - | 12117 | ` */` |
|        2 | 12118 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12119 |  |
|        3 | 12120 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12121 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 12122 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12123 | `		return PH7_OK;` |
|        - | 12124 | `	}` |
|        3 | 12125 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 12126 | `	return SXRET_OK;` |
|        2 | 12127 |  |
|        - | 12128 | `/*` |
|        - | 12129 | ` * void var_dump(expression,....)` |
|        - | 12130 | ` *   var_dump � Dumps information about a variable` |
|        - | 12131 | ` * Parameters` |
|        - | 12132 | ` *   One or more expression to dump.` |
|        - | 12133 | ` * Returns` |
|        - | 12134 | ` *  Nothing.` |
|        - | 12135 | ` */` |
|      218 | 12136 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12137 |  |
|        - | 12138 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 12139 | `	int i;` |
|      220 | 12140 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 12141 | `	/* Dump one or more expressions */` |
|      444 | 12142 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 12143 | `		ph7_value *pObj = apArg[i];` |
|        - | 12144 | `		/* Reset the working buffer */` |
|      226 | 12145 | `		SyBlobReset(&sDump);` |
|        - | 12146 | `		/* Dump the given expression */` |
|      226 | 12147 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 12148 | `		/* Output */` |
|      226 | 12149 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 12150 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 12151 | `		}` |
|      114 | 12152 | `	}` |
|        - | 12153 | `	/* Release the working buffer */` |
|      220 | 12154 | `	SyBlobRelease(&sDump);` |
|      220 | 12155 | `	return SXRET_OK;` |
|        2 | 12156 |  |
|        - | 12157 | `/*` |
|        - | 12158 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 12159 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 12160 | ` * Parameters` |
|        - | 12161 | ` *   expression: Expression to dump` |
|        - | 12162 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 12163 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 12164 | ` *            print_r() will return the information rather than print it.` |
|        - | 12165 | ` * Return` |
|        - | 12166 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 12167 | ` *  Otherwise, the return value is TRUE.` |
|        - | 12168 | ` */` |
|       16 | 12169 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12170 |  |
|       17 | 12171 | `	int ret_string = 0;` |
|        - | 12172 | `	SyBlob sDump;` |
|       17 | 12173 | `	if( nArg < 1 ){` |
|        - | 12174 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 12175 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12176 | `		return SXRET_OK;` |
|        - | 12177 | `	}` |
|       17 | 12178 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 12179 | `	if ( nArg > 1 ){` |
|        - | 12180 | `		/* Where to redirect output */` |
|       11 | 12181 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 12182 | `	}` |
|        - | 12183 | `	/* Generate dump */` |
|       17 | 12184 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 12185 | `	if( !ret_string ){` |
|        - | 12186 | `		/* Output dump */` |
|        7 | 12187 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12188 | `		/* Return true */` |
|        7 | 12189 | `		ph7_result_bool(pCtx,1);` |
|        4 | 12190 | `	}else{` |
|        - | 12191 | `		/* Generated dump as return value */` |
|       11 | 12192 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12193 | `	}` |
|        - | 12194 | `	/* Release the working buffer */` |
|       17 | 12195 | `	SyBlobRelease(&sDump);` |
|       17 | 12196 | `	return SXRET_OK;` |
|        9 | 12197 |  |
|        - | 12198 | `/*` |
|        - | 12199 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 12200 | ` * Same job as print_r. (see coment above)` |
|        - | 12201 | ` */` |
|        2 | 12202 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12203 |  |
|        3 | 12204 | `	int ret_string = 0;` |
|        - | 12205 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 12206 | `	if( nArg < 1 ){` |
|        - | 12207 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 12208 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12209 | `		return SXRET_OK;` |
|        - | 12210 | `	}` |
|        3 | 12211 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 12212 | `	if ( nArg > 1 ){` |
|        - | 12213 | `		/* Where to redirect output */` |
|        3 | 12214 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 12215 | `	}` |
|        - | 12216 | `	/* Generate dump */` |
|        3 | 12217 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 12218 | `	if( !ret_string ){` |
|        - | 12219 | `		/* Output dump */` |
|      ! 0 | 12220 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12221 | `		/* Return NULL */` |
|      ! 0 | 12222 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12223 | `	}else{` |
|        - | 12224 | `		/* Generated dump as return value */` |
|        3 | 12225 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12226 | `	}` |
|        - | 12227 | `	/* Release the working buffer */` |
|        3 | 12228 | `	SyBlobRelease(&sDump);` |
|        3 | 12229 | `	return SXRET_OK;` |
|        2 | 12230 |  |
|        - | 12231 | `/*` |
|        - | 12232 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 12233 | ` *  Set/get the various assert flags.` |
|        - | 12234 | ` * Parameter` |
|        - | 12235 | ` * $what` |
|        - | 12236 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 12237 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 12238 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 12239 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 12240 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 12241 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 12242 | ` * $value` |
|        - | 12243 | ` *   An optional new value for the option.` |
|        - | 12244 | ` * Return` |
|        - | 12245 | ` *  Old setting on success or FALSE on failure.` |
|        - | 12246 | ` */` |
|       28 | 12247 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12248 |  |
|       30 | 12249 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12250 | `	int iOption;` |
|        - | 12251 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 12252 | `	if( nArg < 1 ){` |
|        3 | 12253 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12254 | `			"ArgumentCountError",` |
|        - | 12255 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 12256 | `			);` |
|        - | 12257 | `	}` |
|        - | 12258 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 12259 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 12260 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 12261 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12262 | `			"TypeError",` |
|        - | 12263 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 12264 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 12265 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 12266 | `			);` |
|        - | 12267 | `	}` |
|       28 | 12268 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 12269 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 12270 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 12271 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 12272 | `	switch( iOption ){` |
|        5 | 12273 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 12274 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 12275 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 12276 | `		if( nArg > 1 ){` |
|        5 | 12277 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 12278 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 12279 | `			}else{` |
|        3 | 12280 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 12281 | `			}` |
|        2 | 12282 | `		}` |
|       12 | 12283 | `		break;` |
|        1 | 12284 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 12285 | `		/* Return old callback or null */` |
|        3 | 12286 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 12287 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 12288 | `		}else{` |
|        3 | 12289 | `			ph7_result_null(pCtx);` |
|        - | 12290 | `		}` |
|        3 | 12291 | `		if( nArg > 1 ){` |
|      ! 0 | 12292 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 12293 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 12294 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 12295 | `			}else{` |
|      ! 0 | 12296 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 12297 | `			}` |
|      ! 0 | 12298 | `		}` |
|        3 | 12299 | `		break;` |
|        5 | 12300 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 12301 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 12302 | `		if( nArg > 1 ){` |
|        5 | 12303 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 12304 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 12305 | `			}else{` |
|        3 | 12306 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 12307 | `			}` |
|        2 | 12308 | `		}` |
|       11 | 12309 | `		break;` |
|      ! 0 | 12310 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 12311 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 12312 | `		break;` |
|        1 | 12313 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 12314 | `		ph7_result_int(pCtx, 1);` |
|        3 | 12315 | `		break;` |
|      ! 0 | 12316 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 12317 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 12318 | `		break;` |
|        1 | 12319 | `	default:` |
|        - | 12320 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 12321 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12322 | `			"ValueError",` |
|        - | 12323 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 12324 | `			);` |
|        - | 12325 | `	}` |
|       26 | 12326 | `	return PH7_OK;` |
|       16 | 12327 |  |
|        - | 12328 | `/*` |
|        - | 12329 | ` * bool assert(mixed $assertion)` |
|        - | 12330 | ` *  Checks if assertion is FALSE.` |
|        - | 12331 | ` * Parameter` |
|        - | 12332 | ` *  $assertion` |
|        - | 12333 | ` *    The assertion to test.` |
|        - | 12334 | ` * Return` |
|        - | 12335 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 12336 | ` */` |
|       24 | 12337 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12338 |  |
|       26 | 12339 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12340 | `	int iFlags,iResult;` |
|        - | 12341 | `	const char *zDesc;` |
|        - | 12342 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 12343 | `	if( nArg < 1 ){` |
|        3 | 12344 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12345 | `			"ArgumentCountError",` |
|        - | 12346 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 12347 | `			);` |
|        - | 12348 | `	}` |
|       24 | 12349 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 12350 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 12351 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 12352 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 12353 | `		return PH7_OK;` |
|        - | 12354 | `	}` |
|        - | 12355 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 12356 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 12357 | `	if( !iResult ){` |
|        - | 12358 | `		/* Assertion failed */` |
|        - | 12359 | `		/* Extract optional description */` |
|       13 | 12360 | `		zDesc = 0;` |
|       13 | 12361 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 12362 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 12363 | `		}` |
|       13 | 12364 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 12365 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 12366 | `			ph7_value sFile,sLine;` |
|        - | 12367 | `			ph7_value *apCbArg[3];` |
|        - | 12368 | `			SyString *pFile;` |
|        - | 12369 | `			/* Extract the processed script */` |
|      ! 0 | 12370 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 12371 | `			if( pFile == 0 ){` |
|      ! 0 | 12372 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 12373 | `			}` |
|        - | 12374 | `			/* Invoke the callback */` |
|      ! 0 | 12375 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 12376 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 12377 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 12378 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 12379 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 12380 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 12381 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 12382 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 12383 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 12384 | `		}` |
|       13 | 12385 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 12386 | `			/* Abort VM execution immediately */` |
|      ! 0 | 12387 | `			return PH7_ABORT;` |
|        - | 12388 | `		}` |
|        - | 12389 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 12390 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 12391 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12392 | `				"AssertionError",` |
|        - | 12393 | `				"%s",` |
|        1 | 12394 | `				zDesc` |
|        - | 12395 | `				);` |
|      ! 0 | 12396 | `		}else{` |
|       11 | 12397 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12398 | `				"AssertionError",` |
|        - | 12399 | `				"assert(false)"` |
|        - | 12400 | `				);` |
|        - | 12401 | `		}` |
|        - | 12402 | `	}` |
|        - | 12403 | `	/* Assertion passed */` |
|       11 | 12404 | `	ph7_result_bool(pCtx,1);` |
|       11 | 12405 | `	return PH7_OK;` |
|       14 | 12406 |  |
|        - | 12407 | `/*` |
|        - | 12408 | ` * Section:` |
|        - | 12409 | ` *  Error reporting functions.` |
|        - | 12410 | ` * Status:` |
|        - | 12411 | ` *    Stable.` |
|        - | 12412 | ` */` |
|        - | 12413 | `/*` |
|        - | 12414 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 12415 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 12416 | ` * Parameters` |
|        - | 12417 | ` *  $error_msg` |
|        - | 12418 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 12419 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 12420 | ` * $error_type` |
|        - | 12421 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 12422 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 12423 | ` * Return` |
|        - | 12424 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 12425 | ` */` |
|       12 | 12426 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12427 |  |
|       14 | 12428 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 12429 | `	int rc = PH7_OK;` |
|       14 | 12430 | `	if( nArg > 0 ){` |
|        - | 12431 | `		const char *zErr;` |
|        - | 12432 | `		int nLen;` |
|        - | 12433 | `		/* Extract the error message */` |
|       12 | 12434 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 12435 | `		if( nArg > 1 ){` |
|        - | 12436 | `			/* Extract the error type */` |
|       12 | 12437 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 12438 | `			switch( nErr ){` |
|        1 | 12439 | `			case 1:   /* E_ERROR */` |
|        - | 12440 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 12441 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 12442 | `			case 256: /* E_USER_ERROR */` |
|        3 | 12443 | `				nErr = PH7_CTX_ERR;` |
|        3 | 12444 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 12445 | `				break;` |
|        1 | 12446 | `			case 2:   /* E_WARNING */` |
|        - | 12447 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 12448 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 12449 | `			case 512: /* E_USER_WARNING */` |
|        3 | 12450 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 12451 | `				break;` |
|        3 | 12452 | `			default:` |
|        8 | 12453 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 12454 | `				break;` |
|        - | 12455 | `			}` |
|        5 | 12456 | `		}` |
|        - | 12457 | `		/* Report error */` |
|       12 | 12458 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 12459 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 12460 | `			return rc;` |
|        - | 12461 | `		}` |
|        - | 12462 | `		/* Return true */` |
|       12 | 12463 | `		ph7_result_bool(pCtx,1);` |
|        7 | 12464 | `	}else{` |
|        - | 12465 | `		/* Missing arguments,return FALSE */` |
|        3 | 12466 | `		ph7_result_bool(pCtx,0);` |
|        - | 12467 | `	}` |
|       14 | 12468 | `	return rc;` |
|        8 | 12469 |  |
|        - | 12470 | `/*` |
|        - | 12471 | ` * int error_reporting([int $level])` |
|        - | 12472 | ` *  Sets which PHP errors are reported.` |
|        - | 12473 | ` * Parameters` |
|        - | 12474 | ` *  $level` |
|        - | 12475 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 12476 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 12477 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 12478 | ` *   levels will not always behave as expected.` |
|        - | 12479 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 12480 | ` *   in the predefined constants.` |
|        - | 12481 | ` * Return` |
|        - | 12482 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 12483 | ` *   parameter is given.` |
|        - | 12484 | ` */` |
|       38 | 12485 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12486 |  |
|       40 | 12487 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12488 | `	int nOld;` |
|        - | 12489 | `	/* Extract the old reporting level */` |
|       40 | 12490 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 | 12491 | `	if( nArg > 0 ){` |
|        - | 12492 | `		int nNew;` |
|        - | 12493 | `		/* Extract the desired error reporting level */` |
|       32 | 12494 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 | 12495 | `		if( !nNew ){` |
|        - | 12496 | `			/* Do not report errors at all */` |
|        5 | 12497 | `			pVm->bErrReport = 0;` |
|        3 | 12498 | `		}else{` |
|        - | 12499 | `			/* Report all errors */` |
|       28 | 12500 | `			pVm->bErrReport = 1;` |
|        - | 12501 | `		}` |
|       15 | 12502 | `	}` |
|        - | 12503 | `	/* Return the old level */` |
|       40 | 12504 | `	ph7_result_int(pCtx,nOld);` |
|       40 | 12505 | `	return PH7_OK;` |
|        2 | 12506 |  |
|        - | 12507 | `/*` |
|        - | 12508 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 12509 | ` *  Send an error message somewhere.` |
|        - | 12510 | ` * Parameter` |
|        - | 12511 | ` *  $message` |
|        - | 12512 | ` *   The error message that should be logged.` |
|        - | 12513 | ` *  $message_type` |
|        - | 12514 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 12515 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 12516 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 12517 | ` *       This is the default option.` |
|        - | 12518 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 12519 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 12520 | ` *    2  No longer an option.` |
|        - | 12521 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 12522 | ` *       to the end of the message string.` |
|        - | 12523 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 12524 | ` *  $destination` |
|        - | 12525 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 12526 | ` *  $extra_headers` |
|        - | 12527 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 12528 | ` * Return` |
|        - | 12529 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12530 | ` * NOTE:` |
|        - | 12531 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 12532 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 12533 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 12534 | ` *  Otherwise this function is no-op.` |
|        - | 12535 | ` */` |
|        4 | 12536 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12537 |  |
|        - | 12538 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 12539 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 12540 | `	int iType = 0;` |
|        5 | 12541 | `	if( nArg < 1 ){` |
|        - | 12542 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 12543 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12544 | `		return PH7_OK;` |
|        - | 12545 | `	}` |
|        5 | 12546 | `	if( pVm->xErrLog  ){` |
|        - | 12547 | `		/* Invoke the user callback */` |
|      ! 0 | 12548 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 12549 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 12550 | `		if( nArg > 1 ){` |
|      ! 0 | 12551 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 12552 | `			if( nArg > 2 ){` |
|      ! 0 | 12553 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 12554 | `				if( nArg > 3 ){` |
|      ! 0 | 12555 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 12556 | `				}` |
|      ! 0 | 12557 | `			}` |
|      ! 0 | 12558 | `		}` |
|      ! 0 | 12559 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 12560 | `	}` |
|        - | 12561 | `	/* Retun TRUE */` |
|        5 | 12562 | `	ph7_result_bool(pCtx,1);` |
|        5 | 12563 | `	return PH7_OK;` |
|        3 | 12564 |  |
|        - | 12565 | `/*` |
|        - | 12566 | ` * bool restore_exception_handler(void)` |
|        - | 12567 | ` *  Restores the previously defined exception handler function.` |
|        - | 12568 | ` * Parameter` |
|        - | 12569 | ` *  None` |
|        - | 12570 | ` * Return` |
|        - | 12571 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 12572 | ` */` |
|        4 | 12573 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12574 |  |
|        5 | 12575 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12576 | `	ph7_value *pOld,*pNew;` |
|        - | 12577 | `	/* Point to the old and the new handler */` |
|        5 | 12578 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 12579 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 12580 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 12581 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 12582 | `		SXUNUSED(apArg);` |
|        - | 12583 | `		/* No installed handler,return FALSE */` |
|        5 | 12584 | `		ph7_result_bool(pCtx,0);` |
|        5 | 12585 | `		return PH7_OK;` |
|        - | 12586 | `	}` |
|        - | 12587 | `	/* Copy the old handler */` |
|      ! 0 | 12588 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 12589 | `	PH7_MemObjRelease(pOld);` |
|        - | 12590 | `	/* Return TRUE */` |
|      ! 0 | 12591 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 12592 | `	return PH7_OK;` |
|        3 | 12593 |  |
|        - | 12594 | `/*` |
|        - | 12595 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 12596 | ` *  Sets a user-defined exception handler function.` |
|        - | 12597 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 12598 | ` * NOTE` |
|        - | 12599 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 12600 | ` *  the satndard PHP engine.` |
|        - | 12601 | ` * Parameters` |
|        - | 12602 | ` *  $exception_handler` |
|        - | 12603 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 12604 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 12605 | ` *   that was thrown.` |
|        - | 12606 | ` *  Note:` |
|        - | 12607 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 12608 | ` * Return` |
|        - | 12609 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 12610 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 12611 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 12612 | ` */` |
|        4 | 12613 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12614 |  |
|        6 | 12615 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12616 | `	ph7_value *pOld,*pNew;` |
|        - | 12617 | `	/* Point to the old and the new handler */` |
|        6 | 12618 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 12619 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 12620 | `	/* Return the old handler */` |
|        6 | 12621 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 12622 | `	if( nArg > 0 ){` |
|        6 | 12623 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 12624 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 12625 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 12626 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 12627 | `		}else{` |
|        6 | 12628 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 12629 | `			/* Install the new handler */` |
|        6 | 12630 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 12631 | `		}` |
|        2 | 12632 | `	}` |
|        6 | 12633 | `	return PH7_OK;` |
|        2 | 12634 |  |
|        - | 12635 | `/*` |
|        - | 12636 | ` * bool restore_error_handler(void)` |
|        - | 12637 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 12638 | ` * Parameters:` |
|        - | 12639 | ` *  None.` |
|        - | 12640 | ` * Return` |
|        - | 12641 | ` *  Always TRUE.` |
|        - | 12642 | ` */` |
|        6 | 12643 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12644 |  |
|        7 | 12645 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12646 | `	ph7_value *pOld,*pNew;` |
|        - | 12647 | `	/* Point to the old and the new handler */` |
|        7 | 12648 | `	pOld = &pVm->aErrCB[0];` |
|        7 | 12649 | `	pNew = &pVm->aErrCB[1];` |
|        7 | 12650 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 12651 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 12652 | `		SXUNUSED(apArg);` |
|        - | 12653 | `		/* No installed callback,return FALSE */` |
|        7 | 12654 | `		ph7_result_bool(pCtx,0);` |
|        7 | 12655 | `		return PH7_OK;` |
|        - | 12656 | `	}` |
|        - | 12657 | `	/* Copy the old callback */` |
|      ! 0 | 12658 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 12659 | `	PH7_MemObjRelease(pOld);` |
|        - | 12660 | `	/* Return TRUE */` |
|      ! 0 | 12661 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 12662 | `	return PH7_OK;` |
|        4 | 12663 |  |
|        - | 12664 | `/*` |
|        - | 12665 | ` * value set_error_handler(callable $error_handler)` |
|        - | 12666 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 12667 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 12668 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 12669 | ` *  Sets a user-defined error handler function.` |
|        - | 12670 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 12671 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 12672 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 12673 | ` *  conditions (using trigger_error()).` |
|        - | 12674 | ` * Parameters` |
|        - | 12675 | ` *  $error_handler` |
|        - | 12676 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 12677 | ` *   describing the error.` |
|        - | 12678 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 12679 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 12680 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 12681 | ` *   The function can be shown as:` |
|        - | 12682 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 12683 | ` *     errno` |
|        - | 12684 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 12685 | ` *   errstr` |
|        - | 12686 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 12687 | ` *   errfile` |
|        - | 12688 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 12689 | ` *     was raised in, as a string.` |
|        - | 12690 | ` *  Note:` |
|        - | 12691 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 12692 | ` * Return` |
|        - | 12693 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 12694 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 12695 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 12696 | ` */` |
|    10572 | 12697 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12698 |  |
|    10574 | 12699 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12700 | `	ph7_value *pOld,*pNew;` |
|        - | 12701 | `	/* Point to the old and the new handler */` |
|    10574 | 12702 | `	pOld = &pVm->aErrCB[0];` |
|    10574 | 12703 | `	pNew = &pVm->aErrCB[1];` |
|        - | 12704 | `	/* Return the old handler */` |
|    10574 | 12705 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    10574 | 12706 | `	if( nArg > 0 ){` |
|    10574 | 12707 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 12708 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5281 | 12709 | `			PH7_MemObjRelease(pNew);` |
|     5281 | 12710 | `			ph7_result_bool(pCtx,1);` |
|     2641 | 12711 | `		}else{` |
|     5294 | 12712 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 12713 | `			/* Install the new handler */` |
|     5294 | 12714 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 12715 | `		}` |
|     5286 | 12716 | `	}` |
|    10574 | 12717 | `	return PH7_OK;` |
|        2 | 12718 |  |
|        - | 12719 | `/*` |
|        - | 12720 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 12721 | ` *  Generates a backtrace.` |
|        - | 12722 | ` * Paramaeter` |
|        - | 12723 | ` *  $options` |
|        - | 12724 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 12725 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 12726 | ` *   all the function/method arguments, to save memory.` |
|        - | 12727 | ` * $limit` |
|        - | 12728 | ` *   (Not Used)` |
|        - | 12729 | ` * Return` |
|        - | 12730 | ` *  An array.The possible returned elements are as follows:` |
|        - | 12731 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 12732 | ` *          Name        Type      Description` |
|        - | 12733 | ` *          ------      ------     -----------` |
|        - | 12734 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 12735 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 12736 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 12737 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 12738 | ` *          object      object    The current object.` |
|        - | 12739 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 12740 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 12741 | ` */` |
|      846 | 12742 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12743 |  |
|      848 | 12744 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12745 | `	ph7_value *pArray;` |
|        - | 12746 | `	ph7_class *pClass;` |
|        - | 12747 | `	ph7_value *pValue;` |
|        - | 12748 | `	SyString *pFile;` |
|        - | 12749 | `	/* Create a new array */` |
|      848 | 12750 | `	pArray = ph7_context_new_array(pCtx);` |
|      848 | 12751 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      848 | 12752 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 12753 | `		/* Out of memory,return NULL */` |
|      ! 0 | 12754 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 12755 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12756 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12757 | `		SXUNUSED(apArg);` |
|      ! 0 | 12758 | `		return PH7_OK;` |
|        - | 12759 | `	}` |
|        - | 12760 | `	/* Dump running function name and it's arguments  */` |
|      848 | 12761 | `	if( pVm->pFrame->pParent ){` |
|      848 | 12762 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 12763 | `		ph7_vm_func *pFunc;` |
|        - | 12764 | `		ph7_value *pArg;` |
|      848 | 12765 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      848 | 12766 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      848 | 12767 | `		if( pFrame->pParent && pFunc ){` |
|      848 | 12768 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      848 | 12769 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      848 | 12770 | `			ph7_value_reset_string_cursor(pValue);` |
|      423 | 12771 | `		}` |
|        - | 12772 | `		/* Function arguments */` |
|      848 | 12773 | `		pArg = ph7_context_new_array(pCtx);` |
|      848 | 12774 | `		if( pArg  ){` |
|        - | 12775 | `			ph7_value *pObj;` |
|        - | 12776 | `			VmSlot *aSlot;` |
|        - | 12777 | `			sxu32 n;` |
|        - | 12778 | `			/* Start filling the array with the given arguments */` |
|      848 | 12779 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     3390 | 12780 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2544 | 12781 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2544 | 12782 | `				if( pObj ){` |
|     2544 | 12783 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1271 | 12784 | `				}` |
|     1273 | 12785 | `			}` |
|        - | 12786 | `			/* Save the array */` |
|      848 | 12787 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      423 | 12788 | `		}` |
|      423 | 12789 | `	}` |
|      848 | 12790 | `	ph7_value_int(pValue,1);` |
|        - | 12791 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 12792 | `	 * line numbers at run-time. )` |
|        - | 12793 | `	 */` |
|      848 | 12794 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 12795 | `	/* Current processed script */` |
|      848 | 12796 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      848 | 12797 | `	if( pFile ){` |
|      848 | 12798 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      848 | 12799 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      848 | 12800 | `		ph7_value_reset_string_cursor(pValue);` |
|      423 | 12801 | `	}` |
|        - | 12802 | `	/* Top class */` |
|      848 | 12803 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      848 | 12804 | `	if( pClass ){` |
|      844 | 12805 | `		ph7_value_reset_string_cursor(pValue);` |
|      844 | 12806 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      844 | 12807 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      421 | 12808 | `	}` |
|        - | 12809 | `	/* Return the freshly created array */` |
|      848 | 12810 | `	ph7_result_value(pCtx,pArray);` |
|        - | 12811 | `	/*` |
|        - | 12812 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 12813 | `	 * as soon we return from this function.` |
|        - | 12814 | `	 */` |
|      848 | 12815 | `	return PH7_OK;` |
|      425 | 12816 |  |
|        - | 12817 | `/*` |
|        - | 12818 | ` * Generate a small backtrace.` |
|        - | 12819 | ` * Store the generated dump in the given BLOB` |
|        - | 12820 | ` */` |
|        4 | 12821 | `static int VmMiniBacktrace(` |
|        - | 12822 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 12823 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 12824 | `	)` |
|        1 | 12825 |  |
|        5 | 12826 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12827 | `	ph7_vm_func *pFunc;` |
|        - | 12828 | `	ph7_class *pClass;` |
|        - | 12829 | `	SyString *pFile;` |
|        - | 12830 | `	/* Called function */` |
|        5 | 12831 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 12832 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 12833 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 12834 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 12835 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 12836 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 12837 | `	}else{` |
|      ! 0 | 12838 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 12839 | `	}` |
|        5 | 12840 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 12841 | `	/* Current processed script */` |
|        5 | 12842 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 12843 | `	if( pFile ){` |
|        5 | 12844 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 12845 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 12846 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 12847 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 12848 | `	}` |
|        - | 12849 | `	/* Top class */` |
|        5 | 12850 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 12851 | `	if( pClass ){` |
|      ! 0 | 12852 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 12853 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 12854 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 12855 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 12856 | `	}` |
|        5 | 12857 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 12858 | `	/* All done */` |
|        5 | 12859 | `	return SXRET_OK;` |
|        1 | 12860 |  |
|        - | 12861 | `/*` |
|        - | 12862 | ` * void debug_print_backtrace()` |
|        - | 12863 | ` *  Prints a backtrace` |
|        - | 12864 | ` * Parameters` |
|        - | 12865 | ` * None` |
|        - | 12866 | ` * Return` |
|        - | 12867 | ` * NULL` |
|        - | 12868 | ` */` |
|        2 | 12869 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12870 |  |
|        3 | 12871 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12872 | `	SyBlob sDump;` |
|        3 | 12873 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 12874 | `	/* Generate the backtrace */` |
|        3 | 12875 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 12876 | `	/* Output backtrace */` |
|        3 | 12877 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12878 | `	/* All done,cleanup */` |
|        3 | 12879 | `	SyBlobRelease(&sDump);` |
|        1 | 12880 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 12881 | `	SXUNUSED(apArg);` |
|        3 | 12882 | `	return PH7_OK;` |
|        1 | 12883 |  |
|        - | 12884 | `/*` |
|        - | 12885 | ` * string debug_string_backtrace()` |
|        - | 12886 | ` *  Generate a backtrace` |
|        - | 12887 | ` * Parameters` |
|        - | 12888 | ` * None` |
|        - | 12889 | ` * Return` |
|        - | 12890 | ` *  A mini backtrace().` |
|        - | 12891 | ` * Note that this is a symisc extension.` |
|        - | 12892 | ` */` |
|        2 | 12893 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12894 |  |
|        3 | 12895 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12896 | `	SyBlob sDump;` |
|        3 | 12897 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 12898 | `	/* Generate the backtrace */` |
|        3 | 12899 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 12900 | `	/* Return the backtrace */` |
|        3 | 12901 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 12902 | `	/* All done,cleanup */` |
|        3 | 12903 | `	SyBlobRelease(&sDump);` |
|        1 | 12904 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 12905 | `	SXUNUSED(apArg);` |
|        3 | 12906 | `	return PH7_OK;` |
|        1 | 12907 |  |
|        - | 12908 | `/*` |
|        - | 12909 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 12910 | ` * exception is triggered.` |
|        - | 12911 | ` */` |
|      510 | 12912 | `static sxi32 VmUncaughtException(` |
|        - | 12913 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 12914 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 12915 | `	)` |
|        1 | 12916 |  |
|        - | 12917 | `	ph7_value *apArg[2],sArg;` |
|      511 | 12918 | `	int nArg = 1;` |
|        - | 12919 | `	sxi32 rc;` |
|      511 | 12920 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 12921 | `		/* Nesting limit reached */` |
|      ! 0 | 12922 | `		return SXRET_OK;` |
|        - | 12923 | `	}` |
|        - | 12924 | `	/* Call any exception handler if available */` |
|      511 | 12925 | `	PH7_MemObjInit(pVm,&sArg);` |
|      511 | 12926 | `	if( pThis ){` |
|        - | 12927 | `		/* Load the exception instance */` |
|      511 | 12928 | `		sArg.x.pOther = pThis;` |
|      511 | 12929 | `		pThis->iRef++;` |
|      511 | 12930 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      256 | 12931 | `	}else{` |
|      ! 0 | 12932 | `		nArg = 0;` |
|        - | 12933 | `	}` |
|      511 | 12934 | `	apArg[0] = &sArg;` |
|        - | 12935 | `	/* Call the exception handler if available */` |
|      511 | 12936 | `	pVm->nExceptDepth++;` |
|      511 | 12937 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      511 | 12938 | `	pVm->nExceptDepth--;` |
|      511 | 12939 | `	if( rc != SXRET_OK ){` |
|        - | 12940 | `		SyBlob sMsgBuf;` |
|      509 | 12941 | `		const char *zClass = "Exception";` |
|      509 | 12942 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 12943 | `		const char *zMsg;` |
|        - | 12944 | `		sxu32 nMsg;` |
|        - | 12945 | `		const char *zFuncName;` |
|        - | 12946 | `		int nFuncLen;` |
|      509 | 12947 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      509 | 12948 | `		if( pThis ){` |
|        - | 12949 | `			ph7_class_method *pGetMessage;` |
|        - | 12950 | `			ph7_value sMsg;` |
|        - | 12951 | `			const char *zTmp;` |
|        - | 12952 | `			int nTmp;` |
|      509 | 12953 | `			zClass = pThis->pClass->sName.zString;` |
|      509 | 12954 | `			nClass = pThis->pClass->sName.nByte;` |
|      509 | 12955 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      509 | 12956 | `			if( pGetMessage ){` |
|      509 | 12957 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      509 | 12958 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      509 | 12959 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      509 | 12960 | `					if( zTmp && nTmp > 0 ){` |
|      509 | 12961 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      254 | 12962 | `					}` |
|      254 | 12963 | `				}` |
|      509 | 12964 | `				PH7_MemObjRelease(&sMsg);` |
|      254 | 12965 | `			}` |
|      254 | 12966 | `		}` |
|      509 | 12967 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      509 | 12968 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      509 | 12969 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      509 | 12970 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      509 | 12971 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 12972 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      509 | 12973 | `		rc = SXERR_ABORT;` |
|      254 | 12974 | `	}` |
|      511 | 12975 | `	PH7_MemObjRelease(&sArg);` |
|      511 | 12976 | `	return rc;` |
|      256 | 12977 |  |
|        - | 12978 | `/*` |
|        - | 12979 | ` * Throw a user exception.` |
|        - | 12980 | ` *` |
|        - | 12981 | ` * Exception dispatch follows this sequence:` |
|        - | 12982 | ` *` |
|        - | 12983 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 12984 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 12985 | ` *` |
|        - | 12986 | ` * 2. If NO catch matches:` |
|        - | 12987 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 12988 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 12989 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 12990 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 12991 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 12992 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 12993 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 12994 | ` *` |
|        - | 12995 | ` * 3. If a catch DOES match:` |
|        - | 12996 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 12997 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 12998 | ` *       inside the catch body from immediately propagating past our` |
|        - | 12999 | ` *       finally block.` |
|        - | 13000 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 13001 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 13002 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 13003 | ` *       in pPendingException (step 2c).` |
|        - | 13004 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 13005 | ` *    d. Run finally (if present).` |
|        - | 13006 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 13007 | ` *       that handlers are restored and finally has run.` |
|        - | 13008 | ` */` |
|      802 | 13009 | `static sxi32 VmThrowException(` |
|        - | 13010 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 13011 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13012 | `	)` |
|        2 | 13013 |  |
|        - | 13014 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 13015 | `	ph7_exception **apException;` |
|        - | 13016 | `	ph7_exception *pException;` |
|        - | 13017 | `	/* Point to the stack of loaded exceptions */` |
|      804 | 13018 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      804 | 13019 | `	pException = 0;` |
|      804 | 13020 | `	pCatch = 0;` |
|      804 | 13021 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13022 | `		ph7_exception_block *aCatch;` |
|        - | 13023 | `		ph7_class *pClass;` |
|        - | 13024 | `		SyString *aNames;` |
|        - | 13025 | `		sxu32 nNames;` |
|        - | 13026 | `		int matched;` |
|        - | 13027 | `		sxu32 j,k;` |
|        - | 13028 | `		/* Locate the appropriate block to execute */` |
|      286 | 13029 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      286 | 13030 | `		(void)SySetPop(&pVm->aException);` |
|      286 | 13031 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      294 | 13032 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 13033 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      292 | 13034 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      292 | 13035 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      292 | 13036 | `			matched = 0;` |
|      318 | 13037 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 13038 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 13039 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 13040 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      310 | 13041 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      310 | 13042 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 13043 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 13044 | `					continue;` |
|        - | 13045 | `				}` |
|      310 | 13046 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      284 | 13047 | `					matched = 1;` |
|      284 | 13048 | `					break;` |
|        - | 13049 | `				}` |
|       14 | 13050 | `			}` |
|      292 | 13051 | `			if( matched ){` |
|        - | 13052 | `				/* Catch block found,break immediately */` |
|      284 | 13053 | `				pCatch = &aCatch[j];` |
|      284 | 13054 | `				break;` |
|        - | 13055 | `			}` |
|        5 | 13056 | `		}` |
|      142 | 13057 | `	}` |
|        - | 13058 | `	/* Execute the cached block if available */` |
|      804 | 13059 | `	if( pCatch == 0 ){` |
|        - | 13060 | `		sxi32 rc;` |
|        - | 13061 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      522 | 13062 | `		if( pException && pException->iHasFinally ){` |
|        3 | 13063 | `			pException->iFinallyDone = 1;` |
|        3 | 13064 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 13065 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13066 | `				return SXERR_ABORT;` |
|        - | 13067 | `			}` |
|        1 | 13068 | `		}` |
|        - | 13069 | `		/* Check if there is an outer exception handler on the stack */` |
|      522 | 13070 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13071 | `			/* Re-throw to the outer handler */` |
|        3 | 13072 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 13073 | `		}` |
|        - | 13074 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 13075 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 13076 | `		 * exception instead of reporting it uncaught.` |
|        - | 13077 | `		 */` |
|      520 | 13078 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 13079 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 13080 | `			 * by looking for a catch frame on the stack.` |
|        - | 13081 | `			 */` |
|      520 | 13082 | `			VmFrame *pF = pVm->pFrame;` |
|      520 | 13083 | `			int inCatch = 0;` |
|     1046 | 13084 | `			while( pF ){` |
|      536 | 13085 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        9 | 13086 | `					inCatch = 1;` |
|        9 | 13087 | `					break;` |
|        - | 13088 | `				}` |
|      527 | 13089 | `				pF = pF->pParent;` |
|        1 | 13090 | `			}` |
|      520 | 13091 | `			if( inCatch ){` |
|        - | 13092 | `				/* Defer — will be re-thrown after finally runs */` |
|        9 | 13093 | `				pThis->iRef++;` |
|        9 | 13094 | `				pVm->pPendingException = pThis;` |
|        9 | 13095 | `				return SXRET_OK;` |
|        - | 13096 | `			}` |
|      255 | 13097 | `		}` |
|        - | 13098 | `		/* Truly uncaught */` |
|      511 | 13099 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      511 | 13100 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 13101 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 13102 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 13103 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 13104 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 13105 | `			}` |
|      ! 0 | 13106 | `		}` |
|      511 | 13107 | `		return rc;` |
|      ! 0 | 13108 | `	}else{` |
|      284 | 13109 | `		VmFrame *pFrame = pVm->pFrame;` |
|      284 | 13110 | `		ph7_exception **apSaved = 0;` |
|        - | 13111 | `		sxu32 nSavedCount;` |
|        - | 13112 | `		sxi32 rc;` |
|      284 | 13113 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      284 | 13114 | `		if( pException->pFrame == pFrame ){` |
|      220 | 13115 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      109 | 13116 | `		}` |
|        - | 13117 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 13118 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 13119 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 13120 | `		 */` |
|      284 | 13121 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      284 | 13122 | `		if( nSavedCount > 0 ){` |
|       16 | 13123 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        5 | 13124 | `				nSavedCount * sizeof(ph7_exception *));` |
|       11 | 13125 | `			if( apSaved ){` |
|       16 | 13126 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        5 | 13127 | `					nSavedCount * sizeof(ph7_exception *));` |
|       11 | 13128 | `				SySetReset(&pVm->aException);` |
|        5 | 13129 | `			}` |
|        5 | 13130 | `		}` |
|        - | 13131 | `		/* Create a private frame first */` |
|      284 | 13132 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      284 | 13133 | `		if( rc == SXRET_OK ){` |
|      284 | 13134 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      284 | 13135 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      284 | 13136 | `			if( pObj ){` |
|      284 | 13137 | `				pThis->iRef++;` |
|      284 | 13138 | `				pObj->x.pOther = pThis;` |
|      284 | 13139 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      141 | 13140 | `			}` |
|        - | 13141 | `			/* Execute the catch block */` |
|      284 | 13142 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 13143 | `			/* Leave the frame */` |
|      284 | 13144 | `			VmLeaveFrame(&(*pVm));` |
|      141 | 13145 | `		}` |
|        - | 13146 | `		/* Restore the outer exception handlers */` |
|      284 | 13147 | `		if( apSaved ){` |
|        - | 13148 | `			sxu32 k;` |
|        - | 13149 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 13150 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 13151 | `			 * Restore the original outer entries.` |
|        - | 13152 | `			 */` |
|       11 | 13153 | `			SySetReset(&pVm->aException);` |
|       21 | 13154 | `			for(k = 0; k < nSavedCount; k++){` |
|       11 | 13155 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        6 | 13156 | `			}` |
|       11 | 13157 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        5 | 13158 | `		}` |
|        - | 13159 | `		/* Execute the finally block after catch */` |
|      284 | 13160 | `		if( pException->iHasFinally ){` |
|       16 | 13161 | `			pException->iFinallyDone = 1;` |
|        - | 13162 | `			{` |
|       16 | 13163 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 13164 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 13165 | `					return SXERR_ABORT;` |
|        - | 13166 | `				}` |
|        - | 13167 | `			}` |
|        7 | 13168 | `		}` |
|      284 | 13169 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 13170 | `			return SXERR_ABORT;` |
|        - | 13171 | `		}` |
|        - | 13172 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 13173 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 13174 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 13175 | `		 */` |
|      284 | 13176 | `		if( pVm->pPendingException ){` |
|        9 | 13177 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 13178 | `			pVm->pPendingException = 0;` |
|        9 | 13179 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 13180 | `		}` |
|        - | 13181 | `	}` |
|        - | 13182 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 13183 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 13184 | `	 */` |
|      276 | 13185 | `	return SXRET_OK;` |
|      403 | 13186 |  |
|        - | 13187 | `/*` |
|        - | 13188 | ` * Section:` |
|        - | 13189 | ` *  Version,Credits and Copyright related functions.` |
|        - | 13190 | ` * Status:` |
|        - | 13191 | ` *    Stable.` |
|        - | 13192 | ` */` |
|        - | 13193 | `/*` |
|        - | 13194 | ` * string ph7version(void)` |
|        - | 13195 | ` *  Returns the running version of the PH7 version.` |
|        - | 13196 | ` * Parameters` |
|        - | 13197 | ` *  None` |
|        - | 13198 | ` * Return` |
|        - | 13199 | ` * Current PH7 version.` |
|        - | 13200 | ` */` |
|        2 | 13201 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13202 |  |
|        1 | 13203 | `	SXUNUSED(nArg);` |
|        1 | 13204 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 13205 | `	/* Current engine version */` |
|        3 | 13206 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 13207 | `	return PH7_OK;` |
|        1 | 13208 |  |
|        - | 13209 | `/*` |
|        - | 13210 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 13211 | ` */` |
|        - | 13212 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 13213 | ` "<html><head>"\` |
|        - | 13214 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 13215 | ` "<style type=\"text/css\">"\` |
|        - | 13216 | ` "div {"\` |
|        - | 13217 | `     "border: 1px solid #cccccc;"\` |
|        - | 13218 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 13219 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 13220 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 13221 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 13222 | `     "-webkit-border-radius: 10px;"\` |
|        - | 13223 | `     "-o-border-radius: 10px;"\` |
|        - | 13224 | `     "border-radius: 10px;"\` |
|        - | 13225 | `     "padding-left: 2em;"\` |
|        - | 13226 | `     "background-color: white;"\` |
|        - | 13227 | `     "margin-left: auto;"\` |
|        - | 13228 | `     "font-family: verdana;"\` |
|        - | 13229 | `     "padding-right: 2em;"\` |
|        - | 13230 | `     "margin-right: auto;"\` |
|        - | 13231 | `     "}"\` |
|        - | 13232 | `     "body {"\` |
|        - | 13233 | `     "padding: 0.2em;"\` |
|        - | 13234 | `     "font-style: normal;"\` |
|        - | 13235 | `     "font-size: medium;"\` |
|        - | 13236 | `     "background-color: #f2f2f2;"\` |
|        - | 13237 | `     "}"\` |
|        - | 13238 | `     "hr {"\` |
|        - | 13239 | `     "border-style: solid none none;"\` |
|        - | 13240 | `     "border-width: 1px medium medium;"\` |
|        - | 13241 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 13242 | `     "height: 1px;"\` |
|        - | 13243 | `     "}"\` |
|        - | 13244 | `     "a {"\` |
|        - | 13245 | `     "color: #3366cc;"\` |
|        - | 13246 | `     "text-decoration: none;"\` |
|        - | 13247 | `     "}"\` |
|        - | 13248 | `     "a:hover {"\` |
|        - | 13249 | `     "color: #999999;"\` |
|        - | 13250 | `     "}"\` |
|        - | 13251 | `     "a:active {"\` |
|        - | 13252 | `     "color: #663399;"\` |
|        - | 13253 | `     "}"\` |
|        - | 13254 | `     "h1 {"\` |
|        - | 13255 | `     "margin: 0;"\` |
|        - | 13256 | `     "padding: 0;"\` |
|        - | 13257 | `     "font-family: Verdana;"\` |
|        - | 13258 | `     "font-weight: bold;"\` |
|        - | 13259 | `     "font-style: normal;"\` |
|        - | 13260 | `     "font-size: medium;"\` |
|        - | 13261 | `     "text-transform: capitalize;"\` |
|        - | 13262 | `     "color: #0a328c;"\` |
|        - | 13263 | `     "}"\` |
|        - | 13264 | `     "p {"\` |
|        - | 13265 | `     "margin: 0 auto;"\` |
|        - | 13266 | `     "font-size: medium;"\` |
|        - | 13267 | `     "font-style: normal;"\` |
|        - | 13268 | `     "font-family: verdana;"\` |
|        - | 13269 | `     "}"\` |
|        - | 13270 | `"</style></head><body>"\` |
|        - | 13271 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 13272 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 13273 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 13274 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 13275 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 13276 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 13277 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 13278 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 13279 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 13280 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 13281 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 13282 |  |
|        - | 13283 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13284 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 13285 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 13286 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 13287 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13288 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 13289 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 13290 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 13291 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 13292 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 13293 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13294 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 13295 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 13296 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 13297 |  |
|        - | 13298 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 13299 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 13300 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 13301 | `"&nbsp;*<br>"\` |
|        - | 13302 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 13303 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 13304 | `"&nbsp;* are met:<br>"\` |
|        - | 13305 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 13306 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 13307 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 13308 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 13309 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 13310 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 13311 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 13312 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 13313 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 13314 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 13315 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 13316 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 13317 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 13318 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 13319 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 13320 | `"&nbsp;*<br>"\` |
|        - | 13321 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 13322 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 13323 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 13324 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 13325 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 13326 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 13327 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 13328 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 13329 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 13330 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 13331 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 13332 | `"&nbsp;*/<br>"\` |
|        - | 13333 | `"</span></small></small></p>"\` |
|        - | 13334 | `"</div></body></html>"` |
|        - | 13335 | `/*` |
|        - | 13336 | ` * bool ph7credits(void)` |
|        - | 13337 | ` * bool ph7info(void)` |
|        - | 13338 | ` * bool ph7copyright(void)` |
|        - | 13339 | ` *  Prints out the credits for PH7 engine` |
|        - | 13340 | ` * Parameters` |
|        - | 13341 | ` *  None` |
|        - | 13342 | ` * Return` |
|        - | 13343 | ` *  Always TRUE` |
|        - | 13344 | ` */` |
|        2 | 13345 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13346 |  |
|        3 | 13347 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 13348 | `	/* Expand the HTML page above*/` |
|        3 | 13349 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 13350 | `	ph7_context_output_format(` |
|        1 | 13351 | `		pCtx,` |
|        - | 13352 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 13353 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 13354 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 13355 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 13356 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 13357 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 13358 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 13359 | `#ifdef __WINNT__` |
|        - | 13360 | `		"Windows NT"` |
|        - | 13361 | `#elif defined(__UNIXES__)` |
|        - | 13362 | `		"UNIX-Like"` |
|        - | 13363 | `#else` |
|        - | 13364 | `		"Other OS"` |
|        - | 13365 | `#endif` |
|        - | 13366 | `		);` |
|        3 | 13367 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 13368 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13369 | `	SXUNUSED(apArg);` |
|        - | 13370 | `	/* Return TRUE */` |
|        - | 13371 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 13372 | `	return PH7_OK;` |
|        1 | 13373 |  |
|        - | 13374 | `/*` |
|        - | 13375 | ` * Section:` |
|        - | 13376 | ` *    URL related routines.` |
|        - | 13377 | ` * Status:` |
|        - | 13378 | ` *    Stable.` |
|        - | 13379 | ` */` |
|        - | 13380 | `/*` |
|        - | 13381 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 13382 | ` *  Parse a URL and return its fields.` |
|        - | 13383 | ` * Parameters` |
|        - | 13384 | ` *  $url` |
|        - | 13385 | ` *   The URL to parse.` |
|        - | 13386 | ` * $component` |
|        - | 13387 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 13388 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 13389 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 13390 | ` *  in which case the return value will be an integer).` |
|        - | 13391 | ` * Return` |
|        - | 13392 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 13393 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 13394 | ` *  this array are:` |
|        - | 13395 | ` *   scheme - e.g. http` |
|        - | 13396 | ` *   host` |
|        - | 13397 | ` *   port` |
|        - | 13398 | ` *   user` |
|        - | 13399 | ` *   pass` |
|        - | 13400 | ` *   path` |
|        - | 13401 | ` *   query - after the question mark ?` |
|        - | 13402 | ` *   fragment - after the hashmark #` |
|        - | 13403 | ` * Note:` |
|        - | 13404 | ` *  FALSE is returned on failure.` |
|        - | 13405 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 13406 | ` *  with the standard PHP engine.` |
|        - | 13407 | ` */` |
|       28 | 13408 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13409 |  |
|        - | 13410 | `	const char *zStr; /* Input string */` |
|        - | 13411 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 13412 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 13413 | `	int nLen;` |
|        - | 13414 | `	sxi32 rc;` |
|       29 | 13415 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 13416 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 13417 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13418 | `		return PH7_OK;` |
|        - | 13419 | `	}` |
|        - | 13420 | `	/* Extract the given URI */` |
|       29 | 13421 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 13422 | `	if( nLen < 1 ){` |
|        - | 13423 | `		/* Nothing to process,return FALSE */` |
|        3 | 13424 | `		ph7_result_bool(pCtx,0);` |
|        3 | 13425 | `		return PH7_OK;` |
|        - | 13426 | `	}` |
|        - | 13427 | `	/* Get a parse */` |
|       27 | 13428 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 13429 | `	if( rc != SXRET_OK ){` |
|        - | 13430 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 13431 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13432 | `		return PH7_OK;` |
|        - | 13433 | `	}` |
|       27 | 13434 | `	if( nArg > 1 ){` |
|      ! 0 | 13435 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 13436 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 13437 | `		switch(nComponent){` |
|      ! 0 | 13438 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 13439 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 13440 | `			if( pComp->nByte < 1 ){` |
|        - | 13441 | `				/* No available value,return NULL */` |
|      ! 0 | 13442 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13443 | `			}else{` |
|      ! 0 | 13444 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13445 | `			}` |
|      ! 0 | 13446 | `			break;` |
|      ! 0 | 13447 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 13448 | `			pComp = &sURI.sHost;` |
|      ! 0 | 13449 | `			if( pComp->nByte < 1 ){` |
|        - | 13450 | `				/* No available value,return NULL */` |
|      ! 0 | 13451 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13452 | `			}else{` |
|      ! 0 | 13453 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13454 | `			}` |
|      ! 0 | 13455 | `			break;` |
|      ! 0 | 13456 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 13457 | `			pComp = &sURI.sPort;` |
|      ! 0 | 13458 | `			if( pComp->nByte < 1 ){` |
|        - | 13459 | `				/* No available value,return NULL */` |
|      ! 0 | 13460 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13461 | `			}else{` |
|      ! 0 | 13462 | `				int iPort = 0;` |
|        - | 13463 | `				/* Cast the value to integer */` |
|      ! 0 | 13464 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 13465 | `				ph7_result_int(pCtx,iPort);` |
|        - | 13466 | `			}` |
|      ! 0 | 13467 | `			break;` |
|      ! 0 | 13468 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 13469 | `			pComp = &sURI.sUser;` |
|      ! 0 | 13470 | `			if( pComp->nByte < 1 ){` |
|        - | 13471 | `				/* No available value,return NULL */` |
|      ! 0 | 13472 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13473 | `			}else{` |
|      ! 0 | 13474 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13475 | `			}` |
|      ! 0 | 13476 | `			break;` |
|      ! 0 | 13477 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 13478 | `			pComp = &sURI.sPass;` |
|      ! 0 | 13479 | `			if( pComp->nByte < 1 ){` |
|        - | 13480 | `				/* No available value,return NULL */` |
|      ! 0 | 13481 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13482 | `			}else{` |
|      ! 0 | 13483 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13484 | `			}` |
|      ! 0 | 13485 | `			break;` |
|      ! 0 | 13486 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 13487 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 13488 | `			if( pComp->nByte < 1 ){` |
|        - | 13489 | `				/* No available value,return NULL */` |
|      ! 0 | 13490 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13491 | `			}else{` |
|      ! 0 | 13492 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13493 | `			}` |
|      ! 0 | 13494 | `			break;` |
|      ! 0 | 13495 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 13496 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 13497 | `			if( pComp->nByte < 1 ){` |
|        - | 13498 | `				/* No available value,return NULL */` |
|      ! 0 | 13499 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13500 | `			}else{` |
|      ! 0 | 13501 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13502 | `			}` |
|      ! 0 | 13503 | `			break;` |
|      ! 0 | 13504 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 13505 | `			pComp = &sURI.sPath;` |
|      ! 0 | 13506 | `			if( pComp->nByte < 1 ){` |
|        - | 13507 | `				/* No available value,return NULL */` |
|      ! 0 | 13508 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13509 | `			}else{` |
|      ! 0 | 13510 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13511 | `			}` |
|      ! 0 | 13512 | `			break;` |
|      ! 0 | 13513 | `		default:` |
|        - | 13514 | `			/* No such entry,return NULL */` |
|      ! 0 | 13515 | `			ph7_result_null(pCtx);` |
|      ! 0 | 13516 | `			break;` |
|        - | 13517 | `		}` |
|      ! 0 | 13518 | `	}else{` |
|        - | 13519 | `		ph7_value *pArray,*pValue;` |
|        - | 13520 | `		/* Return an associative array */` |
|       27 | 13521 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 13522 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 13523 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 13524 | `			/* Out of memory */` |
|      ! 0 | 13525 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 13526 | `			/* Return false */` |
|      ! 0 | 13527 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 13528 | `			return PH7_OK;` |
|        - | 13529 | `		}` |
|        - | 13530 | `		/* Fill the array */` |
|       27 | 13531 | `		pComp = &sURI.sScheme;` |
|       27 | 13532 | `		if( pComp->nByte > 0 ){` |
|       19 | 13533 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 13534 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 13535 | `		}` |
|        - | 13536 | `		/* Reset the string cursor */` |
|       27 | 13537 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13538 | `		pComp = &sURI.sHost;` |
|       27 | 13539 | `		if( pComp->nByte > 0 ){` |
|       25 | 13540 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 13541 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 13542 | `		}` |
|        - | 13543 | `		/* Reset the string cursor */` |
|       27 | 13544 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13545 | `		pComp = &sURI.sPort;` |
|       27 | 13546 | `		if( pComp->nByte > 0 ){` |
|       11 | 13547 | `			int iPort = 0;/* cc warning */` |
|        - | 13548 | `			/* Convert to integer */` |
|       11 | 13549 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 13550 | `			ph7_value_int(pValue,iPort);` |
|       11 | 13551 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 13552 | `		}` |
|        - | 13553 | `		/* Reset the string cursor */` |
|       27 | 13554 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13555 | `		pComp = &sURI.sUser;` |
|       27 | 13556 | `		if( pComp->nByte > 0 ){` |
|        7 | 13557 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 13558 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 13559 | `		}` |
|        - | 13560 | `		/* Reset the string cursor */` |
|       27 | 13561 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13562 | `		pComp = &sURI.sPass;` |
|       27 | 13563 | `		if( pComp->nByte > 0 ){` |
|        7 | 13564 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 13565 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 13566 | `		}` |
|        - | 13567 | `		/* Reset the string cursor */` |
|       27 | 13568 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13569 | `		pComp = &sURI.sPath;` |
|       27 | 13570 | `		if( pComp->nByte > 0 ){` |
|       17 | 13571 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 13572 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 13573 | `		}` |
|        - | 13574 | `		/* Reset the string cursor */` |
|       27 | 13575 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13576 | `		pComp = &sURI.sQuery;` |
|       27 | 13577 | `		if( pComp->nByte > 0 ){` |
|        5 | 13578 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 13579 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 13580 | `		}` |
|        - | 13581 | `		/* Reset the string cursor */` |
|       27 | 13582 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13583 | `		pComp = &sURI.sFragment;` |
|       27 | 13584 | `		if( pComp->nByte > 0 ){` |
|        5 | 13585 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 13586 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 13587 | `		}` |
|        - | 13588 | `		/* Return the created array */` |
|       27 | 13589 | `		ph7_result_value(pCtx,pArray);` |
|        - | 13590 | `		/* NOTE:` |
|        - | 13591 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 13592 | `		 * automatically as soon we return from this function.` |
|        - | 13593 | `		 */` |
|        - | 13594 | `	}` |
|        - | 13595 | `	/* All done */` |
|       27 | 13596 | `	return PH7_OK;` |
|       15 | 13597 |  |
|        - | 13598 | `/*` |
|        - | 13599 | ` * Section:` |
|        - | 13600 | ` *   Array related routines.` |
|        - | 13601 | ` * Status:` |
|        - | 13602 | ` *    Stable.` |
|        - | 13603 | ` * Note 2012-5-21 01:04:15:` |
|        - | 13604 | ` *  Array related functions that need access to the underlying` |
|        - | 13605 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 13606 | ` */` |
|        - | 13607 | `/*` |
|        - | 13608 | ` * The [compact()] function store it's state information in an instance` |
|        - | 13609 | ` * of the following structure.` |
|        - | 13610 | ` */` |
|        - | 13611 | `struct compact_data` |
|        - | 13612 |  |
|        - | 13613 | `	ph7_value *pArray;  /* Target array */` |
|        - | 13614 | `	int nRecCount;      /* Recursion count */` |
|        - | 13615 | `};` |
|        - | 13616 | `/*` |
|        - | 13617 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 13618 | ` */` |
|      ! 0 | 13619 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 13620 |  |
|      ! 0 | 13621 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 13622 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 13623 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 13624 | `	/* Act according to the hashmap value */` |
|      ! 0 | 13625 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 13626 | `		SyString sVar;` |
|      ! 0 | 13627 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 13628 | `		if( sVar.nByte > 0 ){` |
|        - | 13629 | `			/* Query the current frame */` |
|      ! 0 | 13630 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 13631 | `			/* ^` |
|        - | 13632 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 13633 | `			 */` |
|      ! 0 | 13634 | `			if( pKey ){` |
|        - | 13635 | `				/* Perform the insertion */` |
|      ! 0 | 13636 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 13637 | `			}` |
|      ! 0 | 13638 | `		}` |
|      ! 0 | 13639 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 13640 | `		int rc;` |
|        - | 13641 | `		/* Recursively traverse this array */` |
|      ! 0 | 13642 | `		pData->nRecCount++;` |
|      ! 0 | 13643 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 13644 | `		pData->nRecCount--;` |
|      ! 0 | 13645 | `		return rc;` |
|        - | 13646 | `	}` |
|      ! 0 | 13647 | `	return SXRET_OK;` |
|      ! 0 | 13648 |  |
|        - | 13649 | `/*` |
|        - | 13650 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 13651 | ` *  Create array containing variables and their values.` |
|        - | 13652 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 13653 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 13654 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 13655 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 13656 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 13657 | ` * Parameters` |
|        - | 13658 | ` *  $varname` |
|        - | 13659 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 13660 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 13661 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 13662 | ` *   it recursively.` |
|        - | 13663 | ` * Return` |
|        - | 13664 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 13665 | ` */` |
|        2 | 13666 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13667 |  |
|        - | 13668 | `	ph7_value *pArray,*pObj;` |
|        3 | 13669 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13670 | `	const char *zName;` |
|        - | 13671 | `	SyString sVar;` |
|        - | 13672 | `	int i,nLen;` |
|        3 | 13673 | `	if( nArg < 1 ){` |
|        - | 13674 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 13675 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13676 | `		return PH7_OK;` |
|        - | 13677 | `	}` |
|        - | 13678 | `	/* Create the array */` |
|        3 | 13679 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 13680 | `	if( pArray == 0 ){` |
|        - | 13681 | `		/* Out of memory */` |
|      ! 0 | 13682 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 13683 | `		/* Return NULL */` |
|      ! 0 | 13684 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13685 | `		return PH7_OK;` |
|        - | 13686 | `	}` |
|        - | 13687 | `	/* Perform the requested operation */` |
|        7 | 13688 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 13689 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 13690 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 13691 | `				struct compact_data sData;` |
|      ! 0 | 13692 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 13693 | `				/* Recursively walk the array */` |
|      ! 0 | 13694 | `				sData.nRecCount = 0;` |
|      ! 0 | 13695 | `				sData.pArray = pArray;` |
|      ! 0 | 13696 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 13697 | `			}` |
|      ! 0 | 13698 | `		}else{` |
|        - | 13699 | `			/* Extract variable name */` |
|        5 | 13700 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 13701 | `			if( nLen > 0 ){` |
|        5 | 13702 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 13703 | `				/* Check if the variable is available in the current frame */` |
|        5 | 13704 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 13705 | `				if( pObj ){` |
|        5 | 13706 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 13707 | `				}` |
|        2 | 13708 | `			}` |
|        - | 13709 | `		}` |
|        3 | 13710 | `	}` |
|        - | 13711 | `	/* Return the array */` |
|        3 | 13712 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 13713 | `	return PH7_OK;` |
|        2 | 13714 |  |
|        - | 13715 | `/*` |
|        - | 13716 | ` * The [extract()] function store it's state information in an instance` |
|        - | 13717 | ` * of the following structure.` |
|        - | 13718 | ` */` |
|        - | 13719 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 13720 | `struct extract_aux_data` |
|        - | 13721 |  |
|        - | 13722 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 13723 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 13724 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 13725 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 13726 | `	int iFlags;           /* Control flags */` |
|        - | 13727 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 13728 | `};` |
|        - | 13729 | `/* Forward declaration */` |
|        - | 13730 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 13731 | `/*` |
|        - | 13732 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 13733 | ` *   Import variables into the current symbol table from an array.` |
|        - | 13734 | ` * Parameters` |
|        - | 13735 | ` * $var_array` |
|        - | 13736 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 13737 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 13738 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 13739 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 13740 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 13741 | ` * $extract_type` |
|        - | 13742 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 13743 | ` *  It can be one of the following values:` |
|        - | 13744 | ` *   EXTR_OVERWRITE` |
|        - | 13745 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 13746 | ` *   EXTR_SKIP` |
|        - | 13747 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 13748 | ` *   EXTR_PREFIX_SAME` |
|        - | 13749 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 13750 | ` *   EXTR_PREFIX_ALL` |
|        - | 13751 | ` *       Prefix all variable names with prefix.` |
|        - | 13752 | ` *   EXTR_PREFIX_INVALID` |
|        - | 13753 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 13754 | ` *   EXTR_IF_EXISTS` |
|        - | 13755 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 13756 | ` *       otherwise do nothing.` |
|        - | 13757 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 13758 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 13759 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 13760 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 13761 | ` *      the current symbol table.` |
|        - | 13762 | ` * $prefix` |
|        - | 13763 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 13764 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 13765 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 13766 | ` *  underscore character.` |
|        - | 13767 | ` * Return` |
|        - | 13768 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 13769 | ` */` |
|        4 | 13770 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13771 |  |
|        - | 13772 | `	extract_aux_data sAux;` |
|        - | 13773 | `	ph7_hashmap *pMap;` |
|        5 | 13774 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 13775 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 13776 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 13777 | `		return PH7_OK;` |
|        - | 13778 | `	}` |
|        - | 13779 | `	/* Point to the target hashmap */` |
|        5 | 13780 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 13781 | `	if( pMap->nEntry < 1 ){` |
|        - | 13782 | `		/* Empty map,return  0 */` |
|      ! 0 | 13783 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 13784 | `		return PH7_OK;` |
|        - | 13785 | `	}` |
|        - | 13786 | `	/* Prepare the aux data */` |
|        5 | 13787 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 13788 | `	if( nArg > 1 ){` |
|        3 | 13789 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 13790 | `		if( nArg > 2 ){` |
|      ! 0 | 13791 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 13792 | `		}` |
|        1 | 13793 | `	}` |
|        5 | 13794 | `	sAux.pVm = pCtx->pVm;` |
|        - | 13795 | `	/* Invoke the worker callback */` |
|        5 | 13796 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 13797 | `	/* Number of variables successfully imported */` |
|        5 | 13798 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 13799 | `	return PH7_OK;` |
|        3 | 13800 |  |
|        - | 13801 | `/*` |
|        - | 13802 | ` * Worker callback for the [extract()] function defined` |
|        - | 13803 | ` * below.` |
|        - | 13804 | ` */` |
|        8 | 13805 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 13806 |  |
|        9 | 13807 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 13808 | `	int iFlags = pAux->iFlags;` |
|        9 | 13809 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 13810 | `	ph7_value *pObj;` |
|        - | 13811 | `	SyString sVar;` |
|        9 | 13812 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 13813 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 13814 | `	}` |
|        - | 13815 | `	/* Perform a string cast */` |
|        9 | 13816 | `	PH7_MemObjToString(pKey);` |
|        9 | 13817 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 13818 | `		/* Unavailable variable name */` |
|      ! 0 | 13819 | `		return SXRET_OK;` |
|        - | 13820 | `	}` |
|        9 | 13821 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 13822 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 13823 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 13824 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 13825 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 13826 | `			);` |
|      ! 0 | 13827 | `	}else{` |
|       13 | 13828 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 13829 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 13830 | `	}` |
|        9 | 13831 | `	sVar.zString = pAux->zWorker;` |
|        - | 13832 | `	/* Try to extract the variable */` |
|        9 | 13833 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 13834 | `	if( pObj ){` |
|        - | 13835 | `		/* Collision */` |
|        5 | 13836 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 13837 | `			return SXRET_OK;` |
|        - | 13838 | `		}` |
|        5 | 13839 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 13840 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 13841 | `				/* Already prefixed */` |
|      ! 0 | 13842 | `				return SXRET_OK;` |
|        - | 13843 | `			}` |
|      ! 0 | 13844 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 13845 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 13846 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 13847 | `				);` |
|      ! 0 | 13848 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 13849 | `		}` |
|        3 | 13850 | `	}else{` |
|        - | 13851 | `		/* Create the variable */` |
|        5 | 13852 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 13853 | `	}` |
|        9 | 13854 | `	if( pObj ){` |
|        - | 13855 | `		/* Overwrite the old value */` |
|        9 | 13856 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 13857 | `		/* Increment counter */` |
|        9 | 13858 | `		pAux->iCount++;` |
|        4 | 13859 | `	}` |
|        9 | 13860 | `	return SXRET_OK;` |
|        5 | 13861 |  |
|        - | 13862 | `/*` |
|        - | 13863 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 13864 | ` * defined below.` |
|        - | 13865 | ` */` |
|        2 | 13866 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 13867 |  |
|        3 | 13868 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 13869 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 13870 | `	ph7_value *pObj;` |
|        - | 13871 | `	SyString sVar;` |
|        - | 13872 | `	/* Perform a string cast */` |
|        3 | 13873 | `	PH7_MemObjToString(pKey);` |
|        3 | 13874 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 13875 | `		/* Unavailable variable name */` |
|      ! 0 | 13876 | `		return SXRET_OK;` |
|        - | 13877 | `	}` |
|        3 | 13878 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 13879 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 13880 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 13881 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 13882 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 13883 | `			);` |
|        2 | 13884 | `	}else{` |
|      ! 0 | 13885 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 13886 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 13887 | `	}` |
|        3 | 13888 | `	sVar.zString = pAux->zWorker;` |
|        - | 13889 | `	/* Extract the variable */` |
|        3 | 13890 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 13891 | `	if( pObj ){` |
|        3 | 13892 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 13893 | `	}` |
|        3 | 13894 | `	return SXRET_OK;` |
|        2 | 13895 |  |
|        - | 13896 | `/*` |
|        - | 13897 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 13898 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 13899 | ` * Parameters` |
|        - | 13900 | ` * $types` |
|        - | 13901 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 13902 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 13903 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 13904 | ` *  POST includes the POST uploaded file information.` |
|        - | 13905 | ` *  Note:` |
|        - | 13906 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 13907 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 13908 | ` * $prefix` |
|        - | 13909 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 13910 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 13911 | ` *  variable named $pref_userid.` |
|        - | 13912 | ` * Return` |
|        - | 13913 | ` *  TRUE on success or FALSE on failure.` |
|        - | 13914 | ` */` |
|        2 | 13915 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13916 |  |
|        - | 13917 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 13918 | `	extract_aux_data sAux;` |
|        - | 13919 | `	int nLen,nPrefixLen;` |
|        - | 13920 | `	ph7_value *pSuper;` |
|        - | 13921 | `	ph7_vm *pVm;` |
|        - | 13922 | `	/* By default import only $_GET variables  */` |
|        3 | 13923 | `	zImport = "G";` |
|        3 | 13924 | `	nLen = (int)sizeof(char);` |
|        3 | 13925 | `	zPrefix = 0;` |
|        3 | 13926 | `	nPrefixLen = 0;` |
|        3 | 13927 | `	if( nArg > 0 ){` |
|        3 | 13928 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 13929 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 13930 | `		}` |
|        3 | 13931 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 13932 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 13933 | `		}` |
|        1 | 13934 | `	}` |
|        - | 13935 | `	/* Point to the underlying VM */` |
|        3 | 13936 | `	pVm = pCtx->pVm;` |
|        - | 13937 | `	/* Initialize the aux data */` |
|        3 | 13938 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 13939 | `	sAux.zPrefix = zPrefix;` |
|        3 | 13940 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 13941 | `	sAux.pVm = pVm;` |
|        - | 13942 | `	/* Extract */` |
|        3 | 13943 | `	zEnd = &zImport[nLen];` |
|        5 | 13944 | `	while( zImport < zEnd ){` |
|        3 | 13945 | `		int c = zImport[0];` |
|        3 | 13946 | `		pSuper = 0;` |
|        3 | 13947 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 13948 | `			/* Import $_GET variables */` |
|        3 | 13949 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 13950 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 13951 | `			/* Import $_POST variables */` |
|      ! 0 | 13952 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 13953 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 13954 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 13955 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 13956 | `		}` |
|        3 | 13957 | `		if( pSuper ){` |
|        - | 13958 | `			/* Iterate throw array entries */` |
|        3 | 13959 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 13960 | `		}` |
|        - | 13961 | `		/* Advance the cursor */` |
|        3 | 13962 | `		zImport++;` |
|        1 | 13963 | `	}` |
|        - | 13964 | `	/* All done,return TRUE*/` |
|        3 | 13965 | `	ph7_result_bool(pCtx,0);` |
|        3 | 13966 | `	return PH7_OK;` |
|        1 | 13967 |  |
|        - | 13968 | `/*` |
|        - | 13969 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 13970 | ` * Refer to the eval() language construct implementation for more` |
|        - | 13971 | ` * information.` |
|        - | 13972 | ` */` |
|    12338 | 13973 | `static sxi32 VmEvalChunk(` |
|        - | 13974 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 13975 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 13976 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 13977 | `	int iFlags,         /* Compile flag */` |
|        - | 13978 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 13979 | `	)` |
|        2 | 13980 |  |
|        - | 13981 | `	SySet *pByteCode,aByteCode;` |
|        - | 13982 | `	SyBlob sSavedNs;` |
|    12340 | 13983 | `	ProcConsumer xErr = 0;` |
|    12340 | 13984 | `	void *pErrData = 0;` |
|        - | 13985 | `	/* Initialize bytecode container */` |
|    12340 | 13986 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    12340 | 13987 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 13988 | `	/* Reset the code generator */` |
|    12340 | 13989 | `	if( bTrueReturn ){` |
|        - | 13990 | `		/* Included file,log compile-time errors */` |
|     9314 | 13991 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9314 | 13992 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4656 | 13993 | `	}` |
|    12340 | 13994 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 13995 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 13996 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 13997 | `	 * the caller's namespace is restored. */` |
|    12340 | 13998 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    12340 | 13999 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    12340 | 14000 | `	if( bTrueReturn ){` |
|        - | 14001 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9314 | 14002 | `		SyBlobReset(&pVm->sNamespace);` |
|     4656 | 14003 | `	}` |
|        - | 14004 | `	/* Swap bytecode container */` |
|    12340 | 14005 | `	pByteCode = pVm->pByteContainer;` |
|    12340 | 14006 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 14007 | `	/* Compile the chunk */` |
|    12340 | 14008 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    18509 | 14009 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 14010 | `		/* Compilation error,return false */` |
|        3 | 14011 | `		if( pCtx ){` |
|        3 | 14012 | `			ph7_result_bool(pCtx,0);` |
|        1 | 14013 | `		}` |
|        2 | 14014 | `	}else{` |
|        - | 14015 | `		/* Mount any newly defined classes */` |
|        - | 14016 | `		SyHashEntry *pEntry;` |
|        - | 14017 | `		ph7_class *pClass;` |
|        - | 14018 | `		ph7_value sResult; /* Return value */` |
|        - | 14019 | `		sxi32 rc;` |
|    12338 | 14020 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   636128 | 14021 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   617624 | 14022 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 14023 | `			/* Only mount classes that haven't been mounted yet */` |
|   617624 | 14024 | `			if( !pClass->bMounted ){` |
|   111924 | 14025 | `				rc = VmMountUserClass(pVm,pClass);` |
|   111924 | 14026 | `				if( rc != SXRET_OK ){` |
|        - | 14027 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 14028 | `					if( pCtx ){` |
|      ! 0 | 14029 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 14030 | `					}` |
|      ! 0 | 14031 | `					goto Cleanup;` |
|        - | 14032 | `				}` |
|    55961 | 14033 | `			}` |
|        2 | 14034 | `		}` |
|    12338 | 14035 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 14036 | `			/* Out of memory */` |
|      ! 0 | 14037 | `			if( pCtx ){` |
|      ! 0 | 14038 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 14039 | `			}` |
|      ! 0 | 14040 | `			goto Cleanup;` |
|        - | 14041 | `		}` |
|    12338 | 14042 | `		if( bTrueReturn ){` |
|        - | 14043 | `			/* Assume a boolean true return value */` |
|     9314 | 14044 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4658 | 14045 | `		}else{` |
|        - | 14046 | `			/* Assume a null return value */` |
|     3026 | 14047 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 14048 | `		}` |
|        - | 14049 | `		/* Execute the compiled chunk */` |
|    12338 | 14050 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    12338 | 14051 | `		if( pCtx ){` |
|        - | 14052 | `			/* Set the execution result */` |
|     9332 | 14053 | `			ph7_result_value(pCtx,&sResult);` |
|     4665 | 14054 | `		}` |
|    12338 | 14055 | `		PH7_MemObjRelease(&sResult);` |
|        - | 14056 | `	}` |
|     6169 | 14057 | `Cleanup:` |
|        - | 14058 | `	/* Cleanup the mess left behind */` |
|    12340 | 14059 | `	pVm->pByteContainer = pByteCode;` |
|    12340 | 14060 | `	SySetRelease(&aByteCode);` |
|        - | 14061 | `	/* Restore caller's namespace state */` |
|    12340 | 14062 | `	SyBlobReset(&pVm->sNamespace);` |
|    12340 | 14063 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    12340 | 14064 | `	SyBlobRelease(&sSavedNs);` |
|    12340 | 14065 | `	return SXRET_OK;` |
|        2 | 14066 |  |
|        - | 14067 | `/*` |
|        - | 14068 | ` * value eval(string $code)` |
|        - | 14069 | ` *   Evaluate a string as PHP code.` |
|        - | 14070 | ` * Parameter` |
|        - | 14071 | ` *  code: PHP code to evaluate.` |
|        - | 14072 | ` * Return` |
|        - | 14073 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 14074 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 14075 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 14076 | ` */` |
|       22 | 14077 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14078 |  |
|        - | 14079 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 14080 | `	if( nArg < 1 ){` |
|        - | 14081 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14082 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14083 | `		return SXRET_OK;` |
|        - | 14084 | `	}` |
|        - | 14085 | `	/* Chunk to evaluate */` |
|       24 | 14086 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 14087 | `	if( sChunk.nByte < 1 ){` |
|        - | 14088 | `		/* Empty string,return NULL */` |
|        3 | 14089 | `		ph7_result_null(pCtx);` |
|        3 | 14090 | `		return SXRET_OK;` |
|        - | 14091 | `	}` |
|        - | 14092 | `	/* Eval the chunk */` |
|       22 | 14093 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 14094 | `	return SXRET_OK;` |
|       13 | 14095 |  |
|        - | 14096 | `/*` |
|        - | 14097 | ` * Check if a file path is already included.` |
|        - | 14098 | ` */` |
|    18620 | 14099 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 14100 |  |
|        - | 14101 | `	SyString *aEntries;` |
|        - | 14102 | `	sxu32 n;` |
|    18622 | 14103 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 14104 | `	/* Perform a linear search */` |
| 86621146 | 14105 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 86602532 | 14106 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 14107 | `			/* Already included */` |
|        7 | 14108 | `			return TRUE;` |
|        - | 14109 | `		}` |
| 43301264 | 14110 | `	}` |
|    18616 | 14111 | `	return FALSE;` |
|     9312 | 14112 |  |
|        - | 14113 | `/*` |
|        - | 14114 | ` * Push a file path in the appropriate VM container.` |
|        - | 14115 | ` */` |
|    21618 | 14116 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 14117 |  |
|        - | 14118 | `	SyString sPath;` |
|        - | 14119 | `	char *zDup;` |
|        - | 14120 | `#ifdef __WINNT__` |
|        - | 14121 | `	char *zCur;` |
|        - | 14122 | `#endif` |
|        - | 14123 | `	sxi32 rc;` |
|    21620 | 14124 | `	if( nLen < 0 ){` |
|     3000 | 14125 | `		nLen = SyStrlen(zPath);` |
|     1499 | 14126 | `	}` |
|        - | 14127 | `	/* Duplicate the file path first */` |
|    21620 | 14128 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    21620 | 14129 | `	if( zDup == 0 ){` |
|      ! 0 | 14130 | `		return SXERR_MEM;` |
|        - | 14131 | `	}` |
|        - | 14132 | `#ifdef __WINNT__` |
|        - | 14133 | `	/* Normalize path on windows` |
|        - | 14134 | `	 * Example:` |
|        - | 14135 | `	 *    Path/To/File.php` |
|        - | 14136 | `	 * becomes` |
|        - | 14137 | `	 *   path\to\file.php` |
|        - | 14138 | `	 */` |
|        2 | 14139 | `	zCur = zDup;` |
|        2 | 14140 | `	while( zCur[0] != 0 ){` |
|        2 | 14141 | `		if( zCur[0] == '/' ){` |
|        2 | 14142 | `			zCur[0] = '\\';` |
|        2 | 14143 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 14144 | `			int c = SyToLower(zCur[0]);` |
|        1 | 14145 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 14146 | `		}` |
|        2 | 14147 | `		zCur++;` |
|        2 | 14148 | `	}` |
|        - | 14149 | `#endif` |
|        - | 14150 | `	/* Install the file path */` |
|    21620 | 14151 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    21620 | 14152 | `	if( !bMain ){` |
|    18622 | 14153 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 14154 | `			/* Already included */` |
|        7 | 14155 | `			*pNew = 0;` |
|        4 | 14156 | `		}else{` |
|        - | 14157 | `			/* Insert in the corresponding container */` |
|    18616 | 14158 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    18616 | 14159 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 14160 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 14161 | `				return rc;` |
|        - | 14162 | `			}` |
|    18616 | 14163 | `			*pNew = 1;` |
|        - | 14164 | `		}` |
|     9310 | 14165 | `	}` |
|    21620 | 14166 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    21620 | 14167 | `	return SXRET_OK;` |
|    10811 | 14168 |  |
|        - | 14169 | `/*` |
|        - | 14170 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 14171 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 14172 | ` * indicates failure.` |
|        - | 14173 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 14174 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 14175 | ` * operations.` |
|        - | 14176 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 14177 | ` * this function is a no-op.` |
|        - | 14178 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 14179 | ` * constructs for more information.` |
|        - | 14180 | ` */` |
|     9322 | 14181 | `static sxi32 VmExecIncludedFile(` |
|        - | 14182 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 14183 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 14184 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 14185 | `	 )` |
|        2 | 14186 |  |
|        - | 14187 | `	sxi32 rc;` |
|        - | 14188 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14189 | `	const ph7_io_stream *pStream;` |
|        - | 14190 | `	SyBlob sContents;` |
|        - | 14191 | `	void *pHandle;` |
|        - | 14192 | `	ph7_vm *pVm;` |
|        - | 14193 | `	int isNew;` |
|        - | 14194 | `	/* Initialize fields */` |
|     9324 | 14195 | `	pVm = pCtx->pVm;` |
|     9324 | 14196 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9324 | 14197 | `	isNew = 0;` |
|        - | 14198 | `	/* Extract the associated stream */` |
|     9324 | 14199 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 14200 | `	/*` |
|        - | 14201 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 14202 | `	 * in a read-only mode.` |
|        - | 14203 | `	 */` |
|     9324 | 14204 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9324 | 14205 | `	if( pHandle == 0 ){` |
|        8 | 14206 | `		return SXERR_IO;` |
|        - | 14207 | `	}` |
|     9318 | 14208 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9318 | 14209 | `	if( IncludeOnce && !isNew ){` |
|        - | 14210 | `		/* Already included */` |
|        5 | 14211 | `		rc = SXERR_EXISTS;` |
|        3 | 14212 | `	}else{` |
|        - | 14213 | `		/* Read the whole file contents */` |
|     9314 | 14214 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9314 | 14215 | `		if( rc == SXRET_OK ){` |
|        - | 14216 | `			SyString sScript;` |
|        - | 14217 | `			/* Compile and execute the script */` |
|     9314 | 14218 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9314 | 14219 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4656 | 14220 | `		}` |
|        - | 14221 | `	}` |
|        - | 14222 | `	/* Pop from the set of included file */` |
|     9318 | 14223 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 14224 | `	/* Close the handle */` |
|     9318 | 14225 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 14226 | `	/* Release the working buffer */` |
|     9318 | 14227 | `	SyBlobRelease(&sContents);` |
|        - | 14228 | `#else` |
|        - | 14229 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 14230 | `	SXUNUSED(pPath);` |
|        - | 14231 | `	SXUNUSED(IncludeOnce);` |
|        - | 14232 | `	rc = SXERR_IO;` |
|        - | 14233 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9318 | 14234 | `	return rc;` |
|     4663 | 14235 |  |
|        - | 14236 | `/*` |
|        - | 14237 | ` * string get_include_path(void)` |
|        - | 14238 | ` *  Gets the current include_path configuration option.` |
|        - | 14239 | ` * Parameter` |
|        - | 14240 | ` *  None` |
|        - | 14241 | ` * Return` |
|        - | 14242 | ` *  Included paths as a string` |
|        - | 14243 | ` */` |
|        2 | 14244 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14245 |  |
|        3 | 14246 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14247 | `	SyString *aEntry;` |
|        - | 14248 | `	int dir_sep;` |
|        - | 14249 | `	sxu32 n;` |
|        - | 14250 | `#ifdef __WINNT__` |
|        1 | 14251 | `	dir_sep = ';';` |
|        - | 14252 | `#else` |
|        - | 14253 | `	/* Assume UNIX path separator */` |
|        2 | 14254 | `	dir_sep = ':';` |
|        - | 14255 | `#endif` |
|        1 | 14256 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14257 | `	SXUNUSED(apArg);` |
|        - | 14258 | `	/* Point to the list of import paths */` |
|        3 | 14259 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 14260 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 14261 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 14262 | `		if( n > 0 ){` |
|        - | 14263 | `			/* Append dir seprator */` |
|      ! 0 | 14264 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 14265 | `		}` |
|        - | 14266 | `		/* Append path */` |
|        3 | 14267 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 14268 | `	}` |
|        3 | 14269 | `	return PH7_OK;` |
|        1 | 14270 |  |
|        - | 14271 | `/*` |
|        - | 14272 | ` * string get_get_included_files(void)` |
|        - | 14273 | ` *  Gets the current include_path configuration option.` |
|        - | 14274 | ` * Parameter` |
|        - | 14275 | ` *  None` |
|        - | 14276 | ` * Return` |
|        - | 14277 | ` *  Included paths as a string` |
|        - | 14278 | ` */` |
|        2 | 14279 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14280 |  |
|        3 | 14281 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 14282 | `	ph7_value *pArray,*pWorker;` |
|        - | 14283 | `	SyString *pEntry;` |
|        - | 14284 | `	int c,d;` |
|        - | 14285 | `	/* Create an array and a working value */` |
|        3 | 14286 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 14287 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 14288 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 14289 | `		/* Out of memory,return null */` |
|      ! 0 | 14290 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14291 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 14292 | `		SXUNUSED(apArg);` |
|      ! 0 | 14293 | `		return PH7_OK;` |
|        - | 14294 | `	}` |
|        3 | 14295 | `	c = d = '/';` |
|        - | 14296 | `#ifdef __WINNT__` |
|        1 | 14297 | `	d = '\\';` |
|        - | 14298 | `#endif` |
|        - | 14299 | `	/* Iterate throw entries */` |
|        3 | 14300 | `	SySetResetCursor(pFiles);` |
|     3839 | 14301 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 14302 | `		const char *zBase,*zEnd;` |
|        - | 14303 | `		int iLen;` |
|        - | 14304 | `		/* reset the string cursor */` |
|     3837 | 14305 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 14306 | `		/* Extract base name */` |
|     3837 | 14307 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 14308 | `		/* Ignore trailing '/' */` |
|     5755 | 14309 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 14310 | `			zEnd--;` |
|      ! 0 | 14311 | `		}` |
|     3837 | 14312 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   118297 | 14313 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   112543 | 14314 | `			zEnd--;` |
|        1 | 14315 | `		}` |
|     3837 | 14316 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3837 | 14317 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 14318 | `		/* Copy entry name */` |
|     3837 | 14319 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 14320 | `		/* Perform the insertion */` |
|     3837 | 14321 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 14322 | `	}` |
|        - | 14323 | `	/* All done,return the created array */` |
|        3 | 14324 | `	ph7_result_value(pCtx,pArray);` |
|        - | 14325 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 14326 | `	 * by the engine as soon we return from this foreign` |
|        - | 14327 | `	 * function.` |
|        - | 14328 | `	 */` |
|        3 | 14329 | `	return PH7_OK;` |
|        2 | 14330 |  |
|        - | 14331 | `/*` |
|        - | 14332 | ` * include:` |
|        - | 14333 | ` * According to the PHP reference manual.` |
|        - | 14334 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 14335 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 14336 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 14337 | ` *  include() will finally check in the calling script's own directory` |
|        - | 14338 | ` *  and the current working directory before failing. The include()` |
|        - | 14339 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 14340 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 14341 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 14342 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 14343 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 14344 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 14345 | ` *  directory to find the requested file.` |
|        - | 14346 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 14347 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 14348 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 14349 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 14350 | ` */` |
|     9304 | 14351 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14352 |  |
|        - | 14353 | `	SyString sFile;` |
|        - | 14354 | `	sxi32 rc;` |
|     9306 | 14355 | `	if( nArg < 1 ){` |
|        - | 14356 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14357 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14358 | `		return SXRET_OK;` |
|        - | 14359 | `	}` |
|        - | 14360 | `	/* File to include */` |
|     9306 | 14361 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9306 | 14362 | `	if( sFile.nByte < 1 ){` |
|        - | 14363 | `		/* Empty string,return NULL */` |
|      ! 0 | 14364 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14365 | `		return SXRET_OK;` |
|        - | 14366 | `	}` |
|        - | 14367 | `	/* Open,compile and execute the desired script */` |
|     9306 | 14368 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9306 | 14369 | `	if( rc != SXRET_OK ){` |
|        - | 14370 | `		/* Emit a warning and return false */` |
|        3 | 14371 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 14372 | `		ph7_result_bool(pCtx,0);` |
|        1 | 14373 | `	}` |
|     9306 | 14374 | `	return SXRET_OK;` |
|     4654 | 14375 |  |
|        - | 14376 | `/*` |
|        - | 14377 | ` * include_once:` |
|        - | 14378 | ` *  According to the PHP reference manual.` |
|        - | 14379 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 14380 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 14381 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 14382 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 14383 | ` *   just once.` |
|        - | 14384 | ` */` |
|        4 | 14385 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14386 |  |
|        - | 14387 | `	SyString sFile;` |
|        - | 14388 | `	sxi32 rc;` |
|        5 | 14389 | `	if( nArg < 1 ){` |
|        - | 14390 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14391 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14392 | `		return SXRET_OK;` |
|        - | 14393 | `	}` |
|        - | 14394 | `	/* File to include */` |
|        5 | 14395 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 14396 | `	if( sFile.nByte < 1 ){` |
|        - | 14397 | `		/* Empty string,return NULL */` |
|      ! 0 | 14398 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14399 | `		return SXRET_OK;` |
|        - | 14400 | `	}` |
|        - | 14401 | `	/* Open,compile and execute the desired script */` |
|        5 | 14402 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 14403 | `	if( rc == SXERR_EXISTS ){` |
|        - | 14404 | `		/* File already included,return TRUE */` |
|        3 | 14405 | `		ph7_result_bool(pCtx,1);` |
|        3 | 14406 | `		return SXRET_OK;` |
|        - | 14407 | `	}` |
|        3 | 14408 | `	if( rc != SXRET_OK ){` |
|        - | 14409 | `		/* Emit a warning and return false */` |
|      ! 0 | 14410 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14411 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14412 | ` 	}` |
|        3 | 14413 | `	return SXRET_OK;` |
|        3 | 14414 |  |
|        - | 14415 | `/*` |
|        - | 14416 | ` * require.` |
|        - | 14417 | ` *  According to the PHP reference manual.` |
|        - | 14418 | ` *   require() is identical to include() except upon failure it will` |
|        - | 14419 | ` *   also produce a fatal level error.` |
|        - | 14420 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 14421 | ` *   emits a warning  which allows the script to continue.` |
|        - | 14422 | ` */` |
|        6 | 14423 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14424 |  |
|        - | 14425 | `	SyString sFile;` |
|        - | 14426 | `	sxi32 rc;` |
|        8 | 14427 | `	if( nArg < 1 ){` |
|        - | 14428 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14429 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14430 | `		return SXRET_OK;` |
|        - | 14431 | `	}` |
|        - | 14432 | `	/* File to include */` |
|        8 | 14433 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 14434 | `	if( sFile.nByte < 1 ){` |
|        - | 14435 | `		/* Empty string,return NULL */` |
|      ! 0 | 14436 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14437 | `		return SXRET_OK;` |
|        - | 14438 | `	}` |
|        - | 14439 | `	/* Open,compile and execute the desired script */` |
|        8 | 14440 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 14441 | `	if( rc != SXRET_OK ){` |
|        - | 14442 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 14443 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14444 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14445 | `		return PH7_ABORT;` |
|        - | 14446 | `	}` |
|        8 | 14447 | `	return SXRET_OK;` |
|        5 | 14448 |  |
|        - | 14449 | `/*` |
|        - | 14450 | ` * require_once:` |
|        - | 14451 | ` *  According to the PHP reference manual.` |
|        - | 14452 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 14453 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 14454 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 14455 | ` *   and how it differs from its non _once siblings.` |
|        - | 14456 | ` */` |
|        4 | 14457 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14458 |  |
|        - | 14459 | `	SyString sFile;` |
|        - | 14460 | `	sxi32 rc;` |
|        5 | 14461 | `	if( nArg < 1 ){` |
|        - | 14462 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14463 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14464 | `		return SXRET_OK;` |
|        - | 14465 | `	}` |
|        - | 14466 | `	/* File to include */` |
|        5 | 14467 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 14468 | `	if( sFile.nByte < 1 ){` |
|        - | 14469 | `		/* Empty string,return NULL */` |
|      ! 0 | 14470 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14471 | `		return SXRET_OK;` |
|        - | 14472 | `	}` |
|        - | 14473 | `	/* Open,compile and execute the desired script */` |
|        5 | 14474 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 14475 | `	if( rc == SXERR_EXISTS ){` |
|        - | 14476 | `		/* File already included,return TRUE */` |
|        3 | 14477 | `		ph7_result_bool(pCtx,1);` |
|        3 | 14478 | `		return SXRET_OK;` |
|        - | 14479 | `	}` |
|        3 | 14480 | `	if( rc != SXRET_OK ){` |
|        - | 14481 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 14482 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14483 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14484 | `		return PH7_ABORT;` |
|        - | 14485 | `	}` |
|        3 | 14486 | `	return SXRET_OK;` |
|        3 | 14487 |  |
|        - | 14488 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 14489 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 14490 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 14491 | `/*` |
|        - | 14492 | ` * Section:` |
|        - | 14493 | ` *  SPL Autoloading functions.` |
|        - | 14494 | ` * Status:` |
|        - | 14495 | ` *  Stable.` |
|        - | 14496 | ` */` |
|        - | 14497 | `/*` |
|        - | 14498 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 14499 | ` *  Register given function as __autoload() implementation.` |
|        - | 14500 | ` * Parameters` |
|        - | 14501 | ` *  callback` |
|        - | 14502 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 14503 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 14504 | ` *  throw` |
|        - | 14505 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 14506 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 14507 | ` *  prepend` |
|        - | 14508 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 14509 | ` *   autoload stack instead of appending it.` |
|        - | 14510 | ` * Return` |
|        - | 14511 | ` *  TRUE on success, FALSE on failure.` |
|        - | 14512 | ` */` |
|       34 | 14513 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14514 |  |
|        - | 14515 | `	VmAutoloadCB sEntry;` |
|       36 | 14516 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 14517 | `	int iPrepend = 0;` |
|        - | 14518 | `	sxu32 n;` |
|       36 | 14519 | `	if( nArg < 1 ){` |
|        - | 14520 | `		/* No callback provided — register default spl_autoload.` |
|        - | 14521 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 14522 | `		/* Check for duplicates first */` |
|        9 | 14523 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 14524 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 14525 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 14526 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 14527 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 14528 | `				ph7_result_bool(pCtx,1);` |
|        5 | 14529 | `				return SXRET_OK;` |
|        - | 14530 | `			}` |
|      ! 0 | 14531 | `		}` |
|        5 | 14532 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 14533 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 14534 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 14535 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 14536 | `		ph7_result_bool(pCtx,1);` |
|        5 | 14537 | `		return SXRET_OK;` |
|        - | 14538 | `	}` |
|        - | 14539 | `	/* Validate that the callback is callable */` |
|       28 | 14540 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 14541 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 14542 | `		if( nArg >= 2 ){` |
|      ! 0 | 14543 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 14544 | `		}` |
|      ! 0 | 14545 | `		if( iThrow ){` |
|      ! 0 | 14546 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 14547 | `				"Argument is not callable");` |
|      ! 0 | 14548 | `		}` |
|      ! 0 | 14549 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14550 | `		return SXRET_OK;` |
|        - | 14551 | `	}` |
|        - | 14552 | `	/* Check for duplicates */` |
|       46 | 14553 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 14554 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 14555 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 14556 | `			/* Already registered */` |
|      ! 0 | 14557 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 14558 | `			return SXRET_OK;` |
|        - | 14559 | `		}` |
|       11 | 14560 | `	}` |
|        - | 14561 | `	/* Check prepend flag */` |
|       28 | 14562 | `	if( nArg >= 3 ){` |
|        3 | 14563 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 14564 | `	}` |
|        - | 14565 | `	/* Store the callback */` |
|       28 | 14566 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 14567 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 14568 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 14569 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 14570 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 14571 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 14572 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 14573 | `		VmAutoloadCB *aBase;` |
|        3 | 14574 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 14575 | `		/* Rotate: move last entry to front */` |
|        3 | 14576 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 14577 | `		if( aBase ){` |
|        - | 14578 | `			VmAutoloadCB sTemp;` |
|        - | 14579 | `			sxu32 i;` |
|        3 | 14580 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 14581 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 14582 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 14583 | `			}` |
|        3 | 14584 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 14585 | `		}` |
|        2 | 14586 | `	}else{` |
|       26 | 14587 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 14588 | `	}` |
|       28 | 14589 | `	ph7_result_bool(pCtx,1);` |
|       28 | 14590 | `	return SXRET_OK;` |
|       19 | 14591 |  |
|        - | 14592 | `/*` |
|        - | 14593 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 14594 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 14595 | ` * Parameters` |
|        - | 14596 | ` *  callback` |
|        - | 14597 | ` *   The autoload function being unregistered.` |
|        - | 14598 | ` * Return` |
|        - | 14599 | ` *  TRUE on success, FALSE on failure.` |
|        - | 14600 | ` */` |
|       32 | 14601 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14602 |  |
|       34 | 14603 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14604 | `	sxu32 n,nEntry;` |
|       34 | 14605 | `	if( nArg < 1 ){` |
|      ! 0 | 14606 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14607 | `		return SXRET_OK;` |
|        - | 14608 | `	}` |
|       34 | 14609 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 14610 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 14611 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 14612 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 14613 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 14614 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 14615 | `			sxu32 i;` |
|       32 | 14616 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 14617 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 14618 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 14619 | `			}` |
|        - | 14620 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 14621 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 14622 | `			ph7_result_bool(pCtx,1);` |
|       32 | 14623 | `			return SXRET_OK;` |
|        - | 14624 | `		}` |
|        3 | 14625 | `	}` |
|        3 | 14626 | `	ph7_result_bool(pCtx,0);` |
|        3 | 14627 | `	return SXRET_OK;` |
|       18 | 14628 |  |
|        - | 14629 | `/*` |
|        - | 14630 | ` * array spl_autoload_functions(void)` |
|        - | 14631 | ` *  Return all registered __autoload() functions.` |
|        - | 14632 | ` * Return` |
|        - | 14633 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 14634 | ` *  an empty array is returned.` |
|        - | 14635 | ` */` |
|       20 | 14636 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14637 |  |
|       21 | 14638 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14639 | `	ph7_value *pArray;` |
|        - | 14640 | `	sxu32 n,nEntry;` |
|       10 | 14641 | `	SXUNUSED(nArg);` |
|       10 | 14642 | `	SXUNUSED(apArg);` |
|       21 | 14643 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 14644 | `	if( pArray == 0 ){` |
|      ! 0 | 14645 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14646 | `		return SXRET_OK;` |
|        - | 14647 | `	}` |
|       21 | 14648 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 14649 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 14650 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 14651 | `		if( pEntry ){` |
|       15 | 14652 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 14653 | `		}` |
|        8 | 14654 | `	}` |
|       21 | 14655 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 14656 | `	return SXRET_OK;` |
|       11 | 14657 |  |
|        - | 14658 | `/*` |
|        - | 14659 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 14660 | ` *  Default implementation of __autoload().` |
|        - | 14661 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 14662 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 14663 | ` * Parameters` |
|        - | 14664 | ` *  class` |
|        - | 14665 | ` *   The class name being searched.` |
|        - | 14666 | ` *  file_extensions` |
|        - | 14667 | ` *   Comma-separated list of file extensions to try.` |
|        - | 14668 | ` */` |
|        2 | 14669 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14670 |  |
|        - | 14671 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 14672 | `	SyBlob sPath;` |
|        - | 14673 | `	int nClass;` |
|        - | 14674 | `	sxi32 rc;` |
|        3 | 14675 | `	if( nArg < 1 ){` |
|      ! 0 | 14676 | `		return SXRET_OK;` |
|        - | 14677 | `	}` |
|        3 | 14678 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 14679 | `	if( nClass < 1 ){` |
|      ! 0 | 14680 | `		return SXRET_OK;` |
|        - | 14681 | `	}` |
|        - | 14682 | `	/* Default extensions */` |
|        3 | 14683 | `	zExt = ".php,.inc";` |
|        3 | 14684 | `	if( nArg >= 2 ){` |
|        - | 14685 | `		int nExt;` |
|      ! 0 | 14686 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 14687 | `		if( nExt < 1 ){` |
|      ! 0 | 14688 | `			zExt = ".php,.inc";` |
|      ! 0 | 14689 | `		}` |
|      ! 0 | 14690 | `	}` |
|        3 | 14691 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 14692 | `	/* Iterate over comma-separated extensions */` |
|        3 | 14693 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 14694 | `	zCur = zExt;` |
|        7 | 14695 | `	while( zCur < zEnd ){` |
|        - | 14696 | `		const char *zComma;` |
|        - | 14697 | `		SyString sFile;` |
|        - | 14698 | `		int i;` |
|        - | 14699 | `		/* Find next comma or end */` |
|        5 | 14700 | `		zComma = zCur;` |
|       21 | 14701 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 14702 | `			zComma++;` |
|        1 | 14703 | `		}` |
|        - | 14704 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 14705 | `		SyBlobReset(&sPath);` |
|       69 | 14706 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 14707 | `			char c = zClass[i];` |
|       65 | 14708 | `			if( c == '\\' ){` |
|      ! 0 | 14709 | `				c = '/';` |
|       65 | 14710 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 14711 | `				c = c + ('a' - 'A');` |
|        6 | 14712 | `			}` |
|       65 | 14713 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 14714 | `		}` |
|        - | 14715 | `		/* Append extension */` |
|        5 | 14716 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 14717 | `		/* Try to include the file */` |
|        5 | 14718 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 14719 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 14720 | `		if( rc == SXRET_OK ){` |
|        - | 14721 | `			/* File included successfully */` |
|      ! 0 | 14722 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 14723 | `			return SXRET_OK;` |
|        - | 14724 | `		}` |
|        - | 14725 | `		/* Move past the comma */` |
|        5 | 14726 | `		zCur = zComma;` |
|        5 | 14727 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 14728 | `			zCur++;` |
|        1 | 14729 | `		}` |
|        1 | 14730 | `	}` |
|        3 | 14731 | `	SyBlobRelease(&sPath);` |
|        3 | 14732 | `	return SXRET_OK;` |
|        2 | 14733 |  |
|        - | 14734 | `/* Table of built-in VM functions. */` |
|        - | 14735 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 14736 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 14737 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 14738 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 14739 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 14740 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 14741 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 14742 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 14743 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 14744 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 14745 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 14746 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 14747 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 14748 | `	    /* Constants management */` |
|        - | 14749 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 14750 | `	{ "define",   vm_builtin_define               },` |
|        - | 14751 | `	{ "constant", vm_builtin_constant             },` |
|        - | 14752 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 14753 | `	   /* Class/Object functions */` |
|        - | 14754 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 14755 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 14756 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 14757 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 14758 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 14759 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 14760 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 14761 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 14762 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 14763 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 14764 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 14765 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 14766 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 14767 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 14768 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 14769 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 14770 | `	   /* SPL Autoloading */` |
|        - | 14771 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 14772 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 14773 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 14774 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 14775 | `	   /* Random numbers/strings generators */` |
|        - | 14776 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 14777 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 14778 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 14779 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 14780 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 14781 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 14782 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 14783 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14784 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 14785 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 14786 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 14787 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 14788 | `	   /* Language constructs functions */` |
|        - | 14789 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 14790 | `	{ "print", vm_builtin_print                   },` |
|        - | 14791 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 14792 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 14793 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 14794 | `	  /* Variable handling functions */` |
|        - | 14795 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 14796 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 14797 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 14798 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 14799 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 14800 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 14801 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 14802 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 14803 | `	  /* Ouput control functions */` |
|        - | 14804 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 14805 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 14806 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 14807 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 14808 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 14809 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 14810 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 14811 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 14812 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 14813 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 14814 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 14815 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 14816 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 14817 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 14818 | `	  /* Assertion functions */` |
|        - | 14819 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 14820 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 14821 | `	  /* Error reporting functions */` |
|        - | 14822 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 14823 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 14824 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 14825 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 14826 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 14827 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 14828 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 14829 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 14830 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 14831 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 14832 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 14833 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 14834 | `	  /* Release info */` |
|        - | 14835 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 14836 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 14837 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 14838 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 14839 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 14840 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 14841 | `	  /* hashmap */` |
|        - | 14842 | `	{"compact",          vm_builtin_compact       },` |
|        - | 14843 | `	{"extract",          vm_builtin_extract       },` |
|        - | 14844 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 14845 | `	  /* URL related function */` |
|        - | 14846 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 14847 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 14848 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14849 | `	   /* XML processing functions */` |
|        - | 14850 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 14851 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 14852 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 14853 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 14854 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 14855 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 14856 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 14857 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 14858 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 14859 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 14860 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 14861 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 14862 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 14863 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 14864 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 14865 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 14866 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 14867 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 14868 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 14869 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 14870 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 14871 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 14872 | `	   /* UTF-8 encoding/decoding */` |
|        - | 14873 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 14874 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 14875 | `	   /* Command line processing */` |
|        - | 14876 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 14877 | `	   /* JSON encoding/decoding */` |
|        - | 14878 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 14879 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 14880 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 14881 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 14882 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 14883 | `	   /* Files/URI inclusion facility */` |
|        - | 14884 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 14885 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 14886 | `	{ "include",      vm_builtin_include          },` |
|        - | 14887 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 14888 | `	{ "require",      vm_builtin_require          },` |
|        - | 14889 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 14890 | `};` |
|        - | 14891 | `/*` |
|        - | 14892 | ` * Register the built-in VM functions defined above.` |
|        - | 14893 | ` */` |
|     2692 | 14894 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 14895 |  |
|        - | 14896 | `	sxi32 rc;` |
|        - | 14897 | `	sxu32 n;` |
|   352654 | 14898 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 14899 | `		/* Note that these special functions have access` |
|        - | 14900 | `		 * to the underlying virtual machine as their` |
|        - | 14901 | `		 * private data.` |
|        - | 14902 | `		 */` |
|   349962 | 14903 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   349962 | 14904 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 14905 | `			return rc;` |
|        - | 14906 | `		}` |
|   174982 | 14907 | `	}` |
|     2694 | 14908 | `	return SXRET_OK;` |
|     1348 | 14909 |  |
|        - | 14910 | `/*` |
|        - | 14911 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 14912 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 14913 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 14914 | ` */` |
|    41944 | 14915 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 14916 |  |
|    41946 | 14917 | `	if( !iLoadable ){` |
|    40062 | 14918 | `		return pClass;` |
|        - | 14919 | `	}` |
|     1890 | 14920 | `	while(pClass){` |
|     1886 | 14921 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1882 | 14922 | `			return pClass;` |
|        - | 14923 | `		}` |
|        5 | 14924 | `		pClass = pClass->pNextName;` |
|        1 | 14925 | `	}` |
|        5 | 14926 | `	return 0;` |
|    20974 | 14927 |  |
|        - | 14928 | `/*` |
|        - | 14929 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 14930 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 14931 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 14932 | ` * registered in the VM's class table.` |
|        - | 14933 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 14934 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 14935 | ` */` |
|       38 | 14936 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 14937 |  |
|        - | 14938 | `	VmAutoloadCB *pEntry;` |
|        - | 14939 | `	ph7_value sArg,sResult;` |
|        - | 14940 | `	SyHashEntry *pHashEntry;` |
|        - | 14941 | `	ph7_class *pClass;` |
|        - | 14942 | `	sxu32 n,nEntry;` |
|       40 | 14943 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       40 | 14944 | `	if( nEntry < 1 ){` |
|       26 | 14945 | `		return 0;` |
|        - | 14946 | `	}` |
|        - | 14947 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 14948 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 14949 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 14950 | `	}` |
|        - | 14951 | `	/* Mark this class as being autoloaded */` |
|       14 | 14952 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 14953 | `	/* Prepare the class name argument */` |
|       14 | 14954 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 14955 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 14956 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 14957 | `	pClass = 0;` |
|       28 | 14958 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 14959 | `		ph7_value *apArg[1];` |
|       24 | 14960 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 14961 | `		if( pEntry == 0 ){` |
|      ! 0 | 14962 | `			continue;` |
|        - | 14963 | `		}` |
|       24 | 14964 | `		apArg[0] = &sArg;` |
|       24 | 14965 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 14966 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 14967 | `			continue;` |
|        - | 14968 | `		}` |
|        - | 14969 | `		/* Check if the class is now available */` |
|       24 | 14970 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 14971 | `		if( pHashEntry ){` |
|       10 | 14972 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 14973 | `			if( pClass ){` |
|       10 | 14974 | `				break;` |
|        - | 14975 | `			}` |
|      ! 0 | 14976 | `		}` |
|        9 | 14977 | `	}` |
|       14 | 14978 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 14979 | `	PH7_MemObjRelease(&sResult);` |
|        - | 14980 | `	/* Remove reentrancy guard */` |
|       14 | 14981 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 14982 | `	return pClass;` |
|       21 | 14983 |  |
|        - | 14984 | `/*` |
|        - | 14985 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 14986 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 14987 | ` */` |
|       18 | 14988 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 14989 |  |
|       20 | 14990 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 14991 |  |
|        - | 14992 | `/*` |
|        - | 14993 | ` * Check if the given name refer to an installed class.` |
|        - | 14994 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 14995 | ` */` |
|    41956 | 14996 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 14997 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 14998 | `	const char *zName,  /* Name of the target class */` |
|        - | 14999 | `	sxu32 nByte,        /* zName length */` |
|        - | 15000 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 15001 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 15002 | `						 */` |
|        - | 15003 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 15004 | `	)` |
|        2 | 15005 |  |
|        - | 15006 | `	SyHashEntry *pEntry;` |
|        - | 15007 | `	ph7_class *pClass;` |
|    20978 | 15008 | `	SXUNUSED(iNest);` |
|        - | 15009 | `	/* Exact class lookup.` |
|        - | 15010 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 15011 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    41958 | 15012 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    41958 | 15013 | `	if( pEntry == 0 ){` |
|        - | 15014 | `		/* Class not found in hash table — try autoload before giving up */` |
|       22 | 15015 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 15016 | `	}` |
|    41938 | 15017 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    41938 | 15018 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    20980 | 15019 |  |
|        - | 15020 | `/*` |
|        - | 15021 | ` * Reference Table Implementation` |
|        - | 15022 | ` * Status: stable <chm@symisc.net>` |
|        - | 15023 | ` * Intro` |
|        - | 15024 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 15025 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 15026 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 15027 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 15028 | ` *  Refer to the official for more information on this powerful` |
|        - | 15029 | ` *  extension.` |
|        - | 15030 | ` */` |
|        - | 15031 | `/*` |
|        - | 15032 | ` * Allocate a new reference entry.` |
|        - | 15033 | ` */` |
|  3162030 | 15034 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 15035 |  |
|        - | 15036 | `	VmRefObj *pRef;` |
|        - | 15037 | `	/* Allocate a new instance */` |
|  3162032 | 15038 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3162032 | 15039 | `	if( pRef == 0 ){` |
|      ! 0 | 15040 | `		return 0;` |
|        - | 15041 | `	}` |
|        - | 15042 | `	/* Zero the structure */` |
|  3162032 | 15043 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 15044 | `	/* Initialize fields */` |
|  3162032 | 15045 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3162032 | 15046 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3162032 | 15047 | `	pRef->nIdx = nIdx;` |
|  3162032 | 15048 | `	return pRef;` |
|  1581017 | 15049 |  |
|        - | 15050 | `/*` |
|        - | 15051 | ` * Default hash function used by the reference table` |
|        - | 15052 | ` * for lookup/insertion operations.` |
|        - | 15053 | ` */` |
| 17367033 | 15054 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 15055 |  |
|        - | 15056 | `	/* Calculate the hash based on the memory object index */` |
| 17367035 | 15057 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 15058 |  |
|        - | 15059 | `/*` |
|        - | 15060 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 15061 | ` * in the reference table.` |
|        - | 15062 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 15063 | ` * otherwise.` |
|        - | 15064 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15065 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15066 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15067 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15068 | ` * Refer to the official for more information on this powerful` |
|        - | 15069 | ` * extension.` |
|        - | 15070 | ` */` |
|  9429416 | 15071 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 15072 |  |
|        - | 15073 | `	VmRefObj *pRef;` |
|        - | 15074 | `	sxu32 nBucket;` |
|        - | 15075 | `	/* Point to the appropriate bucket */` |
|  9429418 | 15076 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 15077 | `	/* Perform the lookup */` |
|  9429418 | 15078 | `	pRef = pVm->apRefObj[nBucket];` |
| 20583851 | 15079 | `	for(;;){` |
| 41164390 | 15080 | `		if( pRef == 0 ){` |
|  3263384 | 15081 | `			break;` |
|        - | 15082 | `		}` |
| 37901008 | 15083 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 15084 | `			/* Entry found */` |
|  6166036 | 15085 | `			return pRef;` |
|        - | 15086 | `		}` |
|        - | 15087 | `		/* Point to the next entry */` |
| 31734974 | 15088 | `		pRef = pRef->pNextCollide;` |
|        2 | 15089 | `	}` |
|        - | 15090 | `	/* No such entry,return NULL */` |
|  3263384 | 15091 | `	return 0;` |
|  4714710 | 15092 |  |
|        - | 15093 | `/*` |
|        - | 15094 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 15095 | ` *` |
|        - | 15096 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15097 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15098 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15099 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15100 | ` * Refer to the official for more information on this powerful` |
|        - | 15101 | ` * extension.` |
|        - | 15102 | ` */` |
|  3162030 | 15103 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 15104 |  |
|        - | 15105 | `	sxu32 nBucket;` |
|  3162032 | 15106 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 15107 | `		VmRefObj **apNew;` |
|        - | 15108 | `		sxu32 nNew;` |
|        - | 15109 | `		/* Allocate a larger table */` |
|     4596 | 15110 | `		nNew = pVm->nRefSize << 1;` |
|     4596 | 15111 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4596 | 15112 | `		if( apNew ){` |
|     4596 | 15113 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 15114 | `			sxu32 n;` |
|        - | 15115 | `			/* Zero the structure */` |
|     4596 | 15116 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 15117 | `			/* Rehash all referenced entries */` |
|  2849904 | 15118 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 15119 | `				/* Remove old collision links */` |
|  2845310 | 15120 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 15121 | `				/* Point to the appropriate bucket */` |
|  2845310 | 15122 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 15123 | `				/* Insert the entry  */` |
|  2845310 | 15124 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2845310 | 15125 | `				if( apNew[nBucket] ){` |
|  2301116 | 15126 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1150557 | 15127 | `				}` |
|  2845310 | 15128 | `				apNew[nBucket] = pEntry;` |
|        - | 15129 | `				/* Point to the next entry */` |
|  2845310 | 15130 | `				pEntry = pEntry->pNext;` |
|  1422656 | 15131 | `			}` |
|        - | 15132 | `			/* Release the old table */` |
|     4596 | 15133 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 15134 | `			/* Install the new one */` |
|     4596 | 15135 | `			pVm->apRefObj = apNew;` |
|     4596 | 15136 | `			pVm->nRefSize = nNew;` |
|     2297 | 15137 | `		}` |
|     2297 | 15138 | `	}` |
|        - | 15139 | `	/* Point to the appropriate bucket */` |
|  3162032 | 15140 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 15141 | `	/* Insert the entry */` |
|  3162032 | 15142 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3162032 | 15143 | `	if( pVm->apRefObj[nBucket] ){` |
|  2584939 | 15144 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1292486 | 15145 | `	}` |
|  3162032 | 15146 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3162032 | 15147 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3162032 | 15148 | `	pVm->nRefUsed++;` |
|  3162032 | 15149 | `	return SXRET_OK;` |
|        2 | 15150 |  |
|        - | 15151 | `/*` |
|        - | 15152 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 15153 | ` * the reference table.` |
|        - | 15154 | ` * This function is invoked when the user perform an unset` |
|        - | 15155 | ` * call [i.e: unset($var); ].` |
|        - | 15156 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15157 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15158 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15159 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15160 | ` * Refer to the official for more information on this powerful` |
|        - | 15161 | ` * extension.` |
|        - | 15162 | ` */` |
|  3122734 | 15163 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 15164 |  |
|        - | 15165 | `	ph7_hashmap_node **apNode;` |
|        - | 15166 | `	SyHashEntry **apEntry;` |
|        - | 15167 | `	sxu32 n;` |
|        - | 15168 | `	/* Point to the reference table */` |
|  3122736 | 15169 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3122736 | 15170 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 15171 | `	/* Unlink the entry from the reference table */` |
|  3230694 | 15172 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   107960 | 15173 | `		if( apEntry[n] ){` |
|   107910 | 15174 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    53954 | 15175 | `		}` |
|    53981 | 15176 | `	}` |
|  6138754 | 15177 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3016020 | 15178 | `		if( apNode[n] ){` |
|     7534 | 15179 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3766 | 15180 | `		}` |
|  1508011 | 15181 | `	}` |
|  3122736 | 15182 | `	if( pRef->pPrevCollide ){` |
|  1192457 | 15183 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   596280 | 15184 | `	}else{` |
|  1930281 | 15185 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 15186 | `	}` |
|  3122736 | 15187 | `	if( pRef->pNextCollide ){` |
|  1772150 | 15188 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   886088 | 15189 | `	}` |
|  3122736 | 15190 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 15191 | `	/* Release the node */` |
|  3122736 | 15192 | `	SySetRelease(&pRef->aReference);` |
|  3122736 | 15193 | `	SySetRelease(&pRef->aArrEntries);` |
|  3122736 | 15194 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3122736 | 15195 | `	pVm->nRefUsed--;` |
|  3122736 | 15196 | `	return SXRET_OK;` |
|        2 | 15197 |  |
|        - | 15198 | `/*` |
|        - | 15199 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 15200 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15201 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15202 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15203 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15204 | ` * Refer to the official for more information on this powerful` |
|        - | 15205 | ` * extension.` |
|        - | 15206 | ` */` |
|  3196778 | 15207 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 15208 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 15209 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 15210 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 15211 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 15212 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 15213 | `	)` |
|        2 | 15214 |  |
|  3196780 | 15215 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 15216 | `	VmRefObj *pRef;` |
|        - | 15217 | `	/* Check if the referenced object already exists */` |
|  3196780 | 15218 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3196780 | 15219 | `	if( pRef == 0 ){` |
|        - | 15220 | `		/* Create a new entry */` |
|  3162032 | 15221 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3162032 | 15222 | `		if( pRef == 0 ){` |
|      ! 0 | 15223 | `			return SXERR_MEM;` |
|        - | 15224 | `		}` |
|  3162032 | 15225 | `		pRef->iFlags = iFlags;` |
|        - | 15226 | `		/* Install the entry */` |
|  3162032 | 15227 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1581015 | 15228 | `	}` |
|  3196780 | 15229 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3196780 | 15230 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 15231 | `		VmSlot sRef;` |
|        - | 15232 | `		/* Local frame,record referenced entry so that it can` |
|        - | 15233 | `		 * be deleted when we leave this frame.` |
|        - | 15234 | `		 */` |
|   101450 | 15235 | `		sRef.nIdx = nIdx;` |
|   101450 | 15236 | `		sRef.pUserData = pEntry;` |
|   101450 | 15237 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 15238 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 15239 | `		}` |
|    50724 | 15240 | `	}` |
|  3196780 | 15241 | `	if( pEntry ){` |
|        - | 15242 | `		/* Address of the hash-entry */` |
|   135998 | 15243 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    67998 | 15244 | `	}` |
|  3196780 | 15245 | `	if( pMapEntry ){` |
|        - | 15246 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3052996 | 15247 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1526497 | 15248 | `	}` |
|  3196780 | 15249 | `	return SXRET_OK;` |
|  1598391 | 15250 |  |
|        - | 15251 | `/*` |
|        - | 15252 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 15253 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15254 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15255 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15256 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15257 | ` * Refer to the official for more information on this powerful` |
|        - | 15258 | ` * extension.` |
|        - | 15259 | ` */` |
|  3109898 | 15260 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 15261 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 15262 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 15263 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 15264 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 15265 | `	)` |
|        2 | 15266 |  |
|        - | 15267 | `	VmRefObj *pRef;` |
|        - | 15268 | `	sxu32 n;` |
|        - | 15269 | `	/* Check if the referenced object already exists */` |
|  3109900 | 15270 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3109900 | 15271 | `	if( pRef == 0 ){` |
|        - | 15272 | `		/* Not such entry */` |
|   101348 | 15273 | `		return SXERR_NOTFOUND;` |
|        - | 15274 | `	}` |
|        - | 15275 | `	/* Remove the desired entry */` |
|  3008554 | 15276 | `	if( pEntry ){` |
|        - | 15277 | `		SyHashEntry **apEntry;` |
|       62 | 15278 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      228 | 15279 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      168 | 15280 | `			if( apEntry[n] == pEntry ){` |
|        - | 15281 | `				/* Nullify the entry */` |
|       62 | 15282 | `				apEntry[n] = 0;` |
|        - | 15283 | `				/*` |
|        - | 15284 | `				 * NOTE:` |
|        - | 15285 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 15286 | `				 * we avoid wasting spaces.` |
|        - | 15287 | `				 */` |
|       30 | 15288 | `			}` |
|       85 | 15289 | `		}` |
|       30 | 15290 | `	}` |
|  3008554 | 15291 | `	if( pMapEntry ){` |
|        - | 15292 | `		ph7_hashmap_node **apNode;` |
|  3008494 | 15293 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  6017080 | 15294 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3008588 | 15295 | `			if( apNode[n] == pMapEntry ){` |
|        - | 15296 | `				/* nullify the entry */` |
|  3008494 | 15297 | `				apNode[n] = 0;` |
|  1504246 | 15298 | `			}` |
|  1504295 | 15299 | `		}` |
|  1504246 | 15300 | `	}` |
|  3008554 | 15301 | `	return SXRET_OK;` |
|  1554951 | 15302 |  |
|        - | 15303 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 15304 | `/*` |
|        - | 15305 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 15306 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 15307 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 15308 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 15309 | ` * For more information on how to register IO stream devices,please` |
|        - | 15310 | ` * refer to the official documentation.` |
|        - | 15311 | ` */` |
|    28248 | 15312 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 15313 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 15314 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 15315 | `	int nByte              /* *pzDevice length*/` |
|        - | 15316 | `	)` |
|        2 | 15317 |  |
|        - | 15318 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 15319 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 15320 | `	SyString sDev,sCur;` |
|        - | 15321 | `	sxu32 n,nEntry;` |
|        - | 15322 | `	int rc;` |
|        - | 15323 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    28250 | 15324 | `	zNext = zCur = zIn = *pzDevice;` |
|    28250 | 15325 | `	zEnd = &zIn[nByte];` |
|  1801942 | 15326 | `	while( zIn < zEnd ){` |
|  1773696 | 15327 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 15328 | `			/* Got one */` |
|        3 | 15329 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 15330 | `			break;` |
|        - | 15331 | `		}` |
|        - | 15332 | `		/* Advance the cursor */` |
|  1773694 | 15333 | `		zIn++;` |
|        2 | 15334 | `	}` |
|    28250 | 15335 | `	if( zIn >= zEnd ){` |
|        - | 15336 | `		/* No such scheme,return the default stream */` |
|    28248 | 15337 | `		return pVm->pDefStream;` |
|        - | 15338 | `	}` |
|        3 | 15339 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 15340 | `	/* Remove leading and trailing white spaces */` |
|        3 | 15341 | `	SyStringFullTrim(&sDev);` |
|        - | 15342 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 15343 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 15344 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 15345 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 15346 | `		pStream = apStream[n];` |
|        3 | 15347 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 15348 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 15349 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 15350 | `		if( rc == 0 ){` |
|        - | 15351 | `			/* Stream device found */` |
|        3 | 15352 | `			*pzDevice = zNext;` |
|        3 | 15353 | `			return pStream;` |
|        - | 15354 | `		}` |
|      ! 0 | 15355 | `	}` |
|        - | 15356 | `	/* No such stream,return NULL */` |
|      ! 0 | 15357 | `	return 0;` |
|    14126 | 15358 |  |
|        - | 15359 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 15360 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 15361 |  |
